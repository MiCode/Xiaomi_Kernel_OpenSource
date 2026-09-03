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

static struct clk *clk_dsi0_eb;
static struct clk *clk_dsi1_eb;
static struct clk *clk_dpu_dpi;
static struct clk *clk_src_384m;
static struct clk *clk_dpuvsp_eb;
static struct clk *clk_dpuvsp_disp_eb;

static struct reset_control *ctx_reset, *s_ctx_reset;

static int dsi_glb_parse_dt(struct dsi_context *ctx,
		struct device_node *np)
{
	struct platform_device *pdev = of_find_device_by_node(np);

	clk_dsi0_eb =
		of_clk_get_by_name(np, "clk_dsi0_eb");
	if (IS_ERR(clk_dsi0_eb)) {
		pr_warn("read clk_dsi0_eb failed\n");
		clk_dsi0_eb = NULL;
	}

	clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");
	if (IS_ERR(clk_dpu_dpi)) {
		pr_warn("read clk_dpu_dpi failed\n");
		clk_dpu_dpi = NULL;
	}

	clk_src_384m =
		of_clk_get_by_name(np, "clk_src_384m");
	if (IS_ERR(clk_src_384m)) {
		pr_warn("read clk_src_384m failed\n");
		clk_src_384m = NULL;
	}

	clk_dpuvsp_eb =
		of_clk_get_by_name(np, "clk_dpuvsp_eb");
	if (IS_ERR(clk_dpuvsp_eb)) {
		pr_warn("read clk_dpuvsp_eb failed\n");
		clk_dpuvsp_eb = NULL;
	}

	clk_dpuvsp_disp_eb =
		of_clk_get_by_name(np, "clk_dpuvsp_disp_eb");
	if (IS_ERR(clk_dpuvsp_disp_eb)) {
		pr_warn("read clk_dpuvsp_disp_eb failed\n");
		clk_dpuvsp_disp_eb = NULL;
	}

	ctx_reset = devm_reset_control_get(&pdev->dev, "dsi_rst");
	if (IS_ERR(ctx_reset)) {
		pr_warn("read ctx_reset failed\n");
		return PTR_ERR(ctx_reset);
	}

	return 0;
}

static int dsi_s_glb_parse_dt(struct dsi_context *ctx,
				struct device_node *np)
{
	struct sprd_dsi *dsi = (struct sprd_dsi *)container_of(ctx,
							struct sprd_dsi, ctx);

	pr_info("%s enter\n", __func__);

	clk_dsi1_eb =
		of_clk_get_by_name(np, "clk_dsi1_eb");
	if (IS_ERR(clk_dsi1_eb)) {
		pr_warn("read clk_dsi1_eb failed\n");
		clk_dsi1_eb = NULL;
	}

	s_ctx_reset = devm_reset_control_get(&dsi->dev, "s_dsi_rst");
	if (IS_ERR(s_ctx_reset)) {
		pr_warn("read s_ctx_reset failed\n");
		return PTR_ERR(s_ctx_reset);
	}

	return 0;
}

static int dsi_core_clk_switch(struct dsi_context *ctx)
{
	int ret;

	ret = clk_set_parent(clk_dpu_dpi, clk_src_384m);
	if (ret) {
		pr_err("clk_dpu_dpi set 384m error\n");
		return ret;
	}
	ret = clk_set_rate(clk_dpu_dpi, 384000000);
	if (ret)
		pr_err("dpi clk rate failed\n");

	return ret;
}

static void dsi_glb_enable(struct dsi_context *ctx)
{
	int ret;

	if (!ctx->is_esd_rst) {
		ret = clk_prepare_enable(clk_dpuvsp_eb);
		if (ret) {
			pr_err("enable clk_dpuvsp_eb failed!\n");
			return;
		}

		ret = clk_prepare_enable(clk_dpuvsp_disp_eb);
		if (ret) {
			pr_err("enable clk_dpuvsp_disp_eb failed!\n");
			return;
		}

		ret = clk_prepare_enable(clk_dsi0_eb);
		if (ret)
			pr_err("enable clk_dsi0_eb failed!\n");
	}
}

static void dsi_s_glb_enable(struct dsi_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_dsi1_eb);
	if (ret)
		pr_err("enable clk_dsi1_eb failed!\n");
}

static void dsi_glb_disable(struct dsi_context *ctx)
{
	if (ctx->clk_dpi_384m) {
		dsi_core_clk_switch(ctx);
		ctx->clk_dpi_384m = false;
	} else {
		clk_disable_unprepare(clk_dsi0_eb);
	}
}

static void dsi_s_glb_disable(struct dsi_context *ctx)
{
	clk_disable_unprepare(clk_dsi1_eb);
}

static void dsi_reset(struct dsi_context *ctx)
{
	if (!IS_ERR(ctx_reset)) {
		reset_control_assert(ctx_reset);
		udelay(10);
		reset_control_deassert(ctx_reset);
	}
}

static void dsi_s_reset(struct dsi_context *ctx)
{
	if (!IS_ERR(s_ctx_reset)) {
		reset_control_assert(s_ctx_reset);
		udelay(10);
		reset_control_deassert(s_ctx_reset);
	}
}

const struct dsi_glb_ops qogirn6pro_dsi_glb_ops = {
	.parse_dt = dsi_glb_parse_dt,
	.reset = dsi_reset,
	.enable = dsi_glb_enable,
	.disable = dsi_glb_disable,
};

const struct dsi_glb_ops qogirn6pro_dsi_s_glb_ops = {
	.parse_dt = dsi_s_glb_parse_dt,
	.reset = dsi_s_reset,
	.enable = dsi_s_glb_enable,
	.disable = dsi_s_glb_disable,
};

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Junxiao.feng@unisoc.com");
MODULE_DESCRIPTION("sprd qogirn6pro dsi global APB regs low-level config");
