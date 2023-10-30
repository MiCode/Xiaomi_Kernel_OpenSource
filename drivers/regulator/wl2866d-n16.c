// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019-2021, The Linux Foundation. All rights reserved. */

#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/debug-regulator.h>

#define wl2866d_err(reg, message, ...) \
	pr_err("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)
#define wl2866d_debug(reg, message, ...) \
	pr_debug("%s: " message, (reg)->rdesc.name, ##__VA_ARGS__)

#define wl2866d_vdd_step(type) \
	(type ? 12500 : 6000)
#define wl2866d_vdd_real(value, type) \
	(type ? (1200000 + value * 12500) : (600000 + value * 6000))
#define wl2866d_vdd_reg(value, type) \
	(type ? ((value - 1200000) / 12500) : ((value - 600000) / 6000))

#define WL2866D_MAX_LDO              4
#define WL2866D_DISCHARGE_REG        0x02
#define WL2866D_ENABLE_REG           0x0E
#define WL2866D_ENABLE_DISCHARGE_VAL 0xFF
#define WL2866D_RETRY_WAIT_TIME      500000
#define WL2866D_PINCTRL_ENABLE       "wl2866d_enable"
#define WL2866D_PINCTRL_DISABLE      "wl2866d_disable"

enum wl2866d_vdd_type {
	VDD_TYPE_DVDD,
	VDD_TYPE_AVDD,
	VDD_TYPE_MAX,
};

enum wl2866d_vdd_index {
	VDD_INDEX_DVDD1,
	VDD_INDEX_DVDD2,
	VDD_INDEX_AVDD1,
	VDD_INDEX_AVDD2,
	VDD_INDEX_MAX,
};

struct wl2866d_regulator {
	u8 addr;
	int   uv;
	bool *suspended;
	bool  reg_enabled;
	struct device *dev;
	struct regmap *regmap;
	struct device_node    *of_node;
	struct regulator_dev  *rdev;
	struct regulator_desc  rdesc;
	enum wl2866d_vdd_type  type;
	enum wl2866d_vdd_index index;
};

struct wl2866d_pmic {
	bool suspended;
	struct device  *dev;
	struct regmap  *regmap;
	struct pinctrl *pinctrl;
	struct device_node   *of_node;
	struct pinctrl_state *gpio_state_enable;
	struct pinctrl_state *gpio_state_disable;
};

static struct regmap_config wl2866d_regulator_regmap_config = {
	.reg_bits     = 8,
	.val_bits     = 8,
	.max_register = 0xff,
};

struct regulator_data {
	char *name;
	int   min_uv;
	int   max_uv;
	int   step_uv;
};

static const struct regulator_data wl2866d_reg_data[WL2866D_MAX_LDO] = {
	{ "dvdd1",  600000, 1800000,  6000 },
	{ "dvdd2",  600000, 1800000,  6000 },
	{ "avdd1", 1200000, 4300000, 12500 },
	{ "avdd2", 1200000, 4300000, 12500 },
};

static int wl2866d_read(struct regmap *regmap,  u8 reg, u8 *val, int count)
{
	int rc;

	pr_debug("wl2866d read: addr-0x%02x, count-%d\n", reg, count);

	rc = regmap_bulk_read(regmap, reg, val, count);
	if (rc < 0) {
		pr_err("wl2866d read: failed to read 0x%02x\n", reg);
		usleep_range(WL2866D_RETRY_WAIT_TIME,
			     WL2866D_RETRY_WAIT_TIME + 100);

		rc = regmap_bulk_read(regmap, reg, val, count);
		if (rc < 0)
			pr_err("wl2866d read: failed to read 0x%02x again\n", reg);
	}

	return rc;
}

static int wl2866d_write(struct regmap *regmap, u8 reg, const u8 *val,
			 int count)
{
	int rc;

	pr_debug("wl2866d write: addr-0x%02x, data-0x%02x\n", reg, *val);

	rc = regmap_bulk_write(regmap, reg, val, count);
	if (rc < 0) {
		pr_err("wl2866d write: failed to write 0x%02x\n", reg);
		usleep_range(WL2866D_RETRY_WAIT_TIME,
			     WL2866D_RETRY_WAIT_TIME + 100);

		rc = regmap_bulk_write(regmap, reg, val, count);
		if (rc < 0)
			pr_err("wl2866d write: failed to write 0x%02x again\n", reg);
	}

	return rc;
}

static int wl2866d_masked_write(struct regmap *regmap, u8 reg, u8 mask,
				u8 val)
{
	int rc;

	pr_debug("wl2866d masked write: addr-0x%02x, mask-0x%02x, "
			 "masked_data-0x%02x\n", reg, mask, mask & val);

	rc = regmap_update_bits(regmap, reg, mask, val);
	if (rc < 0) {
		pr_err("wl2866d masked write: failed to write 0x%02x to "
			   "0x%02x with mask 0x%02x\n", val, reg, mask);
		usleep_range(WL2866D_RETRY_WAIT_TIME,
			     WL2866D_RETRY_WAIT_TIME + 100);

		rc = regmap_update_bits(regmap, reg, mask, val);
		if (rc < 0)
			pr_err("wl2866d masked write: failed to write 0x%02x to "
			       "0x%02x with mask 0x%02x\n", val, reg, mask);
	}

	return rc;
}

static int wl2866d_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *wl2866d_reg = rdev_get_drvdata(rdev);
	int rc, voltage_uv = 0;
	u8  reg_voltage;

	if (*wl2866d_reg->suspended)
		return wl2866d_reg->uv;

	rc = wl2866d_read(wl2866d_reg->regmap, wl2866d_reg->addr,
			  &reg_voltage, 1);
	if (rc < 0) {
		wl2866d_err(wl2866d_reg,
			    "failed to read regulator voltage rc = %d\n", rc);
		return rc;
	}

	voltage_uv = wl2866d_vdd_real(reg_voltage, wl2866d_reg->type);

	wl2866d_debug(wl2866d_reg, "voltage read: 0x%x -> %duV\n",
		      reg_voltage, voltage_uv);
	return voltage_uv;
}

static int wl2866d_regulator_enable(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *wl2866d_reg = rdev_get_drvdata(rdev);
	int rc, current_uv;

	if (*wl2866d_reg->suspended) {
		if (wl2866d_reg->reg_enabled)
			return 0;
		return -EPERM;
	}

	current_uv = wl2866d_regulator_get_voltage(rdev);
	if (current_uv < 0) {
		wl2866d_err(wl2866d_reg, "failed to get current voltage rc = %d\n",
			    current_uv);
		return current_uv;
	}

	rc = wl2866d_masked_write(wl2866d_reg->regmap, WL2866D_ENABLE_REG,
				  1 << wl2866d_reg->index,
				  1 << wl2866d_reg->index);
	if (rc < 0) {
		wl2866d_err(wl2866d_reg, "failed to enable regulator rc = %d\n",
			    rc);
		return rc;
	}

	wl2866d_reg->reg_enabled = true;
	wl2866d_debug(wl2866d_reg, "regulator enabled\n");
	return rc;
}

static int wl2866d_regulator_disable(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *wl2866d_reg = rdev_get_drvdata(rdev);
	int rc;

	if (*wl2866d_reg->suspended) {
		if (!wl2866d_reg->reg_enabled)
			return 0;
		return -EPERM;
	}

	rc = wl2866d_masked_write(wl2866d_reg->regmap, WL2866D_ENABLE_REG,
				  1 << wl2866d_reg->index, 0);

	if (rc < 0) {
		wl2866d_err(wl2866d_reg, "failed to disable regulator rc = %d\n",
			    rc);
		return rc;
	}

	wl2866d_reg->reg_enabled = false;
	wl2866d_debug(wl2866d_reg, "regulator disabled\n");
	return rc;
}

static int wl2866d_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct wl2866d_regulator *wl2866d_reg = rdev_get_drvdata(rdev);
	u8 en_value;
	int rc;

	if (*wl2866d_reg->suspended)
		return wl2866d_reg->reg_enabled;

	rc = wl2866d_read(wl2866d_reg->regmap, wl2866d_reg->addr, &en_value, 1);
	if (rc < 0) {
		wl2866d_err(wl2866d_reg, "failed to read enable reg rc = %d\n", rc);
		return rc;
	}

	return !!(en_value & 1 << wl2866d_reg->index);
}

static int wl2866d_write_voltage(struct wl2866d_regulator *wl2866d_reg,
				 int min_uv, int max_uv)
{
	int rc = 0, voltage;
	u8  reg_voltage;

	voltage = (DIV_ROUND_UP(min_uv, wl2866d_vdd_step(wl2866d_reg->type))
		   * wl2866d_vdd_step(wl2866d_reg->type));
	if (voltage > max_uv) {
		wl2866d_err(wl2866d_reg, "requested voltage above maximum limit\n");
		return -EINVAL;
	}

	reg_voltage = wl2866d_vdd_reg(voltage, wl2866d_reg->type);

	rc = wl2866d_write(wl2866d_reg->regmap, wl2866d_reg->addr, &reg_voltage, 1);
	if (rc < 0) {
		wl2866d_err(wl2866d_reg, "failed to write voltage rc = %d\n", rc);
		return rc;
	}

	wl2866d_reg->uv = voltage;
	wl2866d_debug(wl2866d_reg, "write register voltage: 0x%x\n", reg_voltage);
	return 0;
}

static int wl2866d_regulator_set_voltage(struct regulator_dev *rdev,
					 int min_uv, int max_uv,
					 unsigned int *selector)
{
	struct wl2866d_regulator *wl2866d_reg = rdev_get_drvdata(rdev);
	int rc;

	if (*wl2866d_reg->suspended) {
		if (min_uv <= wl2866d_reg->uv && wl2866d_reg->uv <= max_uv)
			return 0;
		return -EPERM;
	}

	rc = wl2866d_write_voltage(wl2866d_reg, min_uv, max_uv);
	if (rc < 0)
		return rc;

	wl2866d_debug(wl2866d_reg, "voltage set to %d\n", min_uv);
	return rc;
}

static const struct regulator_ops wl2866d_regulator_ops = {
	.enable      = wl2866d_regulator_enable,
	.disable     = wl2866d_regulator_disable,
	.is_enabled  = wl2866d_regulator_is_enabled,
	.set_voltage = wl2866d_regulator_set_voltage,
	.get_voltage = wl2866d_regulator_get_voltage,
};

static int wl2866d_register_ldo(struct wl2866d_regulator *wl2866d_reg,
				const char *name)
{
	const struct regulator_data *reg_data;
	struct device_node *of_node = wl2866d_reg->of_node;
	struct regulator_init_data *init_data;
	struct regulator_config reg_config = {};
	struct device *dev = wl2866d_reg->dev;
	const char *vdd_type;
	int rc, i;

	reg_data = wl2866d_reg_data;

	for (i = 0; i < WL2866D_MAX_LDO; i++)
		if (strstr(name, reg_data[i].name))
			break;
	if (i == WL2866D_MAX_LDO) {
		pr_err("wl2866d_pmic: invalid regulator name %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32(of_node, "cell-index", &wl2866d_reg->index);
	if (rc < 0) {
		wl2866d_err(wl2866d_reg, "failed to get regulator index rc = %d\n",
			    rc);
		return rc;
	}

	rc = of_property_read_u8(of_node, "reg", &wl2866d_reg->addr);
	if (rc < 0) {
		wl2866d_err(wl2866d_reg, "failed to get regulator register rc = %d\n",
			    rc);
		return rc;
	}

	rc = of_property_read_string(of_node, "type", &vdd_type);
	if (rc < 0) {
		wl2866d_err(wl2866d_reg, "failed to get regulator type rc = %d\n",
			    rc);
		return rc;
	}

	if (!strcmp(vdd_type, "dvdd"))
		wl2866d_reg->type = VDD_TYPE_DVDD;
	else if (!strcmp(vdd_type, "avdd"))
		wl2866d_reg->type = VDD_TYPE_AVDD;
	else {
		wl2866d_err(wl2866d_reg, "invalid regulator type %s\n",
			    wl2866d_reg->type);
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(dev, of_node, &wl2866d_reg->rdesc);
	if (init_data == NULL) {
		wl2866d_err(wl2866d_reg, "failed to get regulator data\n");
		return -ENODATA;
	}
	if (!init_data->constraints.name) {
		wl2866d_err(wl2866d_reg, "regulator name missing\n");
		return -EINVAL;
	}

	init_data->constraints.input_uV = init_data->constraints.max_uV;
	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_STATUS
						 | REGULATOR_CHANGE_VOLTAGE;
	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = wl2866d_reg;
	reg_config.of_node = of_node;

	wl2866d_reg->reg_enabled = false;
	wl2866d_reg->uv = reg_data[i].min_uv;

	wl2866d_reg->rdesc.type = REGULATOR_VOLTAGE;
	wl2866d_reg->rdesc.ops = &wl2866d_regulator_ops;
	wl2866d_reg->rdesc.name = init_data->constraints.name;
	wl2866d_reg->rdesc.uV_step = reg_data[i].step_uv;
	wl2866d_reg->rdesc.min_uV = reg_data[i].min_uv;
	wl2866d_reg->rdesc.n_voltages
		= ((reg_data[i].max_uv - reg_data[i].min_uv)
			/ wl2866d_reg->rdesc.uV_step) + 1;

	wl2866d_reg->rdev = devm_regulator_register(dev, &wl2866d_reg->rdesc,
						    &reg_config);
	if (IS_ERR(wl2866d_reg->rdev)) {
		rc = PTR_ERR(wl2866d_reg->rdev);
		wl2866d_err(wl2866d_reg, "failed to register regulator rc = %d\n",
			    rc);
		return rc;
	}

	rc = devm_regulator_debug_register(dev, wl2866d_reg->rdev);
	if (rc)
		wl2866d_err(wl2866d_reg, "failed to register regulator rc = %d\n",
			    rc);

	wl2866d_debug(wl2866d_reg, "regulator registered\n");
	return 0;
}

static int wl2866d_pmic_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct wl2866d_pmic *chip;
	struct wl2866d_regulator *wl2866d_reg;
	struct device_node *child;
	const char *name;
	int rc = 0;
	u8  reg_val;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &client->dev;
	chip->regmap = devm_regmap_init_i2c(client, &wl2866d_regulator_regmap_config);
	if (!chip->regmap)
		return -ENODEV;
	chip->pinctrl = devm_pinctrl_get(&client->dev);
	if (!chip->pinctrl) {
		return -ENODEV;
	}

	chip->gpio_state_enable =
		pinctrl_lookup_state(chip->pinctrl, WL2866D_PINCTRL_ENABLE);
	if (IS_ERR_OR_NULL(chip->gpio_state_enable)) {
		pr_err("wl2866d_pmic: failed to get wl2866d enabled pinctrl\n");
		return -EINVAL;
	}
	chip->gpio_state_disable =
		pinctrl_lookup_state(chip->pinctrl, WL2866D_PINCTRL_DISABLE);
	if (IS_ERR_OR_NULL(chip->gpio_state_disable)) {
		pr_err("wl2866d_pmic: failed to get wl2866d disabled pinctrl\n");
		return -EINVAL;
	}
	rc = pinctrl_select_state(chip->pinctrl, chip->gpio_state_disable);
	if (rc)
		pr_err("wl2866d_pmic: failed to set wl2866d pin to disable rc = %d\n",
			    rc);

	i2c_set_clientdata(client, chip);

	for_each_available_child_of_node(chip->dev->of_node, child) {
		wl2866d_reg = devm_kzalloc(chip->dev, sizeof(*wl2866d_reg), GFP_KERNEL);
		if (!wl2866d_reg)
			return -ENOMEM;

		wl2866d_reg->dev       = chip->dev;
		wl2866d_reg->regmap    = chip->regmap;
		wl2866d_reg->of_node   = child;
		wl2866d_reg->suspended = &(chip->suspended);

		rc = of_property_read_string(child, "regulator-name", &name);
		if (rc < 0) {
			wl2866d_err(wl2866d_reg, "failed to read register name rc = %d\n",
				    rc);
			return rc;
		}

		rc = wl2866d_register_ldo(wl2866d_reg, name);
		if (rc < 0) {
			wl2866d_err(wl2866d_reg, "failed to register regulator rc = %d\n",
				    rc);
			return rc;
		}
	}

	/* enable regulator output discharge func but none fatal */
	reg_val = WL2866D_ENABLE_DISCHARGE_VAL;
	rc = wl2866d_write(chip->regmap, WL2866D_DISCHARGE_REG, &reg_val, 1);
	if (rc < 0) {
		pr_err("wl2866d: failed to enable discharge rc = %d\n", rc);
		rc = 0;
	}

	dev_set_drvdata(&client->dev, chip);
	pr_debug("wl2866d pimc probe successful\n");
	return rc;
}

static int wl2866d_pmic_remove(struct i2c_client *client)
{
	i2c_set_clientdata(client, NULL);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int wl2866d_pmic_suspend(struct device *dev)
{
	struct wl2866d_pmic *chip = dev_get_drvdata(dev);

	chip->suspended = true;

	return 0;
}

static int wl2866d_pmic_resume(struct device *dev)
{
	struct wl2866d_pmic *chip = dev_get_drvdata(dev);

	chip->suspended = false;

	return 0;
}
#else
static int wl2866d_pmic_suspend(struct device *dev)
{
	return 0;
}

static int wl2866d_pmic_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops wl2866d_pmic_pm_ops = {
	.suspend = wl2866d_pmic_suspend,
	.resume  = wl2866d_pmic_resume,
};

static const struct of_device_id wl2866d_pmic_match_table[] = {
	{ .compatible = "xiaomi,wl2866d-pmic" },
	{ },
};

static const struct i2c_device_id wl2866d_pmic_id[] = {
	{ "wl2866d-pmic", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, wl2866d_pmic_id);

static struct i2c_driver wl2866d_pmic_driver = {
	.driver = {
		.name = "wl2866d_pmic",
		.pm   = &wl2866d_pmic_pm_ops,
		.of_match_table = wl2866d_pmic_match_table,
	},
	.probe    = wl2866d_pmic_probe,
	.remove   = wl2866d_pmic_remove,
	.id_table = wl2866d_pmic_id,
};

module_i2c_driver(wl2866d_pmic_driver);

MODULE_DESCRIPTION("Xiaomi WL2866D PMIC Driver");
MODULE_LICENSE("GPL v2");
