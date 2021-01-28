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

#include "../tz_driver/include/teei_id.h"
#include "../tz_driver/include/tz_service.h"
#include "../tz_driver/include/nt_smc_call.h"
#include "../tz_driver/include/utr_tui_cmd.h"
#include "../tz_driver/include/utdriver_macro.h"
#include "utr_tui_main.h"

#define IMSG_TAG "[ut_tui]"
#include <imsg_log.h>


static dev_t devno;
struct ut_tui_dev *ut_tui_devp;
struct class *driver_class;
struct device *class_dev;
int count = 1;
unsigned long ut_tui_client_buf;
unsigned long ut_tui_client_buf_len;

struct ut_tui_dev {
	struct cdev cdev;
	struct semaphore sem;
};

int tz_ut_tui_open(struct inode *inode, struct file *filp)
{
	if (ut_tui_devp == NULL)
		return -EINVAL;

	if (filp == NULL)
		return -EINVAL;

	filp->private_data = ut_tui_devp;
		return 0;
}

int tz_ut_tui_release(struct inode *inode, struct file *filp)
{
	free_pages(ut_tui_client_buf,
		get_order(ROUND_UP(ut_tui_client_buf_len, SZ_4K)));
	ut_tui_client_buf = 0;
	filp->private_data = NULL;
	return 0;
}


static long tz_ut_tui_u32_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	long retVal = 0;
	void *argp = (void __user *) arg;
	int lockNum = 0;
	unsigned long message_buff = 0;
	int buffer_max_size = 0;
	struct ut_tui_data_u32 data;
	u64 addr = 0;
	void __user *pt = 0;

	switch (cmd) {
	case UT_TUI_DISPLAY_COMMAND_U32:
		message_buff = tui_display_message_buff;
		buffer_max_size = TUI_DISPLAY_BUFFER;
		break;

	case UT_TUI_NOTICE_COMMAND_U32:
		message_buff = tui_notice_message_buff;
		buffer_max_size = TUI_NOTICE_BUFFER;
		break;
	default:
		IMSG_ERROR("[%s][%d] command(%x) not found!\n",
						__func__, __LINE__, cmd);
		retVal = -EINVAL;
		return retVal;
	}

	memset(&data, 0, sizeof(data));
	if (copy_from_user(&data, argp, sizeof(data))) {
		IMSG_ERROR("copy data from user failed.\n");
		return -EFAULT;
	}

	memcpy((void *)message_buff, &data.datalen, sizeof(data.datalen));

	addr = data.data;
	pt = compat_ptr(addr);

	if (copy_from_user((void *)message_buff + sizeof(data.datalen),
					pt, data.datalen)) {

		IMSG_ERROR("copy buffer from user failed.\n");
		return -EFAULT;
	}

	Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + buffer_max_size);

	if (cmd == UT_TUI_NOTICE_COMMAND_U32)
		send_tui_display_command(TUI_DISPLAY_SYS_NO);
	else
		send_tui_notice_command(TUI_NOTICE_SYS_NO);

	if (copy_to_user(argp, (void *)message_buff, sizeof(data.datalen))) {
		IMSG_ERROR("copy datalen to user failed.\n");
		return -EFAULT;
	}

	if (copy_to_user(pt, (void *)message_buff + sizeof(data.datalen),
					*((uint32_t *)message_buff))) {
		IMSG_ERROR("copy data to user failed.\n");
		return -EFAULT;
	}

	return retVal;
}


static long tz_ut_tui_u64_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	long retVal = 0;
	void *argp = (void __user *) arg;
	int lockNum = 0;
	unsigned long message_buff = 0;
	int buffer_max_size = 0;
	struct ut_tui_data_u64 data;

	switch (cmd) {
	case UT_TUI_DISPLAY_COMMAND_U64:
		message_buff = tui_display_message_buff;
		buffer_max_size = TUI_DISPLAY_BUFFER;
		break;
	case UT_TUI_NOTICE_COMMAND_U64:
		message_buff = tui_notice_message_buff;
		buffer_max_size = TUI_NOTICE_BUFFER;
		break;
	default:
		IMSG_ERROR("[%s][%d] command(%x) not found!\n",
						__func__, __LINE__, cmd);
		retVal = -EINVAL;
		return retVal;
	}

	memset(&data, 0, sizeof(data));
	if (copy_from_user(&data, argp, sizeof(data))) {
		IMSG_ERROR("copy data from user failed.\n");
		return -EFAULT;
	}

	memcpy((void *)message_buff, &data.datalen, sizeof(data.datalen));
	if (copy_from_user((void *)message_buff + sizeof(data.datalen),
					(void *)data.data, data.datalen)) {

		IMSG_ERROR("copy buffer from user failed.\n");
		return -EFAULT;
	}

	Flush_Dcache_By_Area((unsigned long)message_buff,
				(unsigned long)message_buff + buffer_max_size);

	if (cmd == UT_TUI_NOTICE_COMMAND_U64)
		send_tui_display_command(TUI_DISPLAY_SYS_NO);
	else
		send_tui_notice_command(TUI_NOTICE_SYS_NO);


	if (copy_to_user(argp, (void *)message_buff, sizeof(data.datalen))) {
		IMSG_ERROR("copy datalen to user failed.\n");
		return -EFAULT;
	}

	if (copy_to_user((void *)data.data,
			(void *)message_buff + sizeof(data.datalen),
			*((uint32_t *)message_buff))) {

		IMSG_ERROR("copy data to user failed.\n");
		return -EFAULT;
	}
	return retVal;
}

static ssize_t tz_ut_tui_read(struct file *filp, char __user *buf,
		size_t size, loff_t *ppos)
{
	int ret = 0;

	return ret;
}

static ssize_t tz_ut_tui_write(struct file *filp, const char __user *buf,
		size_t size, loff_t *ppos)
{
	return 0;
}

static loff_t tz_ut_tui_llseek(struct file *filp, loff_t offset, int orig)
{
	return 0;
}

static int tz_ut_tui_map(struct file *filp, struct vm_area_struct *vma)
{
	int retVal = 0;

	ut_tui_client_buf_len = vma->vm_end - vma->vm_start;
#ifdef UT_DMA_ZONE
	ut_tui_client_buf = (unsigned long)__get_free_pages(
			GFP_KERNEL | GFP_DMA,
			get_order(ROUND_UP(ut_tui_client_buf_len, SZ_4K)));
#else
	ut_tui_client_buf = (unsigned long)__get_free_pages(GFP_KERNEL,
			get_order(ROUND_UP(ut_tui_client_buf_len, SZ_4K)));
#endif
	retVal = remap_pfn_range(vma, vma->vm_start,
		     ((virt_to_phys((void *)ut_tui_client_buf)) >> PAGE_SHIFT),
			ut_tui_client_buf_len, vma->vm_page_prot);

	if (retVal) {
		IMSG_ERROR("[%s][%d] remap_pfn_range failed!\n",
							__func__, __LINE__);
		free_pages(ut_tui_client_buf,
			get_order(ROUND_UP(ut_tui_client_buf_len, SZ_4K)));
		ut_tui_client_buf = 0;
		return retVal;
	}
	return retVal;
}

static const struct file_operations ut_tui_fops = {
	.owner = THIS_MODULE,
	.llseek = tz_ut_tui_llseek,
	.read = tz_ut_tui_read,
	.write = tz_ut_tui_write,
	.unlocked_ioctl = tz_ut_tui_u64_ioctl,	/* 64 */
#ifdef CONFIG_COMPAT
	.compat_ioctl = tz_ut_tui_u32_ioctl,	/* 32 */
#endif
	.open = tz_ut_tui_open,
	.mmap = tz_ut_tui_map,
	.release = tz_ut_tui_release,
};

static int ut_tui_setup_cdev(struct ut_tui_dev *dev)
{
	int err = 0;

	cdev_init(&dev->cdev, &ut_tui_fops);
	dev->cdev.owner = ut_tui_fops.owner;
	err = cdev_add(&dev->cdev, MKDEV(MAJOR(devno), 0), count);

	return err;
}

int ut_tui_init(void)
{
	int result = 0;
	int add_cdev_result = 0;
	int firstminor = 0;

	class_dev = NULL;
	driver_class = NULL;

	result = alloc_chrdev_region(&devno, firstminor,
					count, UT_TUI_CLIENT_DEV);
	if (result < 0) {
		IMSG_ERROR("Err: failed in alloc dev %x\n", result);
		return result;
	}

	driver_class = class_create(THIS_MODULE, UT_TUI_CLIENT_DEV);
	if (IS_ERR(driver_class)) {
		result = -ENOMEM;
		IMSG_ERROR("Err: failed in creating class %x.\n", result);
		goto unregister_chrdev_region;
	}

	class_dev = device_create(driver_class, NULL, devno,
					NULL, UT_TUI_CLIENT_DEV);
	if (!class_dev) {
		result = -ENOMEM;
		IMSG_ERROR("Err: failed in creating device %x.\n", result);
		goto class_destroy;
	}

	ut_tui_devp = NULL;
	ut_tui_devp = kmalloc(sizeof(struct ut_tui_dev), GFP_KERNEL);
	if (ut_tui_devp == NULL) {
		result = -ENOMEM;
		goto device_destroy;
	}

	memset(ut_tui_devp, 0, sizeof(struct ut_tui_dev));
	add_cdev_result = ut_tui_setup_cdev(ut_tui_devp);
	if (add_cdev_result < 0) {
		result = -ENOMEM;
		IMSG_ERROR("Err: failed add char_reg_setup_cdev %x\n",
					add_cdev_result);
		goto ut_tui_devp_destroy;
	}

	TZ_SEMA_INIT_1(&ut_tui_devp->sem);
	goto return_fn;

	cdev_del(&ut_tui_devp->cdev);
ut_tui_devp_destroy:
	kfree(ut_tui_devp);
device_destroy:
	device_destroy(driver_class, devno);
class_destroy:
	class_destroy(driver_class);
unregister_chrdev_region:
	unregister_chrdev_region(devno, 1);
return_fn:
	return result;
}

void ut_tui_exit(void)
{
	if (driver_class != NULL)
		device_destroy(driver_class, devno);

	if (class_dev != NULL)
		class_destroy(driver_class);

	if (ut_tui_devp != NULL) {
		cdev_del(&ut_tui_devp->cdev);
		kfree(ut_tui_devp);
	}
	unregister_chrdev_region(devno, count);
}

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Microtrust");
MODULE_DESCRIPTION("TUI Agent");
MODULE_VERSION("1.00");

module_init(ut_tui_init);
module_exit(ut_tui_exit);
