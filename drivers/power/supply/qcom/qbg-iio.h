/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef __QBG_IIO_H__
#define __QBG_IIO_H__

struct qbg_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define QBG_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define QBG_CHAN_ENERGY(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QBG_CHAN_TEMP(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_TEMP,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QBG_CHAN_VOLT(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_VOLTAGE,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QBG_CHAN_TSTAMP(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_TIMESTAMP,	\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QBG_CHAN_RES(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_RESISTANCE,	\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QBG_CHAN_INDEX(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_INDEX,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QBG_CHAN_CUR(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_CURRENT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#define QBG_CHAN_COUNT(_name, _num)			\
	QBG_IIO_CHAN(_name, _num, IIO_COUNT,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

#endif /* __QBG_IIO_H__ */
