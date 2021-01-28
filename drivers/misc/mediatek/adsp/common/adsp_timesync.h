/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Celine Liu <Celine.liu@mediatek.com>
 */

#ifndef _ADSP_TIMESYNC_H_
#define _ADSP_TIMESYNC_H_

#ifdef CONFIG_ARM64
#define IOMEM(a)       ((void __force __iomem *)((a)))
#endif

#define adsp_reg_sync_writel(v, a) \
	do { \
		__raw_writel((v), IOMEM(a)); \
		dsb(sy); \
	} while (0)

#define TIMESYNC_TAG           "[ADSP_TS]"

#define TIMESYNC_MAX_VER       (0xFF)

#define TIMESYNC_FLAG_SYNC     (1 << 0)
#define TIMESYNC_FLAG_ASYNC    (1 << 1)
#define TIMESYNC_FLAG_FREEZE   (1 << 2)
#define TIMESYNC_FLAG_UNFREEZE (1 << 3)

/* sched_clock wrap time is 4398 seconds for arm arch timer
 * applying a period less than it for tinysys timesync
 */
#define TIMESYNC_WRAP_TIME     (4000*NSEC_PER_SEC)

void adsp_timesync_suspend(u8 fz);
void adsp_timesync_resume(void);
int __init adsp_timesync_init(void);

#endif // _ADSP_TIMESYNC_H_

