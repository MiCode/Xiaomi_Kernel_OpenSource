/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __LC_JEITA_H__
#define __LC_JEITA_H__

#include "../common/lx_voter.h"
#include "lx_cp_policy.h"
#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
#include "../charger_class/xm_adapter_class.h"
#endif
#include "../charger_class/lx_fg_class.h"
#include "../charger_class/lx_charger_class.h"
#include "../fuelgauge/charger_partition.h"
#include "lx_charger_manager.h"

#define STEP_JEITA_TUPLE_NUM        9
#define STEP_CYCLE_TUPLE_NUM        4
#define NAGETIVE_10_TO_0_VOL_4200   4200
#define COLD_RECHG_VOLT_OFFSET      100
#define TEMP_45_TO_56_VOL           4100
#define WARM_RECHG_VOLT_OFFSET      200
#define TEMP_LEVEL_NEGATIVE_10      -100
#define TEMP_LEVEL_NEGATIVE_7       -70
#define TEMP_LEVEL_35               350
#define TEMP_LEVEL_42               420
#define TEMP_LEVEL_45               450
#define TEMP_LEVEL_56               560
#define INDEX_0_to_5                1
#define INDEX_5_to_10               2
#define INDEX_10_to_15              3
#define INDEX_15_to_20              4
#define INDEX_20_to_35              5
#define INDEX_35_to_40              6
#define INDEX_40_to_45              7
#define WARM_RECHG_TEMP_OFFSET      20
#define WARM_RECHG_TEMP_OFFSET_30   30
#define NORMAL_TERM_DELTA_CV        0
#define TEMP_LEVEL_20               200
#define ATL_ITERM                   658
#define LOW_TEMP_RECHG_OFFSET       200
#define NOR_TEMP_RECHG_OFFSET       100

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

#define POWER_REPLENISH_VOTER "POWER_REPLENISH_VOTER"
#define FCC_STEPPER_VOTER		"FCC_STEPPER_VOTER"
#define REPLENISH_LOOP_WAIT_S (3 * 1000)
#define REPLENISH_START_WAIT_S (300 * 1000)

struct lx_jeita_info *get_jeita_info(void);
int lx_jeita_init(struct device *dev);

void lx_jeita_deinit(void);
bool get_warm_stop_charge_state(void);
extern struct chargerpump_policy *g_policy;


struct step_jeita_cfg {
	int low_threshold;
	int high_threshold;
	int value;
};
struct step_jeita_cfg_other {
	int vbat1;
	int vbat2;
	int vbat3;
	int cur_val1;
	int cur_val2;
};
/*cycle count start*/
struct step_jeita_cycle_cfg {
	int vbat1;
	int vbat2;
	int vbat3;
	int vbat4;
	int cur_val1;
	int cur_val2;
	int cur_val3;
};

struct step_jeita_normal_cfg {
	int cv_val;
	int cur_val;
};

struct step_jeita_ffc_iterm_cfg {
	int bat1_iterm;   /*NVT*/
	int bat2_iterm;  /*cos*/
	int bat3_iterm;  /*SWD*/
};

#define CC_CV_CYCLE_COUNT_MAX  4
#define CC_CV_TEMP_STEP_MAX  3
#define CC_CV_CYCLE_COUNT_VOLT 3600


struct lx_jeita_info {
	struct device		*dev;
	ktime_t			jeita_last_update_time;
	bool			config_is_read;
	bool			sw_jeita_cfg_valid;
	bool			batt_missing;
	bool			taper_fcc;
	int			get_config_retry_count;
	int			jeita_index;
	int			fv;
	int			fv_offset_15_to_35;
	int			fv_offset_35_to_48;
	int			iterm;
	int			iterm_ffc;
	int			iterm_curr;
	int			under_4200_curr_offset;

	bool		pd_verifed;
	bool		is_charge_done;

	struct wakeup_source	*lx_jeita_ws;
	struct step_jeita_cfg step_chg_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg jeita_fv_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg jeita_fcc_cfg[STEP_JEITA_TUPLE_NUM];
	struct step_jeita_cfg jeita_bypass_high_lmt_cfg[STEP_JEITA_TUPLE_NUM];

	/* voter add here */
	struct votable *main_fcc_votable;
	struct votable *fv_votable;
	struct votable *main_icl_votable;
	struct votable *iterm_votable;
	struct votable *total_fcc_votable;

	/* psy add here */
	struct power_supply	*batt_psy;
	struct power_supply	*bms_psy;
	struct power_supply	*usb_psy;
	#if IS_ENABLED(CONFIG_PD_BATTERY_SECRET)
	struct adapter_device *pd_adapter;
	#endif
	struct fuel_gauge_dev *fuel_gauge;
	struct charger_dev *charger;

	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;

	struct notifier_block nb;
};

int lx_step_jeita_get_index(struct step_jeita_cfg *cfg, int value);

#endif /* __LC_JEITA_H__ */
