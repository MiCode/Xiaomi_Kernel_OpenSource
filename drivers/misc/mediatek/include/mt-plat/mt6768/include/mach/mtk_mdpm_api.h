/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_MDPM_API_H_
#define _MTK_MDPM_API_H_

#include <mach/mtk_pmic.h>
#include <mach/mtk_pbm.h>

enum power_category {
	MAX_POWER = 0,
	AVG_POWER,
	POWER_CATEGORY_NUM
};

enum pbm_kicker {
	KR_DLPT,		/* 0 */
	KR_MD1,			/* 1 */
	KR_MD3,			/* 2 */
	KR_CPU,			/* 3 */
	KR_GPU,			/* 4 */
	KR_FLASH		/* 5 */
};

extern void init_md_section_level(enum pbm_kicker kicker);
extern int get_md1_power(unsigned int power_category, bool need_update);

#endif
