/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Celine Liu <Celine.liu@mediatek.com>
 */

#ifndef _ADSP_TIMESYNC_H_
#define _ADSP_TIMESYNC_H_

/* sched_clock wrap time is 4398 seconds for arm arch timer
 * applying a period less than it for tinysys timesync
 */
#define TIMESYNC_WRAP_TIME_MS  (4390 * 1000)

enum {
	APTIME_UNFREEZE  = 0,
	APTIME_FREEZE    = 1,
};

void adsp_timesync_suspend(u32 fz);
void adsp_timesync_resume(void);
int adsp_timesync_init(void);

#endif // _ADSP_TIMESYNC_H_

