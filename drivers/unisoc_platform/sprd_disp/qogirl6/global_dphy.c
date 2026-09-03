/*
 * Copyright (C) 2020 UNISOC Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
} ctx_enable, ctx_power;

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

	return 0;
}

static void dphy_glb_enable(struct dphy_context *ctx)
{
	regmap_update_bits(ctx_enable.regmap,
		ctx_enable.ctrl_reg,
		ctx_enable.ctrl_mask,
		ctx_enable.ctrl_mask);
}

static void dphy_glb_disable(struct dphy_context *ctx)
{
	regmap_update_bits(ctx_enable.regmap,
		ctx_enable.ctrl_reg,
		ctx_enable.ctrl_mask,
		(unsigned int)(~ctx_enable.ctrl_mask));
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

const struct dphy_glb_ops qogirl6_dphy_glb_ops = {
	.parse_dt = dphy_glb_parse_dt,
	.enable = dphy_glb_enable,
	.disable = dphy_glb_disable,
	.power = dphy_power_domain,
};

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Pony.Wu@unisoc.com");
MODULE_DESCRIPTION("sprd qogirl6 dphy global AHB&APB regs low-level config");
