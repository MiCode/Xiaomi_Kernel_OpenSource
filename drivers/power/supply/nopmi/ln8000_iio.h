/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */
#ifndef __LN8000_IIO_H
#define __LN8000_IIO_H
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
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
#define LN8000_CHAN_ENERGY(_name, _num)			\
	LN8000_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))
static const struct ln8000_iio_channels ln8000_iio_psy_channels[] = {
	LN8000_CHAN_ENERGY("ln_status", PSY_IIO_STATUS)
	LN8000_CHAN_ENERGY("ln_present", PSY_IIO_PRESENT)
	LN8000_CHAN_ENERGY("ln_constant_charge_voltage", PSY_IIO_CONSTANT_CHARGE_VOLTAGE)
	LN8000_CHAN_ENERGY("ln_input_current_limit", PSY_IIO_CP_INPUT_CURRENT_LIMIT)
	LN8000_CHAN_ENERGY("ln_model_name", PSY_IIO_MODEL_NAME)
	LN8000_CHAN_ENERGY("ln_charging_enabled", PSY_IIO_CHARGING_ENABLED)
	LN8000_CHAN_ENERGY("ln_ti_bypass_mode_enable", PSY_IIO_TI_BYPASS_MODE_ENABLED)
	LN8000_CHAN_ENERGY("ln_ti_set_bus_protection_for_qc3", PSY_IIO_TI_SET_BUS_PROTECTION_FOR_QC3)
	LN8000_CHAN_ENERGY("ln_battery_present", PSY_IIO_SC_BATTERY_PRESENT)
	LN8000_CHAN_ENERGY("ln_vbus_present", PSY_IIO_SC_VBUS_PRESENT)
	LN8000_CHAN_ENERGY("ln_battery_voltage", PSY_IIO_SC_BATTERY_VOLTAGE)
	LN8000_CHAN_ENERGY("ln_battery_current", PSY_IIO_SC_BATTERY_CURRENT)
	LN8000_CHAN_ENERGY("ln_battery_temperture", PSY_IIO_SC_BATTERY_TEMPERATURE)
	LN8000_CHAN_ENERGY("ln_bus_voltage", PSY_IIO_SC_BUS_VOLTAGE)
	LN8000_CHAN_ENERGY("ln_bus_current", PSY_IIO_SC_BUS_CURRENT)
	LN8000_CHAN_ENERGY("ln_bus_temperature", PSY_IIO_SC_BUS_TEMPERATURE)
	LN8000_CHAN_ENERGY("ln_die_temperature", PSY_IIO_SC_DIE_TEMPERATURE)
	LN8000_CHAN_ENERGY("ln_alarm_status", PSY_IIO_SC_ALARM_STATUS)
	LN8000_CHAN_ENERGY("ln_fault_status", PSY_IIO_SC_FAULT_STATUS)
	LN8000_CHAN_ENERGY("ln_ti_reg_status", PSY_IIO_TI_REG_STATUS)
	LN8000_CHAN_ENERGY("ln_hv_charge_enable", PSY_IIO_HV_CHARGE_ENABLED)
	LN8000_CHAN_ENERGY("ln_vbus_error_status", PSY_IIO_SC_VBUS_ERROR_STATUS)
	LN8000_CHAN_ENERGY("ln_chip_id", PSY_IIO_DEV_CHIP_ID)
};

#endif
