// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include "clk-mtk.h"
#include "clk-fixup-div.h"

#define div_mask(d)	((1 << (d)) - 1)

static unsigned long clk_fixup_div_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct clk_fixup_div *fixup_div = to_clk_fixup_div(hw);
	unsigned int val;

	val = readl(fixup_div->reg_fixup) >> fixup_div->divider.shift;
	val &= div_mask(fixup_div->divider.width);
	pr_debug("%s: val = %x\n", __func__, val);

	return divider_recalc_rate(hw, parent_rate, val,
				   fixup_div->clk_div_table,
				   fixup_div->divider.flags,
				   fixup_div->divider.width);
}

static long clk_fixup_div_round_rate(struct clk_hw *hw, unsigned long rate,
				unsigned long *prate)
{
	struct clk_fixup_div *fixup_div = to_clk_fixup_div(hw);

	pr_debug("%s: rate = %lu, prate = %lu\n", __func__, rate, *prate);

	return divider_round_rate(hw, rate, prate, fixup_div->clk_div_table,
				  fixup_div->divider.width,
				  fixup_div->divider.flags);
}

static int clk_fixup_div_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct clk_fixup_div *fixup_div = to_clk_fixup_div(hw);
	struct clk_divider *div = to_clk_divider(hw);
	unsigned long flags = 0;
	int val, value;

	value = divider_get_val(rate, parent_rate, div->table,
				div->width, div->flags);
	if (value < 0)
		return value;

	spin_lock_irqsave(div->lock, flags);

	val = readl(fixup_div->reg_fixup);
	val &= ~(div_mask(div->width) << div->shift);
	val |= (u32)value << div->shift;

	writel(val, div->reg);
	writel(val, fixup_div->reg_fixup);

	pr_debug("%s: %s: reg_fixup = %x\n",
		__func__, clk_hw_get_name(hw), readl(fixup_div->reg_fixup));

	spin_unlock_irqrestore(div->lock, flags);

	return 0;
}

static const struct clk_ops clk_fixup_div_ops = {
	.recalc_rate = clk_fixup_div_recalc_rate,
	.round_rate = clk_fixup_div_round_rate,
	.set_rate = clk_fixup_div_set_rate,
};

struct clk *mtk_clk_fixup_divider(const char *name, const char *parent,
				  unsigned long flags, void __iomem *reg,
				  void __iomem *reg_fixup, u8 shift, u8 width,
				  u8 clk_divider_flags, spinlock_t *lock)
{
	struct clk_fixup_div *fixup_div;
	struct clk *clk;
	struct clk_init_data init = {};

	fixup_div = kzalloc(sizeof(*fixup_div), GFP_KERNEL);
	if (!fixup_div)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.ops = &clk_fixup_div_ops;
	init.flags = flags | CLK_IS_BASIC;
	init.parent_names = parent ? &parent : NULL;
	init.num_parents = parent ? 1 : 0;

	fixup_div->reg_fixup = reg_fixup;
	fixup_div->divider.reg = reg;
	fixup_div->divider.flags = clk_divider_flags;
	fixup_div->divider.shift = shift;
	fixup_div->divider.width = width;
	fixup_div->divider.lock = lock;
	fixup_div->divider.hw.init = &init;
	fixup_div->ops = &clk_divider_ops;

	clk = clk_register(NULL, &fixup_div->divider.hw);
	if (IS_ERR(clk))
		kfree(fixup_div);

	return clk;
}
