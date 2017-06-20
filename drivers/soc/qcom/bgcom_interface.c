/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define pr_fmt(msg) "bgcom_dev:" msg

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "bgcom.h"
#include "linux/bgcom_interface.h"

#define BGCOM "bg_com_dev"

static  DEFINE_MUTEX(bg_char_mutex);
static  struct cdev              bg_cdev;
static  struct class             *bg_class;
struct  device                   *dev_ret;
static  dev_t                    bg_dev;
static  int                      device_open;
static  void                     *handle;
static  struct   bgcom_open_config_type   config_type;

static int bgcom_char_open(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&bg_char_mutex);
	if (device_open == 1) {
		pr_err("device is already open\n");
		mutex_unlock(&bg_char_mutex);
		return -EBUSY;
	}
	device_open++;
	handle = bgcom_open(&config_type);
	mutex_unlock(&bg_char_mutex);
	if (IS_ERR(handle)) {
		device_open = 0;
		ret = PTR_ERR(handle);
		handle = NULL;
		return ret;
	}
	return 0;
}

static int bgchar_read_cmd(struct bg_ui_data *fui_obj_msg,
		int type)
{
	void              *read_buf;
	int               ret;
	void __user       *result   = (void *)
			(uintptr_t)fui_obj_msg->result;

	read_buf = kmalloc_array(fui_obj_msg->num_of_words, sizeof(uint32_t),
			GFP_KERNEL);
	if (read_buf == NULL)
		return -ENOMEM;
	switch (type) {
	case REG_READ:
		ret = bgcom_reg_read(handle, fui_obj_msg->cmd,
				fui_obj_msg->num_of_words,
				read_buf);
		break;
	case AHB_READ:
		ret = bgcom_ahb_read(handle,
				fui_obj_msg->bg_address,
				fui_obj_msg->num_of_words,
				read_buf);
		break;
	}
	if (!ret && copy_to_user(result, read_buf,
			fui_obj_msg->num_of_words * sizeof(uint32_t))) {
		pr_err("copy to user failed\n");
		ret = -EFAULT;
	}
	kfree(read_buf);
	return ret;
}

static int bgchar_write_cmd(struct bg_ui_data *fui_obj_msg)
{
	void              *write_buf;
	int               ret;
	void __user       *write     = (void *)
			(uintptr_t)fui_obj_msg->write;

	write_buf = kmalloc_array(fui_obj_msg->num_of_words, sizeof(uint32_t),
			GFP_KERNEL);
	if (write_buf == NULL)
		return -ENOMEM;
	write_buf = memdup_user(write,
			fui_obj_msg->num_of_words * sizeof(uint32_t));
	if (IS_ERR(write_buf)) {
		ret = PTR_ERR(write_buf);
		kfree(write_buf);
		return ret;
	}
	ret = bgcom_ahb_write(handle,
			fui_obj_msg->bg_address,
			fui_obj_msg->num_of_words,
			write_buf);
	kfree(write_buf);
	return ret;
}

static long bg_com_ioctl(struct file *filp,
		unsigned int ui_bgcom_cmd, unsigned long arg)
{
	int ret;
	struct bg_ui_data ui_obj_msg;

	switch (ui_bgcom_cmd) {
	case REG_READ:
	case AHB_READ:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = bgchar_read_cmd(&ui_obj_msg,
				ui_bgcom_cmd);
		if (ret < 0)
			pr_err("bgchar_read_cmd failed\n");
		break;
	case AHB_WRITE:
		if (copy_from_user(&ui_obj_msg, (void __user *) arg,
				sizeof(ui_obj_msg))) {
			pr_err("The copy from user failed\n");
			ret = -EFAULT;
		}
		ret = bgchar_write_cmd(&ui_obj_msg);
		if (ret < 0)
			pr_err("bgchar_write_cmd failed\n");
		break;
	default:
		ret = -ENOIOCTLCMD;
	}
	return ret;
}

static int bgcom_char_close(struct inode *inode, struct file *file)
{
	int ret;

	mutex_lock(&bg_char_mutex);
	ret = bgcom_close(&handle);
	device_open = 0;
	mutex_unlock(&bg_char_mutex);
	return ret;
}

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.open           = bgcom_char_open,
	.release        = bgcom_char_close,
	.unlocked_ioctl = bg_com_ioctl,
};

static int __init init_bg_com_dev(void)
{
	int ret;

	ret = alloc_chrdev_region(&bg_dev, 0, 1, BGCOM);
	if (ret  < 0) {
		pr_err("failed with error %d\n", ret);
		return ret;
	}
	cdev_init(&bg_cdev, &fops);
	ret = cdev_add(&bg_cdev, bg_dev, 1);
	if (ret < 0) {
		unregister_chrdev_region(bg_dev, 1);
		pr_err("device registration failed\n");
		return ret;
	}
	bg_class = class_create(THIS_MODULE, BGCOM);
	if (IS_ERR_OR_NULL(bg_class)) {
		cdev_del(&bg_cdev);
		unregister_chrdev_region(bg_dev, 1);
		pr_err("class creation failed\n");
		return PTR_ERR(bg_class);
	}

	dev_ret = device_create(bg_class, NULL, bg_dev, NULL, BGCOM);
	if (IS_ERR_OR_NULL(dev_ret)) {
		class_destroy(bg_class);
		cdev_del(&bg_cdev);
		unregister_chrdev_region(bg_dev, 1);
		pr_err("device create failed\n");
		return PTR_ERR(dev_ret);
	}
	return 0;
}

static void __exit exit_bg_com_dev(void)
{
	device_destroy(bg_class, bg_dev);
	class_destroy(bg_class);
	cdev_del(&bg_cdev);
	unregister_chrdev_region(bg_dev, 1);
}

module_init(init_bg_com_dev);
module_exit(exit_bg_com_dev);
MODULE_LICENSE("GPL v2");
