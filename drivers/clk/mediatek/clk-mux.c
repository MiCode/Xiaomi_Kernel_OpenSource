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

#if defined(CONFIG_MACH_MT6768)
void mm_polling(struct clk_hw *hw);
#endif

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

#if defined(CONFIG_MACH_MT6768)
		/*
		 * Workaround for mm dvfs. Poll mm rdma reg before
		 * clkmux switching.
		 */
		if (!strcmp(__clk_get_name(hw->clk), "mm_sel"))
			mm_polling(hw);
#endif

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
