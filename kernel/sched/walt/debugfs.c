// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/debugfs.h>
#include <trace/hooks/sched.h>

#include "walt.h"
#include "trace.h"

unsigned int debugfs_walt_features;
// MIUI ADD: Performance_TurboSched
#ifdef CONFIG_MIGT_WALT
EXPORT_SYMBOL_GPL(debugfs_walt_features);
#endif
// END Performance_TurboSched

static struct dentry *debugfs_walt;

static ssize_t counter_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	char buffer[1024];
	int len = 0;

	len += scnprintf(buffer, sizeof(buffer),
			"walt_sched_yield_counter %u\n"
			"walt_yield_to_sleep_counter ",
			walt_sched_yield_counter);

	for (int i = 0; i < WALT_NR_CPUS; i++)
		len += snprintf(buffer + len, sizeof(buffer) - len, "%u ",
				per_cpu(walt_yield_to_sleep, i));
	len += snprintf(buffer + len, sizeof(buffer) - len, "\n");

	return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static const struct file_operations counter_fops = {
	.read = counter_read,
	.open = simple_open,
};

void walt_register_debugfs(void)
{
	debugfs_walt = debugfs_create_dir("walt", NULL);
	debugfs_create_u32("walt_features", 0644, debugfs_walt, &debugfs_walt_features);
	debugfs_create_file("debug_counters", 0444, debugfs_walt, NULL, &counter_fops);
}
