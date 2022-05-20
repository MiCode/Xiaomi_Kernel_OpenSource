// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#include <linux/bits.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/linear_range.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/property.h>
#include <linux/regmap.h>

#include "charger_class.h"
#include "mtk_charger.h"

static bool dbg_log_en;
module_param(dbg_log_en, bool, 0644);
#define mt_dbg(dev, fmt, ...) \
	do { \
		if (dbg_log_en) \
			dev_info(dev, "%s " fmt, __func__, ##__VA_ARGS__); \
	} while (0)
#define RT9758_REG_DEVINFO	0x00
#define RT9758_REG_FLAG1	0x01
#define RT9758_REG_FLAG2	0x02
#define RT9758_REG_FLAG3	0x03
#define RT9758_REG_ICSTAT	0x04
#define RT9758_REG_STAT1	0x08
#define RT9758_REG_CTRL1	0x0A
#define RT9758_REG_CTRL2	0x0B
#define RT9758_REG_CTRL3	0x0C
#define RT9758_REG_CTRL4	0x0D
#define RT9758_REG_CTRL5	0x0E
#define RT9758_REG_CTRL6	0x0F
#define RT9758_REG_CTRL7	0x10
#define RT9758_REG_CTRL8	0x11
#define RT9758_REG_CTRL9	0x12
#define RT9758_REG_CTRL10	0x13

#define RT9758_DEVID_MASK	GENMASK(3, 0)
#define RT9758_WRXIN_MASK	BIT(2)

#define RT9758_DEVICE_ID	0x03
#define RT9758_RSTRG_VAL	BIT(2)
#define RT9758_RESET_WAITUS	1000
#define RT9758_Q0CTRL_OFF	0
#define RT9758_Q0CTRL_ON	2

enum {
	RT9758_SYNC_OFF = 0,
	RT9758_SYNC_MASTER,
	RT9758_SYNC_SLAVE = 3,
	RT9758_MAX_SYNC
};

enum {
	RT9758_PRESENT_MODE = 0,
	RT9758_STANDBY_MODE,
	RT9758_FORWARD_DIV2_MODE,
	RT9758_FORWARD_BYPASS_MODE,
	RT9758_REVERSE_DIV2_MODE,
	RT9758_REVERSE_BYPASS_MODE,
	RT9758_CHARGE_FAULT,
};

enum rt9758_fields {
	F_IC_STAT = 0,
	F_VLERR_STAT,
	F_WRXIN_STAT,
	F_SWITCH_STAT,
	F_VOUT_OVP,
	F_VBUS_OVP,
	F_IBUS_OCP,
	F_SYNC_MODE,
	F_Q0_CTRL,
	F_WDT,
	F_WDT_EN,
	F_CHG_EN,
	F_OP_MODE,
	F_ATEN,
	F_BA_WDT,
	F_TDIE_EN,
	F_MAX_FIELDS
};

enum {
	RT9758_RANGE_VOUTOVP = 0,
	RT9758_RANGE_VBUSOVP,
	RT9758_RANGE_IBUSOCP,
	RT9758_MAX_RANGES
};

enum rt9758_chg_dtprop_type {
	DTPROP_U32,
	DTPROP_BOOL,
};

struct rt9758_priv {
	struct device *dev;
	struct regmap *regmap;
	struct regmap_field *rm_field[F_MAX_FIELDS];
	struct gpio_desc *enable_gpio;
	struct power_supply *psy;
	struct power_supply_desc desc;
	struct charger_device *chg_dev;
	const char *chg_name;
	struct charger_properties chg_prop;
	bool charge_enabled;
	bool bypass_enabled;
};

static struct reg_field rt9758_reg_fields[F_MAX_FIELDS] = {
	[F_IC_STAT]	= REG_FIELD(RT9758_REG_ICSTAT, 0, 2),
	[F_VLERR_STAT]	= REG_FIELD(RT9758_REG_STAT1, 3, 3),
	[F_WRXIN_STAT]	= REG_FIELD(RT9758_REG_STAT1, 2, 2),
	[F_SWITCH_STAT]	= REG_FIELD(RT9758_REG_STAT1, 1, 1),
	[F_VOUT_OVP]	= REG_FIELD(RT9758_REG_CTRL1, 0, 2),
	[F_VBUS_OVP]	= REG_FIELD(RT9758_REG_CTRL2, 0, 5),
	[F_IBUS_OCP]	= REG_FIELD(RT9758_REG_CTRL3, 4, 7),
	[F_SYNC_MODE]	= REG_FIELD(RT9758_REG_CTRL4, 2, 3),
	[F_Q0_CTRL]	= REG_FIELD(RT9758_REG_CTRL4, 0, 1),
	[F_WDT]		= REG_FIELD(RT9758_REG_CTRL5, 4, 6),
	[F_WDT_EN]	= REG_FIELD(RT9758_REG_CTRL5, 3, 3),
	[F_CHG_EN]	= REG_FIELD(RT9758_REG_CTRL6, 2, 2),
	[F_OP_MODE]	= REG_FIELD(RT9758_REG_CTRL6, 0, 0),
	[F_ATEN]	= REG_FIELD(RT9758_REG_CTRL7, 3, 3),
	[F_BA_WDT]	= REG_FIELD(RT9758_REG_CTRL8, 0, 0),
	[F_TDIE_EN]	= REG_FIELD(RT9758_REG_CTRL9, 5, 5)
};

static const struct linear_range rt9758_ranges[RT9758_MAX_RANGES] = {
	[RT9758_RANGE_VOUTOVP] = { 7000000, 0, 7, 1000000 },
	[RT9758_RANGE_VBUSOVP] = { 7250000, 0, 59, 250000 },
	[RT9758_RANGE_IBUSOCP] = { 2000000, 2, 10, 500000 }
};

static int rt9758_get_status(struct rt9758_priv *priv,
			     union power_supply_propval *val)
{
	unsigned int switching;
	int ret;

	ret = regmap_field_read(priv->rm_field[F_SWITCH_STAT], &switching);
	if (ret)
		return ret;

	if (switching)
		val->intval = POWER_SUPPLY_STATUS_CHARGING;
	else
		val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;

	return 0;
}

static int rt9758_get_charge_type(struct rt9758_priv *priv,
				  union power_supply_propval *val)
{
	if (!priv->charge_enabled)
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
	else if (!priv->bypass_enabled)
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
	else
		val->intval = POWER_SUPPLY_CHARGE_TYPE_STANDARD;

	return 0;
}

static int rt9758_get_online(struct rt9758_priv *priv,
			     union power_supply_propval *val)
{
	unsigned int wrxin_stat;
	int ret;

	ret = regmap_field_read(priv->rm_field[F_WRXIN_STAT], &wrxin_stat);
	if (ret)
		return ret;

	val->intval = wrxin_stat;
	return 0;
}

static int rt9758_get_const_charge_volt(struct rt9758_priv *priv,
					union power_supply_propval *val)
{
	const struct linear_range *range = rt9758_ranges + RT9758_RANGE_VOUTOVP;
	unsigned int vout_ovp_sel, vout_ovp_val;
	int ret;

	ret = regmap_field_read(priv->rm_field[F_VOUT_OVP], &vout_ovp_sel);
	if (ret)
		return ret;

	ret = linear_range_get_value(range, vout_ovp_sel, &vout_ovp_val);
	if (ret)
		return ret;

	val->intval = vout_ovp_val;
	return 0;
}

static int rt9758_get_input_curr_lim(struct rt9758_priv *priv,
				     union power_supply_propval *val)
{
	const struct linear_range *range = rt9758_ranges + RT9758_RANGE_IBUSOCP;
	unsigned int ibus_ocp_sel, ibus_ocp_val;
	int ret;

	ret = regmap_field_read(priv->rm_field[F_IBUS_OCP], &ibus_ocp_sel);
	if (ret)
		return ret;

	ret = linear_range_get_value(range, ibus_ocp_sel, &ibus_ocp_val);
	if (ret)
		return ret;

	val->intval = ibus_ocp_val;
	return 0;
}

static int rt9758_get_input_volt_lim(struct rt9758_priv *priv,
				     union power_supply_propval *val)
{
	const struct linear_range *range = rt9758_ranges + RT9758_RANGE_VBUSOVP;
	unsigned int vbus_ovp_sel, vbus_ovp_val;
	int ret;

	ret = regmap_field_read(priv->rm_field[F_VBUS_OVP], &vbus_ovp_sel);
	if (ret)
		return ret;

	ret = linear_range_get_value(range, vbus_ovp_sel, &vbus_ovp_val);
	if (ret)
		return ret;

	val->intval = vbus_ovp_val;
	return 0;
}

static int rt9758_get_manufacturer(struct rt9758_priv *priv,
				   union power_supply_propval *val)
{
	val->strval = "Richtek Technology Corp.";
	return 0;
}

static int rt9758_set_charge_enable(struct rt9758_priv *priv,
				    const union power_supply_propval *val)
{
	unsigned int q0ctrl_val, chgen;
	int ret;

	chgen = !!val->intval;
	if (chgen)
		q0ctrl_val = RT9758_Q0CTRL_ON;
	else
		q0ctrl_val = RT9758_Q0CTRL_OFF;

	ret = regmap_field_write(priv->rm_field[F_Q0_CTRL], q0ctrl_val);
	if (ret)
		return ret;

	ret = regmap_field_write(priv->rm_field[F_CHG_EN], chgen);
	if (ret)
		return ret;

	ret = regmap_field_write(priv->rm_field[F_WDT_EN], chgen);
	if (ret)
		return ret;

	if (priv->enable_gpio)
		gpiod_set_value(priv->enable_gpio, !chgen);
	mdelay(1);

	priv->charge_enabled = chgen;

	return 0;
}

static int rt9758_set_bypass_enable(struct rt9758_priv *priv,
				    const union power_supply_propval *val)
{
	unsigned int bypass_enabled;
	int ret;

	bypass_enabled = !!val->intval;
	ret = regmap_field_write(priv->rm_field[F_OP_MODE], bypass_enabled);
	if (ret)
		return ret;

	priv->bypass_enabled = bypass_enabled;
	return regmap_field_write(priv->rm_field[F_WDT], 0);
}

static int rt9758_set_const_charge_volt(struct rt9758_priv *priv,
				    const union power_supply_propval *val)
{
	const struct linear_range *range = rt9758_ranges + RT9758_RANGE_VOUTOVP;
	unsigned int sel = 0;

	linear_range_get_selector_within(range, val->intval, &sel);
	return regmap_field_write(priv->rm_field[F_VOUT_OVP], sel);
}

static int rt9758_set_input_curr_lim(struct rt9758_priv *priv,
				     const union power_supply_propval *val)
{
	const struct linear_range *range = rt9758_ranges + RT9758_RANGE_IBUSOCP;
	unsigned int sel = 0;

	linear_range_get_selector_within(range, val->intval, &sel);
	return regmap_field_write(priv->rm_field[F_IBUS_OCP], sel);
}

static int rt9758_set_input_volt_lim(struct rt9758_priv *priv,
				     const union power_supply_propval *val)
{
	const struct linear_range *range = rt9758_ranges + RT9758_RANGE_VBUSOVP;
	unsigned int sel = 0;

	linear_range_get_selector_within(range, val->intval, &sel);
	return regmap_field_write(priv->rm_field[F_VBUS_OVP], sel);
}

static int rt9758_charger_get_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct rt9758_priv *priv = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(priv))
		return PTR_ERR_OR_ZERO(priv);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return rt9758_get_status(priv, val);
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		return rt9758_get_charge_type(priv, val);
	case POWER_SUPPLY_PROP_ONLINE:
		return rt9758_get_online(priv, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return rt9758_get_const_charge_volt(priv, val);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return rt9758_get_input_curr_lim(priv, val);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		return rt9758_get_input_volt_lim(priv, val);
	case POWER_SUPPLY_PROP_MANUFACTURER:
		return rt9758_get_manufacturer(priv, val);
	default:
		return -ENODATA;
	}
}

static int rt9758_charger_set_property(struct power_supply *psy,
				       enum power_supply_property psp,
				       const union power_supply_propval *val)
{
	struct rt9758_priv *priv = power_supply_get_drvdata(psy);

	if (IS_ERR_OR_NULL(priv))
		return PTR_ERR_OR_ZERO(priv);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		return rt9758_set_charge_enable(priv, val);
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		return rt9758_set_bypass_enable(priv, val);
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		return rt9758_set_const_charge_volt(priv, val);
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		return rt9758_set_input_curr_lim(priv, val);
	case POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT:
		return rt9758_set_input_volt_lim(priv, val);
	default:
		return -EINVAL;
	}
}

static const enum power_supply_property rt9758_charger_properties[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
	POWER_SUPPLY_PROP_MANUFACTURER
};

static int rt9758_init_chg_properties(struct rt9758_priv *priv)
{
	unsigned int sync_mode = RT9758_SYNC_OFF;
	int ret;

	ret = regmap_write(priv->regmap, RT9758_REG_CTRL9, RT9758_RSTRG_VAL);
	if (ret)
		return ret;

	usleep_range(RT9758_RESET_WAITUS, RT9758_RESET_WAITUS + 100);

	/* mediatek chgdev name */
	ret = device_property_read_string(priv->dev, "chg_name",
					  &priv->chg_name);
	if (ret) {
		dev_notice(priv->dev, "failed to get chg_name\n");
		priv->chg_name = "hvdiv2_chg1";
	}

	device_property_read_u32(priv->dev, "richtek,dv2-sync-mode",
				 &sync_mode);

	if (sync_mode >= RT9758_MAX_SYNC) {
		dev_err(priv->dev, "Not valid mode [%d]\n", sync_mode);
		return -EINVAL;
	}

	ret = regmap_field_write(priv->rm_field[F_BA_WDT], 0);
	if (ret)
		return ret;

	ret = regmap_field_write(priv->rm_field[F_SYNC_MODE], sync_mode);
	if (ret)
		return ret;

	ret = regmap_field_write(priv->rm_field[F_Q0_CTRL], RT9758_Q0CTRL_OFF);
	if (ret)
		return ret;

	return regmap_field_write(priv->rm_field[F_CHG_EN], 0);
}

static int rt9758_check_device_info(struct rt9758_priv *priv)
{
	unsigned int dev_info;
	int ret;

	ret = regmap_read(priv->regmap, RT9758_REG_DEVINFO, &dev_info);
	if (ret)
		return ret;

	if ((dev_info & RT9758_DEVID_MASK) != RT9758_DEVICE_ID) {
		dev_err(priv->dev, "Failed to match devid 0x%02x\n", dev_info);
		return -ENODEV;
	}

	dev_info(priv->dev, "devid = 0x%02X\n", dev_info);
	return 0;
}

static int rt9758_psy_set_prop(struct rt9758_priv *priv,
			       enum power_supply_property psp, uint32_t data)
{
	union power_supply_propval val;

	val.intval = data;
	return power_supply_set_property(priv->psy, psp, &val);
}

static int rt9758_enable_chg(struct charger_device *chg_dev, bool en)
{
	uint32_t set_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	dev_info(priv->dev, "%s %d\n", __func__, en);
	set_val = en;
	return rt9758_psy_set_prop(priv, POWER_SUPPLY_PROP_STATUS, set_val);
}

static int rt9758_is_chg_enabled(struct charger_device *chg_dev, bool *en)
{
	int ret;
	unsigned int get_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	ret = regmap_field_read(priv->rm_field[F_CHG_EN], &get_val);
	if (ret)
		return ret;

	*en = get_val;
	dev_info(priv->dev, "%s %d\n", __func__, *en);
	return 0;
}

static int rt9758_init_chip(struct charger_device *chg_dev)
{
	return 0;
}

static int rt9758_set_vbusovp(struct charger_device *chg_dev, uint32_t uV)
{
	uint32_t set_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	dev_info(priv->dev, "%s %d\n", __func__, uV);
	set_val = uV;
	return rt9758_psy_set_prop(priv, POWER_SUPPLY_PROP_INPUT_VOLTAGE_LIMIT,
				   set_val);
}

static int rt9758_set_ibusocp(struct charger_device *chg_dev, uint32_t uA)
{
	uint32_t set_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	dev_info(priv->dev, "%s %d\n", __func__, uA);
	set_val = uA;
	return rt9758_psy_set_prop(priv, POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
				   set_val);
}

static int rt9758_set_voutovp(struct charger_device *chg_dev, uint32_t uV)
{
	uint32_t set_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	dev_info(priv->dev, "%s %d\n", __func__, uV);
	set_val = uV;
	return rt9758_psy_set_prop(priv,
				   POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
				   set_val);
}

static int rt9758_is_vbuslowerr(struct charger_device *chg_dev, bool *err)
{
	int ret;
	unsigned int get_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	ret = regmap_field_read(priv->rm_field[F_VLERR_STAT], &get_val);
	if (ret)
		return ret;

	*err = get_val;
	dev_info(priv->dev, "%s %d\n", __func__, *err);
	return 0;
}

static int rt9758_enable_auto_trans(struct charger_device *chg_dev, bool en)
{
	uint32_t set_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	dev_info(priv->dev, "%s %d\n", __func__, en);
	set_val = en;
	return regmap_field_write(priv->rm_field[F_ATEN], set_val);
}

static int rt9758_set_auto_trans(struct charger_device *chg_dev, uint32_t uV,
				 bool en)
{
	return 0;
}

static int rt9758_operation_mode_select(struct charger_device *chg_dev,
					bool div2)
{
	unsigned int set_val;
	struct rt9758_priv *priv = charger_get_data(chg_dev);

	dev_info(priv->dev, "div2 = %d\n", div2);
	set_val = div2;
	return rt9758_psy_set_prop(priv, POWER_SUPPLY_PROP_CHARGE_TYPE,
				   set_val);
}

#define DUMP_REG_BUF_SIZE	1024
static int rt9758_dump_registers(struct charger_device *chg_dev)
{
	struct rt9758_priv *priv = charger_get_data(chg_dev);
	int ret, i;
	u32 val;
	char buf[DUMP_REG_BUF_SIZE] = "\0";
	static const struct {
		const u8 reg;
		const char *name;
	} regs[] = {
		{ .reg = RT9758_REG_FLAG1, .name = "FLAG1"},
		{ .reg = RT9758_REG_FLAG2, .name = "FLAG2"},
		{ .reg = RT9758_REG_FLAG3, .name = "FLAG3"},
		{ .reg = RT9758_REG_ICSTAT, .name = "IC_STAT"},
		{ .reg = RT9758_REG_CTRL1, .name = "CTRL1"},
		{ .reg = RT9758_REG_CTRL2, .name = "CTRL2"},
		{ .reg = RT9758_REG_CTRL3, .name = "CTRL3"},
		{ .reg = RT9758_REG_CTRL4, .name = "CTRL4"},
		{ .reg = RT9758_REG_CTRL5, .name = "CTRL5"},
		{ .reg = RT9758_REG_CTRL6, .name = "CTRL6"},
	};

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = regmap_read(priv->regmap, regs[i].reg, &val);
		if (ret) {
			dev_err(priv->dev, "failed to get %s\n", regs[i].name);
			return ret;
		}
		if (i == ARRAY_SIZE(regs) - 1)
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = 0x%02X\n", regs[i].name, val);
		else
			scnprintf(buf + strlen(buf), DUMP_REG_BUF_SIZE,
				  "%s = 0x%02X, ", regs[i].name, val);
	}

	dev_info(priv->dev, "%s %s\n", __func__, buf);
	dev_info(priv->dev, "%s enable_gpio = %d\n", __func__,
		 gpiod_get_value(priv->enable_gpio));
	return 0;
}

static const struct charger_ops rt9758_chg_ops = {
	.enable = rt9758_enable_chg,
	.is_enabled = rt9758_is_chg_enabled,
	.init_chip = rt9758_init_chip,
	.set_vbusovp = rt9758_set_vbusovp,
	.set_ibusocp = rt9758_set_ibusocp,
	.set_vbatovp = rt9758_set_voutovp,
	.is_vbuslowerr = rt9758_is_vbuslowerr,
	.enable_auto_trans = rt9758_enable_auto_trans,
	.set_auto_trans = rt9758_set_auto_trans,
	.set_operation_mode = rt9758_operation_mode_select,
	.dump_registers = rt9758_dump_registers,
};

static const struct regmap_config rt9758_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = RT9758_REG_CTRL10,
};

static int rt9758_charger_probe(struct i2c_client *i2c)
{
	struct rt9758_priv *priv;
	struct power_supply_config cfg = {};
	int ret;

	priv = devm_kzalloc(&i2c->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = &i2c->dev;
	i2c_set_clientdata(i2c, priv);

	priv->regmap = devm_regmap_init_i2c(i2c, &rt9758_regmap_config);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&i2c->dev, "Failed to init regmap [%d]\n", ret);
		return ret;
	}

	ret = devm_regmap_field_bulk_alloc(&i2c->dev, priv->regmap,
					   priv->rm_field, rt9758_reg_fields,
					   ARRAY_SIZE(rt9758_reg_fields));
	if (ret) {
		dev_err(&i2c->dev, "Failed to alloc regmap fields\n");
		return ret;
	}

	ret = rt9758_check_device_info(priv);
	if (ret) {
		dev_err(&i2c->dev, "Failed to check device info\n");
		return ret;
	}

	/* If specified, initial control 'enable' high to enter lowest IQ */
	priv->enable_gpio = devm_gpiod_get_optional(&i2c->dev, "enable",
						    GPIOD_OUT_HIGH);
	if (IS_ERR(priv->enable_gpio)) {
		dev_err(&i2c->dev, "Failed to init enable gpio\n");
		return PTR_ERR(priv->enable_gpio);
	}

	ret = rt9758_init_chg_properties(priv);
	if (ret) {
		dev_err(&i2c->dev, "Failed to init charger properties\n");
		return ret;
	}

	priv->desc.name = dev_name(&i2c->dev);
	priv->desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	priv->desc.properties = rt9758_charger_properties;
	priv->desc.num_properties = ARRAY_SIZE(rt9758_charger_properties);
	priv->desc.get_property = rt9758_charger_get_property;
	priv->desc.set_property = rt9758_charger_set_property;

	cfg.of_node = i2c->dev.of_node;
	cfg.drv_data = priv;

	priv->psy = devm_power_supply_register(&i2c->dev, &priv->desc, &cfg);
	if (IS_ERR(priv->psy)) {
		dev_err(&i2c->dev, "Failed to register psy\n");
		return PTR_ERR(priv->psy);
	}

	priv->chg_prop.alias_name = priv->chg_name;
	priv->chg_dev = charger_device_register(priv->chg_name, priv->dev,
						priv, &rt9758_chg_ops,
						&priv->chg_prop);
	if (IS_ERR(priv->chg_dev)) {
		dev_err(&i2c->dev, "Failed to register chgdev\n");
		return PTR_ERR(priv->chg_dev);
	}

	dev_info(&i2c->dev, "%s ok\n", __func__);
	return 0;
}

static int rt9758_charger_remove(struct i2c_client *i2c)
{
	struct rt9758_priv *priv = i2c_get_clientdata(i2c);

	charger_device_unregister(priv->chg_dev);
	return 0;
}

static const struct of_device_id rt9758_charger_of_match_table[] = {
	{ .compatible = "richtek,rt9758", },
	{ }
};
MODULE_DEVICE_TABLE(of, rt9758_charger_of_match_table);

static struct i2c_driver rt9758_charger_driver = {
	.driver = {
		.name = "rt9758-charger",
		.of_match_table = rt9758_charger_of_match_table,
	},
	.probe_new = rt9758_charger_probe,
	.remove = rt9758_charger_remove,
};
module_i2c_driver(rt9758_charger_driver);

MODULE_DESCRIPTION("Richtek RT9758 charger driver");
MODULE_AUTHOR("ChiYuan Huang <cy_huang@richtek.com>");
MODULE_LICENSE("GPL");
