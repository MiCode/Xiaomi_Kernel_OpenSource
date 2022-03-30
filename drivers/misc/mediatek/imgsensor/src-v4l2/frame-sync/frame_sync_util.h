/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_UTIL_H
#define _FRAME_SYNC_UTIL_H


unsigned int calcLineTimeInNs(unsigned int pclk, unsigned int linelength);
unsigned int convert2TotalTime(unsigned int lineTimeInus, unsigned int time);
unsigned int convert2LineCount(unsigned int lineTimeInNs, unsigned int val);

unsigned int check_tick_b_after_a(unsigned int tick_a, unsigned int tick_b);

#endif
