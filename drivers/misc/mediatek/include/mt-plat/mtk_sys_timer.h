/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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

