// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */
#include <linux/clk.h>
#include <linux/delay.h>
#if IS_ENABLED(CONFIG_MSM_QMP)
#include <linux/mailbox/qmp.h>
#endif
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/cmd-db.h>
#include "main.h"
#include "qmi.h"
#include "debug.h"
#include "power.h"

static struct icnss_vreg_cfg icnss_wcn6750_vreg_list[] = {
	{"vdd-cx-mx", 824000, 952000, 0, 0, 0, false, true},
	{"vdd-1.8-xo", 1872000, 1872000, 0, 0, 0, false, true},
	{"vdd-1.3-rfa", 1256000, 1352000, 0, 0, 0, false, true},
};

static struct icnss_vreg_cfg icnss_adrestea_vreg_list[] = {
	{"vdd-cx-mx", 752000, 752000, 0, 0, 0, false, true},
	{"vdd-1.8-xo", 1800000, 1800000, 0, 0, 0, false, true},
	{"vdd-1.3-rfa", 1304000, 1304000, 0, 0, 0, false, true},
	{"vdd-3.3-ch1", 3312000, 3312000, 0, 0, 0, false, true},
	{"vdd-3.3-ch0", 3312000, 3312000, 0, 0, 0, false, true},
};

static struct icnss_vreg_cfg icnss_wcn6450_vreg_list[] = {
	{"vdd-cx-mx", 824000, 952000, 0, 0, 0, false, true},
	{"vdd-1.8-xo", 1872000, 1872000, 0, 0, 0, false, true},
	{"vdd-1.3-rfa", 1256000, 1352000, 0, 0, 0, false, true},
	{"vdd-aon", 1256000, 1352000, 0, 0, 0, false, true},
};

static struct icnss_clk_cfg icnss_clk_list[] = {
	{"rf_clk", 0, 0},
};

static struct icnss_clk_cfg icnss_adrestea_clk_list[] = {
	{"cxo_ref_clk_pin", 0, 0},
};

#define ICNSS_VREG_LIST_SIZE		ARRAY_SIZE(icnss_wcn6750_vreg_list)
#define ICNSS_VREG_ADRESTEA_LIST_SIZE	ARRAY_SIZE(icnss_adrestea_vreg_list)
#define ICNSS_VREG_EVROS_LIST_SIZE	ARRAY_SIZE(icnss_wcn6450_vreg_list)
#define ICNSS_CLK_LIST_SIZE		ARRAY_SIZE(icnss_clk_list)
#define ICNSS_CLK_ADRESTEA_LIST_SIZE	ARRAY_SIZE(icnss_adrestea_clk_list)

#define ICNSS_CHAIN1_REGULATOR                          "vdd-3.3-ch1"
#define MAX_PROP_SIZE					32
#define ICNSS_THRESHOLD_HIGH				3600000
#define ICNSS_THRESHOLD_LOW				3450000
#define ICNSS_THRESHOLD_GUARD				20000

#define BT_CXMX_VOLTAGE_MV		950
#define ICNSS_MBOX_MSG_MAX_LEN 64
#define ICNSS_MBOX_TIMEOUT_MS 1000

#define WLAN_PON_EN			"wlan_pon_en"
#define WLAN_PON_DIS			"wlan_pon_dis"
#define WLAN_POFF_EN			"wlan_poff_en"
#define WLAN_POFF_DIS			"wlan_poff_dis"
#define WLAN_PON_DELAY			20

/**
 * enum icnss_vreg_param: Voltage regulator TCS param
 * @ICNSS_VREG_VOLTAGE: Provides voltage level to be configured in TCS
 * @ICNSS_VREG_MODE: Regulator mode
 * @ICNSS_VREG_ENABLE: Set Voltage regulator enable config in TCS
 */
enum icnss_vreg_param {
	ICNSS_VREG_VOLTAGE,
	ICNSS_VREG_MODE,
	ICNSS_VREG_ENABLE,
};

/**
 * enum icnss_tcs_seq: TCS sequence ID for trigger
 * ICNSS_TCS_UP_SEQ: TCS Sequence based on up trigger / Wake TCS
 * ICNSS_TCS_DOWN_SEQ: TCS Sequence based on down trigger / Sleep TCS
 * ICNSS_TCS_ALL_SEQ: Update for both up and down triggers
 */
enum icnss_tcs_seq {
	ICNSS_TCS_UP_SEQ,
	ICNSS_TCS_DOWN_SEQ,
	ICNSS_TCS_ALL_SEQ,
};

static int icnss_get_vreg_single(struct icnss_priv *priv,
				 struct icnss_vreg_info *vreg)
{
	int ret = 0;
	struct device *dev = NULL;
	struct regulator *reg = NULL;
	const __be32 *prop = NULL;
	char prop_name[MAX_PROP_SIZE] = {0};
	int len = 0;
	int i;

	dev = &priv->pdev->dev;

	reg = devm_regulator_get_optional(dev, vreg->cfg.name);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		if (ret == -ENODEV) {
			return ret;
		} else if (ret == -EPROBE_DEFER) {
			icnss_pr_info("EPROBE_DEFER for regulator: %s\n",
				      vreg->cfg.name);
			goto out;
		} else if (priv->device_id == ADRASTEA_DEVICE_ID) {
			if (vreg->cfg.required) {
				icnss_pr_err("Regulator %s doesn't exist: %d\n",
					     vreg->cfg.name, ret);
			goto out;
			} else {
				icnss_pr_dbg("Optional regulator %s doesn't exist: %d\n",
					     vreg->cfg.name, ret);
				goto done;
			}
		} else {
			icnss_pr_err("Failed to get regulator %s, err = %d\n",
				     vreg->cfg.name, ret);
			goto out;
		}
	}

	vreg->reg = reg;

	snprintf(prop_name, MAX_PROP_SIZE, "qcom,%s-config",
		 vreg->cfg.name);

	prop = of_get_property(dev->of_node, prop_name, &len);

	icnss_pr_dbg("Got regulator config, prop: %s, len: %d\n",
		     prop_name, len);

	if (!prop || len < (2 * sizeof(__be32))) {
		icnss_pr_dbg("Property %s %s, use default\n", prop_name,
			     prop ? "invalid format" : "doesn't exist");
		goto done;
	}

	for (i = 0; (i * sizeof(__be32)) < len; i++) {
		switch (i) {
		case 0:
			vreg->cfg.min_uv = be32_to_cpup(&prop[0]);
			break;
		case 1:
			vreg->cfg.max_uv = be32_to_cpup(&prop[1]);
			break;
		case 2:
			vreg->cfg.load_ua = be32_to_cpup(&prop[2]);
			break;
		case 3:
			vreg->cfg.delay_us = be32_to_cpup(&prop[3]);
			break;
		case 4:
			vreg->cfg.need_unvote = be32_to_cpup(&prop[4]);
			break;
		default:
			icnss_pr_dbg("Property %s, ignoring value at %d\n",
				     prop_name, i);
			break;
		}
	}

done:
	icnss_pr_dbg("Got regulator: %s, min_uv: %u, max_uv: %u, load_ua: %u, delay_us: %u, need_unvote: %u\n",
		     vreg->cfg.name, vreg->cfg.min_uv,
		     vreg->cfg.max_uv, vreg->cfg.load_ua,
		     vreg->cfg.delay_us, vreg->cfg.need_unvote);

	return 0;

out:
	return ret;
}

static int icnss_vreg_on_single(struct icnss_vreg_info *vreg)
{
	int ret = 0;

	if (vreg->enabled) {
		icnss_pr_dbg("Regulator %s is already enabled\n",
			     vreg->cfg.name);
		return 0;
	}

	icnss_pr_dbg("Regulator %s is being enabled\n", vreg->cfg.name);

	if (vreg->cfg.min_uv != 0 && vreg->cfg.max_uv != 0) {
		ret = regulator_set_voltage(vreg->reg,
					    vreg->cfg.min_uv,
					    vreg->cfg.max_uv);

		if (ret) {
			icnss_pr_err("Failed to set voltage for regulator %s, min_uv: %u, max_uv: %u, err = %d\n",
				     vreg->cfg.name, vreg->cfg.min_uv,
				     vreg->cfg.max_uv, ret);
			goto out;
		}
	}

	if (vreg->cfg.load_ua) {
		ret = regulator_set_load(vreg->reg,
					 vreg->cfg.load_ua);

		if (ret < 0) {
			icnss_pr_err("Failed to set load for regulator %s, load: %u, err = %d\n",
				     vreg->cfg.name, vreg->cfg.load_ua,
				     ret);
			goto out;
		}
	}

	if (vreg->cfg.delay_us)
		udelay(vreg->cfg.delay_us);

	ret = regulator_enable(vreg->reg);
	if (ret) {
		icnss_pr_err("Failed to enable regulator %s, err = %d\n",
			     vreg->cfg.name, ret);
		goto out;
	}

	vreg->enabled = true;

out:
	return ret;
}

static int icnss_vreg_unvote_single(struct icnss_vreg_info *vreg)
{
	int ret = 0;

	if (!vreg->enabled) {
		icnss_pr_dbg("Regulator %s is already disabled\n",
			     vreg->cfg.name);
		return 0;
	}

	icnss_pr_dbg("Removing vote for Regulator %s\n", vreg->cfg.name);

	if (vreg->cfg.load_ua) {
		ret = regulator_set_load(vreg->reg, 0);
		if (ret < 0)
			icnss_pr_err("Failed to set load for regulator %s, err = %d\n",
				     vreg->cfg.name, ret);
	}

	if (vreg->cfg.min_uv != 0 && vreg->cfg.max_uv != 0) {
		ret = regulator_set_voltage(vreg->reg, 0,
					    vreg->cfg.max_uv);
		if (ret)
			icnss_pr_err("Failed to set voltage for regulator %s, err = %d\n",
				     vreg->cfg.name, ret);
	}

	return ret;
}

static int icnss_vreg_off_single(struct icnss_vreg_info *vreg)
{
	int ret = 0;

	if (!vreg->enabled) {
		icnss_pr_dbg("Regulator %s is already disabled\n",
			     vreg->cfg.name);
		return 0;
	}

	icnss_pr_dbg("Regulator %s is being disabled\n",
		     vreg->cfg.name);

	ret = regulator_disable(vreg->reg);
	if (ret)
		icnss_pr_err("Failed to disable regulator %s, err = %d\n",
			     vreg->cfg.name, ret);

	if (vreg->cfg.load_ua) {
		ret = regulator_set_load(vreg->reg, 0);
		if (ret < 0)
			icnss_pr_err("Failed to set load for regulator %s, err = %d\n",
				     vreg->cfg.name, ret);
	}

	if (vreg->cfg.min_uv != 0 && vreg->cfg.max_uv != 0) {
		ret = regulator_set_voltage(vreg->reg, 0,
					    vreg->cfg.max_uv);
		if (ret)
			icnss_pr_err("Failed to set voltage for regulator %s, err = %d\n",
				     vreg->cfg.name, ret);
	}
	vreg->enabled = false;

	return ret;
}

static struct icnss_vreg_cfg *get_vreg_list(u32 *vreg_list_size,
					    unsigned long device_id)
{
	switch (device_id) {
	case WCN6750_DEVICE_ID:
		*vreg_list_size = ICNSS_VREG_LIST_SIZE;
		return icnss_wcn6750_vreg_list;

	case ADRASTEA_DEVICE_ID:
		*vreg_list_size = ICNSS_VREG_ADRESTEA_LIST_SIZE;
		return icnss_adrestea_vreg_list;

	case WCN6450_DEVICE_ID:
		*vreg_list_size = ICNSS_VREG_EVROS_LIST_SIZE;
		return icnss_wcn6450_vreg_list;

	default:
		icnss_pr_err("Unsupported device_id 0x%x\n", device_id);
		*vreg_list_size = 0;
		return NULL;
	}
}

int icnss_get_vreg(struct icnss_priv *priv)
{
	int ret = 0;
	int i;
	struct icnss_vreg_info *vreg;
	struct icnss_vreg_cfg *vreg_cfg = NULL;
	struct list_head *vreg_list = &priv->vreg_list;
	struct device *dev = &priv->pdev->dev;
	u32 vreg_list_size = 0;

	vreg_cfg = get_vreg_list(&vreg_list_size, priv->device_id);
	if (!vreg_cfg)
		return -EINVAL;

	for (i = 0; i < vreg_list_size; i++) {
		vreg = devm_kzalloc(dev, sizeof(*vreg), GFP_KERNEL);
		if (!vreg)
			return -ENOMEM;

		memcpy(&vreg->cfg, &vreg_cfg[i], sizeof(vreg->cfg));
		ret = icnss_get_vreg_single(priv, vreg);
		if (ret != 0) {
			if (ret == -ENODEV)
				continue;
			else
				return ret;
		}
		list_add_tail(&vreg->list, vreg_list);
	}

	return 0;
}

void icnss_put_vreg(struct icnss_priv *priv)
{
	struct list_head *vreg_list = &priv->vreg_list;
	struct icnss_vreg_info *vreg = NULL;

	while (!list_empty(vreg_list)) {
		vreg = list_first_entry(vreg_list,
					struct icnss_vreg_info, list);
		list_del(&vreg->list);
	}
}

static int icnss_vreg_on(struct icnss_priv *priv)
{
	struct list_head *vreg_list = &priv->vreg_list;
	struct icnss_vreg_info *vreg = NULL;
	int ret = 0;

	list_for_each_entry(vreg, vreg_list, list) {
		if (IS_ERR_OR_NULL(vreg->reg) || !vreg->cfg.is_supported)
			continue;
		if (!priv->chain_reg_info_updated &&
		    !strcmp(ICNSS_CHAIN1_REGULATOR, vreg->cfg.name)) {
			priv->chain_reg_info_updated = true;
			if (!priv->is_chain1_supported) {
				vreg->cfg.is_supported = false;
				continue;
			}
		}

		ret = icnss_vreg_on_single(vreg);
		if (ret)
			break;
	}

	if (!ret)
		return 0;

	list_for_each_entry_continue_reverse(vreg, vreg_list, list) {
		if (IS_ERR_OR_NULL(vreg->reg) || !vreg->enabled)
			continue;

		icnss_vreg_off_single(vreg);
	}

	return ret;
}

static int icnss_vreg_off(struct icnss_priv *priv)
{
	struct list_head *vreg_list = &priv->vreg_list;
	struct icnss_vreg_info *vreg = NULL;

	list_for_each_entry_reverse(vreg, vreg_list, list) {
		if (IS_ERR_OR_NULL(vreg->reg))
			continue;

		icnss_vreg_off_single(vreg);
	}

	return 0;
}

int icnss_vreg_unvote(struct icnss_priv *priv)
{
	struct list_head *vreg_list = &priv->vreg_list;
	struct icnss_vreg_info *vreg = NULL;

	list_for_each_entry_reverse(vreg, vreg_list, list) {
		if (IS_ERR_OR_NULL(vreg->reg))
			continue;

		if (vreg->cfg.need_unvote)
			icnss_vreg_unvote_single(vreg);
	}

	return 0;
}

int icnss_get_clk_single(struct icnss_priv *priv,
			 struct icnss_clk_info *clk_info)
{
	struct device *dev = &priv->pdev->dev;
	struct clk *clk;
	int ret;

	clk = devm_clk_get(dev, clk_info->cfg.name);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		if (clk_info->cfg.required)
			icnss_pr_err("Failed to get clock %s, err = %d\n",
				     clk_info->cfg.name, ret);
		else
			icnss_pr_dbg("Failed to get optional clock %s, err = %d\n",
				     clk_info->cfg.name, ret);
		return ret;
	}

	clk_info->clk = clk;
	icnss_pr_dbg("Got clock: %s, freq: %u\n",
		     clk_info->cfg.name, clk_info->cfg.freq);

	return 0;
}

static int icnss_clk_on_single(struct icnss_clk_info *clk_info)
{
	int ret;

	if (clk_info->enabled) {
		icnss_pr_dbg("Clock %s is already enabled\n",
			     clk_info->cfg.name);
		return 0;
	}

	icnss_pr_dbg("Clock %s is being enabled\n", clk_info->cfg.name);

	if (clk_info->cfg.freq) {
		ret = clk_set_rate(clk_info->clk, clk_info->cfg.freq);
		if (ret) {
			icnss_pr_err("Failed to set frequency %u for clock %s, err = %d\n",
				     clk_info->cfg.freq, clk_info->cfg.name,
				     ret);
			return ret;
		}
	}

	ret = clk_prepare_enable(clk_info->clk);
	if (ret) {
		icnss_pr_err("Failed to enable clock %s, err = %d\n",
			     clk_info->cfg.name, ret);
		return ret;
	}

	clk_info->enabled = true;

	return 0;
}

static int icnss_clk_off_single(struct icnss_clk_info *clk_info)
{
	if (!clk_info->enabled) {
		icnss_pr_dbg("Clock %s is already disabled\n",
			     clk_info->cfg.name);
		return 0;
	}

	icnss_pr_dbg("Clock %s is being disabled\n", clk_info->cfg.name);

	clk_disable_unprepare(clk_info->clk);
	clk_info->enabled = false;

	return 0;
}

int icnss_get_clk(struct icnss_priv *priv)
{
	struct device *dev;
	struct list_head *clk_list;
	struct icnss_clk_info *clk_info;
	struct icnss_clk_cfg *clk_cfg;
	int ret, i;
	u32 clk_list_size = 0;

	if (!priv)
		return -ENODEV;

	dev = &priv->pdev->dev;
	clk_list = &priv->clk_list;

	if (priv->device_id == ADRASTEA_DEVICE_ID) {
		clk_cfg = icnss_adrestea_clk_list;
		clk_list_size = ICNSS_CLK_ADRESTEA_LIST_SIZE;
	} else if (priv->device_id == WCN6750_DEVICE_ID ||
		   priv->device_id == WCN6450_DEVICE_ID) {
		clk_cfg = icnss_clk_list;
		clk_list_size = ICNSS_CLK_LIST_SIZE;
	}

	if (!list_empty(clk_list)) {
		icnss_pr_dbg("Clocks have already been updated\n");
		return 0;
	}

	for (i = 0; i < clk_list_size; i++) {
		clk_info = devm_kzalloc(dev, sizeof(*clk_info), GFP_KERNEL);
		if (!clk_info) {
			ret = -ENOMEM;
			goto cleanup;
		}

		memcpy(&clk_info->cfg, &clk_cfg[i],
		       sizeof(clk_info->cfg));
		ret = icnss_get_clk_single(priv, clk_info);
		if (ret != 0) {
			if (clk_info->cfg.required)
				goto cleanup;
			else
				continue;
		}
		list_add_tail(&clk_info->list, clk_list);
	}

	return 0;

cleanup:
	while (!list_empty(clk_list)) {
		clk_info = list_first_entry(clk_list, struct icnss_clk_info,
					    list);
		list_del(&clk_info->list);
	}

	return ret;
}

void icnss_put_clk(struct icnss_priv *priv)
{
	struct device *dev;
	struct list_head *clk_list;
	struct icnss_clk_info *clk_info;

	if (!priv)
		return;

	dev = &priv->pdev->dev;
	clk_list = &priv->clk_list;

	while (!list_empty(clk_list)) {
		clk_info = list_first_entry(clk_list, struct icnss_clk_info,
					    list);
		list_del(&clk_info->list);
	}
}

static int icnss_clk_on(struct list_head *clk_list)
{
	struct icnss_clk_info *clk_info;
	int ret = 0;

	list_for_each_entry(clk_info, clk_list, list) {
		if (IS_ERR_OR_NULL(clk_info->clk))
			continue;
		ret = icnss_clk_on_single(clk_info);
		if (ret)
			break;
	}

	if (!ret)
		return 0;

	list_for_each_entry_continue_reverse(clk_info, clk_list, list) {
		if (IS_ERR_OR_NULL(clk_info->clk))
			continue;

		icnss_clk_off_single(clk_info);
	}

	return ret;
}

static int icnss_clk_off(struct list_head *clk_list)
{
	struct icnss_clk_info *clk_info;

	list_for_each_entry_reverse(clk_info, clk_list, list) {
		if (IS_ERR_OR_NULL(clk_info->clk))
			continue;

		icnss_clk_off_single(clk_info);
	}

	return 0;
}

int icnss_hw_power_on(struct icnss_priv *priv)
{
	int ret = 0;

	icnss_pr_dbg("HW Power on: state: 0x%lx\n", priv->state);

	spin_lock(&priv->on_off_lock);
	if (test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock(&priv->on_off_lock);
		return ret;
	}
	set_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock(&priv->on_off_lock);

	ret = icnss_vreg_on(priv);
	if (ret) {
		icnss_pr_err("Failed to turn on vreg, err = %d\n", ret);
		goto out;
	}

	ret = icnss_clk_on(&priv->clk_list);
	if (ret)
		goto vreg_off;

	if (priv->pon_gpio_control) {
		/* Better to power off and then power on, to rule
		 * out state mismatch between WPSS and wlan chip
		 */
		icnss_power_trigger_pinctrl(&priv->pdev->dev,
					    ICNSS_PINCTRL_OWNER_WLAN,
					    ICNSS_PINCTRL_SEQ_OFF);
		ret = icnss_power_trigger_pinctrl(&priv->pdev->dev,
						  ICNSS_PINCTRL_OWNER_WLAN,
						  ICNSS_PINCTRL_SEQ_ON);
		if (ret) {
			icnss_pr_err("Failed to select pinctrl state, err = %d\n", ret);
			goto clk_off;
		}
	}

	return ret;

clk_off:
	icnss_clk_off(&priv->clk_list);

vreg_off:
	icnss_vreg_off(priv);
out:
	clear_bit(ICNSS_POWER_ON, &priv->state);
	return ret;
}

int icnss_hw_power_off(struct icnss_priv *priv)
{
	int ret = 0;

	if (test_bit(HW_ALWAYS_ON, &priv->ctrl_params.quirks))
		return 0;

	if (test_bit(ICNSS_FW_DOWN, &priv->state))
		return 0;

	icnss_pr_dbg("HW Power off: 0x%lx\n", priv->state);

	spin_lock(&priv->on_off_lock);
	if (!test_bit(ICNSS_POWER_ON, &priv->state)) {
		spin_unlock(&priv->on_off_lock);
		return ret;
	}
	clear_bit(ICNSS_POWER_ON, &priv->state);
	spin_unlock(&priv->on_off_lock);

	icnss_clk_off(&priv->clk_list);

	ret = icnss_vreg_off(priv);

	if (priv->pon_gpio_control)
		ret = icnss_power_trigger_pinctrl(&priv->pdev->dev,
						  ICNSS_PINCTRL_OWNER_WLAN,
						  ICNSS_PINCTRL_SEQ_OFF);

	return ret;
}

int icnss_power_on(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK, data %pK\n",
			     dev, priv);
		return -EINVAL;
	}

	icnss_pr_dbg("Power On: 0x%lx\n", priv->state);

	return icnss_hw_power_on(priv);
}
EXPORT_SYMBOL(icnss_power_on);

int icnss_power_off(struct device *dev)
{
	struct icnss_priv *priv = dev_get_drvdata(dev);

	if (!priv) {
		icnss_pr_err("Invalid drvdata: dev %pK, data %pK\n",
			     dev, priv);
		return -EINVAL;
	}

	icnss_pr_dbg("Power Off: 0x%lx\n", priv->state);

	return icnss_hw_power_off(priv);
}
EXPORT_SYMBOL(icnss_power_off);

void icnss_put_resources(struct icnss_priv *priv)
{
	icnss_put_clk(priv);
	icnss_put_vreg(priv);
}

static int icnss_get_phone_power(struct icnss_priv *priv, uint64_t *result_uv)
{
	int ret = 0;
	int result;

	if (!priv->channel) {
		icnss_pr_err("Channel doesn't exists\n");
		ret = -EINVAL;
		goto out;
	}

	ret = iio_read_channel_processed(priv->channel, &result);
	if (ret < 0) {
		icnss_pr_err("Error reading channel, ret = %d\n", ret);
		goto out;
	}

	*result_uv = (uint64_t)result;
out:
	return ret;
}

static void icnss_vph_notify(enum adc_tm_state state, void *ctx)
{
	struct icnss_priv *priv = ctx;
	u64 vph_pwr = 0;
	u64 vph_pwr_prev;
	int ret = 0;
	bool update = true;

	if (!priv) {
		icnss_pr_err("Priv pointer is NULL\n");
		return;
	}

	vph_pwr_prev = priv->vph_pwr;

	ret = icnss_get_phone_power(priv, &vph_pwr);
	if (ret < 0)
		return;

	if (vph_pwr < ICNSS_THRESHOLD_LOW) {
		if (vph_pwr_prev < ICNSS_THRESHOLD_LOW)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_HIGH_THR_ENABLE;
		priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_LOW +
			ICNSS_THRESHOLD_GUARD;
		priv->vph_monitor_params.low_thr = 0;
	} else if (vph_pwr > ICNSS_THRESHOLD_HIGH) {
		if (vph_pwr_prev > ICNSS_THRESHOLD_HIGH)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_LOW_THR_ENABLE;
		priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_HIGH -
			ICNSS_THRESHOLD_GUARD;
		priv->vph_monitor_params.high_thr = 0;
	} else {
		if (vph_pwr_prev > ICNSS_THRESHOLD_LOW &&
		    vph_pwr_prev < ICNSS_THRESHOLD_HIGH)
			update = false;
		priv->vph_monitor_params.state_request =
			ADC_TM_HIGH_LOW_THR_ENABLE;
		priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_LOW;
		priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_HIGH;
	}

	priv->vph_pwr = vph_pwr;

	if (update) {
		icnss_send_vbatt_update(priv, vph_pwr);
		icnss_pr_dbg("set low threshold to %d, high threshold to %d Phone power=%llu\n",
			     priv->vph_monitor_params.low_thr,
			     priv->vph_monitor_params.high_thr, vph_pwr);
	}

	ret = adc_tm_channel_measure(priv->adc_tm_dev,
				      &priv->vph_monitor_params);
	if (ret)
		icnss_pr_err("TM channel setup failed %d\n", ret);
}

static int icnss_setup_vph_monitor(struct icnss_priv *priv)
{
	int ret = 0;

	if (!priv->adc_tm_dev) {
		icnss_pr_err("ADC TM handler is NULL\n");
		ret = -EINVAL;
		goto out;
	}

	priv->vph_monitor_params.low_thr = ICNSS_THRESHOLD_LOW;
	priv->vph_monitor_params.high_thr = ICNSS_THRESHOLD_HIGH;
	priv->vph_monitor_params.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;
	priv->vph_monitor_params.channel = ADC5_VBAT_SNS;
	priv->vph_monitor_params.btm_ctx = priv;
	priv->vph_monitor_params.threshold_notification = &icnss_vph_notify;
	icnss_pr_dbg("Set low threshold to %d, high threshold to %d\n",
		     priv->vph_monitor_params.low_thr,
		     priv->vph_monitor_params.high_thr);

	ret = adc_tm_channel_measure(priv->adc_tm_dev,
				      &priv->vph_monitor_params);
	if (ret)
		icnss_pr_err("TM channel setup failed %d\n", ret);
out:
	return ret;
}

int icnss_init_vph_monitor(struct icnss_priv *priv)
{
	int ret = 0;

	ret = icnss_get_phone_power(priv, &priv->vph_pwr);
	if (ret < 0)
		goto out;

	icnss_pr_dbg("Phone power=%llu\n", priv->vph_pwr);

	icnss_send_vbatt_update(priv, priv->vph_pwr);

	ret = icnss_setup_vph_monitor(priv);
	if (ret)
		goto out;
out:
	return ret;
}

int icnss_aop_mbox_init(struct icnss_priv *priv)
{
	struct mbox_client *mbox = &priv->mbox_client_data;
	struct mbox_chan *chan;
	int ret = 0;

	ret = of_property_read_string(priv->pdev->dev.of_node,
				      "qcom,vreg_ol_cpr",
				      &priv->cpr_info.vreg_ol_cpr);
	if (ret) {
		icnss_pr_dbg("Vreg for OL CPR not configured\n");
		return -EINVAL;
	}

	mbox->dev = &priv->pdev->dev;
	mbox->tx_block = true;
	mbox->tx_tout = ICNSS_MBOX_TIMEOUT_MS;
	mbox->knows_txdone = false;

	priv->mbox_chan = NULL;
	chan = mbox_request_channel(mbox, 0);
	if (IS_ERR(chan)) {
		ret = PTR_ERR(chan);
		icnss_pr_err("Failed to get mbox channel with err %d\n", ret);
		return ret;
	}
	priv->mbox_chan = chan;

	icnss_pr_dbg("Mbox channel initialized\n");
	return 0;
}

#if IS_ENABLED(CONFIG_MSM_QMP)
static int icnss_aop_set_vreg_param(struct icnss_priv *priv,
				    const char *vreg_name,
				    enum icnss_vreg_param param,
				    enum icnss_tcs_seq seq, int val)
{
	struct qmp_pkt pkt;
	char mbox_msg[ICNSS_MBOX_MSG_MAX_LEN];
	static const char * const vreg_param_str[] = {"v", "m", "e"};
	static const char *const tcs_seq_str[] = {"upval", "dwnval", "enable"};
	int ret = 0;

	if (param > ICNSS_VREG_ENABLE || seq > ICNSS_TCS_ALL_SEQ || !vreg_name)
		return -EINVAL;

	snprintf(mbox_msg, ICNSS_MBOX_MSG_MAX_LEN,
		 "{class: wlan_pdc, res: %s.%s, %s: %d}", vreg_name,
		 vreg_param_str[param], tcs_seq_str[seq], val);

	icnss_pr_dbg("Sending AOP Mbox msg: %s\n", mbox_msg);
	pkt.size = ICNSS_MBOX_MSG_MAX_LEN;
	pkt.data = mbox_msg;

	ret = mbox_send_message(priv->mbox_chan, &pkt);
	if (ret < 0)
		icnss_pr_err("Failed to send AOP mbox msg: %s,ret: %d\n",
			     mbox_msg, ret);
	else
		ret = 0;

	return ret;
}
#else
static int icnss_aop_set_vreg_param(struct icnss_priv *priv,
				    const char *vreg_name,
				    enum icnss_vreg_param param,
				    enum icnss_tcs_seq seq, int val)
{
	return 0;
}
#endif

int icnss_update_cpr_info(struct icnss_priv *priv)
{
	struct icnss_cpr_info *cpr_info = &priv->cpr_info;

	if (!cpr_info->vreg_ol_cpr || !priv->mbox_chan) {
		icnss_pr_dbg("Mbox channel / OL CPR Vreg not configured\n");
		return 0;
	}

	if (cpr_info->voltage == 0) {
		icnss_pr_err("Voltage %dmV is not valid\n", cpr_info->voltage);
		return -EINVAL;
	}

	cpr_info->voltage = cpr_info->voltage > BT_CXMX_VOLTAGE_MV ?
		cpr_info->voltage : BT_CXMX_VOLTAGE_MV;

	return icnss_aop_set_vreg_param(priv,
				       cpr_info->vreg_ol_cpr,
				       ICNSS_VREG_VOLTAGE,
				       ICNSS_TCS_UP_SEQ,
				       cpr_info->voltage);
}

static int icnss_power_pinctrl_set(struct icnss_priv *priv,
				   enum icnss_pinctrl_owner owner,
				   enum icnss_pinctrl_seq seq)
{
	int ret = 0;
	struct icnss_pinctrl_info *pinctrl_info;

	if (!priv) {
		icnss_pr_pon_seq("plat_priv is NULL!\n");
		ret = -ENODEV;
		goto out;
	}

	pinctrl_info = &priv->pinctrl_info;
	icnss_pr_pon_seq("icnss: pinctrl seq %u\n", seq);

	switch (seq) {
	case ICNSS_PINCTRL_SEQ_OFF:
		if (!IS_ERR_OR_NULL(pinctrl_info->wlan_poff_en)) {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
						   pinctrl_info->wlan_poff_en);
			if (ret) {
				icnss_pr_pon_seq("state for poff_en err=%d\n",
						 ret);
				goto out;
			}
		} else {
			ret = -ENODEV;
			goto out;
		}
		mdelay(WLAN_PON_DELAY);
		if (!IS_ERR_OR_NULL(pinctrl_info->wlan_poff_dis)) {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
						   pinctrl_info->wlan_poff_dis);
			if (ret) {
				icnss_pr_pon_seq("state for poff_dis err=%d\n",
						 ret);
				goto out;
			}
			priv->pon_pinctrl_owners &= ~BIT(owner);
			priv->pof_pinctrl_owners |= BIT(owner);
		} else {
			ret = -ENODEV;
			goto out;
		}
		break;
	case ICNSS_PINCTRL_SEQ_ON:
		if (!IS_ERR_OR_NULL(pinctrl_info->wlan_pon_en)) {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
						   pinctrl_info->wlan_pon_en);
			if (ret) {
				icnss_pr_pon_seq("state for pon_en, err=%d\n",
						 ret);
				goto out;
			}
		} else {
			ret = -ENODEV;
			goto out;
		}
		mdelay(WLAN_PON_DELAY);
		if (!IS_ERR_OR_NULL(pinctrl_info->wlan_pon_dis)) {
			ret = pinctrl_select_state(pinctrl_info->pinctrl,
						   pinctrl_info->wlan_pon_dis);
			if (ret) {
				icnss_pr_pon_seq("state for wlan_pon_dis err=%d\n",
						 ret);
				goto out;
			}
			priv->pon_pinctrl_owners |= BIT(owner);
			priv->pof_pinctrl_owners &= ~BIT(owner);
		} else {
			ret = -ENODEV;
			goto out;
		}
		break;
	default:
		icnss_pr_pon_seq("Unhandled pinctrl power sequence %u\n", seq);
		ret = -EINVAL;
		break;
	}
out:
	return ret;
}

int icnss_get_pinctrl(struct icnss_priv *priv)
{
	struct icnss_pinctrl_info *pinctrl_info;
	struct device *dev;
	int ret = 0;

	if (!priv)
		return -EINVAL;

	/* Init to - no power ons, and all are powered down */
	dev = &priv->pdev->dev;
	pinctrl_info = &priv->pinctrl_info;

	pinctrl_info->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl_info->pinctrl)) {
		ret = PTR_ERR(pinctrl_info->pinctrl);
		icnss_pr_pon_seq("Failed to get pinctrl, err = %d\n", ret);
		goto out;
	}

	pinctrl_info->wlan_pon_en =
		pinctrl_lookup_state(pinctrl_info->pinctrl,
				     WLAN_PON_EN);
	if (IS_ERR_OR_NULL(pinctrl_info->wlan_pon_en)) {
		ret = PTR_ERR(pinctrl_info->wlan_pon_en);
		icnss_pr_pon_seq("Failed to get pon_en state, err %d\n", ret);
		goto out;
	}

	pinctrl_info->wlan_pon_dis =
		pinctrl_lookup_state(pinctrl_info->pinctrl,
				     WLAN_PON_DIS);
	if (IS_ERR_OR_NULL(pinctrl_info->wlan_pon_dis)) {
		ret = PTR_ERR(pinctrl_info->wlan_pon_dis);
		icnss_pr_pon_seq("Failed to get pon_dis state, err %d\n", ret);
		goto out;
	}

	pinctrl_info->wlan_poff_en =
		pinctrl_lookup_state(pinctrl_info->pinctrl,
				     WLAN_POFF_EN);
	if (IS_ERR_OR_NULL(pinctrl_info->wlan_poff_en)) {
		ret = PTR_ERR(pinctrl_info->wlan_poff_en);
		icnss_pr_pon_seq("Failed to get poff_en state, err %d\n", ret);
		goto out;
	}

	pinctrl_info->wlan_poff_dis =
		pinctrl_lookup_state(pinctrl_info->pinctrl,
				     WLAN_POFF_DIS);
	if (IS_ERR_OR_NULL(pinctrl_info->wlan_poff_dis)) {
		ret = PTR_ERR(pinctrl_info->wlan_poff_dis);
		icnss_pr_pon_seq("Failed to get poff_dis state, er %d\n", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

int icnss_power_trigger_pinctrl(struct device *dev,
				enum icnss_pinctrl_owner owner,
				enum icnss_pinctrl_seq seq)
{
	struct icnss_priv *priv = icnss_get_plat_priv();
	int retry = 10;
	int ret;
	u32 all_owners = BIT(ICNSS_PINCTRL_OWNER_WLAN) |
			 BIT(ICNSS_PINCTRL_OWNER_BT);

	if (!priv) {
		icnss_pr_pon_seq("icnss2 not initialized");
		return -ENODEV;
	}

	if (!priv->pon_gpio_control) {
		icnss_pr_pon_seq("pinctrl_set: PON pinctrl not present");
		return 0;
	}

	if (seq != ICNSS_PINCTRL_SEQ_ON && seq != ICNSS_PINCTRL_SEQ_OFF) {
		icnss_pr_pon_seq("Invalid power seq %d", seq);
		return -EINVAL;
	}

	icnss_pr_pon_seq("EPower : seq %d, from %d, pon,pof:0x%x, 0x%x",
			 seq, owner, priv->pon_pinctrl_owners,
			 priv->pof_pinctrl_owners);
retry_op:
	/* Don't hold the lock for long, check on pon_in_progress instead */
	spin_lock(&priv->on_off_lock);
	if (priv->pon_in_progress && retry) {
		spin_unlock(&priv->on_off_lock);
		icnss_pr_pon_seq("Wait for operation to complete");
		usleep_range(5000, 10000);
		retry--;
		goto retry_op;
	}

	if (!retry && priv->pon_in_progress) {
		icnss_pr_pon_seq("Prev operation taking too long to complete");
		spin_unlock(&priv->on_off_lock);
		return -EINPROGRESS;
	}
	priv->pon_in_progress = true;
	spin_unlock(&priv->on_off_lock);

	ret = 0;
	/* If PON, and if _this_ owner has not already voted for PON, and none
	 * of the other owners have triggered PON already, only then trigger
	 * PON sequence.
	 */
	if (seq == ICNSS_PINCTRL_SEQ_ON &&
	    (priv->pon_pinctrl_owners & BIT(owner)) == 0) {
		if ((priv->pon_pinctrl_owners & all_owners) == 0) {
			ret = icnss_power_pinctrl_set(priv, owner,
						      ICNSS_PINCTRL_SEQ_ON);
		} else {
			priv->pon_pinctrl_owners |= BIT(owner);
			priv->pof_pinctrl_owners &= ~BIT(owner);
		}
		goto retrn;
	} else if (seq == ICNSS_PINCTRL_SEQ_OFF &&
		   (priv->pof_pinctrl_owners & BIT(owner)) == 0) {
	/* If POF, and if _this_ owner has not already voted for poff, and none
	 * of the other owners require PON any longer, only then trigger
	 * POFF sequence.
	 */
		if ((priv->pon_pinctrl_owners & ~BIT(owner)) == 0) {
			ret = icnss_power_pinctrl_set(priv, owner,
						      ICNSS_PINCTRL_SEQ_OFF);
		} else {
			priv->pon_pinctrl_owners &= ~BIT(owner);
			priv->pof_pinctrl_owners |= BIT(owner);
		}
		goto retrn;
	}

retrn:
	icnss_pr_pon_seq("XPower : seq %d, from %d, pon,pof:0x%x, 0x%x",
			 seq, owner, priv->pon_pinctrl_owners,
			 priv->pof_pinctrl_owners);

	priv->pon_in_progress = false;
	return ret;
}
EXPORT_SYMBOL(icnss_power_trigger_pinctrl);
