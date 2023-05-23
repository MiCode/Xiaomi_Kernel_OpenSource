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

#define STEP_CHG_HYSTERISIS_DELAY_US		5000000 /* 5 secs */

#define BATT_HOT_DECIDEGREE_MAX			600
#define GET_CONFIG_DELAY_MS		2000
#define GET_CONFIG_RETRY_COUNT		50
#define WAIT_BATT_ID_READY_MS		200

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
	ktime_t			step_last_update_time;
	ktime_t			jeita_last_update_time;
	bool			step_chg_enable;
	bool			sw_jeita_enable;
	bool			jeita_arb_en;
	bool			config_is_read;
	bool			step_chg_cfg_valid;
	bool			sw_jeita_cfg_valid;
	bool			batt_missing;
	bool			taper_fcc;
	int			jeita_fcc_index;
	int			jeita_fv_index;
	int			step_index;
	int			get_config_retry_count;

	struct step_chg_cfg	*step_chg_config;
	struct jeita_fcc_cfg	*jeita_fcc_config;
	struct jeita_fv_cfg	*jeita_fv_config;

	struct votable		*fcc_votable;
	struct votable		*fv_votable;
	struct votable		*usb_icl_votable;
	struct wakeup_source	*step_chg_ws;
	struct power_supply	*batt_psy;
	struct power_supply	*usb_psy;
	struct delayed_work	status_change_work;
	struct delayed_work	get_config_work;
	struct notifier_block	nb;
};

#endif /* __STEP_CHG_H__ */

