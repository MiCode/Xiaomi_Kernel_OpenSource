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

#include <linux/regmap.h>
#include <linux/clk-provider.h>

struct clk;

/*
 * define pwr status information.
 * including offsets/mask.
 */

struct pwr_status {
	u32 pwr_ofs;
	u32 pwr2_ofs;
	s32 other_ofs;
	u32 mask;
	u32 val;
};

struct mtk_clk_gate {
	struct clk_hw	hw;
	struct regmap	*regmap;
	int		set_ofs;
	int		clr_ofs;
	int		sta_ofs;
	u8		bit;
	struct pwr_status	*pwr_stat;
	struct regmap	*pwr_regmap;
};

static inline struct mtk_clk_gate *to_mtk_clk_gate(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_gate, hw);
}

extern const struct clk_ops mtk_clk_gate_ops_setclr;
extern const struct clk_ops mtk_clk_gate_ops_setclr_dummy;
extern const struct clk_ops mtk_clk_gate_ops_setclr_inv;
extern const struct clk_ops mtk_clk_gate_ops_setclr_inv_dummy;
extern const struct clk_ops mtk_clk_gate_ops_no_setclr;
extern const struct clk_ops mtk_clk_gate_ops_no_setclr_inv;

struct clk *mtk_clk_register_gate(
		const char *name,
		const char *parent_name,
		struct regmap *regmap,
		int set_ofs,
		int clr_ofs,
		int sta_ofs,
		u8 bit,
		const struct clk_ops *ops,
		unsigned long flags,
		struct pwr_status *pwr_stat,
		struct regmap *pwr_regmap);

#define GATE_MTK_FLAGS(_id, _name, _parent, _regs, _shift,	\
			_ops, _flags) {				\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.regs = _regs,					\
		.shift = _shift,				\
		.ops = _ops,					\
		.flags = _flags,				\
	}

#define GATE_MTK(_id, _name, _parent, _regs, _shift, _ops)	\
	GATE_MTK_FLAGS(_id, _name, _parent, _regs, _shift, _ops, 0)

#define GATE_PWR_STAT(_pwr_ofs, _pwr2_ofs, _other_ofs, _mask, _val) {	\
		.pwr_ofs = _pwr_ofs,				\
		.pwr2_ofs = _pwr2_ofs,				\
		.other_ofs = _other_ofs,			\
		.mask = _mask,				\
		.val = _val,				\
}

#endif /* __DRV_CLK_GATE_H */
