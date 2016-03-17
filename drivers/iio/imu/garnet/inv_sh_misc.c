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
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include <linux/math64.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>

#include "inv_sh_data.h"
#include "inv_mpu_iio.h"
#include "inv_sh_misc.h"

static void misc_push_data(struct inv_mpu_state *st, const uint8_t *data,
				size_t size)
{
	struct iio_dev *indio_dev = iio_priv_to_dev(st);
	static bool error;
	int i, ret;

	if (!atomic_read(&st->data_enable))
		return;

	/* push data in iio device */
	for (i = 0; i < size; ++i) {
		ret = iio_push_to_buffers(indio_dev, (uint8_t *)&data[i]);
		if (ret < 0 && !error) {
			dev_err(&indio_dev->dev,
					"buffer error %d, losing bytes\n", ret);
			error = true;
		} else if (ret >= 0 && error) {
			dev_notice(&indio_dev->dev, "stop losing bytes\n");
			error = false;
		}
	}
}

void inv_sh_misc_send_raw_data(struct inv_mpu_state *st,
				const void *data, size_t size)
{
	misc_push_data(st, data, size);
}

void inv_sh_misc_send_sensor_data(struct inv_mpu_state *st,
				const struct inv_sh_data *sensor_data,
				ktime_t timestamp)
{
	struct inv_sh_timesync *timesync = &st->timesync;
	uint32_t ts;
	uint8_t *data;

	/* replace timestamp in data frame with corrected one */
	data = (uint8_t *)sensor_data->raw;
	ts = div_u64(ktime_to_ns(timestamp), (uint32_t)timesync->resolution);
	memcpy(&data[2], &ts, sizeof(ts));

	misc_push_data(st, sensor_data->raw, sensor_data->size);
}
