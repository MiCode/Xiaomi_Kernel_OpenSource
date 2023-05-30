/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __COND_H__
#define __COND_H__

/* Definition about SPM_COND_CHECK_BLOCKED
 * bit [00 ~ 15]: cg blocking index
 * bit [16 ~ 29]: pll blocking index
 * bit [30]     : pll blocking information
 * bit [31]    : idle condition check fail
 */

#define SPM_COND_BLOCKED_CG_IDX		(0)
#define SPM_COND_BLOCKED_PLL_IDX	(16)
#define SPM_COND_BLOCKED_PLL		(1<<30L)
#define SPM_COND_CHECK_FAIL		(1<<31L)

enum PLAT_SPM_COND {
	PLAT_SPM_COND_MTCMOS_0 = 0,
	PLAT_SPM_COND_MTCMOS_1,
	PLAT_SPM_COND_CG_INFRA_0,
	PLAT_SPM_COND_CG_INFRA_1,
	PLAT_SPM_COND_CG_PERI_0,
	PLAT_SPM_COND_CG_PERI_1,
	PLAT_SPM_COND_CG_MMSYS_0,
	PLAT_SPM_COND_CG_MMSYS_1,
	PLAT_SPM_COND_CG_MDPSYS_0,
	PLAT_SPM_COND_MAX,
};

enum PLAT_SPM_PLL_COND {
	PLAT_SPM_COND_PLL_TVD = 0,
	PLAT_SPM_COND_PLL_UNIV,
	PLAT_SPM_COND_PLL_MSDC,
	PLAT_SPM_COND_PLL_IMG,
	PLAT_SPM_COND_PLL_MFG,
	PLAT_SPM_COND_PLL_MAX,
	PLAT_SPM_COND_PLL_APLL1 = PLAT_SPM_COND_PLL_MAX,
	PLAT_SPM_COND_PLL_APLL2,
};

#endif
