/*
 * include/linux/smb349-charger.h
 *
 * Battery charger driver interface for Summit SMB349
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __LINUX_SMB349_CHARGER_H
#define __LINUX_SMB349_CHARGER_H

#include <linux/regulator/machine.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/usb/otg.h>

struct smb349_charger_platform_data {
	int regulator_id;
	int max_charge_volt_mV;
	int max_charge_current_mA;
	int charging_term_current_mA;
	int num_consumer_supplies;
	struct regulator_consumer_supply *consumer_supplies;
	int otg_regulator_id;
	int num_otg_consumer_supplies;
	struct regulator_consumer_supply *otg_consumer_supplies;
};

enum charging_states {
	idle,
	progress,
	completed,
	stopped,
};

enum charger_type {
	NONE,
	AC,
	USB,
};

typedef void (*charging_callback_t)(enum charging_states state,
enum charger_type chrg_type, void *args);

struct smb349_charger {
	struct i2c_client	*client;
	struct device	*dev;
	void	*charger_cb_data;
	enum charging_states state;
	enum charger_type chrg_type;
	charging_callback_t	charger_cb;

	int is_otg_enabled;
	struct regulator_dev    *rdev;
	struct regulator_desc   reg_desc;
	struct regulator_init_data      reg_init_data;
	struct regulator_dev    *otg_rdev;
	struct regulator_desc   otg_reg_desc;
	struct regulator_init_data      otg_reg_init_data;
};

int smb349_battery_online(void);
/*
 * Register callback function for the client.
 * Used by fuel-gauge driver to get battery charging properties.
 */
extern int register_callback(charging_callback_t cb, void *args);
extern int update_charger_status(void);

#endif /*__LINUX_SMB349_CHARGER_H */
