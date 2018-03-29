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

#ifndef __DRV_CLK_PLL_H
#define __DRV_CLK_PLL_H

/*
 * This is a private header file. DO NOT include it except clk-*.c.
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>

struct mtk_clk_pll {
	struct clk_hw	hw;
	void __iomem	*base_addr;
	void __iomem	*pwr_addr;
	u32		en_mask;
	u32		flags;
};

#define to_mtk_clk_pll(_hw) container_of(_hw, struct mtk_clk_pll, hw)

#define HAVE_RST_BAR	BIT(0)
#define HAVE_PLL_HP	BIT(1)
#define HAVE_FIX_FRQ	BIT(2)
#define PLL_AO		BIT(3)

struct clk *mtk_clk_register_pll(
		const char *name,
		const char *parent_name,
		u32 *base_addr,
		u32 *pwr_addr,
		u32 en_mask,
		u32 flags,
		const struct clk_ops *ops);

#endif /* __DRV_CLK_PLL_H */
