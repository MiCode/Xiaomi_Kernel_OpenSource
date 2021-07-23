/*
 * FAN53870 ON Semiconductor LDO PMIC Driver.
 *
 * Copyright (c) 2019 On Semiconducto.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Zhongming Yang <bright.yang@onsemi.com>
 *
 * This package is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
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

#define fan53870_err(reg, message, ...) \
        pr_err("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)
#define fan53870_debug(reg, message, ...) \
        pr_debug("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)

#define FAN53870_REG_PID      0x00
#define FAN53870_REG_RID      0x01
#define FAN53870_REG_IOUT     0x02
#define FAN53870_REG_ENABLE   0x03
#define FAN53870_REG_LDO0     0x04

#define LDO_VSET_REG(offset) ((offset) + FAN53870_REG_LDO0)

#define VSET_BASE_12      800
#define VSET_BASE_34567   1500
#define VSET_STEP_MV      8

#define MAX_REG_NAME     20
#define FAN53870_MAX_LDO 7

struct fan53870_regulator{
	struct device    *dev;
	struct regmap    *regmap;
	struct regulator_desc rdesc;
	struct regulator_dev  *rdev;
	struct regulator      *parent_supply;
	struct regulator      *en_supply;
	struct device_node    *of_node;
	u16         offset;
	int         min_dropout_uv;
	int         iout_ua;
};

struct regulator_data {
	char *name;
	char *supply_name;
	int default_mv;
	int  min_dropout_uv;
	int iout_ua;
};

static struct regulator_data reg_data[] = {
	/* name,  parent,   headroom */
	{ "fan53870-l1", "vdd_l1_l2",  1048, 225000, 650000},
	{ "fan53870-l2", "vdd_l1_l2", 1048, 225000, 650000},
	{ "fan53870-l3", "vdd_l3_l4", 2804, 200000, 650000},
	{ "fan53870-l4", "vdd_l3_l4", 2804, 200000, 650000},
	{ "fan53870-l5", "vdd_l5", 1804, 300000, 650000},
	{ "fan53870-l6", "vdd_l6", 2804, 300000, 650000},
	{ "fan53870-l7", "vdd_l7", 2804, 300000, 650000},
};

static const struct regmap_config fan53870_regmap_config = {
        .reg_bits = 8,
        .val_bits = 8,
};

/*common functions*/
static int fan53870_read(struct regmap *regmap, u16 reg, u8 *val, int count)
{
	int rc;

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0)
		pr_err("failed to read 0x%04x\n", reg);
	return rc;
}

static int fan53870_write(struct regmap *regmap, u16 reg, u8*val, int count)
{
	int rc;

	pr_debug("Writing 0x%02x to 0x%02x\n", *val, reg);
	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0)
		pr_err("failed to write 0x%04x\n", reg);

	return rc;
}

static int fan53870_masked_write(struct regmap *regmap, u16 reg, u8 mask, u8 val)
{
	int rc;
	pr_debug("Writing 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0)
		pr_err("failed to write 0x%02x to 0x%04x with mask 0x%02x\n", val, reg, mask);
	return rc;
}

static int fan53870_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct fan53870_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;
	u8 reg;

	rc = fan53870_read(fan_reg->regmap,
		FAN53870_REG_ENABLE, &reg, 1);
	if (rc < 0) {
		fan53870_err(fan_reg, "failed to read enable reg rc = %d\n", rc);
		return rc;
	}
	return !!(reg & (1u<<fan_reg->offset));
}

static int fan53870_regulator_enable(struct regulator_dev *rdev)
{
	struct fan53870_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_enable(fan_reg->parent_supply);
		if (rc < 0) {
			fan53870_err(fan_reg, "failed to enable parent rc=%d\n", rc);
			return rc;
		}
	}

	rc = fan53870_masked_write(fan_reg->regmap,
		FAN53870_REG_ENABLE,
		1u<<fan_reg->offset, 1u<<fan_reg->offset);
	if (rc < 0) {
		fan53870_err(fan_reg, "failed to enable regulator rc=%d\n", rc);
		goto remove_vote;
	}

	return 0;

remove_vote:
	if (fan_reg->parent_supply)
		rc = regulator_disable(fan_reg->parent_supply);
	if (rc < 0)
		fan53870_err(fan_reg, "failed to disable parent regulator rc=%d\n",rc);
	return -ETIME;
}

static int fan53870_regulator_disable(struct regulator_dev *rdev)
{
	struct fan53870_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	rc = fan53870_masked_write(fan_reg->regmap,
		FAN53870_REG_ENABLE,
		1u<<fan_reg->offset, 0);

	if (rc < 0) {
		fan53870_err(fan_reg,
			"failed to disable regulator rc=%d\n", rc);
		return rc;
	}

	/*remove voltage vot from parent regulator */
	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
		if (rc < 0) {
			fan53870_err(fan_reg,
				"failed to remove parent voltage rc=%d\n", rc);
			return rc;
		}
		rc = regulator_disable(fan_reg->parent_supply);
		if (rc < 0) {
			fan53870_err(fan_reg,
				"failed to disable parent rc=%d\n", rc);
			return rc;
		}
	}

	fan53870_debug(fan_reg, "regulator disabled\n");
	return 0;
}

static int fan53870_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct fan53870_regulator *fan_reg = rdev_get_drvdata(rdev);
	u8 vset;
	int rc;
	int uv;

	rc = fan53870_read(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
	&vset, 1);
	if (rc < 0) {
		fan53870_err(fan_reg,
			"failed to read regulator voltage rc = %d\n", rc);
		return rc;
	}

	if (vset == 0) {
		uv = reg_data[fan_reg->offset].default_mv;
	} else {
		fan53870_debug(fan_reg, "voltage read [%x]\n", vset);
		if (fan_reg->offset == 0 || fan_reg->offset == 1)
			uv = (VSET_BASE_12 + (vset-99)*VSET_STEP_MV)*1000;
		else
			uv = (VSET_BASE_34567 + (vset-16)*VSET_STEP_MV)*1000;
	}
	return uv;
}

static int fan53870_write_voltage(struct fan53870_regulator* fan_reg, int min_uv,
	int max_uv)
{
	int rc = 0, mv;
	u8 vset;

	mv = DIV_ROUND_UP(min_uv, 1000);
	if (mv*1000 > max_uv) {
		fan53870_err(fan_reg, "requestd voltage above maximum limit\n");
		return -EINVAL;
	}

	if (fan_reg->offset == 0 || fan_reg->offset == 1)
		vset = DIV_ROUND_UP(mv-VSET_BASE_12, VSET_STEP_MV) + 99;
	else
		vset = DIV_ROUND_UP(mv-VSET_BASE_34567, VSET_STEP_MV) + 16;

	rc = fan53870_write(fan_reg->regmap, LDO_VSET_REG(fan_reg->offset),
		&vset, 1);
	if (rc < 0) {
		fan53870_err(fan_reg, "failed to write voltage rc = %d\n", rc);
		return rc;
	}

	fan53870_debug(fan_reg, "VSET=[%2x]\n", vset);
	return 0;
} 

static int fan53870_regulator_set_voltage(struct regulator_dev *rdev,
	int min_uv, int max_uv, unsigned int* selector)
{
	struct fan53870_regulator *fan_reg = rdev_get_drvdata(rdev);
	int rc;

	if (fan_reg->parent_supply) {
		rc = regulator_set_voltage(fan_reg->parent_supply,
			fan_reg->min_dropout_uv + min_uv,
			INT_MAX);
		if (rc < 0) {
			fan53870_err(fan_reg,
				"failed to request parent supply voltage rc=%d\n", rc);
			return rc;
		}
	}

	rc = fan53870_write_voltage(fan_reg, min_uv, max_uv);
	if (rc < 0) {
		/* remove parentn's voltage vote */
		if (fan_reg->parent_supply)
			regulator_set_voltage(fan_reg->parent_supply, 0, INT_MAX);
	}
	fan53870_debug(fan_reg, "voltage set to %d\n", min_uv);
	return rc;
}

static struct regulator_ops fan53870_regulator_ops = {
	.enable = fan53870_regulator_enable,
	.disable = fan53870_regulator_disable,
	.is_enabled = fan53870_regulator_is_enabled,
	.set_voltage = fan53870_regulator_set_voltage,
	.get_voltage = fan53870_regulator_get_voltage,
};

static int fan53870_register_ldo(struct fan53870_regulator *fan53870_reg,
	const char *name)
{
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data;

	struct device_node *reg_node = fan53870_reg->of_node;
	struct device *dev = fan53870_reg->dev;
	int rc, i, init_voltage;
	char buff[MAX_REG_NAME];

	/* try to find ldo pre-defined in the regulator table */
	for (i = 0; i<FAN53870_MAX_LDO; i++) {
		if (!strcmp(reg_data[i].name, name))
			break;
	}

	if ( i == FAN53870_MAX_LDO) {
		pr_err("Invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u16(reg_node, "offset", &fan53870_reg->offset);
	if (rc < 0) {
		pr_err("%s:failed to get regulator offset rc = %d\n", name, rc);
		return rc;
	}

	//assign default value defined in code.
	fan53870_reg->min_dropout_uv = reg_data[i].min_dropout_uv;
	of_property_read_u32(reg_node, "min-dropout-voltage",
		&fan53870_reg->min_dropout_uv);

	fan53870_reg->iout_ua = reg_data[i].iout_ua;
	of_property_read_u32(reg_node, "iout_ua",
		&fan53870_reg->iout_ua);

	init_voltage = -EINVAL;
	of_property_read_u32(reg_node, "init-voltage", &init_voltage);

	scnprintf(buff, MAX_REG_NAME, "%s-supply", reg_data[i].supply_name);
	if (of_find_property(dev->of_node, buff, NULL)) {
		fan53870_reg->parent_supply = devm_regulator_get(dev,
			reg_data[i].supply_name);
		if (IS_ERR(fan53870_reg->parent_supply)) {
			rc = PTR_ERR(fan53870_reg->parent_supply);
			if (rc != EPROBE_DEFER)
				pr_err("%s: failed to get parent regulator rc = %d\n",
					name, rc);
				return rc;
		}
	}

	init_data = of_get_regulator_init_data(dev, reg_node, &fan53870_reg->rdesc);
	if (init_data == NULL) {
		pr_err("%s: failed to get regulator data\n", name);
		return -ENODATA;
	}


	if (!init_data->constraints.name) {
		pr_err("%s: regulator name missing\n", name);
		return -EINVAL;
	}

	/* configure the initial voltage for the regulator */
	if (init_voltage > 0) {
		rc = fan53870_write_voltage(fan53870_reg, init_voltage,
			init_data->constraints.max_uV);
		if (rc < 0)
			pr_err("%s:failed to set initial voltage rc = %d\n", name, rc);
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
		| REGULATOR_CHANGE_VOLTAGE;

	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = fan53870_reg;
	reg_config.of_node = reg_node;

	fan53870_reg->rdesc.owner = THIS_MODULE;
	fan53870_reg->rdesc.type = REGULATOR_VOLTAGE;
	fan53870_reg->rdesc.ops = &fan53870_regulator_ops;
	fan53870_reg->rdesc.name = init_data->constraints.name;
	fan53870_reg->rdesc.n_voltages = 1;

	pr_info("try to register ldo %s\n", name);
	fan53870_reg->rdev = devm_regulator_register(dev, &fan53870_reg->rdesc,
		&reg_config);
	if (IS_ERR(fan53870_reg->rdev)) {
		rc = PTR_ERR(fan53870_reg->rdev);
		pr_err("%s: failed to register regulator rc =%d\n",
		fan53870_reg->rdesc.name, rc);
		return rc;
	}

	pr_info("%s regulator register done\n", name);
	return 0;
}

static int fan53870_parse_regulator(struct regmap *regmap, struct device *dev)
{
	int rc = 0;
	const char *name;
	struct device_node *child;
	struct fan53870_regulator *fan53870_reg;

	/* parse each regulator */
	for_each_available_child_of_node(dev->of_node, child) {
		fan53870_reg = devm_kzalloc(dev, sizeof(*fan53870_reg), GFP_KERNEL);
		if (!fan53870_reg)
			return -ENOMEM;

		fan53870_reg->regmap = regmap;
		fan53870_reg->of_node = child;
		fan53870_reg->dev = dev;

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc)
			continue;

		rc = fan53870_register_ldo(fan53870_reg, name);
		if (rc <0 ) {
			pr_err("failed to register regulator %s rc = %d\n", name, rc);
			return rc;
		}
	}
	return 0;
}

static int fan53870_regulator_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	unsigned int val = 0;
	struct regmap *regmap;
	struct pinctrl *ppinctrl;
	struct pinctrl_state *pins_default;

	regmap = devm_regmap_init_i2c(client, &fan53870_regmap_config);
	if (IS_ERR(regmap)) {
		pr_err("FAN53870 failed to allocate regmap\n");
		return PTR_ERR(regmap);
	}

	rc = regmap_read(regmap, 0x00, &val);
	if (rc < 0) {
		pr_err("FAN53870 failed to get PID\n");
		return rc;
	}
	else
		pr_info("FAN53870 get Product ID: [%02x]\n", val);

	rc = fan53870_parse_regulator(regmap, &client->dev);
	if (rc < 0) {
		pr_err("FAN53870 failed to parse device tree rc=%d\n", rc);
		return rc;
	}

/*	ppinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(ppinctrl)) {
		pr_err("%s : Cannot find fan53870 pinctrl!\n", __func__);
		return -EINVAL;
	}

	pins_default = pinctrl_lookup_state(ppinctrl, PINCTRL_STATE_DEFAULT);
	if (IS_ERR(pins_default)) {
		pr_err("%s : Cannot find pins_default!\n", __func__);
		return -EINVAL;
	}

	rc = pinctrl_select_state(ppinctrl, pins_default);
*/
	//fan53870_masked_write(regmap, FAN53870_REG_ENABLE, 0x7F, 0x7F);
	return 0;
}

static const struct of_device_id fan53870_dt_ids[] = {
	{
		.compatible = "onsemi,fan53870",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, fan53870_dt_ids);

static const struct i2c_device_id fan53870_id[] = {
	{
		.name = "fan53870-regulator",
		.driver_data = 0,
	},
	{ },
};
MODULE_DEVICE_TABLE(i2c, fan53870_id);

static struct i2c_driver fan53870_regulator_driver = {
	.driver = {
		.name = "fan53870-regulator",
		.of_match_table = of_match_ptr(fan53870_dt_ids),
	},
	.probe = fan53870_regulator_probe,
	.id_table = fan53870_id,
};

module_i2c_driver(fan53870_regulator_driver);

