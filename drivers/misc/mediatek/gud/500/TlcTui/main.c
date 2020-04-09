// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include "tlcTui.h"
#include "tui-hal.h"
#include "build_tag.h"

/* Static variables */
static struct cdev tui_cdev;

struct device* (dev_tlc_tui) = NULL;

/* Function pointer */
int (*fptr_get_fd)(u32 buff_id) = NULL;

static long tui_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOTTY;
	int __user *uarg = (int __user *)arg;

	if (_IOC_TYPE(cmd) != TUI_IO_MAGIC)
		return -EINVAL;

	tui_dev_devel("t-base-tui module: ioctl 0x%x", cmd);

	switch (cmd) {
	case TUI_IO_SET_RESOLUTION:
		/* TLC_TUI_CMD_SET_RESOLUTION is for specific platforms
		 * that rely on onConfigurationChanged to set resolution
		 * it has no effect on Trustonic reference implementation.
		 */
		tui_dev_devel("TLC_TUI_CMD_SET_RESOLUTION");
		/* NOT IMPLEMENTED */
		ret = 0;
		break;

	case TUI_IO_NOTIFY:
		tui_dev_devel("TUI_IO_NOTIFY");

		if (tlc_notify_event(arg))
			ret = 0;
		else
			ret = -EFAULT;
		break;

	case TUI_IO_WAITCMD: {
		struct tlc_tui_command_t tui_cmd = {0};

		tui_dev_devel("TUI_IO_WAITCMD");

		ret = tlc_wait_cmd(&tui_cmd);
		if (ret) {
			tui_dev_err(ret, "%d tlc_wait_cmd returned", __LINE__);
			return ret;
		}

		/* Write command id to user */
		tui_dev_devel("IOCTL: sending command %d to user.", tui_cmd.id);

		if (copy_to_user(uarg, &tui_cmd, sizeof(
						struct tlc_tui_command_t)))
			ret = -EFAULT;
		else
			ret = 0;

		break;
	}

	case TUI_IO_ACK: {
		struct tlc_tui_response_t rsp_id;

		tui_dev_devel("TUI_IO_ACK");

		/* Read user response */
		if (copy_from_user(&rsp_id, uarg, sizeof(rsp_id)))
			ret = -EFAULT;
		else
			ret = 0;

		tui_dev_devel("IOCTL: User completed command %d.", rsp_id.id);
		ret = tlc_ack_cmd(&rsp_id);
		if (ret)
			return ret;
		break;
	}

	case TUI_IO_INIT_DRIVER: {
		tui_dev_devel("TUI_IO_INIT_DRIVER");

		ret = tlc_init_driver();
		if (ret) {
			tui_dev_err(ret, "%d tlc_init_driver() failed",
				    __LINE__);
			return ret;
		}
		break;
	}

	/* Ioclt that allows to get buffer info from DrTui
	 */
	case TUI_IO_GET_BUFFER_INFO: {
		tui_dev_devel("TUI_IO_GET_BUFFER_INFO");

		/* Get all buffer info received from DrTui through the dci */
		struct tlc_tui_ioctl_buffer_info buff_info;

		get_buffer_info(&buff_info);

		/* Fill in return struct */
		if (copy_to_user(uarg, &buff_info, sizeof(buff_info)))
			ret = -EFAULT;
		else
			ret = 0;

		break;
	}

	/* Ioclt that allows to get the ion buffer f
	 */
	case TUI_IO_GET_ION_FD: {
		tui_dev_devel("TUI_IO_GET_ION_FD");

		/* Get the back buffer id (in the dci, from DrTui) */
		u32 buff_id = dci->buff_id;

		/* Get the fd of the ion buffer */
		struct tlc_tui_ioctl_ion_t ion;

		if (fptr_get_fd)
			ion.buffer_fd = (*fptr_get_fd)(buff_id);

		/* Fill in return struct */
		if (copy_to_user(uarg, &ion, sizeof(ion))) {
			ret = -EFAULT;
			break;
		}
		ret = 0;
		break;
	}

	default:
		ret = -ENOTTY;
		tui_dev_err(ret, "%d Unknown ioctl (%u)!", __LINE__, cmd);
		return ret;
	}

	return ret;
}

atomic_t fileopened;

static int tui_open(struct inode *inode, struct file *file)
{
	tui_dev_devel("TUI file opened");
	atomic_inc(&fileopened);
	return 0;
}

static int tui_release(struct inode *inode, struct file *file)
{
	tui_dev_devel("TUI file closed");
	if (atomic_dec_and_test(&fileopened))
		tlc_notify_event(NOT_TUI_CANCEL_EVENT);

	return 0;
}

static const struct file_operations tui_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tui_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tui_ioctl,
#endif
	.open = tui_open,
	.release = tui_release,
};

/*--------------------------------------------------------------------------- */

static struct device_driver tui_driver = {
	.name = "Trustonic"
};

struct device tui_dev = {
	.driver = &tui_driver
};

static int __init tlc_tui_init(void)
{
	dev_set_name(&tui_dev, "TUI");

	tui_dev_info("Loading t-base-tui module.");
	tui_dev_devel("=============== Running TUI Kernel TLC ===============");
	tui_dev_info("%s", MOBICORE_COMPONENT_BUILD_TAG);

	dev_t devno;
	int err;
	static struct class *tui_class;

	atomic_set(&fileopened, 0);

	err = alloc_chrdev_region(&devno, 0, 1, TUI_DEV_NAME);
	if (err) {
		tui_dev_err(err, "Unable to allocate Trusted UI device number");
		return err;
	}

	cdev_init(&tui_cdev, &tui_fops);
	tui_cdev.owner = THIS_MODULE;
	/*    tui_cdev.ops = &tui_fops; */

	err = cdev_add(&tui_cdev, devno, 1);
	if (err) {
		tui_dev_err(err, "Unable to add Trusted UI char device");
		unregister_chrdev_region(devno, 1);
		return err;
	}

	tui_class = class_create(THIS_MODULE, "tui_cls");

	dev_tlc_tui = device_create(tui_class, NULL, devno, NULL, TUI_DEV_NAME);

	if (IS_ERR(dev_tlc_tui))
		return PTR_ERR(dev_tlc_tui);

	if (!hal_tui_init())
		return -EPERM;

	return 0;
}

static void __exit tlc_tui_exit(void)
{
	tui_dev_devel("Unloading t-base-tui module.");

	unregister_chrdev_region(tui_cdev.dev, 1);
	cdev_del(&tui_cdev);

	hal_tui_exit();
}

module_init(tlc_tui_init);
module_exit(tlc_tui_exit);

MODULE_AUTHOR("Trustonic Limited");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Kinibi TUI");
