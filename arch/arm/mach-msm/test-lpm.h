/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef __ARCH_ARM_MACH_MSM_TEST_LPM_H
#define __ARCH_ARM_MACH_MSM_TEST_LPM_H

struct lpm_test_platform_data {
	struct msm_rpmrs_level *msm_lpm_test_levels;
	int msm_lpm_test_level_count;
	bool use_qtimer;
};
#endif
