/*
 * max15569-regulator.c -- max15569 regulator driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/max15569-regulator.h>
#include <linux/slab.h>

/* Register definitions */
#define MAX15569_VOUTMAX_REG			0x2
#define MAX15569_STATUS_REG			0x4
#define MAX15569_MASK_REG			0x5
#define MAX15569_SLEW_RATE_REG			0x6
#define MAX15569_SETVOUT_REG			0x7
#define MAX15569_IMON_REG			0x8

#define MAX15569_MAX_REG			0x9

#define MAX15569_MIN_VOLTAGE	500000
#define MAX15569_MAX_VOLTAGE	1520000
#define MAX15569_VOLTAGE_STEP	10000
#define MAX15569_MAX_SEL	0x7F
#define MAX15569_MAX_SLEW_RATE  44
#define MAX15569_MIN_SLEW_RATE  1

#define MAX15569_STATUS_VRHOT	0x20
#define MAX15569_STATUS_UV	0x10
#define MAX15569_STATUS_OV	0x08
#define MAX15569_STATUS_OC	0x4
#define MAX15569_STATUS_VMERR	0x2
#define MAX15569_STATUS_INT	0x1

struct {
	unsigned char soft_start_slew_rate;
	unsigned char regular_slew_rate;
} max15569_slewrate_table[] = {
	{-1,	-1 },
	{ 9,	9},
	{ 9,	9},
	{ 9,	9},
	{ 9,	9},
	{ 0x19,	0x19},
	{ 0x19,	0x19},
	{ 0x23,	0x23},
	{ 3,	3},
	{ 3,	3},
	{ 0x17,	0x1e},
	{ 0x17,	0x1e},
	{ 0x20,	0x20},
	{ 0x20,	0x20},
	{ 0x20,	0x20},
	{ 0x20,	0x20},
	{ 0x20,	0x20},
	{ 0x0, 0x0},
	{ 0x0, 0x0},
	{ 0x0, 0x0},
	{ 0x0, 0x0},
	{ 0x10, 0x10},
	{ 0x10,	0x10},
	{ 0x10,	0x10},
	{ 0x10,	0x10},
	{ 0x25,	0x25},
	{ 0x25,	0x25},
	{ 0x25,	0x25},
	{ 0x25,	0x25},
	{ 0x25,	0x25},
	{ 0x25,	0x25},
	{ 0x25,	0x25},
	{ 0x25,	0x25},
	{ 0x5,	0x5},
	{ 0x5,	0x5},
	{ 0x5,	0x5},
	{ 0x5,	0x5},
	{ 0x5,	0x5},
	{ 0x5,	0x5},
	{ 0x5,	0x5},
	{ 0x5,	0x5},
	{ 0x15,	0x15},
	{ 0x15,	0x15},
	{ 0x15,	0x15},
	{ 0x15,	0x15},
};

/* MAX15569 chip information */
struct max15569_chip {
	struct device *dev;
	struct regulator_desc desc;
	struct regulator_dev *rdev;
	struct regmap *regmap;

	bool output_enabled;
	unsigned int change_mv_per_us;
};

static int max15569_get_voltage_sel(struct regulator_dev *rdev)
{
	struct max15569_chip *max = rdev_get_drvdata(rdev);
	unsigned int data;
	int ret;
	unsigned int reg = MAX15569_SETVOUT_REG;

	ret = regmap_read(max->regmap, reg, &data);
	if (ret < 0) {
		dev_err(max->dev, "reg read failed, err %d\n", ret);
		return ret;
	}
	return data;
}

static int max15569_set_voltage(struct regulator_dev *rdev,
	     int min_uV, int max_uV, unsigned *selector)
{
	struct max15569_chip *max = rdev_get_drvdata(rdev);
	unsigned int reg = MAX15569_SETVOUT_REG;
	int vsel;
	int ret;

	if ((max_uV < min_uV) || (max_uV < MAX15569_MIN_VOLTAGE) ||
			(min_uV > MAX15569_MAX_VOLTAGE))
		return -EINVAL;

	vsel = DIV_ROUND_UP(min_uV - MAX15569_MIN_VOLTAGE,
			MAX15569_VOLTAGE_STEP) + 0x1;
	if (selector)
		*selector = vsel;

	ret = regmap_write(max->regmap, reg, vsel);
	if (ret < 0)
		dev_err(max->dev, "reg write failed, err %d\n", ret);
	return ret;
}

static int max15569_list_voltage(struct regulator_dev *rdev,
					unsigned selector)
{
	if (selector > MAX15569_MAX_SEL)
		return -EINVAL;

	return MAX15569_MIN_VOLTAGE + (selector - 0x1) * MAX15569_VOLTAGE_STEP;
}

static int max15569_set_voltage_time_sel(struct regulator_dev *rdev,
		unsigned int old_selector, unsigned int new_selector)
{
	struct max15569_chip *max = rdev_get_drvdata(rdev);
	int change_mv_per_us = max->change_mv_per_us;

	if (change_mv_per_us > MAX15569_MAX_SLEW_RATE)
		change_mv_per_us = MAX15569_MAX_SLEW_RATE;

	if (change_mv_per_us < MAX15569_MIN_SLEW_RATE)
		change_mv_per_us = MAX15569_MIN_SLEW_RATE;

	return max->output_enabled ?
		max15569_slewrate_table[max->change_mv_per_us].regular_slew_rate :
		max15569_slewrate_table[max->change_mv_per_us].soft_start_slew_rate;
}

static int max15569_set_control_mode(struct regulator_dev *rdev,
		unsigned int mode)
{
	if (mode != REGULATOR_CONTROL_MODE_I2C)
		return -EINVAL;

	return 0;
}

static unsigned int max15569_get_control_mode(struct regulator_dev *rdev)
{
	return REGULATOR_CONTROL_MODE_I2C;
}

static struct regulator_ops max15569_ops = {
	.get_voltage_sel	= max15569_get_voltage_sel,
	.set_voltage		= max15569_set_voltage,
	.list_voltage		= max15569_list_voltage,
	.set_voltage_time_sel	= max15569_set_voltage_time_sel,
	.set_control_mode	= max15569_set_control_mode,
	.get_control_mode	= max15569_get_control_mode,
};

static int max15569_init(struct max15569_chip *max15569,
	struct max15569_regulator_platform_data *pdata)
{
	int ret;
	int vsel;
	unsigned int status;

	max15569->output_enabled = true;

	/* Set slew rate */
	/* max15569->change_mv_per_us = min(44,max(1, pdata->slew_rate_mv_per_us)); */

	if (max15569->change_mv_per_us > 44)
		max15569->change_mv_per_us = 44;

	if (max15569->change_mv_per_us < 1)
		max15569->change_mv_per_us = 1;


	vsel = max15569_slewrate_table[max15569->change_mv_per_us].regular_slew_rate;

	ret = regmap_write(max15569->regmap, MAX15569_SLEW_RATE_REG, vsel);
	if (ret < 0) {
		dev_err(max15569->dev, "SLEW reg write failed, err %d\n", ret);
		return ret;
	}

	/* Set base voltage */
	vsel = DIV_ROUND_UP(pdata->base_voltage_uV -
		MAX15569_MIN_VOLTAGE, MAX15569_VOLTAGE_STEP) + 0x1;
	dev_err(max15569->dev, "Setting rail to vsel %x\n", vsel);
	ret = regmap_write(max15569->regmap, MAX15569_SETVOUT_REG, vsel);
	if (ret < 0) {
		dev_err(max15569->dev, "BASE reg write failed, err %d\n", ret);
		return ret;
	}

	/* setup max voltage */
	if (pdata->max_voltage_uV) {
		vsel = DIV_ROUND_UP(pdata->max_voltage_uV -
			MAX15569_MIN_VOLTAGE, MAX15569_VOLTAGE_STEP) + 0x1;
		ret = regmap_write(max15569->regmap, MAX15569_VOUTMAX_REG, vsel);
		if (ret < 0) {
			dev_err(max15569->dev, "VMAX write failed, err %d\n", ret);
			return ret;
		}
	}

	ret = regmap_read(max15569->regmap, MAX15569_STATUS_REG, &status);
	if (ret < 0) {
		dev_err(max15569->dev, "STATUS reg read failed, err %d\n", ret);
		return ret;
	}

	if (status & MAX15569_STATUS_VRHOT)
		dev_err(max15569->dev, "VRHOT: regulator temperature beyond limit\n");

	if (status & MAX15569_STATUS_UV)
		dev_err(max15569->dev, "UV: regulator under-voltage condition\n");

	if (status & MAX15569_STATUS_OV)
		dev_err(max15569->dev, "UV: regulator over-voltage condition\n");

	if (status & MAX15569_STATUS_OC)
		dev_err(max15569->dev, "UV: regulator over-current condition\n");

	return 0;
}

static bool is_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX15569_IMON_REG:
		return true;
	default:
		return false;
	}
}

static bool is_read_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x02:
	case 0x4 ... 0x08:
		return true;
	default:
		return false;
	}
}

static bool is_write_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case 0x02:
	case 0x5 ... 0x7:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config max15569_regmap_config = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.writeable_reg		= is_write_reg,
	.readable_reg		= is_read_reg,
	.volatile_reg		= is_volatile_reg,
	.max_register		= MAX15569_MAX_REG - 1,
	.cache_type		= REGCACHE_RBTREE,
};

static int max15569_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max15569_regulator_platform_data *pdata;
	struct regulator_dev *rdev;
	struct max15569_chip *max;
	struct regulator_config rconfig = { };
	int ret;

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "No Platform data\n");
		return -EINVAL;
	}

	max = devm_kzalloc(&client->dev, sizeof(*max), GFP_KERNEL);
	if (!max) {
		dev_err(&client->dev, "Memory allocation failed\n");
		return -ENOMEM;
	}

	max->dev = &client->dev;
	max->desc.name = id->name;
	max->desc.id = 0;
	max->desc.ops = &max15569_ops;
	max->desc.type = REGULATOR_VOLTAGE;
	max->desc.owner = THIS_MODULE;
	max->regmap = devm_regmap_init_i2c(client, &max15569_regmap_config);
	if (IS_ERR(max->regmap)) {
		ret = PTR_ERR(max->regmap);
		dev_err(&client->dev, "regmap init failed, err %d\n", ret);
		return ret;
	}
	i2c_set_clientdata(client, max);

	ret = max15569_init(max, pdata);
	if (ret < 0) {
		dev_err(max->dev, "Init failed, err = %d\n", ret);
		return ret;
	}

	/* Register the regulators */
	rconfig.dev = &client->dev;
	rconfig.of_node = NULL;
	rconfig.init_data = pdata->reg_init_data;
	rconfig.driver_data = max;
	rdev = regulator_register(&max->desc, &rconfig);

	if (IS_ERR(rdev)) {
		dev_err(max->dev, "regulator register failed\n");
		return PTR_ERR(rdev);
	}

	max->rdev = rdev;
	return 0;
}

static int max15569_remove(struct i2c_client *client)
{
	struct max15569_chip *max = i2c_get_clientdata(client);

	regulator_unregister(max->rdev);
	return 0;
}

static const struct i2c_device_id max15569_id[] = {
	{.name = "max15569",},
	{},
};

MODULE_DEVICE_TABLE(i2c, max15569_id);

static struct i2c_driver max15569_i2c_driver = {
	.driver = {
		.name = "max15569",
		.owner = THIS_MODULE,
	},
	.probe = max15569_probe,
	.remove = max15569_remove,
	.id_table = max15569_id,
};

static int __init max15569_drv_init(void)
{
	return i2c_add_driver(&max15569_i2c_driver);
}
subsys_initcall(max15569_drv_init);

static void __exit max15569_drv_cleanup(void)
{
	i2c_del_driver(&max15569_i2c_driver);
}
module_exit(max15569_drv_cleanup);

MODULE_AUTHOR("NVIDIA Corporation");
MODULE_DESCRIPTION("MAX15569 voltage regulator driver");
MODULE_LICENSE("GPL v2");
