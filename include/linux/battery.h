/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017, 2019 The Linux Foundation. All rights reserved.
 */

#ifndef __BATTERY_H
#define __BATTERY_H

#define BATT_COOL_THRESHOLD		150
#define BATT_WARM_THRESHOLD		480

struct charger_param {
	u32 fcc_step_delay_ms;
	u32 fcc_step_size_ua;
	u32 hvdcp3_max_icl_ua;
};

int mtk_batt_init(struct charger_param *param);
void mtk_batt_deinit(void);
#endif /* __BATTERY_H */
