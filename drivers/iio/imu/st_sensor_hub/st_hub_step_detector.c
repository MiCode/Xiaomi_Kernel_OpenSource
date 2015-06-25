/*
 * STMicroelectronics st_sensor_hub step detector driver
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
#include <linux/iio/events.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <asm/unaligned.h>

#include "st_sensor_hub.h"

static const struct iio_chan_spec st_hub_step_detector_ch[] = {
	{
		.type = IIO_STEP_DETECTOR,
		.channel = 0,
		.modified = 0,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_type = {
			.sign = 'u',
			.realbits = 16,
			.shift = 0,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_RISING),
	},
	IIO_CHAN_SOFT_TIMESTAMP(1)
};

static ST_HUB_BATCH_MAX_EVENT_COUNT();
static ST_HUB_BATCH_BUFFER_LENGTH();
static ST_HUB_BATCH_TIMEOUT();
static ST_HUB_BATCH_AVAIL();
static ST_HUB_BATCH();

static void st_hub_step_detector_push_data(struct platform_device *pdev,
						u8 *data, int64_t timestamp)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct st_hub_sensor_data *sdata = iio_priv(indio_dev);

	sdata->buffer[0] = data[0];
	sdata->buffer[1] = 0;

	iio_push_to_buffers_with_timestamp(indio_dev, sdata->buffer, timestamp);
}

static void st_hub_step_detector_push_event(struct platform_device *pdev,
							int64_t timestamp)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_push_event(indio_dev, IIO_UNMOD_EVENT_CODE(IIO_STEP_DETECTOR, 0,
						IIO_EV_TYPE_THRESH,
						IIO_EV_DIR_EITHER),
						timestamp);
}

static struct attribute *st_hub_step_detector_attributes[] = {
	&iio_dev_attr_batch_mode_max_event_count.dev_attr.attr,
	&iio_dev_attr_batch_mode_buffer_length.dev_attr.attr,
	&iio_dev_attr_batch_mode_timeout.dev_attr.attr,
	&iio_dev_attr_batch_mode_available.dev_attr.attr,
	&iio_dev_attr_batch_mode.dev_attr.attr,
	NULL,
};

static int st_hub_step_detector_read_event_value(struct iio_dev *indio_dev,
						u64 event_code, int *val)
{
	*val = 0;

	return 0;
}

static int st_hub_step_detector_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *ch, int *val, int *val2, long mask)
{
	int err;

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED)
			return -EBUSY;

		err = st_hub_step_detector_read_event_value(indio_dev,
						0, val);
		if (err < 0)
			return err;

		*val = *val >> ch->scan_type.shift;
		return IIO_VAL_INT;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

static int st_hub_step_detector_read_event_config(struct iio_dev *indio_dev,
						u64 event_code)
{
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	if (info->hdata->enabled_sensor & (1ULL << info->index))
		return 1;

	return 0;
}

static const struct attribute_group st_hub_step_detector_attribute_group = {
	.attrs = st_hub_step_detector_attributes,
};

static const struct iio_info st_hub_step_detector_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_hub_step_detector_attribute_group,
	.read_event_value = &st_hub_step_detector_read_event_value,
	.read_event_config = &st_hub_step_detector_read_event_config,
	.read_raw = &st_hub_step_detector_read_raw,
};

static const struct iio_buffer_setup_ops st_hub_buffer_setup_ops = {
	.preenable = &st_hub_buffer_preenable,
	.postenable = &st_hub_buffer_postenable,
	.predisable = &st_hub_buffer_predisable,
};

static int st_hub_step_detector_probe(struct platform_device *pdev)
{
	int err;
	struct iio_dev *indio_dev;
	struct st_hub_pdata_info *info;
	struct st_hub_sensor_data *adata;
	struct st_sensor_hub_callbacks callback;

	indio_dev = iio_device_alloc(sizeof(*adata));
	if (!indio_dev)
		return -ENOMEM;

	platform_set_drvdata(pdev, indio_dev);

	indio_dev->channels = st_hub_step_detector_ch;
	indio_dev->num_channels = ARRAY_SIZE(st_hub_step_detector_ch);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &st_hub_step_detector_info;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	adata = iio_priv(indio_dev);
	info = pdev->dev.platform_data;
	st_hub_get_common_data(info->hdata, info->index, &adata->cdata);

	err = st_hub_set_default_values(adata, info, indio_dev);
	if (err < 0)
		goto st_hub_deallocate_device;

	err = iio_triggered_buffer_setup(indio_dev, NULL,
						NULL, &st_hub_buffer_setup_ops);
	if (err)
		goto st_hub_deallocate_device;

	err = st_hub_setup_trigger_sensor(indio_dev, adata);
	if (err < 0)
		goto st_hub_clear_buffer;

	err = iio_device_register(indio_dev);
	if (err)
		goto st_hub_remove_trigger;

	callback.pdev = pdev;
	callback.push_data = &st_hub_step_detector_push_data;
	callback.push_event = &st_hub_step_detector_push_event;
	st_hub_register_callback(info->hdata, &callback, info->index);

	return 0;

st_hub_remove_trigger:
	st_hub_remove_trigger(adata);
st_hub_clear_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
st_hub_deallocate_device:
	iio_device_free(indio_dev);
	return err;
}

static int st_hub_step_detector_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct st_hub_sensor_data *adata = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	st_hub_remove_trigger(adata);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static struct platform_device_id st_hub_step_detector_ids[] = {
	{ CONCATENATE_STRING(LIS331EB_DEV_NAME, "step_d") },
	{ CONCATENATE_STRING(LSM6DB0_DEV_NAME, "step_d") },
	{ CONCATENATE_STRING(LIS331EB_DEV_NAME, "step_d_wk") },
	{ CONCATENATE_STRING(LSM6DB0_DEV_NAME, "step_d_wk") },
	{},
};
MODULE_DEVICE_TABLE(platform, st_hub_step_detector_ids);

static struct platform_driver st_hub_step_detector_platform_driver = {
	.id_table = st_hub_step_detector_ids,
	.driver = {
		.name	= KBUILD_MODNAME,
		.owner	= THIS_MODULE,
	},
	.probe		= st_hub_step_detector_probe,
	.remove		= st_hub_step_detector_remove,
};
module_platform_driver(st_hub_step_detector_platform_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub step detector driver");
MODULE_LICENSE("GPL v2");
