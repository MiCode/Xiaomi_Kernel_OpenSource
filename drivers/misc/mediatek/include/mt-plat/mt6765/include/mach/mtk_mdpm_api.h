/*
 * Copyright (C) 2017 MediaTek Inc.
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
extern int get_md1_power(unsigned int power_category);

#endif
