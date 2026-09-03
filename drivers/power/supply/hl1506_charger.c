// SPDX-License-Identifier: GPL-2.0：
// Copyright (C) 2021 Spreadtrum Communications Inc.

/*
 * Driver for the Halo charge pump converter hl506 charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <uapi/linux/usb/charger.h>

#define HL1506_REG_0					0x0
#define HL1506_REG_1					0x1
#define HL1506_REG_2					0x2
#define HL1506_REG_3					0x3
#define HL1506_REG_4					0x4
#define HL1506_REG_54					0x54

/* Register bits */
/* HL1506_REG_0 (0x00) PMID Voltage Limit DAC reg */
#define HL1506_REG_PMID_DAC_MASK		GENMASK(7, 0)
#define HL1506_REG_PMID_DAC_SHIFT		(0)
/* HL1506_REG_1 (0x01) Input Current Limit reg */
#define HL1506_REG_IIN_LIMIT_MASK		GENMASK(7, 3)
#define HL1506_REG_IIN_LIMIT_SHIFT		(3)
#define HL1506_REG_FORCE_BYPASS_MASK		GENMASK(1, 1)
#define HL1506_REG_FORCE_BYPASS_SHIFT		(1)
#define HL1506_REG_FORCE_CP_MASK		GENMASK(0, 0)
#define HL1506_REG_FORCE_CP_SHIFT		(0)
/* HL1506_REG_2 (0x02) CP Frequency reg */
#define HL1506_REG_CP_CK_MASK		GENMASK(7, 5)
#define HL1506_REG_CP_CK_SHIFT		(5)
#define HL1506_REG_DISABLE_LDO5_MASK		GENMASK(3, 3)
#define HL1506_REG_DISABLE_LDO5_SHIFT		(3)
#define HL1506_REG_LDO5_DAC_MASK		GENMASK(2, 0)
#define HL1506_REG_LDO5_DAC_SHIFT		(0)
/* HL1506_REG_3 (0x03) PMID OV reg */
#define HL1506_REG_HOST_ENABLE_MASK		GENMASK(7, 7)
#define HL1506_REG_HOST_ENABLE_SHIFT		(7)
#define HL1506_REG_DISABLE_QRB_MASK		GENMASK(3, 3)
#define HL1506_REG_DISABLE_QRB_SHIFT		(3)
#define HL1506_REG_PMID_OV_DEBOUNCE_MASK		GENMASK(2, 2)
#define HL1506_REG_PMID_OV_DEBOUNCE_SHIFT		(2)
#define HL1506_REG_PMID_OV_MASK		GENMASK(1, 0)
#define HL1506_REG_PMID_OV_SHIFT		(0)
/* HL1506_REG_4 (0x04) STATUS reg */
#define HL1506_REG_BP_CP_MODE_MASK		GENMASK(7, 7)
#define HL1506_REG_BP_CP_MODE_SHIFT		(7)
#define HL1506_REG_OTP_MASK		GENMASK(6, 6)
#define HL1506_REG_OTP_SHIFT		(6)
#define HL1506_REG_IOUTLIM_FLAG_MASK		GENMASK(5, 5)
#define HL1506_REG_IOUTLIM_FLAG_SHIFT		(5)
#define HL1506_REG_PMID_OV_FLAG_MASK		GENMASK(4, 4)
#define HL1506_REG_PMID_OV_FLAG_SHIFT		(4)
#define HL1506_REG_POWER_GOOD_MASK		GENMASK(1, 1)
#define HL1506_REG_POWER_GOOD_SHIFT		(1)
#define HL1506_REG_SHORT_FLAG_MASK		GENMASK(0, 0)
#define HL1506_REG_SHORT_FLAG_SHIFT		(0)
/* HL1506_REG_54 (0x54) Hidden reg */
#define HL1506_REG_OPT_SHORT_PROT_MASK		GENMASK(5, 5)
#define HL1506_REG_OPT_SHORT_PROT_SHIFT		(5)

#define HL1506_INN_LIMIT_MIN    (200U)
#define HL1506_INN_LIMIT_MAX    (1750U)
#define HL1506_INN_LIMIT_STEP    (50U)

#define HL1506_DEVICE_ENABLE    1
#define HL1506_DEVICE_DISABLE    0
#define HL1506_ENABLE_BYPASS_MODE 1
#define HL1506_DISABLE_BYPASS_MODE 0
#define HL1506_ENABLE_CP_MODE 1
#define HL1506_DISABLE_CP_MODE 0
#define HL1506_ENABLE_LDO5 0
#define HL1506_DISABLE_LDO5 1
#define HL1506_HOST_ENABLE    1
#define HL1506_HOST_DISABLE    0

#define HL1506_ENABLE_QRB 0
#define HL1506_DISABLE_QRB 1

#define HL1506_CP_CLK_1000K 0x00
#define HL1506_CP_CLK_750K  0x01
#define HL1506_CP_CLK_600K  0x02
#define HL1506_CP_CLK_500K  0x03
#define HL1506_CP_CLK_429K  0x04
#define HL1506_CP_CLK_375K  0x05
#define HL1506_CP_CLK_333K  0x06
#define HL1506_CP_CLK_300K  0x07

#define HL1506_LDO5_MAX     (5200U)
#define HL1506_LDO5_STEP    (100U)
#define HL1506_DEBOUNCE_10US 0
#define HL1506_DEBOUNCE_0US  1

#define HL1506_PMID_OV_10V   0x00
#define HL1506_PMID_OV_11V   0x01
#define HL1506_PMID_OV_12V   0x02
#define HL1506_PMID_OV_13V   0x03

#define HL1506_CURRENT_WORK_MS		msecs_to_jiffies(100)

struct hl1506_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy;
	struct mutex lock;
	bool charging;
	u32 limit;
	u32 current_limit;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	unsigned int chg_pump_en;
	struct delayed_work work;
};

static int hl1506_charger_hw_init(struct hl1506_charger_info *info);

static int hl1506_read(struct hl1506_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int hl1506_write(struct hl1506_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int hl1506_update_bits(struct hl1506_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = hl1506_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return hl1506_write(info, reg, v);
}

static int hl1506_set_pmid_dac(struct hl1506_charger_info *info, u32 val)
{
	return hl1506_update_bits(info, HL1506_REG_0, HL1506_REG_PMID_DAC_MASK,
				  (val << HL1506_REG_PMID_DAC_SHIFT));
}

static int hl1506_set_iin_limit(struct hl1506_charger_info *info, u32 cur)
{
	u8 reg_val;

	if (cur < HL1506_INN_LIMIT_MIN)
		cur = HL1506_INN_LIMIT_MIN;
	else if ((cur > HL1506_INN_LIMIT_MAX))
		cur = HL1506_INN_LIMIT_MAX;

	reg_val = (cur - HL1506_INN_LIMIT_MIN) / HL1506_INN_LIMIT_STEP;

	return hl1506_update_bits(info, HL1506_REG_1, HL1506_REG_IIN_LIMIT_MASK,
				  (reg_val << HL1506_REG_IIN_LIMIT_SHIFT));
}

static int hl1506_get_iin_limit(struct hl1506_charger_info *info, u32 *cur)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_1, &val);
	if (ret < 0) {
		dev_err(info->dev, "set HL1506 iin limit[7:3] cur failed ret = %d\n", ret);
		return ret;
	}

	val &= HL1506_REG_IIN_LIMIT_MASK;
	val = val >> HL1506_REG_IIN_LIMIT_SHIFT;

	*cur = val * HL1506_INN_LIMIT_STEP + HL1506_INN_LIMIT_MIN;

	return 0;
}

static int hl1506_force_bypass_mode(struct hl1506_charger_info *info, bool enable)
{
	u8 reg_val;

	if (enable)
		reg_val = HL1506_ENABLE_BYPASS_MODE;
	else
		reg_val = HL1506_DISABLE_BYPASS_MODE;

	return hl1506_update_bits(info, HL1506_REG_1, HL1506_REG_FORCE_BYPASS_MASK,
				  (reg_val << HL1506_REG_FORCE_BYPASS_SHIFT));
}

static int hl1506_force_cp_mode(struct hl1506_charger_info *info, bool enable)
{
	u8 reg_val;

	if (enable)
		reg_val = HL1506_ENABLE_CP_MODE;
	else
		reg_val = HL1506_DISABLE_CP_MODE;

	return hl1506_update_bits(info, HL1506_REG_1, HL1506_REG_FORCE_CP_MASK,
				  (reg_val << HL1506_REG_FORCE_CP_SHIFT));
}

static void hl1506_device_enable(struct hl1506_charger_info *info, bool enable)
{
	u8 en_pin;

	if (enable)
		en_pin = HL1506_DEVICE_ENABLE;
	else
		en_pin = HL1506_DEVICE_DISABLE;

	gpio_set_value(info->chg_pump_en, en_pin);
}

static int hl1506_set_cp_clk(struct hl1506_charger_info *info, u32 clk)
{
	u8 reg_val;

	if (clk == 1000)
		reg_val = HL1506_CP_CLK_1000K;
	else if (clk == 750)
		reg_val = HL1506_CP_CLK_750K;
	else if (clk == 600)
		reg_val = HL1506_CP_CLK_600K;
	else if (clk == 500)
		reg_val = HL1506_CP_CLK_500K;
	else if (clk == 429)
		reg_val = HL1506_CP_CLK_429K;
	else if (clk == 375)
		reg_val = HL1506_CP_CLK_375K;
	else if (clk == 333)
		reg_val = HL1506_CP_CLK_333K;
	else if (clk == 300)
		reg_val = HL1506_CP_CLK_300K;
	else
		reg_val = HL1506_CP_CLK_500K;

	return hl1506_update_bits(info, HL1506_REG_2, HL1506_REG_CP_CK_MASK,
				  (reg_val << HL1506_REG_CP_CK_SHIFT));
}

static int hl1506_disable_ldo5(struct hl1506_charger_info *info, bool disable)
{
	u8 reg_val;

	if (disable)
		reg_val = HL1506_DISABLE_LDO5;
	else
		reg_val = HL1506_ENABLE_LDO5;

	return hl1506_update_bits(info, HL1506_REG_2, HL1506_REG_DISABLE_LDO5_MASK,
				  (reg_val << HL1506_REG_DISABLE_LDO5_SHIFT));
}

static int hl1506_set_ldo5_voltage(struct hl1506_charger_info *info, u32 volt)
{
	u8 reg_val;

	if (volt > HL1506_LDO5_MAX)
		volt = HL1506_LDO5_MAX;

	reg_val = (HL1506_LDO5_MAX - volt) / HL1506_LDO5_STEP;

	return hl1506_update_bits(info, HL1506_REG_2, HL1506_REG_LDO5_DAC_MASK,
				  (reg_val << HL1506_REG_LDO5_DAC_SHIFT));
}

static int hl1506_host_enable(struct hl1506_charger_info *info, bool enable)
{
	u8 reg_val;

	if (enable)
		reg_val = HL1506_HOST_ENABLE;
	else
		reg_val = HL1506_HOST_DISABLE;

	return hl1506_update_bits(info, HL1506_REG_3, HL1506_REG_HOST_ENABLE_MASK,
				  (reg_val << HL1506_REG_HOST_ENABLE_SHIFT));

	return 0;
}

static int hl1506_disable_qrb(struct hl1506_charger_info *info, bool disable)
{
	u8 reg_val;

	if (disable)
		reg_val = HL1506_DISABLE_QRB;
	else
		reg_val = HL1506_ENABLE_QRB;

	return hl1506_update_bits(info, HL1506_REG_3, HL1506_REG_DISABLE_QRB_MASK,
				  (reg_val << HL1506_REG_DISABLE_QRB_SHIFT));
}

static int hl1506_set_pmid_ov_debounce(struct hl1506_charger_info *info, u32 debounce)
{
	u8 reg_val;

	if (debounce >= 10)
		reg_val = HL1506_DEBOUNCE_10US;
	else
		reg_val = HL1506_DEBOUNCE_0US;

	return hl1506_update_bits(info, HL1506_REG_3, HL1506_REG_PMID_OV_DEBOUNCE_MASK,
				  (reg_val << HL1506_REG_PMID_OV_DEBOUNCE_SHIFT));
}

static int hl1506_set_pmid_ov(struct hl1506_charger_info *info, u32 volt)
{
	u8 reg_val;

	if (volt <= 10)
		reg_val = HL1506_PMID_OV_10V;
	else if (volt <= 11)
		reg_val = HL1506_PMID_OV_11V;
	else if (volt <= 12)
		reg_val = HL1506_PMID_OV_12V;
	else
		reg_val = HL1506_PMID_OV_13V;

	return hl1506_update_bits(info, HL1506_REG_3, HL1506_REG_PMID_OV_MASK,
				  (reg_val << HL1506_REG_PMID_OV_SHIFT));
}

static int hl1506_get_operation_mode(struct hl1506_charger_info *info, u8 *mode)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "get  operation mode failed ret = %d\n", ret);
		return ret;
	}

	*mode = (val & HL1506_REG_BP_CP_MODE_MASK) >> HL1506_REG_BP_CP_MODE_SHIFT;

	return 0;
}

static int hl1506_is_otp(struct hl1506_charger_info *info, u8 *otp)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "get  otp failed ret = %d\n", ret);
		return ret;
	}

	*otp = (val & HL1506_REG_OTP_MASK) >> HL1506_REG_OTP_SHIFT;

	return 0;
}

static int hl1506_get_ioutlim_flag(struct hl1506_charger_info *info, u8 *flag)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "get  iolimit failed ret = %d\n", ret);
		return ret;
	}

	*flag = (val & HL1506_REG_IOUTLIM_FLAG_MASK) >> HL1506_REG_IOUTLIM_FLAG_SHIFT;

	return 0;
}

static int hl1506_get_pmid_ov_flag(struct hl1506_charger_info *info, u8 *flag)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "get  iolimit failed ret = %d\n", ret);
		return ret;
	}

	*flag = (val & HL1506_REG_PMID_OV_FLAG_MASK) >> HL1506_REG_PMID_OV_FLAG_SHIFT;

	return 0;
}

static int hl1506_get_power_good_flag(struct hl1506_charger_info *info, u8 *flag)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "get  power_good failed ret = %d\n", ret);
		return ret;
	}

	*flag = (val & HL1506_REG_POWER_GOOD_MASK) >> HL1506_REG_POWER_GOOD_SHIFT;

	return 0;
}

static int hl1506_get_short_flag_flag(struct hl1506_charger_info *info, u8 *flag)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "get  short flag failed ret = %d\n", ret);
		return ret;
	}

	*flag = (val & HL1506_REG_SHORT_FLAG_MASK) >> HL1506_REG_SHORT_FLAG_SHIFT;

	return 0;
}

static void hl1506_dump_status(struct hl1506_charger_info *info)
{
	u8 val;
	int ret;

	ret = hl1506_read(info, HL1506_REG_0, &val);
	if (ret < 0) {
		dev_err(info->dev, "get xx failed ret = %d\n", ret);
		return;
	}
	dev_info(info->dev, "%s, dump HL1506_REG_0 = 0x%x", __func__, val);

	ret = hl1506_read(info, HL1506_REG_1, &val);
	if (ret < 0) {
		dev_err(info->dev, "get xx failed ret = %d\n", ret);
		return;
	}
	dev_info(info->dev, "%s, dump HL1506_REG_1 = 0x%x", __func__, val);

	ret = hl1506_read(info, HL1506_REG_2, &val);
	if (ret < 0) {
		dev_err(info->dev, "get xx failed ret = %d\n", ret);
		return;
	}
	dev_info(info->dev, "%s, dump HL1506_REG_2 = 0x%x", __func__, val);

	ret = hl1506_read(info, HL1506_REG_3, &val);
	if (ret < 0) {
		dev_err(info->dev, "get xx failed ret = %d\n", ret);
		return;
	}
	dev_info(info->dev, "%s, dump HL1506_REG_3 = 0x%x", __func__, val);

	ret = hl1506_read(info, HL1506_REG_4, &val);
	if (ret < 0) {
		dev_err(info->dev, "get xx failed ret = %d\n", ret);
		return;
	}
	dev_info(info->dev, "%s, dump HL1506_REG_4 = 0x%x", __func__, val);

	ret = hl1506_get_operation_mode(info, &val);
	if (ret < 0)
		dev_err(info->dev, "get operation mode failed ret = %d\n", ret);
	dev_info(info->dev, "%s, operatiom mode is: %s mode", __func__, val ? "bypass" : "CP");

	ret = hl1506_is_otp(info, &val);
	if (ret < 0)
		dev_err(info->dev, "get otp failed ret = %d\n", ret);

	dev_info(info->dev, "%s, HL1506 is otp: %s ", __func__, val ? "YES" : "NO");

	ret = hl1506_get_ioutlim_flag(info, &val);
	if (ret < 0)
		dev_err(info->dev, "get ioutlim flag failed ret = %d\n", ret);

	dev_info(info->dev, "%s, HL1506 iout is limited: %s ", __func__, val ? "YES" : "NO");

	ret = hl1506_get_pmid_ov_flag(info, &val);
	if (ret < 0)
		dev_err(info->dev, "get pmid ov flag failed ret = %d\n", ret);

	dev_info(info->dev, "%s, HL1506 is pimd ov: %s ", __func__, val ? "YES" : "NO");

	ret = hl1506_get_power_good_flag(info, &val);
	if (ret < 0)
		dev_err(info->dev, "get power good flag failed ret = %d\n", ret);

	dev_info(info->dev, "%s, HL1506 is power good: %s ", __func__, val ? "YES" : "NO");

	ret = hl1506_get_short_flag_flag(info, &val);
	if (ret < 0)
		dev_err(info->dev, "get power short flag failed ret = %d\n", ret);

	dev_info(info->dev, "%s, HL1506 is short : %s ", __func__, val ? "YES" : "NO");
}

static int hl1506_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct hl1506_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur;
	int ret = 0;

	if (!info)
		return -EINVAL;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = hl1506_get_iin_limit(info, &cur);
		if (ret < 0) {
			dev_err(info->dev, "set input current limit failed\n");
			goto out;
		}

		val->intval = cur * 1000;
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int hl1506_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct hl1506_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info)
		return -EINVAL;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		info->limit = val->intval / 1000;
		schedule_delayed_work(&info->work, HL1506_CURRENT_WORK_MS * 20);
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (val->intval) {
			ret = hl1506_charger_hw_init(info);
			hl1506_device_enable(info, true);
			info->limit = HL1506_INN_LIMIT_MAX;
			info->charging = true;
			schedule_delayed_work(&info->work, HL1506_CURRENT_WORK_MS * 20);
		} else {
			cancel_delayed_work_sync(&info->work);
			hl1506_device_enable(info, false);
			info->charging = false;
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int hl1506_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property hl1506_charge_pump_converter_properties[] = {
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
};

static const struct power_supply_desc hl1506_charge_pump_converter_desc = {
	.name			= "hl1506_cp_converter",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= hl1506_charge_pump_converter_properties,
	.num_properties		= ARRAY_SIZE(hl1506_charge_pump_converter_properties),
	.get_property		= hl1506_charger_get_property,
	.set_property		= hl1506_charger_set_property,
	.property_is_writeable	= hl1506_charger_property_is_writeable,
};

static int hl1506_parse_dt(struct hl1506_charger_info *info)
{
	struct device_node *node = info->dev->of_node;
	int ret = 0;

	info->chg_pump_en  = of_get_named_gpio(node, "chg_pump_en_gpio", 0);
	if (!gpio_is_valid(info->chg_pump_en)) {
		dev_err(info->dev, "[rx1619] [%s] chg_pump_en_gpio %d\n",
			__func__, info->chg_pump_en);
		return -EINVAL;
	}

	ret = devm_gpio_request_one(info->dev, info->chg_pump_en,
				    GPIOF_DIR_OUT | GPIOF_INIT_LOW, "chg_pump_en_pin");
	if (ret)
		dev_err(info->dev, "int switch chg en pin failed, ret = %d\n", ret);

	return ret;
}

static int hl1506_charger_hw_init(struct hl1506_charger_info *info)
{
	int ret = 0;

	usleep_range(500, 600);

	ret = hl1506_set_cp_clk(info, 500);
	if (ret) {
		dev_err(info->dev, "set cp clk fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_disable_ldo5(info, false);
	if (ret) {
		dev_err(info->dev, "disable ldo5 fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_set_ldo5_voltage(info, 400);
	if (ret) {
		dev_err(info->dev, "set ldo5 voltage fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_host_enable(info, true);
	if (ret) {
		dev_err(info->dev, "host enable fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_disable_qrb(info, false);
	if (ret) {
		dev_err(info->dev, "disable qrb fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_set_pmid_ov_debounce(info, 10);
	if (ret) {
		dev_err(info->dev, "set pmid ov debounce fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_set_pmid_ov(info, 13);
	if (ret) {
		dev_err(info->dev, "set pmid ov fail, ret = %d\n", ret);
		return ret;
	}

	/* set pmid dac to 13v bypass mode */
	ret = hl1506_set_pmid_dac(info, 0x50);
	if (ret) {
		dev_err(info->dev, "set pmid dac fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_set_iin_limit(info, HL1506_INN_LIMIT_MIN);
	if (ret) {
		dev_err(info->dev, "set iin limit fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_force_cp_mode(info, false);
	if (ret < 0) {
		dev_err(info->dev, "force cp mode fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_force_bypass_mode(info, true);
	if (ret < 0) {
		dev_err(info->dev, "force bypass mode fail, ret = %d\n", ret);
		return ret;
	}

	info->current_limit = HL1506_INN_LIMIT_MIN;

	hl1506_dump_status(info);

	return ret;
}

static void hl1506_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct hl1506_charger_info *info =
		container_of(dwork, struct hl1506_charger_info, work);
	int ret = 0;

	if (info->current_limit > info->limit) {
		ret = hl1506_set_iin_limit(info, info->current_limit);
		if (ret < 0) {
			dev_err(info->dev, "set input current limit failed\n");
			return;
		}
	}

	if (!info->charging)
		return;

	if (info->current_limit + HL1506_INN_LIMIT_STEP <= info->limit)
		info->current_limit += HL1506_INN_LIMIT_STEP;
	else
		return;

	ret = hl1506_set_iin_limit(info, info->current_limit);
	if (ret < 0) {
		dev_err(info->dev, "set input current limit failed\n");
		return;
	}

	schedule_delayed_work(&info->work, HL1506_CURRENT_WORK_MS);
}

static int hl1506_charger_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct hl1506_charger_info *info;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;

	mutex_init(&info->lock);
	i2c_set_clientdata(client, info);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy = devm_power_supply_register(dev,
					       &hl1506_charge_pump_converter_desc,
					       &charger_cfg);
	if (IS_ERR(info->psy)) {
		dev_err(dev, "failed to register power supply\n");
		return PTR_ERR(info->psy);
	}

	INIT_DELAYED_WORK(&info->work, hl1506_current_work);
	ret = hl1506_parse_dt(info);
	if (ret < 0) {
		dev_err(info->dev, "parse dt fail, ret = %d\n", ret);
		return ret;
	}

	ret = hl1506_charger_hw_init(info);
	if (ret)
		return ret;

	return 0;
}

static int hl1506_charger_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id hl1506_i2c_id[] = {
	{"hl1506_cp_converter", 0},
	{}
};

static const struct of_device_id hl1506_charger_of_match[] = {
	{ .compatible = "halo,hl1506_cp_converter", },
	{ }
};

MODULE_DEVICE_TABLE(of, hl1506_charger_of_match);

static struct i2c_driver hl1506_charger_driver = {
	.driver = {
		.name = "hl1506_cp_converter",
		.of_match_table = hl1506_charger_of_match,
	},
	.probe = hl1506_charger_probe,
	.remove = hl1506_charger_remove,
	.id_table = hl1506_i2c_id,
};

module_i2c_driver(hl1506_charger_driver);
MODULE_DESCRIPTION("Halo Charge Pump Converter Driver");
MODULE_LICENSE("GPL v2");
