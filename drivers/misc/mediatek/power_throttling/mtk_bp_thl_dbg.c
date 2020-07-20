// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Samuel Hsieh <samuel.hsieh@mediatek.com>
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include "mtk_bp_thl.h"

static unsigned int ut_level;

void bp_thl_ut_cb(enum BATTERY_PERCENT_LEVEL_TAG level_val)
{
	ut_level = level_val;
	pr_info("[%s] get %d\n", __func__, level_val);
}

static ssize_t bp_thl_dbg_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	int len = 0, ut_input = 0;
	struct power_supply *psy;

	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;
	desc[len] = '\0';

	if (kstrtoint(desc, 10, &ut_input) == 0) {
		if (ut_input == 1 || ut_input == 2) {
			register_bp_thl_notify(&bp_thl_ut_cb, BATTERY_PERCENT_PRIO_UT);
			set_bp_thl_ut_status(ut_input);
			psy = power_supply_get_by_name("battery");
			power_supply_changed(psy);
		} else if (ut_input == 0) {
			unregister_bp_thl_notify(BATTERY_PERCENT_PRIO_UT);
			set_bp_thl_ut_status(ut_input);
		} else
			pr_info("[%s] wrong number (%d)\n", __func__, ut_input);
	} else
		pr_info("[%s] wrong input (%s)\n", __func__, desc);

	return count;
}

static int bp_thl_dbg_show(struct seq_file *s, void *unused)
{
	seq_printf(s, "%d\n", ut_level);
	return 0;
}

static int bp_thl_dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, bp_thl_dbg_show, NULL);
}

static const struct file_operations bp_thl_dbg_ops = {
	.open    = bp_thl_dbg_open,
	.read    = seq_read,
	.write   = bp_thl_dbg_write,
	.llseek  = seq_lseek,
	.release = single_release,
};

static int bp_thl_create_debugfs(void)
{
	struct dentry *dir = NULL;

	dir = debugfs_create_dir("bp_thl", NULL);
	if (!dir) {
		pr_notice("fail to create bp_thl\n");
		return -ENOMEM;
	}

	debugfs_create_file("bp_thl_ut", 0664, dir, NULL, &bp_thl_dbg_ops);

	return 0;
}

static int __init mtk_bp_thl_dbg_init(void)
{
	int ret;

	ret = bp_thl_create_debugfs();
	return ret;
}

static void __exit mtk_bp_thl_dbg_exit(void)
{
}

module_init(mtk_bp_thl_dbg_init);
module_exit(mtk_bp_thl_dbg_exit);

MODULE_AUTHOR("Samuel Hsieh");
MODULE_DESCRIPTION("MTK battery percent throttling debug");
MODULE_LICENSE("GPL");
