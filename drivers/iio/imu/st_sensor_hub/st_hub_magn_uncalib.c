/*
 * STMicroelectronics st_sensor_hub magnetometer uncalibrated driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Denis Ciocca <denis.ciocca@st.com>
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <asm/unaligned.h>

#include "st_sensor_hub.h"

#define ST_HUB_MAGN_UNCALIB_NUM_DATA_CH		3
#define ST_HUB_MAGN_UNCALIB_DATA_BYTE		2

static const struct iio_chan_spec st_hub_magn_uncalib_ch[] = {
	ST_HUB_DEVICE_CHANNEL(IIO_MAGN, 0, 1, IIO_MOD_X, IIO_LE, 32, 32,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE)
				| BIT(IIO_CHAN_INFO_OFFSET), 0, 0, 's'),
	ST_HUB_DEVICE_CHANNEL(IIO_MAGN, 1, 1, IIO_MOD_Y, IIO_LE, 32, 32,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE)
				| BIT(IIO_CHAN_INFO_OFFSET), 0, 0, 's'),
	ST_HUB_DEVICE_CHANNEL(IIO_MAGN, 2, 1, IIO_MOD_Z, IIO_LE, 32, 32,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SCALE)
				| BIT(IIO_CHAN_INFO_OFFSET), 0, 0, 's'),
	IIO_CHAN_SOFT_TIMESTAMP(3)
};

static ST_HUB_DEV_ATTR_SAMP_FREQ_AVAIL();
static ST_HUB_DEV_ATTR_SAMP_FREQ();
static ST_HUB_BATCH_MAX_EVENT_COUNT();
static ST_HUB_BATCH_BUFFER_LENGTH();
static ST_HUB_BATCH_TIMEOUT();
static ST_HUB_BATCH_AVAIL();
static ST_HUB_BATCH();

static void st_hub_magn_uncalib_push_data(struct platform_device *pdev,
						u8 *data, int64_t timestamp)
{
	int i;
	u8 *sensor_data = data;
	size_t byte_for_channel;
	unsigned int init_copy = 0;
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct st_hub_sensor_data *mudata = iio_priv(indio_dev);

	for (i = 0; i < ST_HUB_MAGN_UNCALIB_NUM_DATA_CH; i++) {
		byte_for_channel =
			indio_dev->channels[i].scan_type.storagebits >> 3;
		if (test_bit(i, indio_dev->active_scan_mask)) {
			memcpy(&mudata->buffer[init_copy],
						sensor_data, byte_for_channel);
			init_copy += byte_for_channel;
		}
		sensor_data += byte_for_channel;
	}

	iio_push_to_buffers_with_timestamp(indio_dev,
						mudata->buffer, timestamp);
}

static int st_hub_read_axis_data(struct iio_dev *indio_dev,
				unsigned int index, int *data, long mask)
{
	int err;
	u8 *outdata;
	struct st_hub_sensor_data *gdata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;
	unsigned int byte_for_channel =
			indio_dev->channels[0].scan_type.storagebits >> 3;

	outdata = kmalloc(gdata->cdata->payload_byte, GFP_KERNEL);
	if (!outdata)
		return -EINVAL;

	err = st_hub_set_enable(info->hdata, info->index, true, true, 0, true);
	if (err < 0)
		goto st_hub_read_axis_free_memory;

	err = st_hub_read_axis_data_asincronous(info->hdata, info->index,
					outdata, gdata->cdata->payload_byte);
	if (err < 0)
		goto st_hub_read_axis_free_memory;

	err = st_hub_set_enable(info->hdata, info->index, false, true, 0, true);
	if (err < 0)
		goto st_hub_read_axis_free_memory;

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		*data = (s16)
			get_unaligned_le16(&outdata[byte_for_channel * index]);
		break;
	case IIO_CHAN_INFO_OFFSET:
		*data = (s16)
			get_unaligned_le16(&outdata[byte_for_channel *
					index + ST_HUB_MAGN_UNCALIB_DATA_BYTE]);
		break;
	default:
		break;
	}

st_hub_read_axis_free_memory:
	kfree(outdata);
	return err;
}

static int st_hub_magn_uncalib_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *ch, int *val, int *val2, long mask)
{
	int err;
	struct st_hub_sensor_data *gdata = iio_priv(indio_dev);

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
	case IIO_CHAN_INFO_OFFSET:
		if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED)
			return -EBUSY;

		err = st_hub_read_axis_data(indio_dev, ch->scan_index,
								val, mask);
		if (err < 0)
			return err;

		*val = *val >> ch->scan_type.shift;
		return IIO_VAL_INT;
	case IIO_CHAN_INFO_SCALE:
		*val2 = gdata->cdata->gain;
		return IIO_VAL_INT_PLUS_MICRO;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static struct attribute *st_hub_magn_uncalib_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_batch_mode_max_event_count.dev_attr.attr,
	&iio_dev_attr_batch_mode_buffer_length.dev_attr.attr,
	&iio_dev_attr_batch_mode_timeout.dev_attr.attr,
	&iio_dev_attr_batch_mode_available.dev_attr.attr,
	&iio_dev_attr_batch_mode.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_hub_magn_uncalib_attribute_group = {
	.attrs = st_hub_magn_uncalib_attributes,
};

static const struct iio_info st_hub_magn_uncalib_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_hub_magn_uncalib_attribute_group,
	.read_raw = &st_hub_magn_uncalib_read_raw,
};

static const struct iio_buffer_setup_ops st_hub_buffer_setup_ops = {
	.preenable = &st_hub_buffer_preenable,
	.postenable = &st_hub_buffer_postenable,
	.predisable = &st_hub_buffer_predisable,
};

static int st_hub_magn_uncalib_probe(struct platform_device *pdev)
{
	int err;
	struct iio_dev *indio_dev;
	struct st_hub_pdata_info *info;
	struct st_hub_sensor_data *gdata;
	struct st_sensor_hub_callbacks callback;

	indio_dev = iio_device_alloc(sizeof(*gdata));
	if (!indio_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, indio_dev);

	indio_dev->channels = st_hub_magn_uncalib_ch;
	indio_dev->num_channels = ARRAY_SIZE(st_hub_magn_uncalib_ch);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &st_hub_magn_uncalib_info;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	gdata = iio_priv(indio_dev);
	info = pdev->dev.platform_data;
	st_hub_get_common_data(info->hdata, info->index, &gdata->cdata);

	err = st_hub_set_default_values(gdata, info, indio_dev);
	if (err < 0)
		goto st_hub_deallocate_device;

	err = iio_triggered_buffer_setup(indio_dev, NULL,
						NULL, &st_hub_buffer_setup_ops);
	if (err)
		goto st_hub_deallocate_device;

	err = st_hub_setup_trigger_sensor(indio_dev, gdata);
	if (err < 0)
		goto st_hub_clear_buffer;

	err = iio_device_register(indio_dev);
	if (err)
		goto st_hub_remove_trigger;

	callback.pdev = pdev;
	callback.push_data = &st_hub_magn_uncalib_push_data;
	st_hub_register_callback(info->hdata, &callback, info->index);

	return 0;

st_hub_remove_trigger:
	st_hub_remove_trigger(gdata);
st_hub_clear_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
st_hub_deallocate_device:
	iio_device_free(indio_dev);
	return err;
}

static int st_hub_magn_uncalib_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct st_hub_sensor_data *gdata = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	st_hub_remove_trigger(gdata);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static struct platform_device_id st_hub_magn_uncalib_ids[] = {
	{ CONCATENATE_STRING(LIS331EB_DEV_NAME, "magn_u") },
	{ CONCATENATE_STRING(LSM6DB0_DEV_NAME, "magn_u") },
	{ CONCATENATE_STRING(LIS331EB_DEV_NAME, "magn_u_wk") },
	{ CONCATENATE_STRING(LSM6DB0_DEV_NAME, "magn_u_wk") },
	{},
};
MODULE_DEVICE_TABLE(platform, st_hub_magn_uncalib_ids);

static struct platform_driver st_hub_magn_uncalib_platform_driver = {
	.id_table = st_hub_magn_uncalib_ids,
	.driver = {
		.name	= KBUILD_MODNAME,
		.owner	= THIS_MODULE,
	},
	.probe		= st_hub_magn_uncalib_probe,
	.remove		= st_hub_magn_uncalib_remove,
};
module_platform_driver(st_hub_magn_uncalib_platform_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub magnetometer uncalibrated driver");
MODULE_LICENSE("GPL v2");
