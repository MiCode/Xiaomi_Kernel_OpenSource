/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/rwlock.h>
#include <linux/debugfs.h>

#include "debug.h"

#define dbg_inc(i) ((i+1) % DBG_MAX_MSG)

#define ENABLE_EVENT_LOG 1
unsigned int enable_event_log = ENABLE_EVENT_LOG;
module_param(enable_event_log, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(enable_event_log, "enable event logging in debug buffer");

static struct {
	char buf[DBG_MAX_MSG][DBG_MSG_LEN];   /* buffer */
	unsigned idx;   /* index */
	rwlock_t lck;   /* lock */
	struct dentry *root;
} __maybe_unused dbg_buffer = {
	.idx = 0,
	.lck = __RW_LOCK_UNLOCKED(lck),
	.root = NULL
};

void __maybe_unused put_timestamp(char *tbuf)
{
	unsigned long long t;
	unsigned long nanosec_rem;
	unsigned long flags;

	write_lock_irqsave(&dbg_buffer.lck, flags);
	t = cpu_clock(smp_processor_id());
	write_unlock_irqrestore(&dbg_buffer.lck, flags);
	nanosec_rem = do_div(t, 1000000000)/1000;
	snprintf(tbuf, TIME_BUF_LEN, "[%5lu.%06lu]: ", (unsigned long)t,
		nanosec_rem);
}

void __maybe_unused add_event_to_buf(char *tbuf)
{
	unsigned long flags;
	char *buf;
	write_lock_irqsave(&dbg_buffer.lck, flags);
	buf = dbg_buffer.buf[dbg_buffer.idx];
	memcpy(buf, tbuf, DBG_MSG_LEN);
	dbg_buffer.idx = (dbg_buffer.idx + 1) % DBG_MAX_MSG;
	write_unlock_irqrestore(&dbg_buffer.lck, flags);
}

static int dbg_read_buf_show(struct seq_file *s, void *unused)
{
	unsigned long	flags;
	unsigned	i;

	read_lock_irqsave(&dbg_buffer.lck, flags);

	i = dbg_buffer.idx;
	if (strnlen(dbg_buffer.buf[i], DBG_MSG_LEN))
		seq_printf(s, "%s\n", dbg_buffer.buf[i]);
	for (i = dbg_inc(i); i != dbg_buffer.idx;  i = dbg_inc(i)) {
		if (!strnlen(dbg_buffer.buf[i], DBG_MSG_LEN))
			continue;
		seq_printf(s, "%s\n", dbg_buffer.buf[i]);
	}

	read_unlock_irqrestore(&dbg_buffer.lck, flags);

	return 0;
}

static int dbg_read_buf_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_read_buf_show, inode->i_private);
}

const struct file_operations dbg_read_buf_fops = {
	.open			= dbg_read_buf_open,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

int debug_debugfs_init(void)
{
	struct dentry		*root;
	struct dentry		*file;
	int			ret;

	root = debugfs_create_dir("debug", NULL);
	if (!root) {
		ret = -ENOMEM;
		goto err0;
	}

	dbg_buffer.root = root;

	file = debugfs_create_file("read_buf", S_IRUGO, root,
			NULL, &dbg_read_buf_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

	return 0;

err1:
	debugfs_remove_recursive(root);

err0:
	return ret;
}

void debug_debugfs_exit(void)
{
	debugfs_remove_recursive(dbg_buffer.root);
	dbg_buffer.root = NULL;
}
