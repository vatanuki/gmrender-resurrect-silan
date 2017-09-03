/* upnp_control.c - UPnP RenderingControl routines
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "upnp_control.h"

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <string.h>

#include <upnp/upnp.h>
#include <upnp/ithread.h>

#include "logging.h"
#include "webserver.h"
#include "upnp.h"
#include "upnp_device.h"
#include "output.h"
#include "xmlescape.h"
#include "variable-container.h"

#define CONTROL_TYPE "urn:schemas-upnp-org:service:RenderingControl:1"

// For some reason (predates me), this was explicitly commented out and
// set to the service type; were there clients that were confused about the
// right use of the service-ID ? Setting this back, let's see what happens.
#define CONTROL_SERVICE_ID "urn:upnp-org:serviceId:RenderingControl"
//#define CONTROL_SERVICE_ID CONTROL_TYPE
#define CONTROL_SCPD_URL "/upnp/rendercontrolSCPD.xml"
#define CONTROL_CONTROL_URL "/upnp/control/rendercontrol1"
#define CONTROL_EVENT_URL "/upnp/event/rendercontrol1"

// Namespace, see UPnP-av-RenderingControl-v3-Service-20101231.pdf page 19
#define CONTROL_EVENT_XML_NS "urn:schemas-upnp-org:metadata-1-0/RCS/"

typedef enum {
	CONTROL_VAR_LAST_CHANGE,
	CONTROL_VAR_PRESET_NAME_LIST,
	CONTROL_VAR_AAT_CHANNEL,
	CONTROL_VAR_AAT_INSTANCE_ID,
	CONTROL_VAR_AAT_PRESET_NAME,
	CONTROL_VAR_MUTE,
	CONTROL_VAR_VOLUME,
	CONTROL_VAR_UNKNOWN,
	CONTROL_VAR_COUNT
} control_variable_t;

typedef enum {
	CONTROL_CMD_LIST_PRESETS,
	CONTROL_CMD_GET_MUTE,
	CONTROL_CMD_SET_MUTE,
	CONTROL_CMD_GET_VOL,
	CONTROL_CMD_SET_VOL,
	CONTROL_CMD_UNKNOWN,
	CONTROL_CMD_COUNT
} control_cmd;

static struct action control_actions[];

static const char *control_variable_names[] = {
	[CONTROL_VAR_LAST_CHANGE] = "LastChange",
	[CONTROL_VAR_PRESET_NAME_LIST] = "PresetNameList",
	[CONTROL_VAR_AAT_CHANNEL] = "A_ARG_TYPE_Channel",
	[CONTROL_VAR_AAT_INSTANCE_ID] = "A_ARG_TYPE_InstanceID",
	[CONTROL_VAR_AAT_PRESET_NAME] = "A_ARG_TYPE_PresetName",
	[CONTROL_VAR_MUTE] = "Mute",
	[CONTROL_VAR_VOLUME] = "Volume",
	[CONTROL_VAR_UNKNOWN] = NULL
};

static const char *aat_presetnames[] =
{
	"FactoryDefaults",
	NULL
};

static const char *aat_channels[] =
{
	"Master",
	"LF",
	"RF",
	"CF",
	"LFE",
	"LS",
	"RS",
	//"LFC",
	//"RFC",
	//"SD",
	//"SL",
	//"SR",
	//"T",
	//"B",
	NULL
};

static struct param_range volume_range = { 0, 100, 1 };

static struct var_meta control_var_meta[] = {
	[CONTROL_VAR_LAST_CHANGE] =			{ SENDEVENT_YES, DATATYPE_STRING, NULL, NULL },
	[CONTROL_VAR_PRESET_NAME_LIST] =	{ SENDEVENT_NO, DATATYPE_STRING, NULL, NULL },
	[CONTROL_VAR_AAT_CHANNEL] =			{ SENDEVENT_NO, DATATYPE_STRING, aat_channels, NULL },
	[CONTROL_VAR_AAT_INSTANCE_ID] =		{ SENDEVENT_NO, DATATYPE_UI4, NULL, NULL },
	[CONTROL_VAR_AAT_PRESET_NAME] =		{ SENDEVENT_NO, DATATYPE_STRING, aat_presetnames, NULL },
	[CONTROL_VAR_MUTE] =				{ SENDEVENT_NO, DATATYPE_BOOLEAN, NULL, NULL },
	[CONTROL_VAR_VOLUME] =				{ SENDEVENT_NO, DATATYPE_UI2, NULL, &volume_range },
	[CONTROL_VAR_UNKNOWN] =				{ SENDEVENT_NO, DATATYPE_UNKNOWN, NULL, NULL }
};

static const char *control_default_values[] = {
	[CONTROL_VAR_LAST_CHANGE] = "<Event xmlns = \"urn:schemas-upnp-org:metadata-1-0/RCS/\"/>",
	[CONTROL_VAR_PRESET_NAME_LIST] = "",
	[CONTROL_VAR_AAT_CHANNEL] = "",
	[CONTROL_VAR_AAT_INSTANCE_ID] = "0",
	[CONTROL_VAR_AAT_PRESET_NAME] = "",
	[CONTROL_VAR_MUTE] = "0",
	[CONTROL_VAR_VOLUME] = "0",
	[CONTROL_VAR_UNKNOWN] = NULL
};

extern struct service control_service_;   // Defined below.
static variable_container_t *state_variables_ = NULL;

static ithread_mutex_t control_mutex;

static void service_lock(void)
{
	ithread_mutex_lock(&control_mutex);
	if (control_service_.last_change) {
		UPnPLastChangeCollector_start(control_service_.last_change);
	}
}

static void service_unlock(void)
{
	if (control_service_.last_change) {
		UPnPLastChangeCollector_finish(control_service_.last_change);
	}
	ithread_mutex_unlock(&control_mutex);
}

static struct argument *arguments_list_presets[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, CONTROL_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "CurrentPresetNameList", PARAM_DIR_OUT, CONTROL_VAR_PRESET_NAME_LIST },
	NULL
};
// static struct argument *arguments_select_preset[] = {
// 	& (struct argument) { "InstanceID", PARAM_DIR_IN, CONTROL_VAR_AAT_INSTANCE_ID },
// 	& (struct argument) { "PresetName", PARAM_DIR_IN, CONTROL_VAR_AAT_PRESET_NAME },
// 	NULL
// };
static struct argument *arguments_get_mute[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, CONTROL_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Channel", PARAM_DIR_IN, CONTROL_VAR_AAT_CHANNEL },
	& (struct argument) { "CurrentMute", PARAM_DIR_OUT, CONTROL_VAR_MUTE },
	NULL
};
static struct argument *arguments_set_mute[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, CONTROL_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Channel", PARAM_DIR_IN, CONTROL_VAR_AAT_CHANNEL },
	& (struct argument) { "DesiredMute", PARAM_DIR_IN, CONTROL_VAR_MUTE },
	NULL
};
static struct argument *arguments_get_vol[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, CONTROL_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Channel", PARAM_DIR_IN, CONTROL_VAR_AAT_CHANNEL },
	& (struct argument) { "CurrentVolume", PARAM_DIR_OUT, CONTROL_VAR_VOLUME },
	NULL
};
static struct argument *arguments_set_vol[] = {
	& (struct argument) { "InstanceID", PARAM_DIR_IN, CONTROL_VAR_AAT_INSTANCE_ID },
	& (struct argument) { "Channel", PARAM_DIR_IN, CONTROL_VAR_AAT_CHANNEL },
	& (struct argument) { "DesiredVolume", PARAM_DIR_IN, CONTROL_VAR_VOLUME },
	NULL
};

static struct argument **argument_list[] = {
	[CONTROL_CMD_LIST_PRESETS] =	arguments_list_presets,
	[CONTROL_CMD_GET_MUTE] =		arguments_get_mute,
	[CONTROL_CMD_SET_MUTE] =		arguments_set_mute,
	[CONTROL_CMD_GET_VOL] =			arguments_get_vol,
	[CONTROL_CMD_SET_VOL] =			arguments_set_vol,
	[CONTROL_CMD_UNKNOWN] =			NULL
};

static void replace_var(control_variable_t varnum, const char *new_value) {
	VariableContainer_change(state_variables_, varnum, new_value);
}

static int cmd_obtain_variable(struct action_event *event,
			 control_variable_t varnum,
			 const char *paramname)
{
	char *instance = upnp_get_string(event, "InstanceID");
	if (instance == NULL) {
		return -1;
	}
	Log_info("control", "%s: %s for instance %s\n", __FUNCTION__, paramname, instance);
	free(instance); // we don't care about that value for now.

	upnp_append_variable(event, varnum, paramname);
	return 0;
}

static int list_presets(struct action_event *event)
{
	return cmd_obtain_variable(event, CONTROL_VAR_PRESET_NAME_LIST, "CurrentPresetNameList");
}

static int get_mute(struct action_event *event)
{
	/* FIXME - Channel */
	return cmd_obtain_variable(event, CONTROL_VAR_MUTE, "CurrentMute");
}

static void set_mute_toggle(int do_mute) {
	replace_var(CONTROL_VAR_MUTE, do_mute ? "1" : "0");
	output_set_mute(do_mute);
}

static int set_mute(struct action_event *event) {
	const char *value = upnp_get_string(event, "DesiredMute");
	service_lock();
	const int do_mute = atoi(value);
	set_mute_toggle(do_mute);
	//replace_var(CONTROL_VAR_MUTE, do_mute ? "1" : "0");
	service_unlock();
	return 0;
}

static int get_volume(struct action_event *event)
{
	/* FIXME - Channel */
	return cmd_obtain_variable(event, CONTROL_VAR_VOLUME, "CurrentVolume");
}

static int set_volume(struct action_event *event) {
	const char *volume = upnp_get_string(event, "DesiredVolume");
	service_lock();
	int volume_level = atoi(volume);
	if (volume_level < volume_range.min) volume_level = volume_range.min;
	if (volume_level > volume_range.max) volume_level = volume_range.max;
	replace_var(CONTROL_VAR_VOLUME, volume);
	output_set_volume(volume_level);
	service_unlock();

	return 0;
}

static struct action control_actions[] = {
	[CONTROL_CMD_LIST_PRESETS] =	{"ListPresets", list_presets},
	[CONTROL_CMD_GET_MUTE] =		{"GetMute", get_mute}, /* optional */
	[CONTROL_CMD_SET_MUTE] =		{"SetMute", set_mute}, /* optional */
	[CONTROL_CMD_GET_VOL] =			{"GetVolume", get_volume}, /* optional */
	[CONTROL_CMD_SET_VOL] =			{"SetVolume", set_volume}, /* optional */
	[CONTROL_CMD_UNKNOWN] =			{NULL, NULL}
};

struct service *upnp_control_get_service(void) {
	if (control_service_.variable_container == NULL) {
		state_variables_ =
			VariableContainer_new(CONTROL_VAR_COUNT,
					control_variable_names,
					control_default_values);
		control_service_.variable_container = state_variables_;
	}

	return &control_service_;
}

void upnp_control_init(struct upnp_device *device) {
	upnp_control_get_service();

	// Set initial volume.
	int volume = 0;
	if (output_get_volume(&volume) == 0) {
		Log_info("control", "Output inital volume is %d; setting "
			 "control variables accordingly.", volume);
		char vol[32];
		sprintf(vol, "%d", volume);
		replace_var(CONTROL_VAR_VOLUME, vol);
	}

	// Set initial mute.
	int mute = 0;
	if (output_get_mute(&mute) == 0) {
		Log_info("control", "Output inital mute is %d; setting "
			 "control variables accordingly.", mute);
		char var[32];
		sprintf(var, "%d", mute);
		replace_var(CONTROL_VAR_MUTE, var);
	}

	assert(control_service_.last_change == NULL);
	control_service_.last_change = UPnPLastChangeCollector_new(state_variables_, CONTROL_EVENT_XML_NS, device, CONTROL_SERVICE_ID);
	// According to UPnP-av-RenderingControl-v3-Service-20101231.pdf, 2.3.1
	// page 51, the A_ARG_TYPE* variables are not evented.
	UPnPLastChangeCollector_add_ignore(control_service_.last_change, CONTROL_VAR_AAT_CHANNEL);
	UPnPLastChangeCollector_add_ignore(control_service_.last_change, CONTROL_VAR_AAT_INSTANCE_ID);
	UPnPLastChangeCollector_add_ignore(control_service_.last_change, CONTROL_VAR_AAT_PRESET_NAME);
}

void upnp_control_register_variable_listener(variable_change_listener_t cb, void *userdata) {
	VariableContainer_register_callback(state_variables_, cb, userdata);
}

struct service control_service_ = {
	.service_id =		CONTROL_SERVICE_ID,
	.service_type =		CONTROL_TYPE,
	.scpd_url =			CONTROL_SCPD_URL,
	.control_url =		CONTROL_CONTROL_URL,
	.event_url =		CONTROL_EVENT_URL,
	.event_xml_ns =		CONTROL_EVENT_XML_NS,
	.actions =			control_actions,
	.action_arguments =	argument_list,
	.variable_names =	control_variable_names,
	.variable_container =NULL,// set later.
	.last_change =		NULL,
	.variable_meta =	control_var_meta,
	.variable_count =	CONTROL_VAR_UNKNOWN,
	.command_count =	CONTROL_CMD_UNKNOWN,
	.service_mutex =	&control_mutex
};
