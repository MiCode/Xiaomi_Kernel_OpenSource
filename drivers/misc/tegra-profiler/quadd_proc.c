/*
 * drivers/misc/tegra-profiler/quadd_proc.c
 *
 * Copyright (c) 2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#ifdef CONFIG_PROC_FS

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/tegra_profiler.h>

#include "quadd.h"
#include "version.h"
#include "quadd_proc.h"

#define YES_NO(x) ((x) ? "yes" : "no")

static struct quadd_ctx *ctx;

static int show_version(struct seq_file *f, void *offset)
{
	seq_printf(f, "version:         %s\n", QUADD_MODULE_VERSION);
	seq_printf(f, "branch:          %s\n", QUADD_MODULE_BRANCH);
	seq_printf(f, "samples version: %d\n", QUADD_SAMPLES_VERSION);
	seq_printf(f, "io version:      %d\n", QUADD_IO_VERSION);

	return 0;
}

static int show_version_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_version, NULL);
}

static const struct file_operations version_proc_fops = {
	.open		= show_version_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_capabilities(struct seq_file *f, void *offset)
{
	struct quadd_comm_cap *cap = &ctx->cap;
	struct quadd_events_cap *event = &cap->events_cap;
	unsigned int extra = cap->reserved[QUADD_COMM_CAP_IDX_EXTRA];

	seq_printf(f, "pmu:                            %s\n",
		   YES_NO(cap->pmu));
	seq_printf(f, "tegra 3 LP cluster:             %s\n",
		   YES_NO(cap->tegra_lp_cluster));
	seq_printf(f, "power rate samples:             %s\n",
		   YES_NO(cap->power_rate));

	seq_printf(f, "l2 cache:                       %s\n",
		   YES_NO(cap->l2_cache));
	if (cap->l2_cache) {
		seq_printf(f, "multiple l2 events:             %s\n",
			   YES_NO(cap->l2_multiple_events));
	}

	seq_printf(f, "support polling mode:           %s\n",
		   YES_NO(cap->blocked_read));
	seq_printf(f, "backtrace from the kernel ctx:  %s\n",
		   YES_NO(extra & QUADD_COMM_CAP_EXTRA_BT_KERNEL_CTX));
	seq_printf(f, "send mmap regions at the start: %s\n",
		   YES_NO(extra & QUADD_COMM_CAP_EXTRA_GET_MMAP));
	seq_printf(f, "group samples:                  %s\n",
		   YES_NO(extra & QUADD_COMM_CAP_EXTRA_GROUP_SAMPLES));

	seq_puts(f, "\n");
	seq_puts(f, "Supported events:\n");
	seq_printf(f, "cpu_cycles:                     %s\n",
		   YES_NO(event->cpu_cycles));
	seq_printf(f, "instructions:                   %s\n",
		   YES_NO(event->instructions));
	seq_printf(f, "branch_instructions:            %s\n",
		   YES_NO(event->branch_instructions));
	seq_printf(f, "branch_misses:                  %s\n",
		   YES_NO(event->branch_misses));
	seq_printf(f, "bus_cycles:                     %s\n",
		   YES_NO(event->bus_cycles));
	seq_printf(f, "l1_dcache_read_misses:          %s\n",
		   YES_NO(event->l1_dcache_read_misses));
	seq_printf(f, "l1_dcache_write_misses:         %s\n",
		   YES_NO(event->l1_dcache_write_misses));
	seq_printf(f, "l1_icache_misses:               %s\n",
		   YES_NO(event->l1_icache_misses));
	seq_printf(f, "l2_dcache_read_misses:          %s\n",
		   YES_NO(event->l2_dcache_read_misses));
	seq_printf(f, "l2_dcache_write_misses:         %s\n",
		   YES_NO(event->l2_dcache_write_misses));
	seq_printf(f, "l2_icache_misses:               %s\n",
		   YES_NO(event->l2_icache_misses));

	return 0;
}

static int show_capabilities_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_capabilities, NULL);
}

static const struct file_operations capabilities_proc_fops = {
	.open		= show_capabilities_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int show_status(struct seq_file *f, void *offset)
{
	unsigned int status;
	unsigned int is_auth_open, active;
	struct quadd_module_state s;

	quadd_get_state(&s);
	status = s.reserved[QUADD_MOD_STATE_IDX_STATUS];

	active = status & QUADD_MOD_STATE_STATUS_IS_ACTIVE;
	is_auth_open = status & QUADD_MOD_STATE_STATUS_IS_AUTH_OPEN;

	seq_printf(f, "status:          %s\n", active ? "active" : "waiting");
	seq_printf(f, "auth:            %s\n", YES_NO(is_auth_open));
	seq_printf(f, "all samples:     %llu\n", s.nr_all_samples);
	seq_printf(f, "skipped samples: %llu\n", s.nr_skipped_samples);

	return 0;
}

static int show_status_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, show_status, NULL);
}

static const struct file_operations status_proc_fops = {
	.open		= show_status_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

void quadd_proc_init(struct quadd_ctx *context)
{
	ctx = context;

	proc_mkdir("quadd", NULL);
	proc_create("quadd/version", 0, NULL, &version_proc_fops);
	proc_create("quadd/capabilities", 0, NULL, &capabilities_proc_fops);
	proc_create("quadd/status", 0, NULL, &status_proc_fops);
}

void quadd_proc_deinit(void)
{
	remove_proc_entry("quadd/version", NULL);
	remove_proc_entry("quadd/capabilities", NULL);
	remove_proc_entry("quadd/status", NULL);
	remove_proc_entry("quadd", NULL);
}

#endif	/* CONFIG_PROC_FS */
