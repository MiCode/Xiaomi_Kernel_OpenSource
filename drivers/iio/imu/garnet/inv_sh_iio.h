/*
* Copyright (C) 2015 InvenSense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#ifndef _INV_SH_IIO_H
#define _INV_SH_IIO_H

#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>

#include "inv_mpu_iio.h"

struct inv_sh_iio_chan_info_value {
	int val;
	int val2;
};

struct inv_sh_iio_chan_info {
	struct inv_sh_iio_chan_info_value offset;
	struct inv_sh_iio_chan_info_value scale;
};

struct inv_sh_iio_poll {
	atomic_t pending;
	struct completion wait;
	void *data;
	int (*read_raw)(struct inv_sh_iio_poll *iio_poll,
			struct iio_chan_spec const *chan, int *val, int *val2);
};

struct inv_sh_iio_state {
	struct inv_mpu_state *st;
	int sensor_id;
	const struct inv_sh_iio_chan_info *channels_infos;
	atomic_t enable;
	atomic_t ready;
	struct mutex poll_lock;
	struct inv_sh_iio_poll poll;
};

void inv_sh_iio_init(struct inv_sh_iio_state *iio_st);

void inv_sh_iio_destroy(struct inv_sh_iio_state *iio_st);

/* iio poll push data */
void inv_sh_iio_poll_push_data(struct inv_sh_iio_state *iio_st,
				const void *data, size_t size);

/* iio channel read and write raw handlers */
int inv_sh_iio_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask);
int inv_sh_iio_write_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask);

/* iio channel ext info alloc */
int inv_sh_iio_alloc_channel_ext_info(struct iio_chan_spec *chan,
					struct inv_sh_iio_state *iio_st);
void inv_sh_iio_free_channel_ext_info(const struct iio_chan_spec *chan);

/* iio trigger setup */
int inv_sh_iio_trigger_setup(struct iio_dev *indio_dev);
void inv_sh_iio_trigger_remove(struct iio_dev *indio_dev);

#endif
