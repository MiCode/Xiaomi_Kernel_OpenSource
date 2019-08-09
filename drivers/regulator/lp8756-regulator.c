/*
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Hsin-Hsiung.Wang <hsin-hsiung.wang@mediatek.com>
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
#include <linux/regulator/lp8756-regulator.h>

/* #define LP87565_REGULATOR_MODE */
#define LP87565_DEBUG_SYSFS_NODE

/* lp87565 REGULATOR IDs */
#define lp87565_ID_BUCKA	0
#define lp87565_ID_BUCKB	1

#define lp87565_MAX_REGULATORS 2

enum lp87565_ramp_rate {
	RAMP_RATE_2P5MV,
	RAMP_RATE_5MV,
	RAMP_RATE_10MV,
	RAMP_RATE_20MV,
};

struct lp87565_pdata {
	struct device *dev;
	int num_buck;
	struct regulator_init_data *init_data[lp87565_MAX_REGULATORS];
	int gpio_en[lp87565_MAX_REGULATORS];
	int gpio_vbuck[lp87565_MAX_REGULATORS];
	struct device_node *reg_node[lp87565_MAX_REGULATORS];
};

struct lp87565 {
	struct device *dev;
	struct regmap *regmap;
	struct lp87565_pdata *pdata;
	struct regulator_dev *rdev[lp87565_MAX_REGULATORS];
	int num_regulator;
};

static const struct regmap_config lp87565_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int lp87565_buck_get_vosel(int voltage)
{
	int vosel;

	/*
	 *  500 mv ~  730 mv, step = 10 mv
	 *  730 mv ~ 1400 mv, step =  5 mv
	 * 1400 mv ~ 3360 mv, step = 20 mv
	 */

	if (voltage < LP8756_MIN_MV)
		vosel = 0x00;
	else if (voltage <= LP8756_RANGE0_MAX_MV)
		vosel = ((voltage) - LP8756_MIN_MV) / (LP8756_RANGE0_STEP_MV);
	else if (voltage <= LP8756_RANGE1_MAX_MV)
		vosel = 0x17 + ((voltage) - LP8756_RANGE0_MAX_MV)
					/ (LP8756_RANGE1_STEP_MV);
	else if (voltage <= LP8756_MAX_MV)
		vosel = 0x9D + ((voltage) - LP8756_RANGE1_MAX_MV)
					/ (LP8756_RANGE2_STEP_MV);
	else
		vosel = 0xFF;

	return vosel;
}

static int lp87565_buck_get_voltage(unsigned int vosel)
{
	int voltage;

	if (vosel < 0x17)
		voltage = LP8756_MIN_MV + vosel * LP8756_RANGE0_STEP_MV;
	else if (vosel < 0x9D)
		voltage = LP8756_RANGE0_MAX_MV
				+ (vosel - 0x17) * LP8756_RANGE1_STEP_MV;
	else
		voltage = LP8756_RANGE1_MAX_MV
				+ (vosel - 0x9D) * LP8756_RANGE2_STEP_MV;

	return voltage;
}

static unsigned int lp87565_get_device_id(struct lp87565 *chip)
{
	unsigned int data;
	int ret;

	ret = regmap_read(chip->regmap, LP8756_DEV_REV, &data);
	if (ret < 0)
		return 0;

	data = (data & (LP8756_ALL_LAYER_MASK << LP8756_ALL_LAYER_SHIFT)) >> LP8756_ALL_LAYER_SHIFT;

	return data;
}

#if defined(LP87565_REGULATOR_MODE)
static struct regulator_ops lp87565_buck_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.list_voltage = regulator_list_voltage_linear,
};

#define lp87565_BUCK(_id) \
{\
	.name = #_id,\
	.ops = &lp87565_buck_ops,\
	.type = REGULATOR_VOLTAGE,\
	.id = lp87565_ID_##_id,\
	.n_voltages = (LP8756_MAX_MV - LP8756_MIN_MV) / LP8756_RANGE1_STEP_MV + 1,\
	.min_uV = (LP8756_MIN_MV * 1000),\
	.uV_step = (LP8756_RANGE1_STEP_MV * 1000),\
	.ramp_delay = (LP8756_RAMP_DELAY * 1000), \
	.enable_time = LP8756_TURN_ON_DELAY, \
	.enable_reg = lp87565_ID_##_id,\
	.enable_mask = 1,\
	.vsel_reg = lp87565_ID_##_id * 2,\
	.vsel_mask = 0xFF,\
	.owner = THIS_MODULE,\
}

static struct regulator_desc lp87565_regulators[] = {
	lp87565_BUCK(BUCKA),
	lp87565_BUCK(BUCKB),
};

static int lp87565_regulator_init(struct lp87565 *chip)
{
	struct regulator_config config = { };
	int i, ret;

	chip->num_regulator = chip->pdata->num_buck;

	for (i = 0; i < chip->num_regulator; i++) {
		if (chip->pdata)
			config.init_data = chip->pdata->init_data[i];

		config.dev = chip->dev;
		config.driver_data = chip;
		config.of_node = chip->pdata->reg_node[i];
		config.regmap = chip->regmap;

		chip->rdev[i] = regulator_register(&lp87565_regulators[i], &config);

		if (IS_ERR(chip->rdev[i])) {
			dev_err(chip->dev, "Failed to register lp87565 regulator\n");
			ret = PTR_ERR(chip->rdev[i]);
			goto err_regulator;
		}
	}

	return 0;

 err_regulator:
	while (--i >= 0)
		regulator_unregister(chip->rdev[i]);

	return -1;
}
#endif

/*
 * I2C driver interface functions
 */
static const struct i2c_device_id lp87565_i2c_id[] = {
	{"lp87565-regulator", 0},
	{},
};

#if defined(CONFIG_OF)
static const struct of_device_id lp87565_of_match[] = {
	{.compatible = "ti, lp87565-regulator", .data = &lp87565_i2c_id[0]},
	{},
};

MODULE_DEVICE_TABLE(of, lp87565_of_match);

#if defined(LP87565_REGULATOR_MODE)
static struct of_regulator_match lp87565_matches[] = {
	[lp87565_ID_BUCKA] = {.name = "BUCKA"},
	[lp87565_ID_BUCKB] = {.name = "BUCKB"},
};

static struct lp87565_pdata *of_get_lp87565_platform_data(struct device *dev);
static struct lp87565_pdata *of_get_lp87565_platform_data(struct device *dev)
{
	struct lp87565_pdata *pdata;
	struct device_node *node;
	enum of_gpio_flags flags;
	int num, i, ret;
	int cnt;
	const struct of_device_id *match;

	match = of_match_device(of_match_ptr(lp87565_of_match), dev);
	if (!match) {
		dev_err(dev, "Error: No device match found\n");
		return NULL;
	}

	node = of_get_child_by_name(dev->of_node, "regulators");
	if (!node) {
		dev_err(dev, "regulators node not found\n");
		return NULL;
	}

	num = of_regulator_match(dev, node, lp87565_matches, ARRAY_SIZE(lp87565_matches));

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

	for (i = 0; i < ARRAY_SIZE(lp87565_matches); i++) {
		if (!lp87565_matches[i].init_data)
			continue;

		pdata->init_data[cnt] = lp87565_matches[i].init_data;
		pdata->reg_node[cnt] = lp87565_matches[cnt].of_node;

		ret = of_get_named_gpio_flags(lp87565_matches[cnt].of_node,
										"vbuck-gpio", 0, &flags);
		if (ret >= 0)
			pdata->gpio_vbuck[cnt] = ret;
		ret = of_get_named_gpio_flags(lp87565_matches[cnt].of_node,
										"en-gpio", 0, &flags);
		if (ret >= 0)
			pdata->gpio_en[cnt] = ret;
		cnt++;
	}
	dev_info(dev, "pdata->gpio_en : %d, pdata->gpio_vbuck : %d,\n",
			pdata->gpio_en[0], pdata->gpio_vbuck[1]);

	return pdata;
}
#endif
#endif

#if defined(LP87565_DEBUG_SYSFS_NODE)
unsigned int reg_value_lp87565;
static ssize_t show_lp87565_access(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", reg_value_lp87565);
}

static ssize_t store_lp87565_access(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue;
	char temp_buf[32];
	unsigned long reg_value = 0;
	unsigned long reg_address = 0;
	struct lp87565 *chip = dev_get_drvdata(dev);

	strncpy(temp_buf, buf, sizeof(temp_buf));
	temp_buf[sizeof(temp_buf) - 1] = 0;
	pvalue = temp_buf;

	if (buf != NULL && size != 0) {
		if (size > 4) {
			ret = kstrtoul(strsep(&pvalue, " "), 16, &reg_address);
			if (ret)
				return ret;
			ret = kstrtoul(pvalue, 16, &reg_value);
			if (ret)
				return ret;

			ret = regmap_update_bits(chip->regmap, reg_address, 0xff, reg_value);
			if (ret < 0)
				dev_err(chip->dev, "Failed to update reg: %d\n", ret);

			ret = regmap_read(chip->regmap, reg_address, &reg_value_lp87565);
			if (ret < 0)
				dev_err(chip->dev, "Failed to read CRC: %d\n", ret);

			dev_info(chip->dev, "[%s]reg_addr[0x%lx], val[0x%lx], CRC[0x%x]\r\n",
				__func__, reg_address, reg_value, reg_value_lp87565);
		} else {
			ret = kstrtoul(pvalue, 16, &reg_address);
			if (ret)
				return ret;

			ret = regmap_read(chip->regmap, reg_address, &reg_value_lp87565);
			if (ret < 0)
				dev_err(chip->dev, "Failed to read reg: %d\n", ret);
			dev_info(chip->dev, "ret(%d), reg_addr[0x%lx], reg_value[0x%x]\n",
				ret, reg_address, reg_value_lp87565);
		}
	}
	return size;
}

static ssize_t show_lp87565_buck_0(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	int ret, voltage;
	unsigned int data;
	struct lp87565 *chip = dev_get_drvdata(dev);

	ret = regmap_read(chip->regmap, LP8756_BUCK0_VOUT, &data);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to get buck0 vout: %d\n", ret);
		return sprintf(buf, "-1\n");
	}

	voltage = lp87565_buck_get_voltage(data);
	dev_err(chip->dev, "[%s] vosel[0x%x], vol[%d]\r\n", __func__, data, voltage);

	return sprintf(buf, "%d mv\n", voltage);
}

static ssize_t store_lp87565_buck_0(struct device *dev,
					     struct device_attribute *attr, const char *buf,
					     size_t size)
{
	int ret;
	unsigned int reg_voltage, reg_val;
	struct lp87565 *chip = dev_get_drvdata(dev);

	if (buf != NULL && size != 0) {
		ret = kstrtou32(buf, 0, &reg_voltage);
		if (ret)
			goto error_input;

		if (reg_voltage < 700)
			reg_voltage = 700;
		else if (reg_voltage > 1400)
			reg_voltage = 1400;

		reg_val = lp87565_buck_get_vosel(reg_voltage);
		dev_err(chip->dev, "[%s] voltage[%d], vosel[0x%x]\r\n", __func__, reg_voltage, reg_val);

		ret = regmap_update_bits(chip->regmap, LP8756_BUCK0_VOUT, 0xFF, reg_val);
		if (ret < 0)
			dev_err(chip->dev, "[%s]Failed to set buck0 vol: (%d, %d)\n", __func__, reg_voltage, reg_val);
	}

 error_input:
	return size;
}

static ssize_t show_lp87565_buck_2(struct device *dev,
					    struct device_attribute *attr, char *buf)
{
	int ret, voltage;
	unsigned int data;
	struct lp87565 *chip = dev_get_drvdata(dev);

	ret = regmap_read(chip->regmap, LP8756_BUCK2_VOUT, &data);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to get buck0 vout: %d\n", ret);
		return sprintf(buf, "-1\n");
	}

	voltage = lp87565_buck_get_voltage(data);
	dev_err(chip->dev, "[%s] vosel[0x%x], vol[%d]\r\n", __func__, data, voltage);

	return sprintf(buf, "%d mv\n", voltage);
}

static ssize_t store_lp87565_buck_2(struct device *dev,
					     struct device_attribute *attr, const char *buf,
					     size_t size)
{
	int ret;
	unsigned int reg_voltage, reg_val;
	struct lp87565 *chip = dev_get_drvdata(dev);

	if (buf != NULL && size != 0) {
		ret = kstrtou32(buf, 0, &reg_voltage);
		if (ret)
			goto error_input;

		if (reg_voltage < 700)
			reg_voltage = 700;
		else if (reg_voltage > 1400)
			reg_voltage = 1400;

		reg_val = lp87565_buck_get_vosel(reg_voltage);
		dev_err(chip->dev, "[%s] voltage[%d], vosel[0x%x]\r\n", __func__, reg_voltage, reg_val);

		ret = regmap_update_bits(chip->regmap, LP8756_BUCK2_VOUT, 0xFF, reg_val);
		if (ret < 0)
			dev_err(chip->dev, "[%s]Failed to set buck0 vol: (%d, %d)\n", __func__, reg_voltage, reg_val);
	}

 error_input:
	return size;
}

static DEVICE_ATTR(vcpu_CA72, 0664, show_lp87565_buck_0, store_lp87565_buck_0);
static DEVICE_ATTR(vcpu_CA35, 0664, show_lp87565_buck_2, store_lp87565_buck_2);
static DEVICE_ATTR(access, 0664, show_lp87565_access, store_lp87565_access);
#endif

static int lp87565_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct lp87565 *chip;
	int error, ret = 0;

	chip = devm_kzalloc(&i2c->dev, sizeof(struct lp87565), GFP_KERNEL);

	chip->dev = &i2c->dev;
	chip->regmap = devm_regmap_init_i2c(i2c, &lp87565_regmap_config);
	if (IS_ERR(chip->regmap)) {
		error = PTR_ERR(chip->regmap);
		dev_err(&i2c->dev, "Failed to allocate register map: %d\n", error);
		return error;
	}

	i2c_set_clientdata(i2c, chip);

	#if defined(LP87565_REGULATOR_MODE)
	chip->pdata = i2c->dev.platform_data;
	if (!chip->pdata && (i2c->dev.of_node))
		chip->pdata = of_get_lp87565_platform_data(chip->dev);
	if (!chip->pdata) {
		dev_err(&i2c->dev, "No platform init data supplied\n");
		return -ENODEV;
	}

	ret = lp87565_regulator_init(chip);

	if (ret < 0) {
		dev_err(&i2c->dev, "Failed to initialize regulator: %d\n", ret);
		return ret;
	}
	#endif

	if (lp87565_get_device_id(chip) == 0) {
		dev_err(&i2c->dev, "[%s] hw does not exist\n", __func__);
		return -1;
	}

	dev_info(&i2c->dev, "[%s] Done\n", __func__);

	#if defined(LP87565_DEBUG_SYSFS_NODE)
	/* Debug sysfs */
	device_create_file(&(i2c->dev), &dev_attr_vcpu_CA72);
	device_create_file(&(i2c->dev), &dev_attr_vcpu_CA35);
	device_create_file(&(i2c->dev), &dev_attr_access);
	#endif

	return ret;
}

#if defined(LP87565_REGULATOR_MODE)
static int lp87565_i2c_remove(struct i2c_client *i2c)
{
	struct lp87565 *chip = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < chip->num_regulator; i++)
		regulator_unregister(chip->rdev[i]);

	return 0;
}
#endif

MODULE_DEVICE_TABLE(i2c, lp87565_i2c_id);

static struct i2c_driver lp87565_regulator_driver = {
	.driver = {
		   .name = "lp87565-regulator",
		   .owner = THIS_MODULE,
#if defined(CONFIG_OF)
		   .of_match_table = of_match_ptr(lp87565_of_match),
#endif
		   },
	.probe = lp87565_i2c_probe,
#if defined(LP87565_REGULATOR_MODE)
	.remove = lp87565_i2c_remove,
#endif
	.id_table = lp87565_i2c_id,
};

static int __init lp87565_init(void)
{
	if (i2c_add_driver(&lp87565_regulator_driver) != 0)
		pr_err("Failed to register lp87565 i2c driver.\n");

	return 0;
}
subsys_initcall(lp87565_init);

static void __exit lp87565_cleanup(void)
{
	i2c_del_driver(&lp87565_regulator_driver);
}
module_exit(lp87565_cleanup);

MODULE_DESCRIPTION("Regulator device driver for TI LP87565");
MODULE_LICENSE("GPL v2");
