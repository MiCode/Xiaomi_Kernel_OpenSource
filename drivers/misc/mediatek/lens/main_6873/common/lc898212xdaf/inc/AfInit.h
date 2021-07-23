/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __AFINIT__
#define __AFINIT__

extern void AfInit(unsigned char hall_bias, unsigned char hall_off);

extern void ServoOn(void);

extern unsigned int g_LC898212_SearchDir;

extern void RamWriteA(unsigned short addr, unsigned short data);

extern void RamReadA(unsigned short addr, unsigned short *data);

extern void RegWriteA(unsigned short addr, unsigned char data);

extern void RegReadA(unsigned short addr, unsigned char *data);

extern void WaitTime(unsigned short msec);

#endif /* __AFINIT__ */
