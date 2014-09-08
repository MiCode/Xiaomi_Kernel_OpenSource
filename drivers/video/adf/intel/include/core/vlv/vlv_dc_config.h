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

#ifndef VLV_DC_CONFIG_H
#define VLV_DC_CONFIG_H

#include "core/intel_dc_config.h"

#define VLV_N_PLANES	6
#define VLV_N_PIPES	2
#define VLV_NUM_SPRITES 2
#define VLV_SP_12BIT_MASK 0xFFF

enum pipe {
	PIPE_A = 0,
	PIPE_B,
	MAX_PIPES,
};

enum planes {
	PRIMARY_PLANE = 0,
	SPRITE_A = 1,
	SPRITE_B = 2,
	SECONDARY_PLANE = 3,
	SPRITE_C = 4,
	SPRITE_D = 5,
	NUM_PLANES,
};

enum vlv_disp_plane {
	VLV_PLANE = 0,
	VLV_SPRITE1,
	VLV_SPRITE2,
	VLV_MAX_PLANES,
};

bool vlv_intf_screen_connected(struct intel_pipe *pipe);
u32 vlv_intf_vsync_counter(struct intel_pipe *pipe, u32 interval);
#endif
