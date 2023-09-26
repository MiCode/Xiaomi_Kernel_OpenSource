// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/slab.h>
#include <linux/mfd/syscon.h>

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
	u32 mask = 0;
	unsigned long flags = 0;
	int ret = 0;

	if (mux->gate_shift < 0)
		return 0;

	mask = BIT(mux->gate_shift);

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	ret = regmap_update_bits(mux->regmap, mux->mux_ofs, mask, 0);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return ret;
}

static void mtk_mux_disable(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = 0;
	unsigned long flags = 0;

	if (mux->gate_shift < 0)
		return;

	mask = BIT(mux->gate_shift);

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	regmap_update_bits(mux->regmap, mux->mux_ofs, mask, mask);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);
}

static int mtk_mux_enable_setclr(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 val = 0;
	unsigned long flags = 0;

	if (mux->gate_shift < 0)
		return 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	val = BIT(mux->gate_shift);
	regmap_write(mux->regmap, mux->clr_ofs,
			val);

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return 0;
}

static void mtk_mux_disable_setclr(struct clk_hw *hw)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	unsigned long flags = 0;

	if (mux->gate_shift < 0)
		return;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	regmap_write(mux->regmap, mux->set_ofs,
			BIT(mux->gate_shift));

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);
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
	u32 mask = GENMASK(mux->mux_width - 1, 0)
			<< mux->mux_shift;
	unsigned long flags = 0;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	regmap_update_bits(mux->regmap, mux->mux_ofs, mask,
		index << mux->mux_shift);

	if (mux->upd_shift >= 0)
		regmap_write(mux->regmap, mux->upd_ofs,
				BIT(mux->upd_shift));

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return 0;
}

static int mtk_mux_set_parent_setclr(struct clk_hw *hw, u8 index)
{
	struct mtk_clk_mux *mux = to_mtk_clk_mux(hw);
	u32 mask = GENMASK(mux->mux_width - 1, 0);
	u32 val = 0, orig = 0;
	unsigned long flags = 0;
	const char *name;

	if (mux->lock)
		spin_lock_irqsave(mux->lock, flags);
	else
		__acquire(mux->lock);

	regmap_read(mux->regmap, mux->mux_ofs, &orig);
	val = (orig & ~(mask << mux->mux_shift))
			| (index << mux->mux_shift);

	if (val != orig) {
		regmap_write(mux->regmap, mux->clr_ofs,
				mask << mux->mux_shift);
		regmap_write(mux->regmap, mux->set_ofs,
				index << mux->mux_shift);

#if defined(CONFIG_MACH_MT6768)
		/*
		 * Workaround for mm dvfs. Poll mm rdma reg before
		 * clkmux switching.
		 */

		if (hw && hw->clk) {
			name = __clk_get_name(hw->clk);
			if (name && !strcmp(name, "mm_sel"))
				mm_polling(hw);
		}
#endif

		if (mux->upd_shift >= 0)
			regmap_write(mux->regmap, mux->upd_ofs,
					BIT(mux->upd_shift));
	}

	if (mux->lock)
		spin_unlock_irqrestore(mux->lock, flags);
	else
		__release(mux->lock);

	return 0;
}

const struct clk_ops mtk_mux_ops = {
	.get_parent = mtk_mux_get_parent,
	.set_parent = mtk_mux_set_parent,
};

const struct clk_ops mtk_mux_clr_set_upd_ops = {
	.get_parent = mtk_mux_get_parent,
	.set_parent = mtk_mux_set_parent_setclr,
};

const struct clk_ops mtk_mux_gate_ops = {
	.enable = mtk_mux_enable,
	.disable = mtk_mux_disable,
	.is_enabled = mtk_mux_is_enabled,
	.get_parent = mtk_mux_get_parent,
	.set_parent = mtk_mux_set_parent,
	.determine_rate = NULL,
};

const struct clk_ops mtk_mux_gate_clr_set_upd_ops = {
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
	struct mtk_clk_mux *clk_mux = NULL;
	struct clk_init_data init = {};
	struct clk *clk;

	clk_mux = kzalloc(sizeof(*clk_mux), GFP_KERNEL);
	if (!clk_mux)
		return ERR_PTR(-ENOMEM);

	init.name = mux->name;
	init.flags = (mux->flags) | CLK_SET_RATE_PARENT;
	init.parent_names = mux->parent_names;
	init.num_parents = mux->num_parents;
	init.ops = mux->ops;

	clk_mux->regmap = regmap;
	clk_mux->name = mux->name;
	clk_mux->mux_ofs = mux->mux_ofs;
	clk_mux->set_ofs = mux->set_ofs;
	clk_mux->clr_ofs = mux->clr_ofs;
	clk_mux->upd_ofs = mux->upd_ofs;
	clk_mux->mux_shift = mux->mux_shift;
	clk_mux->mux_width = mux->mux_width;
	clk_mux->gate_shift = mux->gate_shift;
	clk_mux->upd_shift = mux->upd_shift;
	clk_mux->lock = lock;
	clk_mux->hw.init = &init;

	clk = clk_register(NULL, &clk_mux->hw);
	if (IS_ERR(clk)) {
		kfree(clk_mux);
		return clk;
	}

	return clk;
}

int mtk_clk_register_muxes(const struct mtk_mux *muxes,
			   int num, struct device_node *node,
			   spinlock_t *lock,
			   struct clk_onecell_data *clk_data)
{
	struct regmap *regmap;
	struct clk *clk;
	int i;

	regmap = syscon_node_to_regmap(node);
	if (IS_ERR(regmap)) {
		pr_notice("Cannot find regmap for %pOF: %ld\n", node,
		       PTR_ERR(regmap));
		return PTR_ERR(regmap);
	}

	for (i = 0; i < num; i++) {
		const struct mtk_mux *mux = &muxes[i];

		if (IS_ERR_OR_NULL(clk_data->clks[mux->id])) {
			clk = mtk_clk_register_mux(mux, regmap, lock);

			if (IS_ERR(clk)) {
				pr_notice("Failed to register clk %s: %ld\n",
				       mux->name, PTR_ERR(clk));
				continue;
			}

			clk_data->clks[mux->id] = clk;
		}
	}

	return 0;
}
