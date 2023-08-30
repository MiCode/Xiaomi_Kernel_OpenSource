
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __BQ27Z561_IIO_H
#define __BQ27Z561_IIO_H

#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct bq27z561_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define FG_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define FG_CHAN_VOLT(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_VOLTAGE,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_CUR(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_RES(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_RESISTANCE,	\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_TEMP(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_TEMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_POW(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_POWER,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_ENERGY(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_INDEX(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_INDEX,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_ACT(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_ACTIVITY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_TSTAMP(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_TIMESTAMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define FG_CHAN_COUNT(_name, _num)			\
	FG_IIO_CHAN(_name, _num, IIO_COUNT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct bq27z561_iio_channels bq27z561_iio_psy_channels[] = {
	FG_CHAN_ENERGY("bqfg_present", PSY_IIO_BQFG_PRESENT)
	FG_CHAN_ENERGY("bqfg_status", PSY_IIO_BQFG_STATUS)
	FG_CHAN_VOLT("bqfg_voltage_now", PSY_IIO_BQFG_VOLTAGE_NOW)
	FG_CHAN_VOLT("bqfg_voltage_max", PSY_IIO_BQFG_VOLTAGE_MAX)
	FG_CHAN_CUR("bqfg_current_now", PSY_IIO_BQFG_CURRENT_NOW)
	FG_CHAN_ENERGY("bqfg_capacity", PSY_IIO_BQFG_CAPACITY)
	FG_CHAN_ENERGY("bqfg_capacity_level", PSY_IIO_BQFG_CAPACITY_LEVEL)
	FG_CHAN_TEMP("bqfg_temp", PSY_IIO_BQFG_TEMP)
	FG_CHAN_ENERGY("bqfg_charge_full", PSY_IIO_BQFG_CHARGE_FULL)
	FG_CHAN_ENERGY("bqfg_charge_full_design", PSY_IIO_BQFG_CHARGE_FULL_DESIGN)
	FG_CHAN_COUNT("bqfg_cycle_count", PSY_IIO_BQFG_CYCLE_COUNT)
	FG_CHAN_ENERGY("bqfg_time_to_empty_now", PSY_IIO_BQFG_TIME_TO_EMPTY_NOW)
	FG_CHAN_TSTAMP("bqfg_time_to_full_now", PSY_IIO_BQFG_TIME_TO_FULL_NOW)
	FG_CHAN_ENERGY("bqfg_update_now", PSY_IIO_BQFG_UPDATE_NOW)
	FG_CHAN_CUR("bqfg_therm_curr", PSY_IIO_BQFG_THERM_CURR)
	FG_CHAN_CUR("bqfg_chip_ok", PSY_IIO_BQFG_CHIP_OK)
	FG_CHAN_CUR("bqfg_battery_auth", PSY_IIO_BQFG_BATTERY_AUTH)
	FG_CHAN_CUR("bqfg_soc_decimal", PSY_IIO_BQFG_SOC_DECIMAL)
	FG_CHAN_CUR("bqfg_soc_decimal_rate", PSY_IIO_BQFG_SOC_DECIMAL_RATE)
	FG_CHAN_CUR("bqfg_soh", PSY_IIO_BQFG_SOH)
	FG_CHAN_CUR("bqfg_rsoc", PSY_IIO_BQFG_RSOC)
	FG_CHAN_CUR("bqfg_battery_id", PSY_IIO_BQFG_BATTERY_ID)
	FG_CHAN_RES("bqfg_resistance_id", PSY_IIO_BQFG_RESISTANCE_ID)
	FG_CHAN_VOLT("bqfg_shutdown_delay", PSY_IIO_BQFG_SHUTDOWN_DELAY)
	FG_CHAN_VOLT("bqfg_fastcharge_mode", PSY_IIO_BQFG_FASTCHARGE_MODE)
	FG_CHAN_TEMP("bqfg_temp_max", PSY_IIO_BQFG_TEMP_MAX)
	FG_CHAN_TSTAMP("bqfg_time_ot", PSY_IIO_BQFG_TIME_OT)
	FG_CHAN_CUR("bqfg_reg_rsoc", PSY_IIO_BQFG_REG_RSOC)
	FG_CHAN_CUR("bqfg_rm", PSY_IIO_BQFG_RM)
};

int bq27z561_init_iio_psy(struct bq_fg_chip *chip);

#endif /* __BQ27Z561_IIO_H */

