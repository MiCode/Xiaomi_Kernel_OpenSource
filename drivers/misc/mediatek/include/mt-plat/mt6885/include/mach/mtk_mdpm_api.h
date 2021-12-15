/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_MDPM_API_H_
#define _MTK_MDPM_API_H_

#include <mtk_pmic.h>
#include <mtk_pbm.h>

enum mdpm_power_type {
	MAX_POWER = 0,
	AVG_POWER,
	POWER_TYPE_NUM
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
extern int get_md1_power(enum mdpm_power_type power_type, bool need_update);

#endif
