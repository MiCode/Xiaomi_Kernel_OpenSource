/* Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __STEP_CHG_H__
#define __STEP_CHG_H__

#define BATT_CP_COOL_THRESHOLD		150
#define BATT_CP_WARM_THRESHOLD		450
#define SOC_CHG_VOTER		"SOC_CHG_VOTER"
#define SOC_FCC_LIMIT		5300000

#define MAX_STEP_CHG_ENTRIES	5
#define BATT_COOL_THRESHOLD		150
#define BATT_WARM_THRESHOLD		450
#define FFC_CHG_TERM_TEMP_THRESHOLD	350
#define FFC_LOW_TEMP_CHG_TERM_CURRENT	-980
#define FFC_HIGH_TEMP_CHG_TERM_CURRENT	-1110

enum hvdcp3_class_type {
	HVDCP3_CLASS_NONE = 0,
	HVDCP3_CLASS_A_18W,
	HVDCP3_CLASS_B_27W,
	HVDCP3P5_CLASS_A_18W,
	HVDCP3P5_CLASS_B_27W,
};

struct step_chg_jeita_param {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	bool			use_bms;
};

struct range_data {
	int low_threshold;
	int high_threshold;
	u32 value;
};

int qcom_step_chg_init(struct device *dev,
		bool step_chg_enable, bool sw_jeita_enable, bool jeita_arb_en);
void qcom_step_chg_deinit(void);
int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		int max_threshold, u32 max_value);
int get_val(struct range_data *range, int hysteresis, int current_index,
		int threshold, int *new_index, int *val);
#endif /* __STEP_CHG_H__ */
