/* Copyright (c) 2013, 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __GEN_VKEYS_
struct vkeys_platform_data {
	const char *name;
	int disp_maxx;
	int disp_maxy;
	int panel_maxx;
	int panel_maxy;
	int *keycodes;
	int num_keys;
	int y_offset;
};
#endif
