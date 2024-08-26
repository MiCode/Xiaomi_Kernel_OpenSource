/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __LENOVO_JEITA_H__
#define __LENOVO_JEITA_H__

#include "../common/hq_voter.h"
#include "hq_cp_policy.h"
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "../charger_class/xm_adapter_class.h"
#endif
#include "../charger_class/hq_fg_class.h"
#include "../charger_class/hq_charger_class.h"

#define STEP_JEITA_TUPLE_NUM        7
#define STEP_CYCLE_TUPLE_NUM        3
#define NAGETIVE_10_TO_0_VOL_4200   4250
#define COLD_RECHG_VOLT_OFFSET      100
#define TEMP_48_TO_58_VOL           4090
#define CURRENT_NOW_1A              500
#define WARM_RECHG_VOLT_OFFSET      130
#define TEMP_LEVEL_NEGATIVE_10      -100
#define TEMP_LEVEL_35               350
#define TEMP_LEVEL_48               450
#define TEMP_LEVEL_58               580
#define INDEX_15_to_35              4
#define INDEX_35_to_48              5
#define WARM_RECHG_TEMP_OFFSET      20
#define TERM_DELTA_CV               8
#define HEAVY_LOAD_VOLTAGE          4470

#define LOW_TEMP_RECHG_OFFSET       200
#define NOR_TEMP_RECHG_OFFSET       100

int hq_jeita_init(struct device *dev);
void hq_jeita_deinit(void);
bool get_warm_stop_charge_state(void);
extern struct chargerpump_policy *g_policy;

#endif /* __STEP_CHG_H__ */
