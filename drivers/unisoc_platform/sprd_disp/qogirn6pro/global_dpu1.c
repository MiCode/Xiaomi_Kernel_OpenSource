// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/io.h>
#include <linux/of_address.h>

#include "sprd_dpu1.h"

static struct clk *clk_ipa_apb_dispc1_eb;

static struct dpu_clk_context {
	struct clk *clk_src_128m;
	struct clk *clk_src_192m;
	struct clk *clk_src_256m;
	struct clk *clk_src_307m2;
	struct clk *clk_src_384m;
	struct clk *clk_src_512m;
	struct clk *clk_dpu_core;
	struct clk *clk_dpu_dpi;
	struct clk *clk_pixelpll;
} dpu_clk_ctx;

static const u32 dpu_core_clk[] = {
	192000000,
	256000000,
	384000000,
	512000000
};

static const u32 dpi_clk_src[] = {
	128000000,
	192000000,
	297000000,
	307200000
};

static struct dpu_glb_context {
	unsigned int enable_reg;
	unsigned int mask_bit;
	struct regmap *regmap;
} ctx_reset, ctx_force_shutdown;

static struct clk *val_to_clk(struct dpu_clk_context *ctx, u32 val)
{
	switch (val) {
	case 128000000:
		return ctx->clk_src_128m;
	case 192000000:
		return ctx->clk_src_192m;
	case 256000000:
		return ctx->clk_src_256m;
	case 297000000:
		return ctx->clk_pixelpll;
	case 307200000:
		return ctx->clk_src_307m2;
	case 384000000:
		return ctx->clk_src_384m;
	case 512000000:
		return ctx->clk_src_512m;
	default:
		pr_err("invalid clock value %u\n", val);
		return NULL;
	}
}

static int dpu_clk_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	clk_ctx->clk_src_128m =
		of_clk_get_by_name(np, "clk_src_128m");
	clk_ctx->clk_src_192m =
		of_clk_get_by_name(np, "clk_src_192m");
	clk_ctx->clk_src_256m =
		of_clk_get_by_name(np, "clk_src_256m");
	clk_ctx->clk_src_307m2 =
		of_clk_get_by_name(np, "clk_src_307m2");
	clk_ctx->clk_src_384m =
		of_clk_get_by_name(np, "clk_src_384m");
	clk_ctx->clk_src_512m =
		of_clk_get_by_name(np, "clk_src_512m");
	clk_ctx->clk_dpu_core =
		of_clk_get_by_name(np, "clk_dpu_core");
	clk_ctx->clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");
	clk_ctx->clk_pixelpll =
		of_clk_get_by_name(np, "clk_pixelpll");

	if (IS_ERR(clk_ctx->clk_src_128m)) {
		pr_warn("read clk_src_128m failed\n");
		clk_ctx->clk_src_128m = NULL;
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

	if (IS_ERR(clk_ctx->clk_src_512m)) {
		pr_warn("read clk_src_512m failed\n");
		clk_ctx->clk_src_512m = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_core)) {
		pr_warn("read clk_dpu_core failed\n");
		clk_ctx->clk_dpu_core = NULL;
	}

	if (IS_ERR(clk_ctx->clk_dpu_dpi)) {
		pr_warn("read clk_dpu_dpi failed\n");
		clk_ctx->clk_dpu_dpi = NULL;
	}

	if (IS_ERR(clk_ctx->clk_pixelpll)) {
		pr_warn("read clk_pixelpll failed\n");
		clk_ctx->clk_pixelpll = NULL;
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

	pr_info("use default DPI_CLK_SRC\n");
	return 297000000;
}

static u32 calc_pixelpll_clk(u32 dpi_clock, u32 default_clock)
{
	u32 pixelpll_clock, dpi_div, pixelpll_div = 9;

	dpi_div = default_clock / dpi_clock;

	/* TODO
	 * pixelpll_clock divides by 9 to dpi, 4 to vpu_dec high speed.
	 * vpu_dec high speed range needs to be 400MHz-800Mhz.
	 * The range of dpi div is 1-16.
	 * When dpi_sel is 297MHz, the clock source is pixelpll.
	 * Here we choose "dpi_clock * dpi_div" closest to 297MHz.
	 */
	pixelpll_clock = dpi_clock * dpi_div * pixelpll_div;

	return pixelpll_clock;
}


static int dpu_clk_init(struct dpu_context *ctx)
{
	int ret;
	u32 dpu_core_val, dpi_src_val, dpi_clock, pixelpll_clock;
	struct clk *clk_src;
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	dpu_core_val = calc_dpu_core_clk();
	dpi_src_val = calc_dpi_clk_src(ctx->vm.pixelclock);

	pr_info("DPU_CORE_CLK = %u, DPI_CLK_SRC = %u\n",
		dpu_core_val, dpi_src_val);
	pr_info("pixelclock is %lu\n", ctx->vm.pixelclock);

	clk_src = val_to_clk(clk_ctx, dpu_core_val);
	ret = clk_set_parent(clk_ctx->clk_dpu_core, clk_src);
	if (ret)
		pr_warn("set dpu core clk source failed\n");

	clk_src = val_to_clk(clk_ctx, dpi_src_val);
	ret = clk_set_parent(clk_ctx->clk_dpu_dpi, clk_src);
	if (ret)
		pr_warn("set dpi clk source failed\n");

	dpi_clock = ctx->bypass_mode ?
		ctx->vm.pixelclock / 2 : ctx->vm.pixelclock;

	pixelpll_clock = calc_pixelpll_clk(dpi_clock, 297000000);
	ret = clk_set_rate(clk_ctx->clk_pixelpll, pixelpll_clock);
	if (ret)
		pr_err("dpu update pixelpll clk rate failed\n");

	ret = clk_set_rate(clk_ctx->clk_dpu_dpi, dpi_clock * 9);
	if (ret)
		pr_err("dpu update dpi clk rate failed\n");

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

	ret = clk_prepare_enable(clk_ctx->clk_pixelpll);
	if (ret) {
		pr_err("enable clk_pixelpll error\n");
		return ret;
	}

	ret = clk_prepare_enable(clk_ctx->clk_dpu_dpi);
	if (ret) {
		pr_err("enable clk_dpu_dpi error\n");
		return ret;
	}

	return 0;
}

static int dpu_clk_disable(struct dpu_context *ctx)
{
	int ret;
	unsigned long default_pixel_pll = 2673000000UL;
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;

	clk_disable_unprepare(clk_ctx->clk_dpu_dpi);
	clk_disable_unprepare(clk_ctx->clk_pixelpll);
	clk_disable_unprepare(clk_ctx->clk_dpu_core);

	clk_set_parent(clk_ctx->clk_dpu_dpi, clk_ctx->clk_pixelpll);
	clk_set_parent(clk_ctx->clk_dpu_core, clk_ctx->clk_src_192m);
	ret = clk_set_rate(clk_ctx->clk_pixelpll, default_pixel_pll);
	if (ret)
		pr_err("dpu reset pixelpll clk rate failed\n");
	ret = clk_set_rate(clk_ctx->clk_dpu_dpi, default_pixel_pll);
	if (ret)
		pr_err("dpu reset dpi clk rate failed\n");

	return 0;
}

static int dpu_glb_parse_dt(struct dpu_context *ctx,
		struct device_node *np)
{
	unsigned int syscon_args[2];
	ctx_reset.regmap = syscon_regmap_lookup_by_phandle_args(np,
			"reset-syscon", 2, syscon_args);
	if (IS_ERR(ctx_reset.regmap)) {
		pr_warn("failed to reset syscon\n");
		return PTR_ERR(ctx_reset.regmap);
	}  else {
		ctx_reset.enable_reg = syscon_args[0];
		ctx_reset.mask_bit = syscon_args[1];
	}

	clk_ipa_apb_dispc1_eb =
		of_clk_get_by_name(np, "clk_ipa_apb_dispc1_eb");
	if (IS_ERR(clk_ipa_apb_dispc1_eb)) {
		pr_warn("read clk_ipa_apb_dispc1_eb failed\n");
		clk_ipa_apb_dispc1_eb = NULL;
	}

	ctx_force_shutdown.regmap = syscon_regmap_lookup_by_phandle_args(np,
			"force-shutdown-syscon", 2, syscon_args);
	if (IS_ERR(ctx_reset.regmap)) {
		pr_warn("failed to force-shutdown syscon\n");
		return PTR_ERR(ctx_reset.regmap);
	}  else {
		ctx_force_shutdown.enable_reg = syscon_args[0];
		ctx_force_shutdown.mask_bit = syscon_args[1];
	}

	return 0;
}

static void dpu_glb_enable(struct dpu_context *ctx)
{
	int ret;

	ret = clk_prepare_enable(clk_ipa_apb_dispc1_eb);
	if (ret) {
		pr_err("enable clk_aon_apb_disp_eb failed!\n");
		return;
	}
}

static void dpu_glb_disable(struct dpu_context *ctx)
{
	clk_disable_unprepare(clk_ipa_apb_dispc1_eb);
}

static void dpu_reset(struct dpu_context *ctx)
{
/*
	regmap_update_bits(ctx_reset.regmap,
		    ctx_reset.enable_reg,
		    ctx_reset.mask_bit,
		    ctx_reset.mask_bit);
	udelay(10);
	regmap_update_bits(ctx_reset.regmap,
		    ctx_reset.enable_reg,
		    ctx_reset.mask_bit,
		    (unsigned int)(~ctx_reset.mask_bit));
*/
}

static void dpu_power_domain(struct dpu_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(ctx_force_shutdown.regmap,
			ctx_force_shutdown.enable_reg,
			ctx_force_shutdown.mask_bit,
			(unsigned int)(~ctx_force_shutdown.mask_bit));
		/* TODO polling power-on status register */
		udelay(100);
	} else
		regmap_update_bits(ctx_force_shutdown.regmap,
			ctx_force_shutdown.enable_reg,
			ctx_force_shutdown.mask_bit,
			ctx_force_shutdown.mask_bit);
}

const struct dpu_clk_ops qogirn6pro_dpu1_clk_ops = {
	.parse_dt = dpu_clk_parse_dt,
	.init = dpu_clk_init,
	.enable = dpu_clk_enable,
	.disable = dpu_clk_disable,
};

const struct dpu_glb_ops qogirn6pro_dpu1_glb_ops = {
	.parse_dt = dpu_glb_parse_dt,
	.reset = dpu_reset,
	.enable = dpu_glb_enable,
	.disable = dpu_glb_disable,
	.power = dpu_power_domain,
};

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("chen.he@unisoc.com");
MODULE_DESCRIPTION("sprd sharkl6Pro dpu1 global and clk regs config");
