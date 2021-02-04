/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __HELIO_DVFSRC_MT6765_H
#define __HELIO_DVFSRC_MT6765_H

#include <mach/upmu_hw.h>

#define PMIC_VCORE_ADDR		PMIC_RG_BUCK_VCORE_VOSEL

#define VCORE_BASE_UV		518750
#define VCORE_STEP_UV		6250

/* DVFSRC_BASIC_CONTROL 0x0 */
#define DVFSRC_EN_SHIFT		0
#define DVFSRC_EN_MASK		0x1
#define DVFSRC_OUT_EN_SHIFT	8
#define DVFSRC_OUT_EN_MASK	0x1
#define FORCE_EN_CUR_SHIFT	14
#define FORCE_EN_CUR_MASK	0x1
#define FORCE_EN_TAR_SHIFT	15
#define FORCE_EN_TAR_MASK	0x1

/* DVFSRC_SW_REQ 0x4 */
#define EMI_SW_AP_SHIFT		0
#define EMI_SW_AP_MASK		0x3
#define VCORE_SW_AP_SHIFT	2
#define VCORE_SW_AP_MASK	0x3

/* DVFSRC_SW_REQ2 0x8 */
#define EMI_SW_AP2_SHIFT	0
#define EMI_SW_AP2_MASK		0x3
#define VCORE_SW_AP2_SHIFT	2
#define VCORE_SW_AP2_MASK	0x3

/* DVFSRC_VCORE_REQUEST 0x48 */
#define VCORE_SCP_GEAR_SHIFT	30
#define VCORE_SCP_GEAR_MASK	0x3

/* DVFSRC_VCORE_REQUEST2 0x4C */
#define VCORE_QOS_GEAR0_SHIFT	24
#define VCORE_QOS_GEAR0_MASK	0x3

/* DVFSRC_LEVEL 0xDC */
#define CURRENT_LEVEL_SHIFT	16
#define CURRENT_LEVEL_MASK	0xFFFF

/* DVFSRC_FORCE 0x300 */
#define TARGET_FORCE_SHIFT	0
#define TARGET_FORCE_MASK	0xFFFF
#define CURRENT_FORCE_SHIFT	16
#define CURRENT_FORCE_MASK	0xFFFF

/* met profile table index */
enum met_info_index {
	INFO_OPP_IDX = 0,
	INFO_FREQ_IDX,
	INFO_VCORE_IDX,
	INFO_SPM_LEVEL_IDX,
	INFO_MAX,
};

enum met_src_index {
	SRC_MD2SPM_IDX = 0,
	SRC_QOS_EMI_LEVEL_IDX,
	SRC_QOS_VCORE_LEVEL_IDX,
	SRC_CM_MGR_LEVEL_IDX,
	SRC_TOTAL_EMI_LEVEL_1_IDX,
	SRC_TOTAL_EMI_LEVEL_2_IDX,
	SRC_TOTAL_EMI_RESULT_IDX,
	SRC_QOS_BW_LEVEL1_IDX,
	SRC_QOS_BW_LEVEL2_IDX,
	SRC_QOS_BW_RESUT_IDX,
	SRC_SCP_VCORE_LEVEL_IDX,
	SRC_MAX
};

extern void dvfsrc_enable_dvfs_freq_hopping(int gps_on);
extern int dvfsrc_get_dvfs_freq_hopping_status(void);
#endif /* __HELIO_DVFSRC_MT6765_H */
