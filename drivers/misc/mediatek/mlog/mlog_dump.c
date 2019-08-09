/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
/* #include <linux/errno.h> */
/* #include <linux/time.h> */
/* #include <linux/kernel.h> */
/* #include <linux/module.h> */
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>

#include "mlog_internal.h"
#include "mlog_dump.h"

static int mlog_open(struct inode *inode, struct file *file)
{
	MLOG_PRINTK("[mlog] open %d\n", mlog_unread());
	mlog_doopen();
	return 0;
}

static int mlog_release(struct inode *inode, struct file *file)
{
	MLOG_PRINTK("[mlog] release\n");
	return 0;
}

static ssize_t mlog_read(struct file *file, char __user *buf,
		size_t count, loff_t *ppos)
{
	if (file->f_flags & O_NONBLOCK) {
		if (!mlog_unread())
			return -EAGAIN;
		/* MLOG_PRINTK("[mlog] read (NonBlock) %d\n", count); */
	}
	return mlog_doread(buf, count);
}

static unsigned int mlog_poll(struct file *file, poll_table *wait)
{
	/* MLOG_PRINTK("[mlog] poll\n"); */
	poll_wait(file, &mlog_wait, wait);
	if (mlog_unread())
		return POLLIN | POLLRDNORM;
	return 0;
}

static const struct file_operations proc_mlog_operations = {
	.read = mlog_read,
	.poll = mlog_poll,
	.open = mlog_open,
	.release = mlog_release,
	.llseek = generic_file_llseek,
};

static int mlog_fmt_proc_show(struct seq_file *m, void *v)
{
	return mlog_print_fmt(m);
}

static int mlog_fmt_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, mlog_fmt_proc_show, NULL);
}

static const struct file_operations mlog_fmt_proc_fops = {
	.open = mlog_fmt_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations proc_dmlog_operations = {
	.open = dmlog_open,
	.read = dmlog_read,
	.release = dmlog_release,
};

void mlog_init_procfs(void)
{
	debugfs_create_file("mlog_fmt", 0444, NULL, NULL,
			&mlog_fmt_proc_fops);
	debugfs_create_file("mlog", 0444, NULL, NULL,
			&proc_mlog_operations);
	debugfs_create_file("dmlog", 0444, NULL, NULL,
			&proc_dmlog_operations);
}
