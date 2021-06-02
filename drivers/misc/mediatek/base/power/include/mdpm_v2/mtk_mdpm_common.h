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

#ifndef _MTK_MDPM_COMMON_H_
#define _MTK_MDPM_COMMON_H_

extern int mt_mdpm_debug;

#ifdef MD_POWER_UT
extern u32 fake_share_reg;
extern u32 fake_share_mem[SHARE_MEM_SIZE];
#endif

#endif
