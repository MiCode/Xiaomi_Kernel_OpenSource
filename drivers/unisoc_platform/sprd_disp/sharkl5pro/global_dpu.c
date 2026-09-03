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

#include "sprd_dpu.h"

enum {
	CLK_DPI_DIV6 = 6,
	CLK_DPI_DIV8 = 8
};

static struct clk *clk_ap_ahb_disp_eb;
static struct reset_control *dpu_rst;

static struct dpu_clk_context {
	struct clk *clk_src_96m;
	struct clk *clk_src_128m;
	struct clk *clk_src_153m6;
	struct clk *clk_src_192m;
	struct clk *clk_src_256m;
	struct clk *clk_src_307m2;
	struct clk *clk_src_384m;
	struct clk *clk_dpu_core;
	struct clk *clk_dpu_dpi;
	struct clk *clk_src_204m8;
	struct clk *clk_src_273m;
	struct clk *dsi_div6clk_gate;
} dpu_clk_ctx;

static const u32 dpu_core_clk[] = {
	153600000,
	192000000,
	256000000,
	307200000,
	384000000
};

static const u32 dpi_clk_src[] = {
	96000000,
	128000000,
	153600000,
	192000000
};

static struct clk *val_to_clk(struct dpu_clk_context *ctx, u32 val)
{
	switch (val) {
	case 96000000:
		return ctx->clk_src_96m;
	case 128000000:
		return ctx->clk_src_128m;
	case 153600000:
		return ctx->clk_src_153m6;
	case 192000000:
		return ctx->clk_src_192m;
	case 256000000:
		return ctx->clk_src_256m;
	case 307200000:
		return ctx->clk_src_307m2;
	case 384000000:
		return ctx->clk_src_384m;
	default:
		pr_err("invalid clock value %u\n", val);
		return NULL;
	}
}

static struct clk *div_to_clk(struct dpu_clk_context *clk_ctx, u32 clk_div)
{
	switch (clk_div) {
	case CLK_DPI_DIV6:
		return clk_ctx->clk_src_273m;
	case CLK_DPI_DIV8:
		return clk_ctx->clk_src_204m8;
	default:
		pr_err("invalid clock value %u\n", clk_div);
		return NULL;
	}
}

static int dpu_clk_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	clk_ctx->clk_src_96m =
		of_clk_get_by_name(np, "clk_src_96m");
	clk_ctx->clk_src_128m =
		of_clk_get_by_name(np, "clk_src_128m");
	clk_ctx->clk_src_153m6 =
		of_clk_get_by_name(np, "clk_src_153m6");
	clk_ctx->clk_src_192m =
		of_clk_get_by_name(np, "clk_src_192m");
	clk_ctx->clk_src_204m8 =
		of_clk_get_by_name(np, "clk_src_204m8");
	clk_ctx->clk_src_256m =
		of_clk_get_by_name(np, "clk_src_256m");
	clk_ctx->clk_src_273m =
		of_clk_get_by_name(np, "clk_src_273m");
	clk_ctx->clk_src_307m2 =
		of_clk_get_by_name(np, "clk_src_307m2");
	clk_ctx->clk_src_384m =
		of_clk_get_by_name(np, "clk_src_384m");
	clk_ctx->clk_dpu_core =
		of_clk_get_by_name(np, "clk_dpu_core");
	clk_ctx->clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");
	clk_ctx->dsi_div6clk_gate =
		of_clk_get_by_name(np, "dsi_div6clk_gate");

	if (IS_ERR(clk_ctx->clk_src_96m)) {
		pr_warn("read clk_src_96m failed\n");
		clk_ctx->clk_src_96m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_128m)) {
		pr_warn("read clk_src_128m failed\n");
		clk_ctx->clk_src_128m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_153m6)) {
		pr_warn("read clk_src_153m6 failed\n");
		clk_ctx->clk_src_153m6 = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_192m)) {
		pr_warn("read clk_src_192m failed\n");
		clk_ctx->clk_src_192m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_256m)) {
		pr_warn("read clk_src_256m failed\n");
		clk_ctx->clk_src_256m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_307m2)) {
		pr_warn("read clk_src_307m2 failed\n");
		clk_ctx->clk_src_307m2 = NULL;
	}

	if (IS_ERR(clk_ctx->clk_src_384m)) {
		pr_warn("read clk_src_384m failed\n");
		clk_ctx->clk_src_384m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_core)) {
		pr_warn("read clk_dpu_core failed\n");
		clk_ctx->clk_dpu_core = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_dpi)) {
		pr_warn("read clk_dpu_dpi failed\n");
		clk_ctx->clk_dpu_dpi = NULL;
	}

	if (IS_ERR(clk_ctx->dsi_div6clk_gate)) {
		pr_warn("read dsi_div6clk_gate failed\n");
		clk_ctx->dsi_div6clk_gate = NULL;
	}

	return 0;
}

static u32 calc_dpu_core_clk(void)
{
	return dpu_core_clk[ARRAY_SIZE(dpu_core_clk) - 1];
}

static u32 calc_dpi_clk_src(u32 pclk)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(dpi_clk_src); i++) {
		if ((dpi_clk_src[i] % pclk) == 0)
			return dpi_clk_src[i];
	}

	pr_err("calc DPI_CLK_SRC failed, use default\n");
	return 96000000;
}

static int dpu_clk_init(struct dpu_context *ctx)
{
	int ret;
	u32 dpu_core_val;
	u32 dpi_src_val;
	struct clk *clk_src;
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;
	struct sprd_dpu *dpu = (struct sprd_dpu *)container_of(ctx,
				struct sprd_dpu, ctx);

	dpu_core_val = calc_dpu_core_clk();

	if (dpu->dsi->ctx.dpi_clk_div) {
		pr_info("DPU_CORE_CLK = %u, DPI_CLK_DIV = %d\n",
				dpu_core_val, dpu->dsi->ctx.dpi_clk_div);
	} else {
		dpi_src_val = calc_dpi_clk_src(ctx->vm.pixelclock);

		pr_info("DPU_CORE_CLK = %u, DPI_CLK_SRC = %u\n",
			dpu_core_val, dpi_src_val);
		pr_info("dpi clock is %lu\n", ctx->vm.pixelclock);
	}

	clk_src = val_to_clk(clk_ctx, dpu_core_val);
	ret = clk_set_parent(clk_ctx->clk_dpu_core, clk_src);
	if (ret)
		pr_warn("set dpu core clk source failed\n");

	if (dpu->dsi->ctx.dpi_clk_div) {
		clk_src = div_to_clk(clk_ctx, dpu->dsi->ctx.dpi_clk_div);
		ret = clk_set_parent(clk_ctx->clk_dpu_dpi, clk_src);
		if (ret)
			pr_warn("set dpi clk source failed\n");
	} else {
		clk_src = val_to_clk(clk_ctx, dpi_src_val);
		ret = clk_set_parent(clk_ctx->clk_dpu_dpi, clk_src);
		if (ret)
			pr_warn("set dpi clk source failed\n");

		ret = clk_set_rate(clk_ctx->clk_dpu_dpi, ctx->vm.pixelclock);
		if (ret)
			pr_err("dpu update dpi clk rate failed\n");
	}

	return ret;
}

static int dpu_clk_enable(struct dpu_context *ctx)
{
	int ret;
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	ret = clk_prepare_enable(clk_ctx->clk_dpu_core);
	if (ret) {
		pr_err("enable clk_dpu_core error\n");
		return ret;
	}

	ret = clk_prepare_enable(clk_ctx->clk_dpu_dpi);
	if (ret) {
		pr_err("enable clk_dpu_dpi error\n");
		clk_disable_unprepare(clk_ctx->clk_dpu_core);
		return ret;
	}

	return 0;
}

static int dpu_clk_disable(struct dpu_context *ctx)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	clk_disable_unprepare(clk_ctx->clk_dpu_dpi);
	clk_disable_unprepare(clk_ctx->clk_dpu_core);

	clk_set_parent(clk_ctx->clk_dpu_dpi, clk_ctx->clk_src_96m);
	clk_set_parent(clk_ctx->clk_dpu_core, clk_ctx->clk_src_153m6);

	return 0;
}

static int dpu_glb_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct platform_device *pdev = of_find_device_by_node(np);

	dpu_rst = devm_reset_control_get(&pdev->dev, "dpu_rst");
	if (IS_ERR_OR_NULL(dpu_rst)) {
		DRM_ERROR("failed to get dpu reset control\n");
		return -EFAULT;
	}

	clk_ap_ahb_disp_eb =
		of_clk_get_by_name(np, "clk_ap_ahb_disp_eb");
	if (IS_ERR(clk_ap_ahb_disp_eb)) {
		pr_warn("read clk_ap_ahb_disp_eb failed\n");
		clk_ap_ahb_disp_eb = NULL;
	}

	return 0;
}

static void dpu_glb_enable(struct dpu_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_ap_ahb_disp_eb);
	if (ret) {
		pr_err("enable clk_ap_ahb_disp_eb failed!\n");
		return;
	}
}

static void dpu_glb_disable(struct dpu_context *ctx)
{
	clk_disable_unprepare(clk_ap_ahb_disp_eb);
}

static void dpu_reset(struct dpu_context *ctx)
{
	if (!IS_ERR_OR_NULL(dpu_rst)) {
		reset_control_assert(dpu_rst);
		udelay(10);
		reset_control_deassert(dpu_rst);
	}
}

static void dpu_power_domain(struct dpu_context *ctx, int enable)
{

}

const struct dpu_clk_ops sharkl5pro_dpu_clk_ops = {
	.parse_dt = dpu_clk_parse_dt,
	.init = dpu_clk_init,
	.enable = dpu_clk_enable,
	.disable = dpu_clk_disable,
};

const struct dpu_glb_ops sharkl5pro_dpu_glb_ops = {
	.parse_dt = dpu_glb_parse_dt,
	.reset = dpu_reset,
	.enable = dpu_glb_enable,
	.disable = dpu_glb_disable,
	.power = dpu_power_domain,
};

MODULE_AUTHOR("Albert Zhang <albert.zhang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc SharkL5 Pro DPU global and clk regs config");
MODULE_LICENSE("GPL v2");
