/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __DRV_CLK_FIXUP_DIV_H
#define __DRV_CLK_FIXUP_DIV_H

#include <linux/clk-provider.h>

struct clk;

struct clk_fixup_div {
	int id;
	const char *name;
	const char *parent_name;

	void __iomem *reg_fixup;
	const struct clk_div_table *clk_div_table;

	struct clk_divider divider;
	const struct clk_ops *ops;

};

static inline struct clk_fixup_div *to_clk_fixup_div(struct clk_hw *hw)
{
	struct clk_divider *divider = to_clk_divider(hw);

	return container_of(divider, struct clk_fixup_div, divider);
}

struct clk *mtk_clk_fixup_divider(
		const char *name,
		const char *parent,
		unsigned long flags,
		void __iomem *reg,
		void __iomem *reg_fixup,
		u8 shift,
		u8 width,
		u8 clk_divider_flags,
		spinlock_t *lock);

#endif /* __DRV_CLK_FIXUP_DIV_H */
