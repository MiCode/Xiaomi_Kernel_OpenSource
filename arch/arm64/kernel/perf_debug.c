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

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

/*
 * Subsequent patches should add an entry to end of this string.
 * Format is incrementing sequence number followed by text of
 * patch commit title with newline.
 * Note trailing ';' is on its own line to simplify addition of
 * future strings.
 */
static char *descriptions =
	" 0 arm64: perf: add debug patch logging framework\n"
	" 1 Perf: arm64: Add L1 counters to tracepoints\n"
	" 2 Perf: arm64: add support for msm8994v1 irq\n"
	" 3 Perf: arm64: enable cti workaround\n"
	" 4 Perf: arm64: support hotplug and power collapse\n"
	" 5 Perf: arm64: add perf user-mode permissions\n"
	" 6 Perf: arm64: Add debugfs node to clear PMU\n"
	" 7 Perf: arm64: Update PMU force reset\n"
	" 8 Perf: arm64: Use device tree property for CTI PMU workaround\n"
	" 9 Perf: arm64: make request irq pmu-dependent\n"
	"10 Perf: arm64: tracectr: initialize counts after hotplug\n"
	"11 Perf: arm64: Refine disable/enable in tracecounters\n"
	"12 Perf: arm64: fix disable of pmu irq during hotplug\n"
	"13 Perf: arm64: restore registers after reset\n"
	"14 Perf: arm64: stop counters when going into hotplug\n"
	"15 Perf: arm64: make debug dir handle exportable\n"
;

static ssize_t desc_read(struct file *fp, char __user *buf,
			 size_t count, loff_t *pos)
{
	return simple_read_from_buffer(buf, count, pos, descriptions,
				       strlen(descriptions));
}

static const struct file_operations perf_debug_desc_fops = {
	.read = desc_read,
};

static int perf_debugfs_init(void)
{
	int ret = 0;
	struct dentry *dir;
	struct dentry *file;

	dir = debugfs_create_dir("msm-perf-patches", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		pr_err("failed to create msm-perf-patches dir in debugfs\n");
		ret = PTR_ERR(dir);
		goto init_exit;
	}

	file = debugfs_create_file("descriptions", 0444, dir, NULL,
				   &perf_debug_desc_fops);
	if (IS_ERR_OR_NULL(file)) {
		debugfs_remove(dir);
		pr_err("failed to create descriptions file for msm-perf-patches\n");
		ret = PTR_ERR(file);
		goto init_exit;
	}

init_exit:
	return ret;
}
late_initcall(perf_debugfs_init);
