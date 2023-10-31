/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __SH366101_IIO_H
#define __SH366101_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/qti_power_supply.h>

struct sh366101_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define SH366101_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define SH366101_CHAN_CURRENT(_name, _num)			\
	SH366101_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct sh366101_iio_channels sh366101_iio_psy_channels[] = {
	SH366101_CHAN_CURRENT("shutdown_delay", PSY_IIO_SHUTDOWN_DELAY)
	SH366101_CHAN_CURRENT("resistance", PSY_IIO_RESISTANCE)
	SH366101_CHAN_CURRENT("resistance_id", PSY_IIO_RESISTANCE_ID)
	//SH366101_CHAN_CURRENT("soc_decimal", PSY_IIO_SOC_DECIMAL)
	//SH366101_CHAN_CURRENT("soc_decimal_rate", PSY_IIO_SOC_DECIMAL_RATE)
	//SH366101_CHAN_CURRENT("fastcharge_mode", PSY_IIO_FASTCHARGE_MODE)
	SH366101_CHAN_CURRENT("raw_soc", PSY_IIO_RAW_SOC)
	SH366101_CHAN_CURRENT("battery_type", PSY_IIO_BATTERY_TYPE)
	SH366101_CHAN_CURRENT("soh", PSY_IIO_SOH)
	//SH366101_CHAN_CURRENT("fg_monitor_work", PSY_IIO_FG_MONITOR_WORK)
	SH366101_CHAN_CURRENT("fg_batt_id", PSY_IIO_BATT_ID)
	SH366101_CHAN_CURRENT("fg_version", PSY_IIO_VERSION)
};

#endif
