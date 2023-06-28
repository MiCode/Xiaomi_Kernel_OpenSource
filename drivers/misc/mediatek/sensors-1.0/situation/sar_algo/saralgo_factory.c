// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<SARALGO_FAC>" fmt

#include "saralgo_factory.h"

struct saralgo_factory_private {
	uint32_t gain;
	uint32_t sensitivity;
	struct saralgo_factory_fops *fops;
};

static struct saralgo_factory_private saralgo_factory;

static int saralgo_factory_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int saralgo_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long saralgo_factory_unlocked_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	long err = 0;
	void __user *ptr = (void __user *)arg;
	int32_t data_buf[3] = {0};
	//struct SENSOR_DATA sensor_data = {0};
	int32_t step_status;
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
	case SARALGO_IOCTL_INIT:
		if (copy_from_user(&flag, ptr, sizeof(flag)))
			return -EFAULT;
		if (saralgo_factory.fops != NULL &&
		    saralgo_factory.fops->enable_sensor != NULL) {
			err = saralgo_factory.fops->enable_sensor(flag, 200);
			if (err < 0) {
				pr_err("SARALGO_IOCTL_INIT fail!\n");
				return -EINVAL;
			}
			pr_debug(
				"SARALGO_IOCTL_INIT, enable: %d, sample_period:%dms\n",
				flag, 200);
		} else {
			pr_err("SARALGO_IOCTL_INIT NULL\n");
			return -EINVAL;
		}
		return 0;
	case SARALGO_TOP_IOCTL_INIT:
		if (copy_from_user(&flag, ptr, sizeof(flag)))
			return -EFAULT;
		if (saralgo_factory.fops != NULL &&
		    saralgo_factory.fops->enable_top_sensor != NULL) {
			err = saralgo_factory.fops->enable_top_sensor(flag, 200);
			if (err < 0) {
				pr_err("SARALGO_TOP_IOCTL_INIT fail!\n");
				return -EINVAL;
			}
			pr_debug(
				"SARALGO_TOP_IOCTL_INIT, enable: %d, sample_period:%dms\n",
				flag, 200);
		} else {
			pr_err("SARALGO_TOP_IOCTL_INIT NULL\n");
			return -EINVAL;
		}
		return 0;
	case SARALGO_IOCTL_READ_SENSORDATA:
		if (saralgo_factory.fops != NULL &&
		    saralgo_factory.fops->get_data != NULL) {
			err = saralgo_factory.fops->get_data(data_buf);
			if (err < 0) {
				pr_err(
					"SAR_IOCTL_READ_SENSORDATA read data fail!\n");
				return -EINVAL;
			}
			pr_err("SAR_IOCTL_READ_SENSORDATA: (%d, %d, %d)!\n",
				data_buf[0], data_buf[1], data_buf[2]);
			if (copy_to_user(ptr, &data_buf,
							sizeof(data_buf)))
				return -EFAULT;
		} else {
			pr_err("SAR_IOCTL_READ_SENSORDATA NULL\n");
			return -EINVAL;
		}
		return 0;
	case SARALGO_IOCTL_STEP:
		if (copy_from_user(&step_status, ptr, sizeof(step_status)))
			return -EFAULT;
		if (saralgo_factory.fops != NULL &&
		    saralgo_factory.fops->step_set_cfg != NULL) {
			err = saralgo_factory.fops->step_set_cfg(&step_status);
			if (err < 0) {
				pr_err(
					"SAR_IOCTL_READ_SENSORDATA read data fail!\n");
				return -EINVAL;
			}
			}else {
			pr_err("SAR_IOCTL_READ_SENSORDATA NULL\n");
			return -EINVAL;
			}
			return 0;
	default:
		pr_err("unknown IOCTL: 0x%08x\n", cmd);
		return -ENOIOCTLCMD;
	}
	return 0;
	

}
/*----------------------------------------------------------------------------*/
static const struct file_operations _saralgo_factory_fops = {
	.open = saralgo_factory_open,
	.release = saralgo_factory_release,
	.unlocked_ioctl = saralgo_factory_unlocked_ioctl,
};

static struct miscdevice _saralgo_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "saralgo",
	.fops = &_saralgo_factory_fops,
};

int saralgo_factory_device_register(struct saralgo_factory_public *dev)
{
	int err = 0;

	if (!dev || !dev->fops)
		return -1;
	saralgo_factory.gain = dev->gain;
	saralgo_factory.sensitivity = dev->sensitivity;
	saralgo_factory.fops = dev->fops;
	err = misc_register(&_saralgo_factory_device);
	if (err) {
		pr_err("sar_factory_device register failed\n");
		err = -1;
	}
	return err;
}

int saralgo_factory_device_deregister(struct saralgo_factory_public *dev)
{
	saralgo_factory.fops = NULL;
	misc_deregister(&_saralgo_factory_device);
	return 0;
}
