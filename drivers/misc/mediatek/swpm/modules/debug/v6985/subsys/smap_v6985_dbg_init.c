// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
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

/* TODO: independent in after project */
#include <swpm_dbg_fs_common.h>
#include <swpm_dbg_common_v1.h>
#include <swpm_module.h>

#include <smap_v6985.h>

#undef smap_dbg_log
#define smap_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

#define MAX_SMAP_CONFIG_HISTORY (32)

static unsigned int smap_config_history[MAX_SMAP_CONFIG_HISTORY];
static char smap_config_update_flag[MAX_SMAP_CONFIG_HISTORY];

static ssize_t smap_config_read(char *ToUser, size_t sz, void *priv)
{
	int i;
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	for (i = 0; i < MAX_SMAP_CONFIG_HISTORY; i++) {
		if (smap_config_update_flag[i])
			smap_dbg_log("(%d) 0x%X\n", i, smap_config_history[i]);
	}

	return p - ToUser;
}

static ssize_t smap_config_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int type, val, type_idx;
	int ret;

	ret = -EINVAL;

	if (!FromUser)
		goto out;

	if (sz >= MTK_SWPM_SYSFS_BUF_WRITESZ)
		goto out;

	ret = -EPERM;
	if (sscanf(FromUser, "%x %x", &type, &val) == 2) {
		/* record config history */
		if ((type & 0xFFFF) < MAX_SMAP_CONFIG_HISTORY) {
			type_idx = type & 0xFFFF;
			smap_config_history[type_idx] = val;
			smap_config_update_flag[type_idx] = 1;
		}

		/* TODO: independent SCMI call in after project */
		swpm_set_only_cmd(type, val, SMAP_SET_CFG, SMAP_CMD_TYPE);
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op config_fops = {
	.fs_read = smap_config_read,
	.fs_write = smap_config_write,
};

static ssize_t smap_icnt_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	smap_dbg_log("i2max_cnt=%d\n", smap_get_data(0));
	smap_dbg_log("imax_cnt=%d\n", smap_get_data(1));

	return p - ToUser;
}

static ssize_t smap_icnt_write(char *FromUser, size_t sz, void *priv)
{
	if (!FromUser)
		return -EINVAL;

	return sz;
}

static const struct mtk_swpm_sysfs_op smap_icnt_fops = {
	.fs_read = smap_icnt_read,
	.fs_write = smap_icnt_write,
};


int __init smap_v6985_dbg_init(void)
{
	int i;

	for (i = 0; i < MAX_SMAP_CONFIG_HISTORY; i++) {
		smap_config_history[i] = 0;
		smap_config_update_flag[i] = 0;
	}

	mtk_swpm_sysfs_entry_func_node_add("smap_config"
			, 0644, &config_fops, NULL, NULL);
	mtk_swpm_sysfs_entry_func_node_add("dump_smap_cnt"
			, 0444, &smap_icnt_fops, NULL, NULL);

	smap_v6985_init();

	pr_notice("smap install success\n");

	return 0;
}

void __exit smap_v6985_dbg_exit(void)
{
}

module_init(smap_v6985_dbg_init);
module_exit(smap_v6985_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6985 SMAP debug module");
MODULE_AUTHOR("MediaTek Inc.");
