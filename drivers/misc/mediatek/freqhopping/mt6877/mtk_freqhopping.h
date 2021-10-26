/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT_FREQHOPPING_H__
#define __MT_FREQHOPPING_H__


enum FH_PLL_ID {
	FH_TOP_PLL0  = 0,       /*ARMPLL_LL */
	FH_TOP_PLL1  = 1,       /*ARMPLL_BL0 */
	FH_TOP_PLL2  = 2,       /*ARMPLL_B */
	FH_TOP_PLL3  = 3,       /*CCIPLL */
	FH_TOP_PLL4  = 4,       /*MEMPLL */
	FH_TOP_PLL5  = 5,       /*EMIPLL */
	FH_TOP_PLL6  = 6,       /*MPLL */
	FH_TOP_PLL7  = 7,       /*MMPLL */
	FH_TOP_PLL8  = 8,       /*MAINPLL */
	FH_TOP_PLL9  = 9,       /*MSDCPLL */
	FH_TOP_PLL10  = 10,     /*ADSPPLL */
	FH_TOP_PLL11  = 11,     /*IMGPLL */
	FH_TOP_PLL12  = 12,     /*TVDPLL */

	FH_GPU_PLL0  = 13,     /*MFGPLL1 */
	FH_GPU_PLL1  = 14,     /*MFGPLL2 */
	FH_GPU_PLL2  = 15,     /*MFGPLL3 */
	FH_GPU_PLL3  = 16,     /*MFGPLL4 */

	FH_APU_PLL0  = 17,     /*APUPLL */
	FH_APU_PLL1  = 18,     /*NPUPLL */
	FH_APU_PLL2  = 19,     /*APUPLL1 */
	FH_APU_PLL3  = 20,     /*APUPLL2 */
	FH_PLL_NUM,
};

extern int mt_dfs_general_pll(unsigned int pll_id, unsigned int target_dds);

#endif
