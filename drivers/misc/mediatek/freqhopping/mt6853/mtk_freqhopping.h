/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT_FREQHOPPING_H__
#define __MT_FREQHOPPING_H__

enum FH_PLL_ID {
	FH_PLL0  = 0,       /*ARMPLL_LL */
	FH_PLL1  = 1,       /*ARMPLL_BL0 */
	FH_PLL2  = 2,       /*ARMPLL_BL1 */
	FH_PLL3  = 3,       /*ARMPLL_BL2 */
	FH_PLL4  = 4,       /*NPUPLL */
	FH_PLL5  = 5,       /*CCIPLL */
	FH_PLL6  = 6,       /*MFGPLL */
	FH_PLL7  = 7,       /*MEMPLL */
	FH_PLL8  = 8,	    /*MPLL */
	FH_PLL9  = 9,	    /*MMPLL */
	FH_PLL10  = 10,     /*MAINPLL */
	FH_PLL11  = 11,     /*MSDCPLL */
	FH_PLL12  = 12,     /*ADSPPLL */
	FH_PLL13  = 13,     /*APUPLL */
	FH_PLL14  = 14,     /*TVDPLL */
	FH_PLL_NUM,
};

extern int mt_dfs_general_pll(unsigned int pll_id, unsigned int target_dds);

/* for build pass only */
struct freqhopping_ssc {
	unsigned int idx_pattern;
	unsigned int dt;
	unsigned int df;
	unsigned int upbnd;
	unsigned int lowbnd;
	unsigned int dds;
};
struct freqhopping_ioctl {
	unsigned int pll_id;
	struct freqhopping_ssc ssc_setting; /* used only when user-define */
	int result;
};

#endif	/* !__MT_FREQHOPPING_H__ */
