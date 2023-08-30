
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __SYV690D_IIO_H
#define __SYV690D_IIO_H


#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <linux/qti_power_supply.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>


struct syv690d_iio_channels {
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

static const struct syv690d_iio_channels syv690d_iio_psy_channels[] = {
	BQ25890_CHAN_ENERGY("syv_charge_present", PSY_IIO_SYV_CHARGE_PRESENT)
	BQ25890_CHAN_ENERGY("syv_charge_online", PSY_IIO_SYV_CHARGE_ONLINE)
	BQ25890_CHAN_ENERGY("syv_charge_done", PSY_IIO_SYV_CHARGE_DONE)
	BQ25890_CHAN_ENERGY("syv_chager_hz", PSY_IIO_SYV_CHAGER_HZ)
	BQ25890_CHAN_ENERGY("syv_input_current_settled", PSY_IIO_SYV_INPUT_CURRENT_SETTLED)
	BQ25890_CHAN_ENERGY("syv_input_voltage_settled", PSY_IIO_SYV_INPUT_VOLTAGE_SETTLED)
	BQ25890_CHAN_ENERGY("syv_charge_current", PSY_IIO_SYV_CHAGER_CURRENT)
	BQ25890_CHAN_ENERGY("syv_charger_enable", PSY_IIO_SYV_CHARGING_ENABLED)
	BQ25890_CHAN_ENERGY("syv_otg_enable", PSY_IIO_SYV_OTG_ENABLE)
	BQ25890_CHAN_ENERGY("syv_charger_term", PSY_IIO_SYV_CHAGER_TERM)
	BQ25890_CHAN_ENERGY("syv_batt_voltage_term", PSY_IIO_SYV_BATTERY_VOLTAGE_TERM)
	BQ25890_CHAN_ENERGY("syv_charger_status", PSY_IIO_SYV_CHARGER_STATUS)
	BQ25890_CHAN_ENERGY("syv_charger_type", PSY_IIO_SYV_CHARGE_TYPE)
	BQ25890_CHAN_ENERGY("syv_charger_usb_type", PSY_IIO_SYV_CHARGE_USB_TYPE)
	BQ25890_CHAN_ENERGY("syv_vbus_voltage", PSY_IIO_SYV_BUS_VOLTAGE)
	BQ25890_CHAN_ENERGY("syv_vbat_voltage", PSY_IIO_SYV_BATTERY_VOLTAGE)
	BQ25890_CHAN_ENERGY("syv_enable_charger_term", PSY_IIO_SYV_ENABLE_CHAGER_TERM)
};

int bq_init_iio_psy(struct bq2589x *chip);

#endif /* __SYV690D_IIO_H */

