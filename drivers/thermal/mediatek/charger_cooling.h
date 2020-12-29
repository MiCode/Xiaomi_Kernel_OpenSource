/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _CHARGER_COOLING_H
#define _CHARGER_COOLING_H

#include <linux/thermal.h>

/*===========================================================
 *  Macro Definitions
 *===========================================================
 */
#define CHARGER_COOLING_UNLIMITED_STATE	(0)
#define CHARGER_STATE_NUM 9
#define MAX_CHARGER_COOLER_NAME_LEN		(20)

/*==================================================
 * Type Definitions
 *==================================================
 */
enum charger_type {
	SINGLE_CHARGER,
	DUAL_CHARGER,
	NUM_CHARGER_TYPE
};

/**
 * struct charger_cooling_device - data for charger cooling device
 * @name: naming string for this cooling device
 * @target_state: target cooling state which is set in set_cur_state()
 *	callback.
 * @max_state: maximum state supported for this cooling device
 * @cdev: thermal_cooling_device pointer to keep track of the
 *	registered cooling device.
 * @throttle: callback function to handle throttle request
 * @dev: device node pointer
 * @chg_psy: master charger power supply handle
 * @s_chg_psy: slave charger power supply handle
 */
struct charger_cooling_device {
	char name[MAX_CHARGER_COOLER_NAME_LEN];
	unsigned long target_state;
	unsigned long max_state;
	struct thermal_cooling_device *cdev;
	int (*throttle)(struct charger_cooling_device *charger_cdev, unsigned long state);
	struct device *dev;
	enum charger_type type;
	struct power_supply *chg_psy;
	struct power_supply *s_chg_psy;
	const struct charger_cooling_platform_data *pdata;
};

/**
 * struct charger_cooling_platform_data - charger cooler platform dependent data
 * @state_to_charger_lv: callback function to transfer cooling state
 *			to chargering current LV
 * @max_lv: max cooler LV supported by charger
 */
struct charger_cooling_platform_data {
	int (*state_to_charger_limit)(struct charger_cooling_device *chg);
};
#endif
