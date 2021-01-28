/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _EAS_CTRL_PLAT_H_
#define _EAS_CTRL_PLAT_H_

/* control migration cost */
extern unsigned int sysctl_sched_migration_cost;

/* EAS */
extern int uclamp_min_for_perf_idx(int group_idx, int min_value);
extern void set_sched_rotation_enable(bool enable);
extern int set_sched_boost_type(int type);
extern int get_sched_boost_type(void);
extern int sched_set_cpuprefer(pid_t pid, unsigned int prefer_type);

/* Isolation */
extern struct cpumask __cpu_isolated_mask;
extern int sched_isolate_cpu(int cpu);
extern int sched_unisolate_cpu(int cpu);
#endif /* _EAS_CTRL_PLAT_H_ */
