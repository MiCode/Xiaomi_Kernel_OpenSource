/*
 * bq2419x-charger.h -- BQ24190/BQ24192/BQ24192i/BQ24193 Charger driver
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.

 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 * Author: Syed Rafiuddin <srafiuddin@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 *
 */

#ifndef __LINUX_POWER_BQ2419X_CHARGER_H
#define __LINUX_POWER_BQ2419X_CHARGER_H


/* Register definitions */
#define BQ2419X_INPUT_SRC_REG		0x00
#define BQ2419X_PWR_ON_REG		0x01
#define BQ2419X_CHRG_CTRL_REG		0x02
#define BQ2419X_CHRG_TERM_REG		0x03
#define BQ2419X_VOLT_CTRL_REG		0x04
#define BQ2419X_TIME_CTRL_REG		0x05
#define BQ2419X_THERM_REG		0x06
#define BQ2419X_MISC_OPER_REG		0x07
#define BQ2419X_SYS_STAT_REG		0x08
#define BQ2419X_FAULT_REG		0x09
#define BQ2419X_REVISION_REG		0x0a

#define BQ2419X_INPUT_VINDPM_MASK	0x78
#define BQ2419X_INPUT_IINLIM_MASK	0x07

#define BQ2419X_CHRG_CTRL_ICHG_MASK	0xFC

#define BQ2419X_CHRG_TERM_PRECHG_MASK	0xF0
#define BQ2419X_CHRG_TERM_TERM_MASK	0x0F

#define BQ2419X_THERM_BAT_COMP_MASK	0xE0
#define BQ2419X_THERM_VCLAMP_MASK	0x1C
#define BQ2419X_THERM_TREG_MASK		0x03

#define BQ2419X_TIME_JEITA_ISET		0x01

#define BQ2419X_CHG_VOLT_LIMIT_MASK	0xFC

#define BQ24190_IC_VER			0x40
#define BQ24192_IC_VER			0x28
#define BQ24192i_IC_VER			0x18

#define BQ2419X_ENABLE_CHARGE_MASK	0x30
#define BQ2419X_ENABLE_VBUS		0x20
#define BQ2419X_ENABLE_CHARGE		0x10
#define BQ2419X_DISABLE_CHARGE		0x00

#define BQ2419X_REG0			0x0
#define BQ2419X_EN_HIZ			BIT(7)

#define BQ2419X_WD			0x5
#define BQ2419X_WD_MASK			0x30
#define BQ2419X_EN_SFT_TIMER_MASK	BIT(3)
#define BQ2419X_WD_DISABLE		0x00
#define BQ2419X_WD_40ms			0x10
#define BQ2419X_WD_80ms			0x20
#define BQ2419X_WD_160ms		0x30

#define BQ2419x_VBUS_STAT		0xc0
#define BQ2419x_VBUS_UNKNOWN		0x00
#define BQ2419x_VBUS_USB		0x40
#define BQ2419x_VBUS_AC			0x80

#define BQ2419x_CHRG_STATE_MASK			0x30
#define BQ2419x_VSYS_STAT_MASK			0x01
#define BQ2419x_VSYS_STAT_BATT_LOW		0x01
#define BQ2419x_CHRG_STATE_NOTCHARGING		0x00
#define BQ2419x_CHRG_STATE_PRE_CHARGE		0x10
#define BQ2419x_CHRG_STATE_POST_CHARGE		0x20
#define BQ2419x_CHRG_STATE_CHARGE_DONE		0x30

#define BQ2419x_FAULT_WATCHDOG_FAULT		BIT(7)
#define BQ2419x_FAULT_BOOST_FAULT		BIT(6)
#define BQ2419x_FAULT_CHRG_FAULT_MASK		0x30
#define BQ2419x_FAULT_CHRG_NORMAL		0x00
#define BQ2419x_FAULT_CHRG_INPUT		0x10
#define BQ2419x_FAULT_CHRG_THERMAL		0x20
#define BQ2419x_FAULT_CHRG_SAFTY		0x30

#define BQ2419x_FAULT_NTC_FAULT			0x07
#define BQ2419x_TREG				0x03
#define BQ2419x_TREG_100_C			0x02

#define BQ2419x_CONFIG_MASK		0x7
#define BQ2419x_INPUT_VOLTAGE_MASK	0x78
#define BQ2419x_NVCHARGER_INPUT_VOL_SEL	0x40
#define BQ2419x_DEFAULT_INPUT_VOL_SEL	0x30
#define BQ2419x_VOLTAGE_CTRL_MASK	0xFC

#define BQ2419x_CHARGING_CURRENT_STEP_DELAY_US	1000

#define BQ2419X_MAX_REGS		(BQ2419X_REVISION_REG + 1)

/*
 * struct bq2419x_vbus_platform_data - bq2419x VBUS platform data.
 *
 * @gpio_otg_iusb: GPIO number for OTG/IUSB
 * @num_consumer_supplies: Number fo consumer for vbus regulators.
 * @consumer_supplies: List of consumer suppliers.
 */
struct bq2419x_vbus_platform_data {
	int gpio_otg_iusb;
	int num_consumer_supplies;
	struct regulator_consumer_supply *consumer_supplies;
};

/*
 * struct bq2419x_charger_platform_data - bq2419x charger platform data.
 */
struct bq2419x_charger_platform_data {
	int input_voltage_limit_mV;
	int fast_charge_current_limit_mA;
	int pre_charge_current_limit_mA;
	int termination_current_limit_mA;
	int ir_compensation_resister_ohm;
	int ir_compensation_voltage_mV;
	int thermal_regulation_threshold_degC;
	int charge_voltage_limit_mV;
	int max_charge_current_mA;
	int wdt_timeout;
	int rtc_alarm_time;
	int num_consumer_supplies;
	struct regulator_consumer_supply *consumer_supplies;
	int chg_restart_time;
	const char *tz_name; /* Thermal zone name */
	bool disable_suspend_during_charging;
	bool enable_thermal_monitor; /* TRUE if FuelGauge provides temp */
	int temp_polling_time_sec;
	int n_temp_profile;
	u32 *temp_range;
	u32 *chg_current_limit;
};

/*
 * struct bq2419x_platform_data - bq2419x platform data.
 */
struct bq2419x_platform_data {
	struct bq2419x_vbus_platform_data *vbus_pdata;
	struct bq2419x_charger_platform_data *bcharger_pdata;
};

#endif /* __LINUX_POWER_BQ2419X_CHARGER_H */
