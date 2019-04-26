/* Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

static struct cnss_vreg_cfg cnss_vreg_list[] = {
	{"vdd-wlan-core", 1300000, 1300000, 0, 0},
	{"vdd-wlan-io", 1800000, 1800000, 0, 0},
	{"vdd-wlan-xtal-aon", 0, 0, 0, 0},
	{"vdd-wlan-xtal", 1800000, 1800000, 0, 2},
	{"vdd-wlan", 0, 0, 0, 0},
	{"vdd-wlan-aon", 1055000, 1055000, 0, 0},
	{"vdd-wlan-rfa1", 1350000, 1350000, 0, 0},
	{"vdd-wlan-rfa2", 2040000, 2040000, 0, 0},
	{"vdd-wlan-rfa3", 1900000, 1900000, 0, 0},
	{"vdd-wlan-ctrl1", 0, 0, 0, 0},
	{"vdd-wlan-ctrl2", 0, 0, 0, 0},
	{"vdd-wlan-sp2t", 2700000, 2700000, 0, 0},
	{"wlan-ant-switch", 2700000, 2700000, 20000, 0},
	{"wlan-soc-swreg", 1200000, 1200000, 0, 0},
	{"vdd-wlan-en", 0, 0, 0, 10},
};

#define CNSS_VREG_INFO_SIZE		ARRAY_SIZE(cnss_vreg_list)
#define MAX_PROP_SIZE			32

#define BOOTSTRAP_GPIO			"qcom,enable-bootstrap-gpio"
#define BOOTSTRAP_ACTIVE		"bootstrap_active"
#define WLAN_EN_GPIO			"wlan-en-gpio"
#define WLAN_EN_ACTIVE			"wlan_en_active"
#define WLAN_EN_SLEEP			"wlan_en_sleep"
#define WLAN_VREGS_PROP			"wlan_vregs"

#define BOOTSTRAP_DELAY			1000
#define WLAN_ENABLE_DELAY		1000

/* For converged dt node, get the required vregs from property 'wlan_vregs',
 * which is string array; if the property is present but no value is set,
 * means no additional wlan verg is required.
 * For non-converged dt, go through all vregs in static array 'cnss_vreg_list'.
 */
int cnss_get_vreg(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	int i;
	struct cnss_vreg_info *vreg;
	struct device *dev;
	struct regulator *reg;
	const __be32 *prop;
	char prop_name[MAX_PROP_SIZE] = {0};
	int len, id_n;
	struct device_node *dt_node;

	if (!list_empty(&plat_priv->vreg_list) &&
	    !plat_priv->is_converged_dt) {
		cnss_pr_dbg("Vregs have already been updated\n");
		return 0;
	}

	dev = &plat_priv->plat_dev->dev;
	dt_node = (plat_priv->dev_node ? plat_priv->dev_node : dev->of_node);

	if (plat_priv->is_converged_dt) {
		id_n = of_property_count_strings(dt_node, WLAN_VREGS_PROP);
		if (id_n <= 0) {
			if (id_n == -ENODATA) {
				cnss_pr_dbg("No additional vregs for: %s:%lx\n",
					    dt_node->name,
					    plat_priv->device_id);
				return 0;
			}

			cnss_pr_err("property %s is invalid or missed: %s:%lx\n",
				    WLAN_VREGS_PROP, dt_node->name,
				    plat_priv->device_id);
			return -EINVAL;
		}
	} else {
		id_n = CNSS_VREG_INFO_SIZE;
	}

	for (i = 0; i < id_n; i++) {
		vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
		if (!vreg) {
			ret = -ENOMEM;
			goto out;
		}

		if (plat_priv->is_converged_dt) {
			ret = of_property_read_string_index(dt_node,
							    WLAN_VREGS_PROP, i,
							    &vreg->cfg.name);
			if (ret) {
				devm_kfree(dev, vreg);
				cnss_pr_err("Failed to read vreg ids\n");
				goto out;
			}
		} else {
			memcpy(&vreg->cfg, &cnss_vreg_list[i],
			       sizeof(vreg->cfg));
		}

		reg = devm_regulator_get_optional(dev, vreg->cfg.name);
		if (IS_ERR(reg)) {
			ret = PTR_ERR(reg);
			if (ret == -ENODEV) {
				devm_kfree(dev, vreg);
				continue;
			}
			else if (ret == -EPROBE_DEFER)
				cnss_pr_info("EPROBE_DEFER for regulator: %s\n",
					     vreg->cfg.name);
			else
				cnss_pr_err("Failed to get regulator %s, err = %d\n",
					    vreg->cfg.name, ret);

			devm_kfree(dev, vreg);
			goto out;
		}

		vreg->reg = reg;
		snprintf(prop_name, MAX_PROP_SIZE, "qcom,%s-info",
			 vreg->cfg.name);
		prop = of_get_property(dt_node, prop_name, &len);
		if (!prop || len != (4 * sizeof(__be32))) {
			cnss_pr_dbg("Property %s %s, use default\n", prop_name,
				    prop ? "invalid format" : "doesn't exist");
		} else {
			vreg->cfg.min_uv = be32_to_cpup(&prop[0]);
			vreg->cfg.max_uv = be32_to_cpup(&prop[1]);
			vreg->cfg.load_ua = be32_to_cpup(&prop[2]);
			vreg->cfg.delay_us = be32_to_cpup(&prop[3]);
		}

		list_add_tail(&vreg->list, &plat_priv->vreg_list);
		cnss_pr_dbg("Got regulator: %s, min_uv: %u, max_uv: %u, load_ua: %u, delay_us: %u\n",
			    vreg->cfg.name, vreg->cfg.min_uv,
			    vreg->cfg.max_uv, vreg->cfg.load_ua,
			    vreg->cfg.delay_us);
	}

	return 0;
out:
	return ret;
}

void cnss_put_vreg(struct cnss_plat_data *plat_priv)
{
	struct device *dev;
	struct cnss_vreg_info *vreg;

	dev = &plat_priv->plat_dev->dev;
	while (!list_empty(&plat_priv->vreg_list)) {
		vreg = list_first_entry(&plat_priv->vreg_list,
					struct cnss_vreg_info, list);
		list_del(&vreg->list);

		if (IS_ERR_OR_NULL(vreg->reg))
			continue;

		cnss_pr_dbg("Put regulator: %s\n", vreg->cfg.name);
		devm_regulator_put(vreg->reg);
		devm_kfree(dev, vreg);
	}
}

static int cnss_vreg_on(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_vreg_info *vreg;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	list_for_each_entry(vreg, &plat_priv->vreg_list, list) {
		if (IS_ERR_OR_NULL(vreg->reg))
			continue;

		if (vreg->enabled) {
			cnss_pr_dbg("Regulator %s is already enabled\n",
				    vreg->cfg.name);
			continue;
		}

		cnss_pr_dbg("Regulator %s is being enabled\n",
			    vreg->cfg.name);

		if (vreg->cfg.min_uv != 0 && vreg->cfg.max_uv != 0) {
			ret = regulator_set_voltage(vreg->reg,
						    vreg->cfg.min_uv,
						    vreg->cfg.max_uv);

			if (ret) {
				cnss_pr_err("Failed to set voltage for regulator %s, min_uv: %u, max_uv: %u, err = %d\n",
					    vreg->cfg.name,
					    vreg->cfg.min_uv,
					    vreg->cfg.max_uv, ret);
				break;
			}
		}

		if (vreg->cfg.load_ua) {
			ret = regulator_set_load(vreg->reg, vreg->cfg.load_ua);

			if (ret < 0) {
				cnss_pr_err("Failed to set load for regulator %s, load: %u, err = %d\n",
					    vreg->cfg.name, vreg->cfg.load_ua,
					    ret);
				break;
			}
		}

		if (vreg->cfg.delay_us)
			udelay(vreg->cfg.delay_us);

		ret = regulator_enable(vreg->reg);
		if (ret) {
			cnss_pr_err("Failed to enable regulator %s, err = %d\n",
				    vreg->cfg.name, ret);
			break;
		}
		vreg->enabled = true;
	}

	if (!ret)
		return 0;

	list_for_each_entry_continue_reverse(vreg, &plat_priv->vreg_list,
					     list) {
		if (IS_ERR_OR_NULL(vreg->reg) || !vreg->enabled)
			continue;

		regulator_disable(vreg->reg);
		if (vreg->cfg.load_ua)
			regulator_set_load(vreg->reg, 0);
		if (vreg->cfg.min_uv != 0 && vreg->cfg.max_uv != 0)
			regulator_set_voltage(vreg->reg, 0, vreg->cfg.max_uv);
		vreg->enabled = false;
	}

	return ret;
}

static int cnss_vreg_off(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct cnss_vreg_info *vreg;

	if (!plat_priv) {
		cnss_pr_err("plat_priv is NULL!\n");
		return -ENODEV;
	}

	list_for_each_entry_reverse(vreg, &plat_priv->vreg_list, list) {
		if (IS_ERR_OR_NULL(vreg->reg))
			continue;

		if (!vreg->enabled) {
			cnss_pr_dbg("Regulator %s is already disabled\n",
				    vreg->cfg.name);
			continue;
		}

		cnss_pr_dbg("Regulator %s is being disabled\n",
			    vreg->cfg.name);

		ret = regulator_disable(vreg->reg);
		if (ret)
			cnss_pr_err("Failed to disable regulator %s, err = %d\n",
				    vreg->cfg.name, ret);

		if (vreg->cfg.load_ua) {
			ret = regulator_set_load(vreg->reg, 0);
			if (ret < 0)
				cnss_pr_err("Failed to set load for regulator %s, err = %d\n",
					    vreg->cfg.name, ret);
		}

		if (vreg->cfg.min_uv != 0 && vreg->cfg.max_uv != 0) {
			ret = regulator_set_voltage(vreg->reg, 0,
						    vreg->cfg.max_uv);
			if (ret)
				cnss_pr_err("Failed to set voltage for regulator %s, err = %d\n",
					    vreg->cfg.name, ret);
		}
		vreg->enabled = false;
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

void cnss_put_pinctrl(struct cnss_plat_data *plat_priv)
{
	struct pinctrl *pinctrl;

	pinctrl = plat_priv->pinctrl_info.pinctrl;
	if (IS_ERR_OR_NULL(pinctrl))
		return;

	devm_pinctrl_put(pinctrl);
	memset(&plat_priv->pinctrl_info, 0, sizeof(plat_priv->pinctrl_info));
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
		if (pinctrl_info->activated) {
			cnss_pr_dbg("Pinctrl is already activated\n");
			goto out;
		}

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
		pinctrl_info->activated = true;
	} else {
		if (!pinctrl_info->activated) {
			cnss_pr_dbg("Pinctrl is already de-activated\n");
			goto out;
		}

		if (!IS_ERR_OR_NULL(pinctrl_info->wlan_en_sleep)) {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
						   pinctrl_info->wlan_en_sleep);
			if (ret) {
				cnss_pr_err("Failed to select wlan_en sleep state, err = %d\n",
					    ret);
				goto out;
			}
		}
		pinctrl_info->activated = false;
	}

	return 0;
out:
	return ret;
}

int cnss_power_on_device(struct cnss_plat_data *plat_priv)
{
	int ret = 0;

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

	return 0;
vreg_off:
	cnss_vreg_off(plat_priv);
out:
	return ret;
}

void cnss_power_off_device(struct cnss_plat_data *plat_priv)
{
	cnss_select_pinctrl_state(plat_priv, false);
	cnss_vreg_off(plat_priv);
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

/* If it's converged dt, get device specific regulators and enable them. */
int cnss_dev_specific_power_on(struct cnss_plat_data *plat_priv)
{
	int ret;

	if (!plat_priv->is_converged_dt)
		return 0;

	ret = cnss_get_vreg(plat_priv);
	if (ret)
		return ret;

	return cnss_power_on_device(plat_priv);
}
