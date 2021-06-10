// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <asm/cacheflush.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "fp_func.h"
#include "../tz_driver/include/teei_fp.h"
#include "../tz_driver/include/teei_id.h"
#include "../tz_driver/include/nt_smc_call.h"
#include "../tz_driver/include/utdriver_macro.h"
#include "../tz_driver/include/teei_client_main.h"

#define IMSG_TAG "[teei_fp]"
#include <imsg_log.h>

struct fp_dev {
	struct cdev cdev;
	struct semaphore sem;
};

static int fp_major = FP_MAJOR;
static dev_t devno;

static int wait_teei_config_flag = 1;
static struct class *driver_class;
struct semaphore fp_api_lock;
struct fp_dev *fp_devp;
struct semaphore daulOS_rd_sem;
EXPORT_SYMBOL_GPL(daulOS_rd_sem);
struct semaphore daulOS_wr_sem;
EXPORT_SYMBOL_GPL(daulOS_wr_sem);
DECLARE_WAIT_QUEUE_HEAD(__fp_open_wq);
DECLARE_WAIT_QUEUE_HEAD(__wait_spi_wq);


int fp_open(struct inode *inode, struct file *filp)
{
	if (wait_teei_config_flag == 1) {
		int ret;

		IMSG_INFO("[I]%s : Teei_config_flag = %lu\n",
					__func__, teei_config_flag);

		ret = wait_event_timeout(__fp_open_wq, (teei_config_flag == 1),
						msecs_to_jiffies(1000 * 10));
		if (ret == 0) {
			IMSG_ERROR("[E] Tees's loading is not finished.\n");
			return -1;
		}
		if (ret < 0) {
			IMSG_ERROR("[E] Wait_event_timeout error.\n");
			return -1;
		}

		IMSG_INFO("[I]%s : Load tees finished, and wait for %u msecs\n",
				__func__, (1000 * 10 - jiffies_to_msecs(ret)));

		ret = wait_event_timeout(__wait_spi_wq, (spi_ready_flag == 1),
						msecs_to_jiffies(1000 * 10));
		if (ret == 0) {
			IMSG_ERROR("[E] Spi's loading is not finished.\n");
			return -1;
		}
		if (ret < 0) {
			IMSG_ERROR("[E] Wait_event_timeout error.\n");
			return -1;
		}

		IMSG_INFO("[I]%s : Load spi finished, and wait for %u msecs\n",
			__func__, (1000 * 10 - jiffies_to_msecs(ret)));

		wait_teei_config_flag = 0;
	}

#ifdef FP_DEBUG
	IMSG_DEBUG("say hello from fp!\n");
#endif
	filp->private_data = fp_devp;

	return 0;
}

int fp_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static long fp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	unsigned int args_len = 0;
	unsigned char args[16] = {0};
	int ret = 0;

	down(&fp_api_lock);
	switch (cmd) {
	case CMD_FP_MEM_CLEAR:
		IMSG_INFO("CMD FP MEM CLEAR.\n");
		break;
	case CMD_FP_CMD:
		if (copy_from_user((void *)args, (void *)arg, 16)) {
			IMSG_ERROR("copy args from user failed.\n");
			up(&fp_api_lock);
			return -EFAULT;
		}

		/* TODO compute args length */
		/* [11-15] is the length of data */
		args_len = *((unsigned int *)(args + 12));

		if (args_len + 16 > MICROTRUST_FP_SIZE) {
			IMSG_ERROR("args_len=%d is invalid!.\n", args_len);
			up(&fp_api_lock);
			return -EFAULT;
		}

#ifdef DYNAMIC_SET_PRIORITY
		teei_cpus_write_lock();
#endif

		ret  = send_fp_command((void *)arg, args_len + 16);

#ifdef DYNAMIC_SET_PRIORITY
		teei_cpus_write_unlock();
#endif

		if (ret) {
			IMSG_ERROR("transfer data to ta failed.\n");
			up(&fp_api_lock);
			return -EFAULT;
		}

		break;
	case CMD_FP_LOAD_TEE:
#ifdef FP_DEBUG
		IMSG_DEBUG("case CMD_FP_LOAD_TEE\n");
#endif
		complete(&boot_decryto_lock);
		break;
	case CMD_TEEI_SET_PRI:
		ret = teei_set_switch_pri(arg);
		if (ret != 0) {
			IMSG_ERROR("Failed to teei_set_switch_pri %d\n", ret);
			up(&fp_api_lock);
			return ret;
		}
		break;
	default:
		up(&fp_api_lock);
		return -EINVAL;
	}
	up(&fp_api_lock);
	return 0;
}

static ssize_t fp_read(struct file *filp, char __user *buf,
		size_t size, loff_t *ppos)
{
	int ret = 0;
	return ret;
}

static ssize_t fp_write(struct file *filp, const char __user *buf,
		size_t size, loff_t *ppos)
{
	return 0;
}

static loff_t fp_llseek(struct file *filp, loff_t offset, int orig)
{
	return 0;
}
static const struct file_operations fp_fops = {
	.owner = THIS_MODULE,
	.llseek = fp_llseek,
	.read = fp_read,
	.write = fp_write,
	.unlocked_ioctl = fp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fp_ioctl,
#endif
	.open = fp_open,
	.release = fp_release,
};

static void fp_setup_cdev(struct fp_dev *dev, int index)
{
	int err = 0;
	int devno = MKDEV(fp_major, index);

	cdev_init(&dev->cdev, &fp_fops);
	dev->cdev.owner = fp_fops.owner;
	err = cdev_add(&dev->cdev, devno, 1);
	if (err)
		IMSG_ERROR("Error %d adding fp %d.\n", err, index);
}

int fp_init(void)
{
	int result = 0;
	struct device *class_dev = NULL;

	devno = MKDEV(fp_major, 0);
	result = alloc_chrdev_region(&devno, 0, 1, DEV_NAME);
	fp_major = MAJOR(devno);
	TZ_SEMA_INIT_1(&(fp_api_lock));
	if (result < 0)
		return result;

	driver_class = NULL;
	driver_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(driver_class)) {
		result = -ENOMEM;
		IMSG_ERROR("class_create failed %d.\n", result);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, devno, NULL, DEV_NAME);
	if (!class_dev) {
		result = -ENOMEM;
		IMSG_ERROR("class_device_create failed %d.\n", result);
		goto class_destroy;
	}
	fp_devp = NULL;
	fp_devp = vmalloc(sizeof(struct fp_dev));
	if (fp_devp == NULL) {
		result = -ENOMEM;
		goto class_device_destroy;
	}
	memset(fp_devp, 0, sizeof(struct fp_dev));
	fp_setup_cdev(fp_devp, 0);
	TZ_SEMA_INIT_1(&fp_devp->sem);
	TZ_SEMA_INIT_0(&daulOS_rd_sem);
	TZ_SEMA_INIT_0(&daulOS_wr_sem);

	IMSG_DEBUG("[%s][%d]create the teei_fp device node successfully!\n",
							__func__, __LINE__);
	goto return_fn;

class_device_destroy:
	device_destroy(driver_class, devno);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(devno, 1);
return_fn:
	return result;
}

void fp_exit(void)
{
	device_destroy(driver_class, devno);
	class_destroy(driver_class);
	cdev_del(&fp_devp->cdev);
	vfree(fp_devp);
	unregister_chrdev_region(MKDEV(fp_major, 0), 1);
}

MODULE_AUTHOR("Microtrust");
MODULE_LICENSE("Dual BSD/GPL");

module_param(fp_major, int, 0444);

module_init(fp_init);
module_exit(fp_exit);
