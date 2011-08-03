/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_BOARD_APQ8064_H
#define __ARCH_ARM_MACH_MSM_BOARD_APQ8064_H

#include <linux/mfd/pm8xxx/pm8921.h>

extern struct pm8921_regulator_platform_data
	msm8064_pm8921_regulator_pdata[] __devinitdata;

extern int msm8064_pm8921_regulator_pdata_len __devinitdata;

struct mmc_platform_data;
int __init apq8064_add_sdcc(unsigned int controller,
		struct mmc_platform_data *plat);

#endif
