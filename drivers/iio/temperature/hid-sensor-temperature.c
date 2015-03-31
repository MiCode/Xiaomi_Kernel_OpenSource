/*
 * HID Sensor Temperature Driver
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

/* Format: HID-SENSOR-usage_id_in_hex */
/* Usage ID from spec for Temperature: 0x200033 */
#define DRIVER_NAME "HID-SENSOR-200033"

enum temp_channel {
	CHANNEL_SCAN_INDEX_TEMP,
	CHANNEL_SCAN_INDEX_COMMON,
	TEMP_CHANNEL_NB = CHANNEL_SCAN_INDEX_COMMON +
			HID_SENSOR_COMMON_CHANNEL_NB,
};

struct temp_state {
	struct hid_sensor_common common;
	struct hid_sensor_hub_callbacks callbacks;
	struct hid_sensor_hub_attribute_info temp[TEMP_CHANNEL_NB];
	u32 temp_val[TEMP_CHANNEL_NB];
};

static const u32 temp_addresses[TEMP_CHANNEL_NB] = {
	[CHANNEL_SCAN_INDEX_TEMP] = HID_USAGE_SENSOR_DATA_TEMPERATURE,
	HID_SENSOR_COMMON_ADDRESSES(CHANNEL_SCAN_INDEX_COMMON),
};

/* Channel definitions */
static const struct iio_chan_spec temp_channels[] = {
	{
		.type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_OFFSET) |
			BIT(IIO_CHAN_INFO_SCALE),
		.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
			BIT(IIO_CHAN_INFO_HYSTERESIS),
		.scan_index = CHANNEL_SCAN_INDEX_TEMP,
	},
	HID_SENSOR_COMMON_CHANNELS(CHANNEL_SCAN_INDEX_COMMON),
};

/* Channel read_raw handler */
static int temp_read_raw(struct iio_dev *indio_dev,
			 const struct iio_chan_spec *chan,
			 int *val, int *val2, long mask)
{
	struct temp_state *state = iio_priv(indio_dev);
	int report_id = -1;
	int ret_type;

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case 0:
		report_id = state->common.report_id;
		if (report_id >= 0)
			*val = sensor_hub_input_attr_get_raw_value(
					state->common.hsdev,
					state->common.usage_id,
					temp_addresses[chan->scan_index],
					report_id, 0);
		else {
			*val = 0;
			return -EINVAL;
		}
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = state->temp[chan->scan_index].units;
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_OFFSET:
		*val = state->temp[chan->scan_index].unit_expo;
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
static int temp_write_raw(struct iio_dev *indio_dev,
			  const struct iio_chan_spec const *chan,
			  int val, int val2, long mask)
{
	struct temp_state *state = iio_priv(indio_dev);
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

static int temp_write_raw_get_fmt(struct iio_dev *indio_dev,
				  const struct iio_chan_spec const *chan,
				  long mask)
{
	return IIO_VAL_INT_PLUS_MICRO;
}

static const struct iio_info temp_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &temp_read_raw,
	.write_raw = &temp_write_raw,
	.write_raw_get_fmt = &temp_write_raw_get_fmt,
};

/* Function to push data to buffer */
static void hid_sensor_push_data(struct iio_dev *indio_dev, u8 *data, int len)
{
	dev_dbg(&indio_dev->dev, "hid_sensor_push_data\n");
	iio_push_to_buffers(indio_dev, (u8 *)data);
}

/* Callback handler to send event after all samples are received and captured */
static int temp_proc_event(struct hid_sensor_hub_device *hsdev,
			   unsigned usage_id,
			   void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct temp_state *state = iio_priv(indio_dev);

	dev_dbg(&indio_dev->dev, "temp_proc_event [%d]\n",
		state->common.data_ready);
	if (state->common.data_ready) {
		hid_sensor_push_data(indio_dev, (u8 *)&state->temp_val,
				     sizeof(state->temp_val));
	}

	return 0;
}

/* Capture samples in local storage */
static int temp_capture_sample(struct hid_sensor_hub_device *hsdev,
			       unsigned usage_id,
			       size_t raw_len, size_t raw_count,
			       char *raw_data, void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct temp_state *state = iio_priv(indio_dev);
	int index;

	for (index = 0; index < TEMP_CHANNEL_NB; ++index)
		if (usage_id == temp_addresses[index])
			break;
	if (index >= TEMP_CHANNEL_NB)
		return -EINVAL;

	state->temp_val[index] =
			hid_sensor_common_read(&state->temp[index],
					       raw_data, raw_len);

	return 0;
}

/* Parse report which is specific to an usage id*/
static int temp_parse_report(struct platform_device *pdev,
			     struct hid_sensor_hub_device *hsdev,
			     struct iio_chan_spec *channels,
			     struct temp_state *st)
{
	int ret;
	int i;

	for (i = 0; i < TEMP_CHANNEL_NB; ++i) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_INPUT_REPORT,
				st->common.report_id, st->common.usage_id,
				temp_addresses[i], &st->temp[i]);
		if (ret < 0)
			return ret;
		hid_sensor_adjust_channel(channels, i, IIO_TEMP,
				&st->common, &st->temp[i]);
		dev_dbg(&pdev->dev, "temp #%x %d(%d:%u)\n",
			st->common.report_id, i,
			st->temp[i].index, st->temp[i].count);
	}

	return 0;
}

/* Function to initialize the processing for usage id */
static int hid_temp_probe(struct platform_device *pdev)
{
	int ret = 0;
	static const char *name = "temp";
	struct iio_dev *indio_dev;
	struct temp_state *state;
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
			HID_USAGE_SENSOR_TYPE_TEMPERATURE,
			HID_USAGE_SENSOR_DATA_TEMPERATURE,
			&state->common);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup common attributes\n");
		goto error_free_dev;
	}

	channels = kmemdup(temp_channels, sizeof(temp_channels), GFP_KERNEL);
	if (!channels) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "failed to duplicate channels\n");
		goto error_free_dev_common;
	}

	ret = temp_parse_report(pdev, hsdev, channels, state);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup attributes\n");
		goto error_free_dev_channels;
	}

	indio_dev->channels = channels;
	indio_dev->num_channels = ARRAY_SIZE(temp_channels);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &temp_info;
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

	state->callbacks.send_event = temp_proc_event;
	state->callbacks.capture_sample = temp_capture_sample;
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
static int hid_temp_remove(struct platform_device *pdev)
{
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct temp_state *state = iio_priv(indio_dev);

	sensor_hub_remove_callback(hsdev, state->common.report_id);
	iio_device_unregister(indio_dev);
	hid_sensor_remove_trigger(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	kfree(indio_dev->channels);
	hid_sensor_free_common(&state->common);
	iio_device_free(indio_dev);

	return 0;
}

static struct platform_driver hid_temp_platform_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= hid_temp_probe,
	.remove		= hid_temp_remove,
};
module_platform_driver(hid_temp_platform_driver);

MODULE_DESCRIPTION("HID Sensor Temperature");
MODULE_AUTHOR("Jean-Baptiste Maneyrol <jbmaneyrol@movea.com>");
MODULE_LICENSE("GPL");
