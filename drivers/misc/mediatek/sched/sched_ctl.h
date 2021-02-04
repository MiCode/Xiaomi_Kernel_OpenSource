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


#ifdef CONFIG_MTK_SCHED_BOOST
/* For multi-scheduling boost support */
enum {
	SCHED_NO_BOOST = 0,
	SCHED_ALL_BOOST,
	SCHED_FG_BOOST,
	SCHED_UNKNOWN_BOOST
};

#define SCHED_PREFER_NONE   0
#define SCHED_PREFER_BIG    1
#define SCHED_PREFER_LITTLE 2
#define SCHED_PREFER_END    3

extern void set_user_space_global_cpuset
		(struct cpumask *global_cpus, int cgroup_id);
extern void unset_user_space_global_cpuset(int cgroup_id);
extern int sched_scheduler_switch(enum SCHED_LB_TYPE new_sched);
int set_sched_boost(unsigned int val);
#endif

extern int display_set_wait_idle_time(unsigned int wait_idle_time);

#ifdef CONFIG_SCHED_TUNE
extern int prefer_idle_for_perf_idx(int idx, int prefer_idle);
extern int schedtune_task_boost(struct task_struct *tsk);
#else
#define schedtune_task_boost(tsk) 0
#endif

extern unsigned int hmp_cpu_is_slowest(int cpu);
extern unsigned int hmp_cpu_is_fastest(int cpu);
extern bool is_intra_domain(int prev, int target);
