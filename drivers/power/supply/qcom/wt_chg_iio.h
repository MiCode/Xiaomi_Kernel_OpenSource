/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __BATTERY_IIO_H
#define __BATTERY_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

#define IIO_WT_SECOND_OFFSET 10

struct battery_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define BATTERY_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define BATTERY_CHAN_ENERGY(_name, _num)			\
	BATTERY_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct battery_iio_channels battery_iio_psy_channels[] = {
	BATTERY_CHAN_ENERGY("pd_active", PSY_IIO_PD_ACTIVE)
	BATTERY_CHAN_ENERGY("pd_usb_suspend_supported", PSY_IIO_PD_USB_SUSPEND_SUPPORTED)
	BATTERY_CHAN_ENERGY("pd_in_hard_reset", PSY_IIO_PD_IN_HARD_RESET)
	BATTERY_CHAN_ENERGY("pd_current_max", PSY_IIO_PD_CURRENT_MAX)
	BATTERY_CHAN_ENERGY("pd_voltage_min", PSY_IIO_PD_VOLTAGE_MIN)
	BATTERY_CHAN_ENERGY("pd_voltage_max", PSY_IIO_PD_VOLTAGE_MAX)
	BATTERY_CHAN_ENERGY("real_type", PSY_IIO_USB_REAL_TYPE)
	BATTERY_CHAN_ENERGY("otg_enable", PSY_IIO_OTG_ENABLE)
	BATTERY_CHAN_ENERGY("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	BATTERY_CHAN_ENERGY("apdo_max_volt", PSY_IIO_APDO_MAX_VOLT)
	BATTERY_CHAN_ENERGY("apdo_max_curr", PSY_IIO_APDO_MAX_CURR)
	BATTERY_CHAN_ENERGY("typec_mode", PSY_IIO_TYPEC_MODE)
};

enum batt_qg_exit_iio_channels {
	BATT_QG_PRESENT,
	BATT_QG_STATUS,
	BATT_QG_CAPACITY,
	BATT_QG_CURRENT_NOW,
	BATT_QG_VOLTAGE_NOW,
	BATT_QG_VOLTAGE_MAX,
	BATT_QG_CHARGE_FULL,
	BATT_QG_RESISTANCE_ID,
	BATT_QG_TEMP,
	BATT_QG_CYCLE_COUNT,
	BATT_QG_CHARGE_FULL_DESIGN,
	BATT_QG_TIME_TO_FULL_NOW,
	BATT_QG_TIME_TO_EMPTY_NOW,
	BATT_QG_FCC_MAX,
	BATT_QG_CHIP_OK,
	BATT_QG_BATTERY_AUTH,
	BATT_QG_SOC_DECIMAL,
	BATT_QG_SOC_DECIMAL_RATE,
	BATT_QG_BATTERY_ID,
	BATT_QG_CC_SOC,
	BATT_QG_SHUTDOWN_DELAY,
	BATT_QG_FASTCHARGE_MODE,
};

static const char * const qg_ext_iio_chan_name[] = {
	[BATT_QG_PRESENT] = "present",
	[BATT_QG_STATUS] = "status",
	[BATT_QG_CAPACITY] = "capacity",
	[BATT_QG_CURRENT_NOW] = "current_now",
	[BATT_QG_VOLTAGE_NOW] = "voltage_now",
	[BATT_QG_VOLTAGE_MAX] = "voltage_max",
	[BATT_QG_CHARGE_FULL] = "charge_full",
	[BATT_QG_RESISTANCE_ID] = "resistance_id",
	[BATT_QG_TEMP] = "temp",
	[BATT_QG_CYCLE_COUNT] = "cycle_count",
	[BATT_QG_CHARGE_FULL_DESIGN] = "charge_full_design",
	[BATT_QG_TIME_TO_FULL_NOW] = "time_to_full_now",
	[BATT_QG_TIME_TO_EMPTY_NOW] = "time_to_empty_now",
	[BATT_QG_FCC_MAX] = "therm_curr",
	[BATT_QG_CHIP_OK] = "chip_ok",
	[BATT_QG_BATTERY_AUTH] = "battery_auth",
	[BATT_QG_SOC_DECIMAL] = "soc_decimal",
	[BATT_QG_SOC_DECIMAL_RATE] = "soc_decimal_rate",
	[BATT_QG_BATTERY_ID] = "battery_id",
	[BATT_QG_CC_SOC] = "rsoc",
	[BATT_QG_SHUTDOWN_DELAY] = "shutdown_delay",
	[BATT_QG_FASTCHARGE_MODE] = "fastcharge_mode",
};

enum cp_iio_channels {
	CHARGE_PUMP_CHARGING_ENABLED,
	CHARGE_PUMP_SC_BATTERY_VOLTAGE,
	CHARGE_PUMP_SC_BUS_VOLTAGE,
	CHARGE_PUMP_SC_BUS_CURRENT,
	CHARGE_PUMP_SC_PRESENT,
	CHARGE_PUMP_SC_ADC_ENABLE,
	CHARGE_PUMP_LN_CHARGING_ENABLED = IIO_WT_SECOND_OFFSET,
	CHARGE_PUMP_LN_BATTERY_VOLTAGE,
	CHARGE_PUMP_LN_BUS_VOLTAGE,
	CHARGE_PUMP_LN_BUS_CURRENT,
	CHARGE_PUMP_LN_PRESENT,
};

static const char * const cp_iio_chan_name[] = {
	[CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled",
	[CHARGE_PUMP_SC_BATTERY_VOLTAGE] = "sc_battery_voltage",
	[CHARGE_PUMP_SC_BUS_VOLTAGE] = "sc_bus_voltage",
	[CHARGE_PUMP_SC_BUS_CURRENT] = "sc_bus_current",
	[CHARGE_PUMP_SC_PRESENT] = "present",
	[CHARGE_PUMP_SC_ADC_ENABLE] = "sc_enable_adc",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled",
	[CHARGE_PUMP_LN_BATTERY_VOLTAGE] = "ln_battery_voltage",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "ln_bus_voltage",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current",
	[CHARGE_PUMP_LN_PRESENT] = "ln_present",
};

static const char * const cp_sec_iio_chan_name[] = {
	[CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled_slave",
	[CHARGE_PUMP_SC_BUS_VOLTAGE] = "sc_bus_voltage_slave",
	[CHARGE_PUMP_SC_BUS_CURRENT] = "sc_bus_current_slave",
	[CHARGE_PUMP_SC_PRESENT] = "present_slave",
	[CHARGE_PUMP_SC_ADC_ENABLE] = "sc_enable_adc_slave",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled_slave",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "ln_bus_voltage_slave",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current_slave",
	[CHARGE_PUMP_LN_PRESENT] = "ln_present_slave",
};

enum main_iio_channels {
	MAIN_CHARGER_DONE,
	MAIN_CHARGER_HZ,
	MAIN_INPUT_CURRENT_SETTLED,
	MAIN_INPUT_VOLTAGE_SETTLED,
	MAIN_CHAGER_CURRENT,
	MAIN_CHARGING_ENABLED,
	MAIN_OTG_ENABLE,
	MAIN_CHAGER_TERM,
	MAIN_CHARGER_VOLTAGE_TERM,
	MAIN_CHARGER_STATUS,
	MAIN_CHARGER_TYPE,
	MAIN_BUS_VOLTAGE,
	MAIN_VBAT_VOLTAGE,
	MAIN_ENBALE_CHAGER_TERM,
};

static const char * const main_iio_chan_name[] = {
	[MAIN_CHARGER_DONE] = "charge_done",
	[MAIN_CHARGER_HZ] = "main_chager_hz",
	[MAIN_INPUT_CURRENT_SETTLED] = "main_input_current_settled",
	[MAIN_INPUT_VOLTAGE_SETTLED] = "input_voltage_settled",
	[MAIN_CHAGER_CURRENT] = "main_charge_current",
	[MAIN_CHARGING_ENABLED] = "charger_enable",
	[MAIN_OTG_ENABLE] = "otg_enable",
	[MAIN_CHAGER_TERM] = "main_charger_term",
	[MAIN_CHARGER_VOLTAGE_TERM] = "batt_voltage_term",
	[MAIN_CHARGER_STATUS] = "charger_status",
	[MAIN_CHARGER_TYPE] = "charger_type",
	[MAIN_BUS_VOLTAGE] = "vbus_voltage",
	[MAIN_VBAT_VOLTAGE] = "vbat_voltage",
	[MAIN_ENBALE_CHAGER_TERM] = "enable_charger_term",
};

struct quick_charge {
	enum power_supply_type adap_type;
	enum power_supply_quick_charge_type adap_cap;
};

#endif
