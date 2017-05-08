/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/param.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/of_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/regmap.h>

struct voltage_range {
	int	vrange_sel;
	int	min_uV;
	int	max_uV;
	int	step_uV;
};

struct max20010_slew_rate {
	int	slew_sel;
	int	soft_start;
	int	dvs;
};

struct max20010_device_info {
	struct device			*dev;
	struct regulator_dev		*rdev;
	struct regulator_init_data	*init_data;
	struct regmap			*regmap;
	const struct voltage_range	*range;
	const struct max20010_slew_rate	*slew_rate;
	unsigned			vout_sel;
	bool				enabled;
};

#define MAX20010_ID_REG			0x00

#define MAX20010_VMAX_REG		0x02
#define MAX20010_VMAX_MASK		GENMASK(6, 0)

#define MAX20010_CONFIG_REG		0x05
#define MAX20010_CONFIG_SYNC_IO_MASK	GENMASK(1, 0)
#define MAX20010_CONFIG_MODE_MASK	BIT(3)
#define MAX20010_CONFIG_MODE_SYNC	0
#define MAX20010_CONFIG_MODE_FPWM	8
#define MAX20010_CONFIG_VSTEP_MASK	BIT(7)
#define MAX20010_CONFIG_VSTEP_SHIFT	7

#define MAX20010_SLEW_REG		0x06
#define MAX20010_SLEW_MASK		GENMASK(3, 0)

#define MAX20010_VSET_REG		0x07
#define MAX20010_VSET_MASK		GENMASK(6, 0)

static const struct max20010_slew_rate slew_rates[] = {
	{0, 22000, 22000},
	{1, 11000, 22000},
	{2,  5500, 22000},
	{3, 11000, 11000},
	{4,  5500, 11000},
	{5, 44000, 44000},
	{6, 22000, 44000},
	{7, 11000, 44000},
	{8,  5500, 44000},
	{9,  5500,  5500},
};

static const struct voltage_range max20010_range0 = {0, 500000, 1270000, 10000};
static const struct voltage_range max20010_range1 = {1, 625000, 1587500, 12500};

static const struct regmap_config max20010_regmap_config = {
	.reg_bits	= 8,
	.val_bits	= 8,
	.max_register	= MAX20010_VSET_REG,
};

static int max20010_set_voltage_sel(struct regulator_dev *rdev, unsigned sel)
{
	struct max20010_device_info *info = rdev_get_drvdata(rdev);
	int rc = 0;

	/* Set the voltage only if the regulator was enabled earlier */
	if (info->enabled) {
		rc = regulator_set_voltage_sel_regmap(rdev, sel);
		if (rc) {
			dev_err(info->dev,
				"regulator set voltage failed for selector = 0x%2x, rc=%d\n",
				sel, rc);
			return rc;
		}
	}

	info->vout_sel = sel;
	return rc;
}

static int max20010_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct max20010_device_info *info = rdev_get_drvdata(rdev);

	return (info->enabled == true) ? 1 : 0;
}

static int max20010_regulator_enable(struct regulator_dev *rdev)
{
	struct max20010_device_info *info = rdev_get_drvdata(rdev);
	int rc = 0;

	rc = regulator_set_voltage_sel_regmap(rdev, info->vout_sel);
	if (rc) {
		dev_err(info->dev, "regulator enable failed, rc=%d\n", rc);
		return rc;
	}
	info->enabled = true;

	return rc;
}

static int max20010_regulator_disable(struct regulator_dev *rdev)
{
	struct max20010_device_info *info = rdev_get_drvdata(rdev);
	int rc = 0;

	rc = regulator_set_voltage_sel_regmap(rdev, 0x0);
	if (rc) {
		dev_err(info->dev, "regulator disable failed, rc=%d\n", rc);
		return rc;
	}
	info->enabled = false;

	return rc;
}

static inline unsigned int max20010_map_mode(unsigned int mode)
{
	return (mode == MAX20010_CONFIG_MODE_FPWM) ?
		REGULATOR_MODE_NORMAL : REGULATOR_MODE_IDLE;
}

static int max20010_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct max20010_device_info *info = rdev_get_drvdata(rdev);
	int rc = 0;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		rc = regmap_update_bits(info->regmap, MAX20010_CONFIG_REG,
					MAX20010_CONFIG_MODE_MASK,
					MAX20010_CONFIG_MODE_FPWM);
		break;
	case REGULATOR_MODE_IDLE:
		rc = regmap_update_bits(info->regmap, MAX20010_CONFIG_REG,
					(MAX20010_CONFIG_MODE_MASK
					 | MAX20010_CONFIG_SYNC_IO_MASK),
					MAX20010_CONFIG_MODE_SYNC);
		break;
	default:
		return -EINVAL;
	}

	if (rc)
		dev_err(info->dev, "failed to set %s mode, rc=%d\n",
			mode == REGULATOR_MODE_NORMAL ? "Force PWM" : "SYNC",
			rc);
	return rc;
}

static unsigned int max20010_get_mode(struct regulator_dev *rdev)
{
	struct max20010_device_info *info = rdev_get_drvdata(rdev);
	unsigned int val;
	int rc = 0;

	rc = regmap_read(info->regmap, MAX20010_CONFIG_REG, &val);
	if (rc) {
		dev_err(info->dev, "failed to read mode configuration, rc=%d\n",
			rc);
		return rc;
	}

	return  max20010_map_mode(val & MAX20010_CONFIG_MODE_MASK);
}

static int max20010_enable_time(struct regulator_dev *rdev)
{
	struct max20010_device_info *info = rdev_get_drvdata(rdev);
	int volt_uV;

	volt_uV = regulator_list_voltage_linear(rdev, info->vout_sel);
	return DIV_ROUND_UP(volt_uV, info->slew_rate->soft_start);
}

static struct regulator_ops max20010_regulator_ops = {
	.set_voltage_sel = max20010_set_voltage_sel,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.map_voltage = regulator_map_voltage_linear,
	.list_voltage = regulator_list_voltage_linear,
	.is_enabled = max20010_regulator_is_enabled,
	.enable = max20010_regulator_enable,
	.disable = max20010_regulator_disable,
	.set_mode = max20010_set_mode,
	.get_mode = max20010_get_mode,
	.enable_time = max20010_enable_time,
};

static struct regulator_desc rdesc = {
	.name = "max20010-reg",
	.supply_name = "vin",
	.owner = THIS_MODULE,
	.ops = &max20010_regulator_ops,
	.type = REGULATOR_VOLTAGE,
	.linear_min_sel = 1,
	.vsel_reg = MAX20010_VSET_REG,
	.vsel_mask = MAX20010_VSET_MASK,
	.of_map_mode = max20010_map_mode,
};

static int max20010_device_setup(struct max20010_device_info *info)
{
	int max_uV, rc = 0;
	unsigned int val;

	rc = regmap_update_bits(info->regmap, MAX20010_CONFIG_REG,
				MAX20010_CONFIG_VSTEP_MASK,
				(info->range->vrange_sel
				 << MAX20010_CONFIG_VSTEP_SHIFT));
	if (rc) {
		dev_err(info->dev, "failed to update vstep configuration, rc=%d\n",
			rc);
		return rc;
	}

	max_uV = min(info->init_data->constraints.max_uV, info->range->max_uV);
	val = DIV_ROUND_UP(max_uV - info->range->min_uV,
			info->range->step_uV) + 1;
	rc = regmap_update_bits(info->regmap, MAX20010_VMAX_REG,
				MAX20010_VMAX_MASK, val);
	if (rc) {
		dev_err(info->dev, "failed to write VMAX configuration, rc=%d\n",
			rc);
		return rc;
	}

	rc = regmap_update_bits(info->regmap, MAX20010_SLEW_REG,
				MAX20010_SLEW_MASK, info->slew_rate->slew_sel);
	if (rc) {
		dev_err(info->dev, "failed to write slew configuration, rc=%d\n",
			rc);
		return rc;
	}

	/* Store default voltage register value */
	rc = regmap_read(info->regmap, MAX20010_VSET_REG, &val);
	if (rc) {
		dev_err(info->dev, "failed to read voltage register, rc=%d\n",
			rc);
		return rc;
	}

	info->vout_sel = val & MAX20010_VSET_MASK;
	info->enabled = (info->vout_sel != 0x0) ? true : false;

	return rc;
}

static int max20010_parse_init_data(struct max20010_device_info *info)
{
	struct device_node *of_node = info->dev->of_node;
	int i, slew_index, ss_slew_rate, dvs_slew_rate, rc = 0;
	unsigned int val;

	if (of_find_property(of_node, "maxim,vrange-sel", NULL)) {
		rc = of_property_read_u32(of_node, "maxim,vrange-sel", &val);
		if (rc) {
			dev_err(info->dev, "maxim,vrange-sel property read failed, rc=%d\n",
				rc);
			return rc;
		} else if (val > 1) {
			dev_err(info->dev, "unsupported vrange-sel value = %d, should be either 0 or 1\n",
				val);
			return -EINVAL;
		}
	} else {
		/* Read default voltage range value */
		rc = regmap_read(info->regmap, MAX20010_CONFIG_REG, &val);
		if (rc) {
			dev_err(info->dev, "failed to read config register, rc=%d\n",
				rc);
			return rc;
		}

		val = (val & MAX20010_CONFIG_VSTEP_MASK)
		       >> MAX20010_CONFIG_VSTEP_SHIFT;
	}

	info->range = (val == 0) ? &max20010_range0 : &max20010_range1;

	/*
	 * Verify the min and max constraints specified through regulator device
	 * properties are fit with in that of the selected voltage range of the
	 * device.
	 */
	if (info->init_data->constraints.min_uV < info->range->min_uV ||
		info->init_data->constraints.max_uV > info->range->max_uV) {
		dev_err(info->dev,
			"Regulator min/max constraints are not fit with in the device min/max constraints\n");
		return -EINVAL;
	}

	/*
	 * Read soft-start and dvs slew rates from device node. Use default
	 * values if not specified.
	 *
	 * Read the register default values and modify them with the slew-rates
	 * defined through device node.
	 */
	rc = regmap_read(info->regmap, MAX20010_SLEW_REG, &val);
	if (rc) {
		dev_err(info->dev, "failed to read slew register, rc=%d\n",
			rc);
		return rc;
	}

	slew_index = val & MAX20010_SLEW_MASK;

	if (slew_index >= ARRAY_SIZE(slew_rates)) {
		dev_err(info->dev, "unsupported default slew configuration\n");
		return -EINVAL;
	}

	ss_slew_rate = slew_rates[slew_index].soft_start;
	dvs_slew_rate = slew_rates[slew_index].dvs;

	if (of_find_property(of_node, "maxim,soft-start-slew-rate", NULL)) {
		rc = of_property_read_u32(of_node, "maxim,soft-start-slew-rate",
					&val);
		if (rc) {
			dev_err(info->dev, "maxim,soft-start-slew-rate read failed, rc=%d\n",
				rc);
			return rc;
		}

		ss_slew_rate = val;
	}

	if (of_find_property(of_node, "maxim,dvs-slew-rate", NULL)) {
		rc = of_property_read_u32(of_node, "maxim,dvs-slew-rate",
					&val);
		if (rc) {
			dev_err(info->dev, "maxim,dvs-slew-rate read failed, rc=%d\n",
				rc);
			return rc;
		}

		dvs_slew_rate = val;
	}

	for (i = 0; i < ARRAY_SIZE(slew_rates); i++) {
		if (ss_slew_rate == slew_rates[i].soft_start
		    && dvs_slew_rate == slew_rates[i].dvs) {
			info->slew_rate = &slew_rates[i];
			break;
		}
	}

	if (i == ARRAY_SIZE(slew_rates)) {
		dev_err(info->dev, "invalid slew-rate values are specified.\n");
		return -EINVAL;
	}

	return rc;
}

static int max20010_regulator_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct max20010_device_info *info;
	struct regulator_config config = { };
	int val, rc = 0;

	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dev = &client->dev;
	info->init_data = of_get_regulator_init_data(info->dev,
						info->dev->of_node, &rdesc);
	if (!info->init_data) {
		dev_err(info->dev, "regulator init_data is missing\n");
		return -ENODEV;
	}

	info->init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_MODE;
	info->init_data->constraints.valid_modes_mask
				= REGULATOR_MODE_NORMAL | REGULATOR_MODE_IDLE;

	info->regmap = devm_regmap_init_i2c(client, &max20010_regmap_config);
	if (IS_ERR(info->regmap)) {
		dev_err(info->dev, "Error in allocating regmap\n");
		return PTR_ERR(info->regmap);
	}

	i2c_set_clientdata(client, info);

	/* Get chip Id */
	rc = regmap_read(info->regmap, MAX20010_ID_REG, &val);
	if (rc) {
		dev_err(info->dev, "Failed to get chip ID!\n");
		return rc;
	}

	rc = max20010_parse_init_data(info);
	if (rc) {
		dev_err(info->dev, "max20010 init data parsing failed, rc=%d\n",
			rc);
		return rc;
	}

	rc = max20010_device_setup(info);
	if (rc) {
		dev_err(info->dev, "Failed to setup device, rc=%d\n",
			rc);
		return rc;
	}

	config.dev = info->dev;
	config.init_data = info->init_data;
	config.regmap = info->regmap;
	config.driver_data = info;
	config.of_node = client->dev.of_node;

	rdesc.min_uV = info->range->min_uV;
	rdesc.uV_step = info->range->step_uV;
	rdesc.n_voltages = DIV_ROUND_UP((info->range->max_uV
					- info->range->min_uV),
					info->range->step_uV);
	rdesc.ramp_delay = info->slew_rate->dvs;

	info->rdev = devm_regulator_register(info->dev, &rdesc, &config);
	if (IS_ERR(info->rdev)) {
		dev_err(info->dev, "Failed to register regulator, rc=%d\n", rc);
		return PTR_ERR(info->rdev);
	}

	dev_info(info->dev, "Detected regulator MAX20010 PID = %d : voltage-range(%d) : (%d - %d) uV, step = %d uV\n",
		val, info->range->vrange_sel, info->range->min_uV,
		info->range->max_uV, info->range->step_uV);

	return rc;
}

static const struct of_device_id max20010_match_table[] = {
	{.compatible = "maxim,max20010", },
	{ },
};
MODULE_DEVICE_TABLE(of, max20010_match_table);

static const struct i2c_device_id max20010_id[] = {
	{"max20010", -1},
	{ },
};
MODULE_DEVICE_TABLE(i2c, max20010_id);

static struct i2c_driver max20010_regulator_driver = {
	.driver = {
		.name = "max20010-regulator",
		.owner = THIS_MODULE,
		.of_match_table = max20010_match_table,
	},
	.probe = max20010_regulator_probe,
	.id_table = max20010_id,
};
module_i2c_driver(max20010_regulator_driver);

MODULE_DESCRIPTION("MAX20010 regulator driver");
MODULE_LICENSE("GPL v2");
