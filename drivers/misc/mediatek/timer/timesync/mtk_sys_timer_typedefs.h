/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_SYS_TIMER_TYPES_H__
#define __MTK_SYS_TIMER_TYPES_H__

#include <linux/spinlock_types.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

/*
 * Regular timesync by synchronizing time each 60 minutes
 * to avoid overflow issue in cyc_to_ns().
 *
 * This is not necessary now because kernel already has regular
 * "sched_clock_timer" for the same purpose.
 */

/* #define SYS_TIMER_TIMESYNC_REGULAR */

#define SYS_TIMER_CLK_RATE         (13000000)

#define TIMESYNC_BASE_TICK         (0)
#define TIMESYNC_BASE_TS           (8)
#define TIMESYNC_BASE_FREEZE       (16)
#define TIMESYNC_MAX_SEC           (5000)
#define TIMESYNC_MAX_VER           (0x7)
#define TIMESYNC_REGULAR_SYNC_SEC  (60 * 60 * HZ)

#define TIMESYNC_HEADER_FREEZE_OFS (31)
#define TIMESYNC_HEADER_FREEZE     (1 << TIMESYNC_HEADER_FREEZE_OFS)
#define TIMESYNC_HEADER_VER_OFS    (28)
#define TIMESYNC_HEADER_VER_MASK   (TIMESYNC_MAX_VER << TIMESYNC_HEADER_VER_OFS)

#define SYS_TIMER_CNTCV_L          (0x08)
#define SYS_TIMER_CNTCV_H          (0x0C)

struct sys_timer_timesync_context_t {
	void __iomem *ram_base;
	spinlock_t lock;
	u32 mult;
	u32 shift;
	u64 base_tick;
	u64 base_ts;
	struct work_struct work;

#ifdef CONFIG_DEBUG_FS
	struct dentry *dbgfs_root;
	struct dentry *dbgfs_debug;
#endif

#ifdef SYS_TIMER_TIMESYNC_REGULAR
	struct timer_list timer;
#endif

	u8 base_ver;
	u8 base_fz;
	u8 enabled;

	/* support on-chip sysram update */
	u8 support_sysram;
};



#endif

