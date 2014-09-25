/*
 * bq24261_charger.h: platform data structure for bq24261 driver
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef __BQ24261_CHARGER_H__
#define __BQ24261_CHARGER_H__

struct bq24261_plat_data {
	char **supplied_to;
	size_t num_supplicants;
	struct power_supply_throttle *throttle_states;
	size_t num_throttle_states;
	int safety_timer;
	int boost_mode_ma;
	bool is_ts_enabled;
	int max_cc;
	int max_cv;
	bool is_wdt_kick_needed;

	int (*enable_charging)(bool val);
	int (*enable_charger)(bool val);
	int (*set_inlmt)(int val);
	int (*set_cc)(int val);
	int (*set_cv)(int val);
	int (*set_iterm)(int val);
	int (*enable_vbus)(bool val);
	int (*handle_otgmode)(bool val);
};


#ifdef CONFIG_BQ24261_CHARGER
extern void bq24261_cv_to_reg(int, u8*);
extern void bq24261_cc_to_reg(int, u8*);
extern void bq24261_inlmt_to_reg(int, u8*);
extern int bq24261_get_bat_health(void);
extern int bq24261_get_bat_status(void);
#else
static int bq24261_get_bat_health(void)
{
	return 0;
}
static int bq24261_get_bat_status(void)
{
	return 0;
}
static void bq24261_cv_to_reg(int, u8 *regval)
{
	return;
}
static void bq24261_cc_to_reg(int, u8 *regval)
{
	return;
}
static void bq24261_inlmt_to_reg(int, u8 *regval)
{
	return;
}
#endif

#endif
