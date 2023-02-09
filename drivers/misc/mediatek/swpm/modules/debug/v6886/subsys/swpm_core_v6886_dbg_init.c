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
#include <swpm_core_v6886.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

static unsigned int core_static_data_tmp;
static ssize_t core_static_replace_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;

	if (!ToUser)
		return -EINVAL;

	swpm_dbg_log("echo <val> > /proc/swpm/core_static_replace\n");
	swpm_dbg_log("default: %d, replaced %d (valid:0~999)\n",
		   swpm_core_static_data_get(),
		   core_static_data_tmp);

	return p - ToUser;
}

static ssize_t core_static_replace_write(char *FromUser, size_t sz, void *priv)
{
	unsigned int val = 0;
	int ret;

	ret = -EINVAL;

	if (!FromUser)
		goto out;

	if (sz >= MTK_SWPM_SYSFS_BUF_WRITESZ)
		goto out;

	ret = -EPERM;
	if (!kstrtouint(FromUser, 0, &val)) {
		core_static_data_tmp = (val < 1000) ? val : 0;

		/* reset core static power data */
		swpm_core_static_replaced_data_set(core_static_data_tmp);
		swpm_core_static_data_init();
		ret = sz;
	}

out:
	return ret;
}

static const struct mtk_swpm_sysfs_op core_static_replace_fops = {
	.fs_read = core_static_replace_read,
	.fs_write = core_static_replace_write,
};

int __init swpm_core_v6886_dbg_init(void)
{
	mtk_swpm_sysfs_entry_func_node_add("core_static"
			, 0644, &core_static_replace_fops, NULL, NULL);

	swpm_core_v6886_init();

	pr_notice("swpm core install success\n");

	return 0;
}

void __exit swpm_core_v6886_dbg_exit(void)
{
	swpm_core_v6886_exit();
}

module_init(swpm_core_v6886_dbg_init);
module_exit(swpm_core_v6886_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6886 SWPM core debug module");
MODULE_AUTHOR("MediaTek Inc.");
