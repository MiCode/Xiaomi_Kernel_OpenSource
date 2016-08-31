/*
 * STMicroelectronics st_sensor_hub significant motion driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
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

static const struct iio_chan_spec st_hub_sign_motion_ch[] = {
	{
		.type = IIO_SIGN_MOTION,
		.channel = 0,
		.modified = 0,
		.event_mask = IIO_EV_BIT(IIO_EV_TYPE_THRESH, IIO_EV_DIR_RISING),
	},
	IIO_CHAN_SOFT_TIMESTAMP(1)
};

static void st_hub_sign_motion_push_event(struct platform_device *pdev,
							int64_t timestamp)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);

	iio_push_event(indio_dev, IIO_UNMOD_EVENT_CODE(IIO_SIGN_MOTION, 0,
						IIO_EV_TYPE_THRESH,
						IIO_EV_DIR_EITHER),
						timestamp);

	return;
}

static struct attribute *st_hub_sign_motion_attributes[] = {
	NULL,
};

static const struct attribute_group st_hub_sign_motion_attribute_group = {
	.attrs = st_hub_sign_motion_attributes,
};

static const struct iio_info st_hub_sign_motion_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_hub_sign_motion_attribute_group,
};

static int st_hub_sign_motion_buffer_postenable(struct iio_dev *indio_dev)
{
	int err;
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	err = st_hub_set_enable(info->client, info->index, true,
					false, ST_HUB_BATCH_DISABLED_ID);
	if (err < 0)
		return err;

	return 0;
}

static int st_hub_sign_motion_buffer_predisable(struct iio_dev *indio_dev)
{
	int err;
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	err = st_hub_set_enable(info->client, info->index, false,
					false, ST_HUB_BATCH_DISABLED_ID);
	if (err < 0)
		return err;

	return 0;
}

static const struct iio_buffer_setup_ops st_hub_buffer_setup_ops = {
	.preenable = &st_hub_buffer_preenable,
	.postenable = &st_hub_sign_motion_buffer_postenable,
	.predisable = &st_hub_sign_motion_buffer_predisable,
};

static int st_hub_sign_motion_probe(struct platform_device *pdev)
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

	indio_dev->channels = st_hub_sign_motion_ch;
	indio_dev->num_channels = ARRAY_SIZE(st_hub_sign_motion_ch);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &st_hub_sign_motion_info;
	indio_dev->name = pdev->name;
	indio_dev->modes = INDIO_DIRECT_MODE;

	adata = iio_priv(indio_dev);
	info = pdev->dev.platform_data;
	st_hub_get_common_data(info->client, info->index, &adata->cdata);

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
	callback.push_event = &st_hub_sign_motion_push_event;
	st_hub_register_callback(info->client, &callback, info->index);

	return 0;

st_hub_remove_trigger:
	st_hub_remove_trigger(adata);
st_hub_clear_buffer:
	iio_triggered_buffer_cleanup(indio_dev);
st_hub_deallocate_device:
	iio_device_free(indio_dev);
	return err;
}

static int st_hub_sign_motion_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct st_hub_sensor_data *adata = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	st_hub_remove_trigger(adata);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static struct platform_device_id st_hub_sign_motion_ids[] = {
	{ CONCATENATE_STRING(LIS331EB_DEV_NAME, "sign_motion") },
	{ CONCATENATE_STRING(LSM6DB0_DEV_NAME, "sign_motion") },
	{},
};
MODULE_DEVICE_TABLE(platform, st_hub_sign_motion_ids);

static struct platform_driver st_hub_sign_motion_platform_driver = {
	.id_table = st_hub_sign_motion_ids,
	.driver = {
		.name	= KBUILD_MODNAME,
		.owner	= THIS_MODULE,
	},
	.probe		= st_hub_sign_motion_probe,
	.remove		= st_hub_sign_motion_remove,
};
module_platform_driver(st_hub_sign_motion_platform_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub significant motions driver");
MODULE_LICENSE("GPL v2");
