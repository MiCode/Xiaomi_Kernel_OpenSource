/* Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#define MAX_STEP_CHG_ENTRIES	8

#define BATT_COOL_THRESHOLD		150
#define BATT_WARM_THRESHOLD		480
#define FFC_CHG_TERM_TEMP_THRESHOLD	350
#define FFC_LOW_TEMP_CHG_TERM_CURRENT	-710
#define FFC_HIGH_TEMP_CHG_TERM_CURRENT	-760

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

enum step_hvdcp3_type {
	STEP_HVDCP3_NONE = 0,
	STEP_HVDCP3_CLASSA_18W,
	STEP_HVDCP3_CLASSB_27W,
};

int qcom_step_chg_init(struct device *dev,
		bool step_chg_enable, bool sw_jeita_enable, bool jeita_arb_en);
void qcom_step_chg_deinit(void);
int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		int max_threshold, u32 max_value);
#endif /* __STEP_CHG_H__ */
