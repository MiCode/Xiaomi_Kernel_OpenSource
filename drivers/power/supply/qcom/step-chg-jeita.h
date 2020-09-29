/* Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#define MAX_STEP_CHG_ENTRIES	5

#define BATT_CP_COOL_THRESHOLD		100
#define BATT_CP_WARM_THRESHOLD		450

#define BATT_COOL_THRESHOLD		150
#define BATT_WARM_THRESHOLD		450

#define HVDCP3_CLASS_B_27W		2

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
#endif /* __STEP_CHG_H__ */
