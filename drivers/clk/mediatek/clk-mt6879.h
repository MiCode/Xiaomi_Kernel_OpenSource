
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Owen Chen <owen.chen@mediatek.com>
 */

#ifndef __DRV_CLK_MT6879_H
#define __DRV_CLK_MT6879_H

#define MT_CCF_PLL_DISABLE	1
#define MT_CCF_MUX_DISABLE	1

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

#define MFGPLL1			"mfg_ao_mfgpll1"
#define MFGPLL2			"mfg_ao_mfgpll2"
#define MFGPLL3			"mfg_ao_mfgpll3"
#define MFGPLL4			"mfg_ao_mfgpll4"

#define MFGPLL1			"mfg_ao_mfgpll1"
#define MFGPLL2			"mfg_ao_mfgpll2"
#define MFGPLL3			"mfg_ao_mfgpll3"
#define MFGPLL4			"mfg_ao_mfgpll4"

#define MFGMUX1			"mfgmux1"
#define MFGMUX2			"mfgmux2"

#define MT6879_PLL_FMAX		(3800UL * MHZ)
#define MT6879_PLL_FMIN		(1500UL * MHZ)
#define MT6879_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _en_mask, _pll_en_bit,		\
			_pwr_reg, _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pll_en_bit = _pll_en_bit,				\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6879_PLL_FMAX,				\
		.fmin = MT6879_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6879_INTEGER_BITS,			\
	}

#define SUBSYS_MUX(_id, _name, _parents, _mux_ofs,		\
			_shift, _width, _ops) {		\
		.id = _id,						\
		.name = _name,						\
		.mux_ofs = _mux_ofs,					\
		.mux_shift = _shift,					\
		.mux_width = _width,					\
		.parent_names = _parents,				\
		.num_parents = ARRAY_SIZE(_parents),			\
		.flags = CLK_SET_RATE_PARENT,					\
		.ops = &clk_mux_ops,						\
	}

enum subsys_id {
	APMIXEDSYS = 0,
	MFG_PLL_CTRL = 1,
	APU_PLL_CTRL = 1,
	PLL_SYS_NUM = 2,
};

extern int clk_mt6879_mux_registration(enum subsys_id id,
		const struct mtk_mux *muxes,
		struct platform_device *pdev,
		int num_muxes);
extern int clk_mt6879_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls);

#endif/* __DRV_CLK_MT6879_H */
