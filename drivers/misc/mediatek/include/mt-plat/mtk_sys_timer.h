/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#define SYS_TIMER_TIMESYNC_FLAG_SYNC     (1 << 0)
#define SYS_TIMER_TIMESYNC_FLAG_ASYNC    (1 << 1)
#define SYS_TIMER_TIMESYNC_FLAG_FREEZE   (1 << 2)
#define SYS_TIMER_TIMESYNC_FLAG_UNFREEZE (1 << 3)

#ifdef CONFIG_MTK_TIMER_TIMESYNC
extern void sys_timer_timesync_sync_base(unsigned int flag);
extern u64  sys_timer_timesync_tick_to_sched_clock(u64 tick);
#else
extern void sys_timer_timesync_sync_base(unsigned int flag) { return; };
extern u64  sys_timer_timesync_tick_to_sched_clock(u64 tick) { return 0; };
#endif

