/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"sde_evtlog:[%s] " fmt, __func__

#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/ktime.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/dma-buf.h>

#include "sde_dbg.h"
#include "sde_trace.h"

#ifdef CONFIG_DRM_SDE_EVTLOG_DEBUG
#define SDE_EVTLOG_DEFAULT_ENABLE 1
#else
#define SDE_EVTLOG_DEFAULT_ENABLE 0
#endif

#define SDE_DBG_DEFAULT_PANIC		1

/*
 * evtlog will print this number of entries when it is called through
 * sysfs node or panic. This prevents kernel log from evtlog message
 * flood.
 */
#define SDE_EVTLOG_PRINT_ENTRY	256

/*
 * evtlog keeps this number of entries in memory for debug purpose. This
 * number must be greater than print entry to prevent out of bound evtlog
 * entry array access.
 */
#define SDE_EVTLOG_ENTRY	(SDE_EVTLOG_PRINT_ENTRY * 4)
#define SDE_EVTLOG_MAX_DATA 15
#define SDE_EVTLOG_BUF_MAX 512
#define SDE_EVTLOG_BUF_ALIGN 32

DEFINE_SPINLOCK(sde_evtloglock);

struct tlog {
	u32 counter;
	s64 time;
	const char *name;
	int line;
	u32 data[SDE_EVTLOG_MAX_DATA];
	u32 data_cnt;
	int pid;
};

static struct sde_dbg_evtlog {
	struct tlog logs[SDE_EVTLOG_ENTRY];
	u32 first;
	u32 last;
	u32 curr;
	struct dentry *evtlog;
	u32 evtlog_enable;
	u32 panic_on_err;
	struct work_struct evtlog_dump_work;
	bool work_panic;
} sde_dbg_evtlog;

static inline bool sde_evtlog_is_enabled(u32 flag)
{
	return (flag & sde_dbg_evtlog.evtlog_enable) ||
		(flag == SDE_EVTLOG_ALL && sde_dbg_evtlog.evtlog_enable);
}

void sde_evtlog(const char *name, int line, int flag, ...)
{
	unsigned long flags;
	int i, val = 0;
	va_list args;
	struct tlog *log;

	if (!sde_evtlog_is_enabled(flag))
		return;

	spin_lock_irqsave(&sde_evtloglock, flags);
	log = &sde_dbg_evtlog.logs[sde_dbg_evtlog.curr];
	log->time = ktime_to_us(ktime_get());
	log->name = name;
	log->line = line;
	log->data_cnt = 0;
	log->pid = current->pid;

	va_start(args, flag);
	for (i = 0; i < SDE_EVTLOG_MAX_DATA; i++) {

		val = va_arg(args, int);
		if (val == SDE_EVTLOG_DATA_LIMITER)
			break;

		log->data[i] = val;
	}
	va_end(args);
	log->data_cnt = i;
	sde_dbg_evtlog.curr = (sde_dbg_evtlog.curr + 1) % SDE_EVTLOG_ENTRY;
	sde_dbg_evtlog.last++;

	trace_sde_evtlog(name, line, i > 0 ? log->data[0] : 0,
			i > 1 ? log->data[1] : 0);

	spin_unlock_irqrestore(&sde_evtloglock, flags);
}

/* always dump the last entries which are not dumped yet */
static bool _sde_evtlog_dump_calc_range(void)
{
	static u32 next;
	bool need_dump = true;
	unsigned long flags;
	struct sde_dbg_evtlog *evtlog = &sde_dbg_evtlog;

	spin_lock_irqsave(&sde_evtloglock, flags);

	evtlog->first = next;

	if (evtlog->last == evtlog->first) {
		need_dump = false;
		goto dump_exit;
	}

	if (evtlog->last < evtlog->first) {
		evtlog->first %= SDE_EVTLOG_ENTRY;
		if (evtlog->last < evtlog->first)
			evtlog->last += SDE_EVTLOG_ENTRY;
	}

	if ((evtlog->last - evtlog->first) > SDE_EVTLOG_PRINT_ENTRY) {
		pr_warn("evtlog buffer overflow before dump: %d\n",
			evtlog->last - evtlog->first);
		evtlog->first = evtlog->last - SDE_EVTLOG_PRINT_ENTRY;
	}
	next = evtlog->first + 1;

dump_exit:
	spin_unlock_irqrestore(&sde_evtloglock, flags);

	return need_dump;
}

static ssize_t sde_evtlog_dump_entry(char *evtlog_buf, ssize_t evtlog_buf_size)
{
	int i;
	ssize_t off = 0;
	struct tlog *log, *prev_log;
	unsigned long flags;

	spin_lock_irqsave(&sde_evtloglock, flags);

	log = &sde_dbg_evtlog.logs[sde_dbg_evtlog.first %
		SDE_EVTLOG_ENTRY];

	prev_log = &sde_dbg_evtlog.logs[(sde_dbg_evtlog.first - 1) %
		SDE_EVTLOG_ENTRY];

	off = snprintf((evtlog_buf + off), (evtlog_buf_size - off), "%s:%-4d",
		log->name, log->line);

	if (off < SDE_EVTLOG_BUF_ALIGN) {
		memset((evtlog_buf + off), 0x20, (SDE_EVTLOG_BUF_ALIGN - off));
		off = SDE_EVTLOG_BUF_ALIGN;
	}

	off += snprintf((evtlog_buf + off), (evtlog_buf_size - off),
		"=>[%-8d:%-11llu:%9llu][%-4d]:", sde_dbg_evtlog.first,
		log->time, (log->time - prev_log->time), log->pid);

	for (i = 0; i < log->data_cnt; i++)
		off += snprintf((evtlog_buf + off), (evtlog_buf_size - off),
			"%x ", log->data[i]);

	off += snprintf((evtlog_buf + off), (evtlog_buf_size - off), "\n");

	spin_unlock_irqrestore(&sde_evtloglock, flags);

	return off;
}

static void _sde_evtlog_dump_all(void)
{
	char evtlog_buf[SDE_EVTLOG_BUF_MAX];

	while (_sde_evtlog_dump_calc_range()) {
		sde_evtlog_dump_entry(evtlog_buf, SDE_EVTLOG_BUF_MAX);
		pr_info("%s", evtlog_buf);
	}
}

static void _sde_dump_array(bool dead, const char *name)
{
	_sde_evtlog_dump_all();

	if (dead && sde_dbg_evtlog.panic_on_err)
		panic(name);
}

static void _sde_dump_work(struct work_struct *work)
{
	_sde_dump_array(sde_dbg_evtlog.work_panic, "evtlog_workitem");
}

void sde_dbg_dump(bool queue, const char *name, ...)
{
	int i;
	bool dead = false;
	va_list args;
	char *blk_name = NULL;

	if (!sde_evtlog_is_enabled(SDE_EVTLOG_DEFAULT))
		return;

	if (queue && work_pending(&sde_dbg_evtlog.evtlog_dump_work))
		return;

	va_start(args, name);
	for (i = 0; i < SDE_EVTLOG_MAX_DATA; i++) {
		blk_name = va_arg(args, char*);
		if (IS_ERR_OR_NULL(blk_name))
			break;

		if (!strcmp(blk_name, "panic"))
			dead = true;
	}
	va_end(args);

	if (queue) {
		/* schedule work to dump later */
		sde_dbg_evtlog.work_panic = dead;
		schedule_work(&sde_dbg_evtlog.evtlog_dump_work);
	} else {
		_sde_dump_array(dead, name);
	}
}

static int sde_evtlog_dump_open(struct inode *inode, struct file *file)
{
	/* non-seekable */
	file->f_mode &= ~(FMODE_LSEEK | FMODE_PREAD | FMODE_PWRITE);
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t sde_evtlog_dump_read(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	ssize_t len = 0;
	char evtlog_buf[SDE_EVTLOG_BUF_MAX];

	if (_sde_evtlog_dump_calc_range()) {
		len = sde_evtlog_dump_entry(evtlog_buf, SDE_EVTLOG_BUF_MAX);
		if (copy_to_user(buff, evtlog_buf, len))
			return -EFAULT;
		*ppos += len;
	}

	return len;
}

static ssize_t sde_evtlog_dump_write(struct file *file,
	const char __user *user_buf, size_t count, loff_t *ppos)
{
	_sde_evtlog_dump_all();

	if (sde_dbg_evtlog.panic_on_err)
		panic("sde");

	return count;
}

static const struct file_operations sde_evtlog_fops = {
	.open = sde_evtlog_dump_open,
	.read = sde_evtlog_dump_read,
	.write = sde_evtlog_dump_write,
};

int sde_evtlog_init(struct dentry *debugfs_root)
{
	int i;

	sde_dbg_evtlog.evtlog = debugfs_create_dir("evt_dbg", debugfs_root);
	if (IS_ERR_OR_NULL(sde_dbg_evtlog.evtlog)) {
		pr_err("debugfs_create_dir fail, error %ld\n",
		       PTR_ERR(sde_dbg_evtlog.evtlog));
		sde_dbg_evtlog.evtlog = NULL;
		return -ENODEV;
	}

	INIT_WORK(&sde_dbg_evtlog.evtlog_dump_work, _sde_dump_work);
	sde_dbg_evtlog.work_panic = false;

	for (i = 0; i < SDE_EVTLOG_ENTRY; i++)
		sde_dbg_evtlog.logs[i].counter = i;

	debugfs_create_file("dump", 0644, sde_dbg_evtlog.evtlog, NULL,
						&sde_evtlog_fops);
	debugfs_create_u32("enable", 0644, sde_dbg_evtlog.evtlog,
			    &sde_dbg_evtlog.evtlog_enable);
	debugfs_create_u32("panic", 0644, sde_dbg_evtlog.evtlog,
			    &sde_dbg_evtlog.panic_on_err);

	sde_dbg_evtlog.evtlog_enable = SDE_EVTLOG_DEFAULT_ENABLE;
	sde_dbg_evtlog.panic_on_err = SDE_DBG_DEFAULT_PANIC;

	pr_info("evtlog_status: enable:%d, panic:%d\n",
		sde_dbg_evtlog.evtlog_enable, sde_dbg_evtlog.panic_on_err);

	return 0;
}

void sde_evtlog_destroy(void)
{
	debugfs_remove(sde_dbg_evtlog.evtlog);
}
