// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dsi.h"

static struct dsi_glb_context {
	struct clk *clk_aon_apb_disp_eb;
	struct regmap *aon_apb;
	u32 reg;
	u32 mask;
} dsi_glb_ctx;

static int dsi_glb_parse_dt(struct dsi_context *ctx,
				struct device_node *np)
{
	struct dsi_glb_context *glb_ctx = &dsi_glb_ctx;
	u32 args[2];

	glb_ctx->clk_aon_apb_disp_eb =
		of_clk_get_by_name(np, "clk_aon_apb_disp_eb");
	if (IS_ERR(glb_ctx->clk_aon_apb_disp_eb)) {
		pr_warn("read clk_aon_apb_disp_eb failed\n");
		glb_ctx->clk_aon_apb_disp_eb = NULL;
	}

	glb_ctx->aon_apb = syscon_regmap_lookup_by_phandle_args(np,
			"reset-syscon", 2, args);
	if (IS_ERR(glb_ctx->aon_apb)) {
		pr_warn("failed to get reset syscon\n");
		return PTR_ERR(glb_ctx->aon_apb);
	} else {
		glb_ctx->reg = args[0];
		glb_ctx->mask = args[1];
	}

	return 0;
}

static void dsi_glb_enable(struct dsi_context *ctx)
{
	struct dsi_glb_context *glb_ctx = &dsi_glb_ctx;
	int ret;

	ret = clk_prepare_enable(glb_ctx->clk_aon_apb_disp_eb);
	if (ret)
		pr_err("enable clk_aon_apb_disp_eb failed!\n");
}

static void dsi_glb_disable(struct dsi_context *ctx)
{
	struct dsi_glb_context *glb_ctx = &dsi_glb_ctx;

	clk_disable_unprepare(glb_ctx->clk_aon_apb_disp_eb);
}

static void dsi_reset(struct dsi_context *ctx)
{
	struct dsi_glb_context *glb_ctx = &dsi_glb_ctx;

	regmap_update_bits(glb_ctx->aon_apb, glb_ctx->reg,
			   glb_ctx->mask, glb_ctx->mask);
	udelay(10);
	regmap_update_bits(glb_ctx->aon_apb, glb_ctx->reg,
			   glb_ctx->mask, (u32)~glb_ctx->mask);
}

const struct dsi_glb_ops sharkl3_dsi_glb_ops = {
	.parse_dt = dsi_glb_parse_dt,
	.reset = dsi_reset,
	.enable = dsi_glb_enable,
	.disable = dsi_glb_disable,
};

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Unisoc SharkL3 DSI global APB regs low-level config");
MODULE_LICENSE("GPL v2");
