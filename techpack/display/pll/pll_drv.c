// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include "pll_drv.h"
#include "dsi_pll.h"
#include "dp_pll.h"
#include "hdmi_pll.h"

int mdss_pll_resource_enable(struct mdss_pll_resources *pll_res, bool enable)
{
	int rc = 0;
	int changed = 0;

	if (!pll_res) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	/*
	 * Don't turn off resources during handoff or add more than
	 * 1 refcount.
	 */
	if (pll_res->handoff_resources &&
		(!enable || (enable & pll_res->resource_enable))) {
		pr_debug("Do not turn on/off pll resources during handoff case\n");
		return rc;
	}

	if (enable) {
		if (pll_res->resource_ref_cnt == 0)
			changed++;
		pll_res->resource_ref_cnt++;
	} else {
		if (pll_res->resource_ref_cnt) {
			pll_res->resource_ref_cnt--;
			if (pll_res->resource_ref_cnt == 0)
				changed++;
		} else {
			pr_err("PLL Resources already OFF\n");
		}
	}

	if (changed) {
		rc = mdss_pll_util_resource_enable(pll_res, enable);
		if (rc)
			pr_err("Resource update failed rc=%d\n", rc);
		else
			pll_res->resource_enable = enable;
	}

	return rc;
}

static int mdss_pll_resource_init(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = 0;
	struct dss_module_power *mp = &pll_res->mp;

	rc = msm_dss_config_vreg(&pdev->dev,
				mp->vreg_config, mp->num_vreg, 1);
	if (rc) {
		pr_err("Vreg config failed rc=%d\n", rc);
		goto vreg_err;
	}

	rc = msm_dss_get_clk(&pdev->dev, mp->clk_config, mp->num_clk);
	if (rc) {
		pr_err("Clock get failed rc=%d\n", rc);
		goto clk_err;
	}

	return rc;

clk_err:
	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);
vreg_err:
	return rc;
}

static void mdss_pll_resource_deinit(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	struct dss_module_power *mp = &pll_res->mp;

	msm_dss_put_clk(mp->clk_config, mp->num_clk);

	msm_dss_config_vreg(&pdev->dev, mp->vreg_config, mp->num_vreg, 0);
}

static void mdss_pll_resource_release(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res)
{
	struct dss_module_power *mp = &pll_res->mp;

	mp->num_vreg = 0;
	mp->num_clk = 0;
}

static int mdss_pll_resource_parse(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = 0;
	const char *compatible_stream;

	rc = mdss_pll_util_resource_parse(pdev, pll_res);
	if (rc) {
		pr_err("Failed to parse the resources rc=%d\n", rc);
		goto end;
	}

	compatible_stream = of_get_property(pdev->dev.of_node,
				"compatible", NULL);
	if (!compatible_stream) {
		pr_err("Failed to parse the compatible stream\n");
		goto err;
	}

	if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_10nm"))
		pll_res->pll_interface_type = MDSS_DSI_PLL_10NM;
	if (!strcmp(compatible_stream, "qcom,mdss_dp_pll_10nm"))
		pll_res->pll_interface_type = MDSS_DP_PLL_10NM;
	else if (!strcmp(compatible_stream, "qcom,mdss_dp_pll_7nm"))
		pll_res->pll_interface_type = MDSS_DP_PLL_7NM;
	else if (!strcmp(compatible_stream, "qcom,mdss_dp_pll_7nm_v2"))
		pll_res->pll_interface_type = MDSS_DP_PLL_7NM_V2;
	else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_7nm"))
		pll_res->pll_interface_type = MDSS_DSI_PLL_7NM;
	else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_7nm_v2"))
		pll_res->pll_interface_type = MDSS_DSI_PLL_7NM_V2;
	else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_7nm_v4_1"))
		pll_res->pll_interface_type = MDSS_DSI_PLL_7NM_V4_1;
	else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_28lpm"))
		pll_res->pll_interface_type = MDSS_DSI_PLL_28LPM;
	else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_14nm"))
		pll_res->pll_interface_type = MDSS_DSI_PLL_14NM;
	else if (!strcmp(compatible_stream, "qcom,mdss_dp_pll_14nm"))
		pll_res->pll_interface_type = MDSS_DP_PLL_14NM;
	else if (!strcmp(compatible_stream, "qcom,mdss_hdmi_pll_28lpm"))
		pll_res->pll_interface_type = MDSS_HDMI_PLL_28LPM;
	else
		goto err;

	return rc;

err:
	mdss_pll_resource_release(pdev, pll_res);
end:
	return rc;
}
static int mdss_pll_clock_register(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc;

	switch (pll_res->pll_interface_type) {
	case MDSS_DSI_PLL_10NM:
		rc = dsi_pll_clock_register_10nm(pdev, pll_res);
		break;
	case MDSS_DP_PLL_10NM:
		rc = dp_pll_clock_register_10nm(pdev, pll_res);
		break;
	case MDSS_DSI_PLL_7NM:
	case MDSS_DSI_PLL_7NM_V2:
	case MDSS_DSI_PLL_7NM_V4_1:
		rc = dsi_pll_clock_register_7nm(pdev, pll_res);
		break;
	case MDSS_DP_PLL_7NM:
	case MDSS_DP_PLL_7NM_V2:
		rc = dp_pll_clock_register_7nm(pdev, pll_res);
		break;
	case MDSS_DSI_PLL_28LPM:
		rc = dsi_pll_clock_register_28lpm(pdev, pll_res);
		break;
	case MDSS_DSI_PLL_14NM:
		rc = dsi_pll_clock_register_14nm(pdev, pll_res);
		break;
	case MDSS_DP_PLL_14NM:
		rc = dp_pll_clock_register_14nm(pdev, pll_res);
		break;
	case MDSS_HDMI_PLL_28LPM:
		rc = hdmi_pll_clock_register_28lpm(pdev, pll_res);
		break;
	case MDSS_UNKNOWN_PLL:
	default:
		rc = -EINVAL;
		break;
	}

	if (rc)
		pr_err("Pll ndx=%d clock register failed rc=%d\n",
				pll_res->index, rc);

	return rc;
}

static inline int mdss_pll_get_ioresurces(struct platform_device *pdev,
				void __iomem **regmap, char *resource_name)
{
	int rc = 0;
	struct resource *rsc = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, resource_name);
	if (rsc) {
		if (!regmap)
			return -ENOMEM;

		*regmap = devm_ioremap(&pdev->dev,
					rsc->start, resource_size(rsc));
		if (!*regmap)
			return -ENOMEM;
	}
	return rc;
}

static int mdss_pll_probe(struct platform_device *pdev)
{
	int rc = 0;
	const char *label;
	struct mdss_pll_resources *pll_res;

	if (!pdev->dev.of_node) {
		pr_err("MDSS pll driver only supports device tree probe\n");
		return -ENOTSUPP;
	}

	label = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!label)
		pr_info("MDSS pll label not specified\n");
	else
		pr_info("MDSS pll label = %s\n", label);

	pll_res = devm_kzalloc(&pdev->dev, sizeof(struct mdss_pll_resources),
								GFP_KERNEL);
	if (!pll_res)
		return -ENOMEM;

	platform_set_drvdata(pdev, pll_res);

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index",
			&pll_res->index);
	if (rc) {
		pr_err("Unable to get the cell-index rc=%d\n", rc);
		pll_res->index = 0;
	}

	pll_res->ssc_en = of_property_read_bool(pdev->dev.of_node,
						"qcom,dsi-pll-ssc-en");

	if (pll_res->ssc_en) {
		pr_info("%s: label=%s PLL SSC enabled\n", __func__, label);

		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ssc-frequency-hz", &pll_res->ssc_freq);

		rc = of_property_read_u32(pdev->dev.of_node,
			"qcom,ssc-ppm", &pll_res->ssc_ppm);

		pll_res->ssc_center = false;

		label = of_get_property(pdev->dev.of_node,
			"qcom,dsi-pll-ssc-mode", NULL);

		if (label && !strcmp(label, "center-spread"))
			pll_res->ssc_center = true;
	}


	if (mdss_pll_get_ioresurces(pdev, &pll_res->pll_base, "pll_base")) {
		pr_err("Unable to remap pll base resources\n");
		return -ENOMEM;
	}

	pr_debug("%s: ndx=%d base=%p\n", __func__,
			pll_res->index, pll_res->pll_base);

	rc = mdss_pll_resource_parse(pdev, pll_res);
	if (rc) {
		pr_err("Pll resource parsing from dt failed rc=%d\n", rc);
		return rc;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->phy_base, "phy_base")) {
		pr_err("Unable to remap pll phy base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->dyn_pll_base,
							"dynamic_pll_base")) {
		pr_err("Unable to remap dynamic pll base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->ln_tx0_base,
							"ln_tx0_base")) {
		pr_err("Unable to remap Lane TX0 base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->ln_tx0_tran_base,
							"ln_tx0_tran_base")) {
		pr_err("Unable to remap Lane TX0 base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->ln_tx0_vmode_base,
							"ln_tx0_vmode_base")) {
		pr_err("Unable to remap Lane TX0 base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->ln_tx1_base,
							"ln_tx1_base")) {
		pr_err("Unable to remap Lane TX1 base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->ln_tx1_tran_base,
							"ln_tx1_tran_base")) {
		pr_err("Unable to remap Lane TX1 base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->ln_tx1_vmode_base,
							"ln_tx1_vmode_base")) {
		pr_err("Unable to remap Lane TX1 base resources\n");
		return -ENOMEM;
	}

	if (mdss_pll_get_ioresurces(pdev, &pll_res->gdsc_base, "gdsc_base")) {
		pr_err("Unable to remap gdsc base resources\n");
		return -ENOMEM;
	}

	rc = mdss_pll_resource_init(pdev, pll_res);
	if (rc) {
		pr_err("Pll ndx=%d resource init failed rc=%d\n",
				pll_res->index, rc);
		return rc;
	}

	rc = mdss_pll_clock_register(pdev, pll_res);
	if (rc) {
		pr_err("Pll ndx=%d clock register failed rc=%d\n",
			pll_res->index, rc);
		goto clock_register_error;
	}

	return rc;

clock_register_error:
	mdss_pll_resource_deinit(pdev, pll_res);
	return rc;
}

static int mdss_pll_remove(struct platform_device *pdev)
{
	struct mdss_pll_resources *pll_res;

	pll_res = platform_get_drvdata(pdev);
	if (!pll_res) {
		pr_err("Invalid PLL resource data\n");
		return 0;
	}

	mdss_pll_resource_deinit(pdev, pll_res);
	mdss_pll_resource_release(pdev, pll_res);
	return 0;
}

static const struct of_device_id mdss_pll_dt_match[] = {
	{.compatible = "qcom,mdss_dsi_pll_10nm"},
	{.compatible = "qcom,mdss_dp_pll_10nm"},
	{.compatible = "qcom,mdss_dsi_pll_7nm"},
	{.compatible = "qcom,mdss_dsi_pll_7nm_v2"},
	{.compatible = "qcom,mdss_dsi_pll_7nm_v4_1"},
	{.compatible = "qcom,mdss_dp_pll_7nm"},
	{.compatible = "qcom,mdss_dp_pll_7nm_v2"},
	{.compatible = "qcom,mdss_dsi_pll_28lpm"},
	{.compatible = "qcom,mdss_dsi_pll_14nm"},
	{.compatible = "qcom,mdss_dp_pll_14nm"},
	{},
};

MODULE_DEVICE_TABLE(of, mdss_clock_dt_match);

static struct platform_driver mdss_pll_driver = {
	.probe = mdss_pll_probe,
	.remove = mdss_pll_remove,
	.driver = {
		.name = "mdss_pll",
		.of_match_table = mdss_pll_dt_match,
	},
};

static int __init mdss_pll_driver_init(void)
{
	int rc;

	rc = platform_driver_register(&mdss_pll_driver);
	if (rc)
		pr_err("mdss_register_pll_driver() failed!\n");

	return rc;
}
fs_initcall(mdss_pll_driver_init);

static void __exit mdss_pll_driver_deinit(void)
{
	platform_driver_unregister(&mdss_pll_driver);
}
module_exit(mdss_pll_driver_deinit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mdss pll driver");
