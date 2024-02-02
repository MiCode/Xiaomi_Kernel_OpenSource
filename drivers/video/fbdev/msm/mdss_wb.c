/* Copyright (c) 2011-2015, 2018, 2020, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/list.h>
#include <linux/msm_mdp.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/extcon.h>

#include "mdss_panel.h"
#include "mdss_wb.h"
#include "mdss.h"

/**
 * mdss_wb_check_params - check new panel info params
 * @pdata: current panel information
 * @new: updates to panel info
 *
 * Checks if there are any changes that require panel reconfiguration
 * in order to be reflected on writeback buffer.
 *
 * Return negative errno if invalid input, zero if there is no panel reconfig
 * needed and non-zero if reconfiguration is needed.
 */
static int mdss_wb_check_params(struct mdss_panel_data *pdata,
	struct mdss_panel_info *new)
{
	struct mdss_panel_info *old;

	if (!pdata || !new) {
		pr_err("%s: Invalid input\n", __func__);
		return -EINVAL;
	}

	if (new->xres >= 4096 || new->yres >= 4096) {
		pr_err("%s: Invalid resolutions\n", __func__);
		return -EINVAL;
	}

	old = &pdata->panel_info;

	if ((old->xres != new->xres) || (old->yres != new->yres))
		return 1;

	return 0;
}

static int mdss_wb_event_handler(struct mdss_panel_data *pdata,
				 int event, void *arg)
{
	int rc = 0;

	switch (event) {
	case MDSS_EVENT_CHECK_PARAMS:
		rc = mdss_wb_check_params(pdata, (struct mdss_panel_info *)arg);
		break;
	default:
		pr_debug("%s: panel event (%d) not handled\n", __func__, event);
		break;
	}
	return rc;
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

static const unsigned int mdss_wb_disp_supported_cable[] = {
	EXTCON_DISP_HMD + 1, /* For WFD */
	EXTCON_NONE,
};

static int mdss_wb_dev_init(struct mdss_wb_ctrl *wb_ctrl)
{
	int rc = 0;

	if (!wb_ctrl) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	memset(&wb_ctrl->sdev, 0x0, sizeof(wb_ctrl->sdev));
	wb_ctrl->sdev.supported_cable = mdss_wb_disp_supported_cable;
	wb_ctrl->sdev.dev.parent = &wb_ctrl->pdev->dev;
	wb_ctrl->sdev.name = "wfd";
	rc = extcon_dev_register(&wb_ctrl->sdev);
	if (rc) {
		pr_err("Failed to setup switch dev for writeback panel");
		return rc;
	}

	return 0;
}

static int mdss_wb_dev_uninit(struct mdss_wb_ctrl *wb_ctrl)
{
	if (!wb_ctrl) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	extcon_dev_unregister(&wb_ctrl->sdev);
	return 0;
}

static int mdss_wb_probe(struct platform_device *pdev)
{
	struct mdss_panel_data *pdata = NULL;
	struct mdss_wb_ctrl *wb_ctrl = NULL;
	struct mdss_util_intf *util;
	int rc = 0;

	if (!pdev->dev.of_node)
		return -ENODEV;

	util = mdss_get_util_intf();
	if (util == NULL) {
		pr_err("%s: Failed to get mdss utility functions\n", __func__);
		return -ENODEV;
	}

	if (!util->mdp_probe_done) {
		pr_err("%s: MDP not probed yet!\n", __func__);
		return -EPROBE_DEFER;
	}

	wb_ctrl = devm_kzalloc(&pdev->dev, sizeof(*wb_ctrl), GFP_KERNEL);
	if (!wb_ctrl)
		return -ENOMEM;

	pdata = &wb_ctrl->pdata;
	wb_ctrl->pdev = pdev;
	platform_set_drvdata(pdev, wb_ctrl);

	rc = !mdss_wb_parse_dt(pdev, pdata);
	if (!rc)
		goto error_no_mem;

	rc = mdss_wb_dev_init(wb_ctrl);
	if (rc) {
		dev_err(&pdev->dev, "unable to set up device nodes for writeback panel\n");
		goto error_no_mem;
	}

	pdata->panel_info.type = WRITEBACK_PANEL;
	pdata->panel_info.clk_rate = 74250000;
	pdata->panel_info.pdest = DISPLAY_4;
	pdata->panel_info.out_format = MDP_Y_CBCR_H2V2_VENUS;

	pdata->event_handler = mdss_wb_event_handler;
	pdev->dev.platform_data = pdata;

	rc = mdss_register_panel(pdev, pdata);
	if (rc) {
		dev_err(&pdev->dev, "unable to register writeback panel\n");
		goto error_init;
	}

	return rc;

error_init:
	mdss_wb_dev_uninit(wb_ctrl);
error_no_mem:
	devm_kfree(&pdev->dev, wb_ctrl);
	return rc;
}

static int mdss_wb_remove(struct platform_device *pdev)
{
	struct mdss_wb_ctrl *wb_ctrl = platform_get_drvdata(pdev);

	if (!wb_ctrl) {
		pr_err("%s: no driver data\n", __func__);
		return -ENODEV;
	}

	mdss_wb_dev_uninit(wb_ctrl);
	devm_kfree(&wb_ctrl->pdev->dev, wb_ctrl);
	return 0;
}

static const struct of_device_id mdss_wb_match[] = {
	{ .compatible = "qcom,mdss_wb", },
	{ { 0 } }
};

static struct platform_driver mdss_wb_driver = {
	.probe = mdss_wb_probe,
	.remove = mdss_wb_remove,
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
