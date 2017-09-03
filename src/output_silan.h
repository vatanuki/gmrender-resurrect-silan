/* output_gstreamer.h - Definitions for GStreamer output module
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

#ifndef _OUTPUT_SILAN_H
#define _OUTPUT_SILAN_H

#define SILAN_IPC_LOGGING

#define SILAN_RECV_TOUT 500000
#define SILAN_RECV_RETRY 10

#define SILAN_DATA_MUTE			0x0001
#define SILAN_DATA_VOLUME		0x0002
#define SILAN_DATA_DURATION		0x0004
#define SILAN_DATA_POSITION		0x0008
#define SILAN_DATA_PLAYING		0x0010

extern struct output_module silan_output;

#endif /*  _OUTPUT_SILAN_H */
