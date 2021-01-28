/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _APUSYS_POWER_USER_H_
#define _APUSYS_POWER_USER_H_

#if defined(CONFIG_MACH_MT6893)
/* Aging marging voltage */
#define MG_VOLT_18750  (18750)
#define MG_VOLT_12500  (12500)
#define MG_VOLT_06250  (6250)
#define MARGIN_VOLT_0	(0)
#define MARGIN_VOLT_1	(0)
#define MARGIN_VOLT_2	(0)
#define MARGIN_VOLT_3	(0)
#define MARGIN_VOLT_4	(0)
#define MARGIN_VOLT_5	(0)
#define MARGIN_VOLT_6	(0)
#define MARGIN_VOLT_7	(0)
/*
 * While aging, default volt will minus MG_VOLT_06250
 *
 * Aging table focuss on freq, but every time DVFS policy
 * will change apusys_opps.next_buck_volt as 575MV.
 *
 * Suppsely user takes opp 5, the smallest one, apusys_final_volt_check
 * will use MAX(apusys_opps.next_buck_volt[buck_index],
		apusys_opps.user_path_volt[user_index][path_index])
 * and that will never let aging voltage working.
 *
 * That is why default 575mv minus MG_VOLT_06250 first.
 * (the precondition is VPU/MDLA aging voltage on 575mv are the same)
 */
#ifdef AGING_MARGIN
#define MARGIN_VOLT_8	(MG_VOLT_06250)
#else
#define MARGIN_VOLT_8	(0)
#endif
#define MARGIN_VOLT_9	(0)

#else /* mt6885 */
#ifdef AGING_MARGIN
#if 0
#define MARGIN_VOLT_0	(18750 + 41250)
#define MARGIN_VOLT_1	(18750 + 40000)
#define MARGIN_VOLT_2	(18750 + 38750)
#define MARGIN_VOLT_3	(18750 + 37500)
#define MARGIN_VOLT_4	(18750 + 36250)
#define MARGIN_VOLT_5	(12500 + 35000)
#define MARGIN_VOLT_6	(12500 + 32500)
#define MARGIN_VOLT_7	(12500 + 30000)
#define MARGIN_VOLT_8	(6250 + 28750)
#define MARGIN_VOLT_9	(6250 + 27500)
#else
#define MARGIN_VOLT_0	(18750)
#define MARGIN_VOLT_1	(18750)
#define MARGIN_VOLT_2	(18750)
#define MARGIN_VOLT_3	(18750)
#define MARGIN_VOLT_4	(18750)
#define MARGIN_VOLT_5	(12500)
#define MARGIN_VOLT_6	(12500)
#define MARGIN_VOLT_7	(12500)
#define MARGIN_VOLT_8	(6250)
#define MARGIN_VOLT_9	(6250)
#endif
#else
#define MARGIN_VOLT_0	(0)
#define MARGIN_VOLT_1	(0)
#define MARGIN_VOLT_2	(0)
#define MARGIN_VOLT_3	(0)
#define MARGIN_VOLT_4	(0)
#define MARGIN_VOLT_5	(0)
#define MARGIN_VOLT_6	(0)
#define MARGIN_VOLT_7	(0)
#define MARGIN_VOLT_8	(0)
#define MARGIN_VOLT_9	(0)
#endif
#endif

enum POWER_CALLBACK_USER {
	IOMMU = 0,
	REVISOR = 1,
	MNOC = 2,
	DEVAPC = 3,
	APUSYS_POWER_CALLBACK_USER_NUM,
};

enum DVFS_USER {
	VPU0 = 0,
	VPU1 = 1,
	VPU2 = 2,
	MDLA0 = 3,
	MDLA1 = 4,
	APUSYS_DVFS_USER_NUM,

	EDMA = 0x10,	// power user only
	EDMA2 = 0x11,   // power user only
	REVISER = 0x12, // power user only
	APUSYS_POWER_USER_NUM,
};


enum DVFS_VOLTAGE_DOMAIN {
	V_VPU0 = 0,
	V_VPU1 = 1,
	V_VPU2 = 2,
	V_MDLA0 = 3,
	V_MDLA1 = 4,
	V_APU_CONN = 5,
	V_TOP_IOMMU = 6,
	V_VCORE = 7,
	APUSYS_BUCK_DOMAIN_NUM,
};


enum DVFS_BUCK {
	SRAM_BUCK = -1,	// sepcial case for VSRAM constraint
	VPU_BUCK = 0,
	MDLA_BUCK = 1,
	VCORE_BUCK = 2,
	APUSYS_BUCK_NUM,
};


enum DVFS_VOLTAGE {
	DVFS_VOLT_NOT_SUPPORT = 0,
	DVFS_VOLT_00_550000_V = 550000 - MARGIN_VOLT_9,
	DVFS_VOLT_00_575000_V = 575000 - MARGIN_VOLT_8,
	DVFS_VOLT_00_600000_V = 600000 - MARGIN_VOLT_7,
	DVFS_VOLT_00_625000_V = 625000 - MARGIN_VOLT_7,
	DVFS_VOLT_00_650000_V = 650000 - MARGIN_VOLT_6,
	DVFS_VOLT_00_700000_V = 700000 - MARGIN_VOLT_5,
	DVFS_VOLT_00_725000_V = 725000 - MARGIN_VOLT_4,
	DVFS_VOLT_00_737500_V = 737500 - MARGIN_VOLT_3,
	DVFS_VOLT_00_750000_V = 750000 - MARGIN_VOLT_3,
	DVFS_VOLT_00_762500_V = 762500 - MARGIN_VOLT_2,
	DVFS_VOLT_00_775000_V = 775000 - MARGIN_VOLT_2,
#if defined(CONFIG_MACH_MT6893)
	DVFS_VOLT_00_787500_V = 787500 - MARGIN_VOLT_1, // TODO: chk margin val
#endif
	DVFS_VOLT_00_800000_V = 800000 - MARGIN_VOLT_1,
	DVFS_VOLT_00_825000_V = 825000 - MARGIN_VOLT_0,
	DVFS_VOLT_MAX = 825000 + 1,
};


enum DVFS_FREQ {
	DVFS_FREQ_NOT_SUPPORT = 0,
	DVFS_FREQ_00_026000_F = 26000,
	DVFS_FREQ_00_104000_F = 104000,
	DVFS_FREQ_00_136500_F = 136500,
	DVFS_FREQ_00_208000_F = 208000,
	DVFS_FREQ_00_273000_F = 273000,
	DVFS_FREQ_00_280000_F = 280000,
	DVFS_FREQ_00_306000_F = 306000,
	DVFS_FREQ_00_312000_F = 312000,
	DVFS_FREQ_00_364000_F = 364000,
	DVFS_FREQ_00_392857_F = 392857,
	DVFS_FREQ_00_416000_F = 416000,
	DVFS_FREQ_00_457000_F = 457000,
	DVFS_FREQ_00_458333_F = 458333,
	DVFS_FREQ_00_499200_F = 499200,
	DVFS_FREQ_00_546000_F = 546000,
	DVFS_FREQ_00_550000_F = 550000,
	DVFS_FREQ_00_572000_F = 572000,
	DVFS_FREQ_00_594000_F = 594000,
	DVFS_FREQ_00_624000_F = 624000,
	DVFS_FREQ_00_687500_F = 687500,
	DVFS_FREQ_00_700000_F = 700000,
	DVFS_FREQ_00_728000_F = 728000,
	DVFS_FREQ_00_750000_F = 750000,
	DVFS_FREQ_00_800000_F = 800000,
	DVFS_FREQ_00_832000_F = 832000,
	DVFS_FREQ_00_850000_F = 850000,
	DVFS_FREQ_00_880000_F = 880000,
	DVFS_FREQ_00_900000_F = 900000,
	DVFS_FREQ_MAX,
};

#endif
