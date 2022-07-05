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
#include <swpm_mem_v6886.h>

#undef swpm_dbg_log
#define swpm_dbg_log(fmt, args...) \
	do { \
		int l = scnprintf(p, sz, fmt, ##args); \
		p += l; \
		sz -= l; \
	} while (0)

static ssize_t dram_bw_read(char *ToUser, size_t sz, void *priv)
{
	char *p = ToUser;
	unsigned long flags;
	unsigned int i = 0;

	if (!ToUser)
		return -EINVAL;

	spin_lock_irqsave(&mem_swpm_spinlock, flags);
	for (i = 0; i < MAX_EMI_NUM; i++) {
		swpm_dbg_log("DRAM BW(%d) R/W=%d/%d\n",
			mem_idx_snap.read_bw[i],
			mem_idx_snap.write_bw[i]);
	}
	spin_unlock_irqrestore(&mem_swpm_spinlock, flags);

	return p - ToUser;
}

static const struct mtk_swpm_sysfs_op dram_bw_fops = {
	.fs_read = dram_bw_read,
};

int __init swpm_mem_v6886_dbg_init(void)
{
	mtk_swpm_sysfs_entry_func_node_add("dram_bw"
			, 0444, &dram_bw_fops, NULL, NULL);

	swpm_mem_v6886_init();

	pr_notice("swpm mem install success\n");

	return 0;
}

void __exit swpm_mem_v6886_dbg_exit(void)
{
	swpm_mem_v6886_exit();
}

module_init(swpm_mem_v6886_dbg_init);
module_exit(swpm_mem_v6886_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("v6886 SWPM mem debug module");
MODULE_AUTHOR("MediaTek Inc.");
