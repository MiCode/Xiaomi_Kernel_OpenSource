/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __BQ2589X_IIO_H
#define __BQ2589X_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/qti_power_supply.h>

struct bq2589x_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define BQ2589X_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define BQ2589X_CHAN_ENERGY(_name, _num)			\
	BQ2589X_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct bq2589x_iio_channels bq2589x_iio_psy_channels[] = {
	BQ2589X_CHAN_ENERGY("charge_type", PSY_IIO_CHARGE_TYPE)
	BQ2589X_CHAN_ENERGY("charge_enabled", PSY_IIO_CHARGE_ENABLED)
	BQ2589X_CHAN_ENERGY("charge_done", PSY_IIO_CHARGE_DONE)
	BQ2589X_CHAN_ENERGY("charge_ic_type", PSY_IIO_CHARGE_IC_TYPE)
	BQ2589X_CHAN_ENERGY("charge_pd_active", PSY_IIO_PD_ACTIVE)
};

enum fg_ext_iio_channels {
	FG_FASTCHARGE_MODE,
	FG_MONITOR_WORK,
};

static const char * const fg_ext_iio_chan_name[] = {
	[FG_FASTCHARGE_MODE] = "fastcharge_mode",
	[FG_MONITOR_WORK] = "fg_monitor_work",
};

enum nopmi_chg_ext_iio_channels {
	NOPMI_CHG_MTBF_CUR,
	NOPMI_CHG_USB_REAL_TYPE,
	NOPMI_CHG_PD_ACTIVE,
	NOPMI_CHG_TYPEC_CC_ORIENTATION,
	NOPMI_CHG_TYPEC_MODE,
	NOPMI_CHG_FFC_DISABLE,
};

static const char * const nopmi_chg_ext_iio_chan_name[] = {
	[NOPMI_CHG_MTBF_CUR] = "mtbf_cur",
	[NOPMI_CHG_USB_REAL_TYPE] = "usb_real_type",
	[NOPMI_CHG_PD_ACTIVE] = "pd_active",
	[NOPMI_CHG_TYPEC_CC_ORIENTATION] = "typec_cc_orientation",
	[NOPMI_CHG_TYPEC_MODE] = "typec_mode",
	[NOPMI_CHG_FFC_DISABLE] = "ffc_disable",
};

enum ds_ext_iio_channels {
	DS_CHIP_OK,
};

static const char * const ds_ext_iio_chan_name[] = {
	[DS_CHIP_OK] = "ds_chip_ok",
};


enum main_iio_channels {
	MAIN_CHARGING_ENABLED,
	MAIN_CHARGER_TYPE,
};

static const char * const main_iio_chan_name[] = {
	[MAIN_CHARGING_ENABLED] = "charge_enabled",
	[MAIN_CHARGER_TYPE] = "charge_type",
};

#endif
