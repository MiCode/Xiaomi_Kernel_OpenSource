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

#ifndef __DRV_CLK_MTK_H
#define __DRV_CLK_MTK_H

#include <linux/regmap.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>

struct clk;

struct clk *mtk_clk_register_fixed_factor_pdn(struct device *dev,
	const char *name,
	const char *parent_name, unsigned long flags,
	unsigned int mult, unsigned int div, unsigned int shift,
	unsigned int pd_reg, void __iomem *base);


#define MAX_MUX_GATE_BIT	31
#define INVALID_MUX_GATE_BIT	(MAX_MUX_GATE_BIT + 1)
#define INVALID_OFS		-1
#define INVALID_SHFT		-1
#define INVALID_WIDTH		-1
#define MHZ (1000 * 1000)

struct mtk_fixed_clk {
	int id;
	const char *name;
	const char *parent;
	unsigned long rate;
};

#define FIXED_CLK(_id, _name, _parent, _rate) {		\
		.id = _id,				\
		.name = _name,				\
		.parent = _parent,			\
		.rate = _rate,				\
	}

void mtk_clk_register_fixed_clks(const struct mtk_fixed_clk *clks,
		int num, struct clk_onecell_data *clk_data);

struct mtk_fixed_factor {
	int id;
	const char *name;
	const char *parent_name;
	int mult;
	int div;
};

struct mtk_fixed_factor_pdn {
	int id;
	const char *name;
	const char *parent_name;
	int mult;
	int div;
	int shift;
	int pd_reg;
};

#define FACTOR(_id, _name, _parent, _mult, _div) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.mult = _mult,				\
		.div = _div,				\
	}

#define FACTOR_PDN(_id, _name, _parent, _mult, _div, _shift, _pd_reg) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.mult = _mult,				\
		.div = _div,				\
		.shift = _shift,				\
		.pd_reg = _pd_reg,				\
	}

void mtk_clk_register_factors(const struct mtk_fixed_factor *clks,
		int num, struct clk_onecell_data *clk_data);

void mtk_clk_register_factors_pdn(const struct mtk_fixed_factor_pdn *clks,
		int num, struct clk_onecell_data *clk_data, void __iomem *base);

struct mtk_composite {
	int id;
	const char *name;
	const char * const *parent_names;
	const char *parent;
	unsigned long flags;

	uint32_t mux_reg;
	uint32_t divider_reg;
	uint32_t gate_reg;

	signed char mux_shift;
	signed char mux_width;
	signed char gate_shift;

	signed char divider_shift;
	signed char divider_width;

	signed char num_parents;
};

struct mtk_mux {
	int id;
	const char *name;
	const char * const *parent_names;
	unsigned int flags;

	u32 mux_ofs;
	u32 set_ofs;
	u32 clr_ofs;
	u32 upd_ofs;

	signed char mux_shift;
	signed char mux_width;
	signed char gate_shift;
	signed char upd_shift;

	const struct clk_ops *ops;

	signed char num_parents;
};

/*
 * In case the rate change propagation to parent clocks is undesirable,
 * this macro allows to specify the clock flags manually.
 */
#define CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs, _mux_set_ofs,\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd, _flags, _ops) {			\
		.id = _id,						\
		.name = _name,						\
		.mux_ofs = _mux_ofs,					\
		.set_ofs = _mux_set_ofs,				\
		.clr_ofs = _mux_clr_ofs,				\
		.upd_ofs = _upd_ofs,					\
		.mux_shift = _shift,					\
		.mux_width = _width,					\
		.gate_shift = _gate,					\
		.upd_shift = _upd,					\
		.parent_names = _parents,				\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = _flags,					\
		.ops = &_ops,						\
	}

#define MUX_CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs, _mux_set_ofs,\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd, _flags)			\
		CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs,	\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd, _flags,			\
			mtk_mux_clr_set_upd_ops)

#define MUX_CLR_SET_UPD(_id, _name, _parents, _mux_ofs, _mux_set_ofs,	\
			_mux_clr_ofs, _shift, _width, _gate,		\
			_upd_ofs, _upd)				\
		MUX_CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs,	\
			_mux_set_ofs, _mux_clr_ofs, _shift, _width,	\
			_gate, _upd_ofs, _upd, CLK_SET_RATE_PARENT)

#define MUX_UPD(_id, _name, _parents, _mux_ofs, _shift, _width, _gate,	\
			_upd_ofs, _upd)				\
		CLR_SET_UPD_FLAGS(_id, _name, _parents, _mux_ofs,	\
			INVALID_OFS, INVALID_OFS, _shift, _width,	\
			_gate, _upd_ofs, _upd, CLK_SET_RATE_PARENT,	\
			mtk_mux_upd_ops)

#define MUX_GATE_FLAGS(_id, _name, _parents, _reg, _shift, _width,	\
			_gate, _flags) {				\
		.id = _id,						\
		.name = _name,						\
		.mux_reg = _reg,					\
		.mux_shift = _shift,					\
		.mux_width = _width,					\
		.gate_reg = _reg,					\
		.gate_shift = _gate,					\
		.divider_shift = -1,					\
		.parent_names = _parents,				\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = _flags,					\
	}

/*
 * Unless necessary, all MUX_GATE clocks propagate rate changes to their
 * parent clock by default.
 */
#define MUX_GATE(_id, _name, _parents, _reg, _shift, _width, _gate)	\
	MUX_GATE_FLAGS(_id, _name, _parents, _reg, _shift, _width,	\
		_gate, CLK_SET_RATE_PARENT)

#define MUX(_id, _name, _parents, _reg, _shift, _width)		\
	MUX_GATE(_id, _name, _parents, _reg, _shift, _width, INVALID_SHFT)

#define DIV_GATE(_id, _name, _parent, _gate_reg, _gate_shift, _div_reg,	\
					_div_width, _div_shift) {	\
		.id = _id,						\
		.parent = _parent,					\
		.name = _name,						\
		.divider_reg = _div_reg,				\
		.divider_shift = _div_shift,				\
		.divider_width = _div_width,				\
		.gate_reg = _gate_reg,					\
		.gate_shift = _gate_shift,				\
		.mux_shift = -1,					\
		.flags = 0,						\
	}

int mtk_clk_register_muxes(const struct mtk_mux *muxes,
			   int num, struct device_node *node,
			   spinlock_t *lock,
			   struct clk_onecell_data *clk_data);

struct clk *mtk_clk_register_composite(const struct mtk_composite *mc,
		void __iomem *base, spinlock_t *lock);

void mtk_clk_register_composites(const struct mtk_composite *mcs,
		int num, void __iomem *base, spinlock_t *lock,
		struct clk_onecell_data *clk_data);

struct mtk_gate_regs {
	u32 sta_ofs;
	u32 clr_ofs;
	u32 set_ofs;
};

struct mtk_gate {
	int id;
	const char *name;
	const char *parent_name;
	const struct mtk_gate_regs *regs;
	int shift;
	const struct clk_ops *ops;
	unsigned int flags;
};

int mtk_clk_register_gates(struct device_node *node,
			const struct mtk_gate *clks, int num,
			struct clk_onecell_data *clk_data);

struct mtk_clk_divider {
	int id;
	const char *name;
	const char *parent_name;
	unsigned long flags;

	u32 div_reg;
	u32 div_reg_fixup;
	unsigned char div_shift;
	unsigned char div_width;
	unsigned char clk_divider_flags;
	const struct clk_div_table *clk_div_table;
};

#define DIV_ADJ(_id, _name, _parent, _reg, _shift, _width) {	\
		.id = _id,					\
		.name = _name,					\
		.parent_name = _parent,				\
		.div_reg = _reg,				\
		.div_shift = _shift,				\
		.div_width = _width,				\
}

void mtk_clk_register_dividers(const struct mtk_clk_divider *mcds,
			int num, void __iomem *base, spinlock_t *lock,
				struct clk_onecell_data *clk_data);

void mtk_clk_register_fixup_dividers(const struct mtk_clk_divider *mcds,
			int num, void __iomem *base, spinlock_t *lock,
				struct clk_onecell_data *clk_data);

struct clk_onecell_data *mtk_alloc_clk_data(unsigned int clk_num);

#define HAVE_RST_BAR	BIT(0)
#define PLL_AO		BIT(1)

struct mtk_pll_div_table {
	u32 div;
	unsigned long freq;
};

struct mtk_pll_data {
	int id;
	const char *name;
	uint32_t reg;
	uint32_t pwr_reg;
	uint32_t en_mask;
	uint32_t pd_reg;
	uint32_t tuner_reg;
	uint32_t tuner_en_reg;
	uint8_t tuner_en_bit;
	int pd_shift;
	unsigned int flags;
	const struct clk_ops *ops;
	u32 rst_bar_mask;
	unsigned long fmax;
	unsigned long fmin;
	uint32_t pcwchgreg;
	int pcwbits;
	int pcwibits;
	uint32_t pcw_reg;
	int pcw_shift;
	const struct mtk_pll_div_table *div_table;
	const char *parent_name;
};

void mtk_clk_register_plls(struct device_node *node,
		const struct mtk_pll_data *plls, int num_plls,
		struct clk_onecell_data *clk_data);

struct clk *mtk_clk_register_ref2usb_tx(const char *name,
			const char *parent_name, void __iomem *reg);

#ifdef CONFIG_RESET_CONTROLLER
void mtk_register_reset_controller(struct device_node *np,
			unsigned int num_regs, int regofs);
#else
static inline void mtk_register_reset_controller(struct device_node *np,
			unsigned int num_regs, int regofs)
{
}
#endif

int mtk_is_pll_enable(void);
int mtk_is_cg_enable(void);

#endif /* __DRV_CLK_MTK_H */
