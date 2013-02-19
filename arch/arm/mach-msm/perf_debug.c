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
	"0  msm: perf: add debug patch logging framework\n"
	"1  Perf: Restore counter after powercollapse for generic ARM PMU's\n"
	"2  Perf: Toggle PMU IRQ when CPU's are hotplugged\n"
	"3  Perf: Correct irq for CPU hotplug detection\n"
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

static int msm_perf_debugfs_init(void)
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
late_initcall(msm_perf_debugfs_init);
