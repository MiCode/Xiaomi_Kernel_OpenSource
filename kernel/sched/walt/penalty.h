/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _PENALTY_H
#define _PENALTY_H

#include "walt.h"

struct cpu_busy_data {
	bool		is_busy;
	unsigned int    busy_pct;
	unsigned int	cpu;
};

struct sched_opt {
int  (*update_yield_ts)(struct walt_task_struct *wts, u64 clock);
void (*update_yield_util)(struct walt_rq *wrq, struct walt_task_struct *wts);
int  (*penalize_yield)( u64 *delta, struct walt_task_struct *wts);

void (*account_wakeup)(struct task_struct *p);
void (*rollover_task_window)(struct walt_rq *wrq, struct  rq *rq, struct task_struct *p);

void (*cluster_init)(struct walt_sched_cluster *cluster);
void (*cluster_update)(struct walt_sched_cluster *cluster, struct rq *rq);
void (*policy_load)(struct walt_sched_cluster *cluster, u64 *pload, u64 *pplload);

void (*rollover_cpu_window)(struct walt_rq *wrq, struct rq *rq, bool full_window);

int (*eval_need)(struct cpu_busy_data *data,int data_len, unsigned int *pneed_cpus);
void (*waltgov_run_callback)(struct rq *rq, unsigned int flags);

int  (*update_heavy)(struct walt_task_struct **heavy_wts, int heavy_nr);
void (*reserved_1)(void *p);
void (*reserved_2)(void *p);
void (*reserved_3)(void *p);
};

int update_sched_opt(struct sched_opt  *popt, bool opts_init_);
extern struct sched_opt opts;

#endif /*_PENALTY_H*/
