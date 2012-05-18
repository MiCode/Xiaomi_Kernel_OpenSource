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

/*default log levels is error for everything*/
#define KGSL_LOG_LEVEL_DEFAULT 3
#define KGSL_LOG_LEVEL_MAX     7

struct dentry *kgsl_debugfs_dir;
static struct dentry *pm_d_debugfs;

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

void kgsl_core_debugfs_init(void)
{
	kgsl_debugfs_dir = debugfs_create_dir("kgsl", 0);
}

void kgsl_core_debugfs_close(void)
{
	debugfs_remove_recursive(kgsl_debugfs_dir);
}
