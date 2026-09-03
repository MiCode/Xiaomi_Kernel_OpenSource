/* SPDX-License-Identifier: GPL-2.0 */
//
// Spreadtrum pll clock driver
//
// Copyright (C) 2015~2017 Spreadtrum, Inc.
// Author: Chunyan Zhang <chunyan.zhang@spreadtrum.com>

#ifndef _SPRD_PLL_H_
#define _SPRD_PLL_H_

#include "common.h"

#define INVALID_MAX_IBIAS		0xff
#define INVALID_MAX_FREQ		0xffffffff
#define INVALID_MAX_VCO_SEL		0xff

struct reg_cfg {
	u32 val;
	u32 msk;
};

struct clk_bit_field {
	u8 shift;
	u8 width;
};

struct freq_table {
	u32 ibias;
	u64 max_freq;
	u32 vco_sel;
};

enum {
	PLL_LOCK_DONE,
	PLL_DIV_S,
	PLL_MOD_EN,
	PLL_SDM_EN,
	PLL_REFIN,
	PLL_IBIAS,
	PLL_N,
	PLL_NINT,
	PLL_KINT,
	PLL_PREDIV,
	PLL_POSTDIV,
	PLL_REFDIV,
	PLL_VCOSEL,

	PLL_FACT_MAX
};

/*
 * struct sprd_pll - definition of adjustable pll clock
 *
 * @reg:	registers used to set the configuration of pll clock,
 *		reg[0] shows how many registers this pll clock uses.
 * @ftable:	pll freq table
 * @udelay	delay time after setting rate
 * @factors	used to calculate the pll clock rate
 * @fvco:	fvco threshold rate
 * @fflag:	fvco flag
 */
struct sprd_pll {
	u32 regs_num;
	const struct freq_table *ftable;
	const struct clk_bit_field *factors;
	u16 udelay;
	u16 k1;
	u16 k2;
	u16 fflag;
	u64 fvco;

	struct sprd_clk_common	common;
};

#define SPRD_PLL_HW_INIT_FN(_struct, _name, _parent, _reg,	\
			    _regs_num, _ftable, _factors,	\
			    _udelay, _k1, _k2, _fflag,		\
			    _fvco, _fn)				\
	struct sprd_pll _struct = {				\
		.regs_num	= _regs_num,			\
		.ftable		= _ftable,			\
		.factors	= _factors,			\
		.udelay		= _udelay,			\
		.k1		= _k1,				\
		.k2		= _k2,				\
		.fflag		= _fflag,			\
		.fvco		= _fvco,			\
		.common		= {				\
			.regmap		= NULL,			\
			.reg		= _reg,			\
			.hw.init	= _fn(_name, _parent,	\
					      &sprd_pll_ops, 0),\
		},						\
	}

#define SPRD_PLL_WITH_ITABLE_K_FVCO(_struct, _name, _parent, _reg,	\
				    _regs_num, _ftable, _factors,	\
				    _udelay, _k1, _k2, _fflag, _fvco)	\
	SPRD_PLL_HW_INIT_FN(_struct, _name, _parent, _reg, _regs_num,	\
			    _ftable, _factors, _udelay, _k1, _k2,	\
			    _fflag, _fvco, CLK_HW_INIT)

#define SPRD_PLL_WITH_ITABLE_K(_struct, _name, _parent, _reg,		\
			       _regs_num, _ftable, _factors,		\
			       _udelay, _k1, _k2)			\
	SPRD_PLL_WITH_ITABLE_K_FVCO(_struct, _name, _parent, _reg,	\
				    _regs_num, _ftable, _factors,	\
				    _udelay, _k1, _k2, 0, 0)

#define SPRD_PLL_WITH_ITABLE_1K(_struct, _name, _parent, _reg,		\
				_regs_num, _ftable, _factors, _udelay)	\
	SPRD_PLL_WITH_ITABLE_K_FVCO(_struct, _name, _parent, _reg,	\
				    _regs_num, _ftable, _factors,	\
				    _udelay, 1000, 1000, 0, 0)

#define SPRD_PLL_FW_NAME(_struct, _name, _parent, _reg, _regs_num,	\
			 _ftable, _factors, _udelay, _k1, _k2,		\
			 _fflag, _fvco)					\
	SPRD_PLL_HW_INIT_FN(_struct, _name, _parent, _reg, _regs_num,	\
			    _ftable, _factors, _udelay, _k1, _k2,	\
			    _fflag, _fvco, CLK_HW_INIT_FW_NAME)

#define SPRD_PLL_HW(_struct, _name, _parent, _reg, _regs_num, _ftable,	\
		    _factors, _udelay, _k1, _k2, _fflag, _fvco)		\
	SPRD_PLL_HW_INIT_FN(_struct, _name, _parent, _reg, _regs_num,	\
			    _ftable, _factors, _udelay, _k1, _k2,	\
			    _fflag, _fvco, CLK_HW_INIT_HW)

static inline struct sprd_pll *hw_to_sprd_pll(struct clk_hw *hw)
{
	struct sprd_clk_common *common = hw_to_sprd_clk_common(hw);

	return container_of(common, struct sprd_pll, common);
}

extern const struct clk_ops sprd_pll_ops;

#endif /* _SPRD_PLL_H_ */
