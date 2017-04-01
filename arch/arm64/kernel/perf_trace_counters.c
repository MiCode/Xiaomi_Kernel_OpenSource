// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2014, 2017-2018, The Linux Foundation. All rights reserved.
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

#define USE_CPUHP_STATE CPUHP_AP_ONLINE

static int tracectr_cpu_hotplug_coming_up(unsigned int cpu)
{
	per_cpu(hotplug_flag, cpu) = 1;

	return 0;
}

static void setup_prev_cnts(u32 cpu, u32 cnten_val)
{
	int i;

	if (cnten_val & CC)
		per_cpu(previous_ccnt, cpu) =
			read_sysreg(pmccntr_el0);

	for (i = 0; i < NUM_L1_CTRS; i++) {
		if (cnten_val & (1 << i)) {
			/* Select */
			write_sysreg(i, pmselr_el0);
			isb();
			/* Read value */
			per_cpu(previous_l1_cnts[i], cpu) =
				read_sysreg(pmxevcntr_el0);
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
		cnten_val = read_sysreg(pmcntenset_el0);
		per_cpu(cntenset_val, cpu) = cnten_val;
		/* Disable all the counters that were enabled */
		write_sysreg(cnten_val, pmcntenclr_el0);

		if (per_cpu(hotplug_flag, cpu) == 1) {
			per_cpu(hotplug_flag, cpu) = 0;
			setup_prev_cnts(cpu, cnten_val);
		} else {
			trace_sched_switch_with_ctrs(per_cpu(old_pid, cpu),
						     current_pid);
		}

		/* Enable all the counters that were disabled */
		write_sysreg(cnten_val, pmcntenset_el0);
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
	int cpu, rc;

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
	rc = cpuhp_setup_state_nocalls(USE_CPUHP_STATE,
		"tracectr_cpu_hotplug",
		tracectr_cpu_hotplug_coming_up,
		NULL);
	return 0;
}

int __exit exit_tracecounters(void)
{
	cpuhp_remove_state_nocalls(USE_CPUHP_STATE);
	return 0;
}
late_initcall(init_tracecounters);
