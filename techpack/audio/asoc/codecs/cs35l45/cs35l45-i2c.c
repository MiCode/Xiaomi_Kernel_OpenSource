// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * cs35l45-i2c.c -- CS35L45 I2C driver
 *
 * Copyright 2019 Cirrus Logic, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: James Schulman <james.schulman@cirrus.com>
 *
 */
#define DEBUG
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

#include "wm_adsp.h"
#include "cs35l45.h"
#include "cs35l45_user.h"

static struct regmap_config cs35l45_regmap = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = CS35L45_REGSTRIDE,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = CS35L45_LASTREG,
	.reg_defaults = cs35l45_reg,
	.num_reg_defaults = ARRAY_SIZE(cs35l45_reg),
	.volatile_reg = cs35l45_volatile_reg,
	.readable_reg = cs35l45_readable_reg,
	.cache_type = REGCACHE_RBTREE,
};

static int cs35l45_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct cs35l45_private *cs35l45;
	struct device *dev = &client->dev;
	int ret;

	cs35l45 = devm_kzalloc(dev, sizeof(struct cs35l45_private), GFP_KERNEL);
	if (cs35l45 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(client, cs35l45);
	cs35l45->regmap = devm_regmap_init_i2c(client, &cs35l45_regmap);
	if (IS_ERR(cs35l45->regmap)) {
		ret = PTR_ERR(cs35l45->regmap);
		dev_err(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	cs35l45->dev = dev;
	cs35l45->irq = client->irq;
	cs35l45->wksrc = CS35L45_WKSRC_I2C;
	cs35l45->i2c_addr = client->addr;

	ret = cs35l45_probe(cs35l45);
	if (ret < 0) {
		dev_err(dev, "Failed device probe: %d\n", ret);
		return ret;
	}

	usleep_range(2000, 2100);

	ret = cs35l45_initialize(cs35l45);
	if (ret < 0) {
		dev_err(dev, "Failed device initialization: %d\n", ret);
		return ret;
	}

	return 0;
}

static int cs35l45_i2c_remove(struct i2c_client *client)
{
	struct cs35l45_private *cs35l45 = i2c_get_clientdata(client);

	return cs35l45_remove(cs35l45);
}

static const struct of_device_id cs35l45_of_match[] = {
	{.compatible = "cirrus,cs35l45"},
	{},
};
MODULE_DEVICE_TABLE(of, cs35l45_of_match);

static const struct i2c_device_id cs35l45_id_i2c[] = {
	{"cs35l45", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, cs35l45_id_i2c);

static struct i2c_driver cs35l45_i2c_driver = {
	.driver = {
		.name		= "cs35l45",
		.of_match_table = cs35l45_of_match,
	},
	.id_table	= cs35l45_id_i2c,
	.probe		= cs35l45_i2c_probe,
	.remove		= cs35l45_i2c_remove,
};
module_i2c_driver(cs35l45_i2c_driver);

MODULE_DESCRIPTION("I2C CS35L45 driver");
MODULE_AUTHOR("James Schulman, Cirrus Logic Inc, <james.schulman@cirrus.com>");
MODULE_LICENSE("GPL");
