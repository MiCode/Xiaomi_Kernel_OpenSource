/*
 * HID Sensor Orientation Driver
 * Copyright (c) 2013, Movea SA, Jean-Baptiste Maneyrol <jbmaneyrol@movea.com>
 * Copyright (c) 2012, Intel Corporation.
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * based on work from Srinivas Pandruvada <srinivas.pandruvada@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/hid-sensor-hub.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include "../common/hid-sensors/hid-sensor-trigger.h"

/*Format: HID-SENSOR-usage_id_in_hex*/
/*Usage ID from spec for Device Orientation: 0x20008a*/
#define DRIVER_NAME "HID-SENSOR-20008a"

#define ORIENT_ATTRIBUTES_NB		(HID_SENSOR_COMMON_CHANNEL_NB + 2)

enum orient_channel {
	CHANNEL_SCAN_INDEX_QUATERNION_X,
	CHANNEL_SCAN_INDEX_QUATERNION_Y,
	CHANNEL_SCAN_INDEX_QUATERNION_Z,
	CHANNEL_SCAN_INDEX_QUATERNION_W,
	CHANNEL_SCAN_INDEX_HEADING_ERROR,
	CHANNEL_SCAN_INDEX_COMMON,
	ORIENT_CHANNEL_NB = CHANNEL_SCAN_INDEX_COMMON +
			HID_SENSOR_COMMON_CHANNEL_NB,
};

struct orient_state {
	struct hid_sensor_common common;
	struct hid_sensor_hub_callbacks callbacks;
	struct hid_sensor_hub_attribute_info orient[ORIENT_ATTRIBUTES_NB];
	u32 orient_val[ORIENT_CHANNEL_NB];
};

static const u32 orient_addresses[ORIENT_ATTRIBUTES_NB] = {
	[0] = HID_USAGE_SENSOR_DATA_ORIENT_QUATERNION,
	[1] = HID_USAGE_SENSOR_DATA_ORIENT_MAGNETIC_HEADING,
	HID_SENSOR_COMMON_ADDRESSES(2),
};

/* Channel definitions */
static const struct iio_chan_spec orient_channels[] = {
	{
		.type = IIO_ROT,
		.modified = 1,
		.channel2 = IIO_MOD_X,
		.info_mask_separate = BIT(IIO_CHAN_INFO_OFFSET) |
			BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
			BIT(IIO_CHAN_INFO_HYSTERESIS),
		.scan_index = CHANNEL_SCAN_INDEX_QUATERNION_X,
	}, {
		.type = IIO_ROT,
		.modified = 1,
		.channel2 = IIO_MOD_Y,
		.info_mask_separate = BIT(IIO_CHAN_INFO_OFFSET) |
			BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
			BIT(IIO_CHAN_INFO_HYSTERESIS),
		.scan_index = CHANNEL_SCAN_INDEX_QUATERNION_Y,
	}, {
		.type = IIO_ROT,
		.modified = 1,
		.channel2 = IIO_MOD_Z,
		.info_mask_separate = BIT(IIO_CHAN_INFO_OFFSET) |
			BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
			BIT(IIO_CHAN_INFO_HYSTERESIS),
		.scan_index = CHANNEL_SCAN_INDEX_QUATERNION_Z,
	}, {
		.type = IIO_ROT,
		.modified = 1,
		.channel2 = IIO_MOD_W,
		.info_mask_separate = BIT(IIO_CHAN_INFO_OFFSET) |
			BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
			BIT(IIO_CHAN_INFO_HYSTERESIS),
		.scan_index = CHANNEL_SCAN_INDEX_QUATERNION_W,
	}, {
		.type = IIO_ANGL,
		.info_mask_separate = BIT(IIO_CHAN_INFO_OFFSET) |
			BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = CHANNEL_SCAN_INDEX_HEADING_ERROR,
	},
	HID_SENSOR_COMMON_CHANNELS(CHANNEL_SCAN_INDEX_COMMON),
};

/* Channel read_raw handler */
static int orient_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2, long mask)
{
	struct orient_state *state = iio_priv(indio_dev);
	int report_id = -1;
	int attr_idx;
	u32 num = 0;
	int ret_type;

	switch (chan->type) {
	case IIO_ROT:
		attr_idx = 0;
		break;
	case IIO_ANGL:
		attr_idx = 1;
		break;
	default:
		attr_idx = 2 + chan->channel;
		break;
	}

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case 0:
		report_id = state->common.report_id;
		if (chan->modified)
			num = chan->channel2 - IIO_MOD_X;
		if (report_id >= 0)
			*val = sensor_hub_input_attr_get_raw_value(
					state->common.hsdev,
					state->common.usage_id,
					orient_addresses[attr_idx],
					report_id, num);
		else {
			*val = 0;
			return -EINVAL;
		}
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = state->orient[attr_idx].units;
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_OFFSET:
		*val = state->orient[attr_idx].unit_expo;
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		ret_type = hid_sensor_read_samp_freq_value(&state->common,
							   val, val2);
		break;
	case IIO_CHAN_INFO_HYSTERESIS:
		ret_type = hid_sensor_read_raw_value(&state->common,
						     &state->common.sensitivity,
						     val, val2);
		break;
	default:
		ret_type = -EINVAL;
		break;
	}

	return ret_type;
}

/* Channel write_raw handler */
static int orient_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2, long mask)
{
	struct orient_state *state = iio_priv(indio_dev);
	int ret = 0;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		ret = hid_sensor_write_samp_freq_value(&state->common,
						       val, val2);
		break;
	case IIO_CHAN_INFO_HYSTERESIS:
		ret = hid_sensor_write_raw_value(&state->common,
						 &state->common.sensitivity,
						 val, val2);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int orient_write_raw_get_fmt(struct iio_dev *indio_dev,
				    struct iio_chan_spec const *chan,
				    long mask)
{
	return IIO_VAL_INT_PLUS_MICRO;
}

static const struct iio_info orient_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &orient_read_raw,
	.write_raw = &orient_write_raw,
	.write_raw_get_fmt = &orient_write_raw_get_fmt,
};

/* Function to push data to buffer */
static void hid_sensor_push_data(struct iio_dev *indio_dev, u8 *data, int len)
{
	dev_dbg(&indio_dev->dev, "hid_sensor_push_data\n");
	iio_push_to_buffers(indio_dev, (u8 *)data);
}

/* Callback handler to send event after all samples are received and captured */
static int orient_proc_event(struct hid_sensor_hub_device *hsdev,
			     unsigned usage_id, void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct orient_state *state = iio_priv(indio_dev);

	dev_dbg(&indio_dev->dev, "orient_proc_event [%d]\n",
		state->common.data_ready);
	if (state->common.data_ready) {
		hid_sensor_push_data(indio_dev, (u8 *)&state->orient_val,
				     sizeof(state->orient_val));
	}

	return 0;
}

/* Capture samples in local storage */
static int orient_capture_sample(struct hid_sensor_hub_device *hsdev,
				 unsigned usage_id,
				 size_t raw_len, size_t raw_count,
				 char *raw_data, void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct orient_state *state = iio_priv(indio_dev);
	int i;
	u32 val;
	int ret = -EINVAL;

	switch (usage_id) {
	case HID_USAGE_SENSOR_DATA_ORIENT_QUATERNION:
		if (raw_count < 4)
			break;
		for (i = 0; i < 4; i++) {
			val = hid_sensor_common_read(&state->orient[0],
						     raw_data, raw_len);
			state->orient_val[CHANNEL_SCAN_INDEX_QUATERNION_X + i] = val;
			raw_data += raw_len;
		}
		ret = 0;
		break;
	case HID_USAGE_SENSOR_DATA_ORIENT_MAGNETIC_HEADING:
		val = hid_sensor_common_read(&state->orient[1], raw_data,
					     raw_len);
		state->orient_val[CHANNEL_SCAN_INDEX_HEADING_ERROR] = val;
		ret = 0;
		break;
	default:
		for (i = 0; i < HID_SENSOR_COMMON_CHANNEL_NB; i++)
			if (usage_id == orient_addresses[2 + i])
				break;
		if (i >= HID_SENSOR_COMMON_CHANNEL_NB)
			break;
		val = hid_sensor_common_read(&state->orient[2 + i], raw_data,
					     raw_len);
		state->orient_val[CHANNEL_SCAN_INDEX_COMMON + i] = val;
		ret = 0;
		break;
	}

	return ret;
}

/* Parse report which is specific to an usage id*/
static int orient_parse_report(struct platform_device *pdev,
			       struct hid_sensor_hub_device *hsdev,
			       struct iio_chan_spec *channels,
			       struct orient_state *st)
{
	int i, ret;

	for (i = 0; i < ORIENT_ATTRIBUTES_NB; ++i) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_INPUT_REPORT,
				st->common.report_id, st->common.usage_id,
				orient_addresses[i],
				&st->orient[i]);
		if (ret < 0)
			return ret;
		dev_dbg(&pdev->dev, "orient #%x %d(%d:%u)\n",
			st->common.report_id, i,
			st->orient[i].index, st->orient[i].count);
	}

	for (i = CHANNEL_SCAN_INDEX_QUATERNION_X; i <= CHANNEL_SCAN_INDEX_QUATERNION_W; ++i)
		hid_sensor_adjust_channel(channels, i, IIO_ROT, &st->common,
					  &st->orient[0]);
	hid_sensor_adjust_channel(channels, CHANNEL_SCAN_INDEX_HEADING_ERROR,
				  IIO_ROT, &st->common, &st->orient[1]);
	for (i = 0; i < HID_SENSOR_COMMON_CHANNEL_NB; ++i)
		hid_sensor_adjust_channel(channels,
					  CHANNEL_SCAN_INDEX_COMMON + i,
					  IIO_ROT, &st->common,
					  &st->orient[2 + i]);

	return 0;
}

/* Function to initialize the processing for usage id */
static int hid_orient_probe(struct platform_device *pdev)
{
	int ret = 0;
	static const char *name = "orient";
	struct iio_dev *indio_dev;
	struct orient_state *state;
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	struct iio_chan_spec *channels;

	indio_dev = iio_device_alloc(sizeof(*state));
	if (indio_dev == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}
	platform_set_drvdata(pdev, indio_dev);
	state = iio_priv(indio_dev);

	ret = hid_sensor_parse_common(hsdev, pdev->id,
			HID_USAGE_SENSOR_TYPE_DEVICE_ORIENTATION,
			HID_USAGE_SENSOR_DATA_ORIENT_QUATERNION,
			&state->common);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup common attributes\n");
		goto error_free_dev;
	}

	channels = kmemdup(orient_channels, sizeof(orient_channels), GFP_KERNEL);
	if (!channels) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "failed to duplicate channels\n");
		goto error_free_dev_common;
	}

	ret = orient_parse_report(pdev, hsdev, channels, state);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup attributes\n");
		goto error_free_dev_channels;
	}

	indio_dev->channels = channels;
	indio_dev->num_channels = ARRAY_SIZE(orient_channels);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &orient_info;
	indio_dev->name = name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	ret = iio_triggered_buffer_setup(indio_dev, &iio_pollfunc_store_time,
					 NULL, NULL);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize trigger buffer\n");
		goto error_free_dev_channels;
	}
	state->common.data_ready = false;
	ret = hid_sensor_setup_trigger(indio_dev, name, &state->common);
	if (ret < 0) {
		dev_err(&pdev->dev, "trigger setup failed\n");
		goto error_unreg_buffer_funcs;
	}

	ret = iio_device_register(indio_dev);
	if (ret) {
		dev_err(&pdev->dev, "device register failed\n");
		goto error_remove_trigger;
	}

	state->callbacks.send_event = orient_proc_event;
	state->callbacks.capture_sample = orient_capture_sample;
	state->callbacks.pdev = pdev;
	ret = sensor_hub_register_callback(hsdev, state->common.report_id,
					   &state->callbacks);
	if (ret < 0) {
		dev_err(&pdev->dev, "callback reg failed\n");
		goto error_iio_unreg;
	}

	return ret;

error_iio_unreg:
	iio_device_unregister(indio_dev);
error_remove_trigger:
	hid_sensor_remove_trigger(indio_dev);
error_unreg_buffer_funcs:
	iio_triggered_buffer_cleanup(indio_dev);
error_free_dev_channels:
	kfree(channels);
error_free_dev_common:
	hid_sensor_free_common(&state->common);
error_free_dev:
	iio_device_free(indio_dev);
error_ret:
	return ret;
}

/* Function to deinitialize the processing for usage id */
static int hid_orient_remove(struct platform_device *pdev)
{
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct orient_state *state = iio_priv(indio_dev);

	sensor_hub_remove_callback(hsdev, state->common.report_id);
	iio_device_unregister(indio_dev);
	hid_sensor_remove_trigger(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	kfree(indio_dev->channels);
	hid_sensor_free_common(&state->common);
	iio_device_free(indio_dev);

	return 0;
}

static struct platform_driver hid_orient_platform_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= hid_orient_probe,
	.remove		= hid_orient_remove,
};
module_platform_driver(hid_orient_platform_driver);

MODULE_DESCRIPTION("HID Sensor Orientation");
MODULE_AUTHOR("Jean-Baptiste Maneyrol <jbmaneyrol@movea.com>");
MODULE_LICENSE("GPL");
