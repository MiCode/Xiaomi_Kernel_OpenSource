/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/perf_event.h>
#include <linux/types.h>
#include <linux/tracepoint.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/preempt.h>
#include <linux/stat.h>
#include <asm/uaccess.h>

#define CREATE_TRACE_POINTS
#include "perf_trace_user.h"

#undef TRACE_SYSTEM
#define TRACE_SYSTEM perf_trace_counters

#define TRACE_USER_MAX_BUF_SIZE 100
u32 perf_num_l2ctrs;

static int perf_trace_write(struct file *file,
			    const char __user *user_string_in,
			    size_t len, loff_t *ppos)
{
	u32 cnten_val;
	u32 bit;
	u32 cpu;
	u32 num_cores = nr_cpu_ids;
	u32 l2_enmask;
	u32 l2_cnten_val;
	unsigned long idx;
	int i;
	int rc;
	char buf[TRACE_USER_MAX_BUF_SIZE + 1];
	int length;

	if (len == 0)
		return 0;

	length = len > TRACE_USER_MAX_BUF_SIZE ? TRACE_USER_MAX_BUF_SIZE : len;

	rc = copy_from_user(buf, user_string_in, length);

	/* Remove any trailing newline and make sure string is terminated */
	if (buf[length - 1] == '\n')
		buf[length - 1] = '\0';
	else
		buf[length] = '\0';

	/*
	 * Disable preemption to ensure that all the performance counter
	 * accesses happen on the same cpu
	 */
	preempt_disable();
	cpu = smp_processor_id();

	/* stop counters, call the trace function, restart them */

	/* Read PMCNTENSET */
	asm volatile("mrc p15, 0, %0, c9, c12, 1" : "=r"(cnten_val));
	/* Disable all the counters that were enabled */
	asm volatile("mcr p15, 0, %0, c9, c12, 2" : : "r"(cnten_val));

	l2_cnten_val = get_l2_indirect_reg(L2PMCNTENSET);
	l2_enmask = 0;
	for (i = 0; i < NUM_L2_PERCPU; i++) {
		/*
		 * Assign L2 counters to cores sequentially
		 * starting from zero. A core could have
		 * multiple L2 counters allocated if # L2
		 * counters is more than the # cores
		 */
		idx = cpu + (num_cores * i);
		bit = BIT(idx);
		if (idx < perf_num_l2ctrs && (l2_cnten_val & bit)) {
			/* Disable */
			set_l2_indirect_reg(L2PMCNTENCLR, bit);
			l2_enmask |= bit;
		}
	}
	trace_perf_trace_user(buf, cnten_val, l2_enmask);

	/* Enable L2*/
	set_l2_indirect_reg(L2PMCNTENSET, l2_enmask);

	/* Enable all the counters that were disabled */
	asm volatile("mcr p15, 0, %0, c9, c12, 1" : : "r"(cnten_val));
	preempt_enable();

	return length;
}

static const struct file_operations perf_trace_fops = {
	.write = perf_trace_write
};

static int __init init_perf_trace(void)
{
	u32 val;
	struct dentry *dir;
	struct dentry *file;
	unsigned int value = 1;

	dir = perf_create_debug_dir();
	if (!dir)
		return -ENOMEM;
	file = debugfs_create_file("trace_marker", S_IWUSR | S_IWGRP, dir,
		&value, &perf_trace_fops);
	if (!file)
		return -ENOMEM;

	val = get_l2_indirect_reg(L2PMCR);
	perf_num_l2ctrs = ((val >> 11) & 0x1f) + 1;

	return 0;
}

late_initcall(init_perf_trace);
