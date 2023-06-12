/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __QG_IIO_H
#define __QG_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct qg_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define QG_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define QG_CHAN_VOLT(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_VOLTAGE,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_CUR(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_RES(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_RESISTANCE,	\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_TEMP(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_TEMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_POW(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_POWER,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_ENERGY(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_INDEX(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_INDEX,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_ACT(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_ACTIVITY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_TSTAMP(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_TIMESTAMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QG_CHAN_COUNT(_name, _num)			\
	QG_IIO_CHAN(_name, _num, IIO_COUNT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct qg_iio_channels qg_iio_psy_channels[] = {
	QG_CHAN_ENERGY("present", PSY_IIO_PRESENT)
	QG_CHAN_ENERGY("status", PSY_IIO_STATUS)
	QG_CHAN_VOLT("voltage_now", PSY_IIO_VOLTAGE_NOW)
	QG_CHAN_VOLT("voltage_max", PSY_IIO_VOLTAGE_MAX)
	QG_CHAN_CUR("current_now", PSY_IIO_CURRENT_NOW)
	QG_CHAN_ENERGY("capacity", PSY_IIO_CAPACITY)
	QG_CHAN_ENERGY("capacity_level", PSY_IIO_CAPACITY_LEVEL)
	QG_CHAN_TEMP("temp", PSY_IIO_TEMP)
	QG_CHAN_ENERGY("time_to_empty_now", PSY_IIO_TIME_TO_EMPTY_NOW)
	QG_CHAN_ENERGY("charge_full", PSY_IIO_CHARGE_FULL)
	QG_CHAN_ENERGY("charge_full_design", PSY_IIO_CHARGE_FULL_DESIGN)
	QG_CHAN_COUNT("cycle_count", PSY_IIO_CYCLE_COUNT)
	QG_CHAN_TSTAMP("time_to_full_now", PSY_IIO_TIME_TO_FULL_NOW)
	QG_CHAN_RES("resistance_id", PSY_IIO_RESISTANCE_ID)
	QG_CHAN_ENERGY("update_now", PSY_IIO_UPDATE_NOW)
	QG_CHAN_CUR("therm_curr", PSY_IIO_PARALLEL_FCC_MAX)
	QG_CHAN_CUR("chip_ok", PSY_IIO_BMS_CHIP_OK)
	QG_CHAN_CUR("battery_auth", PSY_IIO_BATTERY_AUTH)
	QG_CHAN_CUR("soc_decimal", PSY_IIO_BMS_SOC_DECIMAL)
	QG_CHAN_CUR("soc_decimal_rate", PSY_IIO_BMS_SOC_DECIMAL_RATE)
	QG_CHAN_CUR("soh", PSY_IIO_SOH)
	QG_CHAN_CUR("battery_id", PSY_IIO_BATTERY_ID)
	QG_CHAN_CUR("rsoc", PSY_IIO_CC_SOC)
	QG_CHAN_VOLT("shutdown_delay", PSY_IIO_SHUTDOWN_DELAY)
	QG_CHAN_VOLT("fastcharge_mode", PSY_IIO_FASTCHARGE_MODE)
};

enum qg_ext_iio_channels {
	INPUT_CURRENT_LIMITED = 0,
	RECHARGE_SOC,
	FORCE_RECHARGE,
	CHARGE_DONE,
	PARALLEL_CHARGING_ENABLED,
	CP_CHARGING_ENABLED,
};

static const char * const qg_ext_iio_chan_name[] = {
	[INPUT_CURRENT_LIMITED]	= "input_current_limited",
	[RECHARGE_SOC]			= "recharge_soc",
	[FORCE_RECHARGE]		= "force_recharge",
	[CHARGE_DONE]			= "charge_done",
	[PARALLEL_CHARGING_ENABLED]	= "parallel_charging_enabled",
	[CP_CHARGING_ENABLED]		= "cp_charging_enabled",
};

#endif
