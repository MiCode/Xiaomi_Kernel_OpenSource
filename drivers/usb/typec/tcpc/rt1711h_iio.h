/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */
#ifndef __RT1711H_IIO_H
#define __RT1711H_IIO_H
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct rt1711h_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define RT1711H_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define RT1711H_CHAN_ENERGY(_name, _num)			\
	RT1711H_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct rt1711h_iio_channels rt1711h_iio_psy_channels[] = {
	RT1711H_CHAN_ENERGY("cc_chip_id", PSY_IIO_DEV_CHIP_ID)
};
#endif