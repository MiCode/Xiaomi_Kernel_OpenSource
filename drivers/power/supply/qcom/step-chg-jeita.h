/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
 */

#ifndef __STEP_CHG_H__
#define __STEP_CHG_H__

#define MAX_STEP_CHG_ENTRIES	8

struct step_chg_jeita_param {
	u32			psy_prop;
	char			*prop_name;
	int			hysteresis;
	bool			use_bms;
};

struct range_data {
	u32 low_threshold;
	u32 high_threshold;
	u32 value;
};

int qcom_step_chg_init(struct device *dev,
		bool step_chg_enable, bool sw_jeita_enable, bool jeita_arb_en);
void qcom_step_chg_deinit(void);
int read_range_data_from_node(struct device_node *node,
		const char *prop_str, struct range_data *ranges,
		u32 max_threshold, u32 max_value);
#endif /* __STEP_CHG_H__ */
