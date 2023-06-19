/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 */

#ifndef __DE28E16_IIO_H
#define __DE28E16_IIO_H

#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>

struct ds_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define DS_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define DS_CHAN_CURRENT(_name, _num)			\
	DS_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct ds_iio_channels ds_iio_psy_channels[] = {
	DS_CHAN_CURRENT("ds_romid", PSY_IIO_DS_ROMID)
	DS_CHAN_CURRENT("ds_status", PSY_IIO_DS_STATUS)
	DS_CHAN_CURRENT("ds_authen_result", PSY_IIO_DS_AUTHEN_RESULT)
	DS_CHAN_CURRENT("ds_page0_data", PSY_IIO_DS_PAGE0_DATA)
	DS_CHAN_CURRENT("ds_page1_data", PSY_IIO_DS_PAGE1_DATA)
	DS_CHAN_CURRENT("ds_chip_ok", PSY_IIO_DS_CHIP_OK)
	DS_CHAN_CURRENT("ds_cycle_count", PSY_IIO_DS_CYCLE_COUNT)
	DS_CHAN_CURRENT("ds_authentic", PSY_IIO_DS_AUTHENTIC)
	DS_CHAN_CURRENT("ds_chip_id", PSY_IIO_DEV_CHIP_ID)
};

#endif