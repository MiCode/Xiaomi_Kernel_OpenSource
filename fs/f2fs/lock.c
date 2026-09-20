// SPDX-License-Identifier: GPL-2.0
/*
 * fs/f2fs/checkpoint.c
 *
 * Copyright (c) 2026 Google
 */
#include <linux/f2fs_fs.h>
#include <linux/delayacct.h>
#include <linux/ioprio.h>
#include <linux/math64.h>

#include "f2fs.h"
#include "segment.h"
#include <trace/events/f2fs.h>

static inline void get_lock_elapsed_time(struct f2fs_time_stat *ts)
{
	ts->total_time = ktime_get();
#ifdef CONFIG_64BIT
	ts->running_time = current->se.sum_exec_runtime;
#endif
#if defined(CONFIG_SCHED_INFO) && defined(CONFIG_SCHEDSTATS)
	ts->runnable_time = current->sched_info.run_delay;
#endif
#ifdef CONFIG_TASK_DELAY_ACCT
	if (current->delays)
		ts->io_sleep_time = current->delays->blkio_delay;
#endif
}

static inline void trace_lock_elapsed_time_start(struct f2fs_rwsem *sem,
						struct f2fs_lock_context *lc)
{
	lc->lock_trace = trace_f2fs_lock_elapsed_time_enabled();
	if (!lc->lock_trace)
		return;

	get_lock_elapsed_time(&lc->ts);
}

static inline void trace_lock_elapsed_time_end(struct f2fs_rwsem *sem,
				struct f2fs_lock_context *lc, bool is_write)
{
	struct f2fs_time_stat tts;
	unsigned long long total_time;
	unsigned long long running_time = 0;
	unsigned long long runnable_time = 0;
	unsigned long long io_sleep_time = 0;
	unsigned long long other_time = 0;
	unsigned npm = NSEC_PER_MSEC;

	if (!lc->lock_trace)
		return;

	get_lock_elapsed_time(&tts);

	total_time = div_u64(tts.total_time - lc->ts.total_time, npm);
	if (total_time <= sem->sbi->max_lock_elapsed_time)
		return;

#ifdef CONFIG_64BIT
	running_time = div_u64(tts.running_time - lc->ts.running_time, npm);
#endif
#if defined(CONFIG_SCHED_INFO) && defined(CONFIG_SCHEDSTATS)
	runnable_time = div_u64(tts.runnable_time - lc->ts.runnable_time, npm);
#endif
#ifdef CONFIG_TASK_DELAY_ACCT
	io_sleep_time = div_u64(tts.io_sleep_time - lc->ts.io_sleep_time, npm);
#endif
	if (total_time > running_time + io_sleep_time + runnable_time)
		other_time = total_time - running_time -
				io_sleep_time - runnable_time;

	trace_f2fs_lock_elapsed_time(sem->sbi, sem->name, is_write, current,
			get_current_ioprio(), total_time, running_time,
			runnable_time, io_sleep_time, other_time);
}

static bool need_uplift_priority(struct f2fs_rwsem *sem, bool is_write)
{
	if (!(sem->sbi->adjust_lock_priority & BIT(sem->name - 1)))
		return false;

	switch (sem->name) {
	/*
	 * writer is checkpoint which has high priority, let's just uplift
	 * priority for reader
	 */
	case LOCK_NAME_CP_RWSEM:
	case LOCK_NAME_NODE_CHANGE:
	case LOCK_NAME_NODE_WRITE:
		return !is_write;
	case LOCK_NAME_GC_LOCK:
	case LOCK_NAME_CP_GLOBAL:
	case LOCK_NAME_IO_RWSEM:
		return true;
	default:
		f2fs_bug_on(sem->sbi, 1);
	}
	return false;
}

static void uplift_priority(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc,
						bool is_write)
{
	lc->need_restore = false;
	if (!sem->sbi->adjust_lock_priority)
		return;
	if (rt_task(current))
		return;
	if (!need_uplift_priority(sem, is_write))
		return;
	lc->orig_nice = task_nice(current);
	lc->new_nice = PRIO_TO_NICE(sem->sbi->lock_duration_priority);
	if (lc->orig_nice <= lc->new_nice)
		return;
	set_user_nice(current, lc->new_nice);
	lc->need_restore = true;

	trace_f2fs_priority_uplift(sem->sbi, sem->name, is_write, current,
		NICE_TO_PRIO(lc->orig_nice), NICE_TO_PRIO(lc->new_nice));
}

static void restore_priority(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc,
						bool is_write)
{
	if (!lc->need_restore)
		return;
	/* someone has updated the priority */
	if (task_nice(current) != lc->new_nice)
		return;
	set_user_nice(current, lc->orig_nice);

	trace_f2fs_priority_restore(sem->sbi, sem->name, is_write, current,
		NICE_TO_PRIO(lc->orig_nice), NICE_TO_PRIO(lc->new_nice));
}

void f2fs_down_read_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	uplift_priority(sem, lc, false);
	f2fs_down_read(sem);
	trace_lock_elapsed_time_start(sem, lc);
}

int f2fs_down_read_trylock_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	uplift_priority(sem, lc, false);
	if (!f2fs_down_read_trylock(sem)) {
		restore_priority(sem, lc, false);
		return 0;
	}
	trace_lock_elapsed_time_start(sem, lc);
	return 1;
}

void f2fs_up_read_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	f2fs_up_read(sem);
	restore_priority(sem, lc, false);
	trace_lock_elapsed_time_end(sem, lc, false);
}

void f2fs_down_write_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	uplift_priority(sem, lc, true);
	f2fs_down_write(sem);
	trace_lock_elapsed_time_start(sem, lc);
}

int f2fs_down_write_trylock_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	uplift_priority(sem, lc, true);
	if (!f2fs_down_write_trylock(sem)) {
		restore_priority(sem, lc, true);
		return 0;
	}
	trace_lock_elapsed_time_start(sem, lc);
	return 1;
}

void f2fs_up_write_trace(struct f2fs_rwsem *sem, struct f2fs_lock_context *lc)
{
	f2fs_up_write(sem);
	restore_priority(sem, lc, true);
	trace_lock_elapsed_time_end(sem, lc, true);
}

void f2fs_lock_op(struct f2fs_sb_info *sbi, struct f2fs_lock_context *lc)
{
	f2fs_down_read_trace(&sbi->cp_rwsem, lc);
}

int f2fs_trylock_op(struct f2fs_sb_info *sbi, struct f2fs_lock_context *lc)
{
	if (time_to_inject(sbi, FAULT_LOCK_OP))
		return 0;

	return f2fs_down_read_trylock_trace(&sbi->cp_rwsem, lc);
}

void f2fs_unlock_op(struct f2fs_sb_info *sbi, struct f2fs_lock_context *lc)
{
	f2fs_up_read_trace(&sbi->cp_rwsem, lc);
}
