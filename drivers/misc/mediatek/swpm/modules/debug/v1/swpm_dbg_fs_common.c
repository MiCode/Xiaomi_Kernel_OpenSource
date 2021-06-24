// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/console.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>

#include <swpm_dbg_common_v1.h>
#include <swpm_dbg_fs_common.h>
#include <swpm_module.h>

#if IS_ENABLED(CONFIG_MTK_SWPM_PERF_ARMV8_PMU)
#include <swpm_perf_arm_pmu.h>
#endif

#define SWPM_DGB_ENABLE_NODE	"/proc/swpm/enable"

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

/* TODO: TBD swpm_status */
unsigned int swpm_status;
EXPORT_SYMBOL(swpm_status);

unsigned int swpm_gpu_debug;
EXPORT_SYMBOL(swpm_gpu_debug);

static ssize_t gpu_debug_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("SWPM gpu_debug is %s\n",
		(swpm_gpu_debug == true) ? "enabled" : "disabled");

	if (swpm_gpu_debug == true) {
		/* TODO: TBD */
	}
	return p - ToUser;
}

static ssize_t gpu_debug_write(char *FromUser, size_t sz, void *priv)
{
	int enable_time;

	if (!FromUser)
		return -EINVAL;

	if (sscanf(FromUser, "%d", &enable_time) == 1) {
		/* TODO: TBD for swpm_gpu_debug */
		swpm_gpu_debug = (enable_time) ? true : false;
	} else {
		/* swpm_dbg_log("echo 1/0 > /proc/swpm/gpu_debug\n"); */
	}
	return sz;
}

static const struct mtk_swpm_sysfs_op gpu_debug_fops = {
	.fs_read = gpu_debug_read,
	.fs_write = gpu_debug_write,
};

#if IS_ENABLED(CONFIG_MTK_SWPM_PERF_ARMV8_PMU)
static ssize_t swpm_arm_pmu_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("SWPM arm pmu is %s\n",
		(swpm_arm_pmu_get_status()) ? "enabled" : "disabled");

	return p - ToUser;
}

static ssize_t swpm_arm_pmu_write(char *FromUser, size_t sz, void *priv)
{
	int enable;

	if (!FromUser)
		return -EINVAL;

	if (sscanf(FromUser, "%d", &enable) == 1)
		swpm_arm_pmu_enable_all(enable);

	return sz;
}

static const struct mtk_swpm_sysfs_op swpm_arm_pmu_fops = {
	.fs_read = swpm_arm_pmu_read,
	.fs_write = swpm_arm_pmu_write,
};
#endif

static void swpm_dbg_fs_init(void)
{
	/* mtk_swpm_sysfs_root_entry_create(); */
	mtk_swpm_sysfs_entry_func_node_add("gpu_debug"
			, 0644, &gpu_debug_fops, NULL, NULL);

#if IS_ENABLED(CONFIG_MTK_SWPM_PERF_ARMV8_PMU)
	mtk_swpm_sysfs_entry_func_node_add("swpm_arm_pmu"
			, 0644, &swpm_arm_pmu_fops, NULL, NULL);
#endif
}

void  swpm_dbg_common_fs_exit(void)
{
}
EXPORT_SYMBOL(swpm_dbg_common_fs_exit);

int  swpm_dbg_common_fs_init(void)
{
	swpm_dbg_fs_init();

	return 0;
}
EXPORT_SYMBOL(swpm_dbg_common_fs_init);
MODULE_LICENSE("GPL");
