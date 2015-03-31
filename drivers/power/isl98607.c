/*
 *  isl98607.c - intersil power supply for display drivers
 *
 *  Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of_gpio.h>
#include <asm/bootinfo.h>

static int isl98607_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int isl98607_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int __devinit isl98607_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "%s: no support for i2c read/write"
				"byte data\n", __func__);
		return -EIO;
	}

	if ( get_hw_version_major() == 0x4
			&& get_hw_version_minor() < 0x1 ) {
		isl98607_write_reg(client, 0x06, 0xA);
		isl98607_write_reg(client, 0x08, 0xA);
		isl98607_write_reg(client, 0x09, 0xA);
	}

	printk(KERN_INFO "%s: %s registered 0x%x\n",
			__func__, id->name, isl98607_read_reg(client, 0x08));
	return 0;
}

static int __devexit isl98607_remove(struct i2c_client *client)
{
	return 0;
}

#ifdef CONFIG_PM
static int isl98607_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int isl98607_resume(struct i2c_client *client)
{
	return 0;
}
#else
#define isl98607_suspend		NULL
#define isl98607_resume		NULL
#endif

static const struct i2c_device_id isl98607_id[] = {
	{ "isl98607", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, isl98607_id);
#ifdef CONFIG_OF
static struct of_device_id isl98607_match_table[] = {
	{ .compatible = "intersil,isl98607",},
	{ },
};
#else
#define isl98607_match_table NULL
#endif

static struct i2c_driver isl98607_driver = {
	.driver	= {
		.name	= "isl98607",
		.of_match_table = isl98607_match_table,
	},
	.probe		= isl98607_probe,
	.remove		= __devexit_p(isl98607_remove),
	.suspend	= isl98607_suspend,
	.resume		= isl98607_resume,
	.id_table	= isl98607_id,
};

static int __init isl98607_init(void)
{
	return i2c_add_driver(&isl98607_driver);
}

static void __exit isl98607_exit(void)
{
	i2c_del_driver(&isl98607_driver);
}

module_init(isl98607_init);
module_exit(isl98607_exit);

MODULE_AUTHOR("Anyu LIU <liuanyu@xiaomi.com>");
MODULE_DESCRIPTION("ISL98607 driver");
MODULE_LICENSE("GPL");
