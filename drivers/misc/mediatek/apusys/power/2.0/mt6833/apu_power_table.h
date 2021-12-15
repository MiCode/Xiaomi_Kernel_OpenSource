// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_POWER_TABLE_H_
#define _APU_POWER_TABLE_H_

#include "apusys_power_user.h"


/******************************************************
 * for other cooperation module API, e.g. EARA, thermal
 ******************************************************/

enum APU_OPP_INDEX {
	APU_OPP_0 = 0,
	APU_OPP_1 = 1,
	APU_OPP_2 = 2,
	APU_OPP_3 = 3,
	APU_OPP_4 = 4,
	APU_OPP_NUM
};

struct apu_opp_info {
	enum APU_OPP_INDEX opp_index;   /* vpu or mdla opp */
	int power;                      /* power consumption (mW) */
};

extern struct apu_opp_info vpu_power_table[APU_OPP_NUM];
extern struct apu_opp_info mdla_power_table[APU_OPP_NUM];
extern int32_t apusys_thermal_en_throttle_cb(enum DVFS_USER user,
						enum APU_OPP_INDEX opp);
extern int32_t apusys_thermal_dis_throttle_cb(enum DVFS_USER user);

#endif
