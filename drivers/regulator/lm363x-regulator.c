/*
 * TI LM363X Regulator Driver
 *
 * Copyright 2016 Texas Instruments
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-register.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/slab.h>

/* LM3627X */
#define LM3627X_BOOST_VSEL_MAX		0x3F
#define LM3627X_LDO_VSEL_MAX		0x32
#define LM3627X_VBOOST_MIN		4000000
#define LM3627X_VLDO_MIN			4000000

/* LM3631 */
#define LM3631_BOOST_VSEL_MAX		0x25
#define LM3631_LDO_VSEL_MAX		0x28
#define LM3631_CONT_VSEL_MAX		0x03
#define LM3631_VBOOST_MIN		4500000
#define LM3631_VCONT_MIN		1800000
#define LM3631_VLDO_MIN			4000000
#define ENABLE_TIME_USEC		1000

/* LM3632 */
#define LM3632_BOOST_VSEL_MAX		0x26
#define LM3632_LDO_VSEL_MAX		0x29
#define LM3632_VBOOST_MIN		4500000
#define LM3632_VLDO_MIN			4000000

/* Common */
#define LM363X_STEP_50mV		50000
#define LM363X_STEP_500mV		500000

struct lm363x_regulator {
	struct regmap *regmap;
	struct regulator_dev *regulator;
	struct regulator_init_data *init_data;
};

static const int ldo_cont_enable_time[] = {
	0, 2000, 5000, 10000, 20000, 50000, 100000, 200000,
};

static int lm363x_regulator_enable_time(struct regulator_dev *rdev)
{
	struct lm363x_regulator *lm363x_regulator = rdev_get_drvdata(rdev);
	enum lm363x_regulator_id id = rdev_get_id(rdev);
	u8 val, addr, mask;

	switch (id) {
	case LM3631_LDO_CONT:
		addr = LM3631_REG_ENTIME_VCONT;
		mask = LM3631_ENTIME_CONT_MASK;
		break;
	case LM3631_LDO_OREF:
		addr = LM3631_REG_ENTIME_VOREF;
		mask = LM3631_ENTIME_MASK;
		break;
	case LM3631_LDO_POS:
		addr = LM3631_REG_ENTIME_VPOS;
		mask = LM3631_ENTIME_MASK;
		break;
	case LM3631_LDO_NEG:
		addr = LM3631_REG_ENTIME_VNEG;
		mask = LM3631_ENTIME_MASK;
		break;
	default:
		return 0;
	}

	if (regmap_read(lm363x_regulator->regmap, addr, (unsigned int *)&val))
		return -EINVAL;

	val = (val & mask) >> LM3631_ENTIME_SHIFT;

	if (id == LM3631_LDO_CONT)
		return ldo_cont_enable_time[val];
	else
		return ENABLE_TIME_USEC * val;
}

static struct regulator_ops lm363x_boost_voltage_table_ops = {
	.list_voltage     = regulator_list_voltage_linear,
	.set_voltage_sel  = regulator_set_voltage_sel_regmap,
	.get_voltage_sel  = regulator_get_voltage_sel_regmap,
};

static struct regulator_ops lm363x_regulator_voltage_table_ops = {
	.list_voltage     = regulator_list_voltage_linear,
	.set_voltage_sel  = regulator_set_voltage_sel_regmap,
	.get_voltage_sel  = regulator_get_voltage_sel_regmap,
	.enable           = regulator_enable_regmap,
	.disable          = regulator_disable_regmap,
	.is_enabled       = regulator_is_enabled_regmap,
	.enable_time      = lm363x_regulator_enable_time,
};

static const struct regulator_desc lm363x_regulator_desc[] = {
	/* LM3627X */
	{
		.name           = "vboost",
		.id             = LM3627X_BOOST,
		.ops            = &lm363x_boost_voltage_table_ops,
		.n_voltages     = LM3627X_BOOST_VSEL_MAX + 1,
		.min_uV         = LM3627X_VBOOST_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3627X_REG_VOUT_BOOST,
		.vsel_mask      = LM3627X_VOUT_MASK,
	},
	{
		.name           = "ldo_vpos",
		.id             = LM3627X_LDO_POS,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3627X_LDO_VSEL_MAX + 1,
		.min_uV         = LM3627X_VLDO_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3627X_REG_VOUT_POS,
		.vsel_mask      = LM3627X_VOUT_MASK,
		.enable_reg     = LM3627X_REG_BIAS_CONFIG,
		.enable_mask    = LM3627X_EN_VPOS_MASK,
	},
	{
		.name           = "ldo_vneg",
		.id             = LM3627X_LDO_NEG,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3627X_LDO_VSEL_MAX + 1,
		.min_uV         = LM3627X_VLDO_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3627X_REG_VOUT_NEG,
		.vsel_mask      = LM3627X_VOUT_MASK,
		.enable_reg     = LM3627X_REG_BIAS_CONFIG,
		.enable_mask    = LM3627X_EN_VNEG_MASK,
	},
	/* LM3631 */
	{
		.name           = "vboost",
		.id             = LM3631_BOOST,
		.ops            = &lm363x_boost_voltage_table_ops,
		.n_voltages     = LM3631_BOOST_VSEL_MAX + 1,
		.min_uV         = LM3631_VBOOST_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_BOOST,
		.vsel_mask      = LM3631_VOUT_MASK,
	},
	{
		.name           = "ldo_cont",
		.id             = LM3631_LDO_CONT,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3631_CONT_VSEL_MAX + 1,
		.min_uV         = LM3631_VCONT_MIN,
		.uV_step        = LM363X_STEP_500mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_CONT,
		.vsel_mask      = LM3631_VOUT_CONT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL2,
		.enable_mask    = LM3631_EN_CONT_MASK,
	},
	{
		.name           = "ldo_oref",
		.id             = LM3631_LDO_OREF,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3631_LDO_VSEL_MAX + 1,
		.min_uV         = LM3631_VLDO_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_OREF,
		.vsel_mask      = LM3631_VOUT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL1,
		.enable_mask    = LM3631_EN_OREF_MASK,
	},
	{
		.name           = "ldo_vpos",
		.id             = LM3631_LDO_POS,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3631_LDO_VSEL_MAX + 1,
		.min_uV         = LM3631_VLDO_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_POS,
		.vsel_mask      = LM3631_VOUT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL1,
		.enable_mask    = LM3631_EN_VPOS_MASK,
	},
	{
		.name           = "ldo_vneg",
		.id             = LM3631_LDO_NEG,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3631_LDO_VSEL_MAX + 1,
		.min_uV         = LM3631_VLDO_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3631_REG_VOUT_NEG,
		.vsel_mask      = LM3631_VOUT_MASK,
		.enable_reg     = LM3631_REG_LDO_CTRL1,
		.enable_mask    = LM3631_EN_VNEG_MASK,
	},
	/* LM3632 */
	{
		.name           = "vboost",
		.id             = LM3632_BOOST,
		.ops            = &lm363x_boost_voltage_table_ops,
		.n_voltages     = LM3632_BOOST_VSEL_MAX + 1,
		.min_uV         = LM3632_VBOOST_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3632_REG_VOUT_BOOST,
		.vsel_mask      = LM3632_VOUT_MASK,
	},
	{
		.name           = "ldo_vpos",
		.id             = LM3632_LDO_POS,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3632_LDO_VSEL_MAX + 1,
		.min_uV         = LM3632_VLDO_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3632_REG_VOUT_POS,
		.vsel_mask      = LM3632_VOUT_MASK,
		.enable_reg     = LM3632_REG_BIAS_CONFIG,
		.enable_mask    = LM3632_EN_VPOS_MASK,
	},
	{
		.name           = "ldo_vneg",
		.id             = LM3632_LDO_NEG,
		.ops            = &lm363x_regulator_voltage_table_ops,
		.n_voltages     = LM3632_LDO_VSEL_MAX + 1,
		.min_uV         = LM3632_VLDO_MIN,
		.uV_step        = LM363X_STEP_50mV,
		.type           = REGULATOR_VOLTAGE,
		.owner          = THIS_MODULE,
		.vsel_reg       = LM3632_REG_VOUT_NEG,
		.vsel_mask      = LM3632_VOUT_MASK,
		.enable_reg     = LM3632_REG_BIAS_CONFIG,
		.enable_mask    = LM3632_EN_VNEG_MASK,
	},
};

static int lm363x_regulator_of_get_enable_gpio(struct device_node *np, int id)
{
	/*
	 * Check LCM_EN1/2_GPIO is configured.
	 * Those pins are used for enabling VPOS/VNEG LDOs.
	 */
	switch (id) {
	case LM3627X_LDO_POS:
	case LM3632_LDO_POS:
		return of_get_named_gpio(np, "ti,lcm-en1-gpios", 0);
	case LM3627X_LDO_NEG:
	case LM3632_LDO_NEG:
		return of_get_named_gpio(np, "ti,lcm-en2-gpios", 0);
	default:
		return -EINVAL;
	}
}

static int lm363x_regulator_set_enable_gpio(struct regmap *regmap, int id)
{
	u8 reg;
	u8 mask;

	switch (id) {
	case LM3627X_LDO_POS:
	case LM3627X_LDO_NEG:
		reg = LM3627X_REG_BIAS_CONFIG;
		mask = LM3627X_EXT_EN_MASK;
		break;
	case LM3632_LDO_POS:
	case LM3632_LDO_NEG:
		reg = LM3632_REG_BIAS_CONFIG;
		mask = LM3632_EXT_EN_MASK;
		break;
	default:
		return -EINVAL;
	}

	return regmap_update_bits(regmap, reg, mask, mask);
}

static struct of_regulator_match lm363x_regulator_matches[] = {
	{ .name = "vboost", .driver_data = (void *)LM3627X_BOOST, },
	{ .name = "vpos",   .driver_data = (void *)LM3627X_LDO_POS,  },
	{ .name = "vneg",   .driver_data = (void *)LM3627X_LDO_NEG,  },
	{ .name = "vboost", .driver_data = (void *)LM3631_BOOST, },
	{ .name = "vcont",  .driver_data = (void *)LM3631_LDO_CONT, },
	{ .name = "voref",  .driver_data = (void *)LM3631_LDO_OREF, },
	{ .name = "vpos",   .driver_data = (void *)LM3631_LDO_POS,  },
	{ .name = "vneg",   .driver_data = (void *)LM3631_LDO_NEG,  },
	{ .name = "vboost", .driver_data = (void *)LM3632_BOOST, },
	{ .name = "vpos",   .driver_data = (void *)LM3632_LDO_POS,  },
	{ .name = "vneg",   .driver_data = (void *)LM3632_LDO_NEG,  },
};

static int lm363x_regulator_set_bias_mode(struct device_node *np,
			struct lm363x_regulator *lm363x_regulator, int id)
{
	const char *mode;
	int ret;
	u8 val;

	switch (id) {
	case LM3627X_BOOST:
	case LM3627X_LDO_POS:
	case LM3627X_LDO_NEG:
		ret = of_property_read_string(np, "display-bias-mode", &mode);
		if (ret)
			return ret;
		break;
	default:
		return 0;
	}

	if (!strcmp(mode, "default"))
		val = LM3627X_BIAS_DEFAULT;
	else if (!strcmp(mode, "auto"))
		val = LM3627X_BIAS_AUTO;
	else if (!strcmp(mode, "wake1"))
		val = LM3627X_BIAS_WAKE1;
	else if (!strcmp(mode, "wake2"))
		val = LM3627X_BIAS_WAKE2;
	else
		return -EINVAL;

	return regmap_update_bits(lm363x_regulator->regmap,
				  LM3627X_REG_BIAS_CONFIG,
				  LM3627X_BIAS_MASK, val);
}

static int lm363x_regulator_parse_dt(struct device *dev,
				     struct lm363x_regulator *lm363x_regulator,
				     int id)
{
	struct device_node *node = dev->of_node;
	int count;

	count = of_regulator_match(dev, node, &lm363x_regulator_matches[id], 1);
	if (count <= 0)
		return -ENODEV;

	lm363x_regulator->init_data = lm363x_regulator_matches[id].init_data;

	return lm363x_regulator_set_bias_mode(node, lm363x_regulator, id);
}

static int lm363x_regulator_probe(struct platform_device *pdev)
{
	struct ti_lmu *lmu = dev_get_drvdata(pdev->dev.parent);
	struct lm363x_regulator *lm363x_regulator;
	struct regmap *regmap = lmu->regmap;
	struct regulator_config cfg = { };
	struct regulator_dev *rdev;
	struct device *dev = &pdev->dev;
	int id = pdev->id;
	int ret, ena_gpio;

	lm363x_regulator = devm_kzalloc(dev, sizeof(*lm363x_regulator),
					GFP_KERNEL);
	if (!lm363x_regulator)
		return -ENOMEM;

	lm363x_regulator->regmap = regmap;

	ret = lm363x_regulator_parse_dt(dev, lm363x_regulator, id);
	if (ret)
		return ret;

	cfg.dev = dev;
	cfg.init_data = lm363x_regulator->init_data;
	cfg.driver_data = lm363x_regulator;
	cfg.regmap = regmap;

	/*
	 * LLDOs of LM3627X and LM3632 LDOs can be controlled by external pin.
	 * Register update is required if the pin is used.
	 */
	ena_gpio = lm363x_regulator_of_get_enable_gpio(dev->of_node, id);
	if (gpio_is_valid(ena_gpio)) {
		cfg.ena_gpio = ena_gpio;
		cfg.ena_gpio_flags = GPIOF_OUT_INIT_LOW;

		ret = lm363x_regulator_set_enable_gpio(regmap, id);
		if (ret) {
			dev_err(dev, "External pin err: %d\n", ret);
			return ret;
		}
	}

	rdev = regulator_register(&lm363x_regulator_desc[id], &cfg);
	if (IS_ERR(rdev)) {
		ret = PTR_ERR(rdev);
		dev_err(dev, "[%d] regulator register err: %d\n", id, ret);
		return ret;
	}

	lm363x_regulator->regulator = rdev;
	platform_set_drvdata(pdev, lm363x_regulator);

	return 0;
}

static int lm363x_regulator_remove(struct platform_device *pdev)
{
	struct lm363x_regulator *lm363x_regulator = platform_get_drvdata(pdev);

	regulator_unregister(lm363x_regulator->regulator);
	return 0;
}

static struct platform_driver lm363x_regulator_driver = {
	.probe = lm363x_regulator_probe,
	.remove = lm363x_regulator_remove,
	.driver = {
		.name = "lm363x-regulator",
	},
};

module_platform_driver(lm363x_regulator_driver);

MODULE_DESCRIPTION("TI LM363X Regulator Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:lm363x-regulator");
