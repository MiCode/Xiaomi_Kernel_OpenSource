// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/debugfs.h>
#include <linux/sched/clock.h>
#include <linux/soc/mediatek/mtk-cmdq.h>

#include "cmdq-util.h"

struct cmdq_util_error {
	spinlock_t	lock;
	bool		enable;
	char		*buffer; // ARG_MAX
	u32		length;
	u64		nsec;
	char		caller[TASK_COMM_LEN]; // TODO
};

struct cmdq_util_dentry {
	struct dentry	*status;
	struct dentry	*record;
	struct dentry	*log_feature;
	u8		bit_feature;
};

struct cmdq_util {
	struct cmdq_util_error	err;
	struct cmdq_util_dentry	fs;
};
static struct cmdq_util	util;

u32 cmdq_util_get_bit_feature(void)
{
	return util.fs.bit_feature;
}

void cmdq_util_error_enable(void)
{
	util.err.nsec = sched_clock();
	util.err.enable = true;
}
EXPORT_SYMBOL(cmdq_util_error_enable);

void cmdq_util_error_disable(void)
{
	util.err.enable = false;
}
EXPORT_SYMBOL(cmdq_util_error_disable);

s32 cmdq_util_error_save(const char *str, ...)
{
	unsigned long	flags;
	va_list		args;
	s32		size;

	if (!util.err.enable)
		return -EFAULT;

	va_start(args, str);
	spin_lock_irqsave(&util.err.lock, flags);
	size = vsnprintf(util.err.buffer + util.err.length,
		ARG_MAX - util.err.length, str, args);
	util.err.length += size;
	spin_unlock_irqrestore(&util.err.lock, flags);

	if (util.err.length >= ARG_MAX) {
		cmdq_util_error_disable();
		cmdq_err("util.err.length:%u is over ARG_MAX:%u",
			util.err.length, ARG_MAX);
	}
	va_end(args);
	return 0;
}
EXPORT_SYMBOL(cmdq_util_error_save);

static int cmdq_util_status_print(struct seq_file *seq, void *data)
{
	u64		sec = util.err.nsec;
	unsigned long	nsec = do_div(sec, 1000000000);

	if (!util.err.length)
		return 0;

	seq_printf(seq, "======== [cmdq] first error [%5llu.%06lu] ========\n",
		sec, nsec);
	seq_printf(seq, "%s", util.err.buffer);
	return 0;
}

static int cmdq_util_record_print(struct seq_file *seq, void *data)
{
	return 0;
}

static int cmdq_util_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_status_print, inode->i_private);
}

static int cmdq_util_record_open(struct inode *inode, struct file *file)
{
	return single_open(file, cmdq_util_record_print, inode->i_private);
}

static const struct file_operations cmdq_util_status_fops = {
	.open = cmdq_util_status_open,
};

static const struct file_operations cmdq_util_record_fops = {
	.open = cmdq_util_record_open,
};

static int cmdq_util_log_feature_get(void *data, u64 *val)
{
	cmdq_msg("data:%p val:%#llx bit_feature:%#x",
		data, *val, util.fs.bit_feature);
	return util.fs.bit_feature;
}

static int cmdq_util_log_feature_set(void *data, u64 val)
{
	if (val == ~0) {
		util.fs.bit_feature = 0;
		cmdq_msg("data:%p val:%#llx bit_feature:%#x reset",
			data, val, util.fs.bit_feature);
		return 0;
	}

	if (val >= CMDQ_LOG_FEAT_NUM) {
		cmdq_err("data:%p val:%#llx cannot be over %#x",
			data, val, CMDQ_LOG_FEAT_NUM);
		return -EINVAL;
	}

	util.fs.bit_feature |= (1 << val);
	cmdq_msg("data:%p val:%#llx bit_feature:%#x",
		data, val, util.fs.bit_feature);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(cmdq_util_log_feature_fops,
	cmdq_util_log_feature_get, cmdq_util_log_feature_set, "%llu");

static int __init cmdq_util_init(void)
{
	struct dentry	*dir;

	spin_lock_init(&util.err.lock);
	util.err.buffer = kzalloc(ARG_MAX, GFP_KERNEL);
	if (!util.err.buffer)
		return -ENOMEM;

	dir = debugfs_create_dir("cmdq", NULL);
	if (IS_ERR(dir) && PTR_ERR(dir) != -EEXIST) {
		cmdq_err("debugfs_create_dir cmdq failed:%ld", PTR_ERR(dir));
		return PTR_ERR(dir);
	}

	util.fs.status = debugfs_create_file(
		"cmdq-status", 0444, dir, &util, &cmdq_util_status_fops);
	if (IS_ERR(util.fs.status)) {
		cmdq_err("debugfs_create_file cmdq-status failed:%ld",
			PTR_ERR(util.fs.status));
		return PTR_ERR(util.fs.status);
	}

	util.fs.record = debugfs_create_file(
		"cmdq-record", 0444, dir, &util, &cmdq_util_record_fops);
	if (IS_ERR(util.fs.record)) {
		cmdq_err("debugfs_create_file cmdq-record failed:%ld",
			PTR_ERR(util.fs.record));
		return PTR_ERR(util.fs.record);
	}

	util.fs.log_feature = debugfs_create_file("cmdq-log-feature",
		0444, dir, &util, &cmdq_util_log_feature_fops);
	if (IS_ERR(util.fs.log_feature)) {
		cmdq_err("debugfs_create_file cmdq-log-feature failed:%ld",
			PTR_ERR(util.fs.log_feature));
		return PTR_ERR(util.fs.log_feature);
	}
	return 0;
}
late_initcall(cmdq_util_init);
