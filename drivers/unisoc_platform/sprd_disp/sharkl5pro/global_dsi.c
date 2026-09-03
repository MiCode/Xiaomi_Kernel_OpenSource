// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include "sprd_dsi.h"

static struct clk *clk_ap_ahb_dsi_eb;
static struct reset_control *dsi_rst;

static int dsi_glb_parse_dt(struct dsi_context *ctx,
				struct device_node *np)
{
	struct platform_device *pdev = of_find_device_by_node(np);

	dsi_rst = devm_reset_control_get(&pdev->dev, "dsi_rst");
	if (IS_ERR_OR_NULL(dsi_rst)) {
		DRM_ERROR("failed to get dsi reset control\n");
		return -EFAULT;
	}

	clk_ap_ahb_dsi_eb =
		of_clk_get_by_name(np, "clk_ap_ahb_dsi_eb");
	if (IS_ERR(clk_ap_ahb_dsi_eb)) {
		pr_warn("read clk_ap_ahb_dsi_eb failed\n");
		clk_ap_ahb_dsi_eb = NULL;
	}

	return 0;
}

static void dsi_glb_enable(struct dsi_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_ap_ahb_dsi_eb);
	if (ret)
		pr_err("enable clk_ap_ahb_dsi_eb failed!\n");
}

static void dsi_glb_disable(struct dsi_context *ctx)
{
	clk_disable_unprepare(clk_ap_ahb_dsi_eb);
}

static void dsi_reset(struct dsi_context *ctx)
{
	if (!IS_ERR_OR_NULL(dsi_rst)) {
		reset_control_assert(dsi_rst);
		udelay(10);
		reset_control_deassert(dsi_rst);
	}
}

const struct dsi_glb_ops sharkl5pro_dsi_glb_ops = {
	.parse_dt = dsi_glb_parse_dt,
	.reset = dsi_reset,
	.enable = dsi_glb_enable,
	.disable = dsi_glb_disable,
};

MODULE_AUTHOR("Albert Zhang <albert.zhang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc SharkL5 Pro DSI global APB regs low-level config");
MODULE_LICENSE("GPL v2");