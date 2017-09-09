/* Copyright (c) 2013-2014, 2017 The Linux Foundation. All rights reserved.
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
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <linux/tracepoint.h>
#include <trace/events/sched.h>
#define CREATE_TRACE_POINTS
#include "perf_trace_counters.h"

static unsigned int tp_pid_state;

DEFINE_PER_CPU(u32, cntenset_val);
DEFINE_PER_CPU(u32, previous_ccnt);
DEFINE_PER_CPU(u32[NUM_L1_CTRS], previous_l1_cnts);
DEFINE_PER_CPU(u32, old_pid);
DEFINE_PER_CPU(u32, hotplug_flag);

static int tracectr_cpu_hotplug_notifier(struct notifier_block *self,
					 unsigned long action, void *hcpu)
{
	unsigned long cpu = (unsigned long)hcpu;

	if ((action & (~CPU_TASKS_FROZEN)) == CPU_STARTING)
		per_cpu(hotplug_flag, cpu) = 1;

	return NOTIFY_OK;
}

static struct notifier_block tracectr_cpu_hotplug_notifier_block = {
	.notifier_call = tracectr_cpu_hotplug_notifier,
};

static void setup_prev_cnts(u32 cpu, u32 cnten_val)
{
	int i;

	if (cnten_val & CC)
		asm volatile("mrs %0, pmccntr_el0"
			: "=r"(per_cpu(previous_ccnt, cpu)));

	for (i = 0; i < NUM_L1_CTRS; i++) {
		if (cnten_val & (1 << i)) {
			/* Select */
			asm volatile("msr pmselr_el0, %0" : : "r"(i));
			isb();
			/* Read value */
			asm volatile("mrs %0, pmxevcntr_el0"
				: "=r"(per_cpu(previous_l1_cnts[i], cpu)));
		}
	}
}

void tracectr_notifier(void *ignore, bool preempt,
			struct task_struct *prev, struct task_struct *next)
{
	u32 cnten_val;
	int current_pid;
	u32 cpu = task_cpu(next);

	if (tp_pid_state != 1)
		return;
	current_pid = next->pid;
	if (per_cpu(old_pid, cpu) != -1) {
		asm volatile("mrs %0, pmcntenset_el0" : "=r" (cnten_val));
		per_cpu(cntenset_val, cpu) = cnten_val;
		/* Disable all the counters that were enabled */
		asm volatile("msr pmcntenclr_el0, %0" : : "r" (cnten_val));

		if (per_cpu(hotplug_flag, cpu) == 1) {
			per_cpu(hotplug_flag, cpu) = 0;
			setup_prev_cnts(cpu, cnten_val);
		} else {
			trace_sched_switch_with_ctrs(per_cpu(old_pid, cpu),
						     current_pid);
		}

		/* Enable all the counters that were disabled */
		asm volatile("msr pmcntenset_el0, %0" : : "r" (cnten_val));
	}
	per_cpu(old_pid, cpu) = current_pid;
}

static void enable_tp_pid(void)
{
	if (tp_pid_state == 0) {
		tp_pid_state = 1;
		register_trace_sched_switch(tracectr_notifier, NULL);
	}
}

static void disable_tp_pid(void)
{
	if (tp_pid_state == 1) {
		tp_pid_state = 0;
		unregister_trace_sched_switch(tracectr_notifier, NULL);
	}
}

static ssize_t read_enabled_perftp_file_bool(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[2];
	buf[1] = '\n';
	if (tp_pid_state == 0)
		buf[0] = '0';
	else
		buf[0] = '1';
	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

static ssize_t write_enabled_perftp_file_bool(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;

	buf[0] = 0;
	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	switch (buf[0]) {
	case 'y':
	case 'Y':
	case '1':
		enable_tp_pid();
		break;
	case 'n':
	case 'N':
	case '0':
		disable_tp_pid();
		break;
	}

	return count;
}

static const struct file_operations fops_perftp = {
	.read =		read_enabled_perftp_file_bool,
	.write =	write_enabled_perftp_file_bool,
	.llseek =	default_llseek,
};

int __init init_tracecounters(void)
{
	struct dentry *dir;
	struct dentry *file;
	unsigned int value = 1;
	int cpu;

	dir = debugfs_create_dir("perf_debug_tp", NULL);
	if (!dir)
		return -ENOMEM;
	file = debugfs_create_file("enabled", 0660, dir,
		&value, &fops_perftp);
	if (!file) {
		debugfs_remove(dir);
		return -ENOMEM;
	}
	for_each_possible_cpu(cpu)
		per_cpu(old_pid, cpu) = -1;
	register_cpu_notifier(&tracectr_cpu_hotplug_notifier_block);
	return 0;
}

int __exit exit_tracecounters(void)
{
	unregister_cpu_notifier(&tracectr_cpu_hotplug_notifier_block);
	return 0;
}
late_initcall(init_tracecounters);
