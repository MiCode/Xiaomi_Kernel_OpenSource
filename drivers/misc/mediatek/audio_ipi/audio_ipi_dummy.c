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

#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>



/*
 * =============================================================================
 *                     MACRO
 * =============================================================================
 */

#define AUDIO_IPI_DEVICE_NAME "audio_ipi"


/*
 * =============================================================================
 *                     private global members
 * =============================================================================
 */


/*
 * =============================================================================
 *                     functions declaration
 * =============================================================================
 */



/*
 * =============================================================================
 *                     implementation
 * =============================================================================
 */


static long audio_ipi_driver_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	pr_debug("%s not support!!\n", __func__);
	return 0;
}

#ifdef CONFIG_COMPAT
static long audio_ipi_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		pr_debug("op null\n");
		return -ENOTTY;
	}
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}
#endif


static ssize_t audio_ipi_driver_read(
	struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	pr_debug("%s not support!!\n", __func__);
	return 0;
}



static const struct file_operations audio_ipi_driver_ops = {
	.owner          = THIS_MODULE,
	.read           = audio_ipi_driver_read,
	.unlocked_ioctl = audio_ipi_driver_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = audio_ipi_driver_compat_ioctl,
#endif
};


static struct miscdevice audio_ipi_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = AUDIO_IPI_DEVICE_NAME,
	.fops = &audio_ipi_driver_ops
};


static int __init audio_ipi_driver_init(void)
{
	int ret = 0;

	ret = misc_register(&audio_ipi_device);
	if (unlikely(ret != 0)) {
		pr_debug("[SCP] misc register failed\n");
		return ret;
	}

	return ret;
}


static void __exit audio_ipi_driver_exit(void)
{

}


module_init(audio_ipi_driver_init);
module_exit(audio_ipi_driver_exit);

