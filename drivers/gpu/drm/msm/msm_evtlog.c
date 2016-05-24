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

#define pr_fmt(fmt)	"msm_evtlog:[%s] " fmt, __func__

#include "msm_evtlog.h"
#include <linux/slab.h>
#include <linux/sched.h>
#include <asm-generic/current.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#define SIZE_MASK(x) (x - 1)

static int msm_evtlog_debugfs_dump(struct seq_file *s, void *data)
{
	struct msm_evtlog *log = s->private;
	unsigned long cnt;	/* # of samples since clear */
	unsigned long n;	/* # of samples to print, also head index */
	unsigned long i;
	struct timespec timespec;

	/**
	 * Prints in chronological order, oldest -> newest
	 * Note due to lock-less design, the first few printed entries
	 * may be corrupted by new writer not oldest.
	 * This is a tradeoff for speed of sampling
	 */
	cnt = atomic_read(&log->cnt);
	if (!cnt)
		return 0;

	n = cnt & SIZE_MASK(log->size);

	/**
	 * If not full, print from first log
	 * (which is index 1 since atomic_inc_return is prefix operator)
	 */
	i = (cnt < log->size) ? 0 : n;

	seq_puts(s, "time_ns, pid, func, line, val1, val2, msg\n");
	do {
		i = (i + 1) & SIZE_MASK(log->size);
		timespec = ktime_to_timespec(log->events[i].ktime);
		seq_printf(s, "[%5lu.%06lu], %d, %s, %d, %llu, %llu, %s\n",
				timespec.tv_sec,
				timespec.tv_nsec / 1000,
				log->events[i].pid,
				log->events[i].func,
				log->events[i].line,
				log->events[i].val1,
				log->events[i].val2,
				log->events[i].msg);
	} while (i != n);

	return 0;
}

static int msm_evtlog_debugfs_open_dump(struct inode *inode, struct file *file)
{
	return single_open(file, msm_evtlog_debugfs_dump, inode->i_private);
}

static ssize_t msm_evtlog_debugfs_write(
		struct file *file,
		const char __user *user_buf,
		size_t size,
		loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct msm_evtlog *log = s->private;
	char buf[64];
	int buf_size;

	buf_size = min(size, (sizeof(buf) - 1));
	if (strncpy_from_user(buf, user_buf, buf_size) < 0)
		return -EFAULT;
	buf[buf_size] = 0;

	if (strcmp(buf, "0") == 0)
		atomic_set(&log->cnt, 0);

	return size;

}

static const struct file_operations msm_evtlog_fops = {
	.open =		msm_evtlog_debugfs_open_dump,
	.read =		seq_read,
	.write =	msm_evtlog_debugfs_write,
	.llseek =	seq_lseek,
	.release =	single_release,
};

int msm_evtlog_init(
		struct msm_evtlog *log,
		int size,
		struct dentry *parent)
{
	if (!log || size < 1) {
		pr_err("Invalid params\n");
		return -EINVAL;
	}

	memset(log, 0, sizeof(*log));
	log->size = roundup_pow_of_two(size);
	log->events = kcalloc(log->size, sizeof(struct msm_evtlog_evt),
			GFP_KERNEL);

	if (!log->events) {
		pr_err("Insufficient memory\n");
		return -ENOMEM;
	}

	atomic_set(&log->cnt, 0);

	log->dentry = debugfs_create_file("evtlog", S_IRUGO | S_IWUSR, parent,
			log, &msm_evtlog_fops);

	if (IS_ERR_OR_NULL(log->dentry)) {
		int rc = PTR_ERR(log->dentry);

		pr_err("debugfs create file failed, rc=%d\n", rc);
		kfree(log->events);
		return rc;
	}

	return 0;
}

void msm_evtlog_destroy(struct msm_evtlog *log)
{
	debugfs_remove(log->dentry);

	/* Caller needs to make sure that log sampling has stopped */
	kfree(log->events);

}

void msm_evtlog_sample(
		struct msm_evtlog *log,
		const char *func,
		const char *msg,
		uint64_t val1,
		uint64_t val2,
		uint32_t line)
{
	unsigned long i;

	/**
	 * Since array sized with pow of 2, roll to 0 when cnt overflows
	 * mod the value with the size to get current idx into array
	 */
	i = (unsigned long)(atomic_inc_return(&log->cnt)) &
			SIZE_MASK(log->size);
	log->events[i].ktime = ktime_get();
	log->events[i].func = func;
	log->events[i].msg = msg;
	log->events[i].val1 = val1;
	log->events[i].val2 = val2;
	log->events[i].line = line;
	log->events[i].pid = current->pid;
}
