/*
 * arch/arm/mach-tegra/include/mach/tegra_fuse.h
 *
 * Tegra Public Fuse header file
 *
 * Copyright (c) 2011, NVIDIA Corporation.
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

#ifndef _MACH_TEGRA_PUBLIC_FUSE_H_
#define _MACH_TEGRA_PUBLIC_FUSE_H_

int tegra_fuse_get_revision(u32 *rev);
int tegra_fuse_get_tsensor_calibration_data(u32 *calib);
int tegra_fuse_get_tsensor_spare_bits(u32 *spare_bits);
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
int tegra_fuse_get_vsensor_calib(u32 *calib);
int tegra_fuse_get_tsensor_calib(int index, u32 *calib);
#endif

#endif /* _MACH_TEGRA_PUBLIC_FUSE_H_*/

