/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "inc/humidity_factory.h"

static int hmdy_factory_open(struct inode *inode, struct file *file)
{
	file->private_data = hmdy_context_obj;

	if (file->private_data == NULL) {
		HMDY_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int hmdy_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long hmdy_factory_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;
	struct hmdy_context *cxt = hmdy_context_obj;
	void __user *ptr = (void __user *)arg;
	int dat = 0;


	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		HMDY_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case HUMIDITY_IOCTL_INIT:
		HMDY_LOG("HUMIDITY_IOCTL_INIT enable\n");
		if (cxt->hmdy_ctl.enable_nodata != NULL) {
			err = cxt->hmdy_ctl.enable_nodata(1);
			if (err < 0) {
				HMDY_ERR("HUMIDITY_IOCTL_INIT fail!\n");
				break;
			}
		}
		break;
	case HUMIDITY_GET_HMDY_DATA:
		if (cxt->hmdy_data.get_raw_data != NULL) {
			err = cxt->hmdy_data.get_raw_data(TYPE_PRESS, &dat);
			if (err < 0) {
				HMDY_ERR("HUMIDITY_GET_PRESS_DATA fail!\n");
				break;
			}
		}

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
		break;
	case HUMIDITY_GET_TEMP_DATA:
		if (cxt->hmdy_data.get_raw_data != NULL) {
			err = cxt->hmdy_data.get_raw_data(TYPE_TEMP, &dat);
			if (err < 0) {
				HMDY_ERR("HUMIDITY_GET_PRESS_DATA fail!\n");
				break;
			}
		}

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		HMDY_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}
	return err;
}

static const struct file_operations hmdy_factory_fops = {
	.open = hmdy_factory_open,
	.release = hmdy_factory_release,
	.unlocked_ioctl = hmdy_factory_unlocked_ioctl,
};

static struct miscdevice hmdy_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hmdymeter",
	.fops = &hmdy_factory_fops,
};

int hmdy_factory_device_init(void)
{
	int error = 0;
	struct hmdy_context *cxt = hmdy_context_obj;

	if (!cxt->hmdy_ctl.is_use_common_factory) {
		HMDY_LOG("Node of '/dev/hmdymeter' has already existed!\n");
		return -1;
	}
	error = misc_register(&hmdy_factory_device);
	if (error) {
		HMDY_ERR("hmdy_factory_device register failed\n");
		error = -1;
	}
	return error;
}
