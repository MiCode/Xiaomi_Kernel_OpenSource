/* Copyright (c) 2002,2008-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/debugfs.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"

/*default log levels is error for everything*/
#define KGSL_LOG_LEVEL_DEFAULT 3
#define KGSL_LOG_LEVEL_MAX     7

struct dentry *kgsl_debugfs_dir;
static struct dentry *pm_d_debugfs;
struct dentry *proc_d_debugfs;

static int pm_dump_set(void *data, u64 val)
{
	struct kgsl_device *device = data;

	if (val) {
		mutex_lock(&device->mutex);
		kgsl_postmortem_dump(device, 1);
		mutex_unlock(&device->mutex);
	}

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(pm_dump_fops,
			NULL,
			pm_dump_set, "%llu\n");

static int pm_regs_enabled_set(void *data, u64 val)
{
	struct kgsl_device *device = data;
	device->pm_regs_enabled = val ? 1 : 0;
	return 0;
}

static int pm_regs_enabled_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	*val = device->pm_regs_enabled;
	return 0;
}

static int pm_ib_enabled_set(void *data, u64 val)
{
	struct kgsl_device *device = data;
	device->pm_ib_enabled = val ? 1 : 0;
	return 0;
}

static int pm_ib_enabled_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	*val = device->pm_ib_enabled;
	return 0;
}


DEFINE_SIMPLE_ATTRIBUTE(pm_regs_enabled_fops,
			pm_regs_enabled_get,
			pm_regs_enabled_set, "%llu\n");

DEFINE_SIMPLE_ATTRIBUTE(pm_ib_enabled_fops,
			pm_ib_enabled_get,
			pm_ib_enabled_set, "%llu\n");

static inline int kgsl_log_set(unsigned int *log_val, void *data, u64 val)
{
	*log_val = min((unsigned int)val, (unsigned int)KGSL_LOG_LEVEL_MAX);
	return 0;
}

#define KGSL_DEBUGFS_LOG(__log)                         \
static int __log ## _set(void *data, u64 val)           \
{                                                       \
	struct kgsl_device *device = data;              \
	return kgsl_log_set(&device->__log, data, val); \
}                                                       \
static int __log ## _get(void *data, u64 *val)	        \
{                                                       \
	struct kgsl_device *device = data;              \
	*val = device->__log;                           \
	return 0;                                       \
}                                                       \
DEFINE_SIMPLE_ATTRIBUTE(__log ## _fops,                 \
__log ## _get, __log ## _set, "%llu\n");                \

KGSL_DEBUGFS_LOG(drv_log);
KGSL_DEBUGFS_LOG(cmd_log);
KGSL_DEBUGFS_LOG(ctxt_log);
KGSL_DEBUGFS_LOG(mem_log);
KGSL_DEBUGFS_LOG(pwr_log);

void kgsl_device_debugfs_init(struct kgsl_device *device)
{
	if (kgsl_debugfs_dir && !IS_ERR(kgsl_debugfs_dir))
		device->d_debugfs = debugfs_create_dir(device->name,
						       kgsl_debugfs_dir);

	if (!device->d_debugfs || IS_ERR(device->d_debugfs))
		return;

	device->cmd_log = KGSL_LOG_LEVEL_DEFAULT;
	device->ctxt_log = KGSL_LOG_LEVEL_DEFAULT;
	device->drv_log = KGSL_LOG_LEVEL_DEFAULT;
	device->mem_log = KGSL_LOG_LEVEL_DEFAULT;
	device->pwr_log = KGSL_LOG_LEVEL_DEFAULT;

	debugfs_create_file("log_level_cmd", 0644, device->d_debugfs, device,
			    &cmd_log_fops);
	debugfs_create_file("log_level_ctxt", 0644, device->d_debugfs, device,
			    &ctxt_log_fops);
	debugfs_create_file("log_level_drv", 0644, device->d_debugfs, device,
			    &drv_log_fops);
	debugfs_create_file("log_level_mem", 0644, device->d_debugfs, device,
				&mem_log_fops);
	debugfs_create_file("log_level_pwr", 0644, device->d_debugfs, device,
				&pwr_log_fops);

	/* Create postmortem dump control files */

	pm_d_debugfs = debugfs_create_dir("postmortem", device->d_debugfs);

	if (IS_ERR(pm_d_debugfs))
		return;

	debugfs_create_file("dump",  0600, pm_d_debugfs, device,
			    &pm_dump_fops);
	debugfs_create_file("regs_enabled", 0644, pm_d_debugfs, device,
			    &pm_regs_enabled_fops);
	debugfs_create_file("ib_enabled", 0644, pm_d_debugfs, device,
				    &pm_ib_enabled_fops);

}

static const char * const memtype_strings[] = {
	"gpumem",
	"pmem",
	"ashmem",
	"usermap",
	"ion",
};

static const char *memtype_str(int memtype)
{
	if (memtype < ARRAY_SIZE(memtype_strings))
		return memtype_strings[memtype];
	return "unknown";
}

static int process_mem_print(struct seq_file *s, void *unused)
{
	struct kgsl_mem_entry *entry;
	struct rb_node *node;
	struct kgsl_process_private *private = s->private;
	char flags[4];
	char usage[16];
	unsigned int align;

	spin_lock(&private->mem_lock);
	seq_printf(s, "%8s %8s %5s %10s %16s %5s\n",
		   "gpuaddr", "size", "flags", "type", "usage", "sglen");
	for (node = rb_first(&private->mem_rb); node; node = rb_next(node)) {
		struct kgsl_memdesc *m;

		entry = rb_entry(node, struct kgsl_mem_entry, node);
		m = &entry->memdesc;

		flags[0] = m->priv & KGSL_MEMFLAGS_GLOBAL ?  'g' : '-';
		flags[1] = m->priv & KGSL_MEMFLAGS_GPUREADONLY ? 'r' : '-';

		align = (m->priv & KGSL_MEMALIGN_MASK) >> KGSL_MEMALIGN_SHIFT;
		if (align >= ilog2(SZ_1M))
			flags[2] = 'L';
		else if (align >= ilog2(SZ_64K))
			flags[2] = 'l';
		else
			flags[2] = '-';

		flags[3] = '\0';

		kgsl_get_memory_usage(usage, sizeof(usage), m->priv);

		seq_printf(s, "%08x %8d %5s %10s %16s %5d\n",
			   m->gpuaddr, m->size, flags,
			   memtype_str(entry->memtype), usage, m->sglen);
	}
	spin_unlock(&private->mem_lock);
	return 0;
}

static int process_mem_open(struct inode *inode, struct file *file)
{
	return single_open(file, process_mem_print, inode->i_private);
}

static const struct file_operations process_mem_fops = {
	.open = process_mem_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

void
kgsl_process_init_debugfs(struct kgsl_process_private *private)
{
	unsigned char name[16];

	snprintf(name, sizeof(name), "%d", private->pid);

	private->debug_root = debugfs_create_dir(name, proc_d_debugfs);
	debugfs_create_file("mem", 0400, private->debug_root, private,
			    &process_mem_fops);
}

void kgsl_core_debugfs_init(void)
{
	kgsl_debugfs_dir = debugfs_create_dir("kgsl", 0);
	proc_d_debugfs = debugfs_create_dir("proc", kgsl_debugfs_dir);
}

void kgsl_core_debugfs_close(void)
{
	debugfs_remove_recursive(kgsl_debugfs_dir);
}
