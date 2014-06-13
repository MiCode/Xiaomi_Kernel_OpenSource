/*
 * bq24192_charger.h - Charger driver for TI BQ24190/191/192/192I
 *
 * Copyright (C) 2012 Intel Corporation
 * Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>

#ifndef __BQ24192_CHARGER_H_
#define __BQ24192_CHARGER_H_

#define TEMP_NR_RNG	4
#define BATTID_STR_LEN	8
#define RANGE	25
/* User limits for sysfs charge enable/disable */
#define USER_SET_CHRG_DISABLE	0
#define USER_SET_CHRG_LMT1	1
#define USER_SET_CHRG_LMT2	2
#define USER_SET_CHRG_LMT3	3
#define USER_SET_CHRG_NOLMT	4

#define INPUT_CHRG_CURR_0	0
#define INPUT_CHRG_CURR_100	100
#define INPUT_CHRG_CURR_500	500
#define INPUT_CHRG_CURR_950	950
#define INPUT_CHRG_CURR_1500	1500
/* Charger Master Temperature Control Register */
#define MSIC_CHRTMPCTRL         0x18E
/* Higher Temprature Values*/
#define CHRTMPCTRL_TMPH_60      (3 << 6)
#define CHRTMPCTRL_TMPH_55      (2 << 6)
#define CHRTMPCTRL_TMPH_50      (1 << 6)
#define CHRTMPCTRL_TMPH_45      (0 << 6)

/* Lower Temprature Values*/
#define CHRTMPCTRL_TMPL_15      (3 << 4)
#define CHRTMPCTRL_TMPL_10      (2 << 4)
#define CHRTMPCTRL_TMPL_05      (1 << 4)
#define CHRTMPCTRL_TMPL_00      (0 << 4)

enum bq24192_bat_chrg_mode {
	BATT_CHRG_FULL = 0,
	BATT_CHRG_NORMAL = 1,
	BATT_CHRG_MAINT = 2,
	BATT_CHRG_NONE = 3
};


/*********************************************************************
 * SFI table entries Structures
 ********************************************************************/
/*********************************************************************
 *		Platform Data Section
 *********************************************************************/
/* Battery Thresholds info which need to get from SMIP area */
struct platform_batt_safety_param {
	u8 smip_rev;
	u8 fpo;		/* fixed implementation options */
	u8 fpo1;	/* fixed implementation options1 */
	u8 rsys;	/* System Resistance for Fuel gauging */

	/* Minimum voltage necessary to
	 * be able to safely shut down */
	short int vbatt_sh_min;

	/* Voltage at which the battery driver
	 * should report the LEVEL as CRITICAL */
	short int vbatt_crit;

	short int itc;		/* Charge termination current */
	short int temp_high;	/* Safe Temp Upper Limit */
	short int temp_low;	/* Safe Temp lower Limit */
	u8 brd_id;		/* Unique Board ID */
} __packed;

/* Parameters defining the range */
struct platform_temp_mon_table {
	short int temp_up_lim;
	short int temp_low_lim;
	short int rbatt;
	short int full_chrg_vol;
	short int full_chrg_cur;
	short int maint_chrg_vol_ll;
	short int maint_chrg_vol_ul;
	short int maint_chrg_cur;
} __packed;

struct platform_batt_profile {
	char batt_id[BATTID_STR_LEN];
	unsigned short int voltage_max;
	unsigned int capacity;
	u8 battery_type;
	u8 temp_mon_ranges;
	struct platform_temp_mon_table temp_mon_range[TEMP_NR_RNG];

} __packed;

struct bq24192_platform_data {
	bool slave_mode;
	short int temp_low_lim;
	short int safetemp;
	struct platform_batt_profile batt_profile;
	struct platform_batt_safety_param safety_param;
	struct power_supply_throttle *throttle_states;
	struct ps_batt_chg_prof *chg_profile;

	char **supplied_to;
	size_t	num_supplicants;
	size_t num_throttle_states;
	unsigned long supported_cables;

	/* safety charegr setting */
	int max_cc;
	int max_cv;
	int max_temp;
	int min_temp;

	/* Function pointers for platform specific initialization */
	int (*init_platform_data)(void);
	int (*get_irq_number)(void);
	int (*query_otg)(void *, void *);
	int (*drive_vbus)(bool);
	int (*get_battery_pack_temp)(int *);
	void (*free_platform_data)(void);
};

#ifdef CONFIG_CHARGER_BQ24192
extern int bq24192_slave_mode_enable_charging(int volt, int cur, int ilim);
extern int bq24192_slave_mode_disable_charging(void);
extern int bq24192_query_battery_status(void);
extern int bq24192_get_battery_pack_temp(int *temp);
extern int bq24192_get_battery_health(void);
extern bool bq24192_is_volt_shutdown_enabled(void);
#else
static int bq24192_get_battery_health(void)
{
	return 0;
}
#endif
#endif /* __BQ24192_CHARGER_H_ */
