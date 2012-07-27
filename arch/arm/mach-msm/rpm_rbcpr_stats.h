/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_RPM_RBCPR_STATS_H
#define __ARCH_ARM_MACH_MSM_RPM_RBCPR_STATS_H

#include <linux/types.h>

struct msm_rpmrbcpr_design_data {
	u32 upside_steps;
	u32 downside_steps;
	int svs_voltage;
	int nominal_voltage;
	int turbo_voltage;
};

struct msm_rpmrbcpr_platform_data {
	struct msm_rpmrbcpr_design_data rbcpr_data;
};
#endif
