/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 */

#ifndef __STEP_CHG_H__
#define __STEP_CHG_H__

#define STEP_CHG_VOTER		"STEP_CHG_VOTER"
#define JEITA_VOTER		"JEITA_VOTER"

#define is_between(left, right, value) \
		(((left) >= (right) && (left) >= (value) \
			&& (value) >= (right)) \
		|| ((left) <= (right) && (left) <= (value) \
			&& (value) <= (right)))

#define MAX_STEP_CHG_ENTRIES	8

#define STEP_CHG_DELAY_3S		3000 /* 3 secs */
#define STEP_CHG_DELAY_10S		10000 /* 10 secs */

#define BATT_HOT_DECIDEGREE_MAX		600
#define GET_CONFIG_DELAY_MS		2000
#define GET_CONFIG_RETRY_COUNT		50
#define WAIT_BATT_ID_READY_MS		200

enum cycle_count_status {
	CYCLE_COUNT_LOW,
	CYCLE_COUNT_NORMAL,
	CYCLE_COUNT_HIGH,
};

struct range_data {
	int low_threshold;
	int high_threshold;
	u32 value;
};

struct step_chg_jeita_param {
	u32			iio_prop;
	int			rise_hys;
	int			fall_hys;
};

struct step_chg_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct cold_step_chg_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct jeita_fcc_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fcc_cfg[MAX_STEP_CHG_ENTRIES];
};

struct jeita_fv_cfg {
	struct step_chg_jeita_param	param;
	struct range_data		fv_cfg[MAX_STEP_CHG_ENTRIES];
};

struct step_chg_info {
	struct device		*dev;
	bool			step_chg_enable;
	bool			sw_jeita_enable;
	bool			jeita_arb_en;
	bool			config_is_read;
	bool			step_chg_cfg_valid;
	bool			cold_step_chg_cfg_valid;
	bool			sw_jeita_cfg_valid;
	bool			batt_missing;
	bool			taper_fcc;
	int			jeita_fcc_index;
	int			jeita_cold_fcc_index;
	int			jeita_fv_index;
	int			step_index;
	int			get_config_retry_count;
	int			jeita_hot_th;
	int			jeita_cold_th;
	int			cycle_count;
	int			cycle_count_status;

	bool			hot_cold_dis_chg;
	bool			fastmode_flag;
	bool			chg_change_flag;
	bool			chg_enable_flag;

	struct step_chg_cfg	*step_chg_config;
	struct cold_step_chg_cfg	*cold_step_chg_config;
	struct jeita_fcc_cfg	*jeita_fcc_config;
	struct jeita_fv_cfg	*jeita_fv_config;

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;
};

#endif /* __STEP_CHG_H__ */

