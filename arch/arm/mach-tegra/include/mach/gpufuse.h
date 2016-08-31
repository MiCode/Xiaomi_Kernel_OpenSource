/*
 * arch/arm/mach-tegra/include/mach/gpufuse.h
 *
 * Copyright (C) 2010-2012 NVIDIA Corporation.
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

/* Number of register sets to handle in host context switching */
int tegra_gpu_register_sets(void);

struct gpu_info {
	int num_pixel_pipes;
	int num_alus_per_pixel_pipe;
};

void tegra_gpu_get_info(struct gpu_info *pInfo);

