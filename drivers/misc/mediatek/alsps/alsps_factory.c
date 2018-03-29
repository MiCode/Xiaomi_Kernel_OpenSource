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

#include "inc/alsps_factory.h"

static int alsps_factory_open(struct inode *inode, struct file *file)
{
	file->private_data = alsps_context_obj;

	if (file->private_data == NULL) {
		ALSPS_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int alsps_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long alsps_factory_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;
	struct alsps_context *cxt = alsps_context_obj;
	void __user *ptr = (void __user *) arg;
	int dat = 0;
	uint32_t enable = 0;
	int ps_cali = 0;
	int threshold_data[2];

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		ALSPS_LOG("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case ALSPS_SET_PS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			break;
		}
		ALSPS_LOG("ALSPS_SET_PS_MODE enable: %d!\n", enable);

		if (cxt->ps_ctl.enable_nodata != NULL) {
			err = cxt->ps_ctl.enable_nodata(enable);
			if (err < 0) {
				ALSPS_LOG("ALSPS_SET_PS_MODE fail!\n");
				break;
			}
		}
		break;
	case ALSPS_GET_PS_RAW_DATA:
		if (cxt->ps_data.ps_get_raw_data != NULL) {
			err = cxt->ps_data.ps_get_raw_data(&dat);
			if (err < 0) {
				ALSPS_LOG("ALSPS_GET_PS_RAW_DATA fail!\n");
				break;
			}
		}

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
	break;
	case ALSPS_SET_ALS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			break;
		}
		ALSPS_LOG("ALSPS_SET_ALS_MODE enable: %d!\n", enable);

		if (cxt->als_ctl.enable_nodata != NULL) {
			err = cxt->als_ctl.enable_nodata(enable);
			if (err < 0) {
				ALSPS_LOG("ALSPS_SET_ALS_MODE fail!\n");
				break;
			}
		}
	break;
	case ALSPS_GET_ALS_RAW_DATA:
		if (cxt->als_data.als_get_raw_data != NULL) {
			err = cxt->als_data.als_get_raw_data(&dat);
			if (err < 0) {
				ALSPS_LOG("ALSPS_GET_ALS_RAW_DATA fail!\n");
				break;
			}
		}

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
	break;
	case ALSPS_GET_PS_TEST_RESULT:
		if (cxt->ps_ctl.ps_threshold_setting != NULL) {
			err = cxt->ps_ctl.ps_threshold_setting(GET_TH_RESULT, threshold_data);
			if (err < 0) {
				ALSPS_LOG("ALSPS_GET_PS_TEST_RESULT fail!\n");
				break;
			}
		}
		if (copy_to_user(ptr, &threshold_data[0], sizeof(threshold_data[0]))) {
			err = -EFAULT;
			break;
		}
	break;
	case ALSPS_GET_PS_THRESHOLD_HIGH:
		if (cxt->ps_ctl.ps_threshold_setting != NULL) {
			err = cxt->ps_ctl.ps_threshold_setting(GET_TH_HIGH, threshold_data);
			if (err < 0) {
				ALSPS_LOG("ALSPS_GET_PS_THRESHOLD_HIGH fail!\n");
				break;
			}
		}
		if (copy_to_user(ptr, &threshold_data[0], sizeof(threshold_data[0]))) {
			err = -EFAULT;
			break;
		}
	break;

	case ALSPS_GET_PS_THRESHOLD_LOW:
		if (cxt->ps_ctl.ps_threshold_setting != NULL) {
			err = cxt->ps_ctl.ps_threshold_setting(GET_TH_LOW, threshold_data);
			if (err < 0) {
				ALSPS_LOG("ALSPS_GET_PS_THRESHOLD_LOW fail!\n");
				break;
			}
		}
		if (copy_to_user(ptr, &threshold_data[0], sizeof(threshold_data[0]))) {
			err = -EFAULT;
			break;
		}
	break;

	case ALSPS_SET_PS_THRESHOLD:
		if (copy_from_user(threshold_data, ptr, sizeof(threshold_data))) {
			err = -EFAULT;
			break;
		}

		if (cxt->ps_ctl.ps_threshold_setting != NULL) {
			err = cxt->ps_ctl.ps_threshold_setting(SET_TH, threshold_data);
			if (err < 0) {
				ALSPS_LOG("ALSPS_GET_PS_THRESHOLD_LOW fail!\n");
				break;
			}
		}
	break;

	case ALSPS_IOCTL_SET_CALI:
		if (copy_from_user(&ps_cali, ptr, sizeof(ps_cali))) {
			err = -EFAULT;
			break;
		}
		if (cxt->ps_ctl.ps_calibration != NULL) {
			err = cxt->ps_ctl.ps_calibration(SETCALI, ps_cali);
			if (err < 0) {
				ALSPS_LOG("ALSPS_IOCTL_SET_CALI fail!\n");
				break;
			}
		}
		break;
	case ALSPS_IOCTL_GET_CALI:
		if (cxt->ps_ctl.ps_calibration != NULL) {
			err = cxt->ps_ctl.ps_calibration(GETCALI, ps_cali);
			if (err < 0) {
				ALSPS_LOG("ALSPS_IOCTL_GET_CALI fail!\n");
				break;
			}
		}
		if (copy_to_user(ptr, &ps_cali, sizeof(ps_cali))) {
			err = -EFAULT;
			break;
		}
		break;
	case ALSPS_IOCTL_CLR_CALI:
		if (copy_from_user(&dat, ptr, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
		if (cxt->ps_ctl.ps_calibration != NULL) {
			err = cxt->ps_ctl.ps_calibration(CLRCALI, dat);
			if (err < 0) {
				ALSPS_LOG("ALSPS_IOCTL_CLR_CALI fail!\n");
				break;
			}
		}
		break;

	default:
	ALSPS_ERR("unknown IOCTL: 0x%08x\n", cmd);
	err = -ENOIOCTLCMD;
	break;

	}
	return err;
}
#ifdef CONFIG_COMPAT
static long alsps_factory_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long err = 0;

	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_ALSPS_SET_PS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_ALS_MODE:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_ALS_MODE, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_ALS_RAW_DATA:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_ALS_RAW_DATA, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_TEST_RESULT:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_TEST_RESULT, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_HIGH:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_HIGH, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_GET_PS_THRESHOLD_LOW:
		err = file->f_op->unlocked_ioctl(file, ALSPS_GET_PS_THRESHOLD_LOW, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_SET_PS_THRESHOLD:
		err = file->f_op->unlocked_ioctl(file, ALSPS_SET_PS_THRESHOLD, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_SET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_SET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_GET_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_GET_CALI, (unsigned long)arg32);
		break;
	case COMPAT_ALSPS_IOCTL_CLR_CALI:
		err = file->f_op->unlocked_ioctl(file, ALSPS_IOCTL_CLR_CALI, (unsigned long)arg32);
		break;
	default:
		ALSPS_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;
	}

	return err;
}
#endif

static const struct file_operations alsps_factory_fops = {
	.open = alsps_factory_open,
	.release = alsps_factory_release,
	.unlocked_ioctl = alsps_factory_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = alsps_factory_compat_ioctl,
#endif
};

static struct miscdevice alsps_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "als_ps",
	.fops = &alsps_factory_fops,
};

int alsps_factory_device_init(void)
{
	int error = 0;
	struct alsps_context *cxt = alsps_context_obj;

	if (!cxt->als_ctl.is_use_common_factory && !cxt->ps_ctl.is_use_common_factory) {
		ALSPS_LOG("Node of '/dev/als_ps' has already existed!\n");
		return -1;
	}
	error = misc_register(&alsps_factory_device);
	if (error) {
		ALSPS_LOG("alsps_factory_device register failed\n");
		error = -1;
	}
	return error;
}
