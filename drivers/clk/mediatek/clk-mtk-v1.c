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

#include "clk-mtk-v1.h"

#if !defined(MT_CCF_DEBUG) || !defined(MT_CCF_BRINGUP)
#define MT_CCF_DEBUG	0
#define MT_CCF_BRINGUP	0
#endif

static DEFINE_SPINLOCK(clk_ops_lock);

spinlock_t *get_mtk_clk_lock(void)
{
	return &clk_ops_lock;
}

/*
 * clk_mux
 */

struct clk *mtk_clk_register_mux(
		const char *name,
		const char **parent_names,
		u8 num_parents,
		void __iomem *base_addr,
		u8 shift,
		u8 width,
		u8 gate_bit)
{
	struct clk *clk;
	struct clk_mux *mux;
	struct clk_gate *gate = NULL;
	struct clk_hw *gate_hw = NULL;
	const struct clk_ops *gate_ops = NULL;
	u32 mask = BIT(width) - 1;

#if MT_CCF_DEBUG
	pr_debug("name: %s, num_parents: %d, gate_bit: %d\n",
		name, (int)num_parents, (int)gate_bit);
#endif /* MT_CCF_DEBUG */

	mux = kzalloc(sizeof(struct clk_mux), GFP_KERNEL);
	if (!mux)
		return ERR_PTR(-ENOMEM);

	mux->reg = base_addr;
	mux->mask = mask;
	mux->shift = shift;
	mux->flags = 0;
	mux->lock = &clk_ops_lock;

	if (gate_bit <= MAX_MUX_GATE_BIT) {
		gate = kzalloc(sizeof(struct clk_gate), GFP_KERNEL);
		if (!gate) {
			kfree(mux);
			return ERR_PTR(-ENOMEM);
		}

		gate->reg = base_addr;
		gate->bit_idx = gate_bit;
		gate->flags = CLK_GATE_SET_TO_DISABLE;
		gate->lock = &clk_ops_lock;

		gate_hw = &gate->hw;
		gate_ops = &clk_gate_ops;
	}

	clk = clk_register_composite(NULL, name, parent_names, num_parents,
		&mux->hw, &clk_mux_ops,
		NULL, NULL,
		gate_hw, gate_ops,
		CLK_IGNORE_UNUSED);

	if (IS_ERR(clk)) {
		kfree(gate);
		kfree(mux);
	}

	return clk;
}
