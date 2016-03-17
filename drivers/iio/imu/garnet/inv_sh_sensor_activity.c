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
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/atomic.h>
#include <linux/ktime.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "inv_sh_iio.h"
#include "inv_sh_data.h"
#include "inv_sh_command.h"
#include "inv_sh_sensor.h"
#include "inv_mpu_iio.h"

#define ACTIVITY_SENSOR_ID	(INV_SH_DATA_SENSOR_ID_ACTIVITY_RECOGNITION | \
					INV_SH_DATA_SENSOR_ID_FLAG_WAKE_UP)

enum activity_channel {
	CHANNEL_ACTIVITY,
	CHANNEL_EVENT,
	CHANNEL_TIMESTAMP,
};

struct activity_data {
	uint32_t activity;
	uint32_t event;
	int64_t timestamp;
};

/* Must be a power of 2! */
typedef uint8_t activity_data_buffer_t[16];

static inline void activity_data_serialize(activity_data_buffer_t buffer,
					const struct activity_data *data)
{
	size_t idx = 0;

	memcpy(&buffer[idx], &data->activity, sizeof(data->activity));
	idx += sizeof(data->activity);
	memcpy(&buffer[idx], &data->event, sizeof(data->event));
	idx += sizeof(data->event);
	memcpy(&buffer[idx], &data->timestamp, sizeof(data->timestamp));
}

/* Channel definitions */
static struct iio_chan_spec activity_channels[] = {
	{
		.type = IIO_ACCEL,
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_RAW) |
				BIT(IIO_CHAN_INFO_OFFSET) |
				BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = CHANNEL_ACTIVITY,
		.scan_type = IIO_ST('u', 7, 32, 0),
	}, {
		.type = IIO_INTENSITY,
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_RAW) |
				BIT(IIO_CHAN_INFO_OFFSET) |
				BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = CHANNEL_EVENT,
		.scan_type = IIO_ST('u', 2, 32, 0),
	}, {
		.type = IIO_TIMESTAMP,
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OFFSET) |
				BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = CHANNEL_TIMESTAMP,
		.scan_type = IIO_ST('s', 64, 64, 0),
	},
};

static const struct inv_sh_iio_chan_info activity_channels_infos[] = {
	[CHANNEL_ACTIVITY] = {
		.offset = { 0, 0 },
		.scale = { 1, 0 },
	},
	[CHANNEL_EVENT] = {
		.offset = { 0, 0 },
		.scale = { 1, 0 },
	},
	[CHANNEL_TIMESTAMP] = {
		.offset = { 0, 0 },
		.scale = { 1, 0 },
	},
};

static const struct iio_info activity_info = {
	.driver_module = THIS_MODULE,
	.read_raw = inv_sh_iio_read_raw,
	.write_raw = inv_sh_iio_write_raw,
};

static int activity_read_raw(struct inv_sh_iio_poll *iio_poll,
				struct iio_chan_spec const *chan,
				int *val, int *val2)
{
	const struct activity_data *data = iio_poll->data;

	switch (chan->scan_index) {
	case CHANNEL_ACTIVITY:
		*val = data->activity;
		break;
	case CHANNEL_EVENT:
		*val = data->event;
		break;
	default:
		return -EINVAL;
	}
	*val2 = 0;

	return IIO_VAL_INT;
}

static void activity_push_data(struct iio_dev *indio_dev,
				const struct inv_sh_data *sensor_data,
				ktime_t timestamp)
{
	struct inv_sh_iio_state *iio_st = iio_priv(indio_dev);
	struct activity_data data;
	activity_data_buffer_t buffer;
	int ret;

	/* build activity data sample */
	if (sensor_data->status == INV_SH_DATA_STATUS_FLUSH) {
		data.activity = 0;
		data.event = 0;
	} else {
		data.activity = sensor_data->data.activity_recognition;
		data.event = sensor_data->data.activity_recognition;
		data.event = ((data.event & 0x80) >> 7) + 1;
	}
	data.timestamp = ktime_to_ns(timestamp);

	/* send poll data */
	if (sensor_data->status == INV_SH_DATA_STATUS_POLL) {
		inv_sh_iio_poll_push_data(iio_st, &data, sizeof(data));
		return;
	}

	/* filter out data sending if sensor is off */
	if (!atomic_read(&iio_st->enable))
		return;

	/* change sensor state to ready */
	if (sensor_data->status == INV_SH_DATA_STATUS_STATE_CHANGED) {
		atomic_set(&iio_st->ready, 1);
		return;
	}

	/* send data if sensor is ready */
	if (atomic_read(&iio_st->ready)) {
		activity_data_serialize(buffer, &data);
		ret = iio_push_to_buffers(indio_dev, (uint8_t *)buffer);
		if (ret)
			dev_err(&indio_dev->dev, "losing events\n");
	}
}

static int activity_remove(struct iio_dev *indio_dev)
{
	struct inv_sh_iio_state *iio_st = iio_priv(indio_dev);

	kfree(iio_st->poll.data);

	iio_device_unregister(indio_dev);
	inv_sh_iio_trigger_remove(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	inv_sh_iio_free_channel_ext_info(&indio_dev->channels[0]);
	inv_sh_iio_destroy(iio_st);
	iio_device_free(indio_dev);

	return 0;
}

int inv_sh_activity_probe(struct inv_mpu_state *st)
{
	static const char *name = "inv_sh-activity";
	struct iio_dev *indio_dev;
	struct inv_sh_iio_state *iio_st;
	int ret;

	indio_dev = iio_device_alloc(sizeof(*iio_st));
	if (indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	iio_st = iio_priv(indio_dev);
	inv_sh_iio_init(iio_st);
	iio_st->st = st;
	iio_st->sensor_id = ACTIVITY_SENSOR_ID;
	iio_st->channels_infos = activity_channels_infos;
	iio_st->poll.data = kzalloc(sizeof(struct activity_data), GFP_KERNEL);
	if (!iio_st->poll.data) {
		ret = -ENOMEM;
		goto error_free_dev;
	}
	iio_st->poll.read_raw = activity_read_raw;

	ret = inv_sh_iio_alloc_channel_ext_info(&activity_channels[0], iio_st);
	if (ret) {
		dev_err(&indio_dev->dev, "failed to allocate ext info\n");
		goto error_free_poll_data;
	}

	indio_dev->channels = activity_channels;
	indio_dev->num_channels = ARRAY_SIZE(activity_channels);
	indio_dev->dev.parent = st->dev;
	indio_dev->info = &activity_info;
	indio_dev->name = name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_triggered_buffer_setup(indio_dev, &iio_pollfunc_store_time,
						NULL, NULL);
	if (ret) {
		dev_err(&indio_dev->dev, "failed to initialize trigger buffer\n");
		goto error_free_ext_info;
	}

	ret = inv_sh_iio_trigger_setup(indio_dev);
	if (ret) {
		dev_err(&indio_dev->dev, "trigger setup failed\n");
		goto error_unreg_buffer_funcs;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&indio_dev->dev, "device register failed\n");
		goto error_remove_trigger;
	}

	ret = inv_sh_sensor_register(indio_dev, ACTIVITY_SENSOR_ID,
			activity_push_data, activity_remove);
	if (ret) {
		dev_err(&indio_dev->dev, "callback reg failed\n");
		goto error_iio_unreg;
	}

	return ret;

error_iio_unreg:
	iio_device_unregister(indio_dev);
error_remove_trigger:
	inv_sh_iio_trigger_remove(indio_dev);
error_unreg_buffer_funcs:
	iio_triggered_buffer_cleanup(indio_dev);
error_free_ext_info:
	inv_sh_iio_free_channel_ext_info(&activity_channels[0]);
error_free_poll_data:
	kfree(iio_st->poll.data);
error_free_dev:
	iio_device_free(indio_dev);
error_ret:
	return ret;
}
EXPORT_SYMBOL(inv_sh_activity_probe);
