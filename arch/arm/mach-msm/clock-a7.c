/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/iopoll.h>
#include <linux/spinlock.h>

#include <mach/clock-generic.h>

#define UPDATE_CHECK_MAX_LOOPS 200

struct cortex_reg_data {
	u32 cmd_offset;
	u32 update_mask;
	u32 poll_mask;
};

#define DIV_REG(x) ((x)->base + (x)->div_offset)
#define SRC_REG(x) ((x)->base + (x)->src_offset)
#define CMD_REG(x) ((x)->base + \
			((struct cortex_reg_data *)(x)->priv)->cmd_offset)

static int update_config(struct mux_div_clk *md)
{
	u32 regval, count;
	struct cortex_reg_data *r = md->priv;

	/* Update the configuration */
	regval = readl_relaxed(CMD_REG(md));
	regval |= r->update_mask;
	writel_relaxed(regval, CMD_REG(md));

	/* Wait for update to take effect */
	for (count = UPDATE_CHECK_MAX_LOOPS; count > 0; count--) {
		if (!(readl_relaxed(CMD_REG(md)) &
				r->poll_mask))
			return 0;
		udelay(1);
	}

	CLK_WARN(&md->c, true, "didn't update its configuration.");

	return -EINVAL;
}

static void cortex_get_config(struct mux_div_clk *md, u32 *src_sel, u32 *div)
{
	u32 regval;

	regval = readl_relaxed(DIV_REG(md));
	regval &= (md->div_mask << md->div_shift);
	*div = regval >> md->div_shift;
	*div = max((u32)1, (*div + 1) / 2);

	regval = readl_relaxed(SRC_REG(md));
	regval &= (md->src_mask << md->src_shift);
	*src_sel = regval >> md->src_shift;
}

static int cortex_set_config(struct mux_div_clk *md, u32 src_sel, u32 div)
{
	u32 regval;

	div = div ? ((2 * div) - 1) : 0;
	regval = readl_relaxed(DIV_REG(md));
	regval &= ~(md->div_mask  << md->div_shift);
	regval |= div << md->div_shift;
	writel_relaxed(regval, DIV_REG(md));

	regval = readl_relaxed(SRC_REG(md));
	regval &= ~(md->src_mask  << md->src_shift);
	regval |= src_sel << md->src_shift;
	writel_relaxed(regval, SRC_REG(md));

	return update_config(md);
}

static int cortex_enable(struct mux_div_clk *md)
{
	u32 src_sel = parent_to_src_sel(md->parents, md->num_parents,
							md->c.parent);
	return cortex_set_config(md, src_sel, md->data.div);
}

static void cortex_disable(struct mux_div_clk *md)
{
	u32 src_sel = parent_to_src_sel(md->parents, md->num_parents,
							md->safe_parent);
	cortex_set_config(md, src_sel, md->safe_div);
}

static bool cortex_is_enabled(struct mux_div_clk *md)
{
	return true;
}

struct mux_div_ops cortex_mux_div_ops = {
	.set_src_div = cortex_set_config,
	.get_src_div = cortex_get_config,
	.is_enabled = cortex_is_enabled,
	.enable = cortex_enable,
	.disable = cortex_disable,
};
