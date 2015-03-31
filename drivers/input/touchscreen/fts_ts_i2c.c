/*
 * Copyright (C) 2013-2015 XiaoMi, Inc.
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
#include <linux/input/fts_ts.h>
#include <linux/of_gpio.h>
#include "fts_ts.h"

static int fts_i2c_recv(struct device *dev,
				void *wbuf, int wlen,
				void *rbuf, int rlen)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg xfer_msg[2];

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = wlen;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = wbuf;

	xfer_msg[1].addr = client->addr;
	xfer_msg[1].len = rlen;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = rbuf;

	return i2c_transfer(client->adapter, xfer_msg, 2);
}

static int fts_i2c_send(struct device *dev,
				void *buf, int len)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct i2c_msg xfer_msg[2];

	xfer_msg[0].addr = client->addr;
	xfer_msg[0].len = len;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = buf;

	return i2c_transfer(client->adapter, xfer_msg, 1);
}

static const struct fts_bus_ops fts_i2c_bops = {
	.bustype = BUS_I2C,
	.recv    = fts_i2c_recv,
	.send    = fts_i2c_send,
};

static int __devinit fts_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct fts_data *fts;

	pr_info("fts_i2c_probe detect\n");

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "incompatible i2c adapter.");
		return -ENODEV;
	}

	fts = fts_probe(&client->dev, &fts_i2c_bops, client->irq);
	if (IS_ERR(fts))
		return PTR_ERR(fts);

	i2c_set_clientdata(client, fts);
	return 0;
}

static int __devexit fts_i2c_remove(struct i2c_client *client)
{
	struct fts_data *fts = i2c_get_clientdata(client);
	fts_remove(fts);
	return 0;
}

static const struct i2c_device_id fts_i2c_id[] = {
	{"fts_i2c", 0},
	{/* end list */}
};
MODULE_DEVICE_TABLE(i2c, fts_i2c_id);

#ifdef CONFIG_OF
static struct of_device_id fts_match_table[] = {
	{ .compatible = "st,fts-ts",},
	{ },
};
#else
#define fts_match_table NULL
#endif

static struct i2c_driver fts_i2c_driver = {
	.probe         = fts_i2c_probe,
	.remove        = __devexit_p(fts_i2c_remove),
	.driver = {
		.name  = "fts_i2c",
		.owner = THIS_MODULE,
		.of_match_table = fts_match_table,
	},
	.id_table      = fts_i2c_id,
};

static int __init fts_i2c_init(void)
{
	printk(KERN_ERR "fts_i2c_init\n");
	return i2c_add_driver(&fts_i2c_driver);
}
late_initcall(fts_i2c_init);

static void __exit fts_i2c_exit(void)
{
	i2c_del_driver(&fts_i2c_driver);
}
module_exit(fts_i2c_exit);

MODULE_ALIAS("i2c:fts_i2c");
MODULE_AUTHOR("ZhangBo <zhangbo_a@xiaomi.com>");
MODULE_DESCRIPTION("i2c driver for st fts touchscreen");
MODULE_LICENSE("GPL");
