/* output_module.h - Output module interface definition
 *
 * Copyright (C) 2007   Ivo Clarysse
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

#ifndef _OUTPUT_MODULE_H
#define _OUTPUT_MODULE_H

#include "output.h"

struct output_module {
	const char *shortname;
	const char *description;

	// Commands.
	int (*init)(void);
	void (*set_uri)(const char *uri);
	void (*set_next_uri)(const char *uri);
	int (*play)(void);
	int (*stop)(void);
	int (*pause)(void);
	int (*loop)(void);
	int (*seek)(int position);
	void (*set_transport_callback)(output_transition_cb_t callback);

	// parameters
	int (*get_position)(int *track_dur, int *track_pos);
	int (*get_volume)(int *);
	int (*set_volume)(int);
	int (*get_mute)(int *);
	int (*set_mute)(int);
};

#endif
