/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Wendell Lin <wendell.lin@mediatek.com>
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
#include <linux/slab.h>

#include "clk-mtk.h"
#include "clk-mux.h"

#if defined(CONFIG_MACH_MT6739)
#define WORKAROUND_318_WARNING	1
struct mtk_mux_upd_data {
	struct clk_hw hw;
	void __iomem *base;

	u32 mux_ofs;
	u32 upd_ofs;

	s8 mux_shift;
	s8 mux_width;
	s8 gate_shift;
	s8 upd_shift;

	spinlock_t *lock;
};
struct mtk_mux_clr_set_upd_data {
	struct clk_hw hw;
	void __iomem *base;

	u32 mux_ofs;
	u32 mux_set_ofs;
	u32 mux_clr_ofs;
	u32 upd_ofs;

	s8 mux_shift;
	s8 mux_width;
	s8 gate_shift;
	s8 upd_shift;

	spinlock_t *lock;
};

static inline struct mtk_mux_upd_data *to_mtk_mux_upd_data(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_mux_upd_data, hw);
}

static inline struct mtk_mux_clr_set_upd_data *to_mtk_mux_clr_set_upd_data(
	struct clk_hw *hw)
{
	return container_of(hw, struct mtk_mux_clr_set_upd_data, hw);
}

static int mtk_mux_upd_enable(struct clk_hw *hw)
{
	struct mtk_mux_upd_data *mux = to_mtk_mux_upd_data(hw);
	u32 val, orig;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = clk_readl(mux->base + mux->mux_ofs);
	orig = val;
	val &= ~BIT(mux->gate_shift);

	if (val != orig) {
		clk_writel(val, mux->base + mux->mux_ofs);

		if (mux->upd_shift >= 0)
			clk_writel(BIT(mux->upd_shift),
				mux->base + mux->upd_ofs);
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);

	return 0;
}

static int mtk_mux_clr_set_upd_enable(struct clk_hw *hw)
{
	struct mtk_mux_clr_set_upd_data *mux = to_mtk_mux_clr_set_upd_data(hw);
	u32 val, orig;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = clk_readl(mux->base + mux->mux_ofs);
	orig = val;
	val &= ~BIT(mux->gate_shift);

	if (val != orig) {
#if defined(CONFIG_MACH_MT6799)
		if ((strcmp(__clk_get_name(mux->hw.clk), "venc_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "mfg_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "camtg_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "i2c_sel")))
			clk_writel(val, mux->base + mux->mux_ofs);
		else {
			clk_writel(BIT(mux->gate_shift),
				mux->base + mux->mux_clr_ofs);
		}
#else
			clk_writel(val, mux->base + mux->mux_ofs);
#endif
		if (mux->upd_shift >= 0)
			clk_writel(BIT(mux->upd_shift),
				mux->base + mux->upd_ofs);
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);

	return 0;
}

static void mtk_mux_upd_disable(struct clk_hw *hw)
{
	struct mtk_mux_upd_data *mux = to_mtk_mux_upd_data(hw);
	u32 val, orig;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = clk_readl(mux->base + mux->mux_ofs);
	orig = val;
	val |= BIT(mux->gate_shift);

	if (val != orig) {
		clk_writel(val, mux->base + mux->mux_ofs);

		if (mux->upd_shift >= 0)
			clk_writel(BIT(mux->upd_shift),
				mux->base + mux->upd_ofs);
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
}

static void mtk_mux_clr_set_upd_disable(struct clk_hw *hw)
{
	struct mtk_mux_clr_set_upd_data *mux = to_mtk_mux_clr_set_upd_data(hw);
	u32 val, orig;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = clk_readl(mux->base + mux->mux_ofs);
	orig = val;
	val |= BIT(mux->gate_shift);

	if (val != orig) {
#if defined(CONFIG_MACH_MT6799)
		if ((strcmp(__clk_get_name(mux->hw.clk), "venc_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "mfg_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "camtg_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "i2c_sel")))
			clk_writel(val, mux->base + mux->mux_ofs);
		else {
			writel(BIT(mux->gate_shift),
				mux->base + mux->mux_set_ofs);
		}
#else
		clk_writel(val, mux->base + mux->mux_ofs);
#endif
		if (mux->upd_shift >= 0)
			clk_writel(BIT(mux->upd_shift),
				mux->base + mux->upd_ofs);
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
}

static int mtk_mux_upd_is_enabled(struct clk_hw *hw)
{
	struct mtk_mux_upd_data *mux = to_mtk_mux_upd_data(hw);

	if (mux->gate_shift < 0)
		return true;

	return (clk_readl(mux->base + mux->mux_ofs) &
		BIT(mux->gate_shift)) == 0;
}

static int mtk_mux_clr_set_upd_is_enabled(struct clk_hw *hw)
{
	struct mtk_mux_clr_set_upd_data *mux = to_mtk_mux_clr_set_upd_data(hw);

	if (mux->gate_shift < 0)
		return true;

	return (clk_readl(mux->base + mux->mux_ofs) &
		BIT(mux->gate_shift)) == 0;
}

static u8 mtk_mux_upd_get_parent(struct clk_hw *hw)
{
	struct mtk_mux_upd_data *mux = to_mtk_mux_upd_data(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val;

	val = clk_readl(mux->base + mux->mux_ofs) >> mux->mux_shift;
	val &= mask;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static u8 mtk_mux_clr_set_upd_get_parent(struct clk_hw *hw)
{
	struct mtk_mux_clr_set_upd_data *mux = to_mtk_mux_clr_set_upd_data(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val;

	val = clk_readl(mux->base + mux->mux_ofs) >> mux->mux_shift;
	val &= mask;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static int mtk_mux_upd_set_parent(struct clk_hw *hw, u8 index)
{
	struct mtk_mux_upd_data *mux = to_mtk_mux_upd_data(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val, orig;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = clk_readl(mux->base + mux->mux_ofs);
	orig = val;
	val &= ~(mask << mux->mux_shift);

	val |= index << mux->mux_shift;

	if (val != orig) {
		clk_writel(val, mux->base + mux->mux_ofs);

		if (mux->upd_shift >= 0)
			clk_writel(BIT(mux->upd_shift),
				mux->base + mux->upd_ofs);
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);

	return 0;
}

static int mtk_mux_clr_set_upd_set_parent(struct clk_hw *hw, u8 index)
{
	struct mtk_mux_clr_set_upd_data *mux = to_mtk_mux_clr_set_upd_data(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val, orig;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = clk_readl(mux->base + mux->mux_ofs);
	orig = val;
	val &= ~(mask << mux->mux_shift);
	val |= index << mux->mux_shift;

	if (val != orig) {
#if defined(CONFIG_MACH_MT6799)
		if ((strcmp(__clk_get_name(mux->hw.clk), "venc_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "mfg_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "camtg_sel")) &&
			(strcmp(__clk_get_name(mux->hw.clk), "i2c_sel")))
			writel(val, mux->base + mux->mux_ofs);
		else {
			val = (mask << mux->mux_shift);
			writel(val, mux->base + mux->mux_clr_ofs);

			val = (index << mux->mux_shift);
			writel(val, mux->base + mux->mux_set_ofs);
		}
#else
		val = (mask << mux->mux_shift);
		writel(val, mux->base + mux->mux_clr_ofs);

		val = (index << mux->mux_shift);
		writel(val, mux->base + mux->mux_set_ofs);
#endif
		if (mux->upd_shift >= 0)
			clk_writel(BIT(mux->upd_shift),
				mux->base + mux->upd_ofs);
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);

	return 0;
}

const struct clk_ops mtk_mux_upd_ops = {
	.is_enabled = mtk_mux_upd_is_enabled,
	.get_parent = mtk_mux_upd_get_parent,
	.set_parent = mtk_mux_upd_set_parent,
};

const struct clk_ops mtk_mux_clr_set_upd_ops = {
	.is_enabled = mtk_mux_clr_set_upd_is_enabled,
	.get_parent = mtk_mux_clr_set_upd_get_parent,
	.set_parent = mtk_mux_clr_set_upd_set_parent,
};

const struct clk_ops mtk_mux_upd_gate_ops = {
	.enable = mtk_mux_upd_enable,
	.disable = mtk_mux_upd_disable,
	.is_enabled = mtk_mux_upd_is_enabled,
	.get_parent = mtk_mux_upd_get_parent,
	.set_parent = mtk_mux_upd_set_parent,
};

const struct clk_ops mtk_mux_clr_set_upd_gate_ops = {
	.enable = mtk_mux_clr_set_upd_enable,
	.disable = mtk_mux_clr_set_upd_disable,
	.is_enabled = mtk_mux_clr_set_upd_is_enabled,
	.get_parent = mtk_mux_clr_set_upd_get_parent,
	.set_parent = mtk_mux_clr_set_upd_set_parent,
};

struct clk * __init mtk_clk_register_mux_upd(const struct mtk_mux_upd *mu,
		void __iomem *base, spinlock_t *lock)
{
	struct clk *clk;
	struct mtk_mux_upd_data *mux;
	struct clk_init_data init;

	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	init.name = mu->name;
	init.flags = CLK_SET_RATE_PARENT;
#if WORKAROUND_318_WARNING
	init.parent_names = (const char **)mu->parent_names;
#else
	init.parent_names = mu->parent_names;
#endif
	init.num_parents = mu->num_parents;

	if (mu->gate_shift < 0)
		init.ops = &mtk_mux_upd_ops;
	else
		init.ops = &mtk_mux_upd_gate_ops;
	mux->base = base;
	mux->mux_ofs = mu->mux_ofs;
	mux->upd_ofs = mu->upd_ofs;
	mux->mux_shift = mu->mux_shift;
	mux->mux_width = mu->mux_width;
	mux->gate_shift = mu->gate_shift;
	mux->upd_shift = mu->upd_shift;
	mux->lock = lock;

	mux->hw.init = &init;

	clk = clk_register(NULL, &mux->hw);
	if (IS_ERR(clk))
		kfree(mux);

	return clk;
}

void __init mtk_clk_register_mux_upds(const struct mtk_mux_upd *mus,
		int num, void __iomem *base, spinlock_t *lock,
		struct clk_onecell_data *clk_data)
{
	struct clk *clk;
	int i;

	for (i = 0; i < num; i++) {
		const struct mtk_mux_upd *mu = &mus[i];

		clk = mtk_clk_register_mux_upd(mu, base, lock);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					mu->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[mu->id] = clk;
	}
}

struct clk * __init mtk_clk_register_mux_clr_set_upd(
	const struct mtk_mux_clr_set_upd *mu,
		void __iomem *base, spinlock_t *lock)
{
	struct clk *clk;
	struct mtk_mux_clr_set_upd_data *mux;
	struct clk_init_data init;

	mux = kzalloc(sizeof(*mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	init.name = mu->name;
	init.flags = CLK_SET_RATE_PARENT;
#if WORKAROUND_318_WARNING
	init.parent_names = (const char **)mu->parent_names;
#else
	init.parent_names = mu->parent_names;
#endif
	init.num_parents = mu->num_parents;

	if (mu->gate_shift < 0)
		init.ops = &mtk_mux_clr_set_upd_ops;
	else
		init.ops = &mtk_mux_clr_set_upd_gate_ops;

	mux->base = base;
	mux->mux_ofs = mu->mux_ofs;
	mux->mux_set_ofs = mu->mux_set_ofs;
	mux->mux_clr_ofs = mu->mux_clr_ofs;
	mux->upd_ofs = mu->upd_ofs;
	mux->mux_shift = mu->mux_shift;
	mux->mux_width = mu->mux_width;
	mux->gate_shift = mu->gate_shift;
	mux->upd_shift = mu->upd_shift;
	mux->lock = lock;

	mux->hw.init = &init;

	clk = clk_register(NULL, &mux->hw);
	if (IS_ERR(clk))
		kfree(mux);

	return clk;
}

void __init mtk_clk_register_mux_clr_set_upds(
	const struct mtk_mux_clr_set_upd *mus,
		int num, void __iomem *base, spinlock_t *lock,
		struct clk_onecell_data *clk_data)
{
	struct clk *clk;
	int i;

	for (i = 0; i < num; i++) {
		const struct mtk_mux_clr_set_upd *mu = &mus[i];

		clk = mtk_clk_register_mux_clr_set_upd(mu, base, lock);

		if (IS_ERR(clk)) {
			pr_err("Failed to register clk %s: %ld\n",
					mu->name, PTR_ERR(clk));
			continue;
		}

		if (clk_data)
			clk_data->clks[mu->id] = clk;
	}
}
#else
static inline struct mtk_clk_mux
	*to_mtk_clk_mux(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_mux, hw);
}

static int mtk_mux_enable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = BIT(mux->gate_shift);
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	regmap_update_bits(mux->regmap, mux->mux_ofs, mask, 0);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	return 0;
}

static void mtk_mux_disable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = BIT(mux->gate_shift);
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	regmap_update_bits(mux->regmap, mux->mux_ofs, mask, mask);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
}

static int mtk_mux_enable_setclr(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = BIT(mux->gate_shift);
	regmap_write(mux->regmap, mux->mux_clr_ofs, val);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	return 0;
}

static void mtk_mux_disable_setclr(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	val = BIT(mux->gate_shift);
	regmap_write(mux->regmap, mux->mux_set_ofs, val);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
}

static int mtk_mux_is_enabled(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0;

	if (mux->gate_shift < 0)
		return true;

	regmap_read(mux->regmap, mux->mux_ofs, &val);

	return (val & BIT(mux->gate_shift)) == 0;
}

static u8 mtk_mux_get_parent(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	int num_parents = clk_hw_get_num_parents(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val = 0;

	regmap_read(mux->regmap, mux->mux_ofs, &val);
	val = (val >> mux->mux_shift) & mask;

	if (val >= num_parents)
		return -EINVAL;

	return val;
}

static int mtk_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val, orig = 0;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	regmap_read(mux->regmap, mux->mux_ofs, &val);
	orig = val;
	val &= ~(mask << mux->mux_shift);
	val |= index << mux->mux_shift;

	if (val != orig) {
		regmap_write(mux->regmap, mux->mux_ofs, val);

		if (mux->upd_shift >= 0)
			regmap_write(mux->regmap, mux->upd_ofs,
				     BIT(mux->upd_shift));
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);

	return 0;
}

static int mtk_mux_set_parent_setclr(struct clk_hw *hw, u8 index)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val, orig = 0;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);

	regmap_read(mux->regmap, mux->mux_ofs, &val);
	orig = val;
	val &= ~(mask << mux->mux_shift);
	val |= index << mux->mux_shift;

	if (val != orig) {
		val = (mask << mux->mux_shift);
		regmap_write(mux->regmap, mux->mux_clr_ofs, val);
		val = (index << mux->mux_shift);
		regmap_write(mux->regmap, mux->mux_set_ofs, val);

		if (mux->upd_shift >= 0)
			regmap_write(mux->regmap, mux->upd_ofs,
				     BIT(mux->upd_shift));
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);

	return 0;
}

const struct clk_ops mtk_mux_upd_ops = {
	.enable = mtk_mux_enable,
	.disable = mtk_mux_disable,
	.is_enabled = mtk_mux_is_enabled,
	.get_parent = mtk_mux_get_parent,
	.set_parent = mtk_mux_set_parent,
	.determine_rate = NULL,
};

const struct clk_ops mtk_mux_clr_set_upd_ops = {
	.enable = mtk_mux_enable_setclr,
	.disable = mtk_mux_disable_setclr,
	.is_enabled = mtk_mux_is_enabled,
	.get_parent = mtk_mux_get_parent,
	.set_parent = mtk_mux_set_parent_setclr,
	.determine_rate = NULL,
};
#endif
struct clk *mtk_clk_register_mux(const struct mtk_mux *mux,
				 struct regmap *regmap,
				 spinlock_t *lock)
{
	struct clk *clk;
	struct clk_init_data init;
	struct mtk_clk_mux *mtk_mux = NULL;
	int ret;

	mtk_mux = kzalloc(sizeof(*mtk_mux), GFP_KERNEL);
	if (!mtk_mux)
		return ERR_PTR(-ENOMEM);

	init.name = mux->name;
	init.flags = (mux->flags) | CLK_SET_RATE_PARENT;
	init.parent_names = mux->parent_names;
	init.num_parents = mux->num_parents;
	init.ops = mux->ops;

	mtk_mux->regmap = regmap;
	mtk_mux->name = mux->name;
	mtk_mux->mux_ofs = mux->mux_ofs;
	mtk_mux->mux_set_ofs = mux->set_ofs;
	mtk_mux->mux_clr_ofs = mux->clr_ofs;
	mtk_mux->upd_ofs = mux->upd_ofs;
	mtk_mux->mux_shift = mux->mux_shift;
	mtk_mux->mux_width = mux->mux_width;
	mtk_mux->gate_shift = mux->gate_shift;
	mtk_mux->upd_shift = mux->upd_shift;

	mtk_mux->lock = lock;
	mtk_mux->hw.init = &init;

	clk = clk_register(NULL, &mtk_mux->hw);
	if (IS_ERR(clk)) {
		ret = PTR_ERR(clk);
		goto err_out;
	}

	return clk;
err_out:
	kfree(mtk_mux);

	return ERR_PTR(ret);
}
