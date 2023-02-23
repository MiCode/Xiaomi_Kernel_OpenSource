/* Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

/*	date			author			comment
 *	2021-05-25		chenyichun@xiaomi.com	create driver for SC8551
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/hwid.h>
#include <mt-plat/v1/charger_class.h>
#include "sc8551.h"

enum product_name {
	UNKNOW,
	RUBY,
	RUBYPRO,
	RUBYPLUS,
};

enum sc8551_driver_data {
	SC8551_STANDALONE,
	SC8551_SLAVE,
	SC8551_MASTER,
};

enum sc8551_work_mode {
	WM_STANDALONE,
	WM_SLAVE,
	WM_MASTER,
	WM_RESERVED,
};

enum sc8551_adc_channel {
        ADC_IBUS,
	ADC_VBUS,
	ADC_VAC,
	ADC_VOUT,
	ADC_VBAT,
	ADC_IBAT,
	ADC_TDIE,
};

struct sc8551 {
	struct i2c_client *client;
	struct device *dev;
	struct regmap *regmap;

	bool chip_ok;
	char model_name[I2C_NAME_SIZE];
	char log_tag[25];

	struct charger_device *chg_dev;
	struct power_supply *charger;
	struct power_supply_desc psy_desc;
	struct wakeup_source irq_handle_wakelock;
	struct delayed_work irq_handle_work;
	int irq_gpio;
	int irq;

	int ac_ovp;
	int bus_ovp;
	int bus_ocp;
	int bat_ovp;
	int bat_ocp;
};

struct adc_desc {
	u8 reg;
	u16 mask;
	int step;
	int rate;
};

static const char * const adc_channel_name[] = {
	[ADC_IBUS]	= "IBUS",
	[ADC_VBUS]	= "VBUS",
	[ADC_VAC]	= "VAC",
	[ADC_VOUT]	= "VOUT",
	[ADC_VBAT]	= "VBAT",
	[ADC_IBAT]	= "IBAT",
	[ADC_TDIE]	= "TDIE",
};

static struct adc_desc adc_desc_table[] = {
	[ADC_IBUS] = {SC8551_ADC_IBUS_REG, SC8551_ADC_IBUS_MASK, SC8551_ADC_IBUS_LSB, SC8551_ADC_IBUS_RATE},
	[ADC_VBUS] = {SC8551_ADC_VBUS_REG, SC8551_ADC_VBUS_MASK, SC8551_ADC_VBUS_LSB, SC8551_ADC_VBUS_RATE},
	[ADC_VAC] = {SC8551_ADC_VAC_REG, SC8551_ADC_VAC_MASK, SC8551_ADC_VAC_LSB, SC8551_ADC_VAC_RATE},
	[ADC_VOUT] = {SC8551_ADC_VOUT_REG, SC8551_ADC_VOUT_MASK, SC8551_ADC_VOUT_LSB, SC8551_ADC_VOUT_RATE},
	[ADC_VBAT] = {SC8551_ADC_VBAT_REG, SC8551_ADC_VBAT_MASK, SC8551_ADC_VBAT_LSB, SC8551_ADC_VBAT_RATE},
	[ADC_IBAT] = {SC8551_ADC_IBAT_REG, SC8551_ADC_IBAT_MASK, SC8551_ADC_IBAT_LSB, SC8551_ADC_IBAT_RATE},
	[ADC_TDIE] = {SC8551_ADC_TDIE_REG, SC8551_ADC_TDIE_MASK, SC8551_ADC_TDIE_LSB, SC8551_ADC_TDIE_RATE},
};

static struct regmap_config sc8551_regmap_config = {
	.reg_bits  = 8,
	.val_bits  = 8,
	.max_register  = 0x31,
};

static int log_level = 2;
static int product_name = UNKNOW;
static int fake_work_mode = SC8551_MASTER;

#define sc_err(fmt, ...)					\
do {								\
	if (log_level >= 0)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define sc_info(fmt, ...)					\
do {								\
	if (log_level >= 1)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

#define sc_dbg(fmt, ...)					\
do {								\
	if (log_level >= 2)					\
			printk(KERN_ERR "" fmt, ##__VA_ARGS__);	\
} while (0)

static int sc8551_enable_adc(struct sc8551 *chip, bool enable)
{
	int ret = 0;

	ret = regmap_update_bits(chip->regmap, SC8551_ADC_CTRL_REG, SC8551_ENABLE_ADC_BIT, enable ? SC8551_ENABLE_ADC_BIT : 0);

	return ret;
}

static int sc8551_set_bus_ovp(struct sc8551 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < SC8551_BUS_OVP_BASE)
		value = SC8551_BUS_OVP_BASE;

	data = ((value - SC8551_BUS_OVP_BASE) / SC8551_BUS_OVP_LSB) << SC8551_BUS_OVP_SHIFT;

	ret = regmap_update_bits(chip->regmap, SC8551_BUS_OVP_REG, SC8551_BUS_OVP_MASK, data);
	if (ret)
		sc_err("%s I2C failed to set BUS_OVP\n", chip->log_tag);

	return ret;
}

static int sc8551_set_ac_ovp(struct sc8551 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < SC8551_VAC_OVP_BASE)
		value = SC8551_VAC_OVP_BASE;

	data = ((value - SC8551_VAC_OVP_BASE) / SC8551_VAC_OVP_LSB) << SC8551_VAC_OVP_SHIFT;

	ret = regmap_update_bits(chip->regmap, SC8551_AC_PROTECT_REG, SC8551_VAC_OVP_MASK, data);
	if (ret)
		sc_err("%s I2C failed to set AC_OVP\n", chip->log_tag);

	return ret;
}

static int sc8551_enable_bypass(struct sc8551 *chip, bool enable)
{
	int ret = 0;

	sc_info("%s [ENABLE_BYPASS] %d\n", chip->log_tag, enable);

	ret = regmap_update_bits(chip->regmap, SC8551_BYPASS_REG, SC8551_ENABLE_BYPASS_BIT, enable ? SC8551_ENABLE_BYPASS_BIT : 0);

	/* in bypass mode, ovp will be set to half value automatically */
	/* in charge_pump mode, should set it manually */
	if (!enable) {
		ret = sc8551_set_bus_ovp(chip,chip->bus_ovp);
		ret = sc8551_set_ac_ovp(chip, chip->ac_ovp);
	}

	return ret;
}

static int sc8551_get_bypass_enable(struct sc8551 *chip, bool *enable)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(chip->regmap, SC8551_BYPASS_REG, &data);

	*enable = !!(data & SC8551_ENABLE_BYPASS_BIT);

	return ret;
}

static int sc8551_enable_charge(struct sc8551 *chip, bool enable)
{
	int ret = 0;

	sc_info("%s [ENABLE_CHARGE_PUMP] %d\n", chip->log_tag, enable);

	ret = regmap_update_bits(chip->regmap, SC8551_CHG_CTRL_REG, SC8551_ENABLE_CHG_BIT, enable ? SC8551_ENABLE_CHG_BIT : 0);

	return ret;
}

static int sc8551_get_charge_enable(struct sc8551 *chip, bool *enable)
{
	int ret = 0;
	unsigned int data = 0;

	ret = regmap_read(chip->regmap, SC8551_CHG_CTRL_REG, &data);

	*enable = !!(data & SC8551_ENABLE_CHG_BIT);

	return ret;
}

static int sc8551_read_adc(struct sc8551 *chip, int channel, int *value)
{
	int tchg_result = 0, ret = 0;
	u8 data[2] = {0, 0};
	u16 abs_value = 0;

	if (channel < ADC_IBUS || channel > ADC_TDIE) {
		sc_err("%s not support ADC channel\n", chip->log_tag);
		return -1;
	}

	ret = regmap_raw_read(chip->regmap, adc_desc_table[channel].reg, data, 2);
	if (ret) {
		sc_err("%s I2C failed to read ADC\n", chip->log_tag);
		return -1;
	}

	if (channel == ADC_TDIE) {
		abs_value = ((data[0] & 0x7F) << 8) | data[1];
		tchg_result = abs_value * adc_desc_table[ADC_TDIE].step / adc_desc_table[ADC_TDIE].rate;
		if (data[0] & 0x80)
			*value = -tchg_result;
		else
			*value = tchg_result;
	} else {
		*value = ((((data[0] << 8) + data[1]) & adc_desc_table[channel].mask) * adc_desc_table[channel].step) / adc_desc_table[channel].rate;
	}

	return ret;
}

static int sc8551_set_bus_ocp(struct sc8551 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < SC8551_BUS_OCP_BASE)
		value = SC8551_BUS_OCP_BASE;

	data = ((value - SC8551_BUS_OCP_BASE) / SC8551_BUS_OCP_LSB) << SC8551_BUS_OCP_SHIFT;

	ret = regmap_update_bits(chip->regmap, SC8551_BUS_OCP_UCP_REG, SC8551_BUS_OCP_MASK, data);
	if (ret)
		sc_err("%s I2C failed to set BUS_OCP\n", chip->log_tag);

	return ret;
}

static int sc8551_set_bat_ovp(struct sc8551 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < SC8551_BAT_OVP_BASE)
		value = SC8551_BAT_OVP_BASE;

	data = ((value - SC8551_BAT_OVP_BASE) / SC8551_BAT_OVP_LSB) << SC8551_BAT_OVP_SHIFT;

	ret = regmap_update_bits(chip->regmap, SC8551_BAT_OVP_REG, SC8551_BAT_OVP_MASK, data);
	if (ret)
		sc_err("%s I2C failed to set BAT_OVP\n", chip->log_tag);

	return ret;
}

static int sc8551_set_bat_ocp(struct sc8551 *chip, int value)
{
	int ret = 0;
	u8 data = 0;

	if (value < SC8551_BAT_OCP_BASE)
		value = SC8551_BAT_OCP_BASE;

	data = ((value - SC8551_BAT_OCP_BASE) / SC8551_BAT_OCP_LSB) << SC8551_BAT_OCP_SHIFT;

	ret = regmap_update_bits(chip->regmap, SC8551_BAT_OCP_REG, SC8551_BAT_OCP_MASK, data);
	if (ret)
		sc_err("%s I2C failed to set BAT_OCP\n", chip->log_tag);

	return ret;
}

static int sc8551_dump_registers(struct sc8551 *chip)
{
	u8 data[sc8551_regmap_config.max_register + 1];
	int i = 0, ret = 0;

	ret = regmap_bulk_read(chip->regmap, 0x00, data, sc8551_regmap_config.max_register + 1);
	if (ret) {
		sc_err("%s failed to read registers\n", chip->log_tag);
		return ret;
	}

	printk(KERN_ERR "%s", chip->log_tag);
	for (i = 0; i <= sc8551_regmap_config.max_register; i++)
		printk(KERN_ERR "0x%02x ", i);
	printk(KERN_ERR "\n");
	printk(KERN_ERR "%s", chip->log_tag);
	for (i = 0; i <= sc8551_regmap_config.max_register; i++)
		printk(KERN_ERR "0x%02x ", data[i]);
	printk(KERN_ERR "\n");

	return ret;
}

static int ops_sc8551_enable_charge(struct charger_device *chg_dev, bool enable)
{
	struct sc8551 *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc8551_enable_charge(chip, enable);
	if (ret)
		sc_err("%s ops failed to enable charge\n", chip->log_tag);

	return ret;
}

static int ops_sc8551_get_charge_enable(struct charger_device *chg_dev, bool *enabled)
{
	struct sc8551 *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc8551_get_charge_enable(chip, enabled);
	if (ret)
		sc_err("%s ops failed to get charge_enable\n", chip->log_tag);

	return ret;
}

static int ops_sc8551_enable_bypass(struct charger_device *chg_dev, bool enable)
{
	struct sc8551 *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc8551_enable_bypass(chip, enable);
	if (ret)
		sc_err("%s ops failed to enable BYPASS\n", chip->log_tag);

	return ret;
}

static int ops_sc8551_get_vbus(struct charger_device *chg_dev, u32 *value)
{
	struct sc8551 *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc8551_read_adc(chip, ADC_VBUS, value);
	if (ret)
		sc_err("%s ops failed to get VBUS\n", chip->log_tag);

	return ret;
}

static int ops_sc8551_get_ibus(struct charger_device *chg_dev, u32 *value)
{
	struct sc8551 *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc8551_read_adc(chip, ADC_IBUS, value);
	if (ret)
		sc_err("%s ops failed to get IBUS\n", chip->log_tag);

	return ret;
}

static int ops_sc8551_get_tchg(struct charger_device *chg_dev, int *value_min, int *value_max)
{
	struct sc8551 *chip = charger_get_data(chg_dev);
	int tchg = 0, ret = 0;

	ret = sc8551_read_adc(chip, ADC_TDIE, &tchg);
	if (ret) {
		tchg = 250;
		sc_err("%s ops failed to get charge_status\n", chip->log_tag);
	}

	*value_max = *value_min = tchg / 10;

	return ret;
}

static int ops_sc8551_is_bypass_enabled(struct charger_device *chg_dev, bool *enabled)
{
	struct sc8551 *chip = charger_get_data(chg_dev);
	int ret = 0;

	ret = sc8551_get_bypass_enable(chip, enabled);

	return ret;
}

static const struct charger_ops sc8551_chg_ops = {
	.enable = ops_sc8551_enable_charge,
	.is_enabled = ops_sc8551_get_charge_enable,
	.enable_bypass = ops_sc8551_enable_bypass,
	.is_bypass_enabled = ops_sc8551_is_bypass_enabled,
	.get_vbus_adc = ops_sc8551_get_vbus,
	.get_ibus_adc = ops_sc8551_get_ibus,
	.get_tchg_adc = ops_sc8551_get_tchg,
};

static const struct charger_properties sc8551_standalone_chg_props = {
	.alias_name = "cp_standalone",
};

static const struct charger_properties sc8551_slave_chg_props = {
	.alias_name = "cp_slave",
};

static const struct charger_properties sc8551_master_chg_props = {
	.alias_name = "cp_master",
};

static int sc8551_register_charger(struct sc8551 *chip, int work_mode)
{
	switch (work_mode) {
	case SC8551_STANDALONE:
		chip->chg_dev = charger_device_register("cp_standalone", chip->dev, chip, &sc8551_chg_ops, &sc8551_standalone_chg_props);
		break;
	case SC8551_SLAVE:
		chip->chg_dev = charger_device_register("cp_slave", chip->dev, chip, &sc8551_chg_ops, &sc8551_slave_chg_props);
		break;
	case SC8551_MASTER:
		chip->chg_dev = charger_device_register("cp_master", chip->dev, chip, &sc8551_chg_ops, &sc8551_master_chg_props);
		break;
	default:
		sc_err("%s not support work_mode\n", chip->log_tag);
		return -EINVAL;
	}

	return 0;
}

static enum power_supply_property sc8551_power_supply_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CHIP_OK,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_CHARGE_ENABLED,
	POWER_SUPPLY_PROP_BYPASS,
	POWER_SUPPLY_PROP_BYPASS_SUPPORT,
	POWER_SUPPLY_PROP_CP_VBAT,
	POWER_SUPPLY_PROP_CP_IBAT,
	POWER_SUPPLY_PROP_CP_DIE_TEMP,
};

static int sc8551_property_is_writeable(struct power_supply *psy, enum power_supply_property prop)
{
	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
	case POWER_SUPPLY_PROP_BYPASS:
		return true;
	default:
		return false;
	}
}

static int sc8551_set_property(struct power_supply *psy, enum power_supply_property prop, const union power_supply_propval *val)
{
	struct sc8551 *chip = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		sc8551_enable_charge(chip, val->intval);
		break;
	case POWER_SUPPLY_PROP_BYPASS:
		sc8551_enable_bypass(chip, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sc8551_get_property(struct power_supply *psy, enum power_supply_property psp, union power_supply_propval *val)
{
	struct sc8551 *chip = power_supply_get_drvdata(psy);
	bool enable = false;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_CHIP_OK:
		val->intval = chip->chip_ok;
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = chip->model_name;
		break;
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
		ret = sc8551_get_charge_enable(chip, &enable);
		if (ret)
			val->intval = 0;
		else
			val->intval = enable;
		break;
	case POWER_SUPPLY_PROP_BYPASS:
		ret = sc8551_get_bypass_enable(chip, &enable);
		if (ret)
			val->intval = 0;
		else
			val->intval = enable;
		break;
	case POWER_SUPPLY_PROP_BYPASS_SUPPORT:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sc8551_read_adc(chip, ADC_VBUS, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = sc8551_read_adc(chip, ADC_IBUS, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_VBAT:
		ret = sc8551_read_adc(chip, ADC_VBAT, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_IBAT:
		ret = sc8551_read_adc(chip, ADC_IBAT, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CP_DIE_TEMP:
		ret = sc8551_read_adc(chip, ADC_TDIE, &val->intval);
		if (ret)
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sc8551_power_supply_init(struct sc8551 *chip, struct device *dev, int work_mode)
{
	struct power_supply_config psy_cfg = { .drv_data = chip,
						.of_node = dev->of_node, };

	switch (work_mode) {
	case SC8551_MASTER:
		chip->psy_desc.name = "cp_master";
		break;
	case SC8551_SLAVE:
		chip->psy_desc.name = "cp_slave";
		break;
	case SC8551_STANDALONE:
		chip->psy_desc.name = "cp_standalone";
		break;
	default:
		sc_err("%s not support work_mode\n", chip->log_tag);
		return -EINVAL;
	}

	chip->psy_desc.type = POWER_SUPPLY_TYPE_UNKNOWN,
	chip->psy_desc.properties = sc8551_power_supply_props,
	chip->psy_desc.num_properties = ARRAY_SIZE(sc8551_power_supply_props),
	chip->psy_desc.get_property = sc8551_get_property,
	chip->psy_desc.set_property = sc8551_set_property,
	chip->psy_desc.property_is_writeable = sc8551_property_is_writeable,

	chip->charger = devm_power_supply_register(chip->dev, &chip->psy_desc, &psy_cfg);
	if (IS_ERR(chip->charger))
		return -EINVAL;

	return 0;
}

static ssize_t sc8551_show_register(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sc8551 *chip = dev_get_drvdata(dev);
	u8 tmpbuf[300];
	unsigned int reg = 0, data = 0;
	int len = 0, idx = 0, ret = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "SC8551_REG");
	for (reg = 0x00; reg <= sc8551_regmap_config.max_register; reg++) {
		ret = regmap_read(chip->regmap, reg, &data);
		if (ret) {
			sc_err("%s failed to read register\n", chip->log_tag);
			return idx;
		}

		len = snprintf(tmpbuf, PAGE_SIZE - idx, "Reg[0x%02x] = 0x%02x\n", reg, data);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}

	return idx;
}

static ssize_t sc8551_show_adc(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sc8551 *chip = dev_get_drvdata(dev);
	u8 tmpbuf[150];
	int channel = 0, value = 0, len = 0, idx = 0, ret = 0;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "SC8551_ADC");
	for (channel = ADC_IBUS; channel <= ADC_TDIE; channel++) {
		ret = sc8551_read_adc(chip, channel, &value);
		if (ret) {
			sc_err("%s failed to read ADC\n", chip->log_tag);
			return idx;
		}

		len = snprintf(tmpbuf, PAGE_SIZE - idx, "%s = %d\n", adc_channel_name[channel], value);
		memcpy(&buf[idx], tmpbuf, len);
		idx += len;
	}

	return idx;
}

static ssize_t sc8551_store_registers(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sc8551 *chip = dev_get_drvdata(dev);
	unsigned int reg = 0, data = 0;
	int ret = 0;

	sscanf(buf, "%x %x", &reg, &data);

	ret = regmap_write(chip->regmap, reg, data);
	if (ret)
		sc_err("%s failed to write register\n", chip->log_tag);

	return count;
}

static ssize_t sc8551_show_log_level(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sc8551 *chip = dev_get_drvdata(dev);
	int ret = 0;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", log_level);
	sc_info("%s show log_level = %d\n", chip->log_tag, log_level);

	return ret;
}

static ssize_t sc8551_store_log_level(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct sc8551 *chip = dev_get_drvdata(dev);
	int ret = 0;

	ret = sscanf(buf, "%d", &log_level);
	sc_info("%s store log_level = %d\n", chip->log_tag, log_level);

	return count;
}

static DEVICE_ATTR(register, S_IRUGO | S_IWUSR, sc8551_show_register, sc8551_store_registers);
static DEVICE_ATTR(adc, S_IRUGO, sc8551_show_adc, NULL);
static DEVICE_ATTR(log_level, S_IRUGO | S_IWUSR, sc8551_show_log_level, sc8551_store_log_level);

static struct attribute *sc8551_attributes[] = {
	&dev_attr_register.attr,
	&dev_attr_adc.attr,
	&dev_attr_log_level.attr,
	NULL,
};

static const struct attribute_group sc8551_attr_group = {
	.attrs = sc8551_attributes,
};

static void sc8551_irq_handler(struct work_struct *work)
{
//	struct sc8551 *chip = container_of(work, struct sc8551, irq_handle_work.work);
//	int ret = 0;

	//ret = sc8551_dump_registers(chip);
//	if (ret)
//		sc_err("%s failed to dump registers\n", chip->log_tag);

//	__pm_relax(&chip->irq_handle_wakelock);

	return;
}

static irqreturn_t sc8551_interrupt(int irq, void *private)
{
/*
	struct sc8551 *chip = private;

	sc_info("%s sc8551_interrupt\n", chip->log_tag);

	if (chip->irq_handle_wakelock.active)
		return IRQ_HANDLED;
	else
		__pm_stay_awake(&chip->irq_handle_wakelock);

	schedule_delayed_work(&chip->irq_handle_work, 0);
*/
	return IRQ_HANDLED;
}

static int sc8551_init_irq(struct sc8551 *chip)
{
	int ret = 0;

	ret = devm_gpio_request(chip->dev, chip->irq_gpio, dev_name(chip->dev));
	if (ret) {
		sc_err("%s failed to request gpio\n", chip->log_tag);
		return ret;
	}

	chip->irq = gpio_to_irq(chip->irq_gpio);
	if (chip->irq < 0) {
		sc_err("%s failed to get gpio_irq\n", chip->log_tag);
		return -1;
	}

	ret = request_irq(chip->irq, sc8551_interrupt, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, dev_name(chip->dev), chip);
	if (ret) {
		sc_err("%s failed to request irq\n", chip->log_tag);
		return ret;
	}

	return ret;
}

static int sc8551_parse_dt(struct sc8551 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int ret = 0;

	if (!np) {
		sc_err("%s device tree info missing\n", chip->log_tag);
		return -1;
	}

	ret = of_property_read_u32(np, "ac_ovp", &chip->ac_ovp);
	if (ret)
		sc_err("%s failed to parse ac_ovp\n", chip->log_tag);

	ret = of_property_read_u32(np, "bus_ovp", &chip->bus_ovp);
	if (ret)
		sc_err("%s failed to parse bus_ovp\n", chip->log_tag);

	ret = of_property_read_u32(np, "bus_ocp", &chip->bus_ocp);
	if (ret)
		sc_err("%s failed to parse bus_ocp\n", chip->log_tag);

	ret = of_property_read_u32(np, "bat_ovp", &chip->bat_ovp);
	if (ret)
		sc_err("%s failed to parse bat_ovp\n", chip->log_tag);

	ret = of_property_read_u32(np, "bat_ocp", &chip->bat_ocp);
	if (ret)
		sc_err("%s failed to parse bat_ocp\n", chip->log_tag);

	sc_info("%s parse config, [ac_ovp bus_ovp bus_ocp bat_ovp bat_ocp] = [%d %d %d %d %d]\n", chip->log_tag,
		chip->ac_ovp, chip->bus_ovp, chip->bus_ocp, chip->bat_ovp, chip->bat_ocp);

	chip->irq_gpio = of_get_named_gpio(np, "sc8551_irq_gpio", 0);
	if (!gpio_is_valid(chip->irq_gpio)) {
		sc_err("%s failed to parse sc8551_irq_gpio\n", chip->log_tag);
		return -1;
	}

	return ret;
}

static int sc8551_check_work_mode(struct sc8551 *chip, int driver_data)
{
	unsigned int data = 0;
	int work_mode = WM_STANDALONE, ret = 0;

	ret = regmap_read(chip->regmap, SC8551_CHG_CTRL_REG, &data);
	if (ret) {
		sc_err("failed to read work_mode\n");
		return ret;
	}

	work_mode = (data & SC8551_WORK_MODE_MASK) >> SC8551_WORK_MODE_SHIFT;
	if (work_mode != driver_data) {
		sc_err("work_mode not match, work_mode = %d, driver_data = %d\n", work_mode, driver_data);
		return -EINVAL;
	}

	switch (fake_work_mode) {
	case SC8551_STANDALONE:
		strcpy(chip->log_tag, "[XMCHG_SC8551_ALONE]");
		break;
	case SC8551_SLAVE:
		strcpy(chip->log_tag, "[XMCHG_SC8551_SLAVE]");
		break;
	case SC8551_MASTER:
		strcpy(chip->log_tag, "[XMCHG_SC8551_MASTER]");
		break;
	default:
		sc_err("not support work_mode\n");
		return -EINVAL;
	}

	return ret;
}

static int sc8551_init_adc(struct sc8551 *chip)
{
	int ret = 0;

	ret = regmap_update_bits(chip->regmap, SC8551_ADC_CTRL_REG, SC8551_DISABLE_IBUS_ADC_BIT, 0);
	ret = regmap_update_bits(chip->regmap, SC8551_ADC_FN_DISABLE_REG, SC8551_DISABLE_VBUS_ADC_BIT, 0);
	ret = regmap_update_bits(chip->regmap, SC8551_ADC_FN_DISABLE_REG, SC8551_DISABLE_VAC_ADC_BIT, SC8551_DISABLE_VAC_ADC_BIT);
	ret = regmap_update_bits(chip->regmap, SC8551_ADC_FN_DISABLE_REG, SC8551_DISABLE_VOUT_ADC_BIT, SC8551_DISABLE_VOUT_ADC_BIT);
	ret = regmap_update_bits(chip->regmap, SC8551_ADC_FN_DISABLE_REG, SC8551_DISABLE_VBAT_ADC_BIT, SC8551_DISABLE_VBAT_ADC_BIT);
	ret = regmap_update_bits(chip->regmap, SC8551_ADC_FN_DISABLE_REG, SC8551_DISABLE_IBAT_ADC_BIT, SC8551_DISABLE_IBAT_ADC_BIT);
	ret = regmap_update_bits(chip->regmap, SC8551_ADC_FN_DISABLE_REG, SC8551_DISABLE_TDIE_ADC_BIT, 0);

	ret = sc8551_enable_adc(chip, true);

	return ret;
}

static int sc8551_init_device(struct sc8551 *chip)
{
	int ret = 0;

	ret = regmap_update_bits(chip->regmap, SC8551_BAT_OVP_REG, SC8551_BAT_OVP_DIS_BIT, SC8551_BAT_OVP_DIS_BIT);
	ret = regmap_update_bits(chip->regmap, SC8551_BAT_OCP_REG, SC8551_BAT_OCP_DIS_BIT, SC8551_BAT_OCP_DIS_BIT);
	ret = regmap_update_bits(chip->regmap, SC8551_BUS_OCP_UCP_REG, SC8551_BUS_OCP_DIS_BIT, SC8551_BUS_OCP_DIS_BIT);
	ret = regmap_update_bits(chip->regmap, SC8551_CTRL_REG, SC8551_WD_TIMEOUT_DIS_BIT, SC8551_WD_TIMEOUT_DIS_BIT);

	ret = sc8551_set_bus_ovp(chip, chip->bus_ovp);
	if (ret) {
		sc_err("%s failed to set BUS_OVP", chip->log_tag);
		return ret;
	}

	ret = sc8551_set_bus_ocp(chip, chip->bus_ocp);
	if (ret) {
		sc_err("%s failed to set BUS_OCP", chip->log_tag);
		return ret;
	}

	ret = sc8551_set_ac_ovp(chip, chip->ac_ovp);
	if (ret) {
		sc_err("%s failed to set AC_OVP", chip->log_tag);
		return ret;
	}

	ret = sc8551_set_bat_ovp(chip, chip->bat_ovp);
	if (ret) {
		sc_err("%s failed to set BAT_OVP", chip->log_tag);
		return ret;
	}

	ret = sc8551_set_bat_ocp(chip, chip->bat_ocp);
	if (ret) {
		sc_err("%s failed to set BAT_OCP", chip->log_tag);
		return ret;
	}

	ret = sc8551_init_adc(chip);
	if (ret) {
		sc_err("%s failed to init ADC", chip->log_tag);
		return ret;
	}

	return ret;
}

static void sc8551_parse_cmdline(void)
{
	char *ruby = NULL, *rubypro = NULL, *rubyplus = NULL;
	const char *sku = get_hw_sku();

	ruby = strnstr(sku, "ruby", strlen(sku));
	rubypro = strnstr(sku, "rubypro", strlen(sku));
	rubyplus = strnstr(sku, "rubyplus", strlen(sku));

	if (rubyplus)
		product_name = RUBYPLUS;
	else if (rubypro)
		product_name = RUBYPRO;
	else if (ruby)
		product_name = RUBY;

	sc_info("product_name = %d, ruby = %d, rubypro = %d, rubyplus = %d\n", product_name, ruby ? 1 : 0, rubypro ? 1 : 0, rubyplus ? 1 : 0);
}

static int sc8551_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sc8551 *chip;
	int ret = 0;

	sc8551_parse_cmdline();

	if (product_name == RUBY) {
		sc_info("SC8551 probe start\n");
	} else {
		sc_info("rubypro and rubyplus no need to probe SC8551\n");
		return -ENODEV;
	}

	/* detect device on connected i2c bus */
	ret = i2c_smbus_read_byte_data(client, SC8551_PART_INFO_REG);
	if (IS_ERR_VALUE((unsigned long)ret)) {
		dev_err(&client->dev, "fail to detect sc8551 on i2c_bus(addr=0x%x), retry\n", client->addr);
		msleep(250);
		ret = i2c_smbus_read_byte_data(client, SC8551_PART_INFO_REG);
		if (IS_ERR_VALUE((unsigned long)ret)) {
			dev_err(&client->dev, "fail to detect sc8551 on i2c_bus(addr=0x%x)\n", client->addr);
			return -ENODEV;
		}
	}
	dev_info(&client->dev, "device id=0x%x\n", ret);

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->dev = dev;
	i2c_set_clientdata(client, chip);

	//wakeup_source_init(&chip->irq_handle_wakelock, "sc8551_irq_handle_wakelock");
	INIT_DELAYED_WORK(&chip->irq_handle_work, sc8551_irq_handler);
	strncpy(chip->model_name, id->name, I2C_NAME_SIZE);
	chip->regmap = devm_regmap_init_i2c(client, &sc8551_regmap_config);
	if (IS_ERR(chip->regmap)) {
		sc_err("failed to allocate regmap\n");
		return PTR_ERR(chip->regmap);
	}

	ret = sc8551_check_work_mode(chip, id->driver_data);
	if (ret) {
		sc_err("failed to check work_mode\n");
		return ret;
	}

	ret = sc8551_parse_dt(chip);
	if (ret) {
		sc_err("%s failed to parse DTS\n", chip->log_tag);
		return ret;
	}

	ret = sc8551_init_irq(chip);
	if (ret) {
		sc_err("%s failed to int irq\n", chip->log_tag);
		return ret;
	}

	ret = sc8551_register_charger(chip, fake_work_mode);
	if (ret) {
		sc_err("%s failed to register charger\n", chip->log_tag);
		return ret;
	}

	ret = sc8551_power_supply_init(chip, dev, fake_work_mode);
	if (ret) {
		sc_err("%s failed to init psy\n", chip->log_tag);
		return ret;
	}

	ret = sysfs_create_group(&chip->dev->kobj, &sc8551_attr_group);
	if (ret) {
		sc_err("%s failed to register sysfs\n", chip->log_tag);
		return ret;
	}

	ret = sc8551_init_device(chip);
	if (ret) {
		sc_err("%s failed to init device\n", chip->log_tag);
		return ret;
	}

	chip->chip_ok = true;
	fake_work_mode = SC8551_SLAVE;
	sc_info("%s SC8551 probe success\n", chip->log_tag);

	return 0;
}

static int sc8551_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8551 *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = sc8551_enable_adc(chip, false);
	if (ret)
		sc_err("%s failed to disable ADC\n", chip->log_tag);

	return enable_irq_wake(chip->irq);
}

static int sc8551_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sc8551 *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = sc8551_enable_adc(chip, true);
	if (ret)
		sc_err("%s failed to enable ADC\n", chip->log_tag);

	return disable_irq_wake(chip->irq);
}

static const struct dev_pm_ops sc8551_pm_ops = {
	.suspend	= sc8551_suspend,
	.resume		= sc8551_resume,
};

static int sc8551_remove(struct i2c_client *client)
{
	struct sc8551 *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = sc8551_enable_adc(chip, false);
	if (ret)
		sc_err("%s failed to disable ADC\n", chip->log_tag);

	return 0;
}

static void sc8551_shutdown(struct i2c_client *client)
{
	struct sc8551 *chip = i2c_get_clientdata(client);
	int ret = 0;

	ret = sc8551_enable_bypass(chip, false);
	if (ret)
		sc_err("%s failed to disable bypass\n", chip->log_tag);

	ret = sc8551_enable_charge(chip, false);
	if (ret)
		sc_err("%s failed to disable charge\n", chip->log_tag);

	ret = sc8551_enable_adc(chip, false);
	if (ret)
		sc_err("%s failed to disable ADC\n", chip->log_tag);

	sc_info("%s SC8551 shutdown!\n", chip->log_tag);
}

static const struct i2c_device_id sc8551_i2c_ids[] = {
	{ "sc8551_i2c7", SC8551_STANDALONE },
	{ "sc8551_i2c11", SC8551_STANDALONE },
	{ "sc8551_master", SC8551_MASTER },
	{ "sc8551_slave", SC8551_SLAVE },
	{},
};
MODULE_DEVICE_TABLE(i2c, sc8551_i2c_ids);

static const struct of_device_id sc8551_of_match[] = {
	{ .compatible = "sc8551_i2c7", .data = (void *)SC8551_STANDALONE},
	{ .compatible = "sc8551_i2c11", .data = (void *)SC8551_STANDALONE},
	{ .compatible = "sc8551_master", .data = (void *)SC8551_MASTER},
	{ .compatible = "sc8551_slave", .data = (void *)SC8551_SLAVE},
	{ },
};
MODULE_DEVICE_TABLE(of, sc8551_of_match);

static struct i2c_driver sc8551_driver = {
        .driver = {
                .name = "sc8551_charger",
                .of_match_table = sc8551_of_match,
		.pm = &sc8551_pm_ops,
        },
	.probe = sc8551_probe,
	.remove = sc8551_remove,
	.shutdown = sc8551_shutdown,
	.id_table = sc8551_i2c_ids,
};
module_i2c_driver(sc8551_driver);

MODULE_DESCRIPTION("SC8551 Charger Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Chenyichun");
