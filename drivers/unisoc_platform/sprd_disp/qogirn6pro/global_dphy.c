// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dphy.h"

static struct dphy_glb_context {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;
	struct regmap *regmap;
} ctx_enable, ctx_power, s_ctx_enable, s_ctx_power, ctx_aod_mode, ctx_aod_pd;

static int dphy_glb_parse_dt(struct dphy_context *ctx,
				struct device_node *np)
{
	unsigned int syscon_args[2];

	ctx_enable.regmap = syscon_regmap_lookup_by_phandle_args(np,
	"enable-syscon", 2, syscon_args);
	if (IS_ERR(ctx_enable.regmap)) {
		pr_warn("failed to map dphy glb reg: enable\n");
	} else {
		ctx_enable.ctrl_reg = syscon_args[0];
		ctx_enable.ctrl_mask = syscon_args[1];
	}
	ctx_power.regmap = syscon_regmap_lookup_by_phandle_args(np,
	"power-syscon", 2, syscon_args);
	if (IS_ERR(ctx_power.regmap)) {
		pr_warn("failed to map dphy glb reg: power\n");
	} else {
		ctx_power.ctrl_reg = syscon_args[0];
		ctx_power.ctrl_mask = syscon_args[1];
	}

	ctx_aod_mode.regmap = syscon_regmap_lookup_by_phandle_args(np,
	"aod_mode-syscon", 2, syscon_args);
	if (IS_ERR(ctx_aod_mode.regmap)) {
		pr_warn("failed to map dphy glb reg: aod_mode\n");
	} else {
		ctx_aod_mode.ctrl_reg = syscon_args[0];
		ctx_aod_mode.ctrl_mask = syscon_args[1];
	}

	ctx_aod_pd.regmap = syscon_regmap_lookup_by_phandle_args(np,
	"aod_pd-syscon", 2, syscon_args);
	if (IS_ERR(ctx_aod_pd.regmap)) {
		pr_warn("failed to map dphy glb reg: aod_mode\n");
	} else {
		ctx_aod_pd.ctrl_reg = syscon_args[0];
		ctx_aod_pd.ctrl_mask = syscon_args[1];
	}

	return 0;
}

static int dphy_s_glb_parse_dt(struct dphy_context *ctx,
				struct device_node *np)
{
	unsigned int syscon_args[2];

	s_ctx_enable.regmap = syscon_regmap_lookup_by_phandle_args(np,
	"enable-syscon", 2, syscon_args);
	if (IS_ERR(s_ctx_enable.regmap)) {
		pr_warn("failed to map dphy glb reg: enable\n");
	} else {
		s_ctx_enable.ctrl_reg = syscon_args[0];
		s_ctx_enable.ctrl_mask = syscon_args[1];
	}

	s_ctx_power.regmap = syscon_regmap_lookup_by_phandle_args(np,
	"power-syscon", 2,syscon_args);
	if (IS_ERR(s_ctx_power.regmap)) {
		pr_warn("failed to map dphy glb reg: power\n");
	} else {
		s_ctx_power.ctrl_reg = syscon_args[0];
		s_ctx_power.ctrl_mask = syscon_args[1];
	}

	return 0;
}
static void dphy_glb_enable(struct dphy_context *ctx)
{
	if (ctx->aod_mode)
		regmap_update_bits(ctx_aod_mode.regmap,
				ctx_aod_mode.ctrl_reg,
				ctx_aod_mode.ctrl_mask,
				ctx_enable.ctrl_mask & (ctx->aod_mode << 9));
	if (ctx->aod_mode == 5)
		regmap_update_bits(ctx_aod_pd.regmap,
				ctx_aod_pd.ctrl_reg,
				ctx_aod_pd.ctrl_mask,
				(unsigned int)(~ctx_aod_pd.ctrl_mask));

	regmap_update_bits(ctx_enable.regmap,
		ctx_enable.ctrl_reg,
		ctx_enable.ctrl_mask,
		ctx_enable.ctrl_mask);
}

static void dphy_s_glb_enable(struct dphy_context *ctx)
{
	regmap_update_bits(s_ctx_enable.regmap,
		s_ctx_enable.ctrl_reg,
		s_ctx_enable.ctrl_mask,
		s_ctx_enable.ctrl_mask);
}

static void dphy_glb_disable(struct dphy_context *ctx)
{
	if (ctx->aod_mode == 5)
		regmap_update_bits(ctx_aod_pd.regmap,
				ctx_aod_pd.ctrl_reg,
				ctx_aod_pd.ctrl_mask,
				ctx_aod_pd.ctrl_mask);

	regmap_update_bits(ctx_enable.regmap,
		ctx_enable.ctrl_reg,
		ctx_enable.ctrl_mask,
		(unsigned int)(~ctx_enable.ctrl_mask));
}

static void dphy_s_glb_disable(struct dphy_context *ctx)
{
	regmap_update_bits(s_ctx_enable.regmap,
		s_ctx_enable.ctrl_reg,
		s_ctx_enable.ctrl_mask,
		(unsigned int)(~s_ctx_enable.ctrl_mask));
}

static void dphy_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(ctx_power.regmap,
			ctx_power.ctrl_reg,
			ctx_power.ctrl_mask,
			(unsigned int)(~ctx_power.ctrl_mask));

		/* Dphy has a random wakeup failed after poweron,
		 * this will caused testclr reset failed and
		 * writing pll configuration parameter failed.
		 * Delay 100us after dphy poweron, waiting for pll is stable.
		 */
		udelay(100);
	} else {
		regmap_update_bits(ctx_power.regmap,
			ctx_power.ctrl_reg,
			ctx_power.ctrl_mask,
			ctx_power.ctrl_mask);
	}
}

static void dphy_s_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(s_ctx_power.regmap,
			s_ctx_power.ctrl_reg,
			s_ctx_power.ctrl_mask,
			(unsigned int)(~s_ctx_power.ctrl_mask));

		/* Dphy has a random wakeup failed after poweron,
		 * this will caused testclr reset failed and
		 * writing pll configuration parameter failed.
		 * Delay 100us after dphy poweron, waiting for pll is stable.
		 */
		udelay(100);
	} else {
		regmap_update_bits(s_ctx_power.regmap,
			s_ctx_power.ctrl_reg,
			s_ctx_power.ctrl_mask,
			s_ctx_power.ctrl_mask);
	}
}

const struct dphy_glb_ops qogirn6pro_dphy_glb_ops = {
	.parse_dt = dphy_glb_parse_dt,
	.enable = dphy_glb_enable,
	.disable = dphy_glb_disable,
	.power = dphy_power_domain,
};

const struct dphy_glb_ops qogirn6pro_dphy_s_glb_ops = {
	.parse_dt = dphy_s_glb_parse_dt,
	.enable = dphy_s_glb_enable,
	.disable = dphy_s_glb_disable,
	.power = dphy_s_power_domain,
};

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Junxiao.feng@unisoc.com");
MODULE_DESCRIPTION("sprd qogirn6pro dphy global AHB&APB regs low-level config");
