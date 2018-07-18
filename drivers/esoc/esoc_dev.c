/* Copyright (c) 2013-2014, 2017-2018, The Linux Foundation. All rights reserved.
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
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/esoc_client.h>
#include "esoc.h"

/**
 * struct esoc_udev: Userspace char interface
 * @dev: interface device.
 * @req_fifio: fifo for clink requests.
 * @req_wait: signal availability of request from clink
 * @req_fifo_lock: serialize access to req fifo
 * @evt_fito: fifo for clink events
 * @evt_wait: signal availability of clink event
 * @evt_fifo_lock: serialize access to event fifo
 * @list: entry in esoc dev list.
 * @clink: reference to contorl link
 */
struct esoc_udev {
	struct device *dev;
	struct kfifo req_fifo;
	wait_queue_head_t req_wait;
	spinlock_t req_fifo_lock;
	struct kfifo evt_fifo;
	wait_queue_head_t evt_wait;
	spinlock_t evt_fifo_lock;
	struct list_head list;
	struct esoc_clink *clink;
};

/**
 * struct esoc_uhandle: Userspace handle of esoc
 * @esoc_clink: esoc control link.
 * @eng: esoc engine for commands/ requests.
 * @esoc_udev: user interface device.
 * @req_eng_reg: indicates if engine is registered as request eng
 * @cmd_eng_reg: indicates if engine is registered as cmd eng
 */
struct esoc_uhandle {
	struct esoc_clink *esoc_clink;
	struct esoc_eng eng;
	struct esoc_udev *esoc_udev;
	bool req_eng_reg;
	bool cmd_eng_reg;
};

#define ESOC_MAX_MINOR	256
#define ESOC_MAX_REQ	8
#define ESOC_MAX_EVT	4

static LIST_HEAD(esoc_udev_list);
static DEFINE_SPINLOCK(esoc_udev_list_lock);
struct class *esoc_class;
static int esoc_major;

static struct esoc_udev *get_free_esoc_udev(struct esoc_clink *esoc_clink)
{
	struct esoc_udev *esoc_udev;
	int err;

	if (esoc_clink->id > ESOC_MAX_MINOR) {
		pr_err("too many esoc devices\n");
		return ERR_PTR(-ENODEV);
	}
	esoc_udev = kzalloc(sizeof(*esoc_udev), GFP_KERNEL);
	if (!esoc_udev)
		return ERR_PTR(-ENOMEM);
	err = kfifo_alloc(&esoc_udev->req_fifo, (sizeof(u32)) * ESOC_MAX_REQ,
								GFP_KERNEL);
	if (err) {
		pr_err("unable to allocate request fifo for %s\n",
							esoc_clink->name);
		goto req_fifo_fail;
	}
	err = kfifo_alloc(&esoc_udev->evt_fifo, (sizeof(u32)) * ESOC_MAX_EVT,
								GFP_KERNEL);
	if (err) {
		pr_err("unable to allocate evt fifo for %s\n",
							esoc_clink->name);
		goto evt_fifo_fail;
	}
	init_waitqueue_head(&esoc_udev->req_wait);
	init_waitqueue_head(&esoc_udev->evt_wait);
	spin_lock_init(&esoc_udev->req_fifo_lock);
	spin_lock_init(&esoc_udev->evt_fifo_lock);
	esoc_udev->clink = esoc_clink;
	spin_lock(&esoc_udev_list_lock);
	list_add_tail(&esoc_udev->list, &esoc_udev_list);
	spin_unlock(&esoc_udev_list_lock);
	return esoc_udev;
evt_fifo_fail:
	kfifo_free(&esoc_udev->req_fifo);
req_fifo_fail:
	kfree(esoc_udev);
	return ERR_PTR(-ENODEV);
}

static void return_esoc_udev(struct esoc_udev *esoc_udev)
{
	spin_lock(&esoc_udev_list_lock);
	list_del(&esoc_udev->list);
	spin_unlock(&esoc_udev_list_lock);
	kfifo_free(&esoc_udev->req_fifo);
	kfifo_free(&esoc_udev->evt_fifo);
	kfree(esoc_udev);
}

static struct esoc_udev *esoc_udev_get_by_minor(unsigned int index)
{
	struct esoc_udev *esoc_udev;

	spin_lock(&esoc_udev_list_lock);
	list_for_each_entry(esoc_udev, &esoc_udev_list, list) {
		if (esoc_udev->clink->id == index)
			goto found;
	}
	esoc_udev = NULL;
found:
	spin_unlock(&esoc_udev_list_lock);
	return esoc_udev;
}

void esoc_udev_handle_clink_req(enum esoc_req req, struct esoc_eng *eng)
{
	int err;
	u32 clink_req;
	struct esoc_clink *esoc_clink = eng->esoc_clink;
	struct esoc_udev *esoc_udev = esoc_udev_get_by_minor(esoc_clink->id);

	if (!esoc_udev) {
		esoc_mdm_log("esoc_udev not found\n");
		return;
	}
	clink_req = (u32)req;
	err = kfifo_in_spinlocked(&esoc_udev->req_fifo, &clink_req,
						sizeof(clink_req),
						&esoc_udev->req_fifo_lock);
	if (err != sizeof(clink_req)) {
		esoc_mdm_log("Unable to queue request %d; err: %d\n", req, err);
		pr_err("unable to queue request for %s\n", esoc_clink->name);
		return;
	}
	wake_up_interruptible(&esoc_udev->req_wait);
}

void esoc_udev_handle_clink_evt(enum esoc_evt evt, struct esoc_eng *eng)
{
	int err;
	u32 clink_evt;
	struct esoc_clink *esoc_clink = eng->esoc_clink;
	struct esoc_udev *esoc_udev = esoc_udev_get_by_minor(esoc_clink->id);

	if (!esoc_udev) {
		esoc_mdm_log("esoc_udev not found\n");
		return;
	}
	clink_evt = (u32)evt;
	err = kfifo_in_spinlocked(&esoc_udev->evt_fifo, &clink_evt,
						sizeof(clink_evt),
						&esoc_udev->evt_fifo_lock);
	if (err != sizeof(clink_evt)) {
		esoc_mdm_log("Unable to queue event %d; err: %d\n", evt, err);
		pr_err("unable to queue event for %s\n", esoc_clink->name);
		return;
	}
	wake_up_interruptible(&esoc_udev->evt_wait);
}

static int esoc_get_link_id(struct esoc_clink *esoc_clink,
						unsigned long arg)
{
	struct esoc_link_data link_data;
	struct esoc_client_hook *client_hook;
	struct esoc_link_data __user *user_arg;

	user_arg = (struct esoc_link_data __user *) arg;
	if (!user_arg) {
		dev_err(&esoc_clink->dev, "Missing argument for link id\n");
		return -EINVAL;
	}

	if (copy_from_user((void *) &link_data, user_arg, sizeof(*user_arg))) {
		dev_err(&esoc_clink->dev,
			"Unable to copy the data from the user\n");
		return -EFAULT;
	}

	if (link_data.prio < 0 || link_data.prio >= ESOC_MAX_HOOKS) {
		dev_err(&esoc_clink->dev, "Invalid client identifier passed\n");
		return -EINVAL;
	}

	client_hook = esoc_clink->client_hook[link_data.prio];
	if (client_hook && client_hook->esoc_link_get_id) {
		link_data.link_id =
		client_hook->esoc_link_get_id(client_hook->priv);

		if (copy_to_user((void *) user_arg, &link_data,
						sizeof(*user_arg))) {
			dev_err(&esoc_clink->dev,
				"Failed to send the data to the user\n");
			return -EFAULT;
		}

		return 0;
	}

	dev_err(&esoc_clink->dev,
			"Client hooks not registered for the device\n");
	return -EINVAL;
}

static long esoc_dev_ioctl(struct file *file, unsigned int cmd,
						unsigned long arg)
{
	int err;
	u32 esoc_cmd, status, req, evt;
	struct esoc_uhandle *uhandle = file->private_data;
	struct esoc_udev *esoc_udev = uhandle->esoc_udev;
	struct esoc_clink *esoc_clink = uhandle->esoc_clink;
	const struct esoc_clink_ops * const clink_ops = esoc_clink->clink_ops;
	void __user *uarg = (void __user *)arg;

	switch (cmd) {
	case ESOC_REG_REQ_ENG:
		esoc_mdm_log("ESOC_REG_REQ_ENG\n");
		err = esoc_clink_register_req_eng(esoc_clink, &uhandle->eng);
		if (err) {
			esoc_mdm_log("ESOC_REG_REQ_ENG failed: %d\n", err);
			return err;
		}
		uhandle->req_eng_reg = true;
		break;
	case ESOC_REG_CMD_ENG:
		esoc_mdm_log("ESOC_REG_CMD_ENG\n");
		err = esoc_clink_register_cmd_eng(esoc_clink, &uhandle->eng);
		if (err) {
			esoc_mdm_log("ESOC_REG_CMD_ENG failed: %d\n", err);
			return err;
		}
		uhandle->cmd_eng_reg = true;
		break;
	case ESOC_CMD_EXE:
		if (esoc_clink->cmd_eng != &uhandle->eng) {
			esoc_mdm_log("ESOC_CMD_EXE failed to access\n");
			return -EACCES;
		}
		get_user(esoc_cmd, (u32 __user *)arg);
		esoc_mdm_log("ESOC_CMD_EXE: Executing esoc command: %u\n",
				esoc_cmd);
		return clink_ops->cmd_exe(esoc_cmd, esoc_clink);
	case ESOC_WAIT_FOR_REQ:
		if (esoc_clink->req_eng != &uhandle->eng) {
			esoc_mdm_log("ESOC_WAIT_FOR_REQ: Failed to access\n");
			return -EACCES;
		}
		esoc_mdm_log(
		"ESOC_WAIT_FOR_REQ: Waiting for req event to arrive.\n");
		err = wait_event_interruptible(esoc_udev->req_wait,
					!kfifo_is_empty(&esoc_udev->req_fifo));
		if (!err) {
			err = kfifo_out_spinlocked(&esoc_udev->req_fifo, &req,
								sizeof(req),
						&esoc_udev->req_fifo_lock);
			if (err != sizeof(req)) {
				esoc_mdm_log(
				"ESOC_WAIT_FOR_REQ: Failed to read the event\n");
				pr_err("read from clink %s req q failed\n",
							esoc_clink->name);
				return -EIO;
			}
			put_user(req, (unsigned int __user *)uarg);
			esoc_mdm_log(
			"ESOC_WAIT_FOR_REQ: Event arrived: %u\n", req);

		}
		return err;
	case ESOC_NOTIFY:
		get_user(esoc_cmd, (u32 __user *)arg);
		esoc_mdm_log("ESOC_NOTIFY: Notifying esoc about cmd: %u\n",
				esoc_cmd);
		clink_ops->notify(esoc_cmd, esoc_clink);
		break;
	case ESOC_GET_STATUS:
		clink_ops->get_status(&status, esoc_clink);
		esoc_mdm_log(
		"ESOC_GET_STATUS: Sending the status from esoc: %u\n", status);
		put_user(status, (unsigned int __user *)uarg);
		break;
	case ESOC_GET_ERR_FATAL:
		clink_ops->get_err_fatal(&status, esoc_clink);
		esoc_mdm_log(
		"ESOC_GET_ERR_FATAL: Sending err_fatal status from esoc: %u\n",
		status);
		put_user(status, (unsigned int __user *)uarg);
		break;
	case ESOC_WAIT_FOR_CRASH:
		esoc_mdm_log(
		"ESOC_WAIT_FOR_CRASH: Waiting for evt to arrive..\n");
		err = wait_event_interruptible(esoc_udev->evt_wait,
					!kfifo_is_empty(&esoc_udev->evt_fifo));
		if (!err) {
			err = kfifo_out_spinlocked(&esoc_udev->evt_fifo, &evt,
								sizeof(evt),
						&esoc_udev->evt_fifo_lock);
			if (err != sizeof(evt)) {
				esoc_mdm_log(
				"ESOC_WAIT_FOR_CRASH: Failed to read event\n");
				pr_err("read from clink %s evt q failed\n",
							esoc_clink->name);
				return -EIO;
			}
			put_user(evt, (unsigned int __user *)uarg);
			esoc_mdm_log("ESOC_WAIT_FOR_CRASH: Event arrived: %u\n",
					req);
		}
		return err;
	case ESOC_GET_LINK_ID:
		return esoc_get_link_id(esoc_clink, arg);
	default:
		return -EINVAL;
	};
	return 0;
}

static int esoc_dev_open(struct inode *inode, struct file *file)
{
	struct esoc_uhandle *uhandle;
	struct esoc_udev *esoc_udev;
	struct esoc_clink *esoc_clink;
	struct esoc_eng *eng;
	unsigned int minor = iminor(inode);

	esoc_udev = esoc_udev_get_by_minor(minor);
	if (!esoc_udev) {
		esoc_mdm_log("failed to get udev\n");
		pr_err("failed to get udev\n");
		return -ENOMEM;
	}

	esoc_clink = get_esoc_clink(esoc_udev->clink->id);
	if (!esoc_clink) {
		esoc_mdm_log("failed to get clink\n");
		pr_err("failed to get clink\n");
		return -ENOMEM;
	}

	uhandle = kzalloc(sizeof(*uhandle), GFP_KERNEL);
	if (!uhandle) {
		put_esoc_clink(esoc_clink);
		return -ENOMEM;
	}
	uhandle->esoc_udev = esoc_udev;
	uhandle->esoc_clink = esoc_clink;
	eng = &uhandle->eng;
	eng->handle_clink_req = esoc_udev_handle_clink_req;
	eng->handle_clink_evt = esoc_udev_handle_clink_evt;
	file->private_data = uhandle;
	esoc_mdm_log(
		"%s successfully attached to esoc driver\n", current->comm);
	return 0;
}

static int esoc_dev_release(struct inode *inode, struct file *file)
{
	struct esoc_clink *esoc_clink;
	struct esoc_uhandle *uhandle = file->private_data;

	esoc_clink = uhandle->esoc_clink;
	if (uhandle->req_eng_reg) {
		esoc_mdm_log("Unregistering req_eng\n");
		esoc_clink_unregister_req_eng(esoc_clink, &uhandle->eng);
	} else {
		esoc_mdm_log("No req_eng to unregister\n");
	}
	if (uhandle->cmd_eng_reg) {
		esoc_mdm_log("Unregistering cmd_eng\n");
		esoc_clink_unregister_cmd_eng(esoc_clink, &uhandle->eng);
	} else {
		esoc_mdm_log("No cmd_eng to unregister\n");
	}
	uhandle->req_eng_reg = false;
	uhandle->cmd_eng_reg = false;
	put_esoc_clink(esoc_clink);
	kfree(uhandle);
	esoc_mdm_log("%s Unregistered with esoc\n", current->comm);
	return 0;
}
static const struct file_operations esoc_dev_fops = {
	.owner		= THIS_MODULE,
	.open		= esoc_dev_open,
	.unlocked_ioctl = esoc_dev_ioctl,
	.release	= esoc_dev_release,
};

int esoc_clink_add_device(struct device *dev, void *dummy)
{
	struct esoc_udev *esoc_udev;
	struct esoc_clink *esoc_clink = to_esoc_clink(dev);

	esoc_udev = get_free_esoc_udev(esoc_clink);
	if (IS_ERR_OR_NULL(esoc_udev))
		return PTR_ERR(esoc_udev);
	esoc_udev->dev = device_create(esoc_class, &esoc_clink->dev,
					MKDEV(esoc_major, esoc_clink->id),
					esoc_clink, "esoc-%d", esoc_clink->id);
	if (IS_ERR_OR_NULL(esoc_udev->dev)) {
		pr_err("failed to create user device\n");
		goto dev_err;
	}
	return 0;
dev_err:
	return_esoc_udev(esoc_udev);
	return -ENODEV;
}

int esoc_clink_del_device(struct device *dev, void *dummy)
{
	struct esoc_udev *esoc_udev;
	struct esoc_clink *esoc_clink = to_esoc_clink(dev);

	esoc_udev = esoc_udev_get_by_minor(esoc_clink->id);
	if (!esoc_udev)
		return 0;
	device_destroy(esoc_class, MKDEV(esoc_major, esoc_clink->id));
	return_esoc_udev(esoc_udev);
	return 0;
}

static int esoc_dev_notifier_call(struct notifier_block *nb,
					unsigned long action,
					void *data)
{
	struct device *dev = data;

	switch (action) {
	case BUS_NOTIFY_ADD_DEVICE:
		return esoc_clink_add_device(dev, NULL);
	case BUS_NOTIFY_DEL_DEVICE:
		return esoc_clink_del_device(dev, NULL);
	};
	return 0;
}

static struct notifier_block esoc_dev_notifier = {
	.notifier_call = esoc_dev_notifier_call,
};

int __init esoc_dev_init(void)
{
	int ret = 0;

	esoc_class = class_create(THIS_MODULE, "esoc-dev");
	if (IS_ERR_OR_NULL(esoc_class)) {
		pr_err("coudn't create class");
		return PTR_ERR(esoc_class);
	}
	esoc_major = register_chrdev(0, "esoc", &esoc_dev_fops);
	if (esoc_major < 0) {
		pr_err("failed to allocate char dev\n");
		ret = esoc_major;
		goto class_unreg;
	}
	ret = bus_register_notifier(&esoc_bus_type, &esoc_dev_notifier);
	if (ret)
		goto chrdev_unreg;
	esoc_for_each_dev(NULL, esoc_clink_add_device);
	return ret;
chrdev_unreg:
	unregister_chrdev(esoc_major, "esoc");
class_unreg:
	class_destroy(esoc_class);
	return 0;
}

void __exit esoc_dev_exit(void)
{
	bus_unregister_notifier(&esoc_bus_type, &esoc_dev_notifier);
	class_destroy(esoc_class);
	unregister_chrdev(esoc_major, "esoc-dev");
}

MODULE_LICENSE("GPL v2");
module_init(esoc_dev_init);
module_exit(esoc_dev_exit);
