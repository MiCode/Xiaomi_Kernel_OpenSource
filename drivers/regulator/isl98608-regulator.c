/*
 * isl98608-regulator.c  --  regulator driver for ISL98608IIZ-T
 *
 * Copyright (C) 2014 Xiaomi Corporation
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * Author: Nannan Wang <wangnannan@xiaomi.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#define DEBUG
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>


#define ISL98608_VOLTAGE_MIN	5000000
#define ISL98608_VOLTAGE_MAX	7000000
#define ISL98608_VOLTAGE_STEP	50000
#define ISL98608_VOLTAGE_LEVELS	\
		((ISL98608_VOLTAGE_MAX - ISL98608_VOLTAGE_MIN) \
		/ ISL98608_VOLTAGE_STEP + 1)

#define ISL98608_VPOS	0x08
#define ISL98608_VBST	0x06

struct isl98608_reg {
	struct regulator_dev *rdev;
	struct regulator_desc *desc;
	struct device_node *of_node;
	struct regulator_init_data *init_data;
	enum of_gpio_flags en_flags;
	int en_gpio;
	bool is_enabled;
	int en_ref;
};

struct isl98608_chip {
	struct device *dev;
	struct regmap *regmap;
	struct isl98608_reg regs[2];
};

static int isl98608_request_gpio(int gpio, enum of_gpio_flags flags, bool val, const char *label)
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

static void isl98608_set_gpio(int gpio, enum of_gpio_flags flags, bool val)
{
	if (flags == OF_GPIO_ACTIVE_LOW)
		gpio_set_value_cansleep(gpio, !val);
	else
		gpio_set_value_cansleep(gpio, val);
}

static int isl98608_get_voltage_sel(struct regulator_dev *rdev)
{
	struct isl98608_chip *isl98608 = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret;

	dev_dbg(isl98608->dev, "%s: enter\n", __func__);
	ret = regmap_read(isl98608->regmap, ISL98608_VPOS + rdev->desc->id, &val);
	if (ret < 0)
		return ret;

	return val;
}

static int isl98608_set_voltage_sel(struct regulator_dev *rdev, unsigned selector)
{
	struct isl98608_chip *isl98608 = rdev_get_drvdata(rdev);
	int ret;

	dev_dbg(isl98608->dev, "%s: selector %d\n", __func__, selector);
	ret = regmap_write(isl98608->regmap, ISL98608_VBST, selector);
	if (ret < 0)
		return ret;

	return regmap_write(isl98608->regmap, ISL98608_VPOS + rdev->desc->id, selector);
}

static int isl98608_enable(struct regulator_dev *rdev)
{
	struct isl98608_chip *isl98608 = rdev_get_drvdata(rdev);
	struct isl98608_reg *reg = &isl98608->regs[rdev->desc->id];
	int ret;

	dev_dbg(isl98608->dev, "%s: %s\n", __func__, reg->desc->name);
	if (!reg->is_enabled) {
		if (!reg->en_ref) {
			isl98608_set_gpio(reg->en_gpio, reg->en_flags, true);
			if (ret < 0) { /* restore */
				isl98608_set_gpio(reg->en_gpio, reg->en_flags, false);
				return ret;
			}
		}
		reg->is_enabled = true;
		reg->en_ref++;
	}

	return 0;
}

static int isl98608_disable(struct regulator_dev *rdev)
{
	struct isl98608_chip *isl98608 = rdev_get_drvdata(rdev);
	struct isl98608_reg *reg = &isl98608->regs[rdev->desc->id];

	dev_dbg(isl98608->dev, "%s: %s\n", __func__, reg->desc->name);
	if (reg->is_enabled) {
		reg->is_enabled = false;
		if (!--reg->en_ref)
			isl98608_set_gpio(reg->en_gpio, reg->en_flags, false);
	}

	return 0;
}

static int isl98608_is_enabled(struct regulator_dev *rdev)
{
	struct isl98608_chip *isl98608 = rdev_get_drvdata(rdev);
	struct isl98608_reg *reg = &isl98608->regs[rdev->desc->id];

	return reg->is_enabled;
}

static struct regulator_ops isl98608_out_ops = {
	.set_voltage_sel = isl98608_set_voltage_sel,
	.get_voltage_sel = isl98608_get_voltage_sel,
	.enable = isl98608_enable,
	.disable = isl98608_disable,
	.is_enabled = isl98608_is_enabled,
	.list_voltage = regulator_list_voltage_linear,
	.map_voltage = regulator_map_voltage_linear,
};

static struct regulator_desc isl98608_desc[] = {
	{
		.name = "isl98608_outn",
		.id = 0,
		.n_voltages = ISL98608_VOLTAGE_LEVELS,
		.ops = &isl98608_out_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.min_uV	= ISL98608_VOLTAGE_MIN,
		.uV_step = ISL98608_VOLTAGE_STEP,
	},
	{
		.name = "isl98608_outp",
		.id = 1,
		.n_voltages = ISL98608_VOLTAGE_LEVELS,
		.ops = &isl98608_out_ops,
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
		.min_uV	= ISL98608_VOLTAGE_MIN,
		.uV_step = ISL98608_VOLTAGE_STEP,
	},
};

static const struct regmap_config isl98608_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int isl98608_init_enn_gpio(struct isl98608_chip *isl98608, struct device *dev)
{
	struct isl98608_reg *reg = &isl98608->regs[0];
	int ret;

	reg->en_gpio = of_get_named_gpio_flags(
		dev->of_node, "intersil,enn-gpio", 0, &reg->en_flags);
	if (reg->en_gpio < 0) {
		dev_err(dev, "%s: Failed to get enn-gpio\n", __func__);
		return reg->en_gpio;
	}

	ret = isl98608_request_gpio(reg->en_gpio,
		reg->en_flags, false, "intersil,enn-gpio");
	if (ret < 0) {
		dev_err(dev, "%s: Failed to request enn-gpio\n", __func__);
		return ret;
	}

	return 0;
}

static int isl98608_init_enp_gpio(struct isl98608_chip *isl98608, struct device *dev)
{
	struct isl98608_reg *reg = &isl98608->regs[1];
	int ret;

	reg->en_gpio = of_get_named_gpio_flags(
		dev->of_node, "intersil,enp-gpio", 0, &reg->en_flags);
	if (reg->en_gpio < 0) {
		dev_err(dev, "%s: Failed to get enp-gpio\n", __func__);
		return reg->en_gpio;
	}

	ret = isl98608_request_gpio(reg->en_gpio,
		reg->en_flags, false, "intersil,enp-gpio");
	if (ret < 0) {
		dev_err(dev, "%s: Failed to request enp-gpio\n", __func__);
		return ret;
	}

	return 0;
}

static int isl98608_parse_dt(struct device *dev, struct isl98608_reg *reg)
{
	struct device_node *child;

	dev_dbg(dev, "%s: enter\n", __func__);
	for_each_child_of_node(dev->of_node, child) {
		if (strcmp(child->name, reg->desc->name) == 0) {
			reg->of_node = child;
			break;
		}
	}

	if (reg->of_node == NULL) {
		dev_err(dev, "%s: Failed to find node %s\n", __func__, reg->desc->name);
		return -ENOENT;
	}

	reg->init_data = of_get_regulator_init_data(dev, reg->of_node);
	if (reg->init_data == NULL) {
		dev_err(dev, "%s: Failed to parse node %s\n", __func__, reg->desc->name);
		of_node_put(reg->of_node);
		return -EINVAL;
	}

	return 0;
}

static int isl98608_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct isl98608_chip *isl98608;
	struct isl98608_reg *regs;
	struct regulator_config config;
	int i, ret;

	dev_dbg(&client->dev, "%s: enter\n", __func__);
	isl98608 = kzalloc(sizeof(struct isl98608_chip), GFP_KERNEL);
	if (isl98608 == NULL) {
		dev_err(&client->dev, "%s: Failed to alloc isl98608_chip\n", __func__);
		return -ENOMEM;
	}

	isl98608->dev = &client->dev;
	i2c_set_clientdata(client, isl98608);
	isl98608->regmap = regmap_init_i2c(client, &isl98608_regmap_config);
	if (IS_ERR(isl98608->regmap)) {
		dev_err(&client->dev, "%s: Failed to init regmap\n", __func__);
		ret = PTR_ERR(isl98608->regmap);
		goto init_regmap_err;
	}

	ret = isl98608_init_enn_gpio(isl98608, &client->dev);
	if (ret < 0)
		goto init_enn_gpio_err;

	ret = isl98608_init_enp_gpio(isl98608, &client->dev);
	if (ret < 0)
		goto init_enp_gpio_err;

	for (i = 0, regs = isl98608->regs; i < ARRAY_SIZE(isl98608_desc); i++) {
		regs[i].desc = &isl98608_desc[i];
		ret = isl98608_parse_dt(&client->dev, &regs[i]);
		if (ret < 0)
			goto parse_dt_err;

		config.dev = &client->dev;
		config.init_data = regs[i].init_data;
		config.regmap = isl98608->regmap;
		config.driver_data = isl98608;
		config.of_node = regs[i].of_node;

		regs[i].rdev = regulator_register(regs[i].desc, &config);
		if (IS_ERR(regs[i].rdev)) {
			dev_err(&client->dev, "%s: Failed to register regulator\n", __func__);
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
	gpio_free(isl98608->regs[1].en_gpio);
init_enp_gpio_err:
	gpio_free(isl98608->regs[0].en_gpio);
init_enn_gpio_err:
	regmap_exit(isl98608->regmap);
init_regmap_err:
	kfree(isl98608);
	return ret;
}

static int isl98608_i2c_remove(struct i2c_client *client)
{
	struct isl98608_chip *isl98608 = i2c_get_clientdata(client);
	struct isl98608_reg *regs = isl98608->regs;
	int i;

	for (i = 0; i < ARRAY_SIZE(isl98608_desc); i++)
		regulator_unregister(regs[i].rdev);

	gpio_free(regs[0].en_gpio);
	gpio_free(regs[1].en_gpio);

	regmap_exit(isl98608->regmap);
	kfree(isl98608);

	return 0;
}

static const struct of_device_id isl98608_of_match[] = {
	{ .compatible = "intersil,isl98608", },
	{ }
};
MODULE_DEVICE_TABLE(of, isl98608_of_match);

static const struct i2c_device_id isl98608_i2c_id[] = {
	{ "isl98608", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, isl98608_i2c_id);

static struct i2c_driver isl98608_i2c_driver = {
	.driver = {
		.name = "isl98608",
		.owner = THIS_MODULE,
		.of_match_table = isl98608_of_match,
	},
	.probe = isl98608_i2c_probe,
	.remove = isl98608_i2c_remove,
	.id_table = isl98608_i2c_id,
};
module_i2c_driver(isl98608_i2c_driver);

MODULE_AUTHOR("Nannan Wang <wangnannan@xiaomi.com>");
MODULE_DESCRIPTION("ISL98608IIZ-T regulator driver");
MODULE_LICENSE("GPL");
