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

#ifndef __DRV_CLK_GATE_H
#define __DRV_CLK_GATE_H

/*
 * This is a private header file. DO NOT include it except clk-*.c.
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>

struct mtk_clk_gate {
	struct clk_hw	hw;
	void __iomem	*set_addr;
	void __iomem	*clr_addr;
	void __iomem	*sta_addr;
	u8		bit;
	u32		flags;
};

#define to_clk_gate(_hw) container_of(_hw, struct mtk_clk_gate, hw)

#define CLK_GATE_INVERSE	BIT(0)
#define CLK_GATE_NO_SETCLR_REG	BIT(1)

struct clk *mtk_clk_register_gate(
		const char *name,
		const char *parent_name,
		void __iomem *set_addr,
		void __iomem *clr_addr,
		void __iomem *sta_addr,
		u8 bit,
		u32 flags);

#endif /* __DRV_CLK_GATE_H */
