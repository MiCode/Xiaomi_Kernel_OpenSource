/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>

#include "mdss.h"
#include "mdss_mdp.h"
#include "mdss_debug.h"

#define MDSS_XLOG_ENTRY	256
#define MDSS_XLOG_MAX_DATA 6
#define MDSS_XLOG_BUF_MAX 512
#define MDSS_XLOG_BUF_ALIGN 32

struct tlog {
	u32 tick;
	const char *name;
	int line;
	u32 data[MDSS_XLOG_MAX_DATA];
	u32 data_cnt;
	int pid;
};

struct mdss_dbg_xlog {
	struct tlog logs[MDSS_XLOG_ENTRY];
	u32 first;
	u32 last;
	spinlock_t xlock;
	struct dentry *xlog;
	u32 xlog_enable;
	u32 panic_on_err;
	u32 enable_reg_dump;
} mdss_dbg_xlog;

static inline bool mdss_xlog_is_enabled(u32 flag)
{
	return (flag & mdss_dbg_xlog.xlog_enable) ||
		(flag == MDSS_XLOG_ALL && mdss_dbg_xlog.xlog_enable);
}

void mdss_xlog(const char *name, int line, int flag, ...)
{
	unsigned long flags;
	int i, val = 0;
	va_list args;
	struct tlog *log;
	ktime_t time;

	if (!mdss_xlog_is_enabled(flag))
		return;

	spin_lock_irqsave(&mdss_dbg_xlog.xlock, flags);

	time = ktime_get();

	log = &mdss_dbg_xlog.logs[mdss_dbg_xlog.last % MDSS_XLOG_ENTRY];
	log->tick = local_clock();
	log->name = name;
	log->line = line;
	log->data_cnt = 0;
	log->pid = current->pid;

	va_start(args, flag);
	for (i = 0; i < MDSS_XLOG_MAX_DATA; i++) {

		val = va_arg(args, int);
		if (val == DATA_LIMITER)
			break;

		log->data[i] = val;
	}
	va_end(args);
	log->data_cnt = i;

	mdss_dbg_xlog.last++;

	spin_unlock_irqrestore(&mdss_dbg_xlog.xlock, flags);
}

/* always dump the last entries which are not dumped yet */
static bool __mdss_xlog_dump_calc_range(void)
{
	static u32 next;
	bool need_dump = true;
	unsigned long flags;
	struct mdss_dbg_xlog *xlog = &mdss_dbg_xlog;

	spin_lock_irqsave(&mdss_dbg_xlog.xlock, flags);

	xlog->first = next;

	if (xlog->last == xlog->first) {
		need_dump = false;
		goto dump_exit;
	}

	if (xlog->last < xlog->first) {
		xlog->first %= MDSS_XLOG_ENTRY;
		if (xlog->last < xlog->first)
			xlog->last += MDSS_XLOG_ENTRY;
	}

	if ((xlog->last - xlog->first) > MDSS_XLOG_ENTRY) {
		pr_warn("xlog buffer overflow before dump: %d\n",
			xlog->last - xlog->first);
		xlog->first = xlog->last - MDSS_XLOG_ENTRY;
	}
	need_dump = true;
	next = xlog->first + 1;

dump_exit:
	spin_unlock_irqrestore(&mdss_dbg_xlog.xlock, flags);

	return need_dump;
}

static ssize_t mdss_xlog_dump_entry(char *xlog_buf, ssize_t xlog_buf_size)
{
	int i;
	ssize_t off = 0;
	struct tlog *log, *prev_log;
	unsigned long flags;

	spin_lock_irqsave(&mdss_dbg_xlog.xlock, flags);

	log = &mdss_dbg_xlog.logs[mdss_dbg_xlog.first %
		MDSS_XLOG_ENTRY];

	prev_log = &mdss_dbg_xlog.logs[(mdss_dbg_xlog.first - 1) %
		MDSS_XLOG_ENTRY];

	off = snprintf((xlog_buf + off), (xlog_buf_size - off), "%s:%-4d",
		log->name, log->line);

	if (off < MDSS_XLOG_BUF_ALIGN) {
		memset((xlog_buf + off), 0x20, (MDSS_XLOG_BUF_ALIGN - off));
		off = MDSS_XLOG_BUF_ALIGN;
	}

	off += snprintf((xlog_buf + off), (xlog_buf_size - off),
		"=>[%-8d:%-11u:%9u][%-4d]:", mdss_dbg_xlog.first,
		log->tick / 1000, (log->tick - prev_log->tick) / 1000,
		log->pid);

	for (i = 0; i < log->data_cnt; i++)
		off += snprintf((xlog_buf + off), (xlog_buf_size - off),
			"%x ", log->data[i]);

	off += snprintf((xlog_buf + off), (xlog_buf_size - off), "\n");

	spin_unlock_irqrestore(&mdss_dbg_xlog.xlock, flags);

	return off;
}

static void mdss_xlog_dump_all(void)
{
	char xlog_buf[MDSS_XLOG_BUF_MAX];

	while (__mdss_xlog_dump_calc_range()) {
		mdss_xlog_dump_entry(xlog_buf, MDSS_XLOG_BUF_MAX);
		pr_info("%s", xlog_buf);
	}
}

static void mdss_dump_reg_by_blk(const char *blk_name)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	struct mdss_debug_base *blk_base, *tmp;

	if (!mdd)
		return;

	list_for_each_entry_safe(blk_base, tmp, &mdd->base_list, head) {
		if (blk_base->name &&
			!strcmp(blk_base->name, blk_name))
			mdss_dump_reg(blk_base,
				mdss_dbg_xlog.enable_reg_dump);
	}
}

static void mdss_dump_reg_all(void)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_debug_data *mdd = mdata->debug_inf.debug_data;
	struct mdss_debug_base *blk_base, *tmp;

	if (!mdd)
		return;

	list_for_each_entry_safe(blk_base, tmp, &mdd->base_list, head) {
		if (blk_base->name)
			mdss_dump_reg(blk_base,
				mdss_dbg_xlog.enable_reg_dump);
	}
}

void mdss_xlog_tout_handler_default(const char *name, ...)
{
	int i, dead = 0;
	va_list args;
	char *blk_name = NULL;

	if (!mdss_xlog_is_enabled(MDSS_XLOG_DEFAULT))
		return;

	va_start(args, name);
	for (i = 0; i < MDSS_XLOG_MAX_DATA; i++) {
		blk_name = va_arg(args, char*);
		if (IS_ERR_OR_NULL(blk_name))
			break;

		mdss_dump_reg_by_blk(blk_name);

		if (!strcmp(blk_name, "panic"))
			dead = 1;
	}
	va_end(args);

	mdss_xlog_dump_all();

	if (dead && mdss_dbg_xlog.panic_on_err)
		panic(name);
}
static int mdss_xlog_dump_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mdss_xlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char xlog_buf[MDSS_XLOG_BUF_MAX];

	if (__mdss_xlog_dump_calc_range()) {
		len = mdss_xlog_dump_entry(xlog_buf, MDSS_XLOG_BUF_MAX);
		if (copy_to_user(buff, xlog_buf, len))
			return -EFAULT;
		*ppos += len;
	}

	return len;
}

static ssize_t mdss_xlog_dump_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	mdss_dump_reg_all();

	mdss_xlog_dump_all();

	if (mdss_dbg_xlog.panic_on_err)
		panic("mdss");

	return count;
}


static const struct file_operations mdss_xlog_fops = {
	.open = mdss_xlog_dump_open,
	.read = mdss_xlog_dump_read,
	.write = mdss_xlog_dump_write,
};

int mdss_create_xlog_debug(struct mdss_debug_data *mdd)
{
	spin_lock_init(&mdss_dbg_xlog.xlock);

	mdss_dbg_xlog.xlog = debugfs_create_dir("xlog", mdd->root);
	if (IS_ERR_OR_NULL(mdss_dbg_xlog.xlog)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(mdss_dbg_xlog.xlog));
		mdss_dbg_xlog.xlog = NULL;
		return -ENODEV;
	}
	debugfs_create_file("dump", 0644, mdss_dbg_xlog.xlog, NULL,
						&mdss_xlog_fops);
	debugfs_create_u32("enable", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.xlog_enable);
	debugfs_create_bool("panic", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.panic_on_err);
	debugfs_create_u32("reg_dump", 0644, mdss_dbg_xlog.xlog,
			    &mdss_dbg_xlog.enable_reg_dump);
	return 0;
}

