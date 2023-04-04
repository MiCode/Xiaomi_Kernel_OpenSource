/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _CLK_FIXED_FACTOR_MT6739_H
#define _CLK_FIXED_FACTOR_MT6739_H

#include <linux/platform_device.h>
#include "clk-mtk.h"

struct mtk_fixed_factor_pdn {
	int id;
	const char *name;
	const char *parent_name;
	int mult;
	int div;
	int shift;
	int pd_reg;
};

#define FACTOR_PDN(_id, _name, _parent, _mult, _div, _shift, _pd_reg) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.mult = _mult,				\
		.div = _div,				\
		.shift = _shift,				\
		.pd_reg = _pd_reg,				\
	}


struct clk;

void mtk_clk_register_factors_pdn(const struct mtk_fixed_factor_pdn *clks,
	int num, struct clk_onecell_data *clk_data, void __iomem *base);

struct clk *mtk_clk_register_fixed_factor_pdn(struct device *dev,
	const char *name,
	const char *parent_name, unsigned long flags,
	unsigned int mult, unsigned int div, unsigned int shift,
	unsigned int pd_reg, void __iomem *base);

#endif /* _CLK_FIXED_FACTOR_MT6739_H */
