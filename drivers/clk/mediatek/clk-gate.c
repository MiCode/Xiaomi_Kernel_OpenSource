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

#include "clk-mtk.h"
#include "clk-gate.h"

#if defined(CONFIG_MACH_MT6757)
#define CLK_GATE_INVERSE	BIT(0)
#define CLK_GATE_NO_SETCLR_REG	BIT(1)
#endif

static int mtk_cg_bit_is_cleared(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;

#if !defined(CONFIG_MACH_MT6757)
	regmap_read(cg->regmap, cg->sta_ofs, &val);
#else
#ifdef CONFIG_ARM64
	val = readl_relaxed((void __iomem *)((unsigned long long int)cg->regmap
						+ cg->sta_ofs));
#else
	val = readl_relaxed((void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));
#endif
#endif

	val &= BIT(cg->bit);

	return val == 0;
}

static int mtk_cg_bit_is_set(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 val;
#if !defined(CONFIG_MACH_MT6757)
	regmap_read(cg->regmap, cg->sta_ofs, &val);
#else
#ifdef CONFIG_ARM64
	val = readl_relaxed((void __iomem *)
		((unsigned long long int)cg->regmap + cg->sta_ofs));
#else
	val = readl_relaxed((void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));
#endif
#endif
	val &= BIT(cg->bit);

	return val != 0;
}

#if defined(CONFIG_MACH_MT6757)
static void cg_set_mask(struct mtk_clk_gate *cg, u32 mask)
{
	u32 r;

#ifdef CONFIG_ARM64
	if (cg->flags & CLK_GATE_NO_SETCLR_REG) {
		r = readl_relaxed((void __iomem *)
			((unsigned long long int)cg->regmap + cg->sta_ofs));
		r = r | mask;
		writel_relaxed(r, (void __iomem *)
			((unsigned long long int)cg->regmap + cg->sta_ofs));

		r = readl_relaxed((void __iomem *)
			((unsigned long long int)cg->regmap + cg->sta_ofs));
	} else
		writel_relaxed(mask, (void __iomem *)
			((unsigned long long int)cg->regmap + cg->set_ofs));
#else
	if (cg->flags & CLK_GATE_NO_SETCLR_REG) {
		r = readl_relaxed((void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));
		r = r | mask;
		writel_relaxed(r, (void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));

		r = readl_relaxed((void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));
	} else
		writel_relaxed(mask, (void __iomem *)((unsigned int)cg->regmap
						+ cg->set_ofs));
#endif
}

static void cg_clr_mask(struct mtk_clk_gate *cg, u32 mask)
{
	u32 r;

#ifdef CONFIG_ARM64
	if (cg->flags & CLK_GATE_NO_SETCLR_REG) {
		r = readl_relaxed((void __iomem *)
			((unsigned long long int)cg->regmap + cg->sta_ofs));
		r = r & ~mask;
		writel_relaxed(r, (void __iomem *)
			((unsigned long long int)cg->regmap + cg->sta_ofs));

		r = readl_relaxed((void __iomem *)
			((unsigned long long int)cg->regmap + cg->sta_ofs));
	} else
		writel_relaxed(mask, (void __iomem *)
			((unsigned long long int)cg->regmap + cg->clr_ofs));
#else
	if (cg->flags & CLK_GATE_NO_SETCLR_REG) {
		r = readl_relaxed((void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));
		r = r & ~mask;
		writel_relaxed(r, (void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));

		r = readl_relaxed((void __iomem *)((unsigned int)cg->regmap
						+ cg->sta_ofs));
	} else
		writel_relaxed(mask, (void __iomem *)((unsigned int)cg->regmap
						+ cg->clr_ofs));
#endif
}
#endif

static void mtk_cg_set_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
#if defined(CONFIG_MACH_MT6757)
	u32 mask = BIT(cg->bit);
#endif
#if !defined(CONFIG_MACH_MT6757)
	regmap_write(cg->regmap, cg->set_ofs, BIT(cg->bit));
#else
	if (cg->flags & CLK_GATE_INVERSE)
		cg_clr_mask(cg, mask);
	else
		cg_set_mask(cg, mask);
#endif
}

static void mtk_cg_clr_bit(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
#if defined(CONFIG_MACH_MT6757)
	u32 mask = BIT(cg->bit);
#endif
#if !defined(CONFIG_MACH_MT6757)
	regmap_write(cg->regmap, cg->clr_ofs, BIT(cg->bit));
#else
	if (cg->flags & CLK_GATE_INVERSE)
		cg_set_mask(cg, mask);
	else
		cg_clr_mask(cg, mask);
#endif
}

static void mtk_cg_set_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, cgbit);
}

static void mtk_cg_clr_bit_no_setclr(struct clk_hw *hw)
{
	struct mtk_clk_gate *cg = to_mtk_clk_gate(hw);
	u32 cgbit = BIT(cg->bit);

	regmap_update_bits(cg->regmap, cg->sta_ofs, cgbit, 0);
}

static int mtk_cg_enable(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);

	return 0;
}

static void mtk_cg_disable(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);
}

static int mtk_cg_enable_inv(struct clk_hw *hw)
{
	mtk_cg_set_bit(hw);

	return 0;
}

static void mtk_cg_disable_inv(struct clk_hw *hw)
{
	mtk_cg_clr_bit(hw);
}

static int mtk_cg_enable_no_setclr(struct clk_hw *hw)
{
	mtk_cg_clr_bit_no_setclr(hw);

	return 0;
}

static void mtk_cg_disable_no_setclr(struct clk_hw *hw)
{
	mtk_cg_set_bit_no_setclr(hw);
}

static int mtk_cg_enable_inv_no_setclr(struct clk_hw *hw)
{
	mtk_cg_set_bit_no_setclr(hw);

	return 0;
}

static void mtk_cg_disable_inv_no_setclr(struct clk_hw *hw)
{
	mtk_cg_clr_bit_no_setclr(hw);
}

const struct clk_ops mtk_clk_gate_ops_setclr = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable,
	.disable	= mtk_cg_disable,
};

const struct clk_ops mtk_clk_gate_ops_setclr_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv,
	.disable	= mtk_cg_disable_inv,
};

const struct clk_ops mtk_clk_gate_ops_no_setclr = {
	.is_enabled	= mtk_cg_bit_is_cleared,
	.enable		= mtk_cg_enable_no_setclr,
	.disable	= mtk_cg_disable_no_setclr,
};

const struct clk_ops mtk_clk_gate_ops_no_setclr_inv = {
	.is_enabled	= mtk_cg_bit_is_set,
	.enable		= mtk_cg_enable_inv_no_setclr,
	.disable	= mtk_cg_disable_inv_no_setclr,
};

struct clk *mtk_clk_register_gate(
		const char *name,
		const char *parent_name,
		struct regmap *regmap,
		int set_ofs,
		int clr_ofs,
		int sta_ofs,
		u8 bit,
		const struct clk_ops *ops
#if defined(CONFIG_MACH_MT6757)
		, u32 flags
#endif
		)
{
	struct mtk_clk_gate *cg;
	struct clk *clk;
	struct clk_init_data init = {};

	cg = kzalloc(sizeof(*cg), GFP_KERNEL);
	if (!cg)
		return ERR_PTR(-ENOMEM);

	init.name = name;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = parent_name ? &parent_name : NULL;
	init.num_parents = parent_name ? 1 : 0;
	init.ops = ops;

	cg->regmap = regmap;
	cg->set_ofs = set_ofs;
	cg->clr_ofs = clr_ofs;
	cg->sta_ofs = sta_ofs;
	cg->bit = bit;
#if defined(CONFIG_MACH_MT6757)
	cg->flags = flags;
#endif

	cg->hw.init = &init;

	clk = clk_register(NULL, &cg->hw);
	if (IS_ERR(clk))
		kfree(cg);

	return clk;
}
