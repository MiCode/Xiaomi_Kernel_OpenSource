/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>

#include "main.h"
#include "debug.h"

static struct cnss_vreg_info cnss_vreg_info[] = {
	{NULL, "vdd-wlan-core", 1300000, 1300000, 0, 0},
	{NULL, "vdd-wlan-io", 1800000, 1800000, 0, 0},
	{NULL, "vdd-wlan-xtal-aon", 0, 0, 0, 0},
	{NULL, "vdd-wlan-xtal", 1800000, 1800000, 0, 2},
	{NULL, "vdd-wlan", 0, 0, 0, 0},
	{NULL, "vdd-wlan-ctrl1", 0, 0, 0, 0},
	{NULL, "vdd-wlan-ctrl2", 0, 0, 0, 0},
	{NULL, "vdd-wlan-sp2t", 2700000, 2700000, 0, 0},
	{NULL, "wlan-ant-switch", 2700000, 2700000, 20000, 0},
	{NULL, "wlan-soc-swreg", 1200000, 1200000, 0, 0},
	{NULL, "vdd-wlan-en", 0, 0, 0, 10},
};

#define CNSS_VREG_INFO_SIZE		ARRAY_SIZE(cnss_vreg_info)
#define MAX_PROP_SIZE			32

#define BOOTSTRAP_GPIO			"qcom,enable-bootstrap-gpio"
#define BOOTSTRAP_ACTIVE		"bootstrap_active"
#define WLAN_EN_GPIO			"wlan-en-gpio"
#define WLAN_EN_ACTIVE			"wlan_en_active"
#define WLAN_EN_SLEEP			"wlan_en_sleep"

#define BOOTSTRAP_DELAY			1000
#define WLAN_ENABLE_DELAY		1000

int cnss_get_vreg(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	int i;
	struct cnss_vreg_info *vreg_info;
	struct device *dev;
	struct regulator *reg;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE];
	int len;

	dev = &plat_priv->plat_dev->dev;

	plat_priv->vreg_info = devm_kzalloc(dev, sizeof(cnss_vreg_info),
					    GFP_KERNEL);
	if (!plat_priv->vreg_info) {
		ret = -ENOMEM;
		goto out;
	}

	memcpy(plat_priv->vreg_info, cnss_vreg_info, sizeof(cnss_vreg_info));

	for (i = 0; i < CNSS_VREG_INFO_SIZE; i++) {
		vreg_info = &plat_priv->vreg_info[i];
		reg = devm_regulator_get_optional(dev, vreg_info->name);
		if (IS_ERR(reg)) {
			ret = PTR_ERR(reg);
			if (ret == -ENODEV)
				continue;
			else if (ret == -EPROBE_DEFER)
				cnss_pr_info("EPROBE_DEFER for regulator: %s\n",
					     vreg_info->name);
			else
				cnss_pr_err("Failed to get regulator %s, err = %d\n",
					    vreg_info->name, ret);
			goto out;
		}

		vreg_info->reg = reg;

		snprintf(prop_name, MAX_PROP_SIZE, "qcom,%s-info",
			 vreg_info->name);

		prop = of_get_property(dev->of_node, prop_name, &len);
		cnss_pr_dbg("Got regulator info, name: %s, len: %d\n",
			    prop_name, len);

		if (!prop || len != (4 * sizeof(__be32))) {
			cnss_pr_dbg("Property %s %s, use default\n", prop_name,
				    prop ? "invalid format" : "doesn't exist");
		} else {
			vreg_info->min_uv = be32_to_cpup(&prop[0]);
			vreg_info->max_uv = be32_to_cpup(&prop[1]);
			vreg_info->load_ua = be32_to_cpup(&prop[2]);
			vreg_info->delay_us = be32_to_cpup(&prop[3]);
		}

		cnss_pr_dbg("Got regulator: %s, min_uv: %u, max_uv: %u, load_ua: %u, delay_us: %u\n",
			    vreg_info->name, vreg_info->min_uv,
			    vreg_info->max_uv, vreg_info->load_ua,
			    vreg_info->delay_us);
	}

	return 0;
out:
	return ret;
}

static int cnss_vreg_on(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_vreg_info *vreg_info;
	int i;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	for (i = 0; i < CNSS_VREG_INFO_SIZE; i++) {
		vreg_info = &plat_priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		cnss_pr_dbg("Regulator %s is being enabled\n", vreg_info->name);

		if (vreg_info->min_uv != 0 && vreg_info->max_uv != 0) {
			ret = regulator_set_voltage(vreg_info->reg,
						    vreg_info->min_uv,
						    vreg_info->max_uv);

			if (ret) {
				cnss_pr_err("Failed to set voltage for regulator %s, min_uv: %u, max_uv: %u, err = %d\n",
					    vreg_info->name, vreg_info->min_uv,
					    vreg_info->max_uv, ret);
				break;
			}
		}

		if (vreg_info->load_ua) {
			ret = regulator_set_load(vreg_info->reg,
						 vreg_info->load_ua);

			if (ret < 0) {
				cnss_pr_err("Failed to set load for regulator %s, load: %u, err = %d\n",
					    vreg_info->name, vreg_info->load_ua,
					    ret);
				break;
			}
		}

		if (vreg_info->delay_us)
			udelay(vreg_info->delay_us);

		ret = regulator_enable(vreg_info->reg);
		if (ret) {
			cnss_pr_err("Failed to enable regulator %s, err = %d\n",
				    vreg_info->name, ret);
			break;
		}
	}

	if (ret) {
		for (; i >= 0; i--) {
			vreg_info = &plat_priv->vreg_info[i];

			if (!vreg_info->reg)
				continue;

			regulator_disable(vreg_info->reg);
			if (vreg_info->load_ua)
				regulator_set_load(vreg_info->reg, 0);
			if (vreg_info->min_uv != 0 && vreg_info->max_uv != 0)
				regulator_set_voltage(vreg_info->reg, 0,
						      vreg_info->max_uv);
		}

		return ret;
	}

	return 0;
}

static int cnss_vreg_off(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_vreg_info *vreg_info;
	int i;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	for (i = CNSS_VREG_INFO_SIZE - 1; i >= 0; i--) {
		vreg_info = &plat_priv->vreg_info[i];

		if (!vreg_info->reg)
			continue;

		cnss_pr_dbg("Regulator %s is being disabled\n",
			    vreg_info->name);

		ret = regulator_disable(vreg_info->reg);
		if (ret)
			cnss_pr_err("Failed to disable regulator %s, err = %d\n",
				    vreg_info->name, ret);

		if (vreg_info->load_ua) {
			ret = regulator_set_load(vreg_info->reg, 0);
			if (ret < 0)
				cnss_pr_err("Failed to set load for regulator %s, err = %d\n",
					    vreg_info->name, ret);
		}

		if (vreg_info->min_uv != 0 && vreg_info->max_uv != 0) {
			ret = regulator_set_voltage(vreg_info->reg, 0,
						    vreg_info->max_uv);
			if (ret)
				cnss_pr_err("Failed to set voltage for regulator %s, err = %d\n",
					    vreg_info->name, ret);
		}
	}

	return ret;
}

int cnss_get_pinctrl(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;
	struct cnss_pinctrl_info *pinctrl_info;

	dev = &plat_priv->plat_dev->dev;
	pinctrl_info = &plat_priv->pinctrl_info;

	pinctrl_info->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl_info->pinctrl)) {
		ret = PTR_ERR(pinctrl_info->pinctrl);
		cnss_pr_err("Failed to get pinctrl, err = %d\n", ret);
		goto out;
	}

	if (of_find_property(dev->of_node, BOOTSTRAP_GPIO, NULL)) {
		pinctrl_info->bootstrap_active =
			pinctrl_lookup_state(pinctrl_info->pinctrl,
					     BOOTSTRAP_ACTIVE);
		if (IS_ERR_OR_NULL(pinctrl_info->bootstrap_active)) {
			ret = PTR_ERR(pinctrl_info->bootstrap_active);
			cnss_pr_err("Failed to get bootstrap active state, err = %d\n",
				    ret);
			goto out;
		}
	}

	if (of_find_property(dev->of_node, WLAN_EN_GPIO, NULL)) {
		pinctrl_info->wlan_en_active =
			pinctrl_lookup_state(pinctrl_info->pinctrl,
					     WLAN_EN_ACTIVE);
		if (IS_ERR_OR_NULL(pinctrl_info->wlan_en_active)) {
			ret = PTR_ERR(pinctrl_info->wlan_en_active);
			cnss_pr_err("Failed to get wlan_en active state, err = %d\n",
				    ret);
			goto out;
		}

		pinctrl_info->wlan_en_sleep =
			pinctrl_lookup_state(pinctrl_info->pinctrl,
					     WLAN_EN_SLEEP);
		if (IS_ERR_OR_NULL(pinctrl_info->wlan_en_sleep)) {
			ret = PTR_ERR(pinctrl_info->wlan_en_sleep);
			cnss_pr_err("Failed to get wlan_en sleep state, err = %d\n",
				    ret);
			goto out;
		}
	}

	return 0;
out:
	return ret;
}

static int cnss_select_pinctrl_state(struct cnss_plat_data *plat_priv,
				     bool state)
{
	int ret = 0;
	struct cnss_pinctrl_info *pinctrl_info;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		ret = -ENODEV;
		goto out;
	}

	pinctrl_info = &plat_priv->pinctrl_info;

	if (state) {
		if (!IS_ERR_OR_NULL(pinctrl_info->bootstrap_active)) {
			ret = pinctrl_select_state(
				pinctrl_info->pinctrl,
				pinctrl_info->bootstrap_active);
			if (ret) {
				cnss_pr_err("Failed to select bootstrap active state, err = %d\n",
					    ret);
				goto out;
			}
			udelay(BOOTSTRAP_DELAY);
		}

		if (!IS_ERR_OR_NULL(pinctrl_info->wlan_en_active)) {
			ret = pinctrl_select_state(
				pinctrl_info->pinctrl,
				pinctrl_info->wlan_en_active);
			if (ret) {
				cnss_pr_err("Failed to select wlan_en active state, err = %d\n",
					    ret);
				goto out;
			}
			udelay(WLAN_ENABLE_DELAY);
		}
	} else {
		if (!IS_ERR_OR_NULL(pinctrl_info->wlan_en_sleep)) {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
						   pinctrl_info->wlan_en_sleep);
			if (ret) {
				cnss_pr_err("Failed to select wlan_en sleep state, err = %d\n",
					    ret);
				goto out;
			}
		}
	}

	return 0;
out:
	return ret;
}

int cnss_power_on_device(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

	if (plat_priv->powered_on) {
		cnss_pr_dbg("Already powered up");
		return 0;
	}

	ret = cnss_vreg_on(plat_priv);
	if (ret) {
		cnss_pr_err("Failed to turn on vreg, err = %d\n", ret);
		goto out;
	}

	ret = cnss_select_pinctrl_state(plat_priv, true);
	if (ret) {
		cnss_pr_err("Failed to select pinctrl state, err = %d\n", ret);
		goto vreg_off;
	}
	plat_priv->powered_on = true;

	return 0;
vreg_off:
	cnss_vreg_off(plat_priv);
out:
	return ret;
}

void cnss_power_off_device(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv->powered_on) {
		cnss_pr_dbg("Already powered down");
		return;
	}

	cnss_select_pinctrl_state(plat_priv, false);
	cnss_vreg_off(plat_priv);
	plat_priv->powered_on = false;
}

void cnss_set_pin_connect_status(struct cnss_plat_data *plat_priv)
{
	unsigned long pin_status = 0;

	set_bit(CNSS_WLAN_EN, &pin_status);
	set_bit(CNSS_PCIE_TXN, &pin_status);
	set_bit(CNSS_PCIE_TXP, &pin_status);
	set_bit(CNSS_PCIE_RXN, &pin_status);
	set_bit(CNSS_PCIE_RXP, &pin_status);
	set_bit(CNSS_PCIE_REFCLKN, &pin_status);
	set_bit(CNSS_PCIE_REFCLKP, &pin_status);
	set_bit(CNSS_PCIE_RST, &pin_status);

	plat_priv->pin_result.host_pin_result = pin_status;
}
