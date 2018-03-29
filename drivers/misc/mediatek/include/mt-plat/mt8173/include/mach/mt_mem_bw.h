/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __MT_MEM_BW_H__
#define __MT_MEM_BW_H__

#define DISABLE_FLIPPER_FUNC  0

typedef unsigned long long (*getmembw_func)(void);
extern void mt_getmembw_registerCB(getmembw_func pCB);

unsigned long long get_mem_bw(void);

#endif  /* !__MT_MEM_BW_H__ */
