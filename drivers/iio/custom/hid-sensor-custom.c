/*
 * HID Custom Sensor Driver
 * Copyright (c) 2013, Movea SA, Jean-Baptiste Maneyrol <jbmaneyrol@movea.com>
 * Copyright (C) 2015 XiaoMi, Inc.
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
#include <linux/string.h>
#include <linux/hid-sensor-hub.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include "../common/hid-sensors/hid-sensor-trigger.h"

/* Format: HID-SENSOR-usage_id_in_hex */
/* Usage ID from spec for Custom: 0x2000e1 */
#define DRIVER_NAME "HID-SENSOR-2000e1"

struct custom_attribute {
	struct hid_sensor_hub_attribute_info info;
	int index;
};

struct custom_state {
	struct hid_sensor_common common;
	struct hid_sensor_hub_callbacks callbacks;
	struct custom_attribute *input;
	size_t input_nb;
	u32 *input_val;
	size_t input_val_nb;
	struct custom_attribute *output;
	size_t output_nb;
	size_t output_val_nb;
};

static const u32 common_addresses[] = {
	HID_SENSOR_COMMON_ADDRESSES(0),
};

static const struct iio_chan_spec common_channels[] = {
	HID_SENSOR_COMMON_CHANNELS(0),
};

static ssize_t custom_write_output_report(struct iio_dev *indio_dev,
		uintptr_t private, struct iio_chan_spec const *chan,
		const char *buf, size_t len)
{
	struct custom_state *state = iio_priv(indio_dev);
	struct custom_attribute *attr;
	char *str, *tok1, *tok2, *val;
	int val1[32], val2[32];
	char sign;
	unsigned i, idx;
	int ret;

	if (buf[len - 1] != '\0' && buf[len - 1] != '\n')
		return -EINVAL;
	str = kmemdup(buf, len, GFP_KERNEL);
	if (!str)
		return -ENOMEM;
	str[len - 1] = '\0';

	tok1 = str;
	for (i = 0; i < state->output_nb; i++) {
		attr = &state->output[i];
		if (attr->info.count < 1)
			continue;
		tok2 = strsep(&tok1, "\n");
		if (tok2 == NULL) {
			ret = -EINVAL;
			goto exit;
		}
		for (idx = 0; idx < attr->info.count; idx++) {
			val = strsep(&tok2, ",");
			if (val == NULL) {
				ret = -EINVAL;
				goto exit;
			}
			ret = sscanf(val, "%d.%6u", &val1[idx], &val2[idx]);
			if (ret < 2) {
				ret = -EINVAL;
				goto exit;
			}
			if (val1[idx] == 0) {
				val = skip_spaces(val);
				sign = val[0];
				if (sign == '-')
					val2[idx] = -val2[idx];
			}
		}
		hid_sensor_write_output_field(&state->common, &attr->info,
					      val1, val2);
	}

	ret = sensor_hub_send_output(state->common.hsdev, state->common.report_id);
exit:
	kfree(str);
	return ret;
}

static const struct iio_chan_spec_ext_info custom_output_ext[] = {
	{
		.name = "report",
		.shared = true,
		.write = custom_write_output_report,
	},
	{ }
};

/* Set channel based on report descriptor */
static void custom_set_channel(struct iio_chan_spec *chan,
			       enum iio_chan_type type, int indexed, int index,
			       int channel, int mod, bool output,
			       const struct hid_sensor_hub_attribute_info *info,
			       const struct iio_chan_spec_ext_info *ext)
{
	chan->type = type;
	chan->indexed = indexed;
	chan->channel = channel;
	if (mod >= 0) {
		chan->modified = 1;
		chan->channel2 = IIO_MOD_X + mod;
	}
	if (!output) {
		chan->info_mask_separate = BIT(IIO_CHAN_INFO_OFFSET) |
			BIT(IIO_CHAN_INFO_SCALE);
		if (chan->type == IIO_CUSTOM)
			chan->info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SAMP_FREQ) |
				BIT(IIO_CHAN_INFO_HYSTERESIS);
	} else {
		chan->output = 1;
	}
	chan->scan_index = index;
	if (info->logical_minimum < 0)
		chan->scan_type.sign = 's';
	else
		chan->scan_type.sign = 'u';
	chan->scan_type.realbits = info->size * 8;
	/* Maximum size of a sample is u32 */
	chan->scan_type.storagebits = sizeof(u32) * 8;
	chan->ext_info = ext;
}

/* Channel read_raw handler */
static int custom_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val, int *val2,
			   long mask)
{
	struct custom_state *state = iio_priv(indio_dev);
	struct custom_attribute *attr;
	u32 attr_id;
	unsigned i;
	u32 num = 0;
	int ret_type;

	if (chan->type == IIO_CUSTOM) {
		attr_id = HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(chan->channel);
		if (chan->modified)
			num = chan->channel2 - IIO_MOD_X;
	} else
		attr_id = common_addresses[chan->channel];

	for (i = 0; i < state->input_nb; ++i) {
		attr = &state->input[i];
		if (attr->info.attrib_id == attr_id)
			break;
	}
	if (i >= state->input_nb)
		return -EINVAL;

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case 0:
		if (state->common.report_id < 0)
			return -EINVAL;
		*val = sensor_hub_input_attr_get_raw_value(
				state->common.hsdev,
				state->common.usage_id, attr->info.attrib_id,
				state->common.report_id, num);
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = attr->info.units;
		ret_type = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_OFFSET:
		*val = attr->info.unit_expo;
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
static int custom_write_raw(struct iio_dev *indio_dev,
			    struct iio_chan_spec const *chan,
			    int val, int val2,
			    long mask)
{
	struct custom_state *state = iio_priv(indio_dev);
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

static int custom_write_raw_get_fmt(struct iio_dev *indio_dev,
				    struct iio_chan_spec const *chan,
				    long mask)
{
	return IIO_VAL_INT_PLUS_MICRO;
}

static const struct iio_info custom_info = {
	.driver_module = THIS_MODULE,
	.read_raw = &custom_read_raw,
	.write_raw = &custom_write_raw,
	.write_raw_get_fmt = &custom_write_raw_get_fmt,
};

/* Function to push data to buffer */
static void hid_sensor_push_data(struct iio_dev *indio_dev, u8 *data, int len)
{
	dev_dbg(&indio_dev->dev, "hid_sensor_push_data\n");
	iio_push_to_buffers(indio_dev, (u8 *)data);
}

/* Callback handler to send event after all samples are received and captured */
static int custom_proc_event(struct hid_sensor_hub_device *hsdev,
			     unsigned usage_id,
			     void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct custom_state *state = iio_priv(indio_dev);

	dev_dbg(&indio_dev->dev, "custom_proc_event [%d]\n",
		state->common.data_ready);
	if (state->common.data_ready) {
		hid_sensor_push_data(indio_dev, (u8 *)state->input_val,
			state->input_val_nb * sizeof(*state->input_val));
	}

	return 0;
}

/* Capture samples in local storage */
static int custom_capture_sample(struct hid_sensor_hub_device *hsdev,
				 unsigned usage_id,
				 size_t raw_len, size_t raw_count,
				 char *raw_data, void *priv)
{
	struct iio_dev *indio_dev = platform_get_drvdata(priv);
	struct custom_state *state = iio_priv(indio_dev);
	struct custom_attribute *attr;
	int i, j;
	u32 val;
	int ret = -EINVAL;

	for (i = 0; i < state->input_nb; i++) {
		attr = &state->input[i];
		if (attr->info.attrib_id != usage_id) {
			continue;
		}
		if (raw_count > attr->info.count)
			raw_count = attr->info.count;
		for (j = 0; j < raw_count; j++) {
			val = hid_sensor_common_read(&attr->info, raw_data, raw_len);
			state->input_val[attr->index + j] = val;
			raw_data += raw_len;
		}
		ret = 0;
		break;
	}

	return ret;
}

/* Parse report which is specific to an usage id*/
static int custom_parse_input_report(struct hid_sensor_hub_device *hsdev,
				     struct custom_state *st)
{
	struct hid_sensor_hub_attribute_info info;
	int ret;
	unsigned i, idx;

	/* Init and test if input report is present */
	st->input_nb = 0;
	st->input_val_nb = 0;
	ret = sensor_hub_input_get_attribute_info(hsdev, HID_INPUT_REPORT,
			st->common.report_id, st->common.usage_id,
			HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(0), &info);
	if (ret == -ENODEV) {
		ret = 0;
		goto error;
	}

	/* Parse report to get number of custom attributes */
	for (i = 0; i < HID_USAGE_SENSOR_DATA_CUSTOM_VALUES_NB; ++i) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_INPUT_REPORT,
				st->common.report_id, st->common.usage_id,
				HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(i), &info);
		if (ret == 0)
			st->input_nb++;
	}
	if (st->input_nb == 0) {
		ret = 0;
		goto error;
	}
	st->input_nb += HID_SENSOR_COMMON_CHANNEL_NB;

	/* Allocate and parse attributes info and indexes */
	st->input = kcalloc(st->input_nb, sizeof(*st->input), GFP_KERNEL);
	if (!st->input) {
		ret = -ENOMEM;
		goto error;
	}

	idx = HID_SENSOR_COMMON_CHANNEL_NB;
	for (i = 0; i < HID_USAGE_SENSOR_DATA_CUSTOM_VALUES_NB; ++i) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_INPUT_REPORT,
				st->common.report_id, st->common.usage_id,
				HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(i),
				&st->input[idx].info);
		if (ret == 0) {
			st->input[idx].index = st->input_val_nb;
			if (st->input[idx].info.count > 3)
				st->input[idx].info.count = 3;
			st->input_val_nb += st->input[idx].info.count;
			++idx;
			if (idx == st->input_nb)
				break;
		}
	}

	for (idx = 0; idx < HID_SENSOR_COMMON_CHANNEL_NB; ++idx) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_INPUT_REPORT,
				st->common.report_id, st->common.usage_id,
				common_addresses[idx], &st->input[idx].info);
		if (ret != 0) {
			ret = -ENODEV;
			goto error_free_input;
		}
		st->input[idx].index = st->input_val_nb;
		if (st->input[idx].info.count > 1)
			st->input[idx].info.count = 1;			
		st->input_val_nb += st->input[idx].info.count;
	}

	/* Allocate buffer for custom input values */
	st->input_val = kcalloc(st->input_val_nb, sizeof(*st->input_val),
				GFP_KERNEL);
	if (!st->input_val) {
		ret = -ENOMEM;
		goto error_free_input;
	}

	return 0;
error_free_input:
	kfree(st->input);
error:
	return ret;
}

static int custom_parse_output_report(struct hid_sensor_hub_device *hsdev,
				      struct custom_state *st)
{
	struct hid_sensor_hub_attribute_info info;
	int ret;
	unsigned i, idx;

	/* Init and test if output report is present */
	st->output_nb = 0;
	st->output_val_nb = 0;
	ret = sensor_hub_input_get_attribute_info(hsdev, HID_OUTPUT_REPORT,
			st->common.report_id, st->common.usage_id,
			HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(0), &info);
	if (ret == -ENODEV)
		return 0;

	/* Parse report to get number of custom attributes */
	for (i = 0; i < HID_USAGE_SENSOR_DATA_CUSTOM_VALUES_NB; ++i) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_OUTPUT_REPORT,
				st->common.report_id, st->common.usage_id,
				HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(i), &info);
		if (ret == 0)
			st->output_nb++;
	}
	if (st->output_nb == 0)
		return 0;

	/* Allocate and parse attributes info for output */
	st->output = kcalloc(st->output_nb, sizeof(*st->output), GFP_KERNEL);
	if (!st->output) {
		return -ENOMEM;
	}

	for (i = 0, idx = 0; i < HID_USAGE_SENSOR_DATA_CUSTOM_VALUES_NB; ++i) {
		ret = sensor_hub_input_get_attribute_info(hsdev,
				HID_OUTPUT_REPORT,
				st->common.report_id, st->common.usage_id,
				HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(i),
				&st->output[idx].info);
		if (ret == 0) {
			st->output[idx].index = st->output_val_nb;
			if (st->output[idx].info.count > 3)
				st->output[idx].info.count = 3;
			st->output_val_nb += st->output[idx].info.count;
			++idx;
			if (idx == st->output_nb)
				break;
		}
	}

	return 0;
}

static int custom_allocate_channels(struct platform_device *pdev,
				    struct hid_sensor_hub_device *hsdev,
				    struct iio_chan_spec **channels,
				    struct custom_state *st)
{
	struct iio_chan_spec *chan;
	struct custom_attribute *attr;
	unsigned i, idx;
	int num, mod;

	/* Allocate channels */
	*channels = kcalloc(st->input_val_nb + st->output_val_nb,
			    sizeof(**channels), GFP_KERNEL);
	if (!*channels)
		return -ENOMEM;

	/* Fill input channels */
	for (i = 0, idx = 0; i < st->input_nb; ++i) {
		attr = &st->input[i];
		if (i < HID_SENSOR_COMMON_CHANNEL_NB) {
			chan = &(*channels)[idx];
			custom_set_channel(chan, common_channels[i].type,
					   common_channels[i].indexed,
				   	   attr->index,
					   common_channels[i].channel, -1,
					   false, &attr->info, NULL);
			++idx;
		} else {
			num = attr->info.attrib_id -
					HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(0);
			if (attr->info.count > 1) {
				for (mod = 0; mod < attr->info.count; ++mod) {
					chan = &(*channels)[idx];
					custom_set_channel(chan, IIO_CUSTOM,
							   true,
							   attr->index + mod,
							   num, mod, false,
							   &attr->info,
							   st->common.ext_info);
					++idx;
				}
			} else {
				chan = &(*channels)[idx];
				custom_set_channel(chan, IIO_CUSTOM, true,
						   attr->index, num, -1, false,
						   &attr->info,
						   st->common.ext_info);
				++idx;
			}
		}
	}

	/* Fill output channels */
	for (i = 0; i < st->output_nb; ++i) {
		attr = &st->output[i];
		num = attr->info.attrib_id -
				HID_USAGE_SENSOR_DATA_CUSTOM_VALUES(0);
		if (attr->info.count > 1) {
			for (mod = 0; mod < attr->info.count; ++mod) {
				chan = &(*channels)[idx];
				custom_set_channel(chan, IIO_CUSTOM, true,
						   attr->index + mod, num, mod,
						   true, &attr->info,
						   custom_output_ext);
				++idx;
			}
		} else {
			chan = &(*channels)[idx];
			custom_set_channel(chan, IIO_CUSTOM, true, attr->index,
					   num, -1, 1, &attr->info,
					   custom_output_ext);
			++idx;
		}
	}

	dev_dbg(&pdev->dev, "custom #%u: input_nb %u, input_val_nb %u, "
		"output_nb %u, output_val_nb %u\n",
		st->common.report_id, st->input_nb, st->input_val_nb,
		st->output_nb, st->output_val_nb);
	return 0;
}

/* Function to initialize the processing for usage id */
static int hid_custom_probe(struct platform_device *pdev)
{
	int ret = 0;
	static const char *name = "custom";
	struct iio_dev *indio_dev;
	struct custom_state *state;
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
			HID_USAGE_SENSOR_TYPE_CUSTOM,
			HID_USAGE_SENSOR_DATA_CUSTOM_VALUE,
			&state->common);
	if (ret) {
		dev_err(&pdev->dev, "failed to setup common attributes\n");
		goto error_free_dev;
	}

	ret = custom_parse_input_report(hsdev, state);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse input attributes\n");
		goto error_free_dev_common;
	}
	ret = custom_parse_output_report(hsdev, state);
	if (ret) {
		dev_err(&pdev->dev, "failed to parse output attributes\n");
		goto error_free_input_attr;
	}
	ret = custom_allocate_channels(pdev, hsdev, &channels, state);
	if (ret) {
		dev_err(&pdev->dev, "failed to allocate channels\n");
		goto error_free_output_attr;
	}

	indio_dev->channels = channels;
	indio_dev->num_channels = state->input_val_nb + state->output_val_nb;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &custom_info;
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

	state->callbacks.send_event = custom_proc_event;
	state->callbacks.capture_sample = custom_capture_sample;
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
error_free_output_attr:
	kfree(state->output);
error_free_input_attr:
	kfree(state->input);
	kfree(state->input_val);
error_free_dev_common:
	hid_sensor_free_common(&state->common);
error_free_dev:
	iio_device_free(indio_dev);
error_ret:
	return ret;
}

/* Function to deinitialize the processing for usage id */
static int hid_custom_remove(struct platform_device *pdev)
{
	struct hid_sensor_hub_device *hsdev = pdev->dev.platform_data;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct custom_state *state = iio_priv(indio_dev);

	sensor_hub_remove_callback(hsdev, state->common.report_id);
	iio_device_unregister(indio_dev);
	hid_sensor_remove_trigger(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);
	kfree(indio_dev->channels);
	kfree(state->output);
	kfree(state->input);
	kfree(state->input_val);
	hid_sensor_free_common(&state->common);
	iio_device_free(indio_dev);

	return 0;
}

static struct platform_driver hid_custom_platform_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= hid_custom_probe,
	.remove		= hid_custom_remove,
};
module_platform_driver(hid_custom_platform_driver);

MODULE_DESCRIPTION("Custom HID Sensor");
MODULE_AUTHOR("Jean-Baptiste Maneyrol <jbmaneyrol@movea.com>");
MODULE_LICENSE("GPL");
