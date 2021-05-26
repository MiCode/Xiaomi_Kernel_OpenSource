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

#ifndef _MT_RT_MON_H
#define _MT_RT_MON_H
#include <linux/sched.h>

#define MON_STOP 0
#define MON_START 1
#define MON_RESET 2

#ifdef CONFIG_MTK_RT_THROTTLE_MON
DECLARE_PER_CPU(struct mt_rt_mon_struct, mt_rt_mon_head);
DECLARE_PER_CPU(int, rt_mon_count);
DECLARE_PER_CPU(int, mt_rt_mon_enabled);
DECLARE_PER_CPU(unsigned long long, rt_start_ts);
DECLARE_PER_CPU(unsigned long long, rt_end_ts);
DECLARE_PER_CPU(unsigned long long, rt_dur_ts);

extern void save_mt_rt_mon_info(int cpu, u64 delta_exec, struct task_struct *p);
extern void mt_rt_mon_switch(int on, int cpu);
extern void mt_rt_mon_print_task(int cpu);
extern void mt_rt_mon_print_task_from_buffer(void);
extern void update_mt_rt_mon_start(int cpu, u64 delta_exec);
extern int mt_rt_mon_enable(int cpu);
#else
static inline void
save_mt_rt_mon_info(int cpu, u64 delta_exec, struct task_struct *p) {};
static inline void mt_rt_mon_switch(int on, int cpu) {};
static inline void mt_rt_mon_print_task(int cpu) {};
static inline void mt_rt_mon_print_task_from_buffer(void) {};
static inline void update_mt_rt_mon_start(int cpu, u64 delta_exec) {};
static inline int mt_rt_mon_enable(int cpu)
{
	return 0;
}
#endif /* CONFIG_MTK_RT_THROTTLE_MON */
#endif /* _MT_RT_MON_H */
