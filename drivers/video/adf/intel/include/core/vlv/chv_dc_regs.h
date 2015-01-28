/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _CHV_DC_REG_H
#define _CHV_DC_REG_H

#define CHV_DISPLAY_BASE 0x180000
#define CHV_PORTB_CTRL	(CHV_DISPLAY_BASE + 0x61140)
#define CHV_PORTC_CTRL	(CHV_DISPLAY_BASE + 0x61160)
#define CHV_PORTD_CTRL	(CHV_DISPLAY_BASE + 0x6116C)

#define CHV_HPD_CTRL	(CHV_DISPLAY_BASE + 0x61164)
#define CHV_HPD_STAT	(CHV_DISPLAY_BASE + 0x61114)
#define CHV_HPD_LIVE_STATUS_MASK	(0x7 << 27)
#define CHV_HPD_LIVE_STATUS_B	(0x1 << 29)
#define CHV_HPD_LIVE_STATUS_C	(0x1 << 28)
#define CHV_HPD_LIVE_STATUS_D	(0x1 << 27)
#define CHV_HPD_LIVE_STATUS(port) (CHV_HPD_LIVE_STATUS_B \
				>> (port - PORT_B))

/* CHV SDVO/HDMI bits: */
#define   SDVO_PIPE_SEL_CHV(pipe)		((pipe) << 24)
#define   SDVO_PIPE_SEL_MASK_CHV		(3 << 24)

#define   GMBUS_PORT_DPD_CHV      3

#endif
