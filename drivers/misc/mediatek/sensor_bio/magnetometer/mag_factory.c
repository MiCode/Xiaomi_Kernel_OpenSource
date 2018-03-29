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

#include "inc/mag_factory.h"
static int mag_factory_open(struct inode *inode, struct file *file)
{
	file->private_data = mag_context_obj;

	if (file->private_data == NULL) {
		MAG_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int mag_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long mag_factory_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *data;
	long err = 0;
	struct mag_context *cxt = mag_context_obj;
	int x, y, z;
	char strbuf[256];

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		MAG_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case MSENSOR_IOCTL_SENSOR_ENABLE:
		if (cxt->mag_ctl.enable != NULL) {
			err = cxt->mag_ctl.enable(1);
			if (err < 0) {
				MAG_ERR("MSENSOR_IOCTL_SENSOR_ENABLE read data fail!\n");
				break;
			}
		}
	break;
	case MSENSOR_IOCTL_READ_SENSORDATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
		if (cxt->mag_dev_data.get_data != NULL) {
			err = cxt->mag_dev_data.get_raw_data(&x, &y, &z);
			if (err < 0) {
				MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA read data fail!\n");
				break;
			}
			sprintf(strbuf, "%x %x %x", x, y, z);
			MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA read data : (%d, %d, %d)!\n", x, y, z);
			MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA read strbuf : (%s)!\n", strbuf);

			if (copy_to_user(data, strbuf, strlen(strbuf)+1)) {
				err = -EFAULT;
				break;
			}
		} else
			MAG_ERR("MSENSOR_IOCTL_READ_SENSORDATA NULL!\n");

	break;

	case MSENSOR_IOCTL_READ_FACTORY_SENSORDATA:
		data = (void __user *) arg;
		if (data == NULL) {
			err = -EINVAL;
			break;
		}
	break;

	default:
	MAG_ERR("unknown IOCTL: 0x%08x\n", cmd);
	err = -ENOIOCTLCMD;
	break;
	}

	return err;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_mag_factory_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		MAG_ERR("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	/* case COMPAT_MSENSOR_IOCTL_INIT:
	case COMPAT_MSENSOR_IOCTL_SET_POSTURE:
	case COMPAT_MSENSOR_IOCTL_SET_CALIDATA:
	case COMPAT_MSENSOR_IOCTL_READ_CHIPINFO: */
	case COMPAT_MSENSOR_IOCTL_READ_SENSORDATA:
	/* case COMPAT_MSENSOR_IOCTL_READ_POSTUREDATA:
	case COMPAT_MSENSOR_IOCTL_READ_CALIDATA:
	case COMPAT_MSENSOR_IOCTL_READ_CONTROL:
	case COMPAT_MSENSOR_IOCTL_SET_CONTROL:
	case COMPAT_MSENSOR_IOCTL_SET_MODE: */
	case COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE:
	case COMPAT_MSENSOR_IOCTL_READ_FACTORY_SENSORDATA: {
		MAG_LOG("compat_ion_ioctl : MSENSOR_IOCTL_XXX command is 0x%x\n", cmd);
		return filp->f_op->unlocked_ioctl(filp, cmd,
			(unsigned long)compat_ptr(arg));
	}
	default: {
		MAG_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
	}
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations mag_factory_fops = {
	.open = mag_factory_open,
	.release = mag_factory_release,
	.unlocked_ioctl = mag_factory_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_mag_factory_unlocked_ioctl,
#endif
};

static struct miscdevice mag_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "msensor",
	.fops = &mag_factory_fops,
};

int mag_factory_device_init(void)
{
	int error = 0;
	struct mag_context *cxt = mag_context_obj;

	if (!cxt->mag_ctl.is_use_common_factory) {
		MAG_LOG("Node of '/dev/msensor' has already existed!\n");
		return -1;
	}
	error = misc_register(&mag_factory_device);
	if (error) {
		MAG_ERR("mag_factory_device register failed\n");
		error = -1;
	}
	return error;
}

