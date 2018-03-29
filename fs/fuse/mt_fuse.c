/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#define DEBUG 1

#include "fuse_i.h"
#include "mt_fuse.h"
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/time.h>

/* kerel log buffer and ring buffer for debugfs */
struct fuse_proc_info *fuse_iolog = NULL;
struct fuse_proc_log *fuse_ringbuf = NULL;
int fuse_ringbuf_index = 0;

/* kernel print buffer */
static char *fuse_iolog_buf;

/* mutex used to protect the buffers */
struct mutex fuse_iolog_lock;

/* kernel log thread */
struct task_struct *fuse_iolog_thread = NULL;

/* debugfs dentries */
struct dentry *fuseio_droot = NULL;
struct dentry *fuseio_dklog = NULL;
struct dentry *fuseio_dlog = NULL;

/* kernel log enable */
unsigned int fuseio_klog_enable = 0;

void fuse_time_diff(struct timespec *start, struct timespec *end, struct timespec *diff)
{
	if ((end->tv_nsec-start->tv_nsec) < 0) {
		diff->tv_sec = end->tv_sec-start->tv_sec-1;
		diff->tv_nsec = 1000000000+end->tv_nsec-start->tv_nsec;
	} else {
		diff->tv_sec = end->tv_sec-start->tv_sec;
		diff->tv_nsec = end->tv_nsec-start->tv_nsec;
	}
}

/* type code translation table */
struct fuse_ioiog_type_map {
	int type;
	const char *str;
};

#define FUSE_IOLOG_TYPE_MAX 46

static const char *fuse_iolog_type[FUSE_IOLOG_TYPE_MAX] = {
	"unknown",	/*0*/
	"lookup",	/*1*/
	"forget",	/*2*/
	"getattr",	/*3*/
	"setattr",	/*4*/
	"readlink",	/*5*/
	"symlink",	/*6*/
	"",	/*7*/
	"mknod",	/*8*/
	"mkdir",	/*9*/
	"unlink",	/*10*/
	"rmdir",	/*11*/
	"rename",	/*12*/
	"link",	/*13*/
	"open",	/*14*/
	"read",	/*15*/
	"write",	/*16*/
	"statfs",	/*17*/
	"release",	/*18*/
	"",	/*19*/
	"fsync",	/*20*/
	"setxattr",	/*21*/
	"getxattr",	/*22*/
	"listxattr",	/*23*/
	"removexattr",	/*24*/
	"flush",	/*25*/
	"init",	/*26*/
	"opendir",	/*27*/
	"readdir",	/*28*/
	"releasedir",	/*29*/
	"fsyncdir",	/*30*/
	"getlk",	/*31*/
	"setlk",	/*32*/
	"setlkw",	/*33*/
	"access",	/*34*/
	"create",	/*35*/
	"interrupt",	/*36*/
	"bmap",	/*37*/
	"destroy",	/*38*/
	"ioctl",	/*39*/
	"poll",	/*40*/
	"notify_reply",	/*41*/
	"batch_forget",	/*42*/
	"fallocate", /*43*/
	"readdirplus", /*44*/
	"cuse"
};

/* convert type code to string */
static const char *fuse_iolog_type2str(int type)
{
	if (type >= 0 && type < FUSE_IOLOG_TYPE_MAX)
		goto out;
	else if (type == CUSE_INIT)
		type = FUSE_IOLOG_TYPE_MAX-1;
	else
		type = 0;
out:
	return fuse_iolog_type[type];
}



int fuse_iolog_print(void)
{
	int i, len, n;
	char *ptr;

	len = FUSE_IOLOG_BUFLEN-1;
	ptr = &fuse_iolog_buf[0];

	for (i = 0; i < FUSE_IOLOG_MAX && fuse_iolog[i].valid; i++) {

		if (fuse_iolog[i].read.count || fuse_iolog[i].write.count) {
			n = snprintf(ptr, len, "{%d:R(%d,%d,%d),W(%d,%d,%d)}",
				fuse_iolog[i].pid,
				fuse_iolog[i].read.bytes,
				fuse_iolog[i].read.count,
				fuse_iolog[i].read.us,
				fuse_iolog[i].write.bytes,
				fuse_iolog[i].write.count,
				fuse_iolog[i].write.us);

			len -= n;
			ptr += n;

			if (len < 0)
				goto overflow;
		}

		if (fuse_iolog[i].misc_type) {
			n = snprintf(ptr, len, "{%d:%s(%d,%d,%d)}",
				fuse_iolog[i].pid,
				fuse_iolog_type2str(fuse_iolog[i].misc_type),
				fuse_iolog[i].misc.bytes,
				fuse_iolog[i].misc.count,
				fuse_iolog[i].misc.us);
			len -= n;
			ptr += n;

			if (len < 0)
				goto overflow;
		}
	}

	if (i > 0) {
		if (fuseio_klog_enable)
			pr_debug("[BLOCK_TAG] FUSEIO %s\n", &fuse_iolog_buf[0]);

		memcpy(fuse_ringbuf[fuse_ringbuf_index].info, fuse_iolog, FUSE_IOLOG_MAX*sizeof(struct fuse_proc_info));
		fuse_ringbuf[fuse_ringbuf_index].time = sched_clock();
		fuse_ringbuf_index++;
		if (fuse_ringbuf_index >= FUSE_IOLOG_RINGBUF_MAX)
			fuse_ringbuf_index = 0;
	}

	return ptr - &fuse_iolog_buf[0];

overflow:
	pr_debug("[BLOCK_TAG] FUSEIO log buffer overflow\n");

	return -1;
}

static void fuse_iolog_seq_entry_time(struct seq_file *seq, uint64_t time)
{
	uint32_t nsec;

	if (time > 0) {
		nsec = do_div(time, 1000000000);
		seq_printf(seq, "[%5lu.%06lu] FUSEIO ", (unsigned long)time, (unsigned long)nsec/1000);
	}
}

static int fuse_iolog_seq_entry(struct seq_file *seq, int index)
{
	int i;

	if (index < 0 || index >= FUSE_IOLOG_RINGBUF_MAX)
		pr_warn("fuseio: fuse_iolog_seq_entry, index %d out of range (0-%d).\n",
			index, FUSE_IOLOG_RINGBUF_MAX-1);

	fuse_iolog_seq_entry_time(seq, fuse_ringbuf[index].time);

	for (i = 0; i < FUSE_IOLOG_MAX && fuse_ringbuf[index].info[i].valid; i++) {
		struct fuse_proc_info *info =  &fuse_ringbuf[index].info[i];

		if (info->read.count || info->write.count) {
			seq_printf(seq, "{%d:R(%d,%d,%d),W(%d,%d,%d)}",
				info->pid,
				info->read.bytes,
				info->read.count,
				info->read.us,
				info->write.bytes,
				info->write.count,
				info->write.us);
		}

		if (info->misc_type) {
			seq_printf(seq, "{%d:%s(%d,%d,%d)}",
				info->pid,
				fuse_iolog_type2str(info->misc_type),
				info->misc.bytes,
				info->misc.count,
				info->misc.us);
		}
	}
	if (i > 0)
		seq_puts(seq, "\n");

	return i;
}

static void fuse_iolog_clear(void)
{
	memset(&fuse_iolog[0], 0, sizeof(struct fuse_proc_info)*FUSE_IOLOG_MAX);
}

static void fuse_ringbuf_clear(void)
{
	memset(&fuse_ringbuf[0], 0, sizeof(struct fuse_proc_log)*FUSE_IOLOG_RINGBUF_MAX);
}

inline __u32 fuse_iolog_timeus(struct timespec *t)
{
	__u32 _t;
	long us;

	us = t->tv_nsec;
	do_div(us, 1000);

	if (t->tv_sec > 3600)
		return 0xD693A400; /* 3600000000 */

	_t = t->tv_sec * 1000000 + us;

	if (_t)
		return _t;

	return 1;
}

__u32 fuse_iolog_timeus_diff(struct timespec *start, struct timespec *end)
{
	struct timespec diff;

	fuse_time_diff(start, end, &diff);
	return fuse_iolog_timeus(&diff);
}


inline int fuse_iolog_proc_update(struct fuse_proc_info *info,
	__u32 io_bytes, int type, struct timespec *diff)
{
	struct fuse_rw_info *rwi;
	__u32 _t;

	_t = fuse_iolog_timeus(diff);

	if (type == FUSE_READ)
		rwi = &info->read;
	else if (type == FUSE_WRITE)
		rwi = &info->write;
	else {
		if (info->misc_type == 0)
			info->misc_type = type;
		else if (info->misc_type != type) /* misc type mismatch => continue */
			return -1;
		rwi = &info->misc;
	}

	rwi->bytes += io_bytes;
	rwi->us += _t;
	rwi->count++;

	return 0;
}

static int fuse_iolog_watch(void *arg)
{
	unsigned int timeout;
	int n;
	int empty = 0;  /* how many seconds that log is empty */

	while (1) {
		if (kthread_should_stop())
			break;

		/* log is empty for last 1 seconds => sleep till next io coming */
		if (empty > 1) {
				set_current_state(TASK_INTERRUPTIBLE);
				schedule();
		} else { /* otherwise, check 1 seconds later */
			do {
				set_current_state(TASK_INTERRUPTIBLE);
				timeout = schedule_timeout(FUSE_IOLOG_LATENCY*HZ);
			 } while (timeout);
		}

		mutex_lock(&fuse_iolog_lock);

		n = fuse_iolog_print();

		if (n > 0) {
			fuse_iolog_clear();
			empty = 0;
		} else {
			empty++;
		}

		mutex_unlock(&fuse_iolog_lock);
	}

	return 0;
}


static ssize_t fuseio_debug_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	return count;
}

/* seq file operations */
static void *fuseio_seq_debug_start(struct seq_file *seq, loff_t *pos)
{
	unsigned int idx;

	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

static void *fuseio_seq_debug_next(struct seq_file *seq, void *v, loff_t *pos)
{
	unsigned int idx;

	++*pos;
	if (*pos < 0 || *pos >= 1)
		return NULL;

	idx = *pos + 1;
	return (void *) ((unsigned long) idx);
}

static int fuseio_seq_debug_show(struct seq_file *seq, void *v)
{
	int i, end;

	mutex_lock(&fuse_iolog_lock);
	end = (fuse_ringbuf_index > 0) ? fuse_ringbuf_index-1 : FUSE_IOLOG_RINGBUF_MAX-1;
	for (i = fuse_ringbuf_index; ;) {
		fuse_iolog_seq_entry(seq, i);
		if (i == end)
			break;
		i = (i == FUSE_IOLOG_RINGBUF_MAX-1) ? 0 : i+1;
	};

	mutex_unlock(&fuse_iolog_lock);

	return 0;
}

static void fuseio_seq_debug_stop(struct seq_file *seq, void *v)
{
}

static const struct seq_operations fuseio_seq_debug_ops = {
	.start  = fuseio_seq_debug_start,
	.next   = fuseio_seq_debug_next,
	.stop   = fuseio_seq_debug_stop,
	.show   = fuseio_seq_debug_show,
};

static int fuseio_seq_debug_open(struct inode *inode, struct file *file)
{
	int rc;

	rc = seq_open(file, &fuseio_seq_debug_ops);
	if (rc == 0) {
		struct seq_file *m = file->private_data;

		m->private = &fuse_ringbuf_index;
	}
	return rc;

}

static const struct file_operations fuseio_seq_debug_fops = {
	.owner		= THIS_MODULE,
	.open		= fuseio_seq_debug_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
	.write		= fuseio_debug_write,
};

void fuse_iolog_init(void)
{
	int ret;

	fuse_iolog_buf = kmalloc(FUSE_IOLOG_BUFLEN, GFP_NOFS);
	fuse_iolog = kmalloc_array(FUSE_IOLOG_MAX, sizeof(struct fuse_proc_info), GFP_NOFS);
	fuse_ringbuf = kmalloc_array(FUSE_IOLOG_RINGBUF_MAX, sizeof(struct fuse_proc_log), GFP_NOFS);

	if (!fuse_iolog_buf || !fuse_iolog || !fuse_ringbuf)
		goto error_out;

	mutex_init(&fuse_iolog_lock);
	mutex_lock(&fuse_iolog_lock);
	fuse_iolog_clear();
	fuse_ringbuf_clear();
	mutex_unlock(&fuse_iolog_lock);

	fuse_iolog_thread = kthread_create(fuse_iolog_watch, NULL, "fuse_log");
	if (IS_ERR(fuse_iolog_thread)) {
		ret = PTR_ERR(fuse_iolog_thread);
		pr_warn("[BLOCK_TAG] fuse: fail to create fuse_log thread %d\n", ret);
		fuse_iolog_thread = NULL;
		goto error_out;
	}

	fuseio_droot = debugfs_create_dir("fuseio", NULL);

	if (IS_ERR(fuseio_droot)) {
		pr_warn("[BLOCK_TAG] fuse: fail to create debugfs root\n");
		goto out;
	}

	fuseio_dklog = debugfs_create_u32("klog_enable", 0660, fuseio_droot, &fuseio_klog_enable);

	if (IS_ERR(fuseio_dklog)) {
		pr_warn("[BLOCK_TAG] fuse: fail to create klog_enable at debugfs\n");
		goto out;
	}

	fuseio_dlog = debugfs_create_file("log", S_IFREG | S_IRUGO, fuseio_droot, (void *)0, &fuseio_seq_debug_fops);

	if (IS_ERR(fuseio_dlog)) {
		pr_warn("[BLOCK_TAG] fuse: fail to create log at debugfs\n");
		goto out;
	}

out:
	return;

error_out:
	kfree(fuse_iolog_buf);
	kfree(fuse_iolog);
	kfree(fuse_ringbuf);
}

void fuse_iolog_exit(void)
{
	kthread_stop(fuse_iolog_thread);
	kfree(fuse_iolog_buf);
	kfree(fuse_iolog);
	kfree(fuse_ringbuf);
}

void fuse_iolog_add(__u32 io_bytes, int type,
	struct timespec *start,
	struct timespec *end)
{
	struct fuse_proc_info *info;
	struct timespec diff;
	pid_t pid;
	int i;

	pid = task_pid_nr(current);
	fuse_time_diff(start, end, &diff);

	mutex_lock(&fuse_iolog_lock);

	for (i = 0; i < FUSE_IOLOG_MAX; i++)   {
		info = &fuse_iolog[i];
		if (info->valid) {
			if (info->pid == pid) {
				if (fuse_iolog_proc_update(info, io_bytes, type, &diff))
					continue; /* ops mismatch */
				else
					goto out;
			} else {
				continue;
			}
		} else {
			info->valid = 1;
			info->pid = pid;
			fuse_iolog_proc_update(info, io_bytes, type, &diff);
			if (i == 0) {  /* this is the first entry, wake up the handler */
				if (fuse_iolog_thread)
					wake_up_process(fuse_iolog_thread);
			}
			goto out;
		}
	}

	if (i == FUSE_IOLOG_MAX) {
		fuse_iolog_print();
		fuse_iolog_clear();
		info = &fuse_iolog[0];
		info->valid = 1;
		info->pid = pid;
		fuse_iolog_proc_update(info, io_bytes, type, &diff);
	}
out:
	mutex_unlock(&fuse_iolog_lock);
}

