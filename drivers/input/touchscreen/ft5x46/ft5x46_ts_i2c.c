/*
 * Copyright (C) 2011 XiaoMi, Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input/ft5x46_ts.h>
#include "focaltech_test.h"
struct i2c_client *fts_i2c_client;
static int ft5x46_i2c_recv(struct device *dev,
				void *buf, int len)
{
	struct i2c_client *client = to_i2c_client(dev);
	int count = i2c_master_recv(client, buf, len);
	return count < 0 ? count : 0;
}

static int ft5x46_i2c_send(struct device *dev,
				const void *buf, int len)
{
	struct i2c_client *client = to_i2c_client(dev);
	int count = i2c_master_send(client, buf, len);
	return count < 0 ? count : 0;
}

int fts_i2c_read(struct i2c_client *client, char *writebuf,
		int writelen, char *readbuf, int readlen)
{
	int ret;

	if (readlen > 0)	{
		if (writelen > 0) {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = 0,
					 .len = writelen,
					 .buf = writebuf,
				 },
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret < 0)
				dev_err(&client->dev, "%s:i2c read error.\n",
						__func__);
		} else {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0)
				dev_err(&client->dev, "%s:i2c read error.\n",
						__func__);
		}
	}
	return ret;
}

int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};

	if (writelen > 0) {
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c write error.\n",
					__func__);
	}
	return ret;
}
static int ft5x46_i2c_read(struct device *dev,
				u8 addr, void *buf, u8 len)
{
	struct i2c_client *client = to_i2c_client(dev);
	int i, count = 0;

	for (i = 0; i < len; i += count) {
		count = i2c_smbus_read_i2c_block_data(
				client, addr + i, len - i, buf + i);
		if (count < 0)
			break;
	}

	return count < 0 ? count : 0;
}

static int ft5x46_i2c_write(struct device *dev,
				u8 addr, const void *buf, u8 len)
{
	struct i2c_client *client = to_i2c_client(dev);
	int i, error = 0;

	for (i = 0; i < len; i += I2C_SMBUS_BLOCK_MAX) {
		/* transfer at most I2C_SMBUS_BLOCK_MAX one time */
		error = i2c_smbus_write_i2c_block_data(
				client, addr + i, len - i, buf + i);
		if (error)
			break;
	}

	return error;
}

static const struct ft5x46_bus_ops ft5x46_i2c_bops = {
	.bustype = BUS_I2C,
	.recv    = ft5x46_i2c_recv,
	.send    = ft5x46_i2c_send,
	.read    = ft5x46_i2c_read,
	.write   = ft5x46_i2c_write,
};

static int ft5x46_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ft5x46_data *ft5x46;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "incompatible i2c adapter.");
		return -ENODEV;
	}

	ft5x46 = ft5x46_probe(&client->dev, &ft5x46_i2c_bops);
	if (IS_ERR(ft5x46))
		return PTR_ERR(ft5x46);


	i2c_set_clientdata(client, ft5x46);
	fts_i2c_client = client;
	fts_test_module_init(client);
	device_init_wakeup(&client->dev, 1);

	return 0;
}

static int ft5x46_i2c_remove(struct i2c_client *client)
{
	struct ft5x46_data *ft5x0x = i2c_get_clientdata(client);
	fts_test_module_exit(client);
	ft5x46_remove(ft5x0x);
	return 0;
}

static const struct i2c_device_id ft5x46_i2c_id[] = {
	{"ft5x46_i2c", 0},
	{/* end list */}
};
MODULE_DEVICE_TABLE(i2c, ft5x0x_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id ft5x46_match_table[] = {
	{ .compatible = "ft,ft5x46_i2c", },
	{ },
};
#else
#define ft5x46_match_table NULL
#endif

#ifdef CONFIG_PM
static int ft5x46_i2c_suspend(struct device *dev)
{
	int ret = 0;

	if (device_may_wakeup(dev))
		ret = ft5x46_pm_suspend(dev);

	return ret;
}

static int ft5x46_i2c_resume(struct device *dev)
{
	int ret = 0;

	if (device_may_wakeup(dev))
		ret = ft5x46_pm_resume(dev);

	return ret;
}

static const struct dev_pm_ops ft5x46_i2c_pm_ops = {
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend        = ft5x46_i2c_suspend,
	.resume         = ft5x46_i2c_resume,
#endif
};
#endif

static struct i2c_driver ft5x46_i2c_driver = {
	.driver = {
		.name  = "ft5x46_i2c",
		.owner = THIS_MODULE,
		.of_match_table = ft5x46_match_table,
#ifdef CONFIG_PM
		.pm    = &ft5x46_i2c_pm_ops,
#endif
	},
	.probe         = ft5x46_i2c_probe,
	.remove        = ft5x46_i2c_remove,
	.id_table      = ft5x46_i2c_id,
};

static int __init ft5x46_i2c_init(void)
{
	return i2c_add_driver(&ft5x46_i2c_driver);
}
late_initcall(ft5x46_i2c_init);

static void __exit ft5x46_i2c_exit(void)
{
	i2c_del_driver(&ft5x46_i2c_driver);
}
module_exit(ft5x46_i2c_exit);

MODULE_ALIAS("i2c:ft5x46_i2c");
MODULE_AUTHOR("Tao Jun <taojun@xiaomi.com>");
MODULE_DESCRIPTION("i2c driver for ft5x46 touchscreen");
MODULE_LICENSE("GPL");
