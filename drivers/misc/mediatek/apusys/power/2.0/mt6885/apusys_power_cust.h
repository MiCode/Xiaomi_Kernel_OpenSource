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

#ifndef _APUSYS_POWER_CUST_H_
#define _APUSYS_POWER_CUST_H_

#include <linux/types.h>
#include <linux/printk.h>


#define APUSYS_MAX_NUM_OPPS                (23)
#define APUSYS_PATH_USER_NUM               (4)   // num of DVFS_XXX_PATH
#define APUSYS_DVFS_CONSTRAINT_NUM			(15)
//#define APUSYS_BUCK_NUM						(3)
#define APUSYS_DEFAULT_OPP					(9)

enum POWER_CALLBACK_USER {
	IOMMU = 0,
	REVISOR = 1,
	MNOC = 2,
	APUSYS_POWER_CALLBACK_USER_NUM,
};

enum DVFS_USER {
	VPU0 = 0,
	VPU1 = 1,
	VPU2 = 2,
	MDLA0 = 3,
	MDLA1 = 4,
	APUSYS_DVFS_USER_NUM,
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
	VPU_BUCK = 0,
	MDLA_BUCK = 1,
	VCORE_BUCK = 2,
	APUSYS_BUCK_NUM,
};


enum DVFS_VOLTAGE {
	DVFS_VOLT_NOT_SUPPORT = 0,
	DVFS_VOLT_00_550000_V = 550000,
	DVFS_VOLT_00_575000_V = 575000,
	DVFS_VOLT_00_600000_V = 600000,
	DVFS_VOLT_00_650000_V = 650000,
	DVFS_VOLT_00_700000_V = 700000,
	DVFS_VOLT_00_725000_V = 725000,
	DVFS_VOLT_00_750000_V = 750000,
	DVFS_VOLT_00_800000_V = 800000,
	DVFS_VOLT_00_825000_V = 825000,
	DVFS_VOLT_MAX,
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
	DVFS_FREQ_00_832000_F = 832000,
	DVFS_FREQ_00_850000_F = 850000,
	DVFS_FREQ_MAX,
};


enum DVFS_VPU0_PWR_PATH {
	VPU0_VPU = 0,
	VPU0_APU_CONN = 1,
	VPU0_TOP_IOMMU = 2,
	VPU0_VCORE = 3,
};

enum DVFS_VPU1_PWR_PATH {
	VPU1_VPU = 0,
	VPU1_APU_CONN = 1,
	VPU1_TOP_IOMMU = 2,
	VPU1_VCORE = 3,
};

enum DVFS_VPU2_PWR_PATH {
	VPU2_VPU = 0,
	VPU2_APU_CONN = 1,
	VPU2_TOP_IOMMU = 2,
	VPU2_VCORE = 3,
};


enum DVFS_MDLA0_PWR_PATH {
	VMDLA0_MDLA = 0,
	VMDLA0_APU_CONN = 1,
	VMDLA0_TOP_IOMMU = 2,
	VMDLA0_VCORE = 3,
};

enum DVFS_MDLA1_PWR_PATH {
	VMDLA1_MDLA = 0,
	VMDLA1_APU_CONN = 1,
	VMDLA1_TOP_IOMMU = 2,
	VMDLA1_VCORE = 3,
};


struct apusys_dvfs_steps {
	enum DVFS_FREQ freq;
	enum DVFS_VOLTAGE voltage;
};

struct apusys_dvfs_constraint {
	enum DVFS_VOLTAGE_DOMAIN voltage_domain0;
	enum DVFS_VOLTAGE voltage0;
	enum DVFS_VOLTAGE_DOMAIN voltage_domain1;
	enum DVFS_VOLTAGE voltage1;
};


struct apusys_dvfs_opps {
	// map to dvfs_table
	struct apusys_dvfs_steps (*opps)[APUSYS_BUCK_DOMAIN_NUM];
	enum DVFS_VOLTAGE user_path_volt[APUSYS_DVFS_USER_NUM]
					[APUSYS_PATH_USER_NUM];
	enum DVFS_VOLTAGE final_buck_volt[APUSYS_BUCK_DOMAIN_NUM];
	uint8_t cur_opp_index[APUSYS_BUCK_DOMAIN_NUM];
	uint8_t prev_opp_index[APUSYS_BUCK_DOMAIN_NUM];
	uint8_t power_lock_max_opp[APUSYS_DVFS_USER_NUM];
	uint8_t power_lock_min_opp[APUSYS_DVFS_USER_NUM];
	uint8_t thermal_opp[APUSYS_DVFS_USER_NUM];
	uint8_t user_opp_index[APUSYS_DVFS_USER_NUM];
	uint8_t driver_opp_index[APUSYS_DVFS_USER_NUM];
	bool is_power_on[APUSYS_DVFS_USER_NUM];
	uint8_t power_bit_mask;
	uint64_t id;
};

extern char *user_str[APUSYS_DVFS_USER_NUM];
extern char *buck_domain_str[APUSYS_BUCK_DOMAIN_NUM];
extern enum DVFS_VOLTAGE_DOMAIN apusys_user_to_buck_domain
					[APUSYS_DVFS_USER_NUM];
extern enum DVFS_BUCK apusys_user_to_buck[APUSYS_DVFS_USER_NUM];
extern enum DVFS_VOLTAGE_DOMAIN apusys_buck_to_buck_domain[APUSYS_BUCK_NUM];
extern enum DVFS_USER apusys_buck_domain_to_user[APUSYS_BUCK_DOMAIN_NUM];
extern enum DVFS_BUCK apusys_buck_up_sequence[APUSYS_BUCK_NUM];
extern enum DVFS_BUCK apusys_buck_down_sequence[APUSYS_BUCK_NUM];
extern uint8_t dvfs_clk_path[APUSYS_DVFS_USER_NUM][APUSYS_PATH_USER_NUM];
extern uint8_t dvfs_buck_for_clk_path[APUSYS_DVFS_USER_NUM][APUSYS_BUCK_NUM];
extern bool buck_shared[APUSYS_BUCK_DOMAIN_NUM]
				[APUSYS_DVFS_USER_NUM]
				[APUSYS_PATH_USER_NUM];
extern struct apusys_dvfs_constraint dvfs_constraint_table
					[APUSYS_DVFS_CONSTRAINT_NUM];
extern int vcore_opp_mapping[APUSYS_MAX_NUM_OPPS];
extern struct apusys_dvfs_steps dvfs_table[APUSYS_MAX_NUM_OPPS]
						[APUSYS_BUCK_DOMAIN_NUM];

#endif
