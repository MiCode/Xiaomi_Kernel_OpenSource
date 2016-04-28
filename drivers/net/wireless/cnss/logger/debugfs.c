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

#include <linux/module.h>
#include <linux/debugfs.h>

#include "logger.h"

#define CNSS_LOGGER_STATE_DUMP_BUFFER	(2 * 1024) /* 2KB */

static int logger_state_dump_device(struct logger_device *dev, char *buf,
				    int buf_len)
{
	int len = 0;
	struct logger_event_handler *cur;

	len += scnprintf(buf + len, buf_len - len,
			"==============================================\n");

	len += scnprintf(buf + len, buf_len - len,
			"driver [%s] is registered with radio index: %d\n",
			dev->name, dev->radio_idx);

	if (list_empty(&dev->event_list)) {
		len += scnprintf(buf + len, buf_len - len,
				 "No event registered!\n");
		return len;
	}

	list_for_each_entry(cur, &dev->event_list, list) {
		len += scnprintf(buf + len, buf_len - len,
				"\t event %d\n", cur->event);
	}
	len += scnprintf(buf + len, buf_len - len, "\n");

	return len;
}

static int logger_state_dump(struct logger_context *ctx, char *buf, int buf_len)
{
	int len = 0;
	struct logger_device *cur;

	if (list_empty(&ctx->dev_list)) {
		len += scnprintf(buf + len, buf_len - len,
				 "=======================\n");
		len += scnprintf(buf + len, buf_len - len,
				 "No driver registered!\n");
		return 0;
	}

	list_for_each_entry(cur, &ctx->dev_list, list)
		len += logger_state_dump_device(cur, (buf + len), buf_len);

	return 0;
}

static int logger_state_open(struct inode *inode, struct file *file)
{
	struct logger_context *ctx = inode->i_private;
	void *buf;
	int ret;

	mutex_lock(&ctx->con_mutex);

	buf = kmalloc(CNSS_LOGGER_STATE_DUMP_BUFFER, GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto error_unlock;
	}

	ret = logger_state_dump(ctx, buf, CNSS_LOGGER_STATE_DUMP_BUFFER);
	if (ret)
		goto error_free;

	file->private_data = buf;
	mutex_unlock(&ctx->con_mutex);
	return 0;

error_free:
	kfree(buf);

error_unlock:
	mutex_unlock(&ctx->con_mutex);

	return ret;
}

static int logger_state_release(struct inode *inode, struct file *file)
{
	kfree(file->private_data);
	return 0;
}

static ssize_t logger_state_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	const char *buf = file->private_data;
	unsigned int len = strlen(buf);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_logger_state = {
	.open = logger_state_open,
	.release = logger_state_release,
	.read = logger_state_read,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

void logger_debugfs_init(struct logger_context *ctx)
{
	if (!ctx->debugfs_entry)
		ctx->debugfs_entry = debugfs_create_dir("cnss_logger", NULL);

	debugfs_create_file("state", S_IRUSR, ctx->debugfs_entry, ctx,
			    &fops_logger_state);
}

void logger_debugfs_remove(struct logger_context *ctx)
{
	debugfs_remove(ctx->debugfs_entry);
}

