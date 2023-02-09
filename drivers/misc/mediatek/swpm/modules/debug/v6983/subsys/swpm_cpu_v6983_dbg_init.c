// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/cpu.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/types.h>

#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>
#include <swpm_cpu_v6983.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

/* change the pmu log in per mini-second mode */
/* default: output pmu log with voltage change event */
static unsigned int pmu_ms_mode;
static ssize_t pmu_ms_mode_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("echo <0/1> > /proc/swpm/pmu_ms_mode\n");
	swpm_dbg_log("%d\n", pmu_ms_mode);

	return p - ToUser;
}

static ssize_t pmu_ms_mode_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int enable = 0;
	int ret;

	ret = -EINVAL;

	if (!FromUser)
		goto out;

	if (sz >= MTK_SWPM_SYSFS_BUF_WRITESZ)
		goto out;

	ret = -EPERM;
	if (!kstrtouint(FromUser, 0, &enable)) {
		pmu_ms_mode = enable;
		swpm_set_only_cmd(0, pmu_ms_mode, CPU_SET_PMU_MS, CPU_CMD_TYPE);
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op pmu_ms_mode_fops = {
	.fs_read = pmu_ms_mode_read,
	.fs_write = pmu_ms_mode_write,
};

int __init swpm_cpu_v6983_dbg_init(void)
{
	mtk_swpm_sysfs_entry_func_node_add("pmu_ms_mode"
			, 0644, &pmu_ms_mode_fops, NULL, NULL);

	swpm_cpu_v6983_init();

	pr_notice("swpm cpu install success\n");

	return 0;
}

void __exit swpm_cpu_v6983_dbg_exit(void)
{
	swpm_cpu_v6983_exit();
}

module_init(swpm_cpu_v6983_dbg_init);
module_exit(swpm_cpu_v6983_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6983 SWPM cpu debug module");
MODULE_AUTHOR("MediaTek Inc.");
