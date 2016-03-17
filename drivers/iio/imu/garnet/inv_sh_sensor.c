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
#include <linux/device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/iio/iio.h>

#include "inv_sh_data.h"
#include "inv_sh_command.h"
#include "inv_sh_iio.h"
#include "inv_sh_timesync.h"
#include "inv_mpu_iio.h"
#include "inv_sh_sensor_activity.h"
#include "inv_sh_sensor.h"

struct inv_sh_sensor_list {
	struct list_head list;
	struct iio_dev *indio_dev;
	int sensor_id;
	inv_sh_sensor_data_callback data_callback;
	inv_sh_sensor_remove_callback remove_callback;
};

static int timesync_poll(struct inv_mpu_state *st)
{
	struct inv_sh_command cmd;
	int ret;

	ret = inv_sh_command_get_data(&cmd, INV_SH_DATA_SENSOR_ID_METADATA);
	if (ret)
		return ret;

	ret = inv_sh_command_send(st, &cmd);
	if (ret)
		return ret;

	return 0;
}

int inv_sh_sensor_probe(struct inv_mpu_state *st)
{
	int ret;

	/* init time synchronization */
	inv_sh_timesync_init(&st->timesync, st->dev);
	ret = timesync_poll(st);
	if (ret) {
		dev_err(st->dev, "error polling for time synchro event\n");
		return ret;
	}

	/* TODO: use ping command to do dynamic scan */

	ret = inv_sh_activity_probe(st);
	if (ret) {
		dev_err(st->dev, "error probing activity sensor\n");
		return ret;
	}

	return 0;
}

void inv_sh_sensor_remove(struct inv_mpu_state *st)
{
	struct inv_sh_sensor_list *item;
	struct inv_sh_sensor_list *tmp;
	int ret;

	list_for_each_entry_safe(item, tmp, &st->sensors_list, list) {
		ret = item->remove_callback(item->indio_dev);
		if (ret)
			dev_err(st->dev, "error removing sensor %d\n",
				item->sensor_id);
		list_del(&item->list);
		kfree(item);
	}
}

ktime_t inv_sh_sensor_timestamp(struct inv_mpu_state *st,
				const struct inv_sh_data *sensor_data,
				ktime_t timestamp)
{
	struct inv_sh_timesync *timesync = &st->timesync;
	ktime_t time = ktime_set(0, 0);
	bool reset;

	/* sensor id 0 used for synchronizing time */
	if ((sensor_data->id & ~INV_SH_DATA_SENSOR_ID_FLAG_MASK) == 0) {
		switch (sensor_data->status) {
		case INV_SH_DATA_STATUS_STATE_CHANGED:
			if (sensor_data->accuracy != 0)
				reset = true;
			else
				reset = false;
			inv_sh_timesync_synchronize(timesync,
					sensor_data->timestamp, timestamp,
					reset);
			break;
		default:
			break;
		}
	} else {
		switch (sensor_data->status) {
		case INV_SH_DATA_STATUS_DATA_UPDATED:
		case INV_SH_DATA_STATUS_POLL:
		case INV_SH_DATA_STATUS_STATE_CHANGED:
			time = inv_sh_timesync_get_timestamp(timesync,
					sensor_data->timestamp, timestamp);
			break;
		case INV_SH_DATA_STATUS_FLUSH:
		default:
			break;
		}
	}

	return time;
}

int inv_sh_sensor_dispatch_data(struct inv_mpu_state *st,
				const struct inv_sh_data *sensor_data,
				ktime_t timestamp)
{
	struct inv_sh_sensor_list *item;
	int ret;

	/* nothing to do for sensor id 0 */
	if ((sensor_data->id & ~INV_SH_DATA_SENSOR_ID_FLAG_MASK) == 0)
		return 0;

	switch (sensor_data->status) {
	case INV_SH_DATA_STATUS_DATA_UPDATED:
	case INV_SH_DATA_STATUS_FLUSH:
	case INV_SH_DATA_STATUS_POLL:
	case INV_SH_DATA_STATUS_STATE_CHANGED:
		break;
	default:
		return -EINVAL;
	}

	/* dispatch data to corresponding registered sensor */
	ret = -ENODEV;
	list_for_each_entry(item, &st->sensors_list, list)
		if (sensor_data->id == item->sensor_id) {
			item->data_callback(item->indio_dev, sensor_data,
						timestamp);
			ret = 0;
			break;
		}

	return ret;
}

int inv_sh_sensor_register(struct iio_dev *indio_dev, int sensor_id,
				inv_sh_sensor_data_callback data_callback,
				inv_sh_sensor_remove_callback remove_callback)
{
	struct inv_sh_sensor_list *item;
	struct inv_sh_iio_state *iio_st = iio_priv(indio_dev);
	struct inv_mpu_state *st = iio_st->st;
	int ret;

	list_for_each_entry(item, &st->sensors_list, list)
		if (item->sensor_id == sensor_id) {
			ret = -EINVAL;
			goto error;
		}
	item = kzalloc(sizeof(*item), GFP_KERNEL);
	if (!item) {
		ret = -ENOMEM;
		goto error;
	}
	item->indio_dev = indio_dev;
	item->sensor_id = sensor_id;
	item->data_callback = data_callback;
	item->remove_callback = remove_callback;
	list_add_tail(&item->list, &st->sensors_list);

	return 0;
error:
	return ret;
}
EXPORT_SYMBOL(inv_sh_sensor_register);
