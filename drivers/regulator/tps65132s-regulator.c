/*
 * tps65132s-regulator.c  --  regulator driver for TPS65132S
 *
 * Copyright (C) 2014-2015 Xiaomi Corporation
 *
 * Author: Xiang Xiao <xiaoxiang@xiaomi.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>

#define TPS65132S_VPOS			0x00
#define TPS65132S_VNEG			0x01
#define TPS65132S_DLYX			0x02
#define TPS65132S_FREQ			0x03

#define TPS65132S_APPS_SMARTPHONE	0x00
#define TPS65132S_APPS_TABLET		0x40
#define TPS65132S_APPS_MASK		0x40

struct tps65132s_reg {
	struct regulator_dev *rdev;
	struct regulator_desc *desc;
	struct device_node *of_node;
	struct regulator_init_data *init_data;
	bool is_enabled;
};

struct tps65132s_priv {
	struct regmap *regmap;
	struct tps65132s_reg regs[2];
	enum of_gpio_flags sync_flags;
	int sync_gpio;
	enum of_gpio_flags en_flags;
	int en_gpio;
	int en_ref;
};

static int tps65132s_request_gpio(int gpio, enum of_gpio_flags flags, bool val, const char *label)
{
	if (flags == OF_GPIO_ACTIVE_LOW) {
		if (val)
			return gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, label);
		else
			return gpio_request_one(gpio, GPIOF_OUT_INIT_HIGH, label);
	} else {
		if (val)
			return gpio_request_one(gpio, GPIOF_OUT_INIT_HIGH, label);
		else
			return gpio_request_one(gpio, GPIOF_OUT_INIT_LOW, label);
	}
}

static void tps65132s_set_gpio(int gpio, enum of_gpio_flags flags, bool val)
{
	if (flags == OF_GPIO_ACTIVE_LOW)
		gpio_set_value_cansleep(gpio, !val);
	else
		gpio_set_value_cansleep(gpio, val);
}

static bool tps65132s_get_gpio(int gpio, enum of_gpio_flags flags)
{
	if (flags == OF_GPIO_ACTIVE_LOW)
		return !gpio_get_value_cansleep(gpio);
	else
		return gpio_get_value_cansleep(gpio);
}

static int tps65132s_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	return 100000 * (40 + selector);
}

static int tps65132s_get_voltage_sel(struct regulator_dev *rdev)
{
	struct tps65132s_priv *priv = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	ret = regmap_read(priv->regmap, TPS65132S_VPOS + rdev->desc->id, &val);
	if (ret < 0)
		return ret;

	return val;
}

static int tps65132s_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct tps65132s_priv *priv = rdev_get_drvdata(rdev);
	return regmap_write(priv->regmap, TPS65132S_VPOS + rdev->desc->id, selector);
}

static int tps65132s_set_current_limit(struct regulator_dev *rdev, int min_uA, int max_uA)
{
	struct tps65132s_priv *priv = rdev_get_drvdata(rdev);
	int ret;

	if (min_uA <= 40000) {
		ret = regmap_update_bits(priv->regmap, TPS65132S_FREQ,
			TPS65132S_APPS_MASK, TPS65132S_APPS_SMARTPHONE);
	} else if (min_uA <= 80000) {
		ret = regmap_update_bits(priv->regmap, TPS65132S_FREQ,
			TPS65132S_APPS_MASK, TPS65132S_APPS_TABLET);
	}
	if (ret < 0)
		return ret;

	tps65132s_set_gpio(priv->sync_gpio, priv->sync_flags, min_uA > 80000);
	return 0;
}

static int tps65132s_get_current_limit(struct regulator_dev *rdev)
{
	struct tps65132s_priv *priv = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	if (tps65132s_get_gpio(priv->sync_gpio, priv->sync_flags))
		return 150000;

	ret = regmap_read(priv->regmap, TPS65132S_FREQ, &val);
	if (ret < 0)
		return ret;

	if ((val & TPS65132S_APPS_MASK) == TPS65132S_APPS_TABLET)
		return 80000;
	else
		return 40000;
}

static int tps65132s_enable(struct regulator_dev *rdev)
{
	struct tps65132s_priv *priv = rdev_get_drvdata(rdev);
	struct tps65132s_reg *reg = &priv->regs[rdev->desc->id];

	if (!reg->is_enabled) {
		if (!priv->en_ref) {
			int ret;

			tps65132s_set_gpio(priv->en_gpio, priv->en_flags, true);
			regcache_cache_only(priv->regmap, false);
			ret = regcache_sync(priv->regmap);
			if (ret < 0) { /* restore */
				regcache_cache_only(priv->regmap, true);
				tps65132s_set_gpio(priv->en_gpio, priv->en_flags, false);
				return ret;
			}
		}
		reg->is_enabled = true;
		priv->en_ref++;
	}

	return 0;
}

static int tps65132s_disable(struct regulator_dev *rdev)
{
	struct tps65132s_priv *priv = rdev_get_drvdata(rdev);
	struct tps65132s_reg *reg = &priv->regs[rdev->desc->id];

	if (reg->is_enabled) {
		reg->is_enabled = false;
		if (!--priv->en_ref) {
			regcache_mark_dirty(priv->regmap);
			regcache_cache_only(priv->regmap, true);
			tps65132s_set_gpio(priv->en_gpio, priv->en_flags, false);
		}
	}

	return 0;
}

static int tps65132s_is_enabled(struct regulator_dev *rdev)
{
	struct tps65132s_priv *priv = rdev_get_drvdata(rdev);
	struct tps65132s_reg *reg = &priv->regs[rdev->desc->id];

	return reg->is_enabled;
}

static struct regulator_ops tps65132s_outp_ops = {
	.list_voltage = tps65132s_list_voltage,
	.set_voltage_sel = tps65132s_set_voltage_sel,
	.get_voltage_sel = tps65132s_get_voltage_sel,
	.enable = tps65132s_enable,
	.disable = tps65132s_disable,
	.is_enabled = tps65132s_is_enabled,
};

static struct regulator_ops tps65132s_outn_ops = {
	.list_voltage = tps65132s_list_voltage,
	.set_voltage_sel = tps65132s_set_voltage_sel,
	.get_voltage_sel = tps65132s_get_voltage_sel,
	.set_current_limit = tps65132s_set_current_limit,
	.get_current_limit = tps65132s_get_current_limit,
	.enable = tps65132s_enable,
	.disable = tps65132s_disable,
	.is_enabled = tps65132s_is_enabled,
};

static struct regulator_desc tps65132s_desc[] = {
	{
		.name = "outp",
		.id = 0,
		.n_voltages = 21,
		.ops = &tps65132s_outp_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "outn",
		.id = 1,
		.n_voltages = 21,
		.ops = &tps65132s_outn_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
};

static bool tps65132s_volatile_reg(struct device *dev, unsigned int reg)
{
	return false;
}

static const u8 tps65132s_defaults[] = {
	[TPS65132S_VPOS] = 0x0a,
	[TPS65132S_VNEG] = 0x0a,
	[TPS65132S_DLYX] = 0x00,
	[TPS65132S_FREQ] = 0x43,
};

static const struct regmap_config tps65132s_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.volatile_reg = tps65132s_volatile_reg,
	.max_register = sizeof(tps65132s_defaults),
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults_raw = tps65132s_defaults,
	.num_reg_defaults_raw = sizeof(tps65132s_defaults),
};

static int tps65132s_init_en_gpio(struct tps65132s_priv *priv, struct device *dev)
{
	int ret;

	priv->en_gpio = of_get_named_gpio_flags(
		dev->of_node, "ti,en-gpio", 0, &priv->en_flags);
	if (priv->en_gpio < 0) {
		dev_err(dev, "Failed to get en-gpio\n");
		return priv->en_gpio;
	}

	ret = tps65132s_request_gpio(priv->en_gpio,
		priv->en_flags, false, "ti,en-gpio");
	if (ret < 0) {
		dev_err(dev, "Failed to request en-gpio\n");
		return ret;
	}

	return 0;
}

static int tps65132s_init_sync_gpio(struct tps65132s_priv *priv, struct device *dev)
{
	int ret;

	priv->sync_gpio = of_get_named_gpio_flags(
		dev->of_node, "ti,sync-gpio", 0, &priv->sync_flags);
	if (priv->sync_gpio < 0) {
		dev_err(dev, "Failed to get sync-gpio\n");
		return priv->sync_gpio;
	}

	ret = tps65132s_request_gpio(priv->sync_gpio,
		priv->sync_flags, true, "ti,sync-gpio");
	if (ret < 0) {
		dev_err(dev, "Failed to request sync-gpio\n");
		return ret;
	}

	return 0;
}

static int tps65132s_parse_dt(struct device *dev, struct tps65132s_reg *reg)
{
	struct device_node *child;

	for_each_child_of_node(dev->of_node, child) {
		if (strcmp(child->name, reg->desc->name) == 0) {
			reg->of_node = child;
			break;
		}
	}

	if (reg->of_node == NULL) {
		dev_err(dev, "Failed to find node %s\n", reg->desc->name);
		return -ENOENT;
	}

	reg->init_data = of_get_regulator_init_data(dev, reg->of_node);
	if (reg->init_data == NULL) {
		dev_err(dev, "Failed to parse node %s\n", reg->desc->name);
		of_node_put(reg->of_node);
		return -EINVAL;
	}

	return 0;
}

static int tps65132s_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tps65132s_priv *priv;
	struct tps65132s_reg *regs;
	int i, ret;

	priv = kzalloc(sizeof(struct tps65132s_priv), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&client->dev, "Failed to alloc tps65132s_priv\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, priv);
	priv->regmap = regmap_init_i2c(client, &tps65132s_regmap_config);
	if (IS_ERR(priv->regmap)) {
		dev_err(&client->dev, "Failed to init regmap\n");
		ret = PTR_ERR(priv->regmap);
		goto init_regmap_err;
	}

	/* don't touch hardware until en pin activate */
	regcache_cache_only(priv->regmap, true);

	ret = tps65132s_init_en_gpio(priv, &client->dev);
	if (ret < 0)
		goto init_en_gpio_err;

	ret = tps65132s_init_sync_gpio(priv, &client->dev);
	if (ret < 0)
		goto init_sync_gpio_err;

	for (i = 0, regs = priv->regs; i < ARRAY_SIZE(tps65132s_desc); i++) {
		regs[i].desc = &tps65132s_desc[i];
		ret = tps65132s_parse_dt(&client->dev, &regs[i]);
		if (ret < 0)
			goto parse_dt_err;

		regs[i].rdev = regulator_register(regs[i].desc,
			&client->dev, regs[i].init_data, priv, regs[i].of_node);
		if (IS_ERR(regs[i].rdev)) {
			dev_err(&client->dev, "Failed to register regulator\n");
			ret = PTR_ERR(regs[i].rdev);
			goto register_reg_err;
		}
	}

	return 0;

register_reg_err:
	of_node_put(regs[i].of_node);
parse_dt_err:
	while (--i >= 0)
		regulator_unregister(regs[i].rdev);
	gpio_free(priv->sync_gpio);
init_sync_gpio_err:
	gpio_free(priv->en_gpio);
init_en_gpio_err:
	regmap_exit(priv->regmap);
init_regmap_err:
	kfree(priv);
	return ret;
}

static int tps65132s_i2c_remove(struct i2c_client *client)
{
	struct tps65132s_priv *priv = i2c_get_clientdata(client);
	struct tps65132s_reg *regs = priv->regs;
	int i;

	for (i = 0; i < ARRAY_SIZE(tps65132s_desc); i++)
		regulator_unregister(regs[i].rdev);

	gpio_free(priv->sync_gpio);
	gpio_free(priv->en_gpio);

	regmap_exit(priv->regmap);
	kfree(priv);

	return 0;
}

static const struct of_device_id tps65132s_of_match[] = {
	{ .compatible = "ti,tps65132s", },
	{ }
};
MODULE_DEVICE_TABLE(of, tps65132s_of_match);

static const struct i2c_device_id tps65132s_i2c_id[] = {
	{ "tps65132s", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tps65132s_i2c_id);

static struct i2c_driver tps65132s_i2c_driver = {
	.driver = {
		.name = "tps65132s",
		.owner = THIS_MODULE,
		.of_match_table = tps65132s_of_match,
	},
	.probe = tps65132s_i2c_probe,
	.remove = tps65132s_i2c_remove,
	.id_table = tps65132s_i2c_id,
};
module_i2c_driver(tps65132s_i2c_driver);

MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("TPS65132S regulator driver");
MODULE_LICENSE("GPL");
