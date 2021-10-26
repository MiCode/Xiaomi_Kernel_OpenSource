/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __HELIO_DVFSRC_OPP_MT6893_H
#define __HELIO_DVFSRC_OPP_MT6893_H

#include <linux/pm_qos.h>

int ddr_level_to_step(int opp);

enum ddr_opp {
	DDR_OPP_0 = 0,
	DDR_OPP_1,
	DDR_OPP_2,
	DDR_OPP_3,
	DDR_OPP_4,
	DDR_OPP_5,
	DDR_OPP_6,
	DDR_OPP_7,
	DDR_OPP_NUM,
	DDR_OPP_UNREQ = PM_QOS_DDR_OPP_DEFAULT_VALUE,
};

enum vcore_opp {
	VCORE_OPP_0 = 0,
	VCORE_OPP_1,
	VCORE_OPP_2,
	VCORE_OPP_3,
	VCORE_OPP_4,
	VCORE_OPP_NUM,
	VCORE_OPP_UNREQ = PM_QOS_VCORE_OPP_DEFAULT_VALUE,
};

enum emi_opp {
	EMI_OPP_0 = 0,
	EMI_OPP_1,
	EMI_OPP_2,
	EMI_OPP_NUM,
	EMI_OPP_UNREQ = -1,
};

enum vcore_dvfs_opp {
	VCORE_DVFS_OPP_0 = 0,
	VCORE_DVFS_OPP_1,
	VCORE_DVFS_OPP_2,
	VCORE_DVFS_OPP_3,
	VCORE_DVFS_OPP_4,
	VCORE_DVFS_OPP_5,
	VCORE_DVFS_OPP_6,
	VCORE_DVFS_OPP_7,
	VCORE_DVFS_OPP_8,
	VCORE_DVFS_OPP_9,
	VCORE_DVFS_OPP_10,
	VCORE_DVFS_OPP_11,
	VCORE_DVFS_OPP_12,
	VCORE_DVFS_OPP_13,
	VCORE_DVFS_OPP_14,
	VCORE_DVFS_OPP_15,
	VCORE_DVFS_OPP_16,
	VCORE_DVFS_OPP_17,
	VCORE_DVFS_OPP_18,
	VCORE_DVFS_OPP_19,
	VCORE_DVFS_OPP_20,
	VCORE_DVFS_OPP_21,
	VCORE_DVFS_OPP_22,
	VCORE_DVFS_OPP_23,
	VCORE_DVFS_OPP_NUM,
	VCORE_DVFS_OPP_UNREQ = PM_QOS_VCORE_DVFS_FORCE_OPP_DEFAULT_VALUE,
};

#endif /* __HELIO_DVFSRC_OPP_MT6893_H */
