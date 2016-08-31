/*
 * drivers/misc/tegra-profiler/auth.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/uaccess.h>

#include "auth.h"
#include "quadd.h"
#include "debug.h"

#define QUADD_SECURITY_MAGIC_REQUEST	0x11112222
#define QUADD_SECURITY_MAGIC_RESPONSE	0x33334444

#define QUADD_TIMEOUT	1000	/* msec */

enum {
	QUADD_SECURITY_RESPONSE_ERROR			= 0,
	QUADD_SECURITY_RESPONSE_DEBUG_FLAG_ON		= 1,
	QUADD_SECURITY_RESPONSE_DEBUG_FLAG_OFF		= 2,
	QUADD_SECURITY_RESPONSE_PACKAGE_NOT_FOUND	= 3,
};

enum {
	QUADD_SECURITY_REQUEST_CMD_TEST_DEBUG_FLAG	= 1,
	QUADD_SECURITY_RESPONSE_CMD_TEST_DEBUG_FLAG	= 2,
};

struct quadd_auth_data {
	char package_name[QUADD_MAX_PACKAGE_NAME];

	uid_t debug_app_uid;
	int response_value;
};

static struct quadd_auth_context {
	struct miscdevice misc_dev;

	atomic_t opened;

	wait_queue_head_t request_wait;
	wait_queue_head_t response_wait;

	int request_ready;
	int response_ready;
	struct quadd_auth_data data;
	struct mutex lock;

	unsigned int msg_id;

	struct quadd_ctx *quadd_ctx;
} auth_ctx;

static inline void response_ready(void)
{
	auth_ctx.response_ready = 1;
	wake_up_interruptible(&auth_ctx.response_wait);
}

static inline void request_ready(void)
{
	auth_ctx.request_ready = 1;
	wake_up_interruptible(&auth_ctx.request_wait);
}

static int auth_open(struct inode *inode, struct file *file)
{
	struct quadd_auth_data *data = &auth_ctx.data;

	if (atomic_cmpxchg(&auth_ctx.opened, 0, 1)) {
		pr_err("Error: auth file is already opened\n");
		return -EBUSY;
	}
	pr_info("auth is opened\n");

	auth_ctx.request_ready = 0;
	auth_ctx.response_ready = 0;

	mutex_lock(&auth_ctx.lock);
	data->package_name[0] = '\0';
	data->debug_app_uid = 0;
	data->response_value = 0;
	mutex_unlock(&auth_ctx.lock);

	return 0;
}

static int auth_release(struct inode *inode, struct file *file)
{
	pr_info("auth is released\n");
	atomic_set(&auth_ctx.opened, 0);
	return 0;
}

static ssize_t
auth_read(struct file *filp,
	    char __user *user_buf,
	    size_t length,
	    loff_t *offset)
{
	char buf[QUADD_MAX_PACKAGE_NAME + 4 * sizeof(u32)];
	int msg_length, err;
	struct quadd_auth_data *data = &auth_ctx.data;

	wait_event_interruptible(auth_ctx.request_wait, auth_ctx.request_ready);

	mutex_lock(&auth_ctx.lock);

	((u32 *)buf)[0] = QUADD_SECURITY_MAGIC_REQUEST;
	((u32 *)buf)[1] = ++auth_ctx.msg_id;
	((u32 *)buf)[2] = QUADD_SECURITY_REQUEST_CMD_TEST_DEBUG_FLAG;
	((u32 *)buf)[3] = strlen(data->package_name);

	strcpy(buf + 4 * sizeof(u32), data->package_name);
	msg_length = strlen(data->package_name) + 4 * sizeof(u32);

	mutex_unlock(&auth_ctx.lock);

	err = copy_to_user(user_buf, buf, msg_length);
	if (err != 0) {
		pr_err("Error: copy to user: %d\n", err);
		return err;
	}

	pr_info("auth read, msg_length: %d\n", msg_length);
	return msg_length;
}

static ssize_t
auth_write(struct file *file,
	  const char __user *user_buf,
	  size_t count,
	  loff_t *ppos)
{
	int err;
	char buf[5 * sizeof(u32)];
	u32 magic, response_cmd, response_value, length, uid, msg_id;
	struct quadd_auth_data *data = &auth_ctx.data;

	pr_info("auth read, count: %d\n", count);

	mutex_lock(&auth_ctx.lock);
	data->response_value = QUADD_SECURITY_RESPONSE_ERROR;
	data->debug_app_uid = 0;
	mutex_unlock(&auth_ctx.lock);

	if (count < 5 * sizeof(u32)) {
		pr_err("Error count: %u\n", count);
		response_ready();
		return -E2BIG;
	}

	err = copy_from_user(buf, user_buf, 5 * sizeof(u32));
	if (err) {
		pr_err("Error: copy from user: %d\n", err);
		response_ready();
		return err;
	}

	magic = ((u32 *)buf)[0];
	if (magic != QUADD_SECURITY_MAGIC_RESPONSE) {
		pr_err("Error magic: %#x\n", magic);
		response_ready();
		return -EINVAL;
	}

	msg_id = ((u32 *)buf)[1];
	if (msg_id != auth_ctx.msg_id) {
		pr_err("Error message id: %u\n", msg_id);
		response_ready();
		return -EINVAL;
	}

	response_cmd = ((u32 *)buf)[2];
	response_value = ((u32 *)buf)[3];
	length = ((u32 *)buf)[4];

	switch (response_cmd) {
	case QUADD_SECURITY_RESPONSE_CMD_TEST_DEBUG_FLAG:
		if (length != 4) {
			pr_err("Error: too long data: %u\n", length);
			response_ready();
			return -E2BIG;
		}

		err = get_user(uid, (u32 __user *)user_buf + 5);
		if (err) {
			pr_err("Error: copy from user: %d\n", err);
			response_ready();
			return err;
		}

		mutex_lock(&auth_ctx.lock);
		data->response_value = response_value;
		data->debug_app_uid = uid;
		mutex_unlock(&auth_ctx.lock);

		pr_info("uid: %u, response_value: %u\n",
			uid, response_value);
		break;

	default:
		pr_err("Error: invalid response command: %u\n",
		       response_cmd);
		response_ready();
		return -EINVAL;
	}
	response_ready();

	return count;
}

static const struct file_operations auth_fops = {
	.read		= auth_read,
	.write		= auth_write,
	.open		= auth_open,
	.release	= auth_release,
};

int quadd_auth_check_debug_flag(const char *package_name)
{
	int uid, response_value;
	struct quadd_auth_data *data = &auth_ctx.data;
	int pkg_name_length;

	if (!package_name)
		return -EINVAL;

	pkg_name_length = strlen(package_name);
	if (pkg_name_length == 0 ||
	    pkg_name_length > QUADD_MAX_PACKAGE_NAME)
		return -EINVAL;

	if (atomic_read(&auth_ctx.opened) == 0)
		return -EIO;

	mutex_lock(&auth_ctx.lock);
	data->debug_app_uid = 0;
	data->response_value = 0;

	strncpy(data->package_name, package_name, QUADD_MAX_PACKAGE_NAME);
	mutex_unlock(&auth_ctx.lock);

	request_ready();

	wait_event_interruptible_timeout(auth_ctx.response_wait,
					 auth_ctx.response_ready,
					 msecs_to_jiffies(QUADD_TIMEOUT));
	if (!auth_ctx.response_ready) {
		pr_err("Error: Tegra profiler service did not answer\n");
		return -ETIMEDOUT;
	}

	mutex_lock(&auth_ctx.lock);
	uid = data->debug_app_uid;
	response_value = data->response_value;
	mutex_unlock(&auth_ctx.lock);

	switch (response_value) {
	case QUADD_SECURITY_RESPONSE_DEBUG_FLAG_ON:
		pr_info("package %s is debuggable, uid: %d\n",
			package_name, uid);
		return uid;

	case QUADD_SECURITY_RESPONSE_DEBUG_FLAG_OFF:
		pr_info("package %s is not debuggable\n",
			package_name);
		return 0;

	case QUADD_SECURITY_RESPONSE_PACKAGE_NOT_FOUND:
		pr_err("Error: package %s not found\n", package_name);
		return -ESRCH;

	case QUADD_SECURITY_RESPONSE_ERROR:
	default:
		pr_err("Error: invalid response\n");
		return -EBADMSG;
	}
}

int quadd_auth_is_auth_open(void)
{
	return atomic_read(&auth_ctx.opened) != 0;
}

int quadd_auth_init(struct quadd_ctx *quadd_ctx)
{
	int err;
	struct miscdevice *misc_dev = &auth_ctx.misc_dev;

	pr_info("auth: init\n");

	misc_dev->minor = MISC_DYNAMIC_MINOR;
	misc_dev->name = QUADD_AUTH_DEVICE_NAME;
	misc_dev->fops = &auth_fops;

	err = misc_register(misc_dev);
	if (err < 0) {
		pr_err("Error: misc_register %d\n", err);
		return err;
	}

	init_waitqueue_head(&auth_ctx.request_wait);
	init_waitqueue_head(&auth_ctx.response_wait);

	auth_ctx.request_ready = 0;
	auth_ctx.response_ready = 0;

	atomic_set(&auth_ctx.opened, 0);
	mutex_init(&auth_ctx.lock);
	auth_ctx.msg_id = 0;

	auth_ctx.quadd_ctx = quadd_ctx;
	return 0;
}

void quadd_auth_deinit(void)
{
	struct miscdevice *misc_dev = &auth_ctx.misc_dev;

	pr_info("auth: deinit\n");
	misc_deregister(misc_dev);
}
