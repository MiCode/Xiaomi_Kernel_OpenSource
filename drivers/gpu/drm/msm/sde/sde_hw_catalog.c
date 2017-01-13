/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#include "sde_hw_catalog.h"

struct sde_mdss_hw_cfg_handler cfg_table[] = {
	{ .major = 1, .minor = 7, .cfg_init = sde_mdss_cfg_170_init},
};

/**
 * sde_hw_catalog_init: Returns the catalog information for the
 * passed HW version
 * @major:  Major version of the MDSS HW
 * @minor: Minor version
 * @step: step version
 */
struct sde_mdss_cfg *sde_hw_catalog_init(u32 major, u32 minor, u32 step)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cfg_table); i++) {
		if ((cfg_table[i].major == major) &&
		(cfg_table[i].minor == minor))
			return cfg_table[i].cfg_init(step);
	}

	return ERR_PTR(-ENODEV);
}
