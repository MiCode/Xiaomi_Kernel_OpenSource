/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright 2023 Unisoc(Shanghai) Technologies Co.Ltd
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation; version 2.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#define pr_fmt(fmt) "unisoc_binder: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/unisoc_vd_def.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/sched.h>
#include <uapi/linux/sched/types.h>

#include "unibinder.h"
#include "unibinder_netlink.h"

#define CREATE_TRACE_POINTS
#include "unibinder_trace.h"

/**
 * Use this lock to protect the unibinder_feature_flags
 * operation in multi-thread context.
 */
static DEFINE_SPINLOCK(unibinder_feature_lock);

enum {
	UNIBINDER_DEBUG_THREAD_FEATURE		= BIT(0),
	UNIBINDER_DEBUG_THREAD_PRIORITY		= BIT(1),
};
static uint32_t unibinder_debug_mask;
module_param_named(debug_mask, unibinder_debug_mask, uint, 0644);

#define unibinder_debug(mask, x...) \
	do { \
		if (unibinder_debug_mask & mask) \
			pr_info_ratelimited(x); \
	} while (0)

/* Check if the feature flags is valid */
static bool is_flags_valid(int flags)
{
	return (flags & UBFF_FULL_MASK);
}

/**
 * Check if the thread enabled the feature that the feature_flag refer to.
 * KEEP the task reference safety outside of this function, it is NOT this
 * function responsibility to do the get_task_struct and put_task_struct work.
 * @task:           the task which to be check the feature flag.
 * @feature_flag:   the feature flags, can refer to the is_flags_valid
 *                  function checked flags definition.
 */
static bool is_enabled_feature(struct task_struct *task, int feature_flag)
{
	struct uni_task_struct *uni_task;
	int ret = false;

	if (!task)
		return ret;

	uni_task = (struct uni_task_struct *) task->android_vendor_data1;

	spin_lock(&unibinder_feature_lock);
	if (uni_task->unibinder_feature_flags & feature_flag)
		ret = true;
	spin_unlock(&unibinder_feature_lock);

	return ret;
}

/* Need to be guared by unibinder_feature_lock */
static void
adjust_feature_flags_locked(struct uni_task_struct *uni_task, int request_flags, int set)
{
	if (!uni_task)
		return;

	if (request_flags & UBFF_SCHED_SKIP_RESTORE) {
		if (!set) {
			uni_task->unibinder_feature_flags |= UBFF_SCHED_SKIP_RESTORE_RESET;
			return;
		}

		if (set && (uni_task->unibinder_feature_flags & UBFF_SCHED_SKIP_RESTORE_RESET))
			uni_task->unibinder_feature_flags &= ~UBFF_SCHED_SKIP_RESTORE_RESET;
	}
}

void set_thread_flags(int pid, int flags, int set)
{
	struct task_struct *task;
	struct uni_task_struct *uni_task;
	int updated_flags;

	if (!is_flags_valid(flags)) {
		unibinder_debug(UNIBINDER_DEBUG_THREAD_FEATURE,
				"%d try to set flags %d to %d failed, valid flags %lu",
				current->pid, flags, pid, UBFF_FULL_MASK);
		return;
	}

	task = get_pid_task(find_vpid(pid), PIDTYPE_PID);
	if (!task)
		return;

	uni_task = (struct uni_task_struct *)task->android_vendor_data1;

	spin_lock(&unibinder_feature_lock);
	if ((set && (uni_task->unibinder_feature_flags & flags)) ||
		(!set && !(uni_task->unibinder_feature_flags & flags))) {
		spin_unlock(&unibinder_feature_lock);
		put_task_struct(task);
		return;
	}

	if (set)
		uni_task->unibinder_feature_flags |= flags;
	else
		uni_task->unibinder_feature_flags &= ~flags;

	adjust_feature_flags_locked(uni_task, flags, set);

	updated_flags = uni_task->unibinder_feature_flags;
	spin_unlock(&unibinder_feature_lock);

	trace_unibinder_set_feature(task->tgid, pid, updated_flags);
	unibinder_debug(UNIBINDER_DEBUG_THREAD_FEATURE,
		"%d:%d feature flags has been updated to %d",
		task->tgid, pid, updated_flags);

	put_task_struct(task);
}

/* Check whether the binder thread has some pending binder work to do */
static bool unibinder_has_work(struct binder_thread *thread,
					bool do_proc_work)
{
	bool has_work;

	spin_lock(&thread->proc->inner_lock);
	has_work = thread->process_todo ||
		thread->looper_need_return ||
		(do_proc_work && !list_empty(&thread->proc->todo));
	spin_unlock(&thread->proc->inner_lock);
	return has_work;
}

/**
 * Hook the trace_android_vh_binder_restore_priority for skip restore feature
 * If the binder thread enabled the skip restore feature, we will
 * skip the priority restore flow for it
 */
static void unibinder_restore_priority(void *data, struct binder_transaction *in_reply_to,
					struct task_struct *task)
{
	struct binder_thread *to_thread;

	if (!in_reply_to)
		return;

	to_thread = in_reply_to->to_thread;

	if (!to_thread || !to_thread->task)
		return;

	if (is_enabled_feature(to_thread->task, UBFF_SCHED_SKIP_RESTORE) &&
		unibinder_has_work(to_thread, true)) {
		int orig_prio_state;
		int next_prio;

		spin_lock(&to_thread->prio_lock);
		if (to_thread->prio_state != BINDER_PRIO_ABORT) {
			orig_prio_state = to_thread->prio_state;
			next_prio = to_thread->prio_next.prio;
			to_thread->prio_state = BINDER_PRIO_ABORT;
			spin_unlock(&to_thread->prio_lock);

			unibinder_debug(UNIBINDER_DEBUG_THREAD_PRIORITY,
				"%d:%d prio skip restore enabled, orig pri state %d, cur pri %d next pri %d",
				to_thread->task->tgid, to_thread->task->pid, orig_prio_state,
				to_thread->task->normal_prio, next_prio);
			trace_unibinder_set_priority(to_thread->task->tgid, to_thread->task->pid,
				to_thread->task->normal_prio, next_prio, SET_FROM_RESTORE_PRIO_VH,
				0, orig_prio_state);
		} else
			spin_unlock(&to_thread->prio_lock);
	} else if (is_enabled_feature(to_thread->task, UBFF_SCHED_SKIP_RESTORE_RESET)) {
		struct uni_task_struct *uni_task;

		spin_lock(&to_thread->prio_lock);
		// AOSP BINDER_PRIO_ABORT is designed for nested transaction, current use case
		// is not the nested transaction.Now this restore is safe.
		// TODO: the abort restore should keep the AOSP settings.
		if (to_thread->prio_state == BINDER_PRIO_ABORT)
			to_thread->prio_state = BINDER_PRIO_SET;
		spin_unlock(&to_thread->prio_lock);

		uni_task = (struct uni_task_struct *) to_thread->task->android_vendor_data1;
		spin_lock(&unibinder_feature_lock);
		uni_task->unibinder_feature_flags &= ~UBFF_SCHED_SKIP_RESTORE_RESET;
		spin_unlock(&unibinder_feature_lock);

		unibinder_debug(UNIBINDER_DEBUG_THREAD_PRIORITY,
				"%d:%d prio state restore to normal",
				to_thread->task->tgid, to_thread->task->pid);
	}
}

static void register_binder_vh(void)
{
	register_trace_android_vh_binder_restore_priority(unibinder_restore_priority, NULL);
}

static int __init unibinder_init(void)
{
	register_binder_vh();
	binder_netlink_init();
	return 0;
}

module_init(unibinder_init);

MODULE_AUTHOR("Xiaomei Li <xiaomei.li@unisoc.com>");
MODULE_LICENSE("GPL v2");
