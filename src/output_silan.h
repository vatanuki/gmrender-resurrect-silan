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

//#define SILAN_DEBUG
#define SILAN_IPC_LOGGING
#define SILAN_IR_LOGGING
#define SILAN_SIGNAL_LOGGING

#define SILAN_AUTO_POWER_OFF	300
#define SILAN_AUTO_SPDIFIN		120
#define SILAN_AMP_UNMUTE		3

#define SILAN_RECV_TOUT			500000
#define SILAN_RECV_RETRY		10

#define SILAN_DATA_MUTE			0x0001
#define SILAN_DATA_VOLUME		0x0002
#define SILAN_DATA_DURATION		0x0004
#define SILAN_DATA_POSITION		0x0008
#define SILAN_STATUS_SPDIFIN	0x0010
#define SILAN_STATUS_AMP_MUTE	0x0020
#define SILAN_STATUS_AMP_POWER	0x0040

#define SILAN_FNAME_IR_INPUT	"/dev/input/event0"
#define SILAN_FNAME_DB_SLMP		"/etc/db/Slmp.xml"
#define SILAN_FNAME_DB_SYSTEM	"/etc/db/System.xml"

#define SILAN_SLMP_KEY_PADMUX	"<setpadmux>"
#define SILAN_SYSTEM_KEY_VOLUME	"<volume>"
#define SILAN_SYSTEM_KEY_MUTE	"<mute>"

#define SILAN_FNAME_SPDIF_IN_INFO	"/sys/kernel/debug/spdifin/info"

extern struct output_module silan_output;

#endif /*  _OUTPUT_SILAN_H */
