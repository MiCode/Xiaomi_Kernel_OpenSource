// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<MAG_FAC> " fmt

#include "inc/mag_factory.h"

struct mag_factory_private {
	uint32_t gain;
	uint32_t sensitivity;
	struct mag_factory_fops *fops;
};

static struct mag_factory_private mag_factory;

static int mag_factory_open(struct inode *inode, struct file *file)
{
	file->private_data = mag_context_obj;

	if (file->private_data == NULL) {
		pr_err("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int mag_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long mag_factory_unlocked_ioctl(struct file *file, unsigned int cmd,
				       unsigned long arg)
{
	void __user *ptr = (void __user *)arg;
	int err = 0, status = 0;
	uint32_t flag = 0;
	char strbuf[64];
	int32_t data_buf[3] = {0};

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
	case MSENSOR_IOCTL_SENSOR_ENABLE:
		if (copy_from_user(&flag, ptr, sizeof(flag)))
			return -EFAULT;
		if (mag_factory.fops != NULL &&
		    mag_factory.fops->enable_sensor != NULL) {
			pr_debug("MSENSOR_ENABLE,en: %d, sample_period:%dms\n",
				flag, 5);
			err = mag_factory.fops->enable_sensor(flag, 20);
			if (err < 0) {
				pr_err(
					"MSENSOR_IOCTL_SENSOR_ENABLE fail!\n");
				return -EINVAL;
			}
		} else {
			pr_err("MSENSOR_IOCTL_SENSOR_ENABLE NULL\n");
			return -EINVAL;
		}
		return 0;
	case MSENSOR_IOCTL_READ_SENSORDATA:
		if (mag_factory.fops != NULL &&
		    mag_factory.fops->get_data != NULL) {
			err = mag_factory.fops->get_data(data_buf, &status);
			if (err < 0) {
				pr_err(
					"MSENSOR_READ_SENSORDATA fail!\n");
				return -EINVAL;
			}
			sprintf(strbuf, "%x %x %x", data_buf[0], data_buf[1],
				data_buf[2]);
			pr_debug("MSENSOR_IOCTL_READ_SENSORDATA read:(%s)!\n",
				strbuf);
			if (copy_to_user(ptr, strbuf, strlen(strbuf) + 1))
				return -EFAULT;
		} else {
			pr_err("MSENSOR_IOCTL_READ_SENSORDATA NULL\n");
			return -EINVAL;
		}
		return 0;
	case MSENSOR_IOCTL_SELF_TEST:
		if (mag_factory.fops != NULL &&
		    mag_factory.fops->do_self_test != NULL) {
			err = mag_factory.fops->do_self_test();
			if (err < 0) {
				pr_err(
					"MSENSOR_IOCTL_SELF_TEST fail!\n");
				return -EINVAL;
			}
		} else {
			pr_err("MSENSOR_IOCTL_SELF_TEST NULL\n");
			return -EINVAL;
		}
		return 0;
	default:
		pr_debug("unknown IOCTL: 0x%08x\n", cmd);
		return -ENOIOCTLCMD;
	}
	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_mag_factory_unlocked_ioctl(struct file *filp,
					      unsigned int cmd,
					      unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		pr_err(
			"compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_MSENSOR_IOCTL_READ_SENSORDATA:
	case COMPAT_MSENSOR_IOCTL_SENSOR_ENABLE:
	case COMPAT_MSENSOR_IOCTL_SELF_TEST:
		pr_debug("compat_ion_ioctl : MSENSOR_IOCTL_XXX is 0x%x\n",
			cmd);
		return filp->f_op->unlocked_ioctl(
			filp, cmd, (unsigned long)compat_ptr(arg));
	default:
		pr_err("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
}
#endif

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

int mag_factory_device_register(struct mag_factory_public *dev)
{
	int err = 0;

	if (!dev || !dev->fops)
		return -1;
	mag_factory.gain = dev->gain;
	mag_factory.sensitivity = dev->sensitivity;
	mag_factory.fops = dev->fops;
	err = misc_register(&mag_factory_device);
	if (err) {
		pr_err("%s register failed\n", __func__);
		err = -1;
	}
	return err;
}

int mag_factory_device_deregister(struct mag_factory_public *dev)
{
	mag_factory.fops = NULL;
	misc_deregister(&mag_factory_device);
	return 0;
}
