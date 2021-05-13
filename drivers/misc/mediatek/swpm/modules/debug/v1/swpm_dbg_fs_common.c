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
#include <swpm_module.h>
#include <mtk_swpm_common_sysfs.h>
#include <mtk_swpm_sysfs.h>

#define SWPM_DGB_ENABLE_NODE	"/proc/swpm/enable"

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

/* TODO: TBD swpm_status */
static unsigned int swpm_status;
static unsigned int swpm_gpu_debug;

static ssize_t enable_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("SWPM status = 0x%x\n", swpm_status);

	return p - ToUser;
}

static ssize_t enable_write(char *FromUser,
						   size_t sz, void *priv)
{
	int type, enable;

	if (!FromUser)
		return -EINVAL;

	if (sscanf(FromUser, "%d %d", &type, &enable) == 2) {
		/* TODO: TBD for swpm_status */
	} else {
		/* swpm_dbg_log("echo <type or 65535> <0 or 1> > /proc/swpm/enable\n"); */
	}
	return sz;
}

static const struct mtk_swpm_sysfs_op enable_fops = {
	.fs_read = enable_read,
	.fs_write = enable_write,
};

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

static ssize_t gpu_debug_write(char *FromUser,
						   size_t sz, void *priv)
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

static ssize_t dump_power_read(char *ToUser, size_t sz, void *priv)
{
/* TODO: TBD */
#if 0
	char *ptr = ToUser;
	size_t mSize = 0;
	int i;

	for (i = 0; i < NR_POWER_RAIL; i++) {
		ptr += scnprintf(ptr + mSize, sz - mSize, "%s",
			swpm_power_rail_to_string((enum power_rail)i));
		if (i != NR_POWER_RAIL - 1)
			ptr += sprintf(ptr, "/");
		else
			ptr += sprintf(ptr, " = ");
	}

	for (i = 0; i < NR_POWER_RAIL; i++) {
		ptr += scnprintf(ptr + mSize, sz - mSize, "%d",
			swpm_get_avg_power((enum power_rail)i, avg_window));
		if (i != NR_POWER_RAIL - 1)
			ptr += sprintf(ptr, "/");
		else
			ptr += sprintf(ptr, " uA");
	}

	WARN_ON(buf_sz - mSize <= 0);

	return mSize;
#else
	char *p = ToUser;

	return p - ToUser;
#endif
}

static const struct mtk_swpm_sysfs_op dump_power_fops = {
	.fs_read = dump_power_read,
};

static void swpm_dbg_fs_init(void)
{
	/* mtk_swpm_sysfs_root_entry_create(); */
	mtk_swpm_sysfs_entry_func_node_add("enable"
			, 0644, &enable_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("gpu_debug"
			, 0644, &gpu_debug_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("dump_power"
			, 0444, &dump_power_fops, NULL, NULL);
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
