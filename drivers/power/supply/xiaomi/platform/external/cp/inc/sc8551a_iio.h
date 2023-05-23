/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __SC8551A_IIO_H
#define __SC8551A_IIO_H

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct sc8551_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define SC8551_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define SC8551_CHAN_VOLT(_name, _num)			\
	SC8551_IIO_CHAN(_name, _num, IIO_VOLTAGE,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SC8551_CHAN_CUR(_name, _num)			\
	SC8551_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SC8551_CHAN_TEMP(_name, _num)			\
	SC8551_IIO_CHAN(_name, _num, IIO_TEMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SC8551_CHAN_POW(_name, _num)			\
	SC8551_IIO_CHAN(_name, _num, IIO_POWER,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SC8551_CHAN_ENERGY(_name, _num)			\
	SC8551_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define SC8551_CHAN_COUNT(_name, _num)			\
	SC8551_IIO_CHAN(_name, _num, IIO_COUNT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sc8551_iio_channels sc8551_iio_psy_channels[] = {
	SC8551_CHAN_ENERGY("sc_present", PSY_IIO_SC_PRESENT)
	SC8551_CHAN_ENERGY("sc_charging_enabled", PSY_IIO_SC_CHARGING_ENABLED)
	SC8551_CHAN_ENERGY("sc_status", PSY_IIO_SC_STATUS)
	SC8551_CHAN_ENERGY("sc_battery_present", PSY_IIO_SC_BATTERY_PRESENT)
	SC8551_CHAN_ENERGY("sc_vbus_present", PSY_IIO_SC_VBUS_PRESENT)
	SC8551_CHAN_VOLT("sc_battery_voltage", PSY_IIO_SC_BATTERY_VOLTAGE)
	SC8551_CHAN_CUR("sc_battery_current", PSY_IIO_SC_BATTERY_CURRENT)
	SC8551_CHAN_TEMP("sc_battery_temperature", PSY_IIO_SC_BATTERY_TEMPERATURE)
	SC8551_CHAN_VOLT("sc_bus_voltage", PSY_IIO_SC_BUS_VOLTAGE)
	SC8551_CHAN_CUR("sc_bus_current", PSY_IIO_SC_BUS_CURRENT)
	SC8551_CHAN_TEMP("sc_bus_temperature", PSY_IIO_SC_BUS_TEMPERATURE)
	SC8551_CHAN_TEMP("sc_die_temperature", PSY_IIO_SC_DIE_TEMPERATURE)
	SC8551_CHAN_ENERGY("sc_alarm_status", PSY_IIO_SC_ALARM_STATUS)
	SC8551_CHAN_ENERGY("sc_fault_status", PSY_IIO_SC_FAULT_STATUS)
	SC8551_CHAN_ENERGY("sc_vbus_error_status", PSY_IIO_SC_VBUS_ERROR_STATUS)
	SC8551_CHAN_ENERGY("sc_enable_adc", PSY_IIO_SC_ENABLE_ADC)
};

static const struct sc8551_iio_channels sc8551_slave_iio_psy_channels[] = {
	SC8551_CHAN_ENERGY("sc_present_slave", PSY_IIO_SC_PRESENT)
	SC8551_CHAN_ENERGY("sc_charging_enabled_slave", PSY_IIO_SC_CHARGING_ENABLED)
	SC8551_CHAN_ENERGY("sc_status_slave", PSY_IIO_SC_STATUS)
	SC8551_CHAN_ENERGY("sc_battery_present_slave", PSY_IIO_SC_BATTERY_PRESENT)
	SC8551_CHAN_ENERGY("sc_vbus_present_slave", PSY_IIO_SC_VBUS_PRESENT)
	SC8551_CHAN_VOLT("sc_battery_voltage_slave", PSY_IIO_SC_BATTERY_VOLTAGE)
	SC8551_CHAN_CUR("sc_battery_current_slave", PSY_IIO_SC_BATTERY_CURRENT)
	SC8551_CHAN_TEMP("sc_battery_temperature_slave", PSY_IIO_SC_BATTERY_TEMPERATURE)
	SC8551_CHAN_VOLT("sc_bus_voltage_slave", PSY_IIO_SC_BUS_VOLTAGE)
	SC8551_CHAN_CUR("sc_bus_current_slave", PSY_IIO_SC_BUS_CURRENT)
	SC8551_CHAN_TEMP("sc_bus_temperature_slave", PSY_IIO_SC_BUS_TEMPERATURE)
	SC8551_CHAN_TEMP("sc_die_temperature_slave", PSY_IIO_SC_DIE_TEMPERATURE)
	SC8551_CHAN_ENERGY("sc_alarm_status_slave", PSY_IIO_SC_ALARM_STATUS)
	SC8551_CHAN_ENERGY("sc_fault_status_slave", PSY_IIO_SC_FAULT_STATUS)
	SC8551_CHAN_ENERGY("sc_vbus_error_status_slave", PSY_IIO_SC_VBUS_ERROR_STATUS)
	SC8551_CHAN_ENERGY("sc_enable_adc_slave", PSY_IIO_SC_ENABLE_ADC)
};

int sc_init_iio_psy(struct sc8551 *chip);

#endif /* __SC8551A_IIO_H */