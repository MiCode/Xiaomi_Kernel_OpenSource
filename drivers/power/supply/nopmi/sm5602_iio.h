/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __SM5602_IIO_H
#define __SM5602_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/qti_power_supply.h>

struct sm5602_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define SM5602_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define SM5602_CHAN_CURRENT(_name, _num)			\
	SM5602_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sm5602_iio_channels sm5602_iio_psy_channels[] = {
	SM5602_CHAN_CURRENT("shutdown_delay", PSY_IIO_SHUTDOWN_DELAY)
	SM5602_CHAN_CURRENT("resistance", PSY_IIO_RESISTANCE)
	SM5602_CHAN_CURRENT("resistance_id", PSY_IIO_RESISTANCE_ID)
	SM5602_CHAN_CURRENT("soc_decimal", PSY_IIO_SOC_DECIMAL)
	SM5602_CHAN_CURRENT("soc_decimal_rate", PSY_IIO_SOC_DECIMAL_RATE)
	SM5602_CHAN_CURRENT("fastcharge_mode", PSY_IIO_FASTCHARGE_MODE)
	SM5602_CHAN_CURRENT("battery_type", PSY_IIO_BATTERY_TYPE)
	SM5602_CHAN_CURRENT("soh", PSY_IIO_SOH)
	SM5602_CHAN_CURRENT("fg_monitor_work", PSY_IIO_FG_MONITOR_WORK)
	SM5602_CHAN_CURRENT("fg_batt_id", PSY_IIO_BATT_ID)
};

#endif
