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
#ifndef _INV_SH_SENSOR_COMMON_H
#define _INV_SH_SENSOR_COMMON_H

#include <linux/ktime.h>
#include <linux/iio/iio.h>

#include "inv_sh_data.h"
#include "inv_mpu_iio.h"

/* Functions to use in main driver */
int inv_sh_sensor_probe(struct inv_mpu_state *st);

void inv_sh_sensor_remove(struct inv_mpu_state *st);

ktime_t inv_sh_sensor_timestamp(struct inv_mpu_state *st,
				const struct inv_sh_data *sensor_data,
				ktime_t timestamp);

int inv_sh_sensor_dispatch_data(struct inv_mpu_state *st,
				const struct inv_sh_data *sensor_data,
				ktime_t timestamp);

/* Functions to use in sensor sub-drivers for registering */
typedef void (*inv_sh_sensor_data_callback)(struct iio_dev *indio_dev,
					const struct inv_sh_data *sensor_data,
					ktime_t timestamp);

typedef int (*inv_sh_sensor_remove_callback)(struct iio_dev *indio_dev);

int inv_sh_sensor_register(struct iio_dev *indio_dev, int sensor_id,
				inv_sh_sensor_data_callback data_callback,
				inv_sh_sensor_remove_callback remove_callback);

#endif
