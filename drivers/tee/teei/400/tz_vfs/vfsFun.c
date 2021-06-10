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
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched/types.h>
#include <teei_ioc.h>
#include "TEEI.h"
#include "teei_id.h"
#include "fp_vendor.h"
#include <linux/uaccess.h>
#include "VFS.h"
#include "../tz_driver/include/backward_driver.h"
#include "../tz_driver/include/teei_client_main.h"
#include "../common/include/tee_client_api.h"
#include "../tz_driver/include/teei_fp.h"

#define IMSG_TAG "[tz_vfs]"
#include <imsg_log.h>

#define VFS_SIZE	0x80000
#define MEM_CLEAR       0x1
#define VFS_MAJOR       253

#define GET_FP_VENDOR_CMD _IOWR(TEEI_CONFIG_IOC_MAGIC, 0x80, int)

int enter_tui_flag = 1;

static int vfs_major = VFS_MAJOR;
static struct class *driver_class;
static dev_t devno;

struct vfs_dev {
	struct cdev cdev;
	struct semaphore sem;
};

#ifdef VFS_RDWR_SEM
struct semaphore VFS_rd_sem;
struct semaphore VFS_wr_sem;
#else
DECLARE_COMPLETION(VFS_rd_comp);
DECLARE_COMPLETION(VFS_wr_comp);
#endif

struct vfs_dev *vfs_devp;

int wait_for_vfs_done(void)
{
#ifdef VFS_RDWR_SEM
	down_interruptible(&VFS_wr_sem);
#else
	wait_for_completion_interruptible(&VFS_wr_comp);
#endif
	return 0;
}

int notify_vfs_handle(void)
{
#ifdef VFS_RDWR_SEM
	up(&VFS_rd_sem);
#else
	complete(&VFS_rd_comp);
#endif
	return 0;
}


static int tz_vfs_open(struct inode *inode, struct file *filp)
{
	struct sched_param param = {.sched_priority = 52 };

	if (vfs_devp == NULL)
		return -EINVAL;

	if (filp == NULL)
		return -EINVAL;

	if (strcmp("teei_daemon", current->comm) != 0)
		return -EINVAL;

	filp->private_data = vfs_devp;

	sched_setscheduler_nocheck(current, SCHED_FIFO, &param);

	return 0;
}

static int tz_vfs_release(struct inode *inode, struct file *filp)
{
	filp->private_data = NULL;
	return 0;
}

static long tz_vfs_ioctl(struct file *filp,
			unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int fp_vendor;

	switch (cmd) {
	case GET_FP_VENDOR_CMD:
		fp_vendor = get_fp_vendor();
		ret = copy_to_user((void *)arg, &fp_vendor, sizeof(int));
		break;
	case TEEI_VFS_GET_FP_UUID:
		ret = copy_to_user((void *)arg, &uuid_fp,
					sizeof(struct TEEC_UUID));
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static ssize_t tz_vfs_read(struct file *filp, char __user *buf,
			size_t size, loff_t *ppos)
{
	struct TEEI_vfs_command *vfs_p = NULL;
	int length = 0;
	int ret = 0;

	if (buf == NULL)
		return -EINVAL;

	if ((size < 0) || (size > VFS_SIZE))
		return -EINVAL;

	/*IMSG_DEBUG("read begin cpu[%d]\n",cpu_id);*/
#ifdef VFS_RDWR_SEM
	down_interruptible(&VFS_rd_sem);
#else
	ret = wait_for_completion_interruptible(&VFS_rd_comp);

	if (ret == -ERESTARTSYS) {
		IMSG_ERROR("[%s][%d] wait_for_completion was interrupt\n",
				__func__, __LINE__);
		return ret;
	}
#endif

	if (daulOS_VFS_share_mem == NULL)
		return -EINVAL;

	vfs_p = (struct TEEI_vfs_command *)daulOS_VFS_share_mem;

	if (vfs_p->cmd_size > size)
		length = size;
	else
		length = vfs_p->cmd_size;

	length = size;

	if (copy_to_user(buf, (void *)vfs_p, length))
		ret = -EFAULT;
	else
		ret = length;
	return ret;
}

static ssize_t tz_vfs_write(struct file *filp, const char __user *buf,
			size_t size, loff_t *ppos)
{
	if (buf == NULL)
		return -EINVAL;

	if (daulOS_VFS_share_mem == NULL)
		return -EINVAL;

	if ((size < 0) || (size > VFS_SIZE))
		return -EINVAL;

	/*IMSG_DEBUG("write begin cpu_id[%d]\n",cpu_id);*/
	if (copy_from_user((void *)daulOS_VFS_share_mem, buf, size))
		return -EFAULT;

	Flush_Dcache_By_Area((unsigned long)daulOS_VFS_share_mem,
				(unsigned long)daulOS_VFS_share_mem + size);

#ifdef VFS_RDWR_SEM
	up(&VFS_wr_sem);
#else
	complete(&VFS_wr_comp);
#endif
	return 0;
}

static loff_t tz_vfs_llseek(struct file *filp, loff_t offset, int orig)
{
	loff_t ret = 0;

	switch (orig) {
	case 0:
		if (offset < 0) {
			ret = -EINVAL;
			break;
		}

		if ((unsigned int)offset > VFS_SIZE) {
			ret = -EINVAL;
			break;
		}

		filp->f_pos = (unsigned int)offset;
		ret = filp->f_pos;
		break;

	case 1:
		if ((filp->f_pos + offset) > VFS_SIZE) {
			ret = -EINVAL;
			break;
		}

		if ((filp->f_pos + offset) < 0) {
			ret = -EINVAL;
			break;
		}

		filp->f_pos += offset;
		ret = filp->f_pos;
		break;

	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static const struct file_operations vfs_fops = {
	.owner =		THIS_MODULE,
	.llseek =		tz_vfs_llseek,
	.read =			tz_vfs_read,
	.write =		tz_vfs_write,
	.unlocked_ioctl = tz_vfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = tz_vfs_ioctl,
#endif
	.open =			tz_vfs_open,
	.release =		tz_vfs_release,
};

static void vfs_setup_cdev(struct vfs_dev *dev, int index)
{
	int err = 0;
	int devno = MKDEV(vfs_major, index);

	cdev_init(&dev->cdev, &vfs_fops);
	dev->cdev.owner = vfs_fops.owner;
	err = cdev_add(&dev->cdev, devno, 1);

	if (err)
		IMSG_ERROR("Error %d adding socket %d.\n", err, index);
}

static int vfs_init(void)
{
	int result = 0;
	struct device *class_dev = NULL;

	devno = MKDEV(vfs_major, 0);

	result = alloc_chrdev_region(&devno, 0, 1, "tz_vfs");
	vfs_major = MAJOR(devno);

	if (result < 0)
		return result;

	driver_class = class_create(THIS_MODULE, "tz_vfs");

	if (IS_ERR(driver_class)) {
		result = -ENOMEM;
		IMSG_ERROR("class_create failed %d.\n", result);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, devno, NULL, "tz_vfs");

	if (!class_dev) {
		result = -ENOMEM;
		IMSG_ERROR("class_device_create failed %d.\n", result);
		goto class_destroy;
	}

	vfs_devp = vmalloc(sizeof(struct vfs_dev));

	if (vfs_devp == NULL) {
		result = -ENOMEM;
		goto class_device_destroy;
	}

	memset(vfs_devp, 0, sizeof(struct vfs_dev));
	vfs_setup_cdev(vfs_devp, 0);
	TZ_SEMA_INIT_1(&vfs_devp->sem);

#ifdef VFS_RDWR_SEM
	TZ_SEMA_INIT_0(&VFS_rd_sem);
	TZ_SEMA_INIT_0(&VFS_wr_sem);
#endif
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

static void vfs_exit(void)
{
	device_destroy(driver_class, devno);
	class_destroy(driver_class);
	cdev_del(&vfs_devp->cdev);
	vfree(vfs_devp);
	unregister_chrdev_region(MKDEV(vfs_major, 0), 1);
}

MODULE_AUTHOR("MicroTrust");
MODULE_LICENSE("Dual BSD/GPL");

module_param(vfs_major, int, 0444);

module_init(vfs_init);
module_exit(vfs_exit);
