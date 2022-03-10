// SPDX-License-Identifier: GPL-2.0

/*
 * cs35l43-i2c.c -- CS35l41 I2C driver
 *
 * Copyright 2020 Cirrus Logic, Inc.
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/gpio.h>

#include "wm_adsp.h"
#include "cs35l43.h"
#include <sound/cs35l43.h>

static struct regmap_config cs35l43_regmap_i2c = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = CS35L43_DSP1_PMEM_5114,
	.num_reg_defaults = 0,
	.volatile_reg = cs35l43_volatile_reg,
	.readable_reg = cs35l43_readable_reg,
	.precious_reg = cs35l43_precious_reg,
	.cache_type = REGCACHE_RBTREE,
};

static const struct i2c_device_id cs35l43_id_i2c[] = {
	{"cs35l43", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cs35l43_id_i2c);

static int cs35l43_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct cs35l43_private *cs35l43;
	struct device *dev = &client->dev;
	struct cs35l43_platform_data *pdata = dev_get_platdata(dev);
	const struct regmap_config *regmap_config = &cs35l43_regmap_i2c;
	int ret;
	pr_info("cs35l43 probe start");
	cs35l43 = devm_kzalloc(dev, sizeof(struct cs35l43_private), GFP_KERNEL);
	pr_info("cs35l43 probe start 68");
	if (cs35l43 == NULL){
		pr_info("cs35l43 probe start 70");
		return -ENOMEM;
	}
	cs35l43->dev = dev;
	cs35l43->irq = client->irq;
	pr_info("cs35l43 probe start 75");
	i2c_set_clientdata(client, cs35l43);
	cs35l43->regmap = devm_regmap_init_i2c(client, regmap_config);
	pr_info("cs35l43 probe start 78");
	if (IS_ERR(cs35l43->regmap)) {
		ret = PTR_ERR(cs35l43->regmap);
		dev_info(cs35l43->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	return cs35l43_probe(cs35l43, pdata);
}

static int cs35l43_i2c_remove(struct i2c_client *client)
{
	struct cs35l43_private *cs35l43 = i2c_get_clientdata(client);

	return cs35l43_remove(cs35l43);
}

static const struct of_device_id cs35l43_of_match[] = {
	{.compatible = "cirrus,cs35l43"},
	{},
};
MODULE_DEVICE_TABLE(of, cs35l43_of_match);

static struct i2c_driver cs35l43_i2c_driver = {
	.driver = {
		.name		= "cs35l43",
		.of_match_table = cs35l43_of_match,
	},
	.id_table	= cs35l43_id_i2c,
	.probe		= cs35l43_i2c_probe,
	.remove		= cs35l43_i2c_remove,
};

module_i2c_driver(cs35l43_i2c_driver);

MODULE_DESCRIPTION("I2C CS35L43 driver");
MODULE_AUTHOR("David Rhodes, Cirrus Logic Inc, <david.rhodes@cirrus.com>");
MODULE_LICENSE("GPL");
