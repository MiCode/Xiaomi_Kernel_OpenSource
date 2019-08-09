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

#define pr_fmt(fmt) "<SAR_FAC>" fmt

#include "sar_factory.h"

struct sar_factory_private {
	uint32_t gain;
	uint32_t sensitivity;
	struct sar_factory_fops *fops;
};

static struct sar_factory_private sar_factory;

static int sar_factory_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int sar_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long sar_factory_unlocked_ioctl(struct file *file, unsigned int cmd,
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
	case SAR_IOCTL_INIT:
		if (copy_from_user(&flag, ptr, sizeof(flag)))
			return -EFAULT;
		if (sar_factory.fops != NULL &&
		    sar_factory.fops->enable_sensor != NULL) {
			err = sar_factory.fops->enable_sensor(flag, 200);
			if (err < 0) {
				pr_err("SAR_IOCTL_INIT fail!\n");
				return -EINVAL;
			}
			pr_debug(
				"SAR_IOCTL_INIT, enable: %d, sample_period:%dms\n",
				flag, 200);
		} else {
			pr_err("SAR_IOCTL_INIT NULL\n");
			return -EINVAL;
		}
		return 0;
	case SAR_IOCTL_READ_SENSORDATA:
		if (sar_factory.fops != NULL &&
		    sar_factory.fops->get_data != NULL) {
			err = sar_factory.fops->get_data(data_buf);
			if (err < 0) {
				pr_err(
					"SAR_IOCTL_READ_SENSORDATA read data fail!\n");
				return -EINVAL;
			}
			pr_debug("SAR_IOCTL_READ_SENSORDATA: (%d, %d, %d)!\n",
				data_buf[0], data_buf[1], data_buf[2]);
			sensor_data.x = data_buf[0];
			sensor_data.y = data_buf[1];
			sensor_data.z = data_buf[2];
			if (copy_to_user(ptr, &sensor_data,
							sizeof(sensor_data)))
				return -EFAULT;
		} else {
			pr_err("SAR_IOCTL_READ_SENSORDATA NULL\n");
			return -EINVAL;
		}
		return 0;
	case SAR_IOCTL_ENABLE_CALI:
		if (sar_factory.fops != NULL &&
		    sar_factory.fops->enable_calibration != NULL) {
			err = sar_factory.fops->enable_calibration();
			if (err < 0) {
				pr_err(
					"SAR_IOCTL_ENABLE_CALI fail!\n");
				return -EINVAL;
			}
		} else {
			pr_err("SAR_IOCTL_ENABLE_CALI NULL\n");
			return -EINVAL;
		}
		return 0;
	case SAR_IOCTL_GET_CALI:
		if (sar_factory.fops != NULL &&
		    sar_factory.fops->get_cali != NULL) {
			err = sar_factory.fops->get_cali(data_buf);
			if (err < 0) {
				pr_err("SAR_IOCTL_GET_CALI FAIL!\n");
				return -EINVAL;
			}
		} else {
			pr_err("SAR_IOCTL_GET_CALI NULL\n");
			return -EINVAL;
		}

		pr_debug("SAR_IOCTL_GET_CALI: (%d, %d, %d)!\n",
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
static long compat_sar_factory_unlocked_ioctl(struct file *filp,
					       unsigned int cmd,
					       unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		pr_err(
			"compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_SAR_IOCTL_INIT:
	case COMPAT_SAR_IOCTL_READ_SENSORDATA:
	case COMPAT_SAR_IOCTL_ENABLE_CALI:
	case COMPAT_SAR_IOCTL_GET_CALI: {
		pr_debug(
			"compat_ion_ioctl : SAR_IOCTL_XXX command is 0x%x\n",
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
static const struct file_operations _sar_factory_fops = {
	.open = sar_factory_open,
	.release = sar_factory_release,
	.unlocked_ioctl = sar_factory_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_sar_factory_unlocked_ioctl,
#endif
};

static struct miscdevice _sar_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "sar",
	.fops = &_sar_factory_fops,
};

int sar_factory_device_register(struct sar_factory_public *dev)
{
	int err = 0;

	if (!dev || !dev->fops)
		return -1;
	sar_factory.gain = dev->gain;
	sar_factory.sensitivity = dev->sensitivity;
	sar_factory.fops = dev->fops;
	err = misc_register(&_sar_factory_device);
	if (err) {
		pr_err("sar_factory_device register failed\n");
		err = -1;
	}
	return err;
}

int sar_factory_device_deregister(struct sar_factory_public *dev)
{
	sar_factory.fops = NULL;
	misc_deregister(&_sar_factory_device);
	return 0;
}
