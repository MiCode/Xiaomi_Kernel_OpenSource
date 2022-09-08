// SPDX-License-Identifier: (GPL-2.0 OR BSD-3-Clause)
/*
 * cs35l45-i2c.c -- CS35L45 I2C driver
 *
 * Copyright 2019 Cirrus Logic, Inc.
 *
 * Author: James Schulman <james.schulman@cirrus.com>
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>

#include "wm_adsp.h"
#include "cs35l45.h"
#include <sound/cs35l45.h>

/*mtk_spk_type register*/
#include <mtk-sp-spk-amp.h>

static int cs35l45_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct cs35l45_private *cs35l45;
	struct device *dev = &client->dev;
	const struct i2c_adapter_quirks *quirks;
	int ret;
	dev_info(dev, "enter cs35l45_i2c_probe\n");

	cs35l45 = devm_kzalloc(dev, sizeof(struct cs35l45_private), GFP_KERNEL);
	if (cs35l45 == NULL)
		return -ENOMEM;

	i2c_set_clientdata(client, cs35l45);
	cs35l45->regmap = devm_regmap_init_i2c(client, &cs35l45_i2c_regmap);
	if (IS_ERR(cs35l45->regmap)) {
		ret = PTR_ERR(cs35l45->regmap);
		dev_info(dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	cs35l45->dev = dev;
	cs35l45->irq = client->irq;
	cs35l45->bus_type = CONTROL_BUS_I2C;
	cs35l45->i2c_addr = client->addr;

	quirks = client->adapter->quirks;
	if (quirks != NULL)
		cs35l45->max_quirks_read_nwords = (int) quirks->max_read_len / 4;

	ret = cs35l45_probe(cs35l45);
	if (ret < 0) {
		dev_info(dev, "Failed device probe: %d\n", ret);
		return ret;
	}

	ret = cs35l45_initialize(cs35l45);
	if (ret < 0) {
		dev_info(dev, "Failed device initialization: %d\n", ret);
		goto fail;

	}
	pr_debug("mtk_spk_set_type");
	mtk_spk_set_type(MTK_SPK_CIRRUS_CS35L45);
	pr_debug("mtk_spk_set_type done");

	return 0;

fail:
	cs35l45_remove(cs35l45);
	return ret;
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
		.pm = &cs35l45_pm_ops,
	},
	.id_table	= cs35l45_id_i2c,
	.probe		= cs35l45_i2c_probe,
	.remove		= cs35l45_i2c_remove,
};
module_i2c_driver(cs35l45_i2c_driver);

MODULE_DESCRIPTION("I2C CS35L45 driver");
MODULE_AUTHOR("James Schulman, Cirrus Logic Inc, <james.schulman@cirrus.com>");
MODULE_LICENSE("GPL");
