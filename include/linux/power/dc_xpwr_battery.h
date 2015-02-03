/*
 * dollar_cove_battery.h: platform data struct for dollar cove battery
 *
 * Copyright (C) 2013 Intel Corporation
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __DOLLAR_COVE_BATTERY_H__
#define __DOLLAR_COVE_BATTERY_H__

#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>

#define XPWR_FG_DATA_SIZE	36
#define XPWR_BAT_CURVE_SIZE	32
#define ACPI_FG_CONF_NAME_LEN	8

struct dc_xpwr_fg_config_data {
	char fg_name[ACPI_FG_CONF_NAME_LEN];
	char battid[BATTID_STR_LEN];
	u16 size; /* config size */
	u8 fco; /* FG config options */
	u16 checksum; /* Primary data checksum */
	u8 cap1;
	u8 cap0;
	u8 rdc1;
	u8 rdc0;
	u8 bat_curve[XPWR_BAT_CURVE_SIZE];
} __packed;

struct dc_xpwr_acpi_fg_config {
	struct acpi_table_header acpi_header;
	struct dc_xpwr_fg_config_data cdata;
} __packed;

struct dollarcove_fg_pdata {
	char battid[BATTID_STR_LEN + 1];

	int design_cap;
	int design_min_volt;
	int design_max_volt;
	int max_temp;
	int min_temp;
	int fg_save_restore_enabled;

	struct dc_xpwr_fg_config_data cdata;
};

#endif	/* __DOLLAR_COVE_BATTERY_H__ */
