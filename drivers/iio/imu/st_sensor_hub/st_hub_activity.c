/*
 * STMicroelectronics st_sensor_hub activity driver
 *
 * Copyright 2014 STMicroelectronics Inc.
 * Copyright (C) 2015 XiaoMi, Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <asm/unaligned.h>

#include "st_sensor_hub.h"

static const struct iio_chan_spec st_hub_activity_ch[] = {
	ST_HUB_DEVICE_CHANNEL(IIO_ACTIVITY, 0, 0, IIO_NO_MOD, IIO_LE, 8, 16,
					BIT(IIO_CHAN_INFO_RAW), 0, 0, 'u'),
	IIO_CHAN_SOFT_TIMESTAMP(1)
};

static const struct st_hub_activity_list {
	u8 id;
	char *name;
} st_hub_activity_list[] = {
	{
		.id = ST_ACTIVITY_IN_VEHICLE_INDEX,
		.name = "in-vehicle",
	},
	{
		.id = ST_ACTIVITY_ON_BICYCLE_INDEX,
		.name = "on-bicycle",
	},
	{
		.id = ST_ACTIVITY_STILL_INDEX,
		.name = "still",
	},
	{
		.id = ST_ACTIVITY_STAIRS_INDEX,
		.name = "stairs",
	},
	{
		.id = ST_ACTIVITY_RUNNING_INDEX,
		.name = "running",
	},
	{
		.id = ST_ACTIVITY_WALKING_INDEX,
		.name = "walking",
	},
	{
		.id = ST_ACTIVITY_FAST_WALKING_INDEX,
		.name = "fast-walking",
	},
};

static ST_HUB_BATCH_MAX_EVENT_COUNT();
static ST_HUB_BATCH_BUFFER_LENGTH();
static ST_HUB_BATCH_TIMEOUT();
static ST_HUB_BATCH_AVAIL();
static ST_HUB_BATCH();

static void st_hub_activity_push_data(struct platform_device *pdev,
						u8 *data, int64_t timestamp)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct st_hub_sensor_data *adata = iio_priv(indio_dev);

	adata->buffer[0] = 0;
	adata->buffer[1] = data[0];

	iio_push_to_buffers_with_timestamp(indio_dev, adata->buffer, timestamp);
}

static int st_hub_read_axis_data(struct iio_dev *indio_dev,
						unsigned int index, int *data)
{
	int err;
	u8 *outdata;
	struct st_hub_sensor_data *adata = iio_priv(indio_dev);
	struct st_hub_pdata_info *info = indio_dev->dev.parent->platform_data;

	outdata = kmalloc(adata->cdata->payload_byte, GFP_KERNEL);
	if (!outdata)
		return -EINVAL;

	err = st_hub_set_enable(info->hdata, info->index, true, true, 0, true);
	if (err < 0)
		goto st_hub_read_axis_free_memory;

	err = st_hub_read_axis_data_asincronous(info->hdata, info->index,
					outdata, adata->cdata->payload_byte);
	if (err < 0)
		goto st_hub_read_axis_free_memory;

	err = st_hub_set_enable(info->hdata, info->index, false, true, 0, true);
	if (err < 0)
		goto st_hub_read_axis_free_memory;

	*data = outdata[0];

st_hub_read_axis_free_memory:
	kfree(outdata);
	return err;
}

static int st_hub_activity_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *ch, int *val, int *val2, long mask)
{
	int err;

	*val = 0;
	*val2 = 0;
	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED)
			return -EBUSY;

		err = st_hub_read_axis_data(indio_dev, ch->scan_index, val);
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

static int st_hub_get_activity_list(struct st_hub_data *hdata, u8 command,
								u16 *mask)
{
	int err;
	u8 data[2];

	err = st_hub_send_and_receive(hdata, &command, 1, data, ARRAY_SIZE(data),
					true);
	if (err < 0) {
		dev_err(hdata->dev, "failed to read activity list.\n");
		return err;
	}

	(*mask) = (u16)get_unaligned_le16(data);

	return 0;
}

static int st_hub_set_activity_enable(struct st_hub_data *hdata,
					const char *buf, size_t size,
					bool enable)
{
	int err, idx;
	u16 id_activity;
	u16 enabled_activity;
	u8 command[3], *data;

	err = st_hub_get_activity_list(hdata,
					ST_HUB_GLOBAL_ACTIVITY_ENABLED_LIST,
					&enabled_activity);
	if (err < 0)
		return err;

	for (idx = 0; idx < ARRAY_SIZE(st_hub_activity_list); idx++) {
		if (!strncmp(buf, st_hub_activity_list[idx].name, size - 1)) {
			id_activity = st_hub_activity_list[idx].id;

			break;
		}
	}
	if (idx == ARRAY_SIZE(st_hub_activity_list))
		return -EINVAL;

	if ((enabled_activity >> id_activity) == enable)
		return 0;

	if (enable)
		enabled_activity |= (1 << id_activity);
	else
		enabled_activity &= ~(1 << id_activity);

	data = (u8 *)&enabled_activity;
	command[0] = ST_HUB_GLOBAL_ACTIVITY_ENABLE;
	command[1] = *data;
	command[2] = *(++data);

	err = st_hub_send(hdata, command, ARRAY_SIZE(command), true);
	if (err < 0)
		return err;

	return 0;
}

static ssize_t st_hub_sysfs_set_enabled_activity(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;

	pdev = container_of(dev->parent, struct platform_device, dev);
	info = pdev->dev.platform_data;

	err = st_hub_set_activity_enable(info->hdata, buf, size, 1);

	return ((err < 0) ? err : size);
}

static ssize_t st_hub_sysfs_get_enabled_activity(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err, idx, len;
	u16 enabled_activity;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;

	pdev = container_of(dev->parent, struct platform_device, dev);
	info = pdev->dev.platform_data;

	err = st_hub_get_activity_list(info->hdata,
					ST_HUB_GLOBAL_ACTIVITY_ENABLED_LIST,
					&enabled_activity);
	if (err < 0)
		return err;

	len = 0;
	for (idx = 0; idx < ARRAY_SIZE(st_hub_activity_list); idx++)
		if (enabled_activity & (1 << st_hub_activity_list[idx].id))
			len += sprintf(&buf[len], "%s ",
					st_hub_activity_list[idx].name);
	if (len)
		sprintf(&buf[len++], "\n");

	return len;
}

static ssize_t st_hub_sysfs_set_disable_activity(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int err;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;

	pdev = container_of(dev->parent, struct platform_device, dev);
	info = pdev->dev.platform_data;

	err = st_hub_set_activity_enable(info->hdata, buf, size, 0);

	return ((err < 0) ? err : size);
}

static ssize_t st_hub_sysfs_get_available_activity(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err, idx, len;
	u16 available_activity;
	struct platform_device *pdev;
	struct st_hub_pdata_info *info;

	pdev = container_of(dev->parent, struct platform_device, dev);
	info = pdev->dev.platform_data;

	err = st_hub_get_activity_list(info->hdata,
					ST_HUB_GLOBAL_ACTIVITY_AVAILABLE_LIST,
					&available_activity);
	if (err < 0)
		return err;

	len = 0;
	for (idx = 0; idx < ARRAY_SIZE(st_hub_activity_list); idx++)
		if (available_activity & (1 << st_hub_activity_list[idx].id))
			len += sprintf(&buf[len], "%s ",
					st_hub_activity_list[idx].name);

	if (len)
		sprintf(&buf[len++], "\n");

	return len;
}

static IIO_DEVICE_ATTR(enable_activity, S_IWUSR | S_IRUGO,
				st_hub_sysfs_get_enabled_activity,
				st_hub_sysfs_set_enabled_activity, 0);

static IIO_DEVICE_ATTR(disable_activity, S_IWUSR, NULL,
				st_hub_sysfs_set_disable_activity, 0);

static IIO_DEVICE_ATTR(available_activity, S_IRUGO,
				st_hub_sysfs_get_available_activity, NULL, 0);

static struct attribute *st_hub_activity_attributes[] = {
	&iio_dev_attr_enable_activity.dev_attr.attr,
	&iio_dev_attr_disable_activity.dev_attr.attr,
	&iio_dev_attr_available_activity.dev_attr.attr,
	&iio_dev_attr_batch_mode_max_event_count.dev_attr.attr,
	&iio_dev_attr_batch_mode_buffer_length.dev_attr.attr,
	&iio_dev_attr_batch_mode_timeout.dev_attr.attr,
	&iio_dev_attr_batch_mode_available.dev_attr.attr,
	&iio_dev_attr_batch_mode.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_hub_activity_attribute_group = {
	.attrs = st_hub_activity_attributes,
};

static const struct iio_info st_hub_activity_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_hub_activity_attribute_group,
	.read_raw = &st_hub_activity_read_raw,
};

static const struct iio_buffer_setup_ops st_hub_buffer_setup_ops = {
	.preenable = &st_hub_buffer_preenable,
	.postenable = &st_hub_buffer_postenable,
	.predisable = &st_hub_buffer_predisable,
};

static int st_hub_activity_probe(struct platform_device *pdev)
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

	indio_dev->channels = st_hub_activity_ch;
	indio_dev->num_channels = ARRAY_SIZE(st_hub_activity_ch);
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &st_hub_activity_info;
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
	callback.push_data = &st_hub_activity_push_data;
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

static int st_hub_activity_remove(struct platform_device *pdev)
{
	struct iio_dev *indio_dev = platform_get_drvdata(pdev);
	struct st_hub_sensor_data *adata = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	st_hub_remove_trigger(adata);
	iio_triggered_buffer_cleanup(indio_dev);
	iio_device_free(indio_dev);

	return 0;
}

static struct platform_device_id st_hub_activity_ids[] = {
	{ CONCATENATE_STRING(LIS331EB_DEV_NAME, "activity") },
	{ CONCATENATE_STRING(LSM6DB0_DEV_NAME, "activity") },
	{},
};
MODULE_DEVICE_TABLE(platform, st_hub_activity_ids);

static struct platform_driver st_hub_activity_platform_driver = {
	.id_table = st_hub_activity_ids,
	.driver = {
		.name	= KBUILD_MODNAME,
		.owner	= THIS_MODULE,
	},
	.probe		= st_hub_activity_probe,
	.remove		= st_hub_activity_remove,
};
module_platform_driver(st_hub_activity_platform_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub activity driver");
MODULE_LICENSE("GPL v2");
