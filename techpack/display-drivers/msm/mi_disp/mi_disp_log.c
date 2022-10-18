/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mi_disp_log:[%s:%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/crypto.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/rtc.h>

#include "mi_disp_config.h"
#include "mi_disp_log.h"
#include "mi_disp_print.h"
#include "mi_disp_core.h"

#if MI_DISP_LOG_ENABLE

#define DISP_LOG_DEVICE_NAME "disp_log"

/* DISP_LOG_BUF_SIZE = 4K */
#define DISP_LOG_BUF_SIZE 0x1000

enum disp_log_type {
	LOG_TYPE_KERNEL = 0,   /* mi display kernel log */
	LOG_TYPE_USER   = 1,   /* mi display user log */
	LOG_TYPE_MAX,
};

/* Log ring buffer position */
struct log_pos {
	u32 wrap;
	u32 offset;
};

/* Log ring buffer */
struct log_buf {
	struct spinlock lock;
	struct wait_queue_head wq_head;
	atomic_t wait;
	struct log_pos pos;
	size_t size;
	char *buf;
};

struct disp_log {
	dev_t dev_id;
	struct class *class;
	struct cdev *cdev;
	struct device *dev;
	struct dentry *debug;
	bool initialized;
	struct mutex lock;
	struct log_buf log[LOG_TYPE_MAX];
};

struct disp_log_read {
	struct log_pos read_pos;
	u8 log_type;
	char buf[DISP_LOG_BUF_SIZE];
	struct mutex lock;
	void *private;
};

struct disp_log *g_disp_log = NULL;

bool mi_disp_log_is_initialized(void)
{
	return g_disp_log ? g_disp_log->initialized : false;
}

static int mi_disp_log_printk_func(struct log_buf *log_buf,
		const char *fmt, va_list args)
{
	static char textbuf[256];
	char *text = textbuf;
	u64 ts_usec,ts_sec;
	u32 text_len;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&log_buf->lock,flags);

	ts_usec = local_clock()/1000;
	ts_sec = ts_usec/1000000;
	ts_usec %= 1000000;
	text_len = snprintf(text, sizeof(textbuf), "[%5lld.%06lld] ",
					ts_sec, ts_usec);
	text_len += vsnprintf(text + text_len, sizeof(textbuf) - text_len,
					fmt, args);

	for (i = 0; i < text_len; i++) {
		log_buf->buf[log_buf->pos.offset++] = *text++;
		if (log_buf->pos.offset == log_buf->size) {
			log_buf->pos.offset = 0;
			++log_buf->pos.wrap;
		}
	}

	spin_unlock_irqrestore(&log_buf->lock, flags);

	return text_len;
}

static int mi_disp_log_kenrel_printk(const char *fmt, ...)
{
	va_list args;
	int rc;
	struct log_buf *log_buf = NULL;

	if (!mi_disp_log_is_initialized()) {
		DISP_ERROR("mi disp_log not initialized!\n");
		return -ENODEV;
	}

	log_buf = &g_disp_log->log[LOG_TYPE_KERNEL];

	va_start(args, fmt);
	rc = mi_disp_log_printk_func(log_buf, fmt, args);
	va_end(args);

	if (atomic_read(&log_buf->wait)) {
		atomic_set(&log_buf->wait, 0);
		wake_up_all(&log_buf->wq_head);
	}
	return rc;
}

static int mi_disp_log_user_printk(struct log_buf *log_buf,
		const char *fmt, ...)
{
	va_list args;
	int rc;

	va_start(args, fmt);
	rc = mi_disp_log_printk_func(log_buf, fmt, args);
	va_end(args);

	if (atomic_read(&log_buf->wait)) {
		atomic_set(&log_buf->wait, 0);
		wake_up_all(&log_buf->wq_head);
	}
	return rc;
}

void disp_log_printk(const char *format, ...)
{
	struct va_format vaf;
	va_list args;

	if (!mi_disp_log_is_initialized())
		return;

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	mi_disp_log_kenrel_printk("[%ps] %pV",
	       __builtin_return_address(0), &vaf);

	va_end(args);
}
EXPORT_SYMBOL(disp_log_printk);

void disp_log_printk_utc(const char *format, ...)
{
	struct timespec64 ts;
	struct tm tm;
	struct va_format vaf;
	va_list args;

	if (!mi_disp_log_is_initialized())
		return;

	ktime_get_real_ts64(&ts);
	time64_to_tm(ts.tv_sec, 0, &tm);

	va_start(args, format);
	vaf.fmt = format;
	vaf.va = &args;

	mi_disp_log_kenrel_printk("[%ps][%02d-%02d %02d:%02d:%02d.%06lu] %pV",
			__builtin_return_address(0),
			tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min,
			tm.tm_sec, ts.tv_nsec/1000,
			&vaf);

	va_end(args);
}
EXPORT_SYMBOL(disp_log_printk_utc);

static int mi_disp_log_read_stats(struct log_buf *log,
		struct disp_log_read *read_ptr, size_t count)
{
	uint32_t wrap_start;
	uint32_t wrap_end;
	uint32_t wrap_cnt;
	int max_len = DISP_LOG_BUF_SIZE;
	int len = 0;
	int i = 0;
	struct log_pos *log_start = &read_ptr->read_pos;

	wrap_start = log_start->wrap;
	wrap_end = log->pos.wrap;

	/* Calculate difference in # of buffer wrap-arounds */
	if (wrap_end >= wrap_start)
		wrap_cnt = wrap_end - wrap_start;
	else {
		/* wrap counter has wrapped around, invalidate start position */
		wrap_cnt = 2;
	}

	if (wrap_cnt > 1) {
		/* end position has wrapped around more than once, */
		/* current start no longer valid                   */
		log_start->wrap = log->pos.wrap - 1;
		log_start->offset = log->pos.offset + 1;
	} else if ((wrap_cnt == 1) &&
		(log->pos.offset > log_start->offset)) {
		/* end position has overwritten start */
		log_start->offset = log->pos.offset + 1;
	}

	while (log_start->offset == log->pos.offset) {
		/*
		 * No data in ring buffer,
		 * so we'll hang around until something happens
		 */
		atomic_set(&log->wait, 1);
		if (wait_event_freezable(log->wq_head, atomic_read(&log->wait) == 0)) {
			/* Some event woke us up, so let's quit */
			return 0;
		}
	}

	max_len = (count > DISP_LOG_BUF_SIZE) ? DISP_LOG_BUF_SIZE : count;

	/* Read from ring buff while there is data and space in return buff */
	while ((log_start->offset != log->pos.offset) && (len < max_len)) {
		read_ptr->buf[i++] = log->buf[log_start->offset];
		log_start->offset = (log_start->offset + 1) % log->size;
		if (log_start->offset == 0)
			++log_start->wrap;
		++len;
	}

	return len;
}

static ssize_t mi_disp_log_debugfs_read(struct file *file,
		char __user *buf, size_t count, loff_t *offp)
{
	int ret = 0;
	struct disp_log_read *read_ptr =  file->private_data;
	struct disp_log *disp_log =
			(struct disp_log *)read_ptr->private;
	struct log_buf *log_buf;
	int len = 0;

	ret = mutex_lock_interruptible(&read_ptr->lock);
	if (ret)
		return ret;

	if (read_ptr->log_type == LOG_TYPE_KERNEL) {
		log_buf = &disp_log->log[LOG_TYPE_KERNEL];
	} else {
		log_buf = &disp_log->log[LOG_TYPE_USER];
	}

	*offp = 0;
	len = mi_disp_log_read_stats(log_buf, read_ptr, count);

	if (len > count)
		len = count;

	len = simple_read_from_buffer(buf, len, offp, read_ptr->buf, len);

	mutex_unlock(&read_ptr->lock);

	return len;
}

static ssize_t mi_disp_log_debugfs_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int ret = 0;
	struct disp_log_read *read_ptr =  file->private_data;
	char *input = NULL;

	input = kmalloc(count, GFP_KERNEL);
	if (!input) {
		return -ENOMEM;
	}
	if (copy_from_user(input, buf, count)) {
		DISP_ERROR("copy from user failed\n");
		ret = -EFAULT;
		goto exit;
	}
	input[count - 1] = '\0';
	ret = kstrtou8(input, 10, &read_ptr->log_type);
	if (ret) {
		DISP_ERROR("input buffer conversion failed\n");
		goto exit;
	}

exit:
	kfree(input);
	return ret ? ret : count;
}


int mi_disp_log_debugfs_open(struct inode *inode, struct file *file)
{
	struct disp_log_read *read_ptr = NULL;

	read_ptr = kzalloc(sizeof(struct disp_log_read), GFP_KERNEL);
	if (!read_ptr)
		return -ENOMEM;

	mutex_init(&read_ptr->lock);
	file->private_data = read_ptr;
	if (inode->i_private)
		read_ptr->private = inode->i_private;

	return 0;
}

static int mi_disp_log_debugfs_release(struct inode *inode, struct file *file)
{
	struct disp_log_read *read_ptr = file->private_data;

	if (!read_ptr)
		return 0;

	mutex_destroy(&read_ptr->lock);
	kfree(read_ptr);
	return 0;
}


static const struct file_operations disp_log_debugfs_fops = {
	.owner   = THIS_MODULE,
	.open    = mi_disp_log_debugfs_open,
	.read    = mi_disp_log_debugfs_read,
	.write   = mi_disp_log_debugfs_write,
	.release = mi_disp_log_debugfs_release,
};

static ssize_t mi_disp_log_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *ppos)
{
	int ret;
	struct disp_log *disp_log = file->private_data;
	struct log_buf *log_buf = NULL;
	char *input = NULL;

	ret = mutex_lock_interruptible(&disp_log->lock);
	if (ret)
		return ret;

	input = kmalloc(count, GFP_KERNEL);
	if (!input) {
		ret = -ENOMEM;
		goto err_exit;
	}
	if (copy_from_user(input, buf, count)) {
		DISP_ERROR("copy from user failed\n");
		ret = -EFAULT;
		goto err_free_mem;
	}

	log_buf = &disp_log->log[LOG_TYPE_USER];
	ret = mi_disp_log_user_printk(log_buf, "%s", input);

err_free_mem:
	kfree(input);
err_exit:
	mutex_unlock(&disp_log->lock);
	return ret;
}

static ssize_t mi_disp_log_read(struct file *file, char __user *buf,
			    size_t count, loff_t *ppos)
{
	struct disp_log *disp_log =
			(struct disp_log *)file->private_data;
	struct log_buf *log_buf = &disp_log->log[LOG_TYPE_USER];
	size_t max_len = log_buf->pos.wrap ? log_buf->size : log_buf->pos.offset;
	size_t read_len = 0;
	ssize_t ret;

	ret = mutex_lock_interruptible(&disp_log->lock);
	if (ret)
		return ret;

	if (count > max_len)
		read_len = max_len;
	else
		read_len = count;

	if (log_buf->pos.wrap) {
		if (log_buf->pos.offset + read_len < log_buf->size) {
			if (copy_to_user(buf, &log_buf->buf[log_buf->pos.offset],
					read_len)) {
				ret = -EFAULT;
				goto out;
			}
		} else {
			if (copy_to_user(buf, &log_buf->buf[log_buf->pos.offset],
					log_buf->size - log_buf->pos.offset)) {
				ret = -EFAULT;
				goto out;
			}
			if (copy_to_user(buf + log_buf->size - log_buf->pos.offset,
					&log_buf->buf[log_buf->pos.offset],
					read_len - (log_buf->size - log_buf->pos.offset))) {
				ret = -EFAULT;
				goto out;
			}
		}
	} else {
		if (copy_to_user(buf, log_buf->buf, read_len)) {
			ret = -EFAULT;
			goto out;
		}
	}

	ret = read_len;
out:
	mutex_unlock(&disp_log->lock);
	return ret;
}

int mi_disp_log_open(struct inode *inode, struct file *file)
{
	file->private_data = g_disp_log;
	return 0;
}

const struct file_operations disp_log_fops = {
	.owner = THIS_MODULE,
	.open = mi_disp_log_open,
	.read = mi_disp_log_read,
	.write = mi_disp_log_write,
};

static ssize_t info_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_log *disp_log = dev_get_drvdata(device);
	ssize_t count = 0;
	struct log_buf *log_buf = NULL;

	log_buf = &disp_log->log[LOG_TYPE_KERNEL];
	count = snprintf(buf, PAGE_SIZE,
				"Kernel Display log parameter information:\n");
	count += snprintf(buf + count, PAGE_SIZE - count,
				"buffer size = %d\n",
				(u32)log_buf->size);
	count += snprintf(buf + count, PAGE_SIZE - count,
				"buffer wrap arounds = %d\n",
				log_buf->pos.wrap);
	count += snprintf(buf + count, PAGE_SIZE - count,
				"buffer write position = %d\n",
				log_buf->pos.offset);

	log_buf = &disp_log->log[LOG_TYPE_USER];
	count += snprintf(buf + count, PAGE_SIZE - count,
				"User Display log parameter information:\n");
	count += snprintf(buf + count, PAGE_SIZE - count,
				"buffer size = %d\n",
				(u32)log_buf->size);
	count += snprintf(buf + count, PAGE_SIZE - count,
				"buffer wrap arounds = %d\n",
				log_buf->pos.wrap);
	count += snprintf(buf + count, PAGE_SIZE - count,
				"buffer write position = %d\n",
				log_buf->pos.offset);

	return count;
}

static ssize_t user_buf_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_log *disp_log = dev_get_drvdata(device);
	struct log_buf *log_buf = NULL;

	log_buf = &disp_log->log[LOG_TYPE_USER];

	/* sysfs can only support PAGE_SIZE buffer */
	if (count < PAGE_SIZE)
		count = mi_disp_log_user_printk(log_buf, "%s", buf);
	else
		DISP_ERROR("too large buffer\n");

	return count;
}

static ssize_t user_buf_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_log *disp_log = dev_get_drvdata(device);
	ssize_t count = 0;
	struct log_buf *log_buf = NULL;

	log_buf = &disp_log->log[LOG_TYPE_USER];
	/* sysfs can only support PAGE_SIZE buffer */
	count = snprintf(buf, PAGE_SIZE,"%s\n", log_buf->buf);
	return count;
}

static ssize_t kernel_buf_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_log *disp_log = dev_get_drvdata(device);
	ssize_t count = 0;
	struct log_buf *log_buf = NULL;

	log_buf = &disp_log->log[LOG_TYPE_KERNEL];
	/* sysfs can only support PAGE_SIZE buffer */
	count = snprintf(buf, PAGE_SIZE,"%s\n", log_buf->buf);
	return count;
}

static DEVICE_ATTR_RO(info);
static DEVICE_ATTR_RW(user_buf);
static DEVICE_ATTR_RO(kernel_buf);

static struct attribute *disp_log_dev_attrs[] = {
	&dev_attr_info.attr,
	&dev_attr_user_buf.attr,
	&dev_attr_kernel_buf.attr,
	NULL
};

static const struct attribute_group disp_log_dev_group = {
	.attrs = disp_log_dev_attrs,
};

static const struct attribute_group *disp_log_dev_groups[] = {
	&disp_log_dev_group,
	NULL
};

int mi_disp_log_init(void)
{
	int ret = 0;
	struct disp_log *disp_log = NULL;
	struct log_buf *log_buf = NULL;
	struct disp_core *disp_core = mi_get_disp_core();
	char *buf = NULL;
	int i = 0;

	if (g_disp_log) {
		DISP_INFO("mi disp_log already initialized, return!\n");
		return 0;
	}

	buf = kzalloc(sizeof(*disp_log) + (2 * DISP_LOG_BUF_SIZE),
				GFP_KERNEL);
	if (!buf) {
		DISP_ERROR("can not allocate Buffer\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	disp_log = (struct disp_log *)buf;
	mutex_init(&disp_log->lock);
	buf = &buf[sizeof(*disp_log)];
	for (i = 0; i < LOG_TYPE_MAX; i++) {
		log_buf = &disp_log->log[i];
		spin_lock_init(&log_buf->lock);
		atomic_set(&log_buf->wait, 0);
		init_waitqueue_head(&log_buf->wq_head);
		log_buf->size = DISP_LOG_BUF_SIZE;
		log_buf->buf = &buf[i * DISP_LOG_BUF_SIZE];
	}

	ret = mi_disp_cdev_register(DISP_LOG_DEVICE_NAME,
				&disp_log_fops, &disp_log->cdev);
	if (ret < 0) {
		DISP_ERROR("cdev register failed for %s\n", DISP_LOG_DEVICE_NAME);
		goto err_free_mem;
	}

	disp_log->dev_id = disp_log->cdev->dev;
	disp_log->class = disp_core ? disp_core->class : NULL;
	disp_log->dev = device_create_with_groups(disp_log->class, NULL,
			disp_log->dev_id, disp_log,
			disp_log_dev_groups, DISP_LOG_DEVICE_NAME);
	if (IS_ERR(disp_log->dev)) {
		DISP_ERROR("create device failed for \n");
		ret = -ENODEV;
		goto err_cdev_unreg;
	}

	disp_log->debug = debugfs_create_file(DISP_LOG_DEVICE_NAME,
			0444, disp_core ? disp_core->debugfs_dir : NULL,
			disp_log, &disp_log_debugfs_fops);
	if (disp_log->debug == NULL) {
		DISP_ERROR("debugfs_create_file failed for %s \n", DISP_LOG_DEVICE_NAME);
		ret = -ENOMEM;
		goto err_dev_destroy;
	}

	disp_log->initialized = true;
	g_disp_log = disp_log;

	DISP_INFO("mi disp_log driver initialized!\n");

	return 0;

err_dev_destroy:
	device_destroy(disp_log->class, disp_log->dev_id);
err_cdev_unreg:
	mi_disp_cdev_unregister(disp_log->cdev);
err_free_mem:
	kfree(disp_log);
err_exit:
	return ret;
}

void mi_disp_log_deinit(void)
{
	if (!g_disp_log)
		return;

	debugfs_remove(g_disp_log->debug);
	device_destroy(g_disp_log->class, g_disp_log->dev_id);
	mi_disp_cdev_unregister(g_disp_log->cdev);
	kfree(g_disp_log);
	g_disp_log = NULL;
}

#endif /* MI_DISP_LOG_ENABLE */
