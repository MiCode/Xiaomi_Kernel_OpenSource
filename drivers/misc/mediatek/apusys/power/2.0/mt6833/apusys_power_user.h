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
 * That is why default 525mv minus MG_VOLT_06250 first.
 * (the precondition is VPU/MDLA aging voltage on 525mv are the same)
 */
#ifdef AGING_MARGIN
#define MARGIN_VOLT_8	(MG_VOLT_06250)
#else
#define MARGIN_VOLT_8	(0)
#endif
#define MARGIN_VOLT_9	(0)

enum POWER_CALLBACK_USER {
	IOMMU = 0,
	REVISOR = 1,
	MNOC = 2,
	DEVAPC = 3,
	APUSYS_POWER_CALLBACK_USER_NUM,
};

enum DVFS_USER {
	VPU0 = 0,
	VPU1,
	MDLA0,
	APUSYS_DVFS_USER_NUM,

	EDMA = 0x10,	// power user only
	REVISER, // power user only
	APUSYS_POWER_USER_NUM,
};


enum DVFS_VOLTAGE_DOMAIN {
	V_VPU0 = 0,
	V_VPU1,
	V_MDLA0,
	V_APU_CONN,
	V_VCORE,
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
	DVFS_VOLT_00_800000_V = 800000 - MARGIN_VOLT_1,
	DVFS_VOLT_00_825000_V = 825000 - MARGIN_VOLT_0,
	DVFS_VOLT_00_850000_V = 850000 - MARGIN_VOLT_0,
	DVFS_VOLT_MAX = 850000 + 1,
};


enum DVFS_FREQ {
	DVFS_FREQ_NOT_SUPPORT = 0,
	DVFS_FREQ_00_026000_F = 26000,
	DVFS_FREQ_00_208000_F = 208000,
	DVFS_FREQ_00_242700_F = 242700,
	DVFS_FREQ_00_273000_F = 273000,
	DVFS_FREQ_00_312000_F = 312000,
	DVFS_FREQ_00_416000_F = 416000,
	DVFS_FREQ_00_499200_F = 499200,
	DVFS_FREQ_00_525000_F = 525000,
	DVFS_FREQ_00_546000_F = 546000,
	DVFS_FREQ_00_594000_F = 594000,
	DVFS_FREQ_00_624000_F = 624000,
	DVFS_FREQ_00_688000_F = 688000,
	DVFS_FREQ_00_687500_F = 687500,
	DVFS_FREQ_00_728000_F = 728000,
	DVFS_FREQ_00_800000_F = 800000,
	DVFS_FREQ_00_832000_F = 832000,
	DVFS_FREQ_00_960000_F = 960000,
	DVFS_FREQ_00_1100000_F = 1100000,
	DVFS_FREQ_MAX,
};

enum DVFS_FREQ_POSTDIV {
	POSDIV_NO = 0,
	POSDIV_1 = 0,
	POSDIV_2,
	POSDIV_4,
	POSDIV_8,
	POSDIV_16,
};

#endif
