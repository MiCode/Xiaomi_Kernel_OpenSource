/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _EAS_PLUS_H
#define _EAS_PLUS_H

#include "../../../../kernel/sched/sched.h"
#include "cpufreq.h"

#define MIGR_IDLE_BALANCE 1

#if IS_ENABLED(CONFIG_MTK_EAS)
extern void mtk_find_busiest_group(void *data, struct sched_group *busiest,
		struct rq *dst_rq, int *out_balance);
extern void mtk_find_energy_efficient_cpu(void *data, struct task_struct *p,
		int prev_cpu, int sync, int *new_cpu);

extern void mtk_em_cpu_energy(void *data, struct em_perf_domain *pd,
		unsigned long max_util, unsigned long sum_util, unsigned long *energy);
#if IS_ENABLED(CONFIG_MTK_THERMAL_AWARE_SCHEDULING)
extern int sort_thermal_headroom(struct cpumask *cpus, int *cpu_order);
#endif

extern int init_sram_info(void);
extern void mtk_tick_entry(void *data, struct rq *rq);
extern void mtk_set_wake_flags(void *data, int *wake_flags, unsigned int *mode);

#if IS_ENABLED(CONFIG_MTK_NEWIDLE_BALANCE)
extern void mtk_sched_newidle_balance(void *data, struct rq *this_rq, struct rq_flags *rf, int *pulled_task, int *done);
#endif
#endif
#endif
