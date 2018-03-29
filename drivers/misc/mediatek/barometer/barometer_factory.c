#include "inc/barometer_factory.h"

static int baro_factory_open(struct inode *inode, struct file *file)
{
	file->private_data = baro_context_obj;

	if (file->private_data == NULL) {
		BARO_ERR("null pointer!!\n");
		return -EINVAL;
	}
	return nonseekable_open(inode, file);
}

static int baro_factory_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long baro_factory_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	/* void __user *data; */
	long err = 0;
	struct baro_context *cxt = baro_context_obj;
	void __user *ptr = (void __user *)arg;
	int dat;
	/* uint32_t enable = 0; */
	/* int ps_cali; */
	/* int threshold_data[2]; */


	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));

	if (err) {
		BARO_ERR("access error: %08X, (%2d, %2d)\n", cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case BAROMETER_IOCTL_INIT:
		BARO_LOG("BAROMETER_IOCTL_INIT enable\n");
		if (cxt->baro_ctl.enable_nodata != NULL) {
			err = cxt->baro_ctl.enable_nodata(1);
			if (err < 0) {
				BARO_ERR("BAROMETER_IOCTL_INIT fail!\n");
				break;
			}
		}
		break;
	case BAROMETER_GET_PRESS_DATA:
		if (cxt->baro_data.get_raw_data != NULL) {
			err = cxt->baro_data.get_raw_data(TYPE_PRESS, &dat);
			if (err < 0) {
				BARO_ERR("BAROMETER_GET_PRESS_DATA fail!\n");
				break;
			}
		}

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
		break;
	case BAROMETER_GET_TEMP_DATA:
		if (cxt->baro_data.get_raw_data != NULL) {
			err = cxt->baro_data.get_raw_data(TYPE_TEMP, &dat);
			if (err < 0) {
				BARO_ERR("BAROMETER_GET_PRESS_DATA fail!\n");
				break;
			}
		}


		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			break;
		}
		break;

	default:
		BARO_ERR("unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}
	return err;
}

#if IS_ENABLED(CONFIG_COMPAT)
static long compat_baro_factory_unlocked_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		BARO_ERR("compat_ion_ioctl file has no f_op or no f_op->unlocked_ioctl.\n");
		return -ENOTTY;
	}

	switch (cmd) {
	case COMPAT_BAROMETER_IOCTL_INIT:
	/* case COMPAT_BAROMETER_IOCTL_READ_CHIPINFO: */
	case COMPAT_BAROMETER_GET_PRESS_DATA:
	case COMPAT_BAROMETER_GET_TEMP_DATA: {
		BARO_LOG("compat_ion_ioctl : BAROMETER_IOCTL_XXX command is 0x%x\n", cmd);
		return filp->f_op->unlocked_ioctl(filp, cmd,
			(unsigned long)compat_ptr(arg));
	}
	default:
		BARO_ERR("compat_ion_ioctl : No such command!! 0x%x\n", cmd);
		return -ENOIOCTLCMD;
	}
}
#endif
/*----------------------------------------------------------------------------*/
static const struct file_operations baro_factory_fops = {
	.open = baro_factory_open,
	.release = baro_factory_release,
	.unlocked_ioctl = baro_factory_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl = compat_baro_factory_unlocked_ioctl,
#endif
};

static struct miscdevice baro_factory_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "barometer",
	.fops = &baro_factory_fops,
};

int baro_factory_device_init(void)
{
	int error = 0;
	struct baro_context *cxt = baro_context_obj;

	if (!cxt->baro_ctl.is_use_common_factory) {
		BARO_LOG("Node of '/dev/barometer' has already existed!\n");
		return -1;
	}

	error = misc_register(&baro_factory_device);
	if (error) {
		BARO_ERR("baro_factory_device register failed\n");
		error = -1;
	}
	return error;
}
