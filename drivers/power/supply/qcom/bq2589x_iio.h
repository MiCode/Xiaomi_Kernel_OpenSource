/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __BQ25890_IIO_H
#define __BQ25890_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>

struct bq25890_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define BQ25890_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define BQ25890_CHAN_ENERGY(_name, _num)			\
	BQ25890_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct bq25890_iio_channels bq25890_iio_psy_channels[] = {
	BQ25890_CHAN_ENERGY("charge_done", PSY_IIO_CHARGE_DONE)
	BQ25890_CHAN_ENERGY("main_chager_hz", PSY_IIO_MAIN_CHAGER_HZ)
	BQ25890_CHAN_ENERGY("main_input_current_settled", PSY_IIO_MAIN_INPUT_CURRENT_SETTLED)
	BQ25890_CHAN_ENERGY("input_voltage_settled", PSY_IIO_MAIN_INPUT_VOLTAGE_SETTLED)
	BQ25890_CHAN_ENERGY("main_charge_current", PSY_IIO_MAIN_CHAGER_CURRENT)
	BQ25890_CHAN_ENERGY("charger_enable", PSY_IIO_CHARGING_ENABLED)
	BQ25890_CHAN_ENERGY("otg_enable", PSY_IIO_OTG_ENABLE)
	BQ25890_CHAN_ENERGY("main_charger_term", PSY_IIO_MAIN_CHAGER_TERM)
	BQ25890_CHAN_ENERGY("batt_voltage_term", PSY_IIO_BATTERY_VOLTAGE_TERM)
	BQ25890_CHAN_ENERGY("charger_status", PSY_IIO_CHARGER_STATUS)
	BQ25890_CHAN_ENERGY("charger_type", PSY_IIO_CHARGE_TYPE)
	BQ25890_CHAN_ENERGY("vbus_voltage", PSY_IIO_SC_BUS_VOLTAGE)
	BQ25890_CHAN_ENERGY("vbat_voltage", PSY_IIO_SC_BATTERY_VOLTAGE)
	BQ25890_CHAN_ENERGY("enable_charger_term", PSY_IIO_ENABLE_CHAGER_TERM)
};

#endif