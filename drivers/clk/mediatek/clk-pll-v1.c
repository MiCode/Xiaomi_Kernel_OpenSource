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

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/clkdev.h>

#include "clk-mtk-v1.h"
#include "clk-pll-v1.h"

/*
 * clk_pll
 */

struct clk *mtk_clk_register_pll(
		const char *name,
		const char *parent_name,
		u32 *base_addr,
		u32 *pwr_addr,
		u32 en_mask,
		u32 flags,
		const struct clk_ops *ops)
{
	struct mtk_clk_pll *pll;
	struct clk_init_data init;
	struct clk *clk;

	pr_debug("name: %s\n", name);

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->base_addr = base_addr;
	pll->pwr_addr = pwr_addr;
	pll->en_mask = en_mask;
	pll->flags = flags;
	pll->hw.init = &init;

	init.name = name;
	init.ops = ops;
	init.flags = CLK_IGNORE_UNUSED;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(NULL, &pll->hw);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}
