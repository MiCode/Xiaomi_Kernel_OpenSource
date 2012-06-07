/* Copyright (c) 2010-2012 Code Aurora Forum. All rights reserved.
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

/* Device IDs */
#define FS_GFX2D0	0
#define FS_GFX2D1	1
#define FS_GFX3D	2
#define FS_IJPEG	3
#define FS_MDP		4
#define FS_MFC		5
#define FS_ROT		6
#define FS_VED		7
#define FS_VFE		8
#define FS_VPE		9
#define FS_VCAP		10
#define MAX_FS		11

struct fs_clk_data {
	const char *name;
	struct clk *clk;
	unsigned long rate;
	unsigned long reset_rate;
	bool enabled;
};

struct fs_driver_data {
	int bus_port0, bus_port1;
	struct fs_clk_data *clks;
};

#define FS_GENERIC(_drv_name, _id, _name, _dev_id, _data) \
(&(struct platform_device){ \
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
				REGULATOR_SUPPLY((_name), (_dev_id)), \
			.driver_data = (_data), \
		} \
	}, \
})
#define FS_PCOM(_id, _name, _dev_id) \
		FS_GENERIC("footswitch-pcom", _id, _name, _dev_id, NULL)
#define FS_8X60(_id, _name, _dev_id, _data) \
		FS_GENERIC("footswitch-8x60", _id, _name, _dev_id, _data)

#endif
