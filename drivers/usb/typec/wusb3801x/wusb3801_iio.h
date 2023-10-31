/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __WUSB3801_IIO_H
#define __WUSB3801_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/qti_power_supply.h>

struct wusb3801_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define WUSB3801_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define WUSB3801_CHAN_CURRENT(_name, _num)			\
	WUSB3801_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct wusb3801_iio_channels wusb3801_iio_psy_channels[] = {
	WUSB3801_CHAN_CURRENT("cc_chip_id", PSY_IIO_CC_CHIP_ID)
	WUSB3801_CHAN_CURRENT("typec_cc_orientation", PSY_IIO_TYPEC_CC_ORIENTATION)
	WUSB3801_CHAN_CURRENT("typec_mode", PSY_IIO_TYPEC_MODE)
};

#endif
