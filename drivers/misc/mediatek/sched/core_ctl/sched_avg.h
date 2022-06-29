/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _SCHED_AVG_H
#define _SCHED_AVG_H

enum {
	DISABLE_POLICY = 0,
	AGGRESSIVE_POLICY,
	CONSERVATIVE_POLICY,
	POLICY_CNT
};

extern void sched_max_util_task(int *util);
extern void arch_get_cluster_cpus(struct cpumask *cpus, int package_id);
extern int sched_get_nr_over_thres_avg(int cluster_id,
				       int *dn_avg,
				       int *up_avg,
				       int *sum_nr_over_dn_thres,
				       int *sum_nr_over_up_thres,
				       int *max_nr,
				       int policy);
extern int arch_get_nr_clusters(void);
extern int arch_get_cluster_id(unsigned int cpu);
extern int init_sched_avg(void);
extern void exit_sched_avg(void);
extern unsigned int sched_get_cpu_util_pct(unsigned int cpu);
extern void set_over_threshold(unsigned int index, unsigned int val);
unsigned int get_over_threshold(int index);
unsigned int get_max_capacity(unsigned int cid);
extern unsigned int mtk_get_leakage(unsigned int cpu,
				    unsigned int opp,
				    unsigned int temperature);
extern unsigned long pd_get_opp_capacity(int cpu, int opp);

#endif /* _SCHED_AVG_H */
