/*
 * cs35l41-i2c.c -- CS35l41 I2C driver
 *
 * Copyright 2017 Cirrus Logic, Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include "cs35l41.h"
#include <sound/cs35l41.h>

static struct regmap_config cs35l41_regmap_i2c = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.val_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = CS35L41_LASTREG,
	.reg_defaults = cs35l41_reg,
	.num_reg_defaults = ARRAY_SIZE(cs35l41_reg),
	.volatile_reg = cs35l41_volatile_reg,
	.readable_reg = cs35l41_readable_reg,
	.precious_reg = cs35l41_precious_reg,
	.cache_type = REGCACHE_RBTREE,
};

int cs35l41_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct cs35l41_private *cs35l41;
	struct device *dev = &client->dev;
	/*Audio Start*/
	struct device_node *np = client->dev.of_node;
	/*Audio End*/
	struct cs35l41_platform_data *pdata = dev_get_platdata(dev);
	const struct regmap_config *regmap_config = &cs35l41_regmap_i2c;
	int ret;

	printk(KERN_DEBUG "[CSPL] Enter function %s\n", __func__);

	cs35l41 = devm_kzalloc(dev, sizeof(struct cs35l41_private), GFP_KERNEL);

	if (cs35l41 == NULL)
		return -ENOMEM;

	mutex_init(&cs35l41->rate_lock);

	cs35l41->dev = dev;
	cs35l41->irq = client->irq;
	cs35l41->bus_spi = false;

	/*Audio Start*/
	if (np) {
		cs35l41->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
		if (cs35l41->irq_gpio < 0)
			pr_err("[CSPL] No irq gpio provided.\n");
	} else {
		cs35l41->irq_gpio = -1;
	}
	/*Audio End*/

	printk(KERN_DEBUG "[CSPL] irq_gpio[%d],addr[0x%x],iname[%s]\n",
				cs35l41->irq_gpio, client->addr, client->name);

	i2c_set_clientdata(client, cs35l41);
	cs35l41->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(cs35l41->regmap)) {
		ret = PTR_ERR(cs35l41->regmap);
		dev_err(cs35l41->dev, "Failed to allocate register map: %d\n",
			ret);
		return ret;
	}

	return cs35l41_probe(cs35l41, pdata);
}

int cs35l41_i2c_remove(struct i2c_client *client)
{
	struct cs35l41_private *cs35l41 = i2c_get_clientdata(client);

	regmap_write(cs35l41->regmap, CS35L41_IRQ1_MASK1, 0xFFFFFFFF);
	wm_adsp2_remove(&cs35l41->dsp);
	regulator_bulk_disable(cs35l41->num_supplies, cs35l41->supplies);
	snd_soc_unregister_codec(cs35l41->dev);
	return 0;
}

EXPORT_SYMBOL_GPL(cs35l41_i2c_probe);
EXPORT_SYMBOL_GPL(cs35l41_i2c_remove);
MODULE_LICENSE("GPL v2");
