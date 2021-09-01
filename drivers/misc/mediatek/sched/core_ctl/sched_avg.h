/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _SCHED_AVG_H
#define _SCHED_AVG_H

extern void sched_max_util_task(int *cpu, int *pid, int *util, int *boost);
extern void arch_get_cluster_cpus(struct cpumask *cpus, int package_id);
extern int sched_get_nr_over_thres_avg(unsigned int cluster_id,
				unsigned int *dn_avg,
				unsigned int *up_avg,
				unsigned int *sum_nr_over_dn_thres,
				unsigned int *sum_nr_over_up_thres,
				unsigned int *max_nr);
extern int arch_get_nr_clusters(void);
extern int arch_get_cluster_id(unsigned int cpu);
extern int init_sched_avg(void);
extern void exit_sched_avg(void);
extern unsigned int sched_get_cpu_util_pct(unsigned int cpu);
extern void set_over_threshold(unsigned int index, unsigned int val);
unsigned int get_over_threshold(int index);
unsigned int get_max_capacity(unsigned int cid);

#endif /* _SCHED_AVG_H */
