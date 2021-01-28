/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */


#ifndef __MTK_LPM_PWR_GS_H__
#define __MTK_LPM_PWR_GS_H__

enum MTK_LPM_GS_COMPARER {
	MTK_LPM_GS_CMP_PMIC,
	MTK_LPM_GS_CMP_CLK,
	MTK_LPM_GS_CMP_PLL,
	MTK_LPM_GS_CMP_MAX
};

struct mtk_lpm_gs_cmp {
	int (*init)(void);
	void (*deinit)(void);
	int (*cmp_init)(void *data);
	int (*cmp)(int user);
	int (*cmp_by_type)(int user, unsigned int type);
};

int mtk_lpm_pwr_gs_compare_register(int comparer, struct mtk_lpm_gs_cmp *cmp);

void mtk_lpm_pwr_gs_compare_unregister(int comparer);

int mtk_lpm_pwr_gs_compare_init(int comparer, void *info);

int mtk_lpm_pwr_gs_compare(int comparer, int user);

int mtk_lpm_pwr_gs_compare_by_type(int comparer, int user, unsigned int type);

#endif
