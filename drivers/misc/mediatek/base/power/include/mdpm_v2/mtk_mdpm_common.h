/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */


#ifndef _MTK_MDPM_COMMON_H_
#define _MTK_MDPM_COMMON_H_

extern int mt_mdpm_debug;

#ifdef MD_POWER_UT
extern u32 fake_share_reg;
extern u32 fake_share_mem[SHARE_MEM_SIZE];
#endif

#endif
