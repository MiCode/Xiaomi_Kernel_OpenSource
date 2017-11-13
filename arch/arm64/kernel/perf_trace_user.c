/* Copyright (c) 2014,2017 The Linux Foundation. All rights reserved.
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

static ssize_t perf_trace_write(struct file *file,
				const char __user *user_string_in,
				size_t len, loff_t *ppos)
{
	u32 cnten_val;
	int rc;
	char buf[TRACE_USER_MAX_BUF_SIZE + 1];
	ssize_t length;

	if (len == 0)
		return 0;

	length = len > TRACE_USER_MAX_BUF_SIZE ? TRACE_USER_MAX_BUF_SIZE : len;

	rc = copy_from_user(buf, user_string_in, length);
	if (rc) {
		pr_err("%s copy_from_user failed, rc=%d\n", __func__, rc);
		return -EFAULT;
	}

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
	/* stop counters, call the trace function, restart them */

	cnten_val = read_sysreg(pmcntenset_el0);
	/* Disable all the counters that were enabled */
	write_sysreg(cnten_val, pmcntenclr_el0);

	trace_perf_trace_user(buf, cnten_val);

	/* Enable all the counters that were disabled */
	write_sysreg(cnten_val, pmcntenset_el0);
	preempt_enable();

	return length;
}

static const struct file_operations perf_trace_fops = {
	.write = perf_trace_write
};

static int __init init_perf_trace(void)
{
	struct dentry *dir;
	struct dentry *file;
	unsigned int value = 1;

	dir = debugfs_create_dir("msm_perf", NULL);
	if (!dir)
		return -ENOMEM;
	file = debugfs_create_file("trace_marker", 0220, dir,
		&value, &perf_trace_fops);
	if (!file)
		return -ENOMEM;

	return 0;
}

late_initcall(init_perf_trace);
