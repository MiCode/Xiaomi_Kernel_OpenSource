// SPDX-License-Identifier: GPL-2.0：
// Copyright (C) 2019 Spreadtrum Communications Inc.

#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/slab.h>

#define SC2720_BATTERY_NAME			"sc27xx-fgu"

#define SC2720_CHG_CFG0				0x0
#define SC2720_CHG_CFG1				0x4

#define SC2720_TERM_VOLTAGE_MIN			4200
#define SC2720_TERM_VOLTAGE_MAX			4500
#define SC2720_TERM_VOLTAGE_STEP		100
#define SC2720_CCCV_VOLTAGE_STEP		75
#define SC2720_CHG_DPM_4300MV			4300
#define SC2720_CHG_CCCV_MAXIMUM			0x3f
#define SC2720_CHG_CURRENT_MIN			300
#define SC2720_CHG_CURRENT_800MA		800
#define SC2720_CHG_CURRENT_800MA_REG		0x0a
#define SC2720_CHG_CURRENT_STEP_50MA		50
#define SC2720_CHG_CURRENT_STEP_100MA		100

#define SC2720_CHG_PD				BIT(0)
#define SC2720_CHG_CC_MODE			BIT(12)

#define SC2720_CHG_DPM_MASK			GENMASK(14, 13)
#define SC2720_CHG_DPM_MASK_SHIT		13

#define SC2720_CHG_OVP_MASK			GENMASK(1, 0)

#define SC2720_CHG_TERMINATION_CURRENT_MASK	GENMASK(1, 0)
#define SC2720_CHG_TERMINATION_CURRENT_MASK_SHIT	1

#define SC2720_CHG_END_V_MASK			GENMASK(4, 3)
#define SC2720_CHG_END_V_MASK_SHIT		3

#define SC2720_CHG_CV_V_MASK			GENMASK(10, 5)
#define SC2720_CHG_CV_V_MASK_SHIT		5

#define SC2720_CHG_CC_I_MASK			GENMASK(13, 10)
#define SC2720_CHG_CC_I_MASK_SHIT		10

#define SC2720_CHG_CCCV_MASK			GENMASK(5, 0)


struct sc2720_charge_current {
	int sdp_cur;
	int dcp_cur;
	int cdp_cur;
	int unknown_cur;
};

struct sc2720_charger_info {
	struct device *dev;
	struct regmap *regmap;
	struct power_supply *psy_usb;
	struct sc2720_charge_current cur;
	struct mutex lock;
	bool charging;
	u32 base;
	bool is_charger_online;
};

static int sc2720_set_termination_voltage(struct sc2720_charger_info *info, u32 vol)
{
	struct nvmem_cell *cell;
	u32 big_level, small_level, cv;
	int ret, calib_data;
	void *buf;
	size_t len;

	cell = nvmem_cell_get(info->dev, "cccv_calib");
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);

	if (IS_ERR(buf))
		return PTR_ERR(buf);

	memcpy(&calib_data, buf, min(len, sizeof(u32)));
	calib_data = calib_data & SC2720_CHG_CCCV_MASK;
	kfree(buf);

	if (vol > SC2720_TERM_VOLTAGE_MAX)
		vol = SC2720_TERM_VOLTAGE_MAX;

	if (vol >= SC2720_TERM_VOLTAGE_MIN) {
		u32 temp = vol % SC2720_TERM_VOLTAGE_STEP;

		/*
		 * To set 2720 charger cccv point.
		 * following formula:
		 * cccv point = big_level + small_level + efuse value
		 *
		 * the big_level corresponds to 2720 charger spec register
		 * CHGR_END_V
		 * the small_level corresponds to 2720 charger spec register
		 * CHGR_CV_V
		 */
		big_level = (vol - SC2720_TERM_VOLTAGE_MIN) / SC2720_TERM_VOLTAGE_STEP;
		cv = DIV_ROUND_CLOSEST(temp * 10, SC2720_CCCV_VOLTAGE_STEP);
		small_level = cv + calib_data;
		if (small_level > SC2720_CHG_CCCV_MAXIMUM) {
			big_level++;
			cv = DIV_ROUND_CLOSEST((SC2720_TERM_VOLTAGE_STEP - temp) * 10,
					       SC2720_CCCV_VOLTAGE_STEP);
			small_level = calib_data - cv;
		}
	} else {
		big_level = 0;
		cv = DIV_ROUND_CLOSEST((SC2720_TERM_VOLTAGE_MIN - vol) * 10,
				       SC2720_CCCV_VOLTAGE_STEP);
		if (cv > calib_data)
			small_level = 0;
		else
			small_level = calib_data - cv;
	}

	ret = regmap_update_bits(info->regmap,
				 info->base + SC2720_CHG_CFG0,
				 SC2720_CHG_END_V_MASK,
				 big_level << SC2720_CHG_END_V_MASK_SHIT);
	if (ret) {
		dev_err(info->dev, "failed to set charge end_v\n");
		return ret;
	}

	ret = regmap_update_bits(info->regmap,
				 info->base + SC2720_CHG_CFG0,
				 SC2720_CHG_CV_V_MASK,
				 small_level << SC2720_CHG_CV_V_MASK_SHIT);
	if (ret)
		dev_err(info->dev, "failed to set charge end_v\n");

	return ret;
}

int sc2720_get_battery_cur(struct power_supply *psy,
			   struct sc2720_charge_current *bat_cur)
{
	struct device_node *battery_np;
	const char *value;
	int err;

	bat_cur->sdp_cur = -EINVAL;
	bat_cur->dcp_cur = -EINVAL;
	bat_cur->cdp_cur = -EINVAL;
	bat_cur->unknown_cur = -EINVAL;

	if (!psy->of_node) {
		dev_warn(&psy->dev, "%s currently only supports devicetree\n",
			 __func__);
		return -ENXIO;
	}

	battery_np = of_parse_phandle(psy->of_node, "monitored-battery", 0);
	if (!battery_np)
		return -ENODEV;

	err = of_property_read_string(battery_np, "compatible", &value);
	if (err)
		goto out_put_node;

	if (strcmp("simple-battery", value)) {
		err = -ENODEV;
		goto out_put_node;
	}

	of_property_read_u32_index(battery_np, "charge-sdp-current-microamp", 0,
				   &bat_cur->sdp_cur);
	of_property_read_u32_index(battery_np, "charge-dcp-current-microamp", 0,
				   &bat_cur->dcp_cur);
	of_property_read_u32_index(battery_np, "charge-cdp-current-microamp", 0,
				   &bat_cur->cdp_cur);
	of_property_read_u32_index(battery_np, "charge-unknown-current-microamp", 0,
				   &bat_cur->unknown_cur);

out_put_node:
	of_node_put(battery_np);
	return err;
}

static int sc2720_charger_hw_init(struct sc2720_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	u32 voltage_max_microvolt;
	int ret;

	ret = sc2720_get_battery_cur(info->psy_usb, &info->cur);
	if (ret) {
		dev_warn(info->dev, "no battery current information is supplied\n");

		info->cur.sdp_cur = 500000;
		info->cur.dcp_cur = 500000;
		info->cur.cdp_cur = 1500000;
		info->cur.unknown_cur = 500000;
	}

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");
		voltage_max_microvolt = 4440;
	} else {
		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);
	}

	ret = regmap_update_bits(info->regmap, info->base + SC2720_CHG_CFG0,
				 SC2720_CHG_CC_MODE,
				 SC2720_CHG_CC_MODE);
	if (ret) {
		dev_err(info->dev, "failed to enable charger cc\n");
		return ret;
	}

	if (voltage_max_microvolt < SC2720_CHG_DPM_4300MV) {
		ret = regmap_update_bits(info->regmap,
					 info->base + SC2720_CHG_CFG0,
					 SC2720_CHG_DPM_MASK,
					 0x2 << SC2720_CHG_DPM_MASK_SHIT);
		if (ret) {
			dev_err(info->dev, "failed to set charger dpm\n");
			return ret;
		}
	} else {
		ret = regmap_update_bits(info->regmap,
					 info->base + SC2720_CHG_CFG0,
					 SC2720_CHG_DPM_MASK,
					 0x3 << SC2720_CHG_DPM_MASK_SHIT);
		if (ret) {
			dev_err(info->dev, "failed to set charger dpm\n");
			return ret;
		}
	}

	/* Set charge termination voltage */
	ret = sc2720_set_termination_voltage(info, voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "failed to set termination voltage\n");
		return ret;
	}

	/* Set charge over voltage protection value 6500mv */
	ret = regmap_update_bits(info->regmap, info->base + SC2720_CHG_CFG1,
				 SC2720_CHG_OVP_MASK,
				 0x01);
	if (ret) {
		dev_err(info->dev, "failed to set charger ovp\n");
		return ret;
	}

	return ret;
}

static int sc2720_charger_start_charge(struct sc2720_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC2720_CHG_CFG0,
				 SC2720_CHG_PD, 0);
	if (ret)
		dev_err(info->dev, "failed to satrt charge\n");

	return ret;
}

static int sc2720_charger_stop_charge(struct sc2720_charger_info *info)
{
	int ret;

	ret = regmap_update_bits(info->regmap,
				 info->base + SC2720_CHG_CFG0,
				 SC2720_CHG_PD,
				 SC2720_CHG_PD);
	if (ret)
		dev_err(info->dev, "failed to stop charge\n");

	return ret;
}

static int sc2720_charger_set_current(struct sc2720_charger_info *info, u32 cur)
{
	int temp, ret;

	cur = cur / 1000;
	if (cur < SC2720_CHG_CURRENT_MIN)
		cur = SC2720_CHG_CURRENT_MIN;

	if (cur < SC2720_CHG_CURRENT_800MA) {
		temp = (cur - SC2720_CHG_CURRENT_MIN) /
			SC2720_CHG_CURRENT_STEP_50MA;
	} else {
		temp = (cur - SC2720_CHG_CURRENT_800MA) /
			SC2720_CHG_CURRENT_STEP_100MA;
		temp += SC2720_CHG_CURRENT_800MA_REG;
	}

	/* Disable charge cc mode */
	ret = regmap_update_bits(info->regmap,
				 info->base + SC2720_CHG_CFG0,
				 SC2720_CHG_CC_MODE, 0);
	if (ret) {
		dev_err(info->dev, "failed to disable charge cc mode\n");
		return ret;
	}

	/* Set charge current */
	ret = regmap_update_bits(info->regmap,
				 info->base + SC2720_CHG_CFG1,
				 SC2720_CHG_CC_I_MASK,
				 temp << SC2720_CHG_CC_I_MASK_SHIT);
	if (ret) {
		dev_err(info->dev, "failed to set charge current\n");
		return ret;
	}

	/* Enable charge cc mode */
	ret = regmap_update_bits(info->regmap,
				 info->base + SC2720_CHG_CFG0,
				 SC2720_CHG_CC_MODE,
				 SC2720_CHG_CC_MODE);
	if (ret)
		dev_err(info->dev, "failed to enable charge cc mode\n");

	return ret;
}

static int sc2720_charger_get_current(struct sc2720_charger_info *info, u32 *cur)
{
	int ret;
	u32 val;

	ret = regmap_read(info->regmap, info->base + SC2720_CHG_CFG0, &val);
	if (ret)
		return ret;

	if (!(val & SC2720_CHG_CC_MODE)) {
		*cur = 0;
		return 0;
	}

	ret = regmap_read(info->regmap, info->base + SC2720_CHG_CFG1, &val);
	if (ret)
		return ret;

	val = (val & SC2720_CHG_CC_I_MASK) >> SC2720_CHG_CC_I_MASK_SHIT;
	if (val >= SC2720_CHG_CURRENT_800MA_REG)
		*cur = SC2720_CHG_CURRENT_800MA +
			(val - SC2720_CHG_CURRENT_800MA_REG) *
			SC2720_CHG_CURRENT_STEP_100MA;
	else
		*cur = SC2720_CHG_CURRENT_MIN +
			val * SC2720_CHG_CURRENT_STEP_50MA;

	return 0;
}

static int sc2720_charger_get_health(struct sc2720_charger_info *info,
				     u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sc2720_charger_get_status(struct sc2720_charger_info *info)
{
	if (info->charging == true)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static int sc2720_charger_set_status(struct sc2720_charger_info *info, int val)
{
	int ret = 0;

	if (!val && info->charging) {
		sc2720_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = sc2720_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static int sc2720_charger_usb_get_property(struct power_supply *psy,
					   enum power_supply_property psp,
					   union power_supply_propval *val)
{
	struct sc2720_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, health;

	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = sc2720_charger_get_status(info);
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sc2720_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur * 1000;
		}
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			val->intval = info->cur.dcp_cur;
		}
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sc2720_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int
sc2720_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct sc2720_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sc2720_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;

	case POWER_SUPPLY_PROP_STATUS:
		ret = sc2720_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sc2720_set_termination_voltage(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;

	case POWER_SUPPLY_PROP_PRESENT:
		info->is_charger_online = val->intval;
		break;

	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc2720_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_STATUS:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}


static enum power_supply_property sc2720_usb_props[] = {
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
};

static const struct power_supply_desc sc2720_charger_desc = {
	.name			= "sc2720_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sc2720_usb_props,
	.num_properties		= ARRAY_SIZE(sc2720_usb_props),
	.get_property		= sc2720_charger_usb_get_property,
	.set_property		= sc2720_charger_usb_set_property,
	.property_is_writeable	= sc2720_charger_property_is_writeable,
};

static int sc2720_charger_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct sc2720_charger_info *info;
	struct power_supply_config charger_cfg = { };
	int ret;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->lock);
	info->dev = &pdev->dev;

	info->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!info->regmap) {
		dev_err(&pdev->dev, "failed to get charger regmap\n");
		return -ENODEV;
	}

	platform_set_drvdata(pdev, info);

	ret = of_property_read_u32(np, "reg", &info->base);
	if (ret) {
		dev_err(&pdev->dev, "failed to get register address\n");
		return -ENODEV;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = np;
	info->psy_usb = devm_power_supply_register(&pdev->dev,
						   &sc2720_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(&pdev->dev, "failed to register power supply\n");
		return PTR_ERR(info->psy_usb);
	}

	ret = sc2720_charger_hw_init(info);
	if (ret)
		return ret;

	sc2720_charger_stop_charge(info);


	device_init_wakeup(info->dev, true);

	return 0;
}

static int sc2720_charger_remove(struct platform_device *pdev)
{
	struct sc2720_charger_info *info = platform_get_drvdata(pdev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	mutex_destroy(&info->lock);

	return 0;
}

static const struct of_device_id sc2720_charger_of_match[] = {
	{ .compatible = "sprd,sc2720-charger", },
	{ }
};

static struct platform_driver sc2720_charger_driver = {
	.driver = {
		.name = "sc2720-charger",
		.of_match_table = sc2720_charger_of_match,
	},
	.probe = sc2720_charger_probe,
	.remove = sc2720_charger_remove,
};

module_platform_driver(sc2720_charger_driver);

MODULE_DESCRIPTION("Spreadtrum SC2720 Charger Driver");
MODULE_LICENSE("GPL v2");
