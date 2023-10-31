/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, XiaoMi, Inc. All rights reserved.
 */
#ifndef __NOPMI_CHG_IIO_H
#define __NOPMI_CHG_IIO_H

#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct nopmi_chg_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define LN8000_IIO_CHANNEL_OFFSET 4

#define NOPMI_CHG_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define NOPMI_CHG_CHAN_ENERGY(_name, _num)			\
	NOPMI_CHG_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct nopmi_chg_iio_channels  nopmi_chg_iio_psy_channels[] = {
	NOPMI_CHG_CHAN_ENERGY("battery_charging_enabled", PSY_IIO_CHARGING_ENABLED)
	NOPMI_CHG_CHAN_ENERGY("usb_real_type", PSY_IIO_USB_REAL_TYPE)
	NOPMI_CHG_CHAN_ENERGY("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	NOPMI_CHG_CHAN_ENERGY("typec_mode", PSY_IIO_TYPEC_MODE)
	NOPMI_CHG_CHAN_ENERGY("input_suspend", PSY_IIO_INPUT_SUSPEND)
	NOPMI_CHG_CHAN_ENERGY("mtbf_cur", PSY_IIO_MTBF_CUR)
	NOPMI_CHG_CHAN_ENERGY("nopmi_charge_ic", PSY_IIO_CHARGE_IC_TYPE)
	NOPMI_CHG_CHAN_ENERGY("ffc_disable", PSY_IIO_FFC_DISABLE)
};

enum fg_ext_iio_channels {
	FG_RESISTANCE_ID,
	FG_FASTCHARGE_MODE,
	FG_SHUTDOWN_DELAY,
	FG_SOC_DECIMAL,
	FG_SOC_RATE_DECIMAL,
	FG_RAW_SOC,
	FG_BATT_ID,
};

static const char * const fg_ext_iio_chan_name[] = {
	[FG_RESISTANCE_ID]	= "resistance_id",
	[FG_FASTCHARGE_MODE]	= "fastcharge_mode",
	[FG_SHUTDOWN_DELAY]	= "shutdown_delay",
	[FG_SOC_DECIMAL]	= "soc_decimal",
	[FG_SOC_RATE_DECIMAL]	= "soc_decimal_rate",
	[FG_RAW_SOC]		= "raw_soc",
	[FG_BATT_ID]		= "fg_batt_id",
};

enum cc_ext_iio_channels {
	CC_CHIP_ID,
	CC_TYPEC_CC_ORIENTATION,
	CC_TYPEC_MODE,
};

static const char * const cc_ext_iio_chan_name[] = {
	[CC_CHIP_ID] = "cc_chip_id",
	[CC_TYPEC_CC_ORIENTATION] = "typec_cc_orientation",
	[CC_TYPEC_MODE] = "typec_mode",
};

enum cp_ext_iio_channels {
	CHARGE_PUMP_CHARGING_ENABLED,
	CHARGE_PUMP_LN_CHARGING_ENABLED = LN8000_IIO_CHANNEL_OFFSET,
	CHARGE_PUMP_LN_CHIP_ID,
	CHARGE_PUMP_LN_BUS_CURRENT,
};

static const char * const cp_ext_iio_chan_name[] = {
	[CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled",
	//[CHARGE_PUMP_CHIP_ID] = "sc_chip_id",
	//[CHARGE_PUMP_BUS_CURRENT] = "sc_bus_current", //sc8551 ibus
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled",
	[CHARGE_PUMP_LN_CHIP_ID] = "ln_chip_id",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current", //ln8000 ibus
};

enum main_chg_ext_iio_channels {
	MAIN_CHARGE_TYPE,
	MAIN_CHARGE_ENABLED,
	MAIN_CHARGE_IC_TYPE,
	MAIN_CHARGE_VBUS_VOL,
	MAIN_CHARGE_SET_SHIP_MODE,
};

static const char * const main_chg_ext_iio_chan_name[] = {
	[MAIN_CHARGE_TYPE] = "charge_type",
	[MAIN_CHARGE_ENABLED] = "charge_enabled",
	[MAIN_CHARGE_IC_TYPE] = "charge_ic_type",
	[MAIN_CHARGE_VBUS_VOL] = "charge_vbus_vol",
	[MAIN_CHARGE_SET_SHIP_MODE] = "set_ship_mode",
};

#endif
