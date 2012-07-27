/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/msm_mdp.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/version.h>

#include "mdss_panel.h"

static int mdss_wb_on(struct mdss_panel_data *pdata)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mdss_wb_off(struct mdss_panel_data *pdata)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mdss_wb_parse_dt(struct platform_device *pdev,
			    struct mdss_panel_data *pdata)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[2], tmp;
	int rc;

	rc = of_property_read_u32_array(np, "qcom,mdss_pan_res", res, 2);
	pdata->panel_info.xres = (!rc ? res[0] : 1280);
	pdata->panel_info.yres = (!rc ? res[1] : 720);

	rc = of_property_read_u32(np, "qcom,mdss_pan_bpp", &tmp);
	pdata->panel_info.bpp = (!rc ? tmp : 24);

	return 0;
}

static int mdss_wb_probe(struct platform_device *pdev)
{
	struct mdss_panel_data *pdata = NULL;
	int rc = 0;

	if (!pdev->dev.of_node)
		return -ENODEV;

	pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	rc = !mdss_wb_parse_dt(pdev, pdata);
	if (!rc)
		return rc;

	pdata->panel_info.type = WRITEBACK_PANEL;
	pdata->panel_info.clk_rate = 74250000;
	pdata->panel_info.pdest = DISPLAY_3;
	pdata->panel_info.out_format = MDP_Y_CBCR_H2V2;

	pdata->on = mdss_wb_on;
	pdata->off = mdss_wb_off;
	pdev->dev.platform_data = pdata;

	rc = mdss_register_panel(pdata);
	if (rc) {
		dev_err(&pdev->dev, "unable to register writeback panel\n");
		return rc;
	}

	return rc;
}

static const struct of_device_id mdss_wb_match[] = {
	{ .compatible = "qcom,mdss_wb", },
	{ { 0 } }
};

static struct platform_driver mdss_wb_driver = {
	.probe = mdss_wb_probe,
	.driver = {
		.name = "mdss_wb",
		.of_match_table = mdss_wb_match,
	},
};

static int __init mdss_wb_driver_init(void)
{
	int rc = 0;
	rc = platform_driver_register(&mdss_wb_driver);
	return rc;
}

module_init(mdss_wb_driver_init);
