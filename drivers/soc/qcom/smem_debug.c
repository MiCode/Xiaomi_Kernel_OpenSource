/* arch/arm/mach-msm/smem_debug.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009-2013, The Linux Foundation. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/jiffies.h>

#include <soc/qcom/smem.h>

#include "smem_private.h"

#if defined(CONFIG_DEBUG_FS)

#define SZ_SMEM_ALLOCATION_TABLE 8192

static void debug_read_mem(struct seq_file *s)
{
	unsigned n;
	struct smem_heap_info *heap_info;
	struct smem_heap_entry *toc;

	heap_info = smem_find(SMEM_HEAP_INFO, sizeof(struct smem_heap_info),
						0,
						SMEM_ANY_HOST_FLAG);
	if (!heap_info) {
		seq_puts(s, "SMEM_HEAP_INFO is NULL\n");
		return;
	}
	toc = smem_find(SMEM_ALLOCATION_TABLE, SZ_SMEM_ALLOCATION_TABLE,
							0, SMEM_ANY_HOST_FLAG);
	if (!toc) {
		seq_puts(s, "SMEM_ALLOCATION_TABLE is NULL\n");
		return;
	}

	seq_printf(s, "heap: init=%d free=%d remain=%d\n",
		       heap_info->initialized,
		       heap_info->free_offset,
		       heap_info->heap_remaining);

	for (n = 0; n < SMEM_NUM_ITEMS; n++) {
		if (toc[n].allocated == 0)
			continue;
		seq_printf(s, "%04d: offset %08x size %08x\n",
			       n, toc[n].offset, toc[n].size);
	}
}

static void debug_read_smem_version(struct seq_file *s)
{
	uint32_t n, version;

	for (n = 0; n < 32; n++) {
		version = smem_get_version(n);
		seq_printf(s, "entry %d: smem = %d  proc_comm = %d\n", n,
			       version >> 16,
			       version & 0xffff);
	}
}

static void debug_read_build_id(struct seq_file *s)
{
	unsigned size;
	void *data;

	data = smem_get_entry(SMEM_HW_SW_BUILD_ID, &size, 0,
							SMEM_ANY_HOST_FLAG);
	if (!data)
		return;

	seq_write(s, data, size);
}

static int debugfs_show(struct seq_file *s, void *data)
{
	void (*show)(struct seq_file *) = s->private;

	show(s);

	return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, debugfs_show, inode->i_private);
}

static const struct file_operations debug_ops = {
	.open = debug_open,
	.release = single_release,
	.read = seq_read,
	.llseek = seq_lseek,
};

static void debug_create(const char *name, umode_t mode,
			 struct dentry *dent,
			 void (*show)(struct seq_file *))
{
	struct dentry *file;

	file = debugfs_create_file(name, mode, dent, show, &debug_ops);
	if (!file)
		pr_err("%s: unable to create file '%s'\n", __func__, name);
}

static int __init smem_debugfs_init(void)
{
	struct dentry *dent;

	dent = debugfs_create_dir("smem", 0);
	if (IS_ERR(dent))
		return PTR_ERR(dent);

	debug_create("mem", 0444, dent, debug_read_mem);
	debug_create("version", 0444, dent, debug_read_smem_version);

	/* NNV: this is google only stuff */
	debug_create("build", 0444, dent, debug_read_build_id);

	return 0;
}

late_initcall(smem_debugfs_init);
#endif
