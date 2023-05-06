
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __LN8000_IIO_H
#define __LN8000_IIO_H

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

#define LN8000_RCP_PATCH                1

struct ln8000_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define LN8000_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define LN8000_CHAN_VOLT(_name, _num)			\
	LN8000_IIO_CHAN(_name, _num, IIO_VOLTAGE,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define LN8000_CHAN_CUR(_name, _num)			\
	LN8000_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define LN8000_CHAN_TEMP(_name, _num)			\
	LN8000_IIO_CHAN(_name, _num, IIO_TEMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define LN8000_CHAN_POW(_name, _num)			\
	LN8000_IIO_CHAN(_name, _num, IIO_POWER,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define LN8000_CHAN_ENERGY(_name, _num)			\
	LN8000_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define LN8000_CHAN_COUNT(_name, _num)			\
	LN8000_IIO_CHAN(_name, _num, IIO_COUNT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct ln8000_iio_channels ln8000_iio_psy_channels[] = {
	LN8000_CHAN_ENERGY("ln_present", PSY_IIO_SC_PRESENT)
	LN8000_CHAN_ENERGY("ln_charging_enabled", PSY_IIO_SC_CHARGING_ENABLED)
	LN8000_CHAN_ENERGY("ln_status", PSY_IIO_SC_STATUS)
	LN8000_CHAN_ENERGY("ln_battery_present", PSY_IIO_SC_BATTERY_PRESENT)
	LN8000_CHAN_ENERGY("ln_vbus_present", PSY_IIO_SC_VBUS_PRESENT)
	LN8000_CHAN_VOLT("ln_battery_voltage", PSY_IIO_SC_BATTERY_VOLTAGE)
	LN8000_CHAN_CUR("ln_battery_current", PSY_IIO_SC_BATTERY_CURRENT)
	LN8000_CHAN_TEMP("ln_battery_temperature", PSY_IIO_SC_BATTERY_TEMPERATURE)
	LN8000_CHAN_VOLT("ln_bus_voltage", PSY_IIO_SC_BUS_VOLTAGE)
	LN8000_CHAN_CUR("ln_bus_current", PSY_IIO_SC_BUS_CURRENT)
	LN8000_CHAN_TEMP("ln_bus_temperature", PSY_IIO_SC_BUS_TEMPERATURE)
	LN8000_CHAN_TEMP("ln_die_temperature", PSY_IIO_SC_DIE_TEMPERATURE)
	LN8000_CHAN_ENERGY("ln_alarm_status", PSY_IIO_SC_ALARM_STATUS)
	LN8000_CHAN_ENERGY("ln_fault_status", PSY_IIO_SC_FAULT_STATUS)
	LN8000_CHAN_ENERGY("ln_vbus_error_status", PSY_IIO_SC_VBUS_ERROR_STATUS)
	LN8000_CHAN_ENERGY("ln_reg_status", PSY_IIO_SC_REG_STATUS)
};

static const struct ln8000_iio_channels ln8000_slave_iio_psy_channels[] = {
	LN8000_CHAN_ENERGY("ln_present_slave", PSY_IIO_SC_PRESENT)
	LN8000_CHAN_ENERGY("ln_charging_enabled_slave", PSY_IIO_SC_CHARGING_ENABLED)
	LN8000_CHAN_ENERGY("ln_status_slave", PSY_IIO_SC_STATUS)
	LN8000_CHAN_ENERGY("ln_battery_present_slave", PSY_IIO_SC_BATTERY_PRESENT)
	LN8000_CHAN_ENERGY("ln_vbus_present_slave", PSY_IIO_SC_VBUS_PRESENT)
	LN8000_CHAN_VOLT("ln_battery_voltage_slave", PSY_IIO_SC_BATTERY_VOLTAGE)
	LN8000_CHAN_CUR("ln_battery_current_slave", PSY_IIO_SC_BATTERY_CURRENT)
	LN8000_CHAN_TEMP("ln_battery_temperature_slave", PSY_IIO_SC_BATTERY_TEMPERATURE)
	LN8000_CHAN_VOLT("ln_bus_voltage_slave", PSY_IIO_SC_BUS_VOLTAGE)
	LN8000_CHAN_CUR("ln_bus_current_slave", PSY_IIO_SC_BUS_CURRENT)
	LN8000_CHAN_TEMP("ln_bus_temperature_slave", PSY_IIO_SC_BUS_TEMPERATURE)
	LN8000_CHAN_TEMP("ln_die_temperature_slave", PSY_IIO_SC_DIE_TEMPERATURE)
	LN8000_CHAN_ENERGY("ln_alarm_status_slave", PSY_IIO_SC_ALARM_STATUS)
	LN8000_CHAN_ENERGY("ln_fault_status_slave", PSY_IIO_SC_FAULT_STATUS)
	LN8000_CHAN_ENERGY("ln_vbus_error_status_slave", PSY_IIO_SC_VBUS_ERROR_STATUS)
	LN8000_CHAN_ENERGY("ln_reg_status_slave", PSY_IIO_SC_REG_STATUS)
};

int ln_init_iio_psy(struct ln8000_info *chip);

#endif /* __LN8000_IIO_H */

