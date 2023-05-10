// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/acpi.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include <linux/init.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/cpumask.h>
#include <linux/iio/consumer.h>


struct ipd_data {
	struct i2c_client	*client;
	struct iio_channel      *adc;
	struct task_struct *distance_thread;
	int enable;
	int resistance;
	int ipd_distance;
	int min_res;
	int max_res;
	int min_ipd;
	int max_ipd;
	int sleep_time;
	int enable_print;
};

/*
 * Control, Status and Reset registers.
 */
#define IPD_REG_CNTL1               0x20
#define IPD_REG_CNTL2               0x21
#define IPD_REG_THX                 0x22
#define IPD_REG_THY                 0x23
#define IPD_REG_THZ                 0x24
#define IPD_REG_THV                 0x25
#define IPD_REG_SRST                0x30

int ipd_thread_function(void *ipd_)
{
	struct ipd_data *ipd = ipd_;
	int ret = 0, val;

	while (!kthread_should_stop()) {
		ret = iio_read_channel_processed(ipd->adc, &val);
		if (ipd->enable && ret && (abs(ipd->resistance - val) > 50)) {
			ipd->resistance = val;
			if (ipd->resistance < ipd->min_res)
				ipd->min_res =  ipd->resistance;
			if (ipd->resistance > ipd->max_res)
				ipd->max_res = ipd->resistance;

			if (ipd->max_res > ipd->min_res) {
				ipd->ipd_distance = (int)(ipd->min_ipd +
				(((ipd->max_ipd - ipd->min_ipd) * (ipd->resistance - ipd->min_res))
							/ (ipd->max_res - ipd->min_res)));
			} else {
				pr_debug("Invalid MIN/MAX IPD resistance values\n");
			}

		}
		if (ipd->enable_print) {
			pr_info("In ipd Thread Function ret=%d && resistance=%d && distance=%d\n",
				ret, ipd->resistance, ipd->ipd_distance);
		}

		msleep(ipd->sleep_time);
	}
	return 0;
}


static ssize_t enable_thread_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->enable);
}

static ssize_t enable_thread_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->enable);

	if (ipd->enable) {
		if (ipd->distance_thread) {
			pr_debug("ipd resuming kthread\n");
			wake_up_process(ipd->distance_thread);
		} else {
			pr_debug("ipd creating kthread\n");
			ipd->distance_thread = kthread_create(
					ipd_thread_function, ipd,
					 "IPD Thread");
			if (ipd->distance_thread)
				wake_up_process(ipd->distance_thread);
			else
				pr_err("%s ERROR creating IPD Thread\n", __func__);
		}
	} else {
		if (ipd->distance_thread) {
			kthread_stop(ipd->distance_thread);
			ipd->distance_thread = NULL;
		}
	}

	return count;
}

static ssize_t ipd_distance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->ipd_distance);
}

static ssize_t ipd_distance_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	pr_info("cannot store ipd_distance\n");
	return count;
}

static ssize_t resistance_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->resistance);
}

static ssize_t resistance_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	pr_info("cannot store resistance\n");
	return count;
}

static ssize_t min_res_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->min_res);
}

static ssize_t min_res_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->min_res);
	return count;
}

static ssize_t max_res_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->max_res);
}

static ssize_t max_res_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->max_res);
	return count;
}

static ssize_t min_ipd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->min_ipd);
}

static ssize_t min_ipd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->min_ipd);
	return count;
}

static ssize_t max_ipd_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->max_ipd);
}

static ssize_t max_ipd_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->max_ipd);
	return count;
}

static ssize_t sleep_time_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->sleep_time);
}

static ssize_t sleep_time_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->sleep_time);
	return count;
}

static ssize_t enable_print_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", ipd->enable_print);
}

static ssize_t enable_print_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);
	int ret;

	ret = sscanf(buf, "%d\n", &ipd->enable_print);
	return count;
}

static DEVICE_ATTR_RW(enable_thread);
static DEVICE_ATTR_RW(ipd_distance);
static DEVICE_ATTR_RW(resistance);
static DEVICE_ATTR_RW(min_ipd);
static DEVICE_ATTR_RW(max_ipd);
static DEVICE_ATTR_RW(min_res);
static DEVICE_ATTR_RW(max_res);
static DEVICE_ATTR_RW(sleep_time);
static DEVICE_ATTR_RW(enable_print);

static struct attribute *ipd_i2c_sysfs_attrs[] = {
	&dev_attr_enable_thread.attr,
	&dev_attr_ipd_distance.attr,
	&dev_attr_resistance.attr,
	&dev_attr_max_res.attr,
	&dev_attr_min_res.attr,
	&dev_attr_max_ipd.attr,
	&dev_attr_min_ipd.attr,
	&dev_attr_sleep_time.attr,
	&dev_attr_enable_print.attr,
	NULL,
};

static struct attribute_group ipd_i2c_attribute_group = {
	.attrs = ipd_i2c_sysfs_attrs,
};

static s32 ipd_i2c_write(struct i2c_client *client,
				u8 address, int valueNum, u16 value1, u16 value2)
{
	u8  tx[5];
	s32 ret;
	int n = 0;

	tx[n++] = address;
	tx[n++] = (u8)((0xFF00 & value1) >> 8);
	tx[n++] = (u8)(0xFF & value1);

	pr_debug("[ipd] %s %02XH,%02XH,%02XH\n", __func__, (int)tx[0], (int)tx[1], (int)tx[2]);
	if (valueNum == 2) {
		tx[n++] = (u8)((0xFF00 & value2) >> 8);
		tx[n++] = (u8)(0xFF & value2);
	}

	ret = i2c_master_send(client, tx, n);
	if (ret != n)
		pr_err("%s: I2C write error, ret %d, wlen %d\n", __func__, ret, n);

	pr_debug("%s return %d\n", __func__, ret);

	return ret;
}

static int ipd_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ipd_data *ipd;
	int err;
	const char *name = NULL;

	ipd = devm_kzalloc(&client->dev, sizeof(*ipd), GFP_KERNEL);
	if (!ipd)
		return -ENOMEM;

	i2c_set_clientdata(client, ipd);
	dev_set_drvdata(&client->dev, ipd);

	ipd->client = client;

	ipd->adc = devm_iio_channel_get(&(ipd->client->dev), "ipd");
	if (IS_ERR(ipd->adc)) {
		dev_err(&client->dev, "Not able to fetch IPD IIO channel\n");
		return PTR_ERR(ipd->adc);
	}

	if (id)
		name = id->name;

	err = ipd_i2c_write(client, IPD_REG_CNTL1, 1, 28, 0);

	err = i2c_smbus_write_byte_data(client, IPD_REG_CNTL2, 0x28);
	if (err < 0)
		pr_err("%s: IPD error, ret= %d\n", __func__, err);

	err = ipd_i2c_write(client, IPD_REG_THX, 2, 1500, 500);
	err = ipd_i2c_write(client, IPD_REG_THY, 2, 6000, 5999);
	err = ipd_i2c_write(client, IPD_REG_THZ, 2, 13000, 12000);
	err = ipd_i2c_write(client, IPD_REG_THV, 2, 13000, 12000);

	err = devm_device_add_group(&client->dev, &ipd_i2c_attribute_group);
	if (err < 0) {
		dev_err(&client->dev, "couldn't register sysfs group\n");
		return err;
	}

	ipd->min_res = 950;
	ipd->max_res = 8596;
	ipd->min_ipd = 56;
	ipd->max_ipd = 70;
	ipd->sleep_time = 1000;

	pr_info("[ipd] %s(IPD_device_register=%d)\n", __func__, err);

	return err;
}

static int ipd_remove(struct i2c_client *client)
{
	struct ipd_data *ipd = i2c_get_clientdata(client);

	if (ipd->distance_thread) {
		kthread_stop(ipd->distance_thread);
		ipd->distance_thread = NULL;
	}

	i2c_unregister_device(client);

	return 0;
}

static int ipd_i2c_suspend(struct device *dev)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (ipd->distance_thread) {
		kthread_stop(ipd->distance_thread);
		ipd->distance_thread = NULL;
	}

	return 0;
}

static int ipd_i2c_resume(struct device *dev)
{
	struct ipd_data *ipd = dev_get_drvdata(dev);

	if (ipd->enable) {
		ipd->distance_thread = kthread_create(ipd_thread_function, ipd, "IPD Thread");
		if (ipd->distance_thread)
			wake_up_process(ipd->distance_thread);
		else
			pr_err("%s ERROR creating IPD Thread\n", __func__);
	}

	return 0;
}

static const struct dev_pm_ops ipd_i2c_pops = {
	.suspend	= ipd_i2c_suspend,
	.resume		= ipd_i2c_resume,
};

static const struct i2c_device_id ipd_id[] = {
	{ "ipd", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ipd_id);

static const struct of_device_id ipd_of_match[] = {
	{ .compatible = "qcom,ipd"},
	{}
};
MODULE_DEVICE_TABLE(of, ipd_of_match);

static struct i2c_driver ipd_driver = {
	.driver = {
		.name	= "ipd",
		.pm = &ipd_i2c_pops,
		.of_match_table = of_match_ptr(ipd_of_match),
	},
	.probe		= ipd_probe,
	.remove		= ipd_remove,
	.id_table	= ipd_id,
};
module_i2c_driver(ipd_driver);

MODULE_DESCRIPTION("Inter Pupillary Distance(IPD) Driver");
MODULE_LICENSE("GPL v2");
