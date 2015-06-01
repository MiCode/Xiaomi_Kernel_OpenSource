/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <linux/clk/msm-clock-generic.h>

#include "mdss-pll.h"
#include "mdss-edp-pll.h"
#include "mdss-dsi-pll.h"
#include "mdss-hdmi-pll.h"

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
	if (!pdev || !pll_res) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	return mdss_pll_util_resource_init(pdev, pll_res);
}

static void mdss_pll_resource_deinit(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	if (!pdev || !pll_res) {
		pr_err("Invalid input parameters\n");
		return;
	}

	mdss_pll_util_resource_deinit(pdev, pll_res);
}

static void mdss_pll_resource_release(struct platform_device *pdev,
					struct mdss_pll_resources *pll_res)
{
	if (!pdev || !pll_res) {
		pr_err("Invalid input parameters\n");
		return;
	}

	mdss_pll_util_resource_release(pdev, pll_res);
}

static int mdss_pll_resource_parse(struct platform_device *pdev,
				struct mdss_pll_resources *pll_res)
{
	int rc = 0;
	const char *compatible_stream;

	if (!pdev || !pll_res) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

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

	if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8916")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_LPM;
		pll_res->target_id = MDSS_PLL_TARGET_8916;
	} else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8939")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_LPM;
		pll_res->target_id = MDSS_PLL_TARGET_8939;
	} else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8909")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_LPM;
		pll_res->target_id = MDSS_PLL_TARGET_8909;
	} else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8952")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_LPM;
		pll_res->target_id = MDSS_PLL_TARGET_8952;
	} else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8974")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_HPM;
		pll_res->target_id = MDSS_PLL_TARGET_8974;
	} else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8976")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_HPM;
		pll_res->target_id = MDSS_PLL_TARGET_8976;
	} else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8994")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_20NM;
		pll_res->target_id = MDSS_PLL_TARGET_8994;
	} else if (!strcmp(compatible_stream, "qcom,mdss_dsi_pll_8992")) {
		pll_res->pll_interface_type = MDSS_DSI_PLL_20NM;
		pll_res->target_id = MDSS_PLL_TARGET_8992;
	} else if (!strcmp(compatible_stream, "qcom,mdss_edp_pll")) {
		pll_res->pll_interface_type = MDSS_EDP_PLL;
	} else if (!strcmp(compatible_stream, "qcom,mdss_hdmi_pll")) {
		pll_res->pll_interface_type = MDSS_HDMI_PLL;
	} else if (!strcmp(compatible_stream, "qcom,mdss_hdmi_pll_8994")) {
		pll_res->pll_interface_type = MDSS_HDMI_PLL_20NM;
	} else if (!strcmp(compatible_stream, "qcom,mdss_hdmi_pll_8992")) {
		pll_res->pll_interface_type = MDSS_HDMI_PLL_20NM;
	} else {
		goto err;
	}

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

	if (!pdev || !pll_res) {
		pr_err("Invalid input parameters\n");
		return -EINVAL;
	}

	switch (pll_res->pll_interface_type) {
	case MDSS_DSI_PLL_LPM:
		rc = dsi_pll_clock_register_lpm(pdev, pll_res);
		break;
	case MDSS_DSI_PLL_HPM:
		rc = dsi_pll_clock_register_hpm(pdev, pll_res);
		break;
	case MDSS_DSI_PLL_20NM:
		rc = dsi_pll_clock_register_20nm(pdev, pll_res);
		break;
	case MDSS_EDP_PLL:
		rc = edp_pll_clock_register(pdev, pll_res);
		break;
	case MDSS_HDMI_PLL:
		rc = hdmi_pll_clock_register(pdev, pll_res);
	case MDSS_HDMI_PLL_20NM:
		rc = hdmi_20nm_pll_clock_register(pdev, pll_res);
		break;
	case MDSS_UNKNOWN_PLL:
	default:
		rc = -EINVAL;
		break;
	}

	if (rc)
		pr_err("Pll parent clock register failed rc=%d\n", rc);

	return rc;
}

static int mdss_pll_probe(struct platform_device *pdev)
{
	int rc = 0;
	const char *label;
	struct resource *pll_base_reg;
	struct resource *phy_base_reg;
	struct resource *dynamic_pll_base_reg;
	struct resource *gdsc_base_reg;
	struct mdss_pll_resources *pll_res;

	if (!pdev->dev.of_node) {
		pr_err("MDSS pll driver only supports device tree probe\n");
		rc = -ENOTSUPP;
		goto error;
	}

	label = of_get_property(pdev->dev.of_node, "label", NULL);
	if (!label)
		pr_info("%d: MDSS pll label not specified\n", __LINE__);
	else
		pr_info("MDSS pll label = %s\n", label);

	pll_res = devm_kzalloc(&pdev->dev, sizeof(struct mdss_pll_resources),
								GFP_KERNEL);
	if (!pll_res) {
		pr_err("Failed to allocate the clock pll\n");
		rc = -ENOMEM;
		goto error;
	}
	platform_set_drvdata(pdev, pll_res);

	rc = of_property_read_u32(pdev->dev.of_node, "cell-index",
			&pll_res->index);
	if (rc) {
		pr_err("Unable to get the cell-index rc=%d\n", rc);
		pll_res->index = 0;
	}

	pll_base_reg = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, "pll_base");
	if (!pll_base_reg) {
		pr_err("Unable to get the pll base resources\n");
		rc = -ENOMEM;
		goto io_error;
	}

	pll_res->pll_base = ioremap(pll_base_reg->start,
						resource_size(pll_base_reg));
	if (!pll_res->pll_base) {
		pr_err("Unable to remap pll base resources\n");
		rc = -ENOMEM;
		goto io_error;
	}

	rc = mdss_pll_resource_parse(pdev, pll_res);
	if (rc) {
		pr_err("Pll resource parsing from dt failed rc=%d\n", rc);
		goto res_parse_error;
	}

	phy_base_reg = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, "phy_base");
	if (!phy_base_reg) {
		/* This resource is mandatory for HDMI pll */
		if (pll_res->pll_interface_type == MDSS_HDMI_PLL) {
			pr_err("Unable to get the phy base resources\n");
			rc = -ENOMEM;
			goto phy_io_error;
		}
	} else {
		pll_res->phy_base = ioremap(phy_base_reg->start,
						resource_size(phy_base_reg));
		if (!pll_res->phy_base) {
			pr_err("Unable to remap pll phy base resources\n");
			rc = -ENOMEM;
			goto phy_io_error;
		}
	}

	dynamic_pll_base_reg = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "dynamic_pll_base");
	if (dynamic_pll_base_reg) {
		pll_res->dyn_pll_base = ioremap(dynamic_pll_base_reg->start,
				resource_size(dynamic_pll_base_reg));
		if (!pll_res->dyn_pll_base) {
			pr_err("Unable to remap dynamic pll base resources\n");
			rc = -ENOMEM;
			goto dyn_pll_io_error;
		}
	}

	gdsc_base_reg = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "gdsc_base");
	if (!gdsc_base_reg) {
		pr_err("Unable to get the gdsc base resource\n");
		rc = -ENOMEM;
		goto gdsc_io_error;
	}
	pll_res->gdsc_base = ioremap(gdsc_base_reg->start,
			resource_size(gdsc_base_reg));
	if (!pll_res->gdsc_base) {
		pr_err("Unable to remap gdsc base resources\n");
		rc = -ENOMEM;
		goto gdsc_io_error;
	}

	pll_res->pll_en_90_phase = of_property_read_bool(pdev->dev.of_node,
						"qcom,mdss-en-pll-90-phase");
	if (pll_res->pll_en_90_phase)
		pr_debug("%s: PLL configured to enable 90-Phase", __func__);

	rc = mdss_pll_resource_init(pdev, pll_res);
	if (rc) {
		pr_err("Pll resource init failed rc=%d\n", rc);
		goto res_init_error;
	}

	rc = mdss_pll_clock_register(pdev, pll_res);
	if (rc) {
		pr_err("Pll clock register failed rc=%d\n", rc);
		goto clock_register_error;
	}

	return rc;

clock_register_error:
	mdss_pll_resource_deinit(pdev, pll_res);
res_init_error:
	if (pll_res->gdsc_base)
		iounmap(pll_res->gdsc_base);
gdsc_io_error:
	if (pll_res->dyn_pll_base)
		iounmap(pll_res->dyn_pll_base);
dyn_pll_io_error:
	if (pll_res->phy_base)
		iounmap(pll_res->phy_base);
phy_io_error:
	mdss_pll_resource_release(pdev, pll_res);
res_parse_error:
	iounmap(pll_res->pll_base);
io_error:
	devm_kfree(&pdev->dev, pll_res);
error:
	return rc;
}

static int mdss_pll_remove(struct platform_device *pdev)
{
	struct mdss_pll_resources *pll_res;

	pll_res = platform_get_drvdata(pdev);
	if (!pll_res) {
		pr_err("Invalid PLL resource data");
		return 0;
	}

	mdss_pll_resource_deinit(pdev, pll_res);
	if (pll_res->phy_base)
		iounmap(pll_res->phy_base);
	if (pll_res->gdsc_base)
		iounmap(pll_res->gdsc_base);
	mdss_pll_resource_release(pdev, pll_res);
	iounmap(pll_res->pll_base);
	devm_kfree(&pdev->dev, pll_res);
	return 0;
}

static const struct of_device_id mdss_pll_dt_match[] = {
	{.compatible = "qcom,mdss_dsi_pll_8974"},
	{.compatible = "qcom,mdss_dsi_pll_8994"},
	{.compatible = "qcom,mdss_hdmi_pll_8994"},
	{.compatible = "qcom,mdss_dsi_pll_8992"},
	{.compatible = "qcom,mdss_hdmi_pll_8992"},
	{.compatible = "qcom,mdss_dsi_pll_8916"},
	{.compatible = "qcom,mdss_dsi_pll_8939"},
	{.compatible = "qcom,mdss_dsi_pll_8909"},
	{.compatible = "qcom,mdss_dsi_pll_8952"},
	{.compatible = "qcom,mdss_dsi_pll_8976"},
	{.compatible = "qcom,mdss_edp_pll"},
	{.compatible = "qcom,mdss_hdmi_pll"},
	{}
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
subsys_initcall(mdss_pll_driver_init);

static void __exit mdss_pll_driver_deinit(void)
{
	platform_driver_unregister(&mdss_pll_driver);
}
module_exit(mdss_pll_driver_deinit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("mdss pll driver");
