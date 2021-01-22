/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#ifndef _MTK_MDPM_
#define _MTK_MDPM_

enum mdpm_power_type {
	MAX_POWER = 0,
	AVG_POWER,
	POWER_TYPE_NUM
};

extern void init_md_section_level(enum pbm_kicker kicker, u32 *share_mem);
extern int get_md1_power(enum mdpm_power_type power_type, bool need_update);

#endif
