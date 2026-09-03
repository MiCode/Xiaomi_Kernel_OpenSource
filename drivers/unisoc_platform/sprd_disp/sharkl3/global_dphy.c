// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>

#include "sprd_dphy.h"

struct glb_ctrl {
	struct regmap *regmap;
	u32 reg;
	u32 mask;
};

static struct glb_ctrl enable;
static struct glb_ctrl power_s;
static struct glb_ctrl power_l;
static struct glb_ctrl power_iso;

static int dphy_glb_parse_dt(struct dphy_context *ctx,
				struct device_node *np)
{
	u32 args[2];

	enable.regmap = syscon_regmap_lookup_by_phandle_args(np,
			"enable-syscon", 2, args);
	if (IS_ERR(enable.regmap))
		pr_warn("failed to get enable syscon\n");
	else {
		enable.reg = args[0];
		enable.mask = args[1];
	}

	power_s.regmap = syscon_regmap_lookup_by_phandle_args(np,
			"power-small-syscon", 2, args);
	if (IS_ERR(power_s.regmap))
		pr_warn("failed to get power-small syscon\n");
	else {
		power_s.reg = args[0];
		power_s.mask = args[1];
	}

	power_l.regmap = syscon_regmap_lookup_by_phandle_args(np,
			"power-large-syscon", 2, args);
	if (IS_ERR(power_l.regmap))
		pr_warn("failed to get power-large syscon\n");
	else {
		power_l.reg = args[0];
		power_l.mask = args[1];
	}

	power_iso.regmap = syscon_regmap_lookup_by_phandle_args(np,
			"power-iso-syscon", 2, args);
	if (IS_ERR(power_iso.regmap))
		pr_warn("failed to get power-iso syscon\n");
	else {
		power_iso.reg = args[0];
		power_iso.mask = args[1];
	}

	return 0;
}

static void dphy_glb_enable(struct dphy_context *ctx)
{
	regmap_update_bits(enable.regmap, enable.reg,
			enable.mask, enable.mask);
}

static void dphy_glb_disable(struct dphy_context *ctx)
{
	regmap_update_bits(enable.regmap, enable.reg,
			enable.mask, (u32)~enable.mask);
}

static void dphy_power_domain(struct dphy_context *ctx, int enable)
{
	if (enable) {
		regmap_update_bits(power_s.regmap, power_s.reg,
				power_s.mask, (u32)~power_s.mask);
		udelay(10);
		regmap_update_bits(power_l.regmap, power_l.reg,
				power_l.mask, (u32)~power_l.mask);
		regmap_update_bits(power_iso.regmap, power_iso.reg,
				power_iso.mask, (u32)~power_iso.mask);
	} else {
		regmap_update_bits(power_iso.regmap, power_iso.reg,
				power_iso.mask, power_iso.mask);
		regmap_update_bits(power_s.regmap, power_s.reg,
				power_s.mask, power_s.mask);
		udelay(10);
		regmap_update_bits(power_l.regmap, power_l.reg,
				power_l.mask, power_l.mask);
	}
}

const struct dphy_glb_ops sharkl3_dphy_glb_ops = {
	.parse_dt = dphy_glb_parse_dt,
	.enable = dphy_glb_enable,
	.disable = dphy_glb_disable,
	.power = dphy_power_domain,
};

MODULE_AUTHOR("Leon He <leon.he@unisoc.com>");
MODULE_DESCRIPTION("Unisoc SharkL3 DPHY global AHB regs low-level config");
MODULE_LICENSE("GPL v2");
