/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <asm/thread_notify.h>
#define CREATE_TRACE_POINTS
#include "perf_trace_counters.h"

static int tracectr_notifier(struct notifier_block *self, unsigned long cmd,
		void *v)
{
	static int old_pid = -1;
	struct thread_info *thread = v;
	int current_pid;

	if (cmd != THREAD_NOTIFY_SWITCH)
		return old_pid;

	current_pid = thread->task->pid;
	if (old_pid != -1)
		trace_sched_switch_with_ctrs(old_pid, current_pid);
	old_pid = current_pid;
	return old_pid;
}

static struct notifier_block tracectr_notifier_block = {
	.notifier_call  = tracectr_notifier,
};

int __init init_tracecounters(void)
{
	thread_register_notifier(&tracectr_notifier_block);
	return 0;
}
late_initcall(init_tracecounters);
