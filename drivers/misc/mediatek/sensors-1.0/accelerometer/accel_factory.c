// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "<ACCEL_FAC> " fmt

#include "inc/accel_factory.h"

struct accel_factory_private {
	uint32_t gain;
	uint32_t sensitivity;
	struct accel_factory_fops *fops;
};

static struct accel_factory_private accel_factory;

static int acc_factory_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int acc_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long acc_factory_unlocked_ioctl(struct file *file, unsigned int cmd,
				       unsigned long arg)
{
	void __user *ptr = (void __user *)arg;
	int err = 0, status = 0;
	uint32_t flag = 0;
	char strbuf[64];
	int32_t data_buf[3] = {0};
	struct SENSOR_DATA sensor_data = {0};

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
	case GSENSOR_IOCTL_INIT:
		if (copy_from_user(&flag, ptr, sizeof(flag)))
			return -EFAULT;
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->enable_sensor != NULL) {
			err = accel_factory.fops->enable_sensor(flag, 5);
			if (err < 0) {
				pr_err("GSENSOR_IOCTL_INIT fail!\n");
				return -EINVAL;
			}
			pr_debug("GSENSOR_IOCTL_INIT, enable: %d, s_p:%dms\n",
				flag, 5);
		} else {
			pr_debug("GSENSOR_IOCTL_INIT NULL\n");
			return -EINVAL;
		}
		return 0;
	case GSENSOR_IOCTL_READ_CHIPINFO:
		return 0;
	case GSENSOR_IOCTL_READ_SENSORDATA:
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->get_data != NULL) {
			err = accel_factory.fops->get_data(data_buf, &status);
			if (err < 0) {
				pr_err(
					"GSENSOR_READ_SENSORDATA read fail!\n");
				return -EINVAL;
			}
			sprintf(strbuf, "%x %x %x", data_buf[0], data_buf[1],
				data_buf[2]);
			pr_debug("GSENSOR_READ_SENSORDATA read strbuf : (%s)!\n",
				strbuf);
			if (copy_to_user(ptr, strbuf, strlen(strbuf) + 1))
				return -EFAULT;
		} else {
			pr_debug("GSENSOR_IOCTL_READ_SENSORDATA NULL\n");
			return -EINVAL;
		}
		return 0;
	case GSENSOR_IOCTL_READ_RAW_DATA:
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->get_raw_data != NULL) {
			err = accel_factory.fops->get_raw_data(data_buf);
			if (err < 0) {
				pr_err(
					"GSENSOR_READ_RAW_DATA read fail!\n");
				return -EINVAL;
			}
			sprintf(strbuf, "%x %x %x", data_buf[0], data_buf[1],
				data_buf[2]);
			pr_debug("GSENSOR_SENSORDATA_RAW read strbuf : (%s)!\n",
				strbuf);
			if (copy_to_user(ptr, strbuf, strlen(strbuf) + 1))
				return -EFAULT;
		} else {
			pr_debug("GSENSOR_IOCTL_READ_SENSORDATA_RAW NULL\n");
			return -EINVAL;
		}
		return 0;
	case GSENSOR_IOCTL_SET_CALI:
		if (copy_from_user(&sensor_data, ptr, sizeof(sensor_data)))
			return -EFAULT;
		data_buf[0] = sensor_data.x;
		data_buf[1] = sensor_data.y;
		data_buf[2] = sensor_data.z;
		pr_debug("GSENSOR_IOCTL_SET_CALI: (%d, %d, %d)!\n", data_buf[0],
			data_buf[1], data_buf[2]);
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->set_cali != NULL) {
			err = accel_factory.fops->set_cali(data_buf);
			if (err < 0) {
				pr_err("GSENSOR_IOCTL_SET_CALI FAIL!\n");
				return -EINVAL;
			}
		} else {
			pr_debug("GSENSOR_IOCTL_SET_CALI NULL\n");
			return -EINVAL;
		}
		return 0;
	case GSENSOR_IOCTL_CLR_CALI:
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->clear_cali != NULL) {
			err = accel_factory.fops->clear_cali();
			if (err < 0) {
				pr_err("GSENSOR_IOCTL_CLR_CALI FAIL!\n");
				return -EINVAL;
			}
		} else {
			pr_err("GSENSOR_IOCTL_CLR_CALI NULL\n");
			return -EINVAL;
		}
		return 0;
	case GSENSOR_IOCTL_GET_CALI:
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->get_cali != NULL) {
			err = accel_factory.fops->get_cali(data_buf);
			if (err < 0) {
				pr_err("GSENSOR_IOCTL_GET_CALI FAIL!\n");
				return -EINVAL;
			}
		} else {
			pr_err("GSENSOR_IOCTL_GET_CALI NULL\n");
			return -EINVAL;
		}
		pr_debug("GSENSOR_IOCTL_GET_CALI: (%d, %d, %d)!\n", data_buf[0],
			data_buf[1], data_buf[2]);
		sensor_data.x = data_buf[0];
		sensor_data.y = data_buf[1];
		sensor_data.z = data_buf[2];
		if (copy_to_user(ptr, &sensor_data, sizeof(sensor_data)))
			return -EFAULT;
		return 0;
	case GSENSOR_IOCTL_ENABLE_CALI:
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->enable_calibration != NULL) {
			err = accel_factory.fops->enable_calibration();
			if (err < 0) {
				pr_err("GSENSOR_IOCTL_ENABLE_CALI FAIL!\n");
				return -EINVAL;
			}
		} else {
			pr_err("GSENSOR_IOCTL_ENABLE_CALI NULL\n");
			return -EINVAL;
		}
		return 0;
	case GSENSOR_IOCTL_SELF_TEST:
		if (accel_factory.fops != NULL &&
		    accel_factory.fops->do_self_test != NULL) {
			err = accel_factory.fops->do_self_test();
			if (err < 0) {
				pr_err("GSENSOR_IOCTL_SELF_TEST FAIL!\n");
				return -EINVAL;
			}
		} else {
			pr_err("GSENSOR_IOCTL_SELF_TEST NULL\n");
			return -EINVAL;
		}
		return 0;
	default:
		pr_err("unknown IOCTL: 0x%08x\n", cmd);
		return -ENOIOCTLCMD;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_acc_factory_unlocked_ioctl(struct file *filp,
					unsigned int cmd, unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		pr_err(
			"compat_ioctl has no f_op or no unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_GSENSOR_IOCTL_INIT:
	case COMPAT_GSENSOR_IOCTL_READ_CHIPINFO:
	/* case COMPAT_GSENSOR_IOCTL_READ_GAIN: */
	case COMPAT_GSENSOR_IOCTL_READ_RAW_DATA:
	case COMPAT_GSENSOR_IOCTL_READ_SENSORDATA:
	/* NVRAM will use below ioctl */
	case COMPAT_GSENSOR_IOCTL_SET_CALI:
	case COMPAT_GSENSOR_IOCTL_CLR_CALI:
	case COMPAT_GSENSOR_IOCTL_GET_CALI:
	case COMPAT_GSENSOR_IOCTL_ENABLE_CALI:
	case COMPAT_GSENSOR_IOCTL_SELF_TEST:
		pr_debug("compat_ioctl : GSENSOR_IOCTL command is 0x%x\n", cmd);
		return filp->f_op->unlocked_ioctl(
			filp, cmd, (unsigned long)compat_ptr(arg));

	default:
		pr_err("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations acc_factory_fops = {
	.open = acc_factory_open,
	.release = acc_factory_release,
	.unlocked_ioctl = acc_factory_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_acc_factory_unlocked_ioctl,
#endif
};

static struct miscdevice accel_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "gsensor",
	.fops = &acc_factory_fops,
};

int accel_factory_device_register(struct accel_factory_public *dev)
{
	int err = 0;

	if (!dev || !dev->fops)
		return -1;
	accel_factory.gain = dev->gain;
	accel_factory.sensitivity = dev->sensitivity;
	accel_factory.fops = dev->fops;
	err = misc_register(&accel_factory_device);
	if (err) {
		pr_err("accel_factory_device register failed\n");
		err = -1;
	}
	return err;
}

int accel_factory_device_deregister(struct accel_factory_public *dev)
{
	accel_factory.fops = NULL;
	misc_deregister(&accel_factory_device);
	return 0;
}
