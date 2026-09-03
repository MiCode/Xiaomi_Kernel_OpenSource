// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dpu.h"

static struct dpu_clk_context {
	struct clk *clk_src_128m;
	struct clk *clk_src_153m6;
	struct clk *clk_src_192m;
	struct clk *clk_src_256m;
	struct clk *clk_src_384m;
	struct clk *clk_dpu_core;
	struct clk *clk_dpu_dpi;
} dpu_clk_ctx;

static struct dpu_glb_context {
	struct clk *clk_aon_apb_disp_eb;
	struct regmap *aon_apb;
	u32 reg;
	u32 mask;
} dpu_glb_ctx;

static const u32 dpu_core_clk[] = {
	153600000,
	192000000,
	256000000,
	384000000
};

static const u32 dpi_clk_src[] = {
	128000000,
	153600000,
	192000000
};

static struct clk *val_to_clk(struct dpu_clk_context *ctx, u32 val)
{
	switch (val) {
	case 128000000:
		return ctx->clk_src_128m;
	case 153600000:
		return ctx->clk_src_153m6;
	case 192000000:
		return ctx->clk_src_192m;
	case 256000000:
		return ctx->clk_src_256m;
	case 384000000:
		return ctx->clk_src_384m;
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
	clk_ctx->clk_src_153m6 =
		of_clk_get_by_name(np, "clk_src_153m6");
	clk_ctx->clk_src_192m =
		of_clk_get_by_name(np, "clk_src_192m");
	clk_ctx->clk_src_256m =
		of_clk_get_by_name(np, "clk_src_256m");
	clk_ctx->clk_src_384m =
		of_clk_get_by_name(np, "clk_src_384m");
	clk_ctx->clk_dpu_core =
		of_clk_get_by_name(np, "clk_dpu_core");
	clk_ctx->clk_dpu_dpi =
		of_clk_get_by_name(np, "clk_dpu_dpi");

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
	return 128000000;
}

static int dpu_clk_init(struct dpu_context *ctx)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;
	struct clk *clk_src;
	u32 dpu_core_val;
	u32 dpi_src_val;
	int ret;

	dpu_core_val = calc_dpu_core_clk();
	dpi_src_val = calc_dpi_clk_src(ctx->vm.pixelclock);

	pr_info("DPU_CORE_CLK = %u, DPI_CLK_SRC = %u\n",
		dpu_core_val, dpi_src_val);
	pr_info("dpi clock is %lu\n", ctx->vm.pixelclock);

	clk_src = val_to_clk(clk_ctx, dpu_core_val);
	ret = clk_set_parent(clk_ctx->clk_dpu_core, clk_src);
	if (ret)
		pr_warn("set dpu core clk source failed\n");

	clk_src = val_to_clk(clk_ctx, dpi_src_val);
	ret = clk_set_parent(clk_ctx->clk_dpu_dpi, clk_src);
	if (ret)
		pr_warn("set dpi clk source failed\n");

	ret = clk_set_rate(clk_ctx->clk_dpu_dpi, ctx->vm.pixelclock);
	if (ret)
		pr_err("dpu update dpi clk rate failed\n");

	return ret;
}

static int dpu_clk_enable(struct dpu_context *ctx)
{
	struct dpu_clk_context *clk_ctx = &dpu_clk_ctx;
	int ret;

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

	clk_set_parent(clk_ctx->clk_dpu_dpi, clk_ctx->clk_src_128m);
	clk_set_parent(clk_ctx->clk_dpu_core, clk_ctx->clk_src_153m6);

	return 0;
}

static int dpu_glb_parse_dt(struct dpu_context *ctx,
				struct device_node *np)
{
	struct dpu_glb_context *glb_ctx = &dpu_glb_ctx;
	u32 args[2];

	glb_ctx->aon_apb = syscon_regmap_lookup_by_phandle_args(np,
			"reset-syscon", 2, args);
	if (IS_ERR(glb_ctx->aon_apb)) {
		pr_warn("failed to get reset syscon\n");
		return PTR_ERR(glb_ctx->aon_apb);
	} else {
		glb_ctx->reg = args[0];
		glb_ctx->mask = args[1];
	}

	glb_ctx->clk_aon_apb_disp_eb =
		of_clk_get_by_name(np, "clk_aon_apb_disp_eb");
	if (IS_ERR(glb_ctx->clk_aon_apb_disp_eb)) {
		pr_warn("read clk_aon_apb_disp_eb failed\n");
		glb_ctx->clk_aon_apb_disp_eb = NULL;
	}

	return 0;
}

static void dpu_glb_enable(struct dpu_context *ctx)
{
	struct dpu_glb_context *glb_ctx = &dpu_glb_ctx;
	int ret;

	ret = clk_prepare_enable(glb_ctx->clk_aon_apb_disp_eb);
	if (ret) {
		pr_err("enable clk_aon_apb_disp_eb failed!\n");
		return;
	}
}

static void dpu_glb_disable(struct dpu_context *ctx)
{
	struct dpu_glb_context *glb_ctx = &dpu_glb_ctx;

	clk_disable_unprepare(glb_ctx->clk_aon_apb_disp_eb);
}

static void dpu_reset(struct dpu_context *ctx)
{
	struct dpu_glb_context *glb_ctx = &dpu_glb_ctx;

	regmap_update_bits(glb_ctx->aon_apb, glb_ctx->reg,
			   glb_ctx->mask, glb_ctx->mask);
	udelay(10);
	regmap_update_bits(glb_ctx->aon_apb, glb_ctx->reg,
			   glb_ctx->mask, (u32)~glb_ctx->mask);
}

static void dpu_power_domain(struct dpu_context *ctx, int enable)
{
	/* The dpu power domain code is in drivers/soc/sprd/domain/. */
}

const struct dpu_clk_ops sharkl3_dpu_clk_ops = {
	.parse_dt = dpu_clk_parse_dt,
	.init = dpu_clk_init,
	.enable = dpu_clk_enable,
	.disable = dpu_clk_disable,
};

const struct dpu_glb_ops sharkl3_dpu_glb_ops = {
	.parse_dt = dpu_glb_parse_dt,
	.reset = dpu_reset,
	.enable = dpu_glb_enable,
	.disable = dpu_glb_disable,
	.power = dpu_power_domain,
};

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Unisoc SharkL3 DPU global and clk regs config");
MODULE_LICENSE("GPL v2");
