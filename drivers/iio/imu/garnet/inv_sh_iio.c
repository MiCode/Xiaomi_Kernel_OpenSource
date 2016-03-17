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
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>

#include "inv_sh_command.h"
#include "inv_sh_iio.h"

static inline void init_poll(struct inv_sh_iio_poll *iio_poll)
{
	atomic_set(&iio_poll->pending, 0);
	init_completion(&iio_poll->wait);
	iio_poll->data = NULL;
	iio_poll->read_raw = NULL;
}

void inv_sh_iio_init(struct inv_sh_iio_state *iio_st)
{
	atomic_set(&iio_st->enable, 0);
	atomic_set(&iio_st->ready, 0);
	mutex_init(&iio_st->poll_lock);
	init_poll(&iio_st->poll);
}
EXPORT_SYMBOL(inv_sh_iio_init);

void inv_sh_iio_destroy(struct inv_sh_iio_state *iio_st)
{
	mutex_destroy(&iio_st->poll_lock);
}
EXPORT_SYMBOL(inv_sh_iio_destroy);

void inv_sh_iio_poll_push_data(struct inv_sh_iio_state *iio_st,
				const void *data, size_t size)
{
	if (atomic_read(&iio_st->poll.pending)) {
		memcpy(iio_st->poll.data, data, size);
		complete(&iio_st->poll.wait);
	}
}
EXPORT_SYMBOL(inv_sh_iio_poll_push_data);

int inv_sh_iio_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *chan,
			int *val, int *val2, long mask)
{
	struct inv_sh_iio_state *iio_st = iio_priv(indio_dev);
	struct inv_sh_command cmd;
	const struct inv_sh_iio_chan_info_value *info;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (!iio_st->poll.read_raw)
			return -EINVAL;
		ret = inv_sh_command_get_data(&cmd, iio_st->sensor_id);
		if (ret)
			return ret;
		mutex_lock(&iio_st->poll_lock);
		atomic_set(&iio_st->poll.pending, 1);
		ret = inv_sh_command_send(iio_st->st, &cmd);
		if (ret)
			goto error_raw_pending;
		ret = wait_for_completion_interruptible_timeout(&iio_st->poll.wait, HZ);
		if (ret <= 0) {
			if (ret == 0)
				ret = -ETIMEDOUT;
			goto error_raw_pending;
		}
		ret = iio_st->poll.read_raw(&iio_st->poll, chan, val, val2);
		atomic_set(&iio_st->poll.pending, 0);
		mutex_unlock(&iio_st->poll_lock);
		break;
	case IIO_CHAN_INFO_OFFSET:
		info = &iio_st->channels_infos[chan->scan_index].offset;
		*val = info->val;
		*val2 = info->val2;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	case IIO_CHAN_INFO_SCALE:
		info = &iio_st->channels_infos[chan->scan_index].scale;
		*val = info->val;
		*val2 = info->val2;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
error_raw_pending:
	atomic_set(&iio_st->poll.pending, 0);
	mutex_unlock(&iio_st->poll_lock);
	return ret;
}
EXPORT_SYMBOL(inv_sh_iio_read_raw);

int inv_sh_iio_write_raw(struct iio_dev *indio_dev,
				struct iio_chan_spec const *chan,
				int val, int val2, long mask)
{
	struct inv_sh_iio_state *iio_st = iio_priv(indio_dev);
	struct inv_sh_command cmd;
	int32_t value;
	int ret;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		value = 1000000000 / (val * 1000000 + val2);
		ret = inv_sh_command_set_delay(&cmd, iio_st->sensor_id, value);
		if (ret)
			return ret;
		ret = inv_sh_command_send(iio_st->st, &cmd);
		if (ret)
			return ret;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
EXPORT_SYMBOL(inv_sh_iio_write_raw);

static ssize_t inv_sh_iio_ext_read_fifo_length(struct iio_dev *iio,
			uintptr_t private, struct iio_chan_spec const *chan,
			char *buf)
{
	struct inv_sh_iio_state *iio_st = (struct inv_sh_iio_state *)private;

	return snprintf(buf, PAGE_SIZE, "%zu\n", iio_st->st->fifo_length);
}

static ssize_t inv_sh_iio_ext_write_batch_latency(struct iio_dev *iio,
			uintptr_t private, struct iio_chan_spec const *chan,
			const char *buf, size_t len)
{
	struct inv_sh_iio_state *iio_st = (struct inv_sh_iio_state *)private;
	struct inv_sh_command cmd;
	int timeout_us = -1;
	int ret;

	ret = sscanf(buf, "%d", &timeout_us);
	if (ret != 1 || timeout_us < 0)
		return -EINVAL;

	ret = inv_sh_command_batch(&cmd, iio_st->sensor_id, timeout_us / 1000);
	if (ret)
		return ret;

	return inv_sh_command_send(iio_st->st, &cmd);
}

static ssize_t inv_sh_iio_ext_write_flush(struct iio_dev *iio,
			uintptr_t private, struct iio_chan_spec const *chan,
			const char *buf, size_t len)
{
	struct inv_sh_iio_state *iio_st = (struct inv_sh_iio_state *)private;
	struct inv_sh_command cmd;
	int val = -1;
	int ret;

	ret = sscanf(buf, "%d", &val);
	if (ret != 1 || val != 1)
		return -EINVAL;

	ret = inv_sh_command_flush(&cmd, iio_st->sensor_id);
	if (ret)
		return ret;

	return inv_sh_command_send(iio_st->st, &cmd);
}

static const struct iio_chan_spec_ext_info inv_sh_iio_channel_ext_info[] = {
	{
		.name = "fifo_length",
		.shared = true,
		.read = inv_sh_iio_ext_read_fifo_length,
	}, {
		.name = "batch_latency",
		.shared = true,
		.write = inv_sh_iio_ext_write_batch_latency,
	}, {
		.name = "flush",
		.shared = true,
		.write = inv_sh_iio_ext_write_flush,
	},
	{ }
};

int inv_sh_iio_alloc_channel_ext_info(struct iio_chan_spec *chan,
					struct inv_sh_iio_state *iio_st)
{
	struct iio_chan_spec_ext_info *ext_info;
	int i;

	ext_info = kmemdup(inv_sh_iio_channel_ext_info,
			sizeof(inv_sh_iio_channel_ext_info), GFP_KERNEL);
	if (!ext_info)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(inv_sh_iio_channel_ext_info); ++i)
		ext_info[i].private = (uintptr_t)iio_st;

	chan->ext_info = ext_info;

	return 0;
}
EXPORT_SYMBOL(inv_sh_iio_alloc_channel_ext_info);

void inv_sh_iio_free_channel_ext_info(const struct iio_chan_spec *chan)
{
	kfree(chan->ext_info);
}
EXPORT_SYMBOL(inv_sh_iio_free_channel_ext_info);

static int inv_sh_iio_set_trigger_state(struct iio_trigger *trig, bool state)
{
	struct inv_sh_iio_state *iio_st = iio_trigger_get_drvdata(trig);
	struct inv_sh_command cmd;
	int ret;

	ret = inv_sh_command_activate(&cmd, iio_st->sensor_id, state);
	if (ret)
		return ret;

	ret = inv_sh_command_send(iio_st->st, &cmd);
	if (ret)
		return ret;

	if (!state)
		atomic_set(&iio_st->ready, 0);
	atomic_set(&iio_st->enable, state);

	return 0;
}

static const struct iio_trigger_ops inv_sh_iio_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = inv_sh_iio_set_trigger_state,
};

int inv_sh_iio_trigger_setup(struct iio_dev *indio_dev)
{
	struct inv_sh_iio_state *iio_st = iio_priv(indio_dev);
	struct iio_trigger *trig;
	int ret;

	trig = iio_trigger_alloc("%s-dev%d", indio_dev->name, indio_dev->id);
	if (trig == NULL) {
		dev_err(&indio_dev->dev, "trigger allocate failed\n");
		ret = -ENOMEM;
		goto error;
	}

	trig->dev.parent = indio_dev->dev.parent;
	iio_trigger_set_drvdata(trig, iio_st);
	trig->ops = &inv_sh_iio_trigger_ops;

	ret = iio_trigger_register(trig);
	if (ret) {
		dev_err(&indio_dev->dev, "trigger register failed\n");
		goto error_free_trig;
	}
	indio_dev->trig = trig;

	return 0;
error_free_trig:
	iio_trigger_free(trig);
error:
	return ret;
}
EXPORT_SYMBOL(inv_sh_iio_trigger_setup);

void inv_sh_iio_trigger_remove(struct iio_dev *indio_dev)
{
	iio_trigger_unregister(indio_dev->trig);
	iio_trigger_free(indio_dev->trig);
	indio_dev->trig = NULL;
}
EXPORT_SYMBOL(inv_sh_iio_trigger_remove);
