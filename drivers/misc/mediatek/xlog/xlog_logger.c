/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <asm/ioctls.h>
#include <linux/proc_fs.h>
#include <linux/xlog.h>
#include <linux/aio.h>
#include "drivers/staging/android/logger.h"
#include "xlog_internal.h"

/*
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char *buffer;	/* the ring buffer itself */
	struct miscdevice misc;	/* misc device representing the log */
	wait_queue_head_t wq;	/* wait queue for readers */
	struct list_head readers;	/* this log's readers */
	spinlock_t lock;	/* spinlock protecting buffer */
	size_t w_off;		/* current write head offset */
	size_t head;		/* new readers start here */
	size_t size;		/* size of the log */
	/* kirby 20100427: for log missing warning { */
	long sign;
	/* } kirby 20100427 */
};

/*
 * struct logger_reader - a logging device open for reading
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->lock.
 */
struct logger_reader {
	struct logger_log *log;	/* associated log */
	struct list_head list;	/* entry in logger_log's list */
	size_t r_off;		/* current read head offset */
	/* kirby 20100427: for log missing warning { */
	long sign;
	/* } kirby 20100427 */
};

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
#define logger_offset(n)	((n) & (log->size - 1))

/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 *	1) Need to quickly obtain the associated log during an I/O operation
 *	2) Readers need to maintain state (logger_reader)
 *	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_len - Grabs the length of the payload of the next entry starting
 * from 'off'.
 *
 * Caller needs to hold log->lock.
 */
static __u32 get_entry_len(struct logger_log *log, size_t off)
{
	__u16 val;

	switch (log->size - off) {
	case 1:
		memcpy(&val, log->buffer + off, 1);
		memcpy(((char *)&val) + 1, log->buffer, 1);
		break;
	default:
		memcpy(&val, log->buffer + off, 2);
	}

	return sizeof(struct logger_entry) + val;
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->lock.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader, char __user *buf, size_t count)
{
	size_t len;

	/*
	 * We read from the log in two disjoint operations. First, we read from
	 * the current read head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - reader->r_off);
	if (copy_to_user(buf, log->buffer + reader->r_off, len))
		return -EFAULT;

	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;

	reader->r_off = logger_offset(reader->r_off + count);

	/* kirby 20100427: for log missing warning { */
	reader->sign += count;
	/* } kirby 20100427 */
	return count;
}

/* kirby 20100427: for log missing warning { */
static int is_overflow(long size, long cw, long lw, size_t r_off)
{
	return (cw - lw >= size) ? 1 : 0;
	/* cw = cw%size, lw = lw%size; */
	/* return ((cw<lw && (cw>=r_off || lw<=r_off)) || (lw<r_off && cw>r_off))?1:0; */

}

static ssize_t warn_overflow(struct logger_log *log, struct logger_reader *reader,
			     char __user *buf)
{
	int i;
	char message[960];	/* to make sure stack frame not larger than 1024 bytes. */

	char head[] = {
		0x25, 0x00,
		0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x5, 'A', 'E', 'E', 0x0
	};
	sprintf(&(message[25]), "some logs might have been lost (%ld bytes estimated)",
		log->sign - reader->sign);

	head[0] = strlen(&(message[25])) + 6;
	for (i = 0; i < 25; i++)
		message[i] = head[i];
	if (copy_to_user(buf, message, strlen(&(message[25])) + 26))
		return -EFAULT;
	reader->sign = log->sign - (log->sign % log->size) + reader->r_off;
	if (reader->sign > log->sign)
		reader->sign -= log->size;
	return strlen(&(message[25])) + 26;
}

/* } kirby 20100427 */
/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 *	- O_NONBLOCK works
 *	- If there are no log entries to read, blocks until log is written to
 *	- Atomically reads exactly one log entry
 *
 * Optimal read size is LOGGER_ENTRY_MAX_LEN. Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	unsigned long flags;

	DEFINE_WAIT(wait);

 start:
	while (1) {
		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		spin_lock_irqsave(&log->lock, flags);
		ret = (log->w_off == reader->r_off);
		spin_unlock_irqrestore(&log->lock, flags);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
		return ret;

	spin_lock_irqsave(&log->lock, flags);

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		spin_unlock_irqrestore(&log->lock, flags);
		goto start;
	}
	/* kirby 20100427: for log missing warning { */
	if (is_overflow(log->size, log->sign, reader->sign, reader->r_off)) {
		printk("[AEE] Some logs might have been lost!\n");
		ret = warn_overflow(log, reader, buf);
		goto out;
	}
	/* } kirby 20100427 */

	/* get the size of the next entry */
	ret = get_entry_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, ret);

 out:
	spin_unlock_irqrestore(&log->lock, flags);

	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->lock.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = get_entry_len(log, off);
		off = logger_offset(off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * clock_interval - is a < c < b in mod-space? Put another way, does the line
 * from a to b cross c?
 */
static inline int clock_interval(size_t a, size_t b, size_t c)
{
	if (b < a) {
		if (a < c || b >= c)
			return 1;
	} else {
		if (a < c && b >= c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->lock.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(old + len);
	struct logger_reader *reader;

	if (clock_interval(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
	    if (clock_interval(old, new, reader->r_off))
		reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->lock.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log->w_off + count);

	/* kirby 20100427: for log missing warning { */
	log->sign += count;
	/* } kirby 20100427 */
}

/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->lock.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log, const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			return -EFAULT;

	log->w_off = logger_offset(log->w_off + count);

	/* kirby 20100427: for log missing warning { */
	log->sign += count;
	/* } kirby 20100427 */

	return count;
}

/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
static ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
				unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
	size_t orig = log->w_off;
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;
	unsigned long flags;

	getnstimeofday(&now);
	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, iocb->ki_left, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	spin_lock_irqsave(&log->lock, flags);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			spin_unlock_irqrestore(&log->lock, flags);
			return nr;
		}

		iov++;
		ret += nr;
	}

	spin_unlock_irqrestore(&log->lock, flags);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ret;
}

static struct logger_log *get_log_from_minor(int);

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;
	unsigned long flags;

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		INIT_LIST_HEAD(&reader->list);

		spin_lock_irqsave(&log->lock, flags);
		reader->r_off = log->head;

		/* kirby 20100427: for log missing warning { */
		reader->sign = log->sign - (log->sign % log->size) + log->head;
		if (reader->sign > log->sign)
			reader->sign -= log->size;
		/* } kirby 20100427 */

		list_add_tail(&reader->list, &log->readers);
		spin_unlock_irqrestore(&log->lock, flags);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		list_del(&reader->list);
		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;
	unsigned long flags;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	spin_lock_irqsave(&log->lock, flags);
	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	spin_unlock_irqrestore(&log->lock, flags);

	return ret;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -ENOTTY;
	unsigned long flags;

	spin_lock_irqsave(&log->lock, flags);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off != reader->r_off)
			ret = get_entry_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
		    reader->r_off = log->w_off;
		log->head = log->w_off;
		ret = 0;
		break;
	}

	spin_unlock_irqrestore(&log->lock, flags);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

/*
 * Defines a log structure with name 'NAME' and a size of 'SIZE' bytes, which
 * must be a power of two, greater than LOGGER_ENTRY_MAX_LEN, and less than
 * LONG_MAX minus LOGGER_ENTRY_MAX_LEN.
 */
#define DEFINE_LOGGER_DEVICE(VAR, NAME, SIZE)		\
	static unsigned char _buf_ ## VAR[SIZE];	\
static struct logger_log VAR = {			\
	.buffer = _buf_ ## VAR,				\
	.misc = {					\
		.minor = MISC_DYNAMIC_MINOR,		\
		.name = NAME,				\
		.fops = &logger_fops,			\
		.parent = NULL,				\
	},						\
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq),	\
	.readers = LIST_HEAD_INIT(VAR .readers),	\
	.lock = __SPIN_LOCK_UNLOCKED(VAR .lock),	\
	.w_off = 0,					\
	.head = 0,					\
	.size = SIZE,					\
};

#ifndef __KSYSTEM_BUF_SIZE
#define __KSYSTEM_BUF_SIZE 32*1024
#endif

DEFINE_LOGGER_DEVICE(log_ksystem, LOGGER_LOG_KSYSTEM, __KSYSTEM_BUF_SIZE)

/* } mingjian, 20101208 */
static struct logger_log *get_log_from_minor(int minor)
{
	if (log_ksystem.misc.minor == minor)
		return &log_ksystem;
	return NULL;
}

static int __init init_log(struct logger_log *log)
{
	int ret;

	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "logger: failed to register misc "
		       "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	printk(KERN_INFO "logger: created %luK log '%s'\n",
	       (unsigned long)log->size >> 10, log->misc.name);

	return 0;
}

static int __init logger_init(void)
{
	int ret = init_log(&log_ksystem);
	if (unlikely(ret))
		goto out;

 out:
	return ret;
}
device_initcall(logger_init);

static ssize_t xlog_logger_kernel_write(struct logger_log *log, const char *msg, size_t len)
{
	struct logger_entry header;
	struct timespec now;
	ssize_t ret = 0;

/* mingjian, 20110314: make android timestamp same with printk {*/
	getnstimeofday(&now);
	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, len, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len))
		return 0;

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));
	do_write_log(log, msg, header.len);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);

	return ret;
}

char xlog_print_buf[1024];

#define MAX_TAG_LEN 32		/* Include NUL */

int __xlog_ksystem_printk(const struct xlog_record *rec, ...)
{
	va_list args;
	unsigned long flags;
	int tag_len, n;
	struct logger_log *log = &log_ksystem;

	va_start(args, rec);

	spin_lock_irqsave(&log->lock, flags);

	xlog_print_buf[0] = rec->prio;

	tag_len = strlcpy(&xlog_print_buf[1], rec->tag_str, MAX_TAG_LEN);
	if (tag_len > MAX_TAG_LEN) {
		tag_len = MAX_TAG_LEN;
	}

	n = vsnprintf(&xlog_print_buf[tag_len + 1], sizeof(xlog_print_buf) - tag_len - 1,
		      rec->fmt_str, args);

	xlog_logger_kernel_write(log, xlog_print_buf, n + tag_len + 2);

	spin_unlock_irqrestore(&log->lock, flags);

	va_end(args);

	return 1;
}
EXPORT_SYMBOL(__xlog_ksystem_printk);
