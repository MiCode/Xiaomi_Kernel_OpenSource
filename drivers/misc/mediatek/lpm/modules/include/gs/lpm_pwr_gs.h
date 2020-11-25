/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __LPM_PWR_GS_H__
#define __LPM_PWR_GS_H__

enum LPM_GS_COMPARER {
	LPM_GS_CMP_PMIC,
	LPM_GS_CMP_CLK,
	LPM_GS_CMP_PLL,
	LPM_GS_CMP_MAX
};

struct lpm_gs_cmp {
	int (*init)(void);
	void (*deinit)(void);
	int (*cmp_init)(void *data);
	int (*cmp)(int user);
	int (*cmp_by_type)(int user, unsigned int type);
};

int lpm_pwr_gs_compare_register(int comparer, struct lpm_gs_cmp *cmp);

void lpm_pwr_gs_compare_unregister(int comparer);

int lpm_pwr_gs_compare_init(int comparer, void *info);

int lpm_pwr_gs_compare(int comparer, int user);

int lpm_pwr_gs_compare_by_type(int comparer, int user, unsigned int type);

#endif
