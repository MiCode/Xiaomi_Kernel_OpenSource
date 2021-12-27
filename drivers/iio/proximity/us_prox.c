/*
 * Copyright (C) 2018 XiaoMi, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/iio/buffer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/consumer.h>
#include <linux/of_irq.h>
#include <asm/bootinfo.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>

#define US_PROX_IIO_NAME		"distance"

static struct us_prox_data *g_us_prox;

struct us_prox_data {
	struct platform_device	*pdev;
	/* common state */
	struct mutex		mutex;
	/* for proximity sensor */
	struct iio_dev		*prox_idev;
	bool			prox_enabled;
	int				raw_data;
};

struct us_prox_el_data {
	uint16_t data1;
	uint16_t data2;
	int64_t timestamp;
};

#define US_SENSORS_CHANNELS(device_type, mask, index, mod, \
					ch2, s, endian, rbits, sbits, addr) \
{ \
	.type = device_type, \
	.modified = mod, \
	.info_mask_separate = mask, \
	.scan_index = index, \
	.channel2 = ch2, \
	.address = addr, \
	.scan_type = { \
		.sign = s, \
		.realbits = rbits, \
		.shift = sbits - rbits, \
		.storagebits = sbits, \
		.endianness = endian, \
	}, \
}

static const struct iio_chan_spec us_proximity_channels[] = {
	US_SENSORS_CHANNELS(IIO_PROXIMITY,
		BIT(IIO_CHAN_INFO_RAW) | BIT(IIO_CHAN_INFO_SAMP_FREQ),
		0, 0, IIO_NO_MOD, 'u', IIO_LE, 16, 16, 0),
	IIO_CHAN_SOFT_TIMESTAMP(2)
};

static int us_buffer_postenable(struct iio_dev *indio_dev)
{
	int ret = 0;
	return ret;
}

static int us_buffer_predisable(struct iio_dev *indio_dev)
{
	int ret = 0;
	return ret;
}

static const struct iio_buffer_setup_ops us_buffer_setup_ops = {
	.postenable = us_buffer_postenable,
	.predisable = us_buffer_predisable,
};

static const struct iio_trigger_ops us_sensor_trigger_ops = {
	//.owner = THIS_MODULE,
};

int us_setup_trigger_sensor(struct iio_dev *indio_dev)
{
	struct iio_trigger *trigger;
	int ret;

	trigger = iio_trigger_alloc("%s-dev%d", indio_dev->name, indio_dev->id);
	if (!trigger)
		return -ENOMEM;

	trigger->dev.parent = indio_dev->dev.parent;
	trigger->ops = &us_sensor_trigger_ops;
	ret = iio_trigger_register(trigger);
	if (ret < 0)
		goto exit_free_trigger;

	indio_dev->trig = trigger;

	return 0;

exit_free_trigger:
	iio_trigger_free(trigger);
	return ret;
}

int us_afe_callback(int data)
{
	int ret;
	struct us_prox_el_data el_data;
	struct timespec ts;

	get_monotonic_boottime(&ts);
	el_data.timestamp = timespec_to_ns(&ts);
	pr_info("%s: data = %d\n", __func__, data);

	if (!data)
		el_data.data1 = 0;
	else
		el_data.data1 = 5;

	if (g_us_prox) {
		ret = iio_push_to_buffers(g_us_prox->prox_idev, (unsigned char *)&el_data);
		if (ret < 0)
			pr_err("%s: failed to push us prox data to buffer, err=%d\n", __func__, ret);
	}

	return 0;
}

EXPORT_SYMBOL(us_afe_callback);

static ssize_t us_show_dump_output(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct us_prox_data *data;
	unsigned long value;

	data = *((struct us_prox_data **)iio_priv(dev_get_drvdata(dev)));

	mutex_lock(&data->mutex);
	value = data->raw_data;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", value);
}

static ssize_t us_store_dump_output(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(dump_output, S_IWUSR | S_IRUGO,
		us_show_dump_output, us_store_dump_output);


static struct attribute *us_prox_attributes[] = {
	&dev_attr_dump_output.attr,
	NULL,
};

static struct attribute_group us_prox_attribute_group = {
	.attrs = us_prox_attributes,
};

static const struct iio_info us_proximity_info = {
	//.driver_module = THIS_MODULE,
	.attrs = &us_prox_attribute_group,
};

static int us_proximity_iio_setup(struct us_prox_data *data)
{
	struct iio_dev *idev;
	struct us_prox_data **priv_data;
	int ret = 0;

	idev = iio_device_alloc(sizeof(*priv_data));
	if (!idev) {
		pr_err("us prox IIO memory alloc fail\n");
		return -ENOMEM;
	}

	data->prox_idev = idev;

	idev->channels = us_proximity_channels;
	idev->num_channels = ARRAY_SIZE(us_proximity_channels);
	idev->dev.parent = &(data->pdev->dev);
	idev->info = &us_proximity_info;
	idev->name = US_PROX_IIO_NAME;
	idev->modes = INDIO_DIRECT_MODE;

	priv_data = iio_priv(idev);
	*priv_data = data;

	ret = iio_triggered_buffer_setup(idev, NULL, NULL,
					&us_buffer_setup_ops);
	if (ret < 0)
		goto free_iio_p;

	ret = us_setup_trigger_sensor(idev);
	if (ret < 0)
		goto free_buffer_p;

	ret = iio_device_register(idev);
	if (ret) {
		pr_err("Proximity IIO register fail\n");
		goto free_trigger_p;
	}
	return ret;

free_trigger_p:
	iio_trigger_unregister(idev->trig);
	iio_trigger_free(idev->trig);
free_buffer_p:
	iio_triggered_buffer_cleanup(idev);
free_iio_p:
	data->prox_idev = NULL;
	iio_device_free(idev);
	return ret;
}

static int us_proximity_teardown(struct us_prox_data *data)
{
	iio_device_unregister(data->prox_idev);
	iio_trigger_unregister(data->prox_idev->trig);
	iio_trigger_free(data->prox_idev->trig);
	iio_triggered_buffer_cleanup(data->prox_idev);
	iio_device_free(data->prox_idev);

	return 0;
}

static int us_prox_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct us_prox_data *us_prox;

	pr_info("%s: start\n", __func__);

	us_prox = kzalloc(sizeof(*us_prox), GFP_KERNEL);
	if (!us_prox)
		return -ENOMEM;

	us_prox->pdev = pdev;
	dev_set_drvdata(&pdev->dev, us_prox);

	g_us_prox = us_prox;

	mutex_init(&us_prox->mutex);
	ret = us_proximity_iio_setup(us_prox);
	if (ret < 0) {
		pr_err("%s: iio setup failed ret = %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s: end\n", __func__);
	return ret;
}

static int us_prox_remove(struct platform_device *pdev)
{
	struct us_prox_data *us_prox;

	us_prox = dev_get_drvdata(&pdev->dev);

	dev_set_drvdata(&pdev->dev, NULL);

	if (us_prox) {
		us_proximity_teardown(us_prox);
		kfree(us_prox);
	}

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "us_prox" },
	{}
};

static struct platform_driver us_prox_driver = {
	.probe		= us_prox_probe,
	.remove		= us_prox_remove,
	.driver		= {
		.name		= "us_prox",
		.of_match_table	= dt_match,
	},
};

static struct platform_device us_prox_dev = {
	.name = "us_prox",
	.dev = {
		.platform_data = NULL,
	},
};

static int __init us_prox_init(void)
{
	platform_device_register(&us_prox_dev);
	return platform_driver_register(&us_prox_driver);
}
module_init(us_prox_init);

static void __exit us_prox_exit(void)
{
	platform_driver_unregister(&us_prox_driver);
	platform_device_unregister(&us_prox_dev);
}
module_exit(us_prox_exit);

MODULE_AUTHOR("xiaomi");
MODULE_DESCRIPTION("Ultrasound IIO driver");
MODULE_LICENSE("GPL");
