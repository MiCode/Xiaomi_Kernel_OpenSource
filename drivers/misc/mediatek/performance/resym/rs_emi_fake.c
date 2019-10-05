/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/average.h>
#include <linux/topology.h>
#include <linux/vmalloc.h>
#include <asm/div64.h>

#include <linux/notifier.h>
#include <mt-plat/mtk_perfobserver.h>

#include "rs_base.h"
#include "rs_trace.h"

#define RSE_DEBUGFS_ENTRY(name) \
static int rse_##name##_open(struct inode *i, struct file *file) \
{ \
	return single_open(file, rse_##name##_show, i->i_private); \
} \
\
static const struct file_operations rse_##name##_fops = { \
	.owner = THIS_MODULE, \
	.open = rse_##name##_open, \
	.read = seq_read, \
	.write = rse_##name##_write, \
	.llseek = seq_lseek, \
	.release = single_release, \
}

#define RSE_SYSTRACE_LIST(macro) \
	macro(MANDATORY, 0), \
	macro(CPURESCUE, 1), \
	macro(QUAWEITIME, 2), \
	macro(MAX, 3), \

#define RSE_GENERATE_ENUM(name, shft) RSE_DEBUG_##name = 1U << shft
enum {
	RSE_SYSTRACE_LIST(RSE_GENERATE_ENUM)
};

#define RSE_GENERATE_STRING(name, unused) #name
static const char * const mask_string[] = {
	RSE_SYSTRACE_LIST(RSE_GENERATE_STRING)
};

#define rse_systrace_c(mask, pid, val, fmt...) \
	do { \
		if (rse_systrace_mask & mask) \
			__rs_systrace_c(pid, val, fmt); \
	} while (0)

#define rse_systrace_c_uint64(mask, pid, val, fmt...) \
	do { \
		if (rse_systrace_mask & mask) \
			__rs_systrace_c_uint64(pid, val, fmt); \
	} while (0)

#define rse_systrace_b(mask, tgid, fmt, ...) \
	do { \
		if (rse_systrace_mask & mask) \
			__rs_systrace_b(tgid, fmt); \
	} while (0)

#define rse_systrace_e(mask) \
	do { \
		if (rse_systrace_mask & mask) \
			__rs_systrace_e(); \
	} while (0)

#define rse_systrace_c_log(pid, val, fmt...) \
	rse_systrace_c(RSE_DEBUG_MANDATORY, pid, val, fmt)

#define rse_systrace_c_uint64_log(pid, val, fmt...) \
	rse_systrace_c_uint64(RSE_DEBUG_MANDATORY, pid, val, fmt)

static uint32_t rse_systrace_mask;

static int rse_systrace_mask_show(struct seq_file *m, void *unused)
{
	int i;

	seq_puts(m, " Current enabled systrace:\n");
	for (i = 0; (1U << i) < RSE_DEBUG_MAX; i++)
		seq_printf(m, "  %-*s ... %s\n", 12, mask_string[i],
			   rse_systrace_mask & (1U << i) ?
			     "On" : "Off");

	return 0;
}

static ssize_t rse_systrace_mask_write(struct file *flip,
			const char *ubuf, size_t cnt, loff_t *data)
{
	uint32_t val;
	int ret;

	ret = kstrtou32_from_user(ubuf, cnt, 16, &val);
	if (ret)
		return ret;

	val = val & (RSE_DEBUG_MAX - 1U);

	rse_systrace_mask = val;

	return cnt;
}

RSE_DEBUGFS_ENTRY(systrace_mask);

static int rs_emi_fake_callback(struct notifier_block *nb,
			unsigned long val, void *data)
{
	struct pob_rs_quaweitime_info *prqi = data;

	switch (val) {
	case POB_RS_CPURESCUE_START:
		rse_systrace_c(RSE_DEBUG_CPURESCUE, 0, 1, "REF CPU Rescuing");
		break;
	case POB_RS_CPURESCUE_END:
		rse_systrace_c(RSE_DEBUG_CPURESCUE, 0, 0, "REF CPU Rescuing");
		break;
	case POB_RS_QUAWEITIME:
		if (prqi) {
			rse_systrace_c(RSE_DEBUG_QUAWEITIME, 0,
					prqi->MaxQWCPUTime,
					"REF MAXQWCPUTime");
			rse_systrace_c(RSE_DEBUG_QUAWEITIME, 0,
					prqi->MaxQWGPUTime,
					"REF MAXQWGPUTime");
		}
		break;
	default:

		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block rs_emi_fake_notifier = {
	.notifier_call = rs_emi_fake_callback,
};

static int __init rse_debugfs_init(struct dentry *pob_debugfs_dir)
{
	if (!pob_debugfs_dir)
		return -ENODEV;

	debugfs_create_file("systrace_mask",
			    0644,
			    pob_debugfs_dir,
			    NULL,
			    &rse_systrace_mask_fops);

	return 0;
}

static int __init mtk_rs_emi_fake_init(void)
{
	struct dentry *rs_debugfs_dir = NULL;
	struct dentry *rs_emi_debugfs_dir = NULL;

	rs_debugfs_dir = rsm_debugfs_dir;

	if (!rs_debugfs_dir)
		return -ENOENT;

	rs_emi_debugfs_dir = debugfs_create_dir("emi", rs_debugfs_dir);

	if (!rs_emi_debugfs_dir)
		return -ENODEV;

	rse_debugfs_init(rs_emi_debugfs_dir);

	rse_systrace_mask = RSE_DEBUG_MANDATORY;

	pob_rs_register_client(&rs_emi_fake_notifier);

	return 0;
}

//device_initcall(mtk_rs_emi_fake_init);
late_initcall(mtk_rs_emi_fake_init);

