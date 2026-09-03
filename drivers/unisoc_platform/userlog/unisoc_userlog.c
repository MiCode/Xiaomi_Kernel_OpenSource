/*
 * Copyright (C) 2021 Unisoc Communications Inc.
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

#define pr_fmt(fmt) "unisoc_userlog: " fmt

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time64.h>
#include <linux/vmalloc.h>
#include <linux/uio.h>
#include <linux/sched/signal.h>
#include "unisoc_userlog.h"

struct userlog_log {
	unsigned char		*buffer;
	struct miscdevice	logdev;
	wait_queue_head_t	wq;
	struct list_head	readers;
	struct mutex		mutex;
	size_t				w_off;
	size_t				head;
	size_t				size;
	size_t				free;
};

struct userlog_reader {
	struct userlog_log	*log;
	struct list_head	list;
	size_t				r_off;
};

struct userlog_log *glog;

/* return userlog offset */

static size_t userlog_offset(struct userlog_log *log, size_t n)
{
	return n & (log->size - 1);
}

/* read sub function start */

static struct userlog_entry *get_entry_header(struct userlog_log *log,
						size_t off, struct userlog_entry *scratch)
{
	size_t len = min(sizeof(struct userlog_entry), log->size - off);

	if (len != sizeof(struct userlog_entry)) {
		memcpy(((void *)scratch), log->buffer + off, len);
		memcpy(((void *)scratch) + len, log->buffer,
		       sizeof(struct userlog_entry) - len);
		return scratch;
	}

	return (struct userlog_entry *) (log->buffer + off);
}

static __u32 get_entry_msg_len(struct userlog_log *log, size_t off)
{
	struct userlog_entry scratch;
	struct userlog_entry *entry;

	entry = get_entry_header(log, off, &scratch);
	return entry->len;
}

static ssize_t do_read_log_to_user(struct userlog_log *log,
					struct userlog_reader *reader,
					char __user *buf,
					size_t count)
{
	struct userlog_entry scratch;
	struct userlog_entry *entry;
	size_t len;
	size_t msg_start;

	/* don't copy the header to userspace */
	entry = get_entry_header(log, reader->r_off, &scratch);

	/* calculate count and buf */
	count -= sizeof(struct userlog_entry);
	msg_start = userlog_offset(log,
					reader->r_off + sizeof(struct userlog_entry));

	/* copy sysmsg to userspace */
	len = min(count, log->size - msg_start);
	if (copy_to_user(buf, log->buffer + msg_start, len))
		return -EFAULT;

	if (count != len) {
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;
	}

	/* calculate next reader offset */
	reader->r_off = userlog_offset(log, reader->r_off +
		sizeof(struct userlog_entry) + count);

	return count;
}

/* read sub function end */

/* userlog_read - our log's read() method */

static ssize_t userlog_read(struct file *file, char __user *buf,
					size_t count, loff_t *pos)
{
	struct userlog_reader *reader = file->private_data;
	struct userlog_log *log = glog;
	ssize_t ret;
	ssize_t entry_size;
	DEFINE_WAIT(wait);

	/* read userlog check */
	while (1) {
		mutex_lock(&log->mutex);

		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		ret = (glog->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
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
	/* read userlog start */
	mutex_lock(&log->mutex);

	/* get the size of the next entry*/
	entry_size = sizeof(struct userlog_entry) + get_entry_msg_len(log, reader->r_off);

	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, buf, entry_size);

	mutex_unlock(&log->mutex);

	return ret;
}

/* userlog_write_iter - implementing support for write()*/

static ssize_t userlog_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct userlog_log *log = glog;
	struct userlog_entry header;
	struct userlog_reader *reader;
	size_t len, count, w_off, oldhead;
	ssize_t entry_size;

	count = min_t(size_t, iov_iter_count(from), USERLOG_ENTRY_MAX_PAYLOAD);

	/* set header info */
	header.len = count;

	/* check write count zero, return 0 */
	if (unlikely(!header.len))
		return 0;

	mutex_lock(&log->mutex);

	/* rollover messages until the free space is enough */
	while (sizeof(header) + count + 1 > log->free) {
		/* calculate the entry size at the head location */
		entry_size = sizeof(struct userlog_entry) + get_entry_msg_len(log, log->head);
		oldhead = log->head;

		/* update the head offset */
		log->head = userlog_offset(log, log->head + entry_size);
		log->free += entry_size;

		/* update the read offset of all readers */
		list_for_each_entry(reader, &log->readers, list)
			if (reader->r_off == oldhead)
				reader->r_off = log->head;
	}

	/* write userlog header message */

	len = min(sizeof(header), log->size - log->w_off);
	memcpy(log->buffer + log->w_off, &header, len);
	memcpy(log->buffer, (char *)&header + len, sizeof(header) - len);

	/* get write offset for system message */
	w_off =  userlog_offset(log, log->w_off + sizeof(struct userlog_entry));

	/* write userlog system message */
	len = min(count, log->size - w_off);

	if (copy_from_iter(log->buffer + w_off, len, from) != len) {
		mutex_unlock(&log->mutex);
		return -EFAULT;
	}

	if (copy_from_iter(log->buffer, count - len, from) != count - len) {
		mutex_unlock(&log->mutex);
		return -EFAULT;
	}

	/* get write offset */
	log->w_off = userlog_offset(log, w_off + count);
	log->free -= sizeof(header) + count;

	mutex_unlock(&log->mutex);

	/* wake up any readers */
	wake_up_interruptible(&log->wq);

	return len;
}

/* userlog_open - the log's open() file operation */

static int userlog_open(struct inode *inode, struct file *file)
{
	struct userlog_reader *reader;

	reader = kmalloc(sizeof(struct userlog_reader), GFP_KERNEL);
	if (!reader)
		return -ENOMEM;
	reader->log = glog;
	mutex_lock(&glog->mutex);
	reader->r_off = glog->head;
	list_add(&reader->list, &glog->readers);
	mutex_unlock(&glog->mutex);
	file->private_data = reader;

	return 0;
}

/* userlog_release - the log's release file operation */

static int userlog_release(struct inode *ignored, struct file *file)
{
	struct userlog_reader *reader = file->private_data;

	mutex_lock(&glog->mutex);
	list_del(&reader->list);
	mutex_unlock(&glog->mutex);
	kfree(reader);

	pr_info("userlog_release\n");
	return 0;
}

static const struct file_operations userlog_fops = {
	.owner = THIS_MODULE,
	.read = userlog_read,
	.write_iter = userlog_write_iter,
	.open = userlog_open,
	.release = userlog_release,
};

/* create userlog init*/
static int __init create_userlog(char *log_name, int size)
{
	int ret = 0;
	unsigned char *buffer;

	/* vmalloc buffer size */
	buffer = vmalloc(size);
	if (buffer == NULL)
		return -ENOMEM;

	/* kzalloc userlog log */
	glog = kzalloc(sizeof(struct userlog_log), GFP_KERNEL);
	if (glog == NULL) {
		ret = -ENOMEM;
		goto out_free_buffer;
	}
	glog->buffer = buffer;

	glog->logdev.minor = MISC_DYNAMIC_MINOR;
	glog->logdev.name = kstrdup(log_name, GFP_KERNEL);
	if (glog->logdev.name == NULL) {
		ret = -ENOMEM;
		goto out_free_log;
	}

	/* init logdev fops */
	glog->logdev.fops = &userlog_fops;
	glog->logdev.parent = NULL;

	/* init wq */
	init_waitqueue_head(&glog->wq);
	/* init mutex */
	mutex_init(&glog->mutex);
	/* init list */
	INIT_LIST_HEAD(&glog->readers);

	/* init log w_off,head,size */
	glog->w_off = 0;
	glog->head = 0;
	glog->size = size;
	glog->free = size;

	/* register misc device for this log dev */
	ret = misc_register(&glog->logdev);
	if (unlikely(ret)) {
		pr_err("failed to register misc device for log '%s'!\n",
				glog->logdev.name);
		goto out_free_logdev_name;
	}

	pr_info("created %luK log '%s'\n",
			(unsigned long)glog->size >> 10, glog->logdev.name);

	return 0;

out_free_logdev_name:
	kfree(glog->logdev.name);

out_free_log:
	kfree(glog);

out_free_buffer:
	vfree(buffer);
	return ret;
}

/* module init */
static int __init unisoc_userlog_init(void)
{
	int ret;

	/* create userlog_system */
	pr_info("unisoc_userlog_init\n");
	ret = create_userlog(USERLOG_SYSTEM, 256*1024);
	return ret;
}

/* module exit */
static void __exit unisoc_userlog_exit(void)
{
	misc_deregister(&glog->logdev);
	vfree(glog->buffer);
	kfree(glog->logdev.name);
	kfree(glog);
	pr_info("unisoc_userlog_exit\n");
}

device_initcall(unisoc_userlog_init);
module_exit(unisoc_userlog_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhenguo.Zhao, <zhenguo.zhao1@unisoc.com>");
MODULE_DESCRIPTION("Unisoc User Log Driver");
