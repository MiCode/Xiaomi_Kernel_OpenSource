/*
 * max8907c-regulator.c -- support regulators in max8907c
 *
 * Copyright (C) 2010 Gyungoh Yoo <jack.yoo@maxim-ic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/max8907c.h>
#include <linux/regulator/max8907c-regulator.h>

#define MAX8907C_II2RR_VERSION_MASK	0xF0
#define MAX8907C_II2RR_VERSION_REV_A	0x00
#define MAX8907C_II2RR_VERSION_REV_B	0x10
#define MAX8907C_II2RR_VERSION_REV_C	0x30

#define MAX8907C_REGULATOR_CNT (ARRAY_SIZE(max8907c_regulators))

struct max8907c_regulator_info {
	u32 min_uV;
	u32 max_uV;
	u32 step_uV;
	u8 reg_base;
	u32 enable_time_us;
	struct regulator_desc desc;
	struct i2c_client *i2c;
};

#define REG_LDO(ids, base, min, max, step) \
	{ \
		.min_uV = (min), \
		.max_uV = (max), \
		.step_uV = (step), \
		.reg_base = (base), \
		.desc = { \
			.name = #ids, \
			.id = MAX8907C_##ids, \
			.n_voltages = ((max) - (min)) / (step) + 1, \
			.ops = &max8907c_ldo_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
	}

#define REG_FIXED(ids, voltage) \
	{ \
		.min_uV = (voltage), \
		.max_uV = (voltage), \
		.desc = { \
			.name = #ids, \
			.id = MAX8907C_##ids, \
			.n_voltages = 1, \
			.ops = &max8907c_fixed_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
	}

#define REG_OUT5V(ids, base, voltage) \
	{ \
		.min_uV = (voltage), \
		.max_uV = (voltage), \
		.reg_base = (base), \
		.desc = { \
			.name = #ids, \
			.id = MAX8907C_##ids, \
			.n_voltages = 1, \
			.ops = &max8907c_out5v_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
	}

#define REG_BBAT(ids, base, min, max, step) \
	{ \
		.min_uV = (min), \
		.max_uV = (max), \
		.step_uV = (step), \
		.reg_base = (base), \
		.desc = { \
			.name = #ids, \
			.id = MAX8907C_##ids, \
			.n_voltages = ((max) - (min)) / (step) + 1, \
			.ops = &max8907c_bbat_ops, \
			.type = REGULATOR_VOLTAGE, \
			.owner = THIS_MODULE, \
		}, \
	}

#define REG_WLED(ids, base, voltage) \
	{ \
		.min_uV = (voltage), \
		.max_uV = (voltage), \
		.reg_base = (base), \
		.desc = { \
			.name = #ids, \
			.id = MAX8907C_##ids, \
			.n_voltages = 1, \
			.ops = &max8907c_wled_ops, \
			.type = REGULATOR_CURRENT, \
			.owner = THIS_MODULE, \
		}, \
	}

#define LDO_750_50(id, base) REG_LDO(id, (base), 750000, 3900000, 50000)
#define LDO_650_25(id, base) REG_LDO(id, (base), 650000, 2225000, 25000)

static int max8907c_regulator_list_voltage(struct regulator_dev *dev,
					   unsigned index);
static int max8907c_regulator_ldo_set_voltage(struct regulator_dev *dev,
					      int min_uV, int max_uV);
static int max8907c_regulator_bbat_set_voltage(struct regulator_dev *dev,
					       int min_uV, int max_uV);
static int max8907c_regulator_ldo_get_voltage(struct regulator_dev *dev);
static int max8907c_regulator_fixed_get_voltage(struct regulator_dev *dev);
static int max8907c_regulator_bbat_get_voltage(struct regulator_dev *dev);
static int max8907c_regulator_wled_set_current_limit(struct regulator_dev *dev,
						     int min_uA, int max_uA);
static int max8907c_regulator_wled_get_current_limit(struct regulator_dev *dev);
static int max8907c_regulator_ldo_enable(struct regulator_dev *dev);
static int max8907c_regulator_out5v_enable(struct regulator_dev *dev);
static int max8907c_regulator_ldo_disable(struct regulator_dev *dev);
static int max8907c_regulator_out5v_disable(struct regulator_dev *dev);
static int max8907c_regulator_ldo_is_enabled(struct regulator_dev *dev);
static int max8907c_regulator_ldo_enable_time(struct regulator_dev *dev);
static int max8907c_regulator_out5v_is_enabled(struct regulator_dev *dev);

static struct regulator_ops max8907c_ldo_ops = {
	.list_voltage = max8907c_regulator_list_voltage,
	.set_voltage = max8907c_regulator_ldo_set_voltage,
	.get_voltage = max8907c_regulator_ldo_get_voltage,
	.enable = max8907c_regulator_ldo_enable,
	.disable = max8907c_regulator_ldo_disable,
	.is_enabled = max8907c_regulator_ldo_is_enabled,
	.enable_time = max8907c_regulator_ldo_enable_time,
};

static struct regulator_ops max8907c_fixed_ops = {
	.list_voltage = max8907c_regulator_list_voltage,
	.get_voltage = max8907c_regulator_fixed_get_voltage,
};

static struct regulator_ops max8907c_out5v_ops = {
	.list_voltage = max8907c_regulator_list_voltage,
	.get_voltage = max8907c_regulator_fixed_get_voltage,
	.enable = max8907c_regulator_out5v_enable,
	.disable = max8907c_regulator_out5v_disable,
	.is_enabled = max8907c_regulator_out5v_is_enabled,
};

static struct regulator_ops max8907c_bbat_ops = {
	.list_voltage = max8907c_regulator_list_voltage,
	.set_voltage = max8907c_regulator_bbat_set_voltage,
	.get_voltage = max8907c_regulator_bbat_get_voltage,
};

static struct regulator_ops max8907c_wled_ops = {
	.list_voltage = max8907c_regulator_list_voltage,
	.set_current_limit = max8907c_regulator_wled_set_current_limit,
	.get_current_limit = max8907c_regulator_wled_get_current_limit,
	.get_voltage = max8907c_regulator_fixed_get_voltage,
};

static struct max8907c_regulator_info max8907c_regulators[] = {
	REG_LDO(SD1, MAX8907C_REG_SDCTL1, 650000, 2225000, 25000),
	REG_LDO(SD2, MAX8907C_REG_SDCTL2, 637500, 1425000, 12500),
	REG_LDO(SD3, MAX8907C_REG_SDCTL3, 750000, 3900000, 50000),
	LDO_750_50(LDO1, MAX8907C_REG_LDOCTL1),
	LDO_650_25(LDO2, MAX8907C_REG_LDOCTL2),
	LDO_650_25(LDO3, MAX8907C_REG_LDOCTL3),
	LDO_750_50(LDO4, MAX8907C_REG_LDOCTL4),
	LDO_750_50(LDO5, MAX8907C_REG_LDOCTL5),
	LDO_750_50(LDO6, MAX8907C_REG_LDOCTL6),
	LDO_750_50(LDO7, MAX8907C_REG_LDOCTL7),
	LDO_750_50(LDO8, MAX8907C_REG_LDOCTL8),
	LDO_750_50(LDO9, MAX8907C_REG_LDOCTL9),
	LDO_750_50(LDO10, MAX8907C_REG_LDOCTL10),
	LDO_750_50(LDO11, MAX8907C_REG_LDOCTL11),
	LDO_750_50(LDO12, MAX8907C_REG_LDOCTL12),
	LDO_750_50(LDO13, MAX8907C_REG_LDOCTL13),
	LDO_750_50(LDO14, MAX8907C_REG_LDOCTL14),
	LDO_750_50(LDO15, MAX8907C_REG_LDOCTL15),
	LDO_750_50(LDO16, MAX8907C_REG_LDOCTL16),
	LDO_650_25(LDO17, MAX8907C_REG_LDOCTL17),
	LDO_650_25(LDO18, MAX8907C_REG_LDOCTL18),
	LDO_750_50(LDO19, MAX8907C_REG_LDOCTL19),
	LDO_750_50(LDO20, MAX8907C_REG_LDOCTL20),
	REG_OUT5V(OUT5V, MAX8907C_REG_OUT5VEN, 5000000),
	REG_OUT5V(OUT33V, MAX8907C_REG_OUT33VEN, 3300000),
	REG_BBAT(BBAT, MAX8907C_REG_BBAT_CNFG, 2400000, 3000000, 200000),
	REG_FIXED(SDBY, 1200000),
	REG_FIXED(VRTC, 3300000),
	REG_WLED(WLED, MAX8907C_REG_ILED_CNTL, 0),
};

static int max8907c_regulator_list_voltage(struct regulator_dev *rdev,
					   unsigned index)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	return info->min_uV + info->step_uV * index;
}

static int max8907c_regulator_ldo_set_voltage(struct regulator_dev *rdev,
					      int min_uV, int max_uV)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	if (min_uV < info->min_uV || max_uV > info->max_uV)
		return -EDOM;

	val = (min_uV - info->min_uV) / info->step_uV;

	return max8907c_reg_write(info->i2c, info->reg_base + MAX8907C_VOUT, val);
}

static int max8907c_regulator_bbat_set_voltage(struct regulator_dev *rdev,
					       int min_uV, int max_uV)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	if (min_uV < info->min_uV || max_uV > info->max_uV)
		return -EDOM;

	val = (min_uV - info->min_uV) / info->step_uV;

	return max8907c_set_bits(info->i2c, info->reg_base, MAX8907C_MASK_VBBATTCV,
				 val);
}

static int max8907c_regulator_ldo_get_voltage(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	val = max8907c_reg_read(info->i2c, info->reg_base + MAX8907C_VOUT);
	return val * info->step_uV + info->min_uV;
}

static int max8907c_regulator_fixed_get_voltage(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	return info->min_uV;
}

static int max8907c_regulator_bbat_get_voltage(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	val =
	    max8907c_reg_read(info->i2c, info->reg_base) & MAX8907C_MASK_VBBATTCV;
	return val * info->step_uV + info->min_uV;
}

static int max8907c_regulator_wled_set_current_limit(struct regulator_dev *rdev,
						     int min_uA, int max_uA)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	if (min_uA > 25500)
		return -EDOM;

	return max8907c_reg_write(info->i2c, info->reg_base, min_uA / 100);
}

static int max8907c_regulator_wled_get_current_limit(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	val = max8907c_reg_read(info->i2c, info->reg_base);
	return val * 100;
}

static int max8907c_regulator_ldo_enable(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	return max8907c_set_bits(info->i2c, info->reg_base + MAX8907C_CTL,
				 MAX8907C_MASK_LDO_EN | MAX8907C_MASK_LDO_SEQ,
				 MAX8907C_MASK_LDO_EN | MAX8907C_MASK_LDO_SEQ);
}

static int max8907c_regulator_out5v_enable(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	return max8907c_set_bits(info->i2c, info->reg_base,
				 MAX8907C_MASK_OUT5V_VINEN |
				 MAX8907C_MASK_OUT5V_ENSRC |
				 MAX8907C_MASK_OUT5V_EN,
				 MAX8907C_MASK_OUT5V_ENSRC |
				 MAX8907C_MASK_OUT5V_EN);
}

static int max8907c_regulator_ldo_disable(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	return max8907c_set_bits(info->i2c, info->reg_base + MAX8907C_CTL,
				 MAX8907C_MASK_LDO_EN | MAX8907C_MASK_LDO_SEQ,
				 MAX8907C_MASK_LDO_SEQ);
}

static int max8907c_regulator_out5v_disable(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	return max8907c_set_bits(info->i2c, info->reg_base,
				 MAX8907C_MASK_OUT5V_VINEN |
				 MAX8907C_MASK_OUT5V_ENSRC |
				 MAX8907C_MASK_OUT5V_EN,
				 MAX8907C_MASK_OUT5V_ENSRC);
}

static int max8907c_regulator_ldo_is_enabled(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	val = max8907c_reg_read(info->i2c, info->reg_base + MAX8907C_CTL);
	if (val < 0)
		return -EDOM;

	return (val & MAX8907C_MASK_LDO_EN) || !(val & MAX8907C_MASK_LDO_SEQ);
}

static int max8907c_regulator_ldo_enable_time(struct regulator_dev *rdev)
{
	struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);

	return info->enable_time_us;
}

static int max8907c_regulator_out5v_is_enabled(struct regulator_dev *rdev)
{
	const struct max8907c_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	val = max8907c_reg_read(info->i2c, info->reg_base);
	if (val < 0)
		return -EDOM;

	if ((val &
	     (MAX8907C_MASK_OUT5V_VINEN | MAX8907C_MASK_OUT5V_ENSRC |
	      MAX8907C_MASK_OUT5V_EN))
	    == MAX8907C_MASK_OUT5V_ENSRC)
		return 1;

	return 0;
}

static int max8907c_regulator_probe(struct platform_device *pdev)
{
	struct max8907c *max8907c = dev_get_drvdata(pdev->dev.parent);
	struct max8907c_regulator_info *info;
	struct regulator_dev *rdev;
	struct regulator_init_data *p = pdev->dev.platform_data;
	struct max8907c_chip_regulator_data *chip_data = p->driver_data;;
	u8 version;

	/* Backwards compatibility with max8907b, SD1 uses different voltages */
	version = max8907c_reg_read(max8907c->i2c_power, MAX8907C_REG_II2RR);
	if ((version & MAX8907C_II2RR_VERSION_MASK) == MAX8907C_II2RR_VERSION_REV_B) {
		max8907c_regulators[MAX8907C_SD1].min_uV = 637500;
		max8907c_regulators[MAX8907C_SD1].max_uV = 1425000;
		max8907c_regulators[MAX8907C_SD1].step_uV = 12500;
	}

	info = &max8907c_regulators[pdev->id];
	info->i2c = max8907c->i2c_power;

	if (chip_data != NULL)
		info->enable_time_us = chip_data->enable_time_us;

	rdev = regulator_register(&info->desc, &pdev->dev,
				pdev->dev.platform_data, info, NULL);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "Cannot register regulator \"%s\", %ld\n",
			info->desc.name, PTR_ERR(rdev));
		goto error;
	}

	platform_set_drvdata(pdev, rdev);
	return 0;

error:
	return PTR_ERR(rdev);
}

static int max8907c_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);
	return 0;
}

static struct platform_driver max8907c_regulator_driver = {
	.driver = {
		   .name = "max8907c-regulator",
		   .owner = THIS_MODULE,
		   },
	.probe = max8907c_regulator_probe,
	.remove = __devexit_p(max8907c_regulator_remove),
};

static int __init max8907c_regulator_init(void)
{
	return platform_driver_register(&max8907c_regulator_driver);
}

subsys_initcall(max8907c_regulator_init);

static void __exit max8907c_reg_exit(void)
{
	platform_driver_unregister(&max8907c_regulator_driver);
}

module_exit(max8907c_reg_exit);

MODULE_DESCRIPTION("MAX8907C regulator driver");
MODULE_AUTHOR("Gyungoh Yoo <jack.yoo@maxim-ic.com>");
MODULE_LICENSE("GPL");
