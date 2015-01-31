/*
 * Copyright (C) 2011 XiaoMi, Inc.
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
#include "ft5x06_ts.h"

static int ft5x06_i2c_recv(struct device *dev,
				void *buf, int len)
{
	struct i2c_client *client = to_i2c_client(dev);
	int count = i2c_master_recv(client, buf, len);
	return count < 0 ? count : 0;
}

static int ft5x06_i2c_send(struct device *dev,
				const void *buf, int len)
{
	struct i2c_client *client = to_i2c_client(dev);
	int count = i2c_master_send(client, buf, len);
	return count < 0 ? count : 0;
}

static int ft5x06_i2c_read(struct device *dev,
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

static int ft5x06_i2c_write(struct device *dev,
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

static const struct ft5x06_bus_ops ft5x06_i2c_bops = {
	.bustype = BUS_I2C,
	.recv    = ft5x06_i2c_recv,
	.send    = ft5x06_i2c_send,
	.read    = ft5x06_i2c_read,
	.write   = ft5x06_i2c_write,
};

static int __devinit ft5x06_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ft5x06_data *ft5x06;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "incompatible i2c adapter.");
		return -ENODEV;
	}

	ft5x06 = ft5x06_probe(&client->dev, &ft5x06_i2c_bops);
	if (IS_ERR(ft5x06))
		return PTR_ERR(ft5x06);

	i2c_set_clientdata(client, ft5x06);
	return 0;
}

static int __devexit ft5x06_i2c_remove(struct i2c_client *client)
{
	struct ft5x06_data *ft5x0x = i2c_get_clientdata(client);
	ft5x06_remove(ft5x0x);
	return 0;
}

static const struct i2c_device_id ft5x06_i2c_id[] = {
	{"ft5x06_i2c", 0},
	{/* end list */}
};
MODULE_DEVICE_TABLE(i2c, ft5x0x_i2c_id);

static struct i2c_driver ft5x06_i2c_driver = {
	.probe         = ft5x06_i2c_probe,
	.remove        = __devexit_p(ft5x06_i2c_remove),
	.driver = {
		.name  = "ft5x06_i2c",
		.owner = THIS_MODULE,
	},
	.id_table      = ft5x06_i2c_id,
};

static int __init ft5x06_i2c_init(void)
{
	printk(KERN_ERR "ft5x06_i2c_init\n");
	return i2c_add_driver(&ft5x06_i2c_driver);
}
module_init(ft5x06_i2c_init);

static void __exit ft5x06_i2c_exit(void)
{
	i2c_del_driver(&ft5x06_i2c_driver);
}
module_exit(ft5x06_i2c_exit);

MODULE_ALIAS("i2c:ft5x06_i2c");
MODULE_AUTHOR("ZhangBo <zhangbo_a@xiaomi.com>");
MODULE_DESCRIPTION("i2c driver for ft5x06 touchscreen");
MODULE_LICENSE("GPL");
