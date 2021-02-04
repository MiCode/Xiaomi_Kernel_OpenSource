/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: HenryC.Chen <henryc.chen@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regmap.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/sym827-regulator.h>

#define SYM827_BUCK_MODE_AUTO	0
#define SYM827_BUCK_MODE_PWM	1

/* sym827 REGULATOR IDs */
#define SYM827_ID_BUCK	0

#define SYM827_TEST_NODE

struct sym827_pdata {
	struct device *dev;
	struct regulator_init_data *init_data;
	int gpio_vsel;
	int gpio_en;
	struct device_node *reg_node;
};

struct sym827 {
	struct device *dev;
	struct regmap *regmap;
	struct sym827_pdata *pdata;
	struct regulator_dev *rdev;
};

static const struct regmap_config sym827_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/* Default limits measured in millivolts and milliamps */
#define SYM827_MIN_UV		600000
#define SYM827_MAX_UV		1387500
#define SYM827_STEP_UV		12500

#ifdef CONFIG_ARCH_MT8163
__attribute__ ((weak))
void set_slp_spm_deepidle_flags(bool en)
{
	pr_debug("need deep idle porting!\n");
}

static int sym827_hw_component_detect(struct sym827 *chip)
{
	u32 ret = 0;
	u32 ret_rev = 0;
	u32 val = 0;
	u32 val_rev = 0;
	u32 gpio_vsel = 0;

	ret = regmap_read(chip->regmap, SYM827_REG_ID_1, &val);
	val = (val >> 5) & 0x7;
	/* check default SPEC. value */
	if (val != 0x04) {
		pr_err("%s: SYM827_REG_ID_1 wrong: %x\n", __func__, val);
		return -ENODEV;
	}

	ret = regmap_read(chip->regmap, SYM827_REG_ID_1, &val);
	val &= 0xF;
	ret_rev = regmap_read(chip->regmap, SYM827_REG_ID_2, &val_rev);

	gpio_vsel = chip->pdata->gpio_vsel;
	if (!ret && !ret_rev) {
		pr_notice("%s: DIE_ID = %d, DIE_REV = %d\n", __func__, val,
			val_rev);
		if (!val) {
			if (!val_rev) { /* For Failchild */
				if (gpio_vsel > 0) {
					gpio_direction_output(gpio_vsel, 1);
				} else {
					slp_cpu_dvs_en(0);
					set_slp_spm_deepidle_flags(0);
					spm_sodi_cpu_dvs_en(0);
					chip->pdata->gpio_vsel = 0;
				}
			} else { /* For Silergy */
				slp_cpu_dvs_en(0);
				set_slp_spm_deepidle_flags(0);
				spm_sodi_cpu_dvs_en(0);
				chip->pdata->gpio_vsel = 0;
			}
		} else { /* For Silergy */
			if (gpio_vsel > 0) {
				gpio_direction_output(gpio_vsel, 1);
			} else {
				slp_cpu_dvs_en(0);
				set_slp_spm_deepidle_flags(0);
				spm_sodi_cpu_dvs_en(0);
				chip->pdata->gpio_vsel = 0;
			}
		}
	} else {
		pr_err("%s: sym827_read_interface failed %d\n", __func__, ret);
		return ret;
	}
	return 0;
}
#endif
static unsigned int sym827_buck_get_mode(struct regulator_dev *rdev)
{
	struct sym827 *chip = rdev_get_drvdata(rdev);
	unsigned int data;
	int reg = 0, ret, mode;

	if (chip->pdata->gpio_vsel) {
		if (gpio_get_value(chip->pdata->gpio_vsel))
			reg = SYM827_REG_VSEL_1;
		else
			reg = SYM827_REG_VSEL_0;
	}

	ret = regmap_read(chip->regmap, reg, &data);
	if (ret < 0)
		return ret;
	mode = 0;
	switch ((data & SYM827_BUCK_MODE_MASK) >> 6) {
	case SYM827_BUCK_MODE_PWM:
		mode = REGULATOR_MODE_FAST;
		break;
	case SYM827_BUCK_MODE_AUTO:
		mode = REGULATOR_MODE_NORMAL;
		break;
	}

	return mode;
}

static int sym827_buck_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct sym827 *chip = rdev_get_drvdata(rdev);
	int val = 0, reg = 0;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = SYM827_BUCK_MODE_PWM;
		break;
	case REGULATOR_MODE_NORMAL:
		val = SYM827_BUCK_MODE_AUTO;
		break;
	default:
		val = SYM827_BUCK_MODE_AUTO;
		break;
	}

	if (chip->pdata->gpio_vsel) {
		if (gpio_get_value(chip->pdata->gpio_vsel))
			reg = SYM827_REG_VSEL_1;
		else
			reg = SYM827_REG_VSEL_0;
	}

	return regmap_update_bits(chip->regmap, reg, SYM827_BUCK_MODE_MASK,
				  val << SYM827_BUCK_MODE_SHIFT);
}

static int sym827_enable_regmap(struct regulator_dev *rdev)
{
	struct sym827 *chip = rdev_get_drvdata(rdev);
	unsigned int reg = 0;

	if (chip->pdata->gpio_en) {
		gpio_set_value_cansleep(chip->pdata->gpio_en, 1);
		return 1;
	}

	if (chip->pdata->gpio_vsel) {
		if (gpio_get_value(chip->pdata->gpio_vsel))
			reg = SYM827_REG_VSEL_1;
		else
			reg = SYM827_REG_VSEL_0;
	}

	return regmap_update_bits(rdev->regmap, reg, SYM827_BUCK_EN_MASK,
				  1 << SYM827_BUCK_EN_SHIFT);
}

static int sym827_disable_regmap(struct regulator_dev *rdev)
{
	struct sym827 *chip = rdev_get_drvdata(rdev);
	unsigned int reg = 0;

	if (chip->pdata->gpio_en) {
		gpio_set_value_cansleep(chip->pdata->gpio_en, 0);
		return 1;
	}

	if (chip->pdata->gpio_vsel) {
		if (gpio_get_value(chip->pdata->gpio_vsel))
			reg = SYM827_REG_VSEL_1;
		else
			reg = SYM827_REG_VSEL_0;
	}

	return regmap_update_bits(rdev->regmap, reg, SYM827_BUCK_EN_MASK,
				  0 << SYM827_BUCK_EN_SHIFT);
}

static int sym827_is_enabled_regmap(struct regulator_dev *rdev)
{
	struct sym827 *chip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret, reg = 0;

	if (chip->pdata->gpio_en)
		return gpio_get_value_cansleep(chip->pdata->gpio_en);

	if (chip->pdata->gpio_vsel) {
		if (gpio_get_value(chip->pdata->gpio_vsel))
			reg = SYM827_REG_VSEL_1;
		else
			reg = SYM827_REG_VSEL_0;
	}

	ret = regmap_read(rdev->regmap, reg, &val);
	if (ret != 0)
		return ret;

	return (val & SYM827_BUCK_EN_MASK) != 0;
}

static int sym827_set_voltage_sel_regmap(struct regulator_dev *rdev,
					 unsigned int sel)
{
	struct sym827 *chip = rdev_get_drvdata(rdev);
	int ret, reg = 0;

	sel <<= ffs(SYM827_BUCK_NSEL_MASK) - 1;

	if (chip->pdata->gpio_vsel) {
		if (gpio_get_value(chip->pdata->gpio_vsel))
			reg = SYM827_REG_VSEL_1;
		else
			reg = SYM827_REG_VSEL_0;
	}

	ret = regmap_update_bits(rdev->regmap, reg, SYM827_BUCK_NSEL_MASK,
				 sel);

	return ret;
}

static int sym827_get_voltage_sel_regmap(struct regulator_dev *rdev)
{
	struct sym827 *chip = rdev_get_drvdata(rdev);
	unsigned int val;
	int ret, reg = 0;

	if (chip->pdata->gpio_vsel) {
		if (gpio_get_value(chip->pdata->gpio_vsel))
			reg = SYM827_REG_VSEL_1;
		else
			reg = SYM827_REG_VSEL_0;
	}

	ret = regmap_read(rdev->regmap, reg, &val);
	if (ret != 0)
		return ret;

	val &= SYM827_BUCK_NSEL_MASK;
	val >>= ffs(SYM827_BUCK_NSEL_MASK) - 1;

	return val;
}

static struct regulator_ops sym827_buck_ops = {
	.get_mode = sym827_buck_get_mode,
	.set_mode = sym827_buck_set_mode,
	.enable = sym827_enable_regmap,
	.disable = sym827_disable_regmap,
	.is_enabled = sym827_is_enabled_regmap,
	.set_voltage_sel = sym827_set_voltage_sel_regmap,
	.get_voltage_sel = sym827_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.list_voltage = regulator_list_voltage_linear,
};

static struct regulator_desc sym827_reg = {
	.name = "sym827",
	.ops = &sym827_buck_ops,
	.type = REGULATOR_VOLTAGE,
	.id = 0,
	.n_voltages = (SYM827_MAX_UV - SYM827_MIN_UV) / SYM827_STEP_UV + 1,
	.min_uV = SYM827_MIN_UV,
	.uV_step = SYM827_STEP_UV,
	.owner = THIS_MODULE,
};

static int sym827_regulator_init(struct sym827 *chip)
{
	struct regulator_config config = {};
	struct regulation_constraints *c;
	int ret;
	unsigned int data;

	ret = regmap_read(chip->regmap, SYM827_REG_ID_1, &data);
	if (ret < 0 || (!(data & SYM827_VENDOR_ID))) {
		dev_err(chip->dev, "Failed to read SYM827_REG_ID_1 reg: %x\n",
			data);
		goto err;
	}
#ifdef CONFIG_ARCH_MT8163
	ret = sym827_hw_component_detect(chip);
	if (ret)
		return ret;
#endif

	config.init_data = chip->pdata->init_data;
	config.dev = chip->dev;
	config.driver_data = chip;
	config.of_node = chip->pdata->reg_node;
	config.regmap = chip->regmap;

	chip->rdev = regulator_register(&sym827_reg, &config);

	if (IS_ERR(chip->rdev)) {
		dev_err(chip->dev, "Failed to register sym827 regulator\n");
		ret = PTR_ERR(chip->rdev);
		goto err_regulator;
	}

	/* Constrain board-specific capabilities according to what
	 * this driver and the chip itself can actually do.
	 */
	c = chip->rdev->constraints;
	c->valid_modes_mask |= REGULATOR_MODE_NORMAL |
	REGULATOR_MODE_STANDBY | REGULATOR_MODE_FAST;
	c->valid_ops_mask |= REGULATOR_CHANGE_MODE;

	return 0;

err_regulator:
	regulator_unregister(chip->rdev);
err:
	return ret;
}

/*
 * I2C driver interface functions
 */
static const struct i2c_device_id sym827_i2c_id[] = {
	{"sym827-regulator", 0},
	{},
};

#if defined(CONFIG_OF)
static const struct of_device_id sym827_of_match[] = {
	{.compatible = "silergy,sym827-regulator", .data = &sym827_i2c_id[0]},
	{},
};

static struct sym827_pdata *of_get_sym827_platform_data(struct device *dev);
static struct sym827_pdata *of_get_sym827_platform_data(struct device *dev)
{
	struct sym827_pdata *pdata;
	struct device_node *node = dev->of_node;
	int ret;
	const struct of_device_id *match;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return NULL;

	match = of_match_device(of_match_ptr(sym827_of_match), dev);
	if (!match) {
		dev_err(dev, "Error: No device match found\n");
		return NULL;
	}

	pdata->init_data = of_get_regulator_init_data(dev, node, &sym827_reg);

	of_node_put(node);

	if (!pdata->init_data) {
		dev_err(dev, "Failed to parse regulator init data\n");
		return NULL;
	}

	pdata->reg_node = node;

	ret = of_get_named_gpio(node, "vsel-gpio", 0);
	if (ret >= 0) {
		pdata->gpio_vsel = ret;
		ret = gpio_request(pdata->gpio_vsel, "sym827_vsel");
		if (ret)
			dev_err(dev, "Failed to gpio_request %d\n",
				pdata->gpio_vsel);
	}

	ret = of_get_named_gpio(node, "en-gpio", 0);
	if (ret >= 0) {
		pdata->gpio_en = ret;
		ret = gpio_request(pdata->gpio_en, "sym827_gpio_en");
		if (ret)
			dev_err(dev, "Failed to gpio_request %d\n",
				pdata->gpio_en);
		gpio_direction_output(pdata->gpio_en, 1);
	}

	return pdata;
}
#else
static struct sym827_pdata *of_get_sym827_platform_data(struct device *dev)
{
	return NULL;
}
#endif
#ifdef SYM827_TEST_NODE
unsigned int reg_value_sym827;
static ssize_t show_sym827_access(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", reg_value_sym827);
}

static ssize_t store_sym827_access(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	int ret = 0;
	unsigned long reg_value = 0;
	unsigned long reg_address = 0;
	struct sym827 *chip = dev_get_drvdata(dev);

	if (buf != NULL && size != 0) {
		if (size > 4) {
			ret = kstrtoul(strsep((char **)&buf, " "), 16,
				       &reg_address);
			if (ret)
				return ret;
			ret = kstrtoul(buf, 16, &reg_value);
			if (ret)
				return ret;
			ret = regmap_update_bits(chip->regmap, reg_address,
						 0xff, reg_value);
			if (ret < 0)
				dev_err(chip->dev, "Failed update PAGE: %d\n",
					ret);
		} else {
			ret = kstrtoul(buf, 16, &reg_address);
			if (ret)
				return ret;
			ret = regmap_read(chip->regmap, reg_address,
					  &reg_value_sym827);
			if (ret < 0)
				dev_err(chip->dev, "Failed to read reg: %d\n",
					ret);
		}
		dev_notice(chip->dev, "add: %lx, reg: %lx = %x\n",
			reg_address, reg_value, reg_value_sym827);
	}
	return size;
}
static DEVICE_ATTR(access, 0664, show_sym827_access, store_sym827_access);
#endif
static int sym827_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct sym827 *chip;
	int error, ret;

	chip = devm_kzalloc(&i2c->dev, sizeof(struct sym827), GFP_KERNEL);

	chip->dev = &i2c->dev;
	chip->regmap = devm_regmap_init_i2c(i2c, &sym827_regmap_config);
	if (IS_ERR(chip->regmap)) {
		error = PTR_ERR(chip->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n",
			error);
		return error;
	}

	i2c_set_clientdata(i2c, chip);

	chip->pdata = i2c->dev.platform_data;
	if (!chip->pdata && (i2c->dev.of_node))
		chip->pdata = of_get_sym827_platform_data(chip->dev);
	if (!chip->pdata) {
		dev_err(&i2c->dev, "No platform init data supplied\n");
		return -ENODEV;
	}

	ret = sym827_regulator_init(chip);

	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to initialize regulator: %d\n",
			ret);
		return ret;
	}
	dev_notice(&i2c->dev, "%s: done\n", __func__);
#ifdef SYM827_TEST_NODE
	/* Debug sysfs */
	device_create_file(&(i2c->dev), &dev_attr_access);
#endif
	return ret;
}

static int sym827_i2c_remove(struct i2c_client *i2c)
{
	struct sym827 *chip = i2c_get_clientdata(i2c);

	regulator_unregister(chip->rdev);

	return 0;
}

MODULE_DEVICE_TABLE(i2c, sym827_i2c_id);

static struct i2c_driver sym827_regulator_driver = {
	.driver = {
		   .name = "sym827-regulator",
		   .owner = THIS_MODULE,
#if defined(CONFIG_OF)
		   .of_match_table = of_match_ptr(sym827_of_match),
#endif
	},
	.probe = sym827_i2c_probe,
	.remove = sym827_i2c_remove,
	.id_table = sym827_i2c_id,
};

static int __init sym827_init(void)
{
	if (i2c_add_driver(&sym827_regulator_driver) != 0)
		pr_err("failed to register sym827 i2c driver.\n");

	return 0;
}
subsys_initcall(sym827_init);

static void __exit sym827_cleanup(void)
{
	i2c_del_driver(&sym827_regulator_driver);
}
module_exit(sym827_cleanup);

MODULE_DESCRIPTION("Regulator device driver for Silergy sym827");
MODULE_LICENSE("GPL v2");
