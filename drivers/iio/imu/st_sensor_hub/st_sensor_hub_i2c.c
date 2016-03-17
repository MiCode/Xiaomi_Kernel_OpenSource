/*
 * STMicroelectronics st_sensor_hub i2c driver
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
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/iio/iio.h>
#include <linux/of.h>

#include <linux/platform_data/st_hub_pdata.h>

#include "st_sensor_hub.h"

static int st_hub_i2c_read(struct device *dev, size_t len, u8 *data)
{
	u8 dummy = 0x80;
	struct i2c_msg msg[2];
	struct i2c_client *client = to_i2c_client(dev);

	msg[0].addr = client->addr;
	msg[0].flags = client->flags;
	msg[0].len = 1;
	msg[0].buf = &dummy;

	msg[1].addr = client->addr;
	msg[1].flags = client->flags | I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = data;

	return i2c_transfer(client->adapter, msg, 2);
}

static int st_hub_i2c_read_no_write(struct device *dev, size_t len, u8 *data)
{
	struct i2c_msg msg;
	struct i2c_client *client = to_i2c_client(dev);

	msg.addr = client->addr;
	msg.flags = client->flags | I2C_M_RD;
	msg.len = len;
	msg.buf = data;

	return i2c_transfer(client->adapter, &msg, 1);
}

static int st_hub_i2c_write_with_dummy(struct device *dev, size_t len, u8 *data)
{
	u8 send[len + 1];
	struct i2c_msg msg;
	struct i2c_client *client = to_i2c_client(dev);

	send[0] = 0x00;
	memcpy(&send[1], data, len * sizeof(u8));

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len + 1;
	msg.buf = send;

	return i2c_transfer(client->adapter, &msg, 1);
}

static int st_hub_i2c_write(struct device *dev, size_t len, u8 *data)
{

	struct i2c_msg msg;
	struct i2c_client *client = to_i2c_client(dev);

	msg.addr = client->addr;
	msg.flags = client->flags;
	msg.len = len;
	msg.buf = data;

	return i2c_transfer(client->adapter, &msg, 1);
}

static const struct st_hub_transfer_function st_hub_tf_i2c = {
	.read = st_hub_i2c_read,
	.write = st_hub_i2c_write_with_dummy,
	.read_rl = st_hub_i2c_read_no_write,
	.write_rl = st_hub_i2c_write,
};

static char *st_sensor_hub_i2c_get_name(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	return client->name;
}

static int st_sensor_hub_i2c_probe(struct i2c_client *client,
						const struct i2c_device_id *id)
{
	int err;
	struct st_hub_data *hdata;

	hdata = kzalloc(sizeof(*hdata), GFP_KERNEL);
	if (!hdata)
		return -ENOMEM;

	hdata->irq = client->irq;
	hdata->dev = &client->dev;
	hdata->tf = &st_hub_tf_i2c;
	hdata->get_dev_name = &st_sensor_hub_i2c_get_name;
	i2c_set_clientdata(client, hdata);
	dev_info(hdata->dev, "st_sensor_hub_i2c_probe irq=%d\n", hdata->irq);
	err = st_sensor_hub_common_probe(hdata);
	if (err < 0)
		goto st_hub_free_hdata;

	return 0;

st_hub_free_hdata:
	kfree(hdata);
	return err;
}

static int st_sensor_hub_i2c_remove(struct i2c_client *client)
{
	struct st_hub_data *hdata = i2c_get_clientdata(client);

	st_sensor_hub_common_remove(hdata);

	kfree(hdata);
	return 0;
}

#ifdef CONFIG_PM
static int st_sensor_hub_i2c_suspend(struct device *dev)
{
	struct st_hub_data *hdata = i2c_get_clientdata(to_i2c_client(dev));

	return st_sensor_hub_common_suspend(hdata);
}

static int st_sensor_hub_i2c_resume(struct device *dev)
{
	struct st_hub_data *hdata = i2c_get_clientdata(to_i2c_client(dev));

	return st_sensor_hub_common_resume(hdata);
}

static const struct dev_pm_ops st_sensor_hub_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_sensor_hub_i2c_suspend, \
						st_sensor_hub_i2c_resume)
};

#define ST_SENSOR_HUB_PM_OPS		(&st_sensor_hub_pm_ops)
#else /* CONFIG_PM */
#define ST_SENSOR_HUB_PM_OPS		NULL
#endif /* CONFIG_PM */

static const struct i2c_device_id st_sensor_hub_id_table[] = {
	{ LIS331EB_DEV_NAME },
	{ LIS332EB_DEV_NAME },
	{ LSM6DB0_DEV_NAME },
	{ },
};
MODULE_DEVICE_TABLE(i2c, st_sensor_hub_id_table);

#ifdef CONFIG_OF
static const struct of_device_id st_sensor_hub_of_match[] = {
	{ .compatible = CONCATENATE_STRING("st,", LIS331EB_DEV_NAME) },
	{ .compatible = CONCATENATE_STRING("st,", LIS332EB_DEV_NAME) },
	{ .compatible = CONCATENATE_STRING("st,", LSM6DB0_DEV_NAME) },
	{ }
};
MODULE_DEVICE_TABLE(of, st_sensor_hub_of_match);
#endif /* CONFIG_OF */

static struct i2c_driver st_sensor_hub_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "st-sensor-hub-i2c",
		.pm = ST_SENSOR_HUB_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(st_sensor_hub_of_match),
#endif /* CONFIG_OF */
	},
	.probe = st_sensor_hub_i2c_probe,
	.remove = st_sensor_hub_i2c_remove,
	.id_table = st_sensor_hub_id_table,
};
module_i2c_driver(st_sensor_hub_driver);

MODULE_AUTHOR("Denis Ciocca <denis.ciocca@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sensor-hub i2c driver");
MODULE_LICENSE("GPL v2");
