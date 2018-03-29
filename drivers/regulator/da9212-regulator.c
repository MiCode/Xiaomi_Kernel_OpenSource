/*
* Copyright (c) 2014 MediaTek Inc.
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
#include <linux/regulator/da9212-regulator.h>

#define DA9212_BUCK_MODE_SLEEP	1
#define DA9212_BUCK_MODE_SYNC	2
#define DA9212_BUCK_MODE_AUTO	3

/* DA9212 REGULATOR IDs */
#define DA9212_ID_BUCKA	0
#define DA9212_ID_BUCKB	1

#define DA9212_MAX_REGULATORS 2
#define DA9212_TEST_NODE

enum da9212_ramp_rate {
	RAMP_RATE_2P5MV,
	RAMP_RATE_5MV,
	RAMP_RATE_10MV,
	RAMP_RATE_20MV,
};

struct da9212_pdata {
	struct device *dev;
	int num_buck;
	struct regulator_init_data *init_data[DA9212_MAX_REGULATORS];
	int gpio_en[DA9212_MAX_REGULATORS];
	int gpio_vbuck[DA9212_MAX_REGULATORS];
	struct device_node *reg_node[DA9212_MAX_REGULATORS];
};

struct da9212 {
	struct device *dev;
	struct regmap *regmap;
	struct da9212_pdata *pdata;
	struct regulator_dev *rdev[DA9212_MAX_REGULATORS];
	int num_regulator;
};

static const struct regmap_config da9212_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

/* Default limits measured in millivolts and milliamps */
#define DA9212_MIN_MV		300
#define DA9212_MAX_MV		1570
#define DA9212_STEP_MV		10
#define DA9212_RAMP_DELAY	10
#define DA9212_TURN_ON_DELAY	70

/* Current limits for buck (uA) indices corresponds with register values */
static const int da9212_current_limits[] = {
	2000000, 2200000, 2400000, 2600000, 2800000, 3000000, 3200000, 3400000,
	3600000, 3800000, 4000000, 4200000, 4400000, 4600000, 4800000, 5000000
};

static unsigned int da9212_buck_get_mode(struct regulator_dev *rdev)
{
	int id = rdev_get_id(rdev);
	struct da9212 *chip = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret, mode;

	ret = regmap_read(chip->regmap, DA9212_REG_BUCKA_CONF + id, &data);
	if (ret < 0)
		return ret;
	mode = 0;
	switch (data & DA9212_BUCKA_MODE_MASK) {
	case DA9212_BUCK_MODE_SYNC:
		mode = REGULATOR_MODE_FAST;
		break;
	case DA9212_BUCK_MODE_AUTO:
		mode = REGULATOR_MODE_NORMAL;
		break;
	case DA9212_BUCK_MODE_SLEEP:
		mode = REGULATOR_MODE_STANDBY;
		break;
	}

	return mode;
}

static int da9212_buck_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	int id = rdev_get_id(rdev);
	struct da9212 *chip = rdev_get_drvdata(rdev);
	int val;

	val = 0;
	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = DA9212_BUCK_MODE_SYNC;
		break;
	case REGULATOR_MODE_NORMAL:
		val = DA9212_BUCK_MODE_AUTO;
		break;
	case REGULATOR_MODE_STANDBY:
		val = DA9212_BUCK_MODE_SLEEP;
		break;
	}

	return regmap_update_bits(chip->regmap, DA9212_REG_BUCKA_CONF + id, 0x03, val);
}

static int da9212_get_current_limit(struct regulator_dev *rdev)
{
	int id = rdev_get_id(rdev);
	struct da9212 *chip = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;

	ret = regmap_read(chip->regmap, DA9212_REG_BUCK_ILIM, &data);
	if (ret < 0)
		return ret;

	/* select one of 16 values: 0000 (2000mA) to 1111 (5000mA) */
	data = (data >> id * 4) & 0x0F;
	return da9212_current_limits[data];
}

static int da9212_set_ramp_delay(struct regulator_dev *rdev, int ramp_delay)
{
	int id = rdev_get_id(rdev);
	unsigned int ramp_value = DA9212_RAMP_DELAY;
	unsigned int mask;

	switch (ramp_delay) {
	case 1 ... 2500:
		ramp_value = RAMP_RATE_2P5MV;
		break;
	case 2501 ... 5000:
		ramp_value = RAMP_RATE_5MV;
		break;
	case 5001 ... 10000:
		ramp_value = RAMP_RATE_10MV;
		break;
	case 10001 ... 20000:
		ramp_value = RAMP_RATE_20MV;
		break;
	default:
		return -EINVAL;
	}
	mask = id ? DA9212_SLEW_RATE_B_MASK : DA9212_SLEW_RATE_A_MASK;
	return regmap_update_bits(rdev->regmap, rdev->desc->enable_reg, mask, ramp_value << 6);
}

static struct regulator_ops da9212_buck_ops = {
	.get_mode = da9212_buck_get_mode,
	.set_mode = da9212_buck_set_mode,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.set_ramp_delay = da9212_set_ramp_delay,
	.list_voltage = regulator_list_voltage_linear,
	.get_current_limit = da9212_get_current_limit,
};

#define DA9212_BUCK(_id) \
{\
	.name = #_id,\
	.ops = &da9212_buck_ops,\
	.type = REGULATOR_VOLTAGE,\
	.id = DA9212_ID_##_id,\
	.n_voltages = (DA9212_MAX_MV - DA9212_MIN_MV) / DA9212_STEP_MV + 1,\
	.min_uV = (DA9212_MIN_MV * 1000),\
	.uV_step = (DA9212_STEP_MV * 1000),\
	.ramp_delay = (DA9212_RAMP_DELAY * 1000), \
	.enable_time = DA9212_TURN_ON_DELAY, \
	.enable_reg = DA9212_REG_BUCKA_CONT + DA9212_ID_##_id,\
	.enable_mask = 1,\
	.vsel_reg = DA9212_REG_VBUCKA_A + DA9212_ID_##_id * 2,\
	.vsel_mask = DA9212_VBUCK_MASK,\
	.owner = THIS_MODULE,\
}

static struct regulator_desc da9212_regulators[] = {
	DA9212_BUCK(BUCKA),
	DA9212_BUCK(BUCKB),
};

static int da9212_regulator_init(struct da9212 *chip)
{
	struct regulator_config config = { };
	int i, ret;
	unsigned int data;
	struct regulation_constraints *c;

	ret = regmap_update_bits(chip->regmap, DA9212_REG_PAGE_CON,
				 DA9212_REG_PAGE_MASK, DA9212_REG_PAGE2);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to update PAGE reg: %d\n", ret);
		goto err;
	}

	ret = regmap_read(chip->regmap, DA9212_REG_INTERFACE, &data);
	if (ret < 0 || ((data >> 4) != 0xD)) {
		ret = regmap_update_bits(chip->regmap, DA9212_REG_PAGE_CON,
					 DA9212_REG_PAGE_MASK, DA9212_REG_PAGE0);
		dev_err(chip->dev, "Failed to read CONTROL_E reg: %d\n", data);
		goto err;
	}

	chip->num_regulator = chip->pdata->num_buck;

	ret = regmap_update_bits(chip->regmap, DA9212_REG_PAGE_CON,
				 DA9212_REG_PAGE_MASK, DA9212_REG_PAGE0);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to update PAGE reg: %d\n", ret);
		goto err;
	}

	for (i = 0; i < chip->num_regulator; i++) {
		if (chip->pdata->init_data[i])
			config.init_data = chip->pdata->init_data[i];
		config.ena_gpio = 0; /* initialize */
		if (chip->pdata->gpio_en[i]) {
			config.ena_gpio = chip->pdata->gpio_en[i];
			config.ena_gpio_flags |= GPIOF_OUT_INIT_HIGH;
		}
		dev_err(chip->dev, "config.ena_gpio: %d\n", config.ena_gpio);
		config.dev = chip->dev;
		config.driver_data = chip;
		config.of_node = chip->pdata->reg_node[i];
		config.regmap = chip->regmap;

		chip->rdev[i] = regulator_register(&da9212_regulators[i], &config);
		if (IS_ERR(chip->rdev[i])) {
			dev_err(chip->dev, "Failed to register DA9212 regulator\n");
			ret = PTR_ERR(chip->rdev[i]);
			goto err_regulator;
		}
		/* Constrain board-specific capabilities according to what
		 * this driver and the chip itself can actually do.
		 */
		c = chip->rdev[i]->constraints;
		c->valid_modes_mask |= REGULATOR_MODE_NORMAL |
		REGULATOR_MODE_STANDBY | REGULATOR_MODE_FAST;
		c->valid_ops_mask |= REGULATOR_CHANGE_MODE;
	}

	return 0;

 err_regulator:
	while (--i >= 0)
		regulator_unregister(chip->rdev[i]);
 err:
	return ret;
}

/*
 * I2C driver interface functions
 */
static const struct i2c_device_id da9212_i2c_id[] = {
	{"da9212-regulator", 0},
	{},
};

#if defined(CONFIG_OF)
static const struct of_device_id da9212_of_match[] = {
	{.compatible = "dlg,da9212-regulator", .data = &da9212_i2c_id[0]},
	{},
};

MODULE_DEVICE_TABLE(of, da9212_of_match);
static struct of_regulator_match da9212_matches[] = {
	[DA9212_ID_BUCKA] = {.name = "BUCKA"},
	[DA9212_ID_BUCKB] = {.name = "BUCKB"},
};

static struct da9212_pdata *of_get_da9212_platform_data(struct device *dev);
static struct da9212_pdata *of_get_da9212_platform_data(struct device *dev)
{
	struct da9212_pdata *pdata;
	struct device_node *node;
	enum of_gpio_flags flags;
	int num, i, ret;
	int cnt;
	const struct of_device_id *match;

	match = of_match_device(of_match_ptr(da9212_of_match), dev);
	if (!match) {
		dev_err(dev, "Error: No device match found\n");
		return NULL;
	}

	node = of_get_child_by_name(dev->of_node, "regulators");
	if (!node) {
		dev_err(dev, "regulators node not found\n");
		return NULL;
	}

	num = of_regulator_match(dev, node, da9212_matches, ARRAY_SIZE(da9212_matches));

	of_node_put(node);

	if (num < 0) {
		dev_err(dev, "Failed to match regulators\n");
		return NULL;
	}

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return ERR_PTR(-ENOMEM);

	pdata->num_buck = num;
	cnt = 0;

	for (i = 0; i < ARRAY_SIZE(da9212_matches); i++) {
		if (!da9212_matches[i].init_data)
			continue;

		pdata->init_data[cnt] = da9212_matches[i].init_data;
		pdata->reg_node[cnt] = da9212_matches[cnt].of_node;

		ret = of_get_named_gpio_flags(da9212_matches[cnt].of_node,
										"vbuck-gpio", 0, &flags);
		if (ret >= 0)
			pdata->gpio_vbuck[cnt] = ret;
		ret = of_get_named_gpio_flags(da9212_matches[cnt].of_node,
										"en-gpio", 0, &flags);
		if (ret >= 0)
			pdata->gpio_en[cnt] = ret;
		cnt++;
	}
	dev_info(dev, "pdata->gpio_en : %d, pdata->gpio_vbuck : %d,\n",
			pdata->gpio_en[0], pdata->gpio_vbuck[1]);

	return pdata;
}
#else
static struct da9212 *of_get_da9212_platform_data(struct device *dev)
{
	return NULL;
}
#endif
#ifdef DA9212_TEST_NODE
unsigned int reg_value_da9212 = 0;
static ssize_t show_da9212_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", reg_value_da9212);
}

static ssize_t store_da9212_access(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue;
	char temp_buf[32];
	unsigned long reg_value = 0;
	unsigned long reg_address = 0;
	struct da9212 *chip = dev_get_drvdata(dev);

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	if (size != 0) {
		if (size > 4) {
			ret = kstrtoul(strsep(&pvalue, " "), 16, &reg_address);
			if (ret)
				return ret;
			ret = kstrtoul(pvalue, 16, &reg_value);
			if (ret)
				return ret;
			ret = regmap_update_bits(chip->regmap, reg_address, 0xff, reg_value);
			if (ret < 0)
				dev_err(chip->dev, "Failed to update PAGE reg: %d\n", ret);

			/* restore to page 0,1 */
			ret = regmap_update_bits(chip->regmap, DA9212_REG_PAGE_CON,
						 DA9212_REG_PAGE_MASK, DA9212_REG_PAGE0);
			if (ret < 0)
				dev_err(chip->dev, "Failed to update PAGE reg: %d\n", ret);
		} else {
			ret = kstrtoul(pvalue, 16, &reg_address);
			if (ret)
				return ret;

			ret = regmap_read(chip->regmap, reg_address, &reg_value_da9212);
			if (ret < 0)
				dev_err(chip->dev, "Failed to read reg: %d\n", ret);

			/* restore to page 0,1 */
			ret = regmap_update_bits(chip->regmap, DA9212_REG_PAGE_CON,
						 DA9212_REG_PAGE_MASK, DA9212_REG_PAGE0);
			if (ret < 0)
				dev_err(chip->dev, "Failed to update PAGE reg: %d\n", ret);
		}
	}
	return size;
}

static ssize_t show_da9212_buck_voltage_cpu(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	int ret;
	struct regulator *reg = devm_regulator_get(dev, "VBUCKA");

	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(dev, "Failed to request VBUCKA: %d\n", ret);
		return 0;
	}
	ret = sprintf(buf, "%d\n", regulator_get_voltage(reg));

	devm_regulator_put(reg);
	return ret;
}

static ssize_t store_da9212_buck_voltage_cpu(struct device *dev,
					     struct device_attribute *attr, const char *buf,
					     size_t size)
{
	int ret;
	unsigned int reg_value = 0;
	struct regulator *reg = devm_regulator_get(dev, "VBUCKA");

	ret = 0;
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(dev, "Failed to request VBUCKA: %d\n", ret);
		return ret;
	}
	if (buf != NULL && size != 0) {
		ret = kstrtou32(buf, 0, &reg_value);
		if (ret)
			goto error_input;
		/* set voltage */
		ret = regulator_set_voltage(reg, reg_value, reg_value);
		if (ret != 0)
			dev_err(dev, "Failed to set reg voltage: %d\n", reg_value);
	}
 error_input:
	devm_regulator_put(reg);
	return size;
}

static ssize_t show_da9212_buck_voltage_gpu(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	int ret;
	struct regulator *reg = devm_regulator_get(dev, "VBUCKB");

	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(dev, "Failed to request VBUCKB: %d\n", ret);
		return 0;
	}
	ret = sprintf(buf, "%d\n", regulator_get_voltage(reg));

	devm_regulator_put(reg);
	return ret;
}

static ssize_t store_da9212_buck_voltage_gpu(struct device *dev,
					     struct device_attribute *attr, const char *buf,
					     size_t size)
{
	int ret;
	unsigned int reg_value = 0;
	struct regulator *reg = devm_regulator_get(dev, "VBUCKB");

	ret = 0;
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(dev, "Failed to request VBUCKB: %d\n", ret);
		return ret;
	}
	if (buf != NULL && size != 0) {
		ret = kstrtou32(buf, 0, &reg_value);
		if (ret)
			goto error_input;
		/* set voltage */
		ret = regulator_set_voltage(reg, reg_value, reg_value);
		if (ret != 0)
			dev_err(dev, "Failed to set reg voltage: %d\n", reg_value);
	}
 error_input:
	devm_regulator_put(reg);
	return size;
}

static DEVICE_ATTR(voltage_cpu, 0664, show_da9212_buck_voltage_cpu, store_da9212_buck_voltage_cpu);
static DEVICE_ATTR(voltage_gpu, 0664, show_da9212_buck_voltage_gpu, store_da9212_buck_voltage_gpu);
static DEVICE_ATTR(access, 0664, show_da9212_access, store_da9212_access);
#endif
static int da9212_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct da9212 *chip;
	int error, ret;

	chip = devm_kzalloc(&i2c->dev, sizeof(struct da9212), GFP_KERNEL);

	chip->dev = &i2c->dev;
	chip->regmap = devm_regmap_init_i2c(i2c, &da9212_regmap_config);
	if (IS_ERR(chip->regmap)) {
		error = PTR_ERR(chip->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n", error);
		return error;
	}

	i2c_set_clientdata(i2c, chip);

	chip->pdata = i2c->dev.platform_data;
	if (!chip->pdata && (i2c->dev.of_node))
		chip->pdata = of_get_da9212_platform_data(chip->dev);
	if (!chip->pdata) {
		dev_err(&i2c->dev, "No platform init data supplied\n");
		return -ENODEV;
	}

	ret = da9212_regulator_init(chip);

	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to initialize regulator: %d\n", ret);
		return ret;
	}
	dev_err(&i2c->dev, "da9212_i2c_probe: done...\n");
#ifdef DA9212_TEST_NODE
	/* Debug sysfs */
	device_create_file(&(i2c->dev), &dev_attr_voltage_cpu);
	device_create_file(&(i2c->dev), &dev_attr_voltage_gpu);
	device_create_file(&(i2c->dev), &dev_attr_access);
#endif
	return ret;
}

static int da9212_i2c_remove(struct i2c_client *i2c)
{
	struct da9212 *chip = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < chip->num_regulator; i++)
		regulator_unregister(chip->rdev[i]);

	return 0;
}

MODULE_DEVICE_TABLE(i2c, da9212_i2c_id);

static struct i2c_driver da9212_regulator_driver = {
	.driver = {
		   .name = "da9212-regulator",
		   .owner = THIS_MODULE,
#if defined(CONFIG_OF)
		   .of_match_table = of_match_ptr(da9212_of_match),
#endif
		   },
	.probe = da9212_i2c_probe,
	.remove = da9212_i2c_remove,
	.id_table = da9212_i2c_id,
};

static int __init da9212_init(void)
{
	if (i2c_add_driver(&da9212_regulator_driver) != 0)
		pr_err("failed to register da9212 i2c driver.\n");
	else
		pr_err("Success to register da9212-regulator.\n");
	return 0;
}
subsys_initcall(da9212_init);

static void __exit da9212_cleanup(void)
{
	i2c_del_driver(&da9212_regulator_driver);
}
module_exit(da9212_cleanup);

MODULE_DESCRIPTION("Regulator device driver for Dialog DA9212");
MODULE_LICENSE("GPL v2");
