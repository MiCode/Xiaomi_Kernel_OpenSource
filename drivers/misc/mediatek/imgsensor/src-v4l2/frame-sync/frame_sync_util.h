/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_UTIL_H
#define _FRAME_SYNC_UTIL_H


unsigned int convert2TotalTime(unsigned int lineTimeInus, unsigned int time);
unsigned int convert2LineCount(unsigned int lineTimeInNs, unsigned int val);

#endif
