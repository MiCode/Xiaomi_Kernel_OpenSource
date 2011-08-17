/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MSM_FOOTSWITCH__
#define __MSM_FOOTSWITCH__

#include <linux/regulator/machine.h>

enum fs_ids {
	FS_GFX2D0 = 0,
	FS_GFX2D1,
	FS_GFX3D,
	FS_IJPEG,
	FS_MDP,
	FS_MFC,
	FS_ROT,
	FS_VED,
	FS_VFE,
	FS_VPE,
	MAX_FS
};

#endif

#define FS_GENERIC(_drv_name, _id, _name) (&(struct platform_device){ \
	.name	= (_drv_name), \
	.id	= (_id), \
	.dev	= { \
		.platform_data = &(struct regulator_init_data){ \
			.constraints = { \
				.valid_modes_mask = REGULATOR_MODE_NORMAL, \
				.valid_ops_mask   = REGULATOR_CHANGE_STATUS, \
			}, \
			.num_consumer_supplies = 1, \
			.consumer_supplies = \
				&(struct regulator_consumer_supply) \
				REGULATOR_SUPPLY((_name), NULL), \
		} \
	}, \
})
#define FS_PCOM(_id, _name) FS_GENERIC("footswitch-pcom",    (_id), (_name))
#define FS_8X60(_id, _name) FS_GENERIC("footswitch-msm8x60", (_id), (_name))
