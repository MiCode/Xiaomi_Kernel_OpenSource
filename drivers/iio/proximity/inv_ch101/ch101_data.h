/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 InvenSense, Inc.
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

#ifndef DRIVERS_IIO_PROXIMITY_CH101_DATA_H_
#define DRIVERS_IIO_PROXIMITY_CH101_DATA_H_

#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/completion.h>

#include <asm-generic/int-ll64.h>

#include "ch101_client.h"

struct ch101_data {
	struct device *dev;
	struct ch101_client client;
	struct ch101_buffer buffer;
	struct regmap *regmap;
	struct mutex lock;
	struct completion ss_completion;
	struct completion data_completion;
	struct iio_trigger *trig;
	struct hrtimer timer;
	int irq[CHBSP_MAX_DEVICES];
	int irq_set[CHBSP_MAX_DEVICES];
	int irq_map[CHBSP_MAX_DEVICES];
	int scan_rate;
	ktime_t period;
	int counter;
	int fw_initialized;
};

#endif /* DRIVERS_IIO_PROXIMITY_CH101_DATA_H_ */
