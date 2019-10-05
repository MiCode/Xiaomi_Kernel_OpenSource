// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "cmdq-util.h"

struct cmdq_util {
	bool ena;
	spinlock_t lock;
	char *buf; // ARG_MAX
	u32 len;
	u64 nsec;
	char caller[TASK_COMM_LEN]; // TODO

	struct dentry *fs_status;
	struct dentry *fs_record;
};
static struct cmdq_util *g_util;

void cmdq_util_enable(void)
{
	g_util->nsec = sched_clock();
	g_util->ena = true;
}
EXPORT_SYMBOL(cmdq_util_enable);

void cmdq_util_disable(void)
{
	g_util->ena = false;
}
EXPORT_SYMBOL(cmdq_util_disable);

s32 cmdq_util_save_first_error(const char *str, ...)
{
	unsigned long flags;
	va_list args;
	s32 len;

	if (!g_util->ena)
		return -EFAULT;

	spin_lock_irqsave(&g_util->lock, flags);
	va_start(args, str);
	len = vsnprintf(
		g_util->buf + g_util->len, ARG_MAX - g_util->len, str, args);
	g_util->len += len;

	if (g_util->len >= ARG_MAX) {
		cmdq_util_disable();
		cmdq_msg("Error0 buf is full");
	}
	va_end(args);
	spin_unlock_irqrestore(&g_util->lock, flags);
	return 0;
}
EXPORT_SYMBOL(cmdq_util_save_first_error);

static int cmdq_util_print_status(struct seq_file *seq, void *arg)
{
	struct cmdq_util *util = (struct cmdq_util *)seq->private;
	u64 sec = util->nsec;
	unsigned long nsec = do_div(sec, 1000000000);

	if (!util->len)
		return 0;

	seq_printf(seq, "======== [cmdq] first error [%5llu.%06lu] ========\n",
		sec, nsec);
	seq_printf(seq, "%s", util->buf);
	return 0;
}

static int cmdq_util_print_record(struct seq_file *seq, void *data)
{
	return 0;
}

static int cmdq_util_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_print_status, inode->i_private);
}

static int cmdq_util_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_print_record, inode->i_private);
}

static const struct file_operations cmdq_util_status_fops = {
	.open = cmdq_util_status_open,
};

static const struct file_operations cmdq_util_record_fops = {
	.open = cmdq_util_record_open,
};

static int __init cmdq_util_init(void)
{
	struct cmdq_util *util;
	struct dentry *dir;

	util = kzalloc(sizeof(*util), GFP_KERNEL);
	if (!util)
		return -ENOMEM;
	g_util = util;

	spin_lock_init(&util->lock);

	util->buf = kzalloc(ARG_MAX, GFP_KERNEL);
	if (!util->buf)
		return -ENOMEM;

	// fs
	dir = debugfs_create_dir("cmdq", NULL);
	if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
		cmdq_err("debugfs_create_dir cmdq failed:%d", PTR_ERR(dir));
		return PTR_ERR(dir);
	}

	util->fs_status = debugfs_create_file(
		"cmdq-status", 0444, dir, util, &cmdq_util_status_fops);
	if (IS_ERR(util->fs_status)) {
		cmdq_err("debugfs_create_file cmdq-status failed:%d",
			PTR_ERR(util->fs_status));
		return PTR_ERR(util->fs_status);
	}

	util->fs_record = debugfs_create_file(
		"cmdq-record", 0444, dir, util, &cmdq_util_record_fops);
	if (IS_ERR(util->fs_record)) {
		cmdq_err("debugfs_create_file cmdq-record failed:%d",
			PTR_ERR(util->fs_record));
		return PTR_ERR(util->fs_record);
	}
	return 0;
}
late_initcall(cmdq_util_init);
