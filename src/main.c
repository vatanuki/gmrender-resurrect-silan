/* main.c - Main program routines
 *
 * Copyright (C) 2005-2007   Ivo Clarysse
 *
 * This file is part of GMediaRender.
 *
 * GMediaRender is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * GMediaRender is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GMediaRender; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#define _GNU_SOURCE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
//#include <glib.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef HAVE_LIBUPNP
# error "To have gmrender any useful, you need to have libupnp installed."
#endif

#include <upnp/ithread.h>

// For version strings of upnp and gstreamer
#include <upnp/upnpconfig.h>

#include "git-version.h"
#include "logging.h"
#include "output.h"
#include "upnp.h"
#include "upnp_control.h"
#include "upnp_device.h"
#include "upnp_renderer.h"
#include "upnp_transport.h"

static char show_devicedesc = 0;
static char show_connmgr_scpd = 0;
static char show_control_scpd = 0;
static char show_transport_scpd = 0;
static char show_outputs = 0;
static char daemon_mode = 1;

static const char *ip_address = NULL;
static int listen_port = 49494;

#ifdef GMRENDER_UUID
// Compile-time uuid.
static const char *uuid = GMRENDER_UUID;
#else
static const char *uuid = "GMediaRender-1_0-000-000-002";
#endif
static const char *friendly_name = "silan";//PACKAGE_NAME;
static const char *output = NULL;
//static const char *pid_file = NULL;
//static const char *log_file = NULL;
//static const char *log_file = "stdout";
//static const char *log_file = "/var/log/renderer.log";

static void log_variable_change(void *userdata, int var_num,
				const char *variable_name,
				const char *old_value,
				const char *variable_value) {
	const char *category = (const char*) userdata;
	int needs_newline = variable_value[strlen(variable_value) - 1] != '\n';
	// Silly terminal codes. Set to empty strings if not needed.
	const char *var_start = Log_color_allowed() ? "\033[1m\033[34m" : "";
	const char *var_end = Log_color_allowed() ? "\033[0m" : "";
	Log_info(category, "%s%s%s: %s%s",
		 var_start, variable_name, var_end,
		 variable_value, needs_newline ? "\n" : "");
}

int main(int argc, char **argv)
{
	int rc;
	struct upnp_device_descriptor *upnp_renderer;

	if (show_connmgr_scpd) {
		upnp_renderer_dump_connmgr_scpd();
		exit(EXIT_SUCCESS);
	}
	if (show_control_scpd) {
		upnp_renderer_dump_control_scpd();
		exit(EXIT_SUCCESS);
	}
	if (show_transport_scpd) {
		upnp_renderer_dump_transport_scpd();
		exit(EXIT_SUCCESS);
	}
	if (show_outputs) {
		output_dump_modules();
		exit(EXIT_SUCCESS);
	}

//	Log_init("stdout");
	fprintf(stderr, "%s started [ gmediarender %s (libupnp-%s) ].\n", PACKAGE_STRING, GM_COMPILE_VERSION, UPNP_VERSION_STRING);

	// Now we're going to start threads etc, which means we need
	// to become a daemon before that.

	// TODO: check for availability of daemon() in configure.
	if (daemon_mode) {
		if (daemon(0, 0) < 0) {
			perror("Becoming daemon: ");
			return EXIT_FAILURE;
		}
	}

	upnp_renderer = upnp_renderer_descriptor(friendly_name, uuid);
	if (upnp_renderer == NULL) {
		return EXIT_FAILURE;
	}

	rc = output_init(output);
	if (rc != 0) {
		Log_error("main",
			  "ERROR: Failed to initialize Output subsystem");
		return EXIT_FAILURE;
	}

	struct upnp_device *device;
	if (listen_port != 0 &&
	    (listen_port < 49152 || listen_port > 65535)) {
		// Somewhere obscure internally in libupnp, they clamp the
		// port to be outside of the IANA range, so at least 49152.
		// Instead of surprising the user by ignoring lower port
		// numbers, complain loudly.
		Log_error("main", "Parameter error: --port needs to be in "
			  "range [49152..65535] (but was set to %d)",
			  listen_port);
		return EXIT_FAILURE;
	}
	device = upnp_device_init(upnp_renderer, ip_address, listen_port);
	if (device == NULL) {
		Log_error("main", "ERROR: Failed to initialize UPnP device");
		return EXIT_FAILURE;
	}

	upnp_transport_init(device);
	upnp_control_init(device);

	if (show_devicedesc) {
		// This can only be run after all services have been
		// initialized.
		char *buf = upnp_create_device_desc(upnp_renderer);
		assert(buf != NULL);
		fputs(buf, stdout);
		exit(EXIT_SUCCESS);
	}

	if (Log_info_enabled()) {
		upnp_transport_register_variable_listener(log_variable_change, (void*) "transport");
		upnp_control_register_variable_listener(log_variable_change, (void*) "control");
	}

	// Write both to the log (which might be disabled) and console.
	Log_info("main", "Ready for rendering.");
	fprintf(stderr, "Ready for rendering.\n");

	output_loop();

	// We're here, because the loop exited. Probably due to catching
	// a signal.
	Log_info("main", "Exiting.");
	upnp_device_shutdown(device);

	return EXIT_SUCCESS;
}
