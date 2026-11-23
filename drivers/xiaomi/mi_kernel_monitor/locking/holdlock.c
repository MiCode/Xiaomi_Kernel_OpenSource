// SPDX-License-Identifier: GPL-2.0+
/*
 * mi_locking will show some kernel lock wait/hold time.
 *
 * Copyright (C) 2024 Xiaomi Ltd.
 */

#define MI_LOCK_LOG_TAG       "holdlock"
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/slab.h>
#include <trace/events/napi.h>
#include <linux/init.h>
#include <linux/stacktrace.h>
#include <linux/mutex.h>
#include <linux/jiffies.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/dtask.h>
#include <asm/syscall.h>
#include <linux/proc_fs.h>
#include <linux/mi_kernel_monitor.h>

#include "holdlock.h"
#include "waitlock.h"
#include "locking_main.h"
#include "waitlock.h"
#include "internal.h"

#define MI_RWSEM_WRITER_LOCKED	(1UL << 0)
#define LOCK_END_FLAG           0

extern char *holdlock_str[];

struct xm_stack_trace __percpu *p_mutex_stack_trace;
struct xm_stack_trace __percpu *p_rwsem_stack_trace;
struct lock_stats monitor_info[GRP_TYPES];

/******************************saving stack strace********************************/
bool xm_stack_trace_record_for_opt_lock(struct task_struct* tsk, struct xm_stack_trace *stack_trace, struct pt_regs *regs, int duration, bool flag)
{
	unsigned int nr_entries, nr_stack_entries;
	struct stack_entry *stack_entry;

	nr_entries = stack_trace->nr_entries;
	if (nr_entries >= MAX_TRACE_ENTRIES)
	{
	    pr_err("xm_stack nr_entries is full, %u\n", nr_entries);
		return false;
	}

	nr_stack_entries = stack_trace->nr_stack_entries;
	if (nr_stack_entries >= MAX_STACE_TRACE_ENTRIES)
		return false;

	////strlcpy(stack_trace->curr_comms[nr_stack_entries], current->comm, (unsigned long)TASK_COMM_LEN);
	////strlcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm, (unsigned long)TASK_COMM_LEN);
	strcpy(stack_trace->curr_comms[nr_stack_entries], current->comm);
	stack_trace->curr_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;
	strcpy(stack_trace->parent_comms[nr_stack_entries], current->group_leader->comm);
	stack_trace->parent_comms[nr_stack_entries][TASK_COMM_LEN - 1] = 0;

	stack_trace->pids[nr_stack_entries] = tsk->pid;
	stack_trace->duration[nr_stack_entries] = duration;
	stack_trace->timestamp[nr_stack_entries] = sched_clock();
	stack_trace->prio[nr_stack_entries] = current->prio;
	////stack_trace->flag[nr_stack_entries] = flag;

	stack_entry = stack_trace->stack_entries + nr_stack_entries;
	xm_store_stack_trace(regs, stack_entry, stack_trace->entries + nr_entries, MAX_TRACE_ENTRIES - nr_entries, 0);
	stack_trace->nr_entries += stack_entry->nr_entries;

	smp_store_release(&stack_trace->nr_stack_entries, nr_stack_entries + 1);

	if ((stack_trace->nr_entries >= MAX_TRACE_ENTRIES)  || (stack_trace->nr_stack_entries > MAX_STACE_TRACE_ENTRIES)) {
		pr_err("BUG: xm_stack MAX_TRACE_ENTRIES too low cpu=%d, nr_entries=%u, nr_stack_entries=%u\n",
			smp_processor_id(),
			stack_trace->nr_entries,
			stack_trace->nr_stack_entries);
		return false;
	}

	return true;
}

/**
 * stack_trace_record - to save current backtrace
 * @param1: struct xm_stack_trace *stack_trace
 * @param2: duration time, ms
 * @param3: flag, useless
 *
 * eg:
 	 struct xm_stack_trace *stack_trace = this_cpu_ptr(p_mutex_stack_trace);
	 stack_trace_record(stack_trace, ms, flag);
 *
 */
inline bool stack_trace_record(struct xm_stack_trace *stack_trace, int duration, bool flag)
{
		return xm_stack_trace_record_for_opt_lock(current, stack_trace, get_irq_regs(), duration, flag);
}

static int track_stat_update(int grp_idx, int type, struct track_stat *ts, u64 time)
{
	int thres_type;

	if (NULL == ts)
		return -1;

	if (type >= LOCK_TYPES) {
		ml_err("pass error type param,""type = %d\n", type);
		return -1;
	}

	thres_type = waittime_thres_exceed_type(grp_idx, type, time); //LOW ,HIGH, FATAL
	if (thres_type < 0)
		return thres_type;

	atomic_inc(&ts->level[thres_type]);
//#ifdef CONFIG_XM_INTERNAL_VERSION
//	atomic64_add(time, &ts->exp_total_time);
//#endif
	cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
		"[%s]: add exceed low thres item, grp = %s, type = %s, time = %llu\n", 
		__func__, group_str[grp_idx], holdlock_str[type], time);
	return thres_type;
}

static int lock_stats_update(int grp_idx, int type, u64 time)
{
	struct lock_stats *pcs;
	int ret;
	/*
	 * Get percpu data ptr.
	 * Prevent process from switching to another CPU, disable preempt.
	 * There only is a atomic_inc operation(and/or a trace printk)
	 * during the preemtion off, no particularly time-consuming operations.
	 */
	pcs = &monitor_info[grp_idx];

	ret = track_stat_update(grp_idx, type, &pcs->per_type_stat[type], time);

	return ret;
}

/**
 * handle_hold_stats
 * @param1: mutex or rwsemw
 * @param2: hold lock time， ns
 *
 * handle hold lock time, get cnt to show or
 * save stack trace if UX/RT in fatal
 *
 */
__always_inline void handle_hold_stats(int type, u64 time)
{
	int grp_idx;
	int ret;
	int flag = 1;

	grp_idx = get_task_grp_idx(current);

	ret = lock_stats_update(grp_idx, type, time); //update data

	if (g_opt_stack && ret >= 0) {
		/*
		 * Only record UX/RT's fatal info, We don't care other groups.
		 */
		if ((ret == FATAL_THRES) && (grp_idx <= GRP_RT)) {
			cond_trace_printk(locking_opt_debug(LK_DEBUG_PRINTK),
				"[%s]: type=%d, curr->pid=%d, curr->comm=%s, comm->name=%s, holdtime_ms=%llu\n",
				__func__, type, current->pid, current->comm, current->group_leader->comm, time/NSEC_PER_MSEC);
			if (type == MUTEX) {
				struct xm_stack_trace *stack_trace = this_cpu_ptr(p_mutex_stack_trace);
				stack_trace_record(stack_trace, time/NSEC_PER_MSEC, flag);
			}
			else if (type == RWSEM_WRITE){
				struct xm_stack_trace *stack_trace = this_cpu_ptr(p_rwsem_stack_trace);
				stack_trace_record(stack_trace, time/NSEC_PER_MSEC, flag);
			}
		}
	}
}

void android_vh_record_rwsem_lock_starttime_handler(void *ignore, struct rw_semaphore *sem, unsigned long settime_jiffies)
{
	u64 delta = 0;

	/* resem lock holding end flag */
	if (settime_jiffies != LOCK_END_FLAG) {
		if (atomic_long_read(&sem->count) & MI_RWSEM_WRITER_LOCKED)
		{
		    KERNEL_MONITOR_RWSEM_OEM_DATA(sem) = settime_jiffies;
		}

		return;
	}

    delta = ((jiffies - KERNEL_MONITOR_RWSEM_OEM_DATA(sem)) * 1000000000) / HZ;
    handle_hold_stats(RWSEM_WRITE,delta);

    KERNEL_MONITOR_RWSEM_OEM_DATA(sem) = LOCK_END_FLAG;
}

void android_vh_record_mutex_lock_starttime_handler(void *nouse, struct mutex *lock, unsigned long settime_jiffies)
{
	u64 delta = 0;

	/* mutex lock holding start flag */
	if (settime_jiffies != LOCK_END_FLAG){
		lock->android_oem_data1[0] = settime_jiffies;
		return;
	}

	/* mutex lock holding end flag */
    delta = ((jiffies - lock->android_oem_data1[0]) * 1000000000) / HZ;
    handle_hold_stats(MUTEX, delta);

    lock->android_oem_data1[0] = LOCK_END_FLAG;
}

int holdlock_init(void)
{
	int ret = 0;

	if (g_opt_enable & HOLD_LK_ENABLE) {
		REGISTER_TRACE_VH(android_vh_record_mutex_lock_starttime, android_vh_record_mutex_lock_starttime_handler);
		REGISTER_TRACE_VH(android_vh_record_rwsem_lock_starttime, android_vh_record_rwsem_lock_starttime_handler);
		////REGISTER_TRACE_VH(android_vh_rwsem_write_finished, android_vh_rwsem_write_finished_handler);
	}

	p_mutex_stack_trace = alloc_percpu(struct xm_stack_trace);
	p_rwsem_stack_trace = alloc_percpu(struct xm_stack_trace);

	if (!p_mutex_stack_trace || !p_rwsem_stack_trace) {
		ml_err("alloc_percpu failed!");
		goto free_buf;
	}

	ret = holdlock_proc_init();
	if (ret) {
		ml_err(" failed, ret = %d\n", ret);
		goto out;
	}

	ml_info(" %s: succesed!\n", __func__);

 	return 0;

out:
	return 1;

free_buf:
	free_percpu(p_mutex_stack_trace);
	free_percpu(p_rwsem_stack_trace);

	return -ENOMEM;
}

void holdlock_exit(void)
{
	if (g_opt_enable & HOLD_LK_ENABLE) {
		UNREGISTER_TRACE_VH(android_vh_record_mutex_lock_starttime, android_vh_record_mutex_lock_starttime_handler);
		UNREGISTER_TRACE_VH(android_vh_record_rwsem_lock_starttime, android_vh_record_rwsem_lock_starttime_handler);
		////UNREGISTER_TRACE_VH(android_vh_rwsem_write_finished, android_vh_rwsem_write_finished_handler);
	}

	free_percpu(p_mutex_stack_trace);
	free_percpu(p_rwsem_stack_trace);

    remove_hold_stats_procs();

	ml_info("finished!\n");

	return;
}

