/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#define pr_fmt(fmt) "<LIGHTSECONDARY_FAC>" fmt

#include "lightsecondary_factory.h"

struct lightsecondary_factory_private {
	uint32_t gain;
	uint32_t sensitivity;
	struct lightsecondary_factory_fops *fops;
};

static struct lightsecondary_factory_private lightsecondary_factory;

static int lightsecondary_factory_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int lightsecondary_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long lightsecondary_factory_unlocked_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	long err = 0;
	void __user *ptr = (void __user *)arg;
	int32_t data_buf[3] = {0};
	struct SENSOR_DATA sensor_data = {0};
	uint32_t flag = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
				 _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				 _IOC_SIZE(cmd));

	if (err) {
		pr_err("access error: %08X, (%2d, %2d)\n", cmd,
			    _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case LIGHTSECONDARY_IOCTL_INIT:
		if (copy_from_user(&flag, ptr, sizeof(flag)))
			return -EFAULT;
		if (lightsecondary_factory.fops != NULL &&
		    lightsecondary_factory.fops->enable_sensor != NULL) {
			err = lightsecondary_factory.fops->enable_sensor(flag, 200);
			if (err < 0) {
				pr_err("LIGHTSECONDARY_IOCTL_INIT fail!\n");
				return -EINVAL;
			}
			pr_debug(
				"LIGHTSECONDARY_IOCTL_INIT, enable: %d, sample_period:%dms\n",
				flag, 200);
		} else {
			pr_err("LIGHTSECONDARY_IOCTL_INIT NULL\n");
			return -EINVAL;
		}
		return 0;
	case LIGHTSECONDARY_IOCTL_READ_SENSORDATA:
		if (lightsecondary_factory.fops != NULL &&
		    lightsecondary_factory.fops->get_data != NULL) {
			err = lightsecondary_factory.fops->get_data(data_buf);
			if (err < 0) {
				pr_err(
					"LIGHTSECONDARY_IOCTL_READ_SENSORDATA read data fail!\n");
				return -EINVAL;
			}
			pr_debug("LIGHTSECONDARY_IOCTL_READ_SENSORDATA: (%d, %d, %d)!\n",
				data_buf[0], data_buf[1], data_buf[2]);
			sensor_data.x = data_buf[0];
			sensor_data.y = data_buf[1];
			sensor_data.z = data_buf[2];
			if (copy_to_user(ptr, &sensor_data,
							sizeof(sensor_data)))
				return -EFAULT;
		} else {
			pr_err("LIGHTSECONDARY_IOCTL_READ_SENSORDATA NULL\n");
			return -EINVAL;
		}
		return 0;
	case LIGHTSECONDARY_IOCTL_ENABLE_CALI:
		if (lightsecondary_factory.fops != NULL &&
		    lightsecondary_factory.fops->enable_calibration != NULL) {
			err = lightsecondary_factory.fops->enable_calibration();
			if (err < 0) {
				pr_err(
					"LIGHTSECONDARY_IOCTL_ENABLE_CALI fail!\n");
				return -EINVAL;
			}
		} else {
			pr_err("LIGHTSECONDARY_IOCTL_ENABLE_CALI NULL\n");
			return -EINVAL;
		}
		return 0;
	case LIGHTSECONDARY_IOCTL_GET_CALI:
		if (lightsecondary_factory.fops != NULL &&
		    lightsecondary_factory.fops->get_cali != NULL) {
			err = lightsecondary_factory.fops->get_cali(data_buf);
			if (err < 0) {
				pr_err("LIGHTSECONDARY_IOCTL_GET_CALI FAIL!\n");
				return -EINVAL;
			}
		} else {
			pr_err("LIGHTSECONDARY_IOCTL_GET_CALI NULL\n");
			return -EINVAL;
		}

		pr_debug("LIGHTSECONDARY_IOCTL_GET_CALI: (%d, %d, %d)!\n",
			data_buf[0], data_buf[1], data_buf[2]);
		sensor_data.x = data_buf[0];
		sensor_data.y = data_buf[1];
		sensor_data.z = data_buf[2];
		if (copy_to_user(ptr, &sensor_data, sizeof(sensor_data)))
			return -EFAULT;
		return 0;
	default:
		pr_err("unknown IOCTL: 0x%08x\n", cmd);
		return -ENOIOCTLCMD;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_lightsecondary_factory_unlocked_ioctl(struct file *filp,
					       unsigned int cmd,
					       unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		pr_err(
			"compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_LIGHTSECONDARY_IOCTL_INIT:
	case COMPAT_LIGHTSECONDARY_IOCTL_READ_SENSORDATA:
	case COMPAT_LIGHTSECONDARY_IOCTL_ENABLE_CALI:
	case COMPAT_LIGHTSECONDARY_IOCTL_GET_CALI: {
		pr_debug(
			"compat_ion_ioctl : LIGHTSECONDARY_IOCTL_XXX command is 0x%x\n",
			cmd);
		return filp->f_op->unlocked_ioctl(
			filp, cmd, (unsigned long)compat_ptr(arg));
	}
	default:
		pr_err("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations _lightsecondary_factory_fops = {
	.open = lightsecondary_factory_open,
	.release = lightsecondary_factory_release,
	.unlocked_ioctl = lightsecondary_factory_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_lightsecondary_factory_unlocked_ioctl,
#endif
};

static struct miscdevice _lightsecondary_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lightsecondary",
	.fops = &_lightsecondary_factory_fops,
};

int lightsecondary_factory_device_register(struct lightsecondary_factory_public *dev)
{
	int err = 0;

	if (!dev || !dev->fops)
		return -1;
	lightsecondary_factory.gain = dev->gain;
	lightsecondary_factory.sensitivity = dev->sensitivity;
	lightsecondary_factory.fops = dev->fops;
	err = misc_register(&_lightsecondary_factory_device);
	if (err) {
		pr_err("lightsecondary_factory_device register failed\n");
		err = -1;
	}
	return err;
}

int lightsecondary_factory_device_deregister(struct lightsecondary_factory_public *dev)
{
	lightsecondary_factory.fops = NULL;
	misc_deregister(&_lightsecondary_factory_device);
	return 0;
}
