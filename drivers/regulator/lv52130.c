/*
 * lv52130.c  --  regulator driver for LV52130
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
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>

#define LV52130_VOUT1			0x00
#define LV52130_VOUT2			0x01
#define LV52130_MODE			0x03

struct lv52130_reg {
	struct regulator_dev *rdev;
	struct regulator_desc *desc;
	struct device_node *of_node;
	struct regulator_init_data *init_data;
	enum of_gpio_flags enable_flags;
	int enable_gpio;
	bool is_enabled;
	unsigned selector;
};

struct lv52130_priv {
	struct i2c_client *client;
	struct lv52130_reg regs[2];
};

static int lv52130_list_voltage(struct regulator_dev *rdev, unsigned selector)
{
	return 100000 * (41 + selector);
}

static int lv52130_get_voltage_sel(struct regulator_dev *rdev)
{
	struct lv52130_priv *priv = rdev_get_drvdata(rdev);
	struct lv52130_reg *reg = &priv->regs[rdev->desc->id];

	return reg->selector;
}

static int lv52130_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct lv52130_priv *priv = rdev_get_drvdata(rdev);
	struct lv52130_reg *reg = &priv->regs[rdev->desc->id];
	int id = rdev->desc->id, ret;

	ret = i2c_smbus_write_byte_data(priv->client, LV52130_VOUT1 + id, selector + 1);
	if (ret >= 0)
		reg->selector = selector;

	return ret;
}

static int lv52130_enable(struct regulator_dev *rdev)
{
	struct lv52130_priv *priv = rdev_get_drvdata(rdev);
	struct lv52130_reg *reg = &priv->regs[rdev->desc->id];

	if (reg->enable_flags == OF_GPIO_ACTIVE_LOW)
		gpio_set_value_cansleep(reg->enable_gpio, 0);
	else
		gpio_set_value_cansleep(reg->enable_gpio, 1);
	reg->is_enabled = true;

	return 0;
}

static int lv52130_disable(struct regulator_dev *rdev)
{
	struct lv52130_priv *priv = rdev_get_drvdata(rdev);
	struct lv52130_reg *reg = &priv->regs[rdev->desc->id];

	if (reg->enable_flags == OF_GPIO_ACTIVE_LOW)
		gpio_set_value_cansleep(reg->enable_gpio, 1);
	else
		gpio_set_value_cansleep(reg->enable_gpio, 0);
	reg->is_enabled = false;

	return 0;
}

static int lv52130_is_enabled(struct regulator_dev *rdev)
{
	struct lv52130_priv *priv = rdev_get_drvdata(rdev);
	struct lv52130_reg *reg = &priv->regs[rdev->desc->id];

	return reg->is_enabled;
}

static int lv52130_enable_time(struct regulator_dev *dev)
{
	return 1000;
}

static struct regulator_ops lv52130_ops = {
	.list_voltage = lv52130_list_voltage,
	.set_voltage_sel = lv52130_set_voltage_sel,
	.get_voltage_sel = lv52130_get_voltage_sel,
	.enable = lv52130_enable,
	.disable = lv52130_disable,
	.is_enabled = lv52130_is_enabled,
	.enable_time = lv52130_enable_time,
};

static struct regulator_desc lv52130_desc[] = {
	{
		.name = "vout1",
		.id = 0,
		.n_voltages = 17,
		.ops = &lv52130_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "vout2",
		.id = 1,
		.n_voltages = 17,
		.ops = &lv52130_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
};

static int lv52130_parse_dt(struct device *dev, struct lv52130_reg *reg)
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

	if (reg->init_data->constraints.always_on ||
	    reg->init_data->constraints.boot_on)
		reg->is_enabled = true;

	reg->enable_gpio = of_get_named_gpio_flags(reg->of_node,
		"on,enable-gpio", 0, &reg->enable_flags);
	if (reg->enable_gpio < 0) {
		dev_err(dev, "Failed to get enable-gpio\n");
		of_node_put(reg->of_node);
		return reg->enable_gpio;
	}

	return 0;
}

static int lv52130_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct lv52130_priv *priv;
	struct lv52130_reg *regs;
	int i, ret;

	priv = kzalloc(sizeof(struct lv52130_priv), GFP_KERNEL);
	if (priv == NULL) {
		dev_err(&client->dev, "Failed to alloc lv52130_priv\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, priv);
	priv->client = client;
	regs = priv->regs;

	for (i = 0; i < ARRAY_SIZE(lv52130_desc); i++) {
		regs[i].desc = &lv52130_desc[i];
		ret = lv52130_parse_dt(&client->dev, &regs[i]);
		if (ret < 0)
			goto parse_dt_err;

		if (regs[i].enable_flags == OF_GPIO_ACTIVE_LOW) {
			if (regs[i].is_enabled) {
				ret = gpio_request_one(regs[i].enable_gpio,
					GPIOF_OUT_INIT_LOW, "on,enable-gpio");
			} else {
				ret = gpio_request_one(regs[i].enable_gpio,
					GPIOF_OUT_INIT_HIGH, "on,enable-gpio");
			}
		} else {
			if (regs[i].is_enabled) {
				ret = gpio_request_one(regs[i].enable_gpio,
					GPIOF_OUT_INIT_HIGH, "on,enable-gpio");
			} else {
				ret = gpio_request_one(regs[i].enable_gpio,
					GPIOF_OUT_INIT_LOW, "on,enable-gpio");
			}
		}
		if (ret < 0) {
			dev_err(&client->dev, "Failed to request gpio\n");
			goto request_gpio_err;
		}

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
	gpio_free(regs[i].enable_gpio);
request_gpio_err:
	of_node_put(regs[i].of_node);
parse_dt_err:
	while (--i >= 0) {
		regulator_unregister(regs[i].rdev);
		gpio_free(regs[i].enable_gpio);
	}
	kfree(priv);
	return ret;
}

static int lv52130_i2c_remove(struct i2c_client *client)
{
	struct lv52130_priv *priv = i2c_get_clientdata(client);
	struct lv52130_reg *regs = priv->regs;
	int i;

	for (i = 0; i < ARRAY_SIZE(lv52130_desc); i++) {
		regulator_unregister(regs[i].rdev);
		gpio_free(regs[i].enable_gpio);
	}

	kfree(priv);
	return 0;
}

static const struct of_device_id lv52130_of_match[] = {
	{ .compatible = "on,lv52130", },
	{ }
};
MODULE_DEVICE_TABLE(of, lv52130_of_match);

static const struct i2c_device_id lv52130_i2c_id[] = {
	{ "lv52130", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lv52130_i2c_id);

static struct i2c_driver lv52130_i2c_driver = {
	.driver = {
		.name = "lv52130",
		.owner = THIS_MODULE,
		.of_match_table = lv52130_of_match,
	},
	.probe = lv52130_i2c_probe,
	.remove = lv52130_i2c_remove,
	.id_table = lv52130_i2c_id,
};
module_i2c_driver(lv52130_i2c_driver);

MODULE_AUTHOR("Xiang Xiao <xiaoxiang@xiaomi.com>");
MODULE_DESCRIPTION("LV52130 regulator driver");
MODULE_LICENSE("GPL");
