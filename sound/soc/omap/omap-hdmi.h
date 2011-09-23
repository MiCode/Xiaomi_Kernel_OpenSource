/*
 * omap-hdmi.h
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Contact: Jorge Candelaria <x0107209@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#ifndef __OMAP_HDMI_H__
#define __OMAP_HDMI_H__

#ifndef CONFIG_HDMI_NO_IP_MODULE

#define HDMI_WP			0x58006000
#define HDMI_WP_AUDIO_DATA	0x8Cul

extern void hdmi_audio_core_stub_init(void);
#endif

#endif	/* End of __OMAP_HDMI_H__ */
