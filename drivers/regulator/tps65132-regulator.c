/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/bitops.h>
#include <linux/types.h>

struct tps65132_regulator {
	struct regulator_init_data	*init_data;
	struct regulator_dev		*rdev;
	struct device_node		*node;
	struct regulator_desc		rdesc;
	struct tps65132_chip		*chip;
	const char			*name;
	u8				vol_reg;
	u8				dischg_bit_pos;
	bool				dischg_en;
	u8				ctrl_reg;
	u8				wrt_en_bit_pos;
	u8				read_eeprom_bit_pos;
	int				en_gpio;
	enum				of_gpio_flags gpio_flags;
	bool				is_enabled;
	int				curr_uV;
	u8				vol_set_val;
	bool				vol_set_postpone;
};

struct tps65132_chip {
	struct tps65132_regulator	*vreg;
	struct regulator		*i2c_pwr;
	struct regmap			*regmap;
	struct device			*dev;
	u8				num_regulators;
	u8				apps_cfg;
	u8				apps_dischg_reg;
	u8				apps_cfg_bit_pos;
	u8				apps_dischg_val;
	bool				apps_dischg_cfg_postpone;
	bool				en_gpio_lpm;
};

#define TPS65132_REG_VPOS		0x00
#define TPS65132_REG_VNEG		0x01
#define TPS65132_VOLTAGE_MASK		0x1f
#define TPS65132_REG_APPS_DISCHARGE	0x03
#define TPS65132_DISCHARGE_NEG_BIT	0
#define TPS65132_DISCHARGE_POS_BIT	1
#define TPS65132_APPSCFG_BIT		6
#define TPS65132_REG_CTRL		0xff
#define TPS65132_WRITE_EN_BIT		7

#define TPS65132_VOLTAGE_MIN	4000000
#define TPS65132_VOLTAGE_MAX	6000000
#define TPS65132_VOLTAGE_STEP	100000
#define TPS65132_VOLTAGE_LEVELS	\
	((TPS65132_VOLTAGE_MAX - TPS65132_VOLTAGE_MIN) \
	/ TPS65132_VOLTAGE_STEP + 1)
#define TPS65132_CTRL_READ_DAC		0
#define TPS65132_CTRL_READ_EEPROM	1

#define TPS65132_NEG_TABLET_CURR_LIMIT_UA	80000
#define TPS65132_NEG_SMARTPHONE_CURR_LIMIT_UA	40000
#define TPS65132_POS_CURR_LIMIT_UA		200000

#define I2C_VOLTAGE_LEVEL	1800000

enum {
	TPS65132_POSITIVE_BOOST = 0,
	TPS65132_NEGATIVE_BOOST,
};

static struct of_regulator_match tps65132_reg_matches[] = {
	{ .name = "pos-boost", .driver_data = (void *)TPS65132_POSITIVE_BOOST },
	{ .name = "neg-boost", .driver_data = (void *)TPS65132_NEGATIVE_BOOST },
};

static int tps65132_regulator_disable(struct regulator_dev *rdev)
{
	struct tps65132_regulator *vreg = rdev_get_drvdata(rdev);
	struct tps65132_chip *chip = vreg->chip;
	printk("tps65132_regulator_disable\n");
	printk("vreg->en_gpio is %d\n", vreg->en_gpio);
	if (chip->en_gpio_lpm) {
		gpio_direction_output(vreg->en_gpio, 0);
	} else{
		gpio_set_value_cansleep(vreg->en_gpio,
			vreg->gpio_flags & OF_GPIO_ACTIVE_LOW ? 1 : 0);
}
	vreg->is_enabled = false;

	return 0;
}

static int tps65132_regulator_enable(struct regulator_dev *rdev)
{
	struct tps65132_regulator *vreg = rdev_get_drvdata(rdev);
	struct tps65132_chip *chip = vreg->chip;
	int rc;
	printk("tps65132_regulator_enable\n");
	printk("vreg->en_gpio is %d\n", vreg->en_gpio);
	if (chip->en_gpio_lpm) {
		gpio_direction_output(vreg->en_gpio, 1);
}
	else {
		gpio_set_value_cansleep(vreg->en_gpio,
			vreg->gpio_flags & OF_GPIO_ACTIVE_LOW ? 0 : 1);
}
	vreg->is_enabled = true;

	if (chip->apps_dischg_cfg_postpone) {
		rc = regmap_write(chip->regmap, chip->apps_dischg_reg,
						chip->apps_dischg_val);
		if (rc) {
			pr_err("apps_dischg set failed, rc = %d\n", rc);
			return rc;
		}
		chip->apps_dischg_cfg_postpone = false;
	}

	if (vreg->vol_set_postpone) {
		rc = regmap_write(rdev->regmap, vreg->vol_reg,
					vreg->vol_set_val);

		if (rc) {
			pr_err("set voltage failed, rc = %d\n", rc);
			return rc;
		}
		vreg->vol_set_postpone = false;
	}

	return 0;
}

static int tps65132_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct tps65132_regulator *vreg = rdev_get_drvdata(rdev);
	int rc, val;
		 return 0;

	if (!rdev->regmap) {
		pr_err("regmap not found\n");
		return -EINVAL;
	}
	if (!vreg->is_enabled)
		return vreg->curr_uV;

	rc = regmap_write(rdev->regmap, vreg->ctrl_reg, TPS65132_CTRL_READ_DAC);
	if (rc) {
		pr_err("failed to write reg %d, rc = %d\n", vreg->ctrl_reg, rc);
		return rc;
	}

	rc = regmap_read(rdev->regmap, vreg->vol_reg, &val);
	if (rc) {
		pr_err("read reg %d failed, rc = %d\n", vreg->vol_reg, rc);
		return rc;
	} else {
		vreg->curr_uV = (val & TPS65132_VOLTAGE_MASK) *
			TPS65132_VOLTAGE_STEP + TPS65132_VOLTAGE_MIN;
	}

	return vreg->curr_uV;
}

static int tps65132_regulator_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct tps65132_regulator *vreg = rdev_get_drvdata(rdev);
	int val, new_uV, rc;

	if (!rdev->regmap) {
		pr_err("regmap not found\n");
		return -EINVAL;
	}

	val = DIV_ROUND_UP(min_uV - TPS65132_VOLTAGE_MIN,
				TPS65132_VOLTAGE_STEP);
	val = val & TPS65132_VOLTAGE_MASK;
	new_uV = TPS65132_VOLTAGE_MIN + (val * TPS65132_VOLTAGE_STEP);
	if (new_uV > max_uV) {
		pr_err("failed to set voltage (%d %d)\n", min_uV, max_uV);
		return -EINVAL;
	}
	if (!vreg->is_enabled) {
		vreg->vol_set_val = val;
		vreg->vol_set_postpone = true;
	} else {
		rc = regmap_write(rdev->regmap, vreg->vol_reg, val);
		if (rc) {
			pr_err("failed to write reg %d, rc = %d\n",
						vreg->vol_reg, rc);
			return rc;
		}
	}
	vreg->curr_uV = new_uV;

	*selector = val;

	return 0;
}

static int tps65132_regulator_list_voltage(struct regulator_dev *rdev,
							unsigned selector)
{
	if (selector >= TPS65132_VOLTAGE_LEVELS)
		return 0;

	return selector * TPS65132_VOLTAGE_STEP + TPS65132_VOLTAGE_MIN;
}

static int tps65132_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct tps65132_regulator *vreg = rdev_get_drvdata(rdev);

	return vreg->is_enabled ? 1 : 0;
}

static struct regulator_ops tps65132_ops = {
	.set_voltage = tps65132_regulator_set_voltage,
	.get_voltage = tps65132_regulator_get_voltage,
	.list_voltage = tps65132_regulator_list_voltage,
	.enable = tps65132_regulator_enable,
	.disable = tps65132_regulator_disable,
	.is_enabled = tps65132_regulator_is_enabled,
};

static int tps65132_regulator_gpio_init(struct tps65132_chip *chip)
{
	struct tps65132_regulator *vreg;
	u32 gpio;
	enum of_gpio_flags flags;
	int state;
	int i, rc = 0;

	for (i = 0; i < chip->num_regulators; i++) {
		vreg = &chip->vreg[i];
		gpio = vreg->en_gpio;
		flags = vreg->gpio_flags;
		if (gpio_is_valid(gpio)) {
			rc = devm_gpio_request(chip->dev, gpio, vreg->name);
			if (rc < 0) {
				pr_err("gpio %d request failed, rc = %d\n",
								gpio, rc);
				return rc;
			}
			state = gpio_get_value_cansleep(gpio);
			if (state < 0) {
				pr_err("gpio %d: get value failed, rc = %d\n",
						gpio, state);
				return state;
			}
			rc = gpio_direction_output(gpio, state);
			if (rc < 0) {
				pr_err("gpio %d set output failed, rc = %d\n",
								gpio, rc);
				return rc;
			}
			if (((flags & OF_GPIO_ACTIVE_LOW) && (state == 0)) ||
				(!(flags & OF_GPIO_ACTIVE_LOW) && (state == 1)))
				vreg->is_enabled = true;
		} else {
			pr_err("gpio %d is invalid for %s EN-pin\n",
						gpio, vreg->name);
			return -EINVAL;
		}
	}

	return rc;
}

static int tps65132_regulator_apps_dischg_config(struct tps65132_chip *chip)
{
	struct tps65132_regulator *vreg;
	u8 value = 0;
	bool online = false;
	int i, rc = 0;

	if (chip->apps_cfg > 0)
		value = BIT(chip->apps_cfg_bit_pos);

	for (i = 0; i < chip->num_regulators; i++) {
		vreg = &chip->vreg[i];
		if (vreg->dischg_en)
			value |= BIT(vreg->dischg_bit_pos);
		if (vreg->is_enabled)
			online = true;
	}
	if (online) {
		rc = regmap_write(chip->regmap, chip->apps_dischg_reg, value);
		if (rc)
			pr_err("write reg %d failed, rc = %d\n",
					chip->apps_dischg_reg, rc);
	} else {
		chip->apps_dischg_cfg_postpone = true;
		chip->apps_dischg_val = value;
	}

	return 0;
}

static int tps65132_regulator_hw_init(struct tps65132_chip *chip)
{
	int rc;
	struct regulator *i2c_pwr = chip->i2c_pwr;

	rc = tps65132_regulator_gpio_init(chip);
	if (rc) {
		pr_err("gpios initialize failed, rc = %d\n", rc);
		return rc;
	}

	if (i2c_pwr) {
		if (regulator_count_voltages(i2c_pwr) > 0) {
			rc = regulator_set_voltage(i2c_pwr,
				I2C_VOLTAGE_LEVEL, I2C_VOLTAGE_LEVEL);
			if (rc < 0) {
				pr_err("set i2c-pwr voltage failed, rc = %d\n",
									rc);
				return rc;
			}
		}
		rc = regulator_enable(i2c_pwr);
		if (rc) {
			pr_err("enable i2c-pwr voltage failed, rc = %d\n", rc);
			return rc;
		}
	}

	rc = tps65132_regulator_apps_dischg_config(chip);
	if (rc)
		pr_err("appscfg set failed, rc = %d\n", rc);

	return rc;
}

static int tps65132_parse_dt(struct tps65132_chip *chip,
				struct i2c_client *client)
{
	struct device_node *node;
	struct of_regulator_match *match;
	int type, i, rc;
	u32 current_limit;

	if (!client->dev.of_node) {
		pr_err("device node missing\n");
		return -EINVAL;
	}

	node = of_find_node_by_name(client->dev.of_node, "regulators");
	if (!node) {
		pr_err("get regulators node failed\n");
		return -EINVAL;
	}

	rc = of_regulator_match(&client->dev, node, tps65132_reg_matches,
					ARRAY_SIZE(tps65132_reg_matches));
	if (rc < 0) {
		pr_err("regulator match failed, rc = %d\n", rc);
		return rc;
	}

	chip->num_regulators = rc;

	chip->vreg = devm_kzalloc(&client->dev, chip->num_regulators *
					sizeof(struct tps65132_regulator),
					GFP_KERNEL);
	if (!chip->vreg) {
		pr_err("memory allocation failed for vreg\n");
		return -ENOMEM;
	}
	if (of_find_property(client->dev.of_node, "i2c-pwr-supply", NULL)) {
		chip->i2c_pwr = devm_regulator_get(&client->dev, "i2c-pwr");
		if (IS_ERR_OR_NULL(chip->i2c_pwr)) {
			rc = PTR_RET(chip->i2c_pwr);
			if (rc != EPROBE_DEFER)
				pr_err("get i2c_pwr failed, rc = %d\n", rc);
			return rc;
		}
	}
	chip->en_gpio_lpm = of_property_read_bool(client->dev.of_node,
						"ti,en-gpio-lpm");

	for (i = 0; i < chip->num_regulators; i++) {
		match = &tps65132_reg_matches[i];
		if (!match->init_data)
			continue;
		match->init_data->constraints.input_uV =
				match->init_data->constraints.max_uV;
		match->init_data->constraints.valid_ops_mask =
					REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS;

		chip->vreg[i].init_data = match->init_data;
		chip->vreg[i].node = match->of_node;
		chip->vreg[i].chip = chip;
		chip->vreg[i].name = match->init_data->constraints.name;
		chip->vreg[i].ctrl_reg = TPS65132_REG_CTRL;
		chip->vreg[i].wrt_en_bit_pos = TPS65132_WRITE_EN_BIT;
		chip->vreg[i].read_eeprom_bit_pos =
						TPS65132_WRITE_EN_BIT;
		chip->vreg[i].dischg_en = of_property_read_bool(
					match->of_node, "ti,discharge-enable");
		rc = of_property_read_u32(match->of_node, "ti,enable-time",
					&chip->vreg[i].rdesc.enable_time);
		if (rc < 0) {
			pr_debug("ti,enable-time read failed, rc = %d\n", rc);
			chip->vreg[i].rdesc.enable_time = 800;
		}
		rc = of_property_read_u32(match->of_node, "ti,current-limit",
							&current_limit);
		type = (uintptr_t)match->driver_data;
		if (type == TPS65132_POSITIVE_BOOST) {
			chip->vreg[i].vol_reg = TPS65132_REG_VPOS;
			chip->vreg[i].dischg_bit_pos =
					TPS65132_DISCHARGE_POS_BIT;
			if (!rc && (current_limit !=
					TPS65132_POS_CURR_LIMIT_UA)) {
				pr_err("current limit %duA is invalid for postive boost\n",
							current_limit);
				return -EINVAL;
			}
		} else if (type == TPS65132_NEGATIVE_BOOST) {
			chip->vreg[i].vol_reg = TPS65132_REG_VNEG;
			chip->vreg[i].dischg_bit_pos =
					TPS65132_DISCHARGE_NEG_BIT;
			if (!rc && (current_limit !=
				TPS65132_NEG_TABLET_CURR_LIMIT_UA) &&
				(current_limit !=
				 TPS65132_NEG_SMARTPHONE_CURR_LIMIT_UA)) {
				pr_err("current limit %duA is invalid for negative boost\n",
							current_limit);
				return -EINVAL;
			} else if (!rc && (current_limit ==
					TPS65132_NEG_TABLET_CURR_LIMIT_UA)) {
				chip->apps_cfg = 1;
			}
		} else {
			pr_err("unknown regulator type: %d\n", type);
			return -EINVAL;
		}
		rc = 0;
		chip->vreg[i].en_gpio = of_get_named_gpio_flags(
					match->of_node, "ti,en-gpio", 0,
					&chip->vreg[i].gpio_flags);
		if (chip->vreg[i].en_gpio < 0) {
			pr_err("get ti,en-gpio failed, rc = %d\n",
					chip->vreg[i].en_gpio);
			return chip->vreg[i].en_gpio;
		}
	}
	chip->apps_dischg_reg = TPS65132_REG_APPS_DISCHARGE;
	chip->apps_cfg_bit_pos = TPS65132_APPSCFG_BIT;

	return rc;
}

static struct regmap_config tps65132_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int tps65132_regulator_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct tps65132_chip *chip;
	struct regulator_config config = {};
	struct regulator_desc *rdesc;
	int i, j, rc;
	chip = devm_kzalloc(&client->dev, sizeof(struct tps65132_chip),
							GFP_KERNEL);
	if (!chip) {
		pr_err("memory allocation failed for tps65132_chip\n");
		return -ENOMEM;
	}
	rc = tps65132_parse_dt(chip, client);
	if (rc) {
		pr_err("parse device tree failed for tps65132, rc = %d\n",
								rc);
		return rc;
	}
	chip->regmap = devm_regmap_init_i2c(client, &tps65132_regmap_config);
	if (IS_ERR(chip->regmap)) {
		pr_err("init regmap failed for tps65132, rc = %ld\n",
						PTR_ERR(chip->regmap));
		return PTR_ERR(chip->regmap);
	}
	chip->dev = &client->dev;

	rc = tps65132_regulator_hw_init(chip);
	if (rc < 0) {
		pr_err("hardware init failed for tps65132, rc = %d\n", rc);
		return rc;
	}
	i2c_set_clientdata(client, chip);

	for (i = 0; i < chip->num_regulators; i++) {
		config.dev = &client->dev;
		config.init_data = chip->vreg[i].init_data;
		config.regmap = chip->regmap;
		config.driver_data = &chip->vreg[i];
		config.of_node = chip->vreg[i].node;

		rdesc = &chip->vreg[i].rdesc;
		rdesc->name = chip->vreg[i].name;
		rdesc->type = REGULATOR_VOLTAGE;
		rdesc->owner = THIS_MODULE;
		rdesc->n_voltages = TPS65132_VOLTAGE_LEVELS;
		if (of_get_property(client->dev.of_node, "vin-supply", NULL))
			rdesc->supply_name = "vin";
		rdesc->ops = &tps65132_ops;
		chip->vreg[i].rdev = regulator_register(rdesc, &config);
		if (IS_ERR(chip->vreg[i].rdev)) {
			pr_err("regulator register failed, rc = %ld\n",
					PTR_ERR(chip->vreg[i].rdev));
			for (j = i - 1; j >= 0; j--)
				regulator_unregister(chip->vreg[j].rdev);

			return PTR_ERR(chip->vreg[i].rdev);
		}
	}

	return 0;
}

static int tps65132_regulator_remove(struct i2c_client *client)
{
	struct tps65132_chip *chip = i2c_get_clientdata(client);
	struct regulator *i2c_pwr = chip->i2c_pwr;
	int i;

	if (i2c_pwr)
		regulator_disable(i2c_pwr);

	for (i = 0; i < chip->num_regulators; i++)
		regulator_unregister(chip->vreg[i].rdev);

	return 0;
}

static struct of_device_id tps65132_match_table[] = {
	{ .compatible = "ti,tps65132", },
	{},
};
MODULE_DEVICE_TABLE(of, tps65132_match_table);

static const struct i2c_device_id tps65132_id[] = {
	{"tps65132", -1},
	{ },
};

static int tps65132_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tps65132_chip *chip = i2c_get_clientdata(client);
	int rc = 0;

	if (chip->i2c_pwr)
		rc = regulator_disable(chip->i2c_pwr);

	return rc;
}

static int tps65132_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tps65132_chip *chip = i2c_get_clientdata(client);
	int rc = 0;

	if (chip->i2c_pwr)
		rc = regulator_enable(chip->i2c_pwr);

	return rc;
}

const struct dev_pm_ops tps65132_pm_ops = {
	.resume = tps65132_resume,
	.suspend = tps65132_suspend,
};

static struct i2c_driver tps65132_regulator_driver = {
	.driver = {
		.name		= "tps65132",
		.owner		= THIS_MODULE,
		.of_match_table	= tps65132_match_table,
	},
	.probe = tps65132_regulator_probe,
	.remove = tps65132_regulator_remove,
	.id_table = tps65132_id,
};

static int __init tps65132_init(void)
{
	return i2c_add_driver(&tps65132_regulator_driver);
}
subsys_initcall(tps65132_init);

static void __exit tps65132_exit(void)
{
	i2c_del_driver(&tps65132_regulator_driver);
}
module_exit(tps65132_exit);

MODULE_DESCRIPTION("TI TPS65132 regulator driver");
MODULE_LICENSE("GPL v2");
