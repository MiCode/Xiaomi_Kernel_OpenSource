// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */
#ifndef __HOLDLOCK_H
#define __HOLDLOCK_H

#include <linux/proc_fs.h>
#include <linux/kernel.h>
#include <linux/seq_file.h>

#include "waitlock.h"
#include "internal.h"

#define LOCK_STATS_DIRNAME                      "lock_stats"
#define MAX_TRACE_ENTRIES                       (512)
#define OVER_FLOW_LEN                           32
#define MAX_STACE_TRACE_ENTRIES                 (32)
#define TASK_COMM_LEN                           16
#define BACKTRACE_DEPTH                         30
#define MONI_LOCK_TYPES                          3

void holdlock_exit(void);
int holdlock_init(void);
int holdlock_proc_init(void);
void remove_hold_stats_procs(void);
void android_vh_record_mutex_lock_starttime_handler(void *nouse, struct mutex *lock, unsigned long settime_jiffies);
void android_vh_record_rwsem_lock_starttime_handler(void *ignore, struct rw_semaphore *sem, unsigned long settime_jiffies);
void android_vh_rwsem_write_finished_handler(void *ignore, struct rw_semaphore *sem);
void stack_trace_show(struct seq_file *m, struct xm_stack_trace __percpu *p_stack_trace);
inline bool stack_trace_record(struct xm_stack_trace *stack_trace, int duration, bool flag);

extern struct xm_stack_trace __percpu *p_mutex_stack_trace;
extern struct xm_stack_trace __percpu *p_rwsem_stack_trace;
extern struct lock_stats monitor_info[GRP_TYPES];

#endif /* __HOLDLOCK_H */