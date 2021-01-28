// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define pr_fmt(fmt) "[ALS/AAL]" fmt

#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/uaccess.h>

#include "inc/aal_control.h"
#include "inc/alsps.h"
#include <sensors_io.h>

int aal_use /* = 0*/;

static int AAL_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int AAL_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long AAL_unlocked_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	long err = 0;
	void __user *ptr = (void __user *)arg;
	int dat;
	uint32_t enable;

	switch (cmd) {

	case AAL_SET_ALS_MODE:
		if (copy_from_user(&enable, ptr, sizeof(enable))) {
			err = -EFAULT;
			goto err_out;
		}

		if (enable)
			aal_use = 1;
		else
			aal_use = 0;

		err = alsps_aal_enable(enable);
		if (err)
			pr_err(
				"als driver don't support new arch, goto execute old arch: %ld\n",
				err);
		break;

	case AAL_GET_ALS_MODE:
		pr_debug("AAL_GET_ALS_MODE do nothing\n");
		break;

	case AAL_GET_ALS_DATA:
		dat = alsps_aal_get_data();
		/* pr_debug("Get als dat :%d\n", dat); */

		if (copy_to_user(ptr, &dat, sizeof(dat))) {
			err = -EFAULT;
			goto err_out;
		}
		break;

	default:
		pr_err("%s not supported = 0x%04x", __func__, cmd);
		err = -ENOIOCTLCMD;
		break;
	}

err_out:
	return err;
}

#ifdef CONFIG_COMPAT
static long AAL_compact_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	void __user *data32;

	data32 = compat_ptr(arg);
	return AAL_unlocked_ioctl(file, cmd, (unsigned long)data32);
}
#endif

static const struct file_operations AAL_fops = {
	.owner = THIS_MODULE,
	.open = AAL_open,
	.release = AAL_release,
	.unlocked_ioctl = AAL_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = AAL_compact_ioctl,
#endif
};

static struct miscdevice AAL_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "aal_als",
	.fops = &AAL_fops,
};

/*----------------------------------------------------------------------------*/
static int __init AAL_init(void)
{
	int err;

	err = misc_register(&AAL_device);
	if (err)
		pr_err("AAL_device misc_register failed: %d\n", err);

	pr_debug("OK!\n");
	return 0;
}
/*----------------------------------------------------------------------------*/
static void __exit AAL_exit(void)
{
	/*int err;*/

	/*err = */ misc_deregister(&AAL_device);
	/*	if (err)
	 *	pr_err("AAL_device misc_deregister fail: %d\n", err);
	 */

}
late_initcall(AAL_init);
MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("AAL driver");
MODULE_LICENSE("GPL");
