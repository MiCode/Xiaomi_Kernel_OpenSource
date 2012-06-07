/* Header file for:
 * Cypress CY8CTMA300 Prototype touchscreen driver.
 *
 * Copyright (C) 2009, 2010 Cypress Semiconductor, Inc.
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Cypress reserves the right to make changes without further notice
 * to the materials described herein. Cypress does not assume any
 * liability arising out of the application described herein.
 *
 * Contact Cypress Semiconductor at www.cypress.com
 *
 * History:
 *			(C) 2010 Cypress - Update for GPL distribution
 *			(C) 2009 Cypress - Assume maintenance ownership
 *			(C) 2009 Enea - Original prototype
 *
 */
#ifndef __CY8C8CTS_H__
#define __CY8C8CTS_H__


/* CY8CTMA300-TMG200 platform data
 */
struct cy8c_ts_platform_data {
	int (*power_on)(int on);
	int (*dev_setup)(bool on);
	const char *ts_name;
	u32 dis_min_x; /* display resoltion */
	u32 dis_max_x;
	u32 dis_min_y;
	u32 dis_max_y;
	u32 min_touch; /* no.of touches supported */
	u32 max_touch;
	u32 min_tid; /* track id */
	u32 max_tid;
	u32 min_width;/* size of the finger */
	u32 max_width;
	u32 res_x; /* TS resolution */
	u32 res_y;
	u32 swap_xy;
	u32 flags;
	u16 invert_x;
	u16 invert_y;
	u8 nfingers;
	u32 irq_gpio;
	int resout_gpio;
	bool wakeup;
};

#endif
