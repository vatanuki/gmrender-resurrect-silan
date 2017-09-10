/* output.h - Output module frontend
 *
 * Copyright (C) 2007 Ivo Clarysse,  (C) 2012 Henner Zeller
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

#ifndef _OUTPUT_H
#define _OUTPUT_H

// Feedback for the controlling part what is happening with the
// output.
enum PlayFeedback {
	PLAY_STOPPED,
	PLAY_STARTED_NEXT_STREAM,
};
typedef void (*output_transition_cb_t)(enum PlayFeedback);

int output_init(const char *shortname);
void output_dump_modules(void);

int output_loop(void);

void output_set_uri(const char *uri);
void output_set_next_uri(const char *uri);

int output_play(output_transition_cb_t done_callback);
int output_stop(void);
int output_pause(void);
int output_get_position(int *track_dur, int *track_pos);
int output_seek(int position);

int output_get_volume(int *v);
int output_set_volume(int v);
int output_get_mute(int *m);
int output_set_mute(int m);

#endif /* _OUTPUT_H */
