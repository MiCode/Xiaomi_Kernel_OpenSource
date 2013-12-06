/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/rmnet_ipa_fd_ioctl.h>
#include <ipa_qmi_service.h>

#define DRIVER_NAME "wwan_ioctl"
static unsigned int dev_num = 1;
static struct cdev wan_ioctl_cdev;

static long wan_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	u32 pyld_sz;
	u8 *param = NULL;

	IPAWANDBG("device %s got ioctl events :>>>\n",
		DRIVER_NAME);
	switch (cmd) {
	case WAN_IOC_ADD_FLT_RULE:
		IPAWANDBG("device %s got WAN_IOC_ADD_FLT_RULE :>>>\n",
		DRIVER_NAME);
		pyld_sz = sizeof(struct ipa_install_fltr_rule_req_msg_v01);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (qmi_filter_request_send(
			(struct ipa_install_fltr_rule_req_msg_v01 *)param)) {
			IPAWANDBG("IPACM->Q6 add filter rule failed\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;

	case WAN_IOC_ADD_FLT_RULE_INDEX:
	    IPAWANDBG("device %s got WAN_IOC_ADD_FLT_RULE_INDEX :>>>\n",
		DRIVER_NAME);
		pyld_sz = sizeof(struct ipa_fltr_installed_notif_req_msg_v01);
		param = kzalloc(pyld_sz, GFP_KERNEL);
		if (!param) {
			retval = -ENOMEM;
			break;
		}
		if (copy_from_user(param, (u8 *)arg, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		if (qmi_filter_notify_send(
		(struct ipa_fltr_installed_notif_req_msg_v01 *)param)) {
			IPAWANDBG("IPACM->Q6 rule index fail\n");
			retval = -EFAULT;
			break;
		}
		if (copy_to_user((u8 *)arg, param, pyld_sz)) {
			retval = -EFAULT;
			break;
		}
		break;
	default:
		retval = -ENOTTY;
	}
	kfree(param);
	return retval;
}

static int wan_ioctl_open(struct inode *inode, struct file *filp)
{
	IPAWANDBG("\n IPA A7 wan_ioctl open OK :>>>> ");
	return 0;
}

const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = wan_ioctl_open,
	.read = NULL,
	.unlocked_ioctl = wan_ioctl,
};

int wan_ioctl_init(void)
{
	unsigned int wan_ioctl_major = 0;
	dev_t device = MKDEV(wan_ioctl_major, 0);
	int alloc_ret = 0;
	int cdev_ret = 0;
	struct class *class;
	struct device *dev;

	alloc_ret = alloc_chrdev_region(&device, 0, dev_num, DRIVER_NAME);
	if (alloc_ret) {
		IPAWANERR(":device_alloc err.\n");
		goto error;
	}
	wan_ioctl_major = MAJOR(device);

	class = class_create(THIS_MODULE, DRIVER_NAME);
	dev = device_create(class, NULL, device,
		NULL, DRIVER_NAME);
	if (IS_ERR(dev)) {
		IPAWANERR(":device_create err.\n");
		goto error;
	}

	cdev_init(&wan_ioctl_cdev, &fops);
	cdev_ret = cdev_add(&wan_ioctl_cdev, device, dev_num);
	if (cdev_ret) {
		IPAWANERR(":cdev_add err.\n");
		goto error;
	}

	IPAWANDBG("IPA %s major(%d) initial ok :>>>>\n",
	DRIVER_NAME, wan_ioctl_major);
	return 0;
error:
	if (cdev_ret == 0)
		cdev_del(&wan_ioctl_cdev);
	if (alloc_ret == 0)
		unregister_chrdev_region(device, dev_num);
	return -ENODEV;
}
