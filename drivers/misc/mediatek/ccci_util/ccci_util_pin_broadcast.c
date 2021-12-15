// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2017 MediaTek Inc.
 */



#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <asm/ioctl.h>
#include <linux/compiler.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "ccci_util_log.h"
#include "ccci_util_lib_main.h"

/* PIN broadcast event defination */
struct pin_status_event {
	int pin_value;
	char pin_name[32];
};

struct pin_event_user_ctrl {
	struct pin_status_event pin_event;
	int pin_update;
	struct list_head entry;
};

static dev_t s_pin_status_dev;
static struct cdev s_pin_char_dev;
static struct class *s_ccci_pin_class;
static struct pin_status_event *pin_event;
static wait_queue_head_t pin_event_wait;
static spinlock_t pin_event_update_lock;
static struct list_head user_list;

void inject_pin_status_event(int pin_value, const char pin_name[])
{
	struct pin_event_user_ctrl *user_ctrl;
	int ret = 0;

	spin_lock(&pin_event_update_lock);
	if (pin_name != NULL)
		ret = snprintf(pin_event->pin_name, 32, "%s", pin_name);
	else
		ret = snprintf(pin_event->pin_name, 32, "%s", "----");

	if (ret < 0 || ret >= 32)
		CCCI_UTIL_ERR_MSG(
			"%s-%d:snprintf fail,ret=%d\n",
			__func__, __LINE__, ret);

	pin_event->pin_value = pin_value;
	list_for_each_entry(user_ctrl, &user_list, entry)
		user_ctrl->pin_update = 1;
	wake_up_interruptible(&pin_event_wait);
	spin_unlock(&pin_event_update_lock);
}

static int ccci_util_pin_bc_open(struct inode *inode, struct file *filp)
{
	struct pin_event_user_ctrl *user_ctrl;

	user_ctrl = kzalloc(sizeof(struct pin_event_user_ctrl), GFP_KERNEL);
	if (user_ctrl == NULL)
		return -ENOMEM;

	INIT_LIST_HEAD(&user_ctrl->entry);
	filp->private_data = user_ctrl;
	spin_lock(&pin_event_update_lock);
	list_add_tail(&user_ctrl->entry, &user_list);
	spin_unlock(&pin_event_update_lock);

	return 0;
}

static int ccci_util_pin_bc_release(struct inode *inode, struct file *filp)
{
	struct pin_event_user_ctrl *user_ctrl;

	spin_lock(&pin_event_update_lock);
	user_ctrl = filp->private_data;
	user_ctrl->pin_update = 0;

	list_del(&user_ctrl->entry);
	spin_unlock(&pin_event_update_lock);
	kfree(user_ctrl);

	return 0;
}

static ssize_t ccci_util_pin_bc_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	struct pin_event_user_ctrl *user_ctrl;
	int ret;

	user_ctrl = filp->private_data;
	if (filp->f_flags & O_NONBLOCK) {
		spin_lock(&pin_event_update_lock);
		if (user_ctrl->pin_update == 0) {
			spin_unlock(&pin_event_update_lock);
			return 0;
		}
		memcpy(&user_ctrl->pin_event, pin_event, sizeof(struct pin_status_event));
		spin_unlock(&pin_event_update_lock);
		if (copy_to_user(buf, &user_ctrl->pin_event, sizeof(struct pin_status_event)))
			return -EFAULT;
	} else {
		ret = wait_event_interruptible(pin_event_wait, user_ctrl->pin_update == 1);
		if (ret < 0)
			return -EINTR;
		spin_lock(&pin_event_update_lock);
		user_ctrl->pin_update = 0;
		memcpy(&user_ctrl->pin_event, pin_event, sizeof(struct pin_status_event));
		spin_unlock(&pin_event_update_lock);
		if (copy_to_user(buf, &user_ctrl->pin_event, sizeof(struct pin_status_event)))
			return -EFAULT;
	}

	return sizeof(struct pin_status_event);
}

static unsigned int ccci_util_pin_bc_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct pin_event_user_ctrl *user_ctrl;
	unsigned int mask = 0;

	user_ctrl = filp->private_data;
	poll_wait(filp, &pin_event_wait, wait);
	if (user_ctrl->pin_update == 1)
		mask |= POLLIN|POLLRDNORM;

	return mask;
}

static const struct file_operations pin_sta_bc_fops = {
	.owner = THIS_MODULE,
	.open = ccci_util_pin_bc_open,
	.read = ccci_util_pin_bc_read,
	.poll = ccci_util_pin_bc_poll,
	.release = ccci_util_pin_bc_release,
};

int ccci_util_pin_broadcast_init(void)
{
	int ret;

	pin_event = kzalloc(sizeof(struct pin_status_event), GFP_KERNEL);
	if (pin_event == NULL)
		goto _exit;

	spin_lock_init(&pin_event_update_lock);
	init_waitqueue_head(&pin_event_wait);
	INIT_LIST_HEAD(&user_list);

	ret = alloc_chrdev_region(&s_pin_status_dev, 0, 1, "ccci_pin_sta");
	if (ret != 0) {
		CCCI_UTIL_ERR_MSG("alloc chrdev fail for ccci_pin_sta(%d)\n", ret);
		goto _exit_1;
	}
	cdev_init(&s_pin_char_dev, &pin_sta_bc_fops);
	s_pin_char_dev.owner = THIS_MODULE;
	ret = cdev_add(&s_pin_char_dev, s_pin_status_dev, 1);
	if (ret) {
		CCCI_UTIL_ERR_MSG("cdev_add failed\n");
		goto _exit_2;
	}

	s_ccci_pin_class = class_create(THIS_MODULE, "ccci_pin_sta");
	device_create(s_ccci_pin_class, NULL, s_pin_status_dev, NULL, "ccci_pin_sta");

	return 0;

_exit_2:
	cdev_del(&s_pin_char_dev);

_exit_1:
	unregister_chrdev_region(s_pin_status_dev, 1);

_exit:
	kfree(pin_event);

	return -1;
}
