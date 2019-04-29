/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"msm-dsi-phy:[%s] " fmt, __func__

#include <linux/of_device.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/msm-bus.h>
#include <linux/list.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_gpu.h"
#include "dsi_phy.h"
#include "dsi_phy_hw.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "dsi_catalog.h"

#include "sde_dbg.h"

#define DSI_PHY_DEFAULT_LABEL "MDSS PHY CTRL"

#define BITS_PER_BYTE	8

struct dsi_phy_list_item {
	struct msm_dsi_phy *phy;
	struct list_head list;
};

static LIST_HEAD(dsi_phy_list);
static DEFINE_MUTEX(dsi_phy_list_lock);

static const struct dsi_ver_spec_info dsi_phy_v0_0_hpm = {
	.version = DSI_PHY_VERSION_0_0_HPM,
	.lane_cfg_count = 4,
	.strength_cfg_count = 2,
	.regulator_cfg_count = 1,
	.timing_cfg_count = 8,
};
static const struct dsi_ver_spec_info dsi_phy_v0_0_lpm = {
	.version = DSI_PHY_VERSION_0_0_LPM,
	.lane_cfg_count = 4,
	.strength_cfg_count = 2,
	.regulator_cfg_count = 1,
	.timing_cfg_count = 8,
};
static const struct dsi_ver_spec_info dsi_phy_v1_0 = {
	.version = DSI_PHY_VERSION_1_0,
	.lane_cfg_count = 4,
	.strength_cfg_count = 2,
	.regulator_cfg_count = 1,
	.timing_cfg_count = 8,
};
static const struct dsi_ver_spec_info dsi_phy_v2_0 = {
	.version = DSI_PHY_VERSION_2_0,
	.lane_cfg_count = 4,
	.strength_cfg_count = 2,
	.regulator_cfg_count = 1,
	.timing_cfg_count = 8,
};
static const struct dsi_ver_spec_info dsi_phy_v3_0 = {
	.version = DSI_PHY_VERSION_3_0,
	.lane_cfg_count = 4,
	.strength_cfg_count = 2,
	.regulator_cfg_count = 0,
	.timing_cfg_count = 12,
};

static const struct of_device_id msm_dsi_phy_of_match[] = {
	{ .compatible = "qcom,dsi-phy-v0.0-hpm",
	  .data = &dsi_phy_v0_0_hpm,},
	{ .compatible = "qcom,dsi-phy-v0.0-lpm",
	  .data = &dsi_phy_v0_0_lpm,},
	{ .compatible = "qcom,dsi-phy-v1.0",
	  .data = &dsi_phy_v1_0,},
	{ .compatible = "qcom,dsi-phy-v2.0",
	  .data = &dsi_phy_v2_0,},
	{ .compatible = "qcom,dsi-phy-v3.0",
	  .data = &dsi_phy_v3_0,},
	{}
};

static int dsi_phy_regmap_init(struct platform_device *pdev,
			       struct msm_dsi_phy *phy)
{
	int rc = 0;
	void __iomem *ptr;

	ptr = msm_ioremap(pdev, "dsi_phy", phy->name);
	if (IS_ERR(ptr)) {
		rc = PTR_ERR(ptr);
		return rc;
	}

	phy->hw.base = ptr;

	ptr = msm_ioremap(pdev, "dyn_refresh_base", phy->name);
	phy->hw.dyn_pll_base = ptr;

	pr_debug("[%s] map dsi_phy registers to %pK\n",
		phy->name, phy->hw.base);

	return rc;
}

static int dsi_phy_regmap_deinit(struct msm_dsi_phy *phy)
{
	pr_debug("[%s] unmap registers\n", phy->name);
	return 0;
}

static int dsi_phy_supplies_init(struct platform_device *pdev,
				 struct msm_dsi_phy *phy)
{
	int rc = 0;
	int i = 0;
	struct dsi_regulator_info *regs;
	struct regulator *vreg = NULL;

	regs = &phy->pwr_info.digital;
	regs->vregs = devm_kzalloc(&pdev->dev, sizeof(struct dsi_vreg),
				   GFP_KERNEL);
	if (!regs->vregs)
		goto error;

	regs->count = 1;
	snprintf(regs->vregs->vreg_name,
		 ARRAY_SIZE(regs->vregs[i].vreg_name),
		 "%s", "gdsc");

	rc = dsi_pwr_get_dt_vreg_data(&pdev->dev,
					  &phy->pwr_info.phy_pwr,
					  "qcom,phy-supply-entries");
	if (rc) {
		pr_err("failed to get host power supplies, rc = %d\n", rc);
		goto error_digital;
	}

	regs = &phy->pwr_info.digital;
	for (i = 0; i < regs->count; i++) {
		vreg = devm_regulator_get(&pdev->dev, regs->vregs[i].vreg_name);
		rc = PTR_RET(vreg);
		if (rc) {
			pr_err("failed to get %s regulator\n",
			       regs->vregs[i].vreg_name);
			goto error_host_pwr;
		}
		regs->vregs[i].vreg = vreg;
	}

	regs = &phy->pwr_info.phy_pwr;
	for (i = 0; i < regs->count; i++) {
		vreg = devm_regulator_get(&pdev->dev, regs->vregs[i].vreg_name);
		rc = PTR_RET(vreg);
		if (rc) {
			pr_err("failed to get %s regulator\n",
			       regs->vregs[i].vreg_name);
			for (--i; i >= 0; i--)
				devm_regulator_put(regs->vregs[i].vreg);
			goto error_digital_put;
		}
		regs->vregs[i].vreg = vreg;
	}

	return rc;

error_digital_put:
	regs = &phy->pwr_info.digital;
	for (i = 0; i < regs->count; i++)
		devm_regulator_put(regs->vregs[i].vreg);
error_host_pwr:
	devm_kfree(&pdev->dev, phy->pwr_info.phy_pwr.vregs);
	phy->pwr_info.phy_pwr.vregs = NULL;
	phy->pwr_info.phy_pwr.count = 0;
error_digital:
	devm_kfree(&pdev->dev, phy->pwr_info.digital.vregs);
	phy->pwr_info.digital.vregs = NULL;
	phy->pwr_info.digital.count = 0;
error:
	return rc;
}

static int dsi_phy_supplies_deinit(struct msm_dsi_phy *phy)
{
	int i = 0;
	int rc = 0;
	struct dsi_regulator_info *regs;

	regs = &phy->pwr_info.digital;
	for (i = 0; i < regs->count; i++) {
		if (!regs->vregs[i].vreg)
			pr_err("vreg is NULL, should not reach here\n");
		else
			devm_regulator_put(regs->vregs[i].vreg);
	}

	regs = &phy->pwr_info.phy_pwr;
	for (i = 0; i < regs->count; i++) {
		if (!regs->vregs[i].vreg)
			pr_err("vreg is NULL, should not reach here\n");
		else
			devm_regulator_put(regs->vregs[i].vreg);
	}

	if (phy->pwr_info.phy_pwr.vregs) {
		devm_kfree(&phy->pdev->dev, phy->pwr_info.phy_pwr.vregs);
		phy->pwr_info.phy_pwr.vregs = NULL;
		phy->pwr_info.phy_pwr.count = 0;
	}
	if (phy->pwr_info.digital.vregs) {
		devm_kfree(&phy->pdev->dev, phy->pwr_info.digital.vregs);
		phy->pwr_info.digital.vregs = NULL;
		phy->pwr_info.digital.count = 0;
	}

	return rc;
}

static int dsi_phy_parse_dt_per_lane_cfgs(struct platform_device *pdev,
					  struct dsi_phy_per_lane_cfgs *cfg,
					  char *property)
{
	int rc = 0, i = 0, j = 0;
	const u8 *data;
	u32 len = 0;

	data = of_get_property(pdev->dev.of_node, property, &len);
	if (!data) {
		pr_err("Unable to read Phy %s settings\n", property);
		return -EINVAL;
	}

	if (len != DSI_LANE_MAX * cfg->count_per_lane) {
		pr_err("incorrect phy %s settings, exp=%d, act=%d\n",
		       property, (DSI_LANE_MAX * cfg->count_per_lane), len);
		return -EINVAL;
	}

	for (i = DSI_LOGICAL_LANE_0; i < DSI_LANE_MAX; i++) {
		for (j = 0; j < cfg->count_per_lane; j++) {
			cfg->lane[i][j] = *data;
			data++;
		}
	}

	return rc;
}

static int dsi_phy_settings_init(struct platform_device *pdev,
				 struct msm_dsi_phy *phy)
{
	int rc = 0;
	struct dsi_phy_per_lane_cfgs *lane = &phy->cfg.lanecfg;
	struct dsi_phy_per_lane_cfgs *strength = &phy->cfg.strength;
	struct dsi_phy_per_lane_cfgs *timing = &phy->cfg.timing;
	struct dsi_phy_per_lane_cfgs *regs = &phy->cfg.regulators;

	lane->count_per_lane = phy->ver_info->lane_cfg_count;
	rc = dsi_phy_parse_dt_per_lane_cfgs(pdev, lane,
					    "qcom,platform-lane-config");
	if (rc) {
		pr_err("failed to parse lane cfgs, rc=%d\n", rc);
		goto err;
	}

	strength->count_per_lane = phy->ver_info->strength_cfg_count;
	rc = dsi_phy_parse_dt_per_lane_cfgs(pdev, strength,
					    "qcom,platform-strength-ctrl");
	if (rc) {
		pr_err("failed to parse lane cfgs, rc=%d\n", rc);
		goto err;
	}

	regs->count_per_lane = phy->ver_info->regulator_cfg_count;
	if (regs->count_per_lane > 0) {
	rc = dsi_phy_parse_dt_per_lane_cfgs(pdev, regs,
					    "qcom,platform-regulator-settings");
		if (rc) {
			pr_err("failed to parse lane cfgs, rc=%d\n", rc);
			goto err;
		}
	}

	/* Actual timing values are dependent on panel */
	timing->count_per_lane = phy->ver_info->timing_cfg_count;

	phy->allow_phy_power_off = of_property_read_bool(pdev->dev.of_node,
			"qcom,panel-allow-phy-poweroff");

	of_property_read_u32(pdev->dev.of_node,
			"qcom,dsi-phy-regulator-min-datarate-bps",
			&phy->regulator_min_datarate_bps);

	return 0;
err:
	lane->count_per_lane = 0;
	strength->count_per_lane = 0;
	regs->count_per_lane = 0;
	timing->count_per_lane = 0;
	return rc;
}

static int dsi_phy_settings_deinit(struct msm_dsi_phy *phy)
{
	memset(&phy->cfg.lanecfg, 0x0, sizeof(phy->cfg.lanecfg));
	memset(&phy->cfg.strength, 0x0, sizeof(phy->cfg.strength));
	memset(&phy->cfg.timing, 0x0, sizeof(phy->cfg.timing));
	memset(&phy->cfg.regulators, 0x0, sizeof(phy->cfg.regulators));
	return 0;
}

static int dsi_phy_driver_probe(struct platform_device *pdev)
{
	struct msm_dsi_phy *dsi_phy;
	struct dsi_phy_list_item *item;
	const struct of_device_id *id;
	const struct dsi_ver_spec_info *ver_info;
	int rc = 0;
	u32 index = 0;

	if (!pdev || !pdev->dev.of_node) {
		pr_err("pdev not found\n");
		return -ENODEV;
	}

	id = of_match_node(msm_dsi_phy_of_match, pdev->dev.of_node);
	if (!id)
		return -ENODEV;

	ver_info = id->data;

	item = devm_kzalloc(&pdev->dev, sizeof(*item), GFP_KERNEL);
	if (!item)
		return -ENOMEM;


	dsi_phy = devm_kzalloc(&pdev->dev, sizeof(*dsi_phy), GFP_KERNEL);
	if (!dsi_phy) {
		devm_kfree(&pdev->dev, item);
		return -ENOMEM;
	}

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index", &index);
	if (rc) {
		pr_debug("cell index not set, default to 0\n");
		index = 0;
	}

	dsi_phy->index = index;

	dsi_phy->name = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!dsi_phy->name)
		dsi_phy->name = DSI_PHY_DEFAULT_LABEL;

	pr_debug("Probing %s device\n", dsi_phy->name);

	rc = dsi_phy_regmap_init(pdev, dsi_phy);
	if (rc) {
		pr_err("Failed to parse register information, rc=%d\n", rc);
		goto fail;
	}

	rc = dsi_phy_supplies_init(pdev, dsi_phy);
	if (rc) {
		pr_err("failed to parse voltage supplies, rc = %d\n", rc);
		goto fail_regmap;
	}

	rc = dsi_catalog_phy_setup(&dsi_phy->hw, ver_info->version,
				   dsi_phy->index);
	if (rc) {
		pr_err("Catalog does not support version (%d)\n",
		       ver_info->version);
		goto fail_supplies;
	}

	dsi_phy->ver_info = ver_info;
	rc = dsi_phy_settings_init(pdev, dsi_phy);
	if (rc) {
		pr_err("Failed to parse phy setting, rc=%d\n", rc);
		goto fail_supplies;
	}

	item->phy = dsi_phy;

	mutex_lock(&dsi_phy_list_lock);
	list_add(&item->list, &dsi_phy_list);
	mutex_unlock(&dsi_phy_list_lock);

	mutex_init(&dsi_phy->phy_lock);
	/** TODO: initialize debugfs */
	dsi_phy->pdev = pdev;
	platform_set_drvdata(pdev, dsi_phy);
	pr_info("Probe successful for %s\n", dsi_phy->name);
	return 0;

fail_supplies:
	(void)dsi_phy_supplies_deinit(dsi_phy);
fail_regmap:
	(void)dsi_phy_regmap_deinit(dsi_phy);
fail:
	devm_kfree(&pdev->dev, dsi_phy);
	devm_kfree(&pdev->dev, item);
	return rc;
}

static int dsi_phy_driver_remove(struct platform_device *pdev)
{
	int rc = 0;
	struct msm_dsi_phy *phy = platform_get_drvdata(pdev);
	struct list_head *pos, *tmp;

	if (!pdev || !phy) {
		pr_err("Invalid device\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_phy_list_lock);
	list_for_each_safe(pos, tmp, &dsi_phy_list) {
		struct dsi_phy_list_item *n;

		n = list_entry(pos, struct dsi_phy_list_item, list);
		if (n->phy == phy) {
			list_del(&n->list);
			devm_kfree(&pdev->dev, n);
			break;
		}
	}
	mutex_unlock(&dsi_phy_list_lock);

	mutex_lock(&phy->phy_lock);
	rc = dsi_phy_settings_deinit(phy);
	if (rc)
		pr_err("failed to deinitialize phy settings, rc=%d\n", rc);

	rc = dsi_phy_supplies_deinit(phy);
	if (rc)
		pr_err("failed to deinitialize voltage supplies, rc=%d\n", rc);

	rc = dsi_phy_regmap_deinit(phy);
	if (rc)
		pr_err("failed to deinitialize regmap, rc=%d\n", rc);
	mutex_unlock(&phy->phy_lock);

	mutex_destroy(&phy->phy_lock);
	devm_kfree(&pdev->dev, phy);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver dsi_phy_platform_driver = {
	.probe      = dsi_phy_driver_probe,
	.remove     = dsi_phy_driver_remove,
	.driver     = {
		.name   = "dsi_phy",
		.of_match_table = msm_dsi_phy_of_match,
	},
};

static void dsi_phy_enable_hw(struct msm_dsi_phy *phy)
{
	if (phy->hw.ops.regulator_enable)
		phy->hw.ops.regulator_enable(&phy->hw, &phy->cfg.regulators);

	if (phy->hw.ops.enable)
		phy->hw.ops.enable(&phy->hw, &phy->cfg);
}

static void dsi_phy_disable_hw(struct msm_dsi_phy *phy)
{
	if (phy->hw.ops.disable)
		phy->hw.ops.disable(&phy->hw, &phy->cfg);

	if (phy->hw.ops.regulator_disable)
		phy->hw.ops.regulator_disable(&phy->hw);
}

/**
 * dsi_phy_get() - get a dsi phy handle from device node
 * @of_node:           device node for dsi phy controller
 *
 * Gets the DSI PHY handle for the corresponding of_node. The ref count is
 * incremented to one all subsequents get will fail until the original client
 * calls a put.
 *
 * Return: DSI PHY handle or an error code.
 */
struct msm_dsi_phy *dsi_phy_get(struct device_node *of_node)
{
	struct list_head *pos, *tmp;
	struct msm_dsi_phy *phy = NULL;

	mutex_lock(&dsi_phy_list_lock);
	list_for_each_safe(pos, tmp, &dsi_phy_list) {
		struct dsi_phy_list_item *n;

		n = list_entry(pos, struct dsi_phy_list_item, list);
		if (n->phy->pdev->dev.of_node == of_node) {
			phy = n->phy;
			break;
		}
	}
	mutex_unlock(&dsi_phy_list_lock);

	if (!phy) {
		pr_err("Device with of node not found\n");
		phy = ERR_PTR(-EPROBE_DEFER);
		return phy;
	}

	mutex_lock(&phy->phy_lock);
	if (phy->refcount > 0) {
		pr_err("[PHY_%d] Device under use\n", phy->index);
		phy = ERR_PTR(-EINVAL);
	} else {
		phy->refcount++;
	}
	mutex_unlock(&phy->phy_lock);
	return phy;
}

/**
 * dsi_phy_put() - release dsi phy handle
 * @dsi_phy:              DSI PHY handle.
 *
 * Release the DSI PHY hardware. Driver will clean up all resources and puts
 * back the DSI PHY into reset state.
 */
void dsi_phy_put(struct msm_dsi_phy *dsi_phy)
{
	mutex_lock(&dsi_phy->phy_lock);

	if (dsi_phy->refcount == 0)
		pr_err("Unbalanced dsi_phy_put call\n");
	else
		dsi_phy->refcount--;

	mutex_unlock(&dsi_phy->phy_lock);
}

/**
 * dsi_phy_drv_init() - initialize dsi phy driver
 * @dsi_phy:         DSI PHY handle.
 *
 * Initializes DSI PHY driver. Should be called after dsi_phy_get().
 *
 * Return: error code.
 */
int dsi_phy_drv_init(struct msm_dsi_phy *dsi_phy)
{
	char dbg_name[DSI_DEBUG_NAME_LEN];

	snprintf(dbg_name, DSI_DEBUG_NAME_LEN, "dsi%d_phy", dsi_phy->index);
	sde_dbg_reg_register_base(dbg_name, dsi_phy->hw.base,
				msm_iomap_size(dsi_phy->pdev, "dsi_phy"));
	sde_dbg_reg_register_dump_range(dbg_name, dbg_name, 0,
				msm_iomap_size(dsi_phy->pdev, "dsi_phy"), 0);
	return 0;
}

/**
 * dsi_phy_drv_deinit() - de-initialize dsi phy driver
 * @dsi_phy:          DSI PHY handle.
 *
 * Release all resources acquired by dsi_phy_drv_init().
 *
 * Return: error code.
 */
int dsi_phy_drv_deinit(struct msm_dsi_phy *dsi_phy)
{
	return 0;
}

int dsi_phy_clk_cb_register(struct msm_dsi_phy *dsi_phy,
	struct clk_ctrl_cb *clk_cb)
{
	if (!dsi_phy || !clk_cb) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	dsi_phy->clk_cb.priv = clk_cb->priv;
	dsi_phy->clk_cb.dsi_clk_cb = clk_cb->dsi_clk_cb;
	return 0;
}

/**
 * dsi_phy_validate_mode() - validate a display mode
 * @dsi_phy:            DSI PHY handle.
 * @mode:               Mode information.
 *
 * Validation will fail if the mode cannot be supported by the PHY driver or
 * hardware.
 *
 * Return: error code.
 */
int dsi_phy_validate_mode(struct msm_dsi_phy *dsi_phy,
			  struct dsi_mode_info *mode)
{
	int rc = 0;

	if (!dsi_phy || !mode) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	pr_debug("[PHY_%d] Skipping validation\n", dsi_phy->index);

	return rc;
}

/**
 * dsi_phy_set_power_state() - enable/disable dsi phy power supplies
 * @dsi_phy:               DSI PHY handle.
 * @enable:                Boolean flag to enable/disable.
 *
 * Return: error code.
 */
int dsi_phy_set_power_state(struct msm_dsi_phy *dsi_phy, bool enable)
{
	int rc = 0;

	if (!dsi_phy) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&dsi_phy->phy_lock);

	if (enable == dsi_phy->power_state) {
		pr_err("[PHY_%d] No state change\n", dsi_phy->index);
		goto error;
	}

	if (enable) {
		rc = dsi_pwr_enable_regulator(&dsi_phy->pwr_info.digital, true);
		if (rc) {
			pr_err("failed to enable digital regulator\n");
			goto error;
		}

		if (dsi_phy->dsi_phy_state == DSI_PHY_ENGINE_OFF &&
				dsi_phy->regulator_required) {
			rc = dsi_pwr_enable_regulator(
				&dsi_phy->pwr_info.phy_pwr, true);
			if (rc) {
				pr_err("failed to enable phy power\n");
				(void)dsi_pwr_enable_regulator(
					&dsi_phy->pwr_info.digital, false);
				goto error;
			}
		}
	} else {
		if (dsi_phy->dsi_phy_state == DSI_PHY_ENGINE_OFF &&
				dsi_phy->regulator_required) {
			rc = dsi_pwr_enable_regulator(
				&dsi_phy->pwr_info.phy_pwr, false);
			if (rc) {
				pr_err("failed to enable digital regulator\n");
				goto error;
			}
		}

		rc = dsi_pwr_enable_regulator(&dsi_phy->pwr_info.digital,
					      false);
		if (rc) {
			pr_err("failed to enable phy power\n");
			goto error;
		}
	}

	dsi_phy->power_state = enable;
error:
	mutex_unlock(&dsi_phy->phy_lock);
	return rc;
}

static int dsi_phy_enable_ulps(struct msm_dsi_phy *phy,
		struct dsi_host_config *config, bool clamp_enabled)
{
	int rc = 0;
	u32 lanes = 0;
	u32 ulps_lanes;

	lanes = config->common_config.data_lanes;
	lanes |= DSI_CLOCK_LANE;

	/*
	 * If DSI clamps are enabled, it means that the DSI lanes are
	 * already in idle state. Checking for lanes to be in idle state
	 * should be skipped during ULPS entry programming while coming
	 * out of idle screen.
	 */
	if (!clamp_enabled) {
		rc = phy->hw.ops.ulps_ops.wait_for_lane_idle(&phy->hw, lanes);
		if (rc) {
			pr_err("lanes not entering idle, skip ULPS\n");
			return rc;
		}
	}

	phy->hw.ops.ulps_ops.ulps_request(&phy->hw, &phy->cfg, lanes);

	ulps_lanes = phy->hw.ops.ulps_ops.get_lanes_in_ulps(&phy->hw);

	if (!phy->hw.ops.ulps_ops.is_lanes_in_ulps(lanes, ulps_lanes)) {
		pr_err("Failed to enter ULPS, request=0x%x, actual=0x%x\n",
		       lanes, ulps_lanes);
		rc = -EIO;
	}

	return rc;
}

static int dsi_phy_disable_ulps(struct msm_dsi_phy *phy,
		 struct dsi_host_config *config)
{
	u32 ulps_lanes, lanes = 0;

	lanes = config->common_config.data_lanes;
	lanes |= DSI_CLOCK_LANE;

	ulps_lanes = phy->hw.ops.ulps_ops.get_lanes_in_ulps(&phy->hw);

	if (!phy->hw.ops.ulps_ops.is_lanes_in_ulps(lanes, ulps_lanes)) {
		pr_err("Mismatch in ULPS: lanes:%d, ulps_lanes:%d\n",
				lanes, ulps_lanes);
		return -EIO;
	}

	phy->hw.ops.ulps_ops.ulps_exit(&phy->hw, &phy->cfg, lanes);

	ulps_lanes = phy->hw.ops.ulps_ops.get_lanes_in_ulps(&phy->hw);

	if (phy->hw.ops.ulps_ops.is_lanes_in_ulps(lanes, ulps_lanes)) {
		pr_err("Lanes (0x%x) stuck in ULPS\n", ulps_lanes);
		return -EIO;
	}

	return 0;
}

void dsi_phy_toggle_resync_fifo(struct msm_dsi_phy *phy)
{
	if (!phy)
		return;

	if (!phy->hw.ops.toggle_resync_fifo)
		return;

	phy->hw.ops.toggle_resync_fifo(&phy->hw);
}

int dsi_phy_set_ulps(struct msm_dsi_phy *phy, struct dsi_host_config *config,
		bool enable, bool clamp_enabled)
{
	int rc = 0;

	if (!phy) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (!phy->hw.ops.ulps_ops.ulps_request ||
			!phy->hw.ops.ulps_ops.ulps_exit ||
			!phy->hw.ops.ulps_ops.get_lanes_in_ulps ||
			!phy->hw.ops.ulps_ops.is_lanes_in_ulps ||
			!phy->hw.ops.ulps_ops.wait_for_lane_idle) {
		pr_debug("DSI PHY ULPS ops not present\n");
		return 0;
	}

	mutex_lock(&phy->phy_lock);

	if (enable)
		rc = dsi_phy_enable_ulps(phy, config, clamp_enabled);
	else
		rc = dsi_phy_disable_ulps(phy, config);

	if (rc) {
		pr_err("[DSI_PHY%d] Ulps state change(%d) failed, rc=%d\n",
			phy->index, enable, rc);
		goto error;
	}
	pr_debug("[DSI_PHY%d] ULPS state = %d\n", phy->index, enable);

error:
	mutex_unlock(&phy->phy_lock);
	return rc;
}

/**
 * dsi_phy_enable() - enable DSI PHY hardware
 * @dsi_phy:            DSI PHY handle.
 * @config:             DSI host configuration.
 * @pll_source:         Source PLL for PHY clock.
 * @skip_validation:    Validation will not be performed on parameters.
 * @is_cont_splash_enabled:    check whether continuous splash enabled.
 *
 * Validates and enables DSI PHY.
 *
 * Return: error code.
 */
int dsi_phy_enable(struct msm_dsi_phy *phy,
		   struct dsi_host_config *config,
		   enum dsi_phy_pll_source pll_source,
		   bool skip_validation,
		   bool is_cont_splash_enabled)
{
	int rc = 0;

	if (!phy || !config) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&phy->phy_lock);

	if (!skip_validation)
		pr_debug("[PHY_%d] TODO: perform validation\n", phy->index);

	memcpy(&phy->mode, &config->video_timing, sizeof(phy->mode));
	memcpy(&phy->cfg.lane_map, &config->lane_map, sizeof(config->lane_map));
	phy->data_lanes = config->common_config.data_lanes;
	phy->dst_format = config->common_config.dst_format;
	phy->cfg.pll_source = pll_source;

	/**
	 * If PHY timing parameters are not present in panel dtsi file,
	 * then calculate them in the driver
	 */
	if (!phy->cfg.is_phy_timing_present)
		rc = phy->hw.ops.calculate_timing_params(&phy->hw,
						 &phy->mode,
						 &config->common_config,
						 &phy->cfg.timing, false);
	if (rc) {
		pr_err("[%s] failed to set timing, rc=%d\n", phy->name, rc);
		goto error;
	}

	if (!is_cont_splash_enabled) {
		dsi_phy_enable_hw(phy);
		pr_debug("cont splash not enabled, phy enable required\n");
	}
	phy->dsi_phy_state = DSI_PHY_ENGINE_ON;

error:
	mutex_unlock(&phy->phy_lock);

	return rc;
}

/* update dsi phy timings for dynamic clk switch use case */
int dsi_phy_update_phy_timings(struct msm_dsi_phy *phy,
			       struct dsi_host_config *config)
{
	int rc = 0;

	if (!phy || !config) {
		pr_err("invalid argument\n");
		return -EINVAL;
	}

	memcpy(&phy->mode, &config->video_timing, sizeof(phy->mode));
	rc = phy->hw.ops.calculate_timing_params(&phy->hw, &phy->mode,
						 &config->common_config,
						 &phy->cfg.timing, true);
	if (rc)
		pr_err("failed to calculate phy timings %d\n", rc);
	else
		phy->cfg.is_phy_timing_present = true;

	return rc;
}

int dsi_phy_lane_reset(struct msm_dsi_phy *phy)
{
	int ret = 0;

	if (!phy)
		return ret;

	mutex_lock(&phy->phy_lock);
	if (phy->hw.ops.phy_lane_reset)
		ret = phy->hw.ops.phy_lane_reset(&phy->hw);
	mutex_unlock(&phy->phy_lock);

	return ret;
}

/**
 * dsi_phy_disable() - disable DSI PHY hardware.
 * @phy:        DSI PHY handle.
 *
 * Return: error code.
 */
int dsi_phy_disable(struct msm_dsi_phy *phy)
{
	int rc = 0;

	if (!phy) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	mutex_lock(&phy->phy_lock);
	dsi_phy_disable_hw(phy);
	phy->dsi_phy_state = DSI_PHY_ENGINE_OFF;
	mutex_unlock(&phy->phy_lock);

	return rc;
}

/**
 * dsi_phy_set_clamp_state() - configure clamps for DSI lanes
 * @phy:        DSI PHY handle.
 * @enable:     boolean to specify clamp enable/disable.
 *
 * Return: error code.
 */
int dsi_phy_set_clamp_state(struct msm_dsi_phy *phy, bool enable)
{
	if (!phy)
		return -EINVAL;

	pr_debug("[%s] enable=%d\n", phy->name, enable);

	if (phy->hw.ops.clamp_ctrl)
		phy->hw.ops.clamp_ctrl(&phy->hw, enable);

	return 0;
}

/**
 * dsi_phy_idle_ctrl() - enable/disable DSI PHY during idle screen
 * @phy:          DSI PHY handle
 * @enable:       boolean to specify PHY enable/disable.
 *
 * Return: error code.
 */

int dsi_phy_idle_ctrl(struct msm_dsi_phy *phy, bool enable)
{
	if (!phy) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	pr_debug("[%s] enable=%d\n", phy->name, enable);

	mutex_lock(&phy->phy_lock);
	if (enable) {
		if (phy->hw.ops.phy_idle_on)
			phy->hw.ops.phy_idle_on(&phy->hw, &phy->cfg);

		if (phy->hw.ops.regulator_enable)
			phy->hw.ops.regulator_enable(&phy->hw,
				&phy->cfg.regulators);

		if (phy->hw.ops.enable)
			phy->hw.ops.enable(&phy->hw, &phy->cfg);

		phy->dsi_phy_state = DSI_PHY_ENGINE_ON;
	} else {
		phy->dsi_phy_state = DSI_PHY_ENGINE_OFF;

		if (phy->hw.ops.disable)
			phy->hw.ops.disable(&phy->hw, &phy->cfg);

		if (phy->hw.ops.phy_idle_off)
			phy->hw.ops.phy_idle_off(&phy->hw);
	}
	mutex_unlock(&phy->phy_lock);

	return 0;
}

/**
 * dsi_phy_set_clk_freq() - set DSI PHY clock frequency setting
 * @phy:          DSI PHY handle
 * @clk_freq:     link clock frequency
 *
 * Return: error code.
 */
int dsi_phy_set_clk_freq(struct msm_dsi_phy *phy,
		struct link_clk_freq *clk_freq)
{
	if (!phy || !clk_freq) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	phy->regulator_required = clk_freq->byte_clk_rate >
		(phy->regulator_min_datarate_bps / BITS_PER_BYTE);

	/*
	 * DSI PLL needs 0p9 LDO1A for Powering DSI PLL block.
	 * PLL driver can vote for this regulator in PLL driver file, but for
	 * the usecase where we come out of idle(static screen), if PLL and
	 * PHY vote for regulator ,there will be performance delays as both
	 * votes go through RPM to enable regulators.
	 */
	phy->regulator_required = true;
	pr_debug("[%s] lane_datarate=%u min_datarate=%u required=%d\n",
			phy->name,
			clk_freq->byte_clk_rate * BITS_PER_BYTE,
			phy->regulator_min_datarate_bps,
			phy->regulator_required);

	return 0;
}

/**
 * dsi_phy_set_timing_params() - timing parameters for the panel
 * @phy:          DSI PHY handle
 * @timing:       array holding timing params.
 * @size:         size of the array.
 *
 * When PHY timing calculator is not implemented, this array will be used to
 * pass PHY timing information.
 *
 * Return: error code.
 */
int dsi_phy_set_timing_params(struct msm_dsi_phy *phy,
			      u32 *timing, u32 size)
{
	int rc = 0;

	if (!phy || !timing || !size) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	if (phy->cfg.is_phy_timing_present)
		return rc;

	mutex_lock(&phy->phy_lock);

	if (phy->hw.ops.phy_timing_val)
		rc = phy->hw.ops.phy_timing_val(&phy->cfg.timing, timing, size);
	if (!rc)
		phy->cfg.is_phy_timing_present = true;

	mutex_unlock(&phy->phy_lock);
	return rc;
}

/**
 * dsi_phy_dynamic_refresh_trigger() - trigger dynamic refresh
 * @phy:	DSI PHY handle
 * @is_master:	Boolean to indicate if for master or slave.
 */
void dsi_phy_dynamic_refresh_trigger(struct msm_dsi_phy *phy, bool is_master)
{
	u32 off;

	if (!phy)
		return;

	mutex_lock(&phy->phy_lock);
	/*
	 * program PLL_SWI_INTF_SEL and SW_TRIGGER bit only for
	 * master and program SYNC_MODE bit only for slave.
	 */
	if (is_master)
		off = BIT(DYN_REFRESH_INTF_SEL) | BIT(DYN_REFRESH_SWI_CTRL) |
			BIT(DYN_REFRESH_SW_TRIGGER);
	else
		off = BIT(DYN_REFRESH_SYNC_MODE) | BIT(DYN_REFRESH_SWI_CTRL);

	if (phy->hw.ops.dyn_refresh_ops.dyn_refresh_helper)
		phy->hw.ops.dyn_refresh_ops.dyn_refresh_helper(&phy->hw, off);

	mutex_unlock(&phy->phy_lock);
}

/**
 * dsi_phy_config_dynamic_refresh() - Configure dynamic refresh registers
 * @phy:	DSI PHY handle
 * @delay:	pipe delays for dynamic refresh
 * @is_master:	Boolean to indicate if for master or slave.
 */
void dsi_phy_config_dynamic_refresh(struct msm_dsi_phy *phy,
				    struct dsi_dyn_clk_delay *delay,
				    bool is_master)
{
	struct dsi_phy_cfg *cfg;

	if (!phy)
		return;

	mutex_lock(&phy->phy_lock);

	cfg = &phy->cfg;

	if (phy->hw.ops.dyn_refresh_ops.dyn_refresh_config)
		phy->hw.ops.dyn_refresh_ops.dyn_refresh_config(&phy->hw, cfg,
							       is_master);
	if (phy->hw.ops.dyn_refresh_ops.dyn_refresh_pipe_delay)
		phy->hw.ops.dyn_refresh_ops.dyn_refresh_pipe_delay(
						&phy->hw, delay);

	mutex_unlock(&phy->phy_lock);
}

/**
 * dsi_phy_cache_phy_timings - cache the phy timings calculated as part of
 *				dynamic refresh.
 * @phy:	   DSI PHY Handle.
 * @dst:	   Pointer to cache location.
 * @size:	   Number of phy lane settings.
 */
int dsi_phy_dyn_refresh_cache_phy_timings(struct msm_dsi_phy *phy, u32 *dst,
					  u32 size)
{
	int rc = 0;

	if (!phy || !dst || !size)
		return -EINVAL;

	if (phy->hw.ops.dyn_refresh_ops.cache_phy_timings)
		rc = phy->hw.ops.dyn_refresh_ops.cache_phy_timings(
					   &phy->cfg.timing, dst, size);

	if (rc)
		pr_err("failed to cache phy timings %d\n", rc);

	return rc;
}

/**
 * dsi_phy_dynamic_refresh_clear() - clear dynamic refresh config
 * @phy:	DSI PHY handle
 */
void dsi_phy_dynamic_refresh_clear(struct msm_dsi_phy *phy)
{
	if (!phy)
		return;

	mutex_lock(&phy->phy_lock);

	if (phy->hw.ops.dyn_refresh_ops.dyn_refresh_helper)
		phy->hw.ops.dyn_refresh_ops.dyn_refresh_helper(&phy->hw, 0);

	mutex_unlock(&phy->phy_lock);
}

void dsi_phy_drv_register(void)
{
	platform_driver_register(&dsi_phy_platform_driver);
}

void dsi_phy_drv_unregister(void)
{
	platform_driver_unregister(&dsi_phy_platform_driver);
}
