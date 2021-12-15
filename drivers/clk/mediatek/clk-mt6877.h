/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __DRV_CLK_MT6877_H
#define __DRV_CLK_MT6877_H

#define MT_CCF_PLL_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

#define MT6877_PLL_FMAX		(3800UL * MHZ)
#define MT6877_PLL_FMIN		(1500UL * MHZ)
#define MT6877_INTEGER_BITS	8

#define MFGPLL1			"mfg_ao_mfgpll1"
#define MFGPLL2			"mfg_ao_mfgpll2"
#define MFGPLL3			"mfg_ao_mfgpll3"
#define MFGPLL4			"mfg_ao_mfgpll4"

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL_PWR(_id, _name, _reg, _en_reg, _en_mask,		\
			_pwr_reg, _flags,		\
			_pd_reg, _pd_shift,			\
			_pcw_reg, _pcw_shift, _pcwbits,			\
			_pwr_stat) {					\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS),				\
		.fmax = MT6877_PLL_FMAX,				\
		.fmin = MT6877_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6877_INTEGER_BITS,			\
		.pwr_stat = _pwr_stat,					\
	}


enum subsys_id {
	APMIXEDSYS = 0,
	GPU_PLL_CTRL = 1,
	PLL_SYS_NUM = 2,
};

extern int clk_mt6877_pll_registration(enum subsys_id id,
		const struct mtk_pll_data *plls,
		struct platform_device *pdev,
		int num_plls);

#endif/* __DRV_CLK_MT6877_H */
