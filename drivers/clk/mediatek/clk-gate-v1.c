/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/of_address.h>

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/clkdev.h>
#include <linux/ratelimit.h>

#include "clk-mtk-v1.h"
#include "clk-gate-v1.h"

#if !defined(MT_CCF_DEBUG) || !defined(MT_CCF_BRINGUP)
#if defined(CONFIG_ARCH_MT6755)
#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP  0
#elif defined(CONFIG_ARCH_MT6797)
#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP  0
#elif defined(CONFIG_ARCH_MT6757)
#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP  1
#else
#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP	0
#endif
#endif
/*
 * clk_gate
 */

static void cg_set_mask(struct mtk_clk_gate *cg, u32 mask)
{
	u32 r;

#if MT_CCF_DEBUG
	pr_debug_ratelimited("[CCF] %s: %s, mask=%u, bit=%u, flags=%u\n",
			__func__, __clk_get_name(cg->hw.clk), mask,
			cg->bit, cg->flags);
#endif /* MT_CCF_DEBUG */
	if (cg->flags & CLK_GATE_NO_SETCLR_REG) {
		r = readl_relaxed(cg->sta_addr) | mask;
		writel_relaxed(r, cg->sta_addr);
	} else
		writel_relaxed(mask, cg->set_addr);
}

static void cg_clr_mask(struct mtk_clk_gate *cg, u32 mask)
{
	u32 r;

#if MT_CCF_DEBUG
	pr_debug_ratelimited("[CCF] %s: %s, mask=%u, bit=%u, flags=%u\n",
			 __func__, __clk_get_name(cg->hw.clk), mask,
			cg->bit, cg->flags);
#endif /* MT_CCF_DEBUG */
	if (cg->flags & CLK_GATE_NO_SETCLR_REG) {
		r = readl_relaxed(cg->sta_addr) & ~mask;
		writel_relaxed(r, cg->sta_addr);
	} else
		writel_relaxed(mask, cg->clr_addr);
}

static int cg_enable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_gate *cg = to_clk_gate(hw);
	u32 mask = BIT(cg->bit);

#if MT_CCF_BRINGUP
	pr_debug_ratelimited("[CCF] %s: %s, bit: %u\n", __func__,
			__clk_get_name(hw->clk), cg->bit);
	return 0;
#endif /* MT_CCF_BRINGUP */
#if MT_CCF_DEBUG
	pr_debug_ratelimited("[CCF] %s: %s\n", __func__, __clk_get_name(hw->clk));
#endif

	mtk_clk_lock(flags);

	if (cg->flags & CLK_GATE_INVERSE)
		cg_set_mask(cg, mask);
	else
		cg_clr_mask(cg, mask);

	mtk_clk_unlock(flags);

	return 0;
}

static void cg_disable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	struct mtk_clk_gate *cg = to_clk_gate(hw);
	u32 mask = BIT(cg->bit);

#if MT_CCF_BRINGUP
	pr_debug_ratelimited("[CCF] %s: %s, bit: %u\n", __func__,
			__clk_get_name(hw->clk), cg->bit);
	return;
#endif /* MT_CCF_BRINGUP */
#if MT_CCF_DEBUG
	pr_debug_ratelimited("[CCF] %s: %s\n", __func__, __clk_get_name(hw->clk));
#endif

	mtk_clk_lock(flags);

	if (cg->flags & CLK_GATE_INVERSE)
		cg_clr_mask(cg, mask);
	else
		cg_set_mask(cg, mask);

	mtk_clk_unlock(flags);
}

static int cg_is_enabled(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_clk_gate(hw);
	u32 mask;
	u32 val;
	int r;

#if MT_CCF_BRINGUP
	pr_debug_ratelimited("[CCF] %s: %s\n", __func__, __clk_get_name(hw->clk));
	return 1;
#endif /* MT_CCF_BRINGUP */

	mask = BIT(cg->bit);
	val = mask & readl(cg->sta_addr);

	r = (cg->flags & CLK_GATE_INVERSE) ? (val != 0) : (val == 0);

#if MT_CCF_DEBUG
	pr_debug_ratelimited("[CCF] %s: %d, %s, bit[%d]\n", __func__, r,
		 __clk_get_name(hw->clk), (int)cg->bit);
#endif

	return r;
}

static const struct clk_ops mtk_clk_gate_ops = {
	.is_enabled	= cg_is_enabled,
	.enable		= cg_enable,
	.disable	= cg_disable,
};

struct clk *mtk_clk_register_gate(
		const char *name,
		const char *parent_name,
		void __iomem *set_addr,
		void __iomem *clr_addr,
		void __iomem *sta_addr,
		u8 bit,
		u32 flags)
{
	struct mtk_clk_gate *cg;
	struct clk *clk;
	struct clk_init_data init;

#if MT_CCF_DEBUG
	pr_debug_ratelimited("[CCF] name: %s, bit: %d\n", name, (int)bit);
#endif /* MT_CCF_DEBUG */

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = &mtk_clk_gate_ops;

	cg->set_addr = set_addr;
	cg->clr_addr = clr_addr;
	cg->sta_addr = sta_addr;
	cg->bit = bit;
	cg->flags = flags;

	cg->hw.init = &init;

	clk = clk_register(NULL, &cg->hw);
	if (IS_ERR(clk))
		kfree(cg);

	return clk;
}
