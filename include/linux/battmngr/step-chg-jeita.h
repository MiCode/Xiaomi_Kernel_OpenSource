/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 */

#ifndef __STEP_CHG_H__
#define __STEP_CHG_H__

#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define JEITA_VOTER		"JEITA_VOTER"

#define jeita_is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

#define MAX_JEITA_ENTRIES	6

#define STEP_CHG_DELAY_3S		3000 /* 3 secs */
#define STEP_CHG_DELAY_10S		10000 /* 10 secs */

#define BATT_HOT_DECIDEGREE_MAX		600
#define GET_CONFIG_DELAY_MS		2000
#define GET_CONFIG_RETRY_COUNT		50
#define WAIT_BATT_ID_READY_MS		200

struct jeita_range_data {
	int low_threshold;
	int high_threshold;
	u32 value;
};


struct jeita_param {
	int rise_hys;
	int fall_hys;
};

struct jeita_fcc_config {
	struct jeita_param param;
	struct jeita_range_data ranges[MAX_JEITA_ENTRIES];
};

struct jeita_fv_config {
	struct jeita_param param;
	struct jeita_range_data ranges[MAX_JEITA_ENTRIES];
};


struct chg_jeita_info {
	struct device		*dev;
	struct jeita_fcc_config	*jeita_fcc_cfg;
	struct jeita_fv_config	*jeita_fv_cfg;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct delayed_work	status_change_work;
	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	int			jeita_fcc_index;
	int			jeita_fv_index;
	int			jeita_hot_th;
	int			jeita_cold_th;
	int			max_fv_uv;
	int			max_fcc_ma;
	bool			jeita_arb_en;
	bool			config_is_read;
	bool			chg_change_flag;
	bool			chg_enable_flag;
	bool			hot_cold_dis_chg;
};

#endif /* __STEP_CHG_H__ */

