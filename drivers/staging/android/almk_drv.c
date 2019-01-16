#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/xlog.h>

#include <asm/io.h>

#include "almk_drv.h"

#define pr_fmt(fmt) "["KBUILD_MODNAME"]"fmt
#include <linux/module.h>

/* #define USE_SYSRAM */
/* #define ALMK_MSG(...)   xlog_printk(ANDROID_LOG_DEBUG, "xlog/almk", __VA_ARGS__) */
/* #define ALMK_WRN(...)   xlog_printk(ANDROID_LOG_WARN,  "xlog/almk", __VA_ARGS__) */
/* #define ALMK_ERR(...)   xlog_printk(ANDROID_LOG_ERROR, "xlog/almk", __VA_ARGS__) */
#define ALMK_MSG pr_debug
#define ALMK_WRN pr_warning
#define ALMK_ERR pr_err
#define ALMK_DEVNAME "mtk_almk"


#define ALMK_PROCESS 0x2



/* -------------------------------------------------------------------------- */
/*  */
/* -------------------------------------------------------------------------- */

/* global function */
static unsigned int _almk_int_status;

/* device and driver */
static dev_t almk_devno;
static struct cdev *almk_cdev;
static struct class *almk_class;


/* static wait_queue_head_t enc_wait_queue; */
static spinlock_t almk_lock;
static int almk_status;




#if 0
static int check_all_minfree(void *param, void *param2)
{
#if 1
	extern int get_min_free_pages(pid_t pid);
	struct task_struct *p = 0;
	int n = 4096 * 170000;
	int nr_pages = (n / PAGE_SIZE) + ((n % PAGE_SIZE) ? 1 : 0);
	int free_pages = global_page_state(NR_FREE_PAGES) +
	    global_page_state(NR_FILE_PAGES) + global_page_state(NR_FILE_DIRTY);
	ALMK_WRN(KERN_ALERT "%s\n", __func__);
	ALMK_WRN(KERN_ALERT "=====================================\n");
	for_each_process(p) {
		/* get_min_free_pages(p->pid); */
		ALMK_WRN(KERN_ALERT "trying to alloc %d bytes (%d pages)\n"
			 "(NR_FREE_PAGES) + "
			 "(NR_FILE_PAGES) + "
			 "(NR_FILE_DIRTY) - "
			 "nr_pages = (%d + %d + %d - %d) = %d\n"
			 "target_min_free_pages = %d\n",
			 n, nr_pages,
			 global_page_state(NR_FREE_PAGES),
			 global_page_state(NR_FILE_PAGES),
			 global_page_state(NR_FILE_DIRTY),
			 nr_pages, free_pages - nr_pages, get_min_free_pages(p->pid));
		ALMK_WRN(KERN_ALERT "allocation is %s\n",
			 (free_pages - nr_pages >= get_min_free_pages(p->pid)) ?
			 "safe" : "not safe");
	}
#endif
}
#endif

static unsigned int get_max_safe_size(pid_t pid)
{
	extern int get_min_free_pages(pid_t pid);
	extern int query_lmk_minfree(int index);

	unsigned int all_free_pages;

	unsigned int lmk_pages;

	unsigned int lowBoundPages = get_min_free_pages(pid);

	unsigned int max_safe_size;

	lmk_pages = query_lmk_minfree(0);


	all_free_pages = global_page_state(NR_FREE_PAGES) +
	    global_page_state(NR_FILE_PAGES) + global_page_state(NR_FILE_DIRTY);


	if (all_free_pages >= (lowBoundPages + lmk_pages)) {
		max_safe_size = (all_free_pages - lowBoundPages - lmk_pages) * PAGE_SIZE;
	} else if (all_free_pages >= (lowBoundPages)) {
		max_safe_size = (all_free_pages - lowBoundPages) * PAGE_SIZE;
	} else
		return 0;



	return max_safe_size;

}



static int almk_ioctl(unsigned int cmd, unsigned long arg, struct file *file)
{
	ALMK_DRV_DATA drv_data;
	unsigned int max_safe_size;

	unsigned int *pStatus;

	pStatus = (unsigned int *)file->private_data;

	if (NULL == pStatus) {
		ALMK_WRN("Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
		return -EFAULT;
	}

	switch (cmd) {

		/* initial and reset ALMK */
	case ALMK_IOCTL_CMD_INIT:
		ALMK_MSG("ALMK Driver Initial and Lock\n");

		*pStatus = ALMK_PROCESS;

		break;

	case ALMK_IOCTL_CMD_GET_MAX_SIZE:
		ALMK_MSG("ALMK Driver GET_MAX_SIZE!!\n");
		if (*pStatus != ALMK_PROCESS) {
			ALMK_WRN("Permission Denied! This process can not access ALMK Driver");
			return -EFAULT;
		}


		if (copy_from_user(&drv_data, (void *)arg, sizeof(ALMK_DRV_DATA))) {
			ALMK_WRN("ALMK Driver : Copy from user error\n");
			return -EFAULT;
		}

		max_safe_size = get_max_safe_size(drv_data.pid);


		if (copy_to_user(drv_data.maxSafeSize, &max_safe_size, sizeof(unsigned int))) {
			ALMK_WRN("ALMK Driver : Copy to user error (result)\n");
			return -EFAULT;
		}
		break;


	case ALMK_IOCTL_CMD_DEINIT:
		/* copy input parameters */
		ALMK_MSG("ALMK Driver Deinit!!\n");
		if (*pStatus != ALMK_PROCESS) {
			ALMK_WRN("Permission Denied! This process can not access ALMK Driver");
			return -EFAULT;
		}

		*pStatus = 0;

		return 0;

	}

	return 0;
}

/* -------------------------------------------------------------------------- */
/*  */
/* -------------------------------------------------------------------------- */
/* static int almk_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) */
static long almk_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {

	case ALMK_IOCTL_CMD_INIT:
	case ALMK_IOCTL_CMD_GET_MAX_SIZE:
	case ALMK_IOCTL_CMD_DEINIT:
		return almk_ioctl(cmd, arg, file);

	default:
		break;
	}

	return -EINVAL;
}

static int almk_open(struct inode *inode, struct file *file)
{
	unsigned int *pStatus;
	/* Allocate and initialize private data */
	file->private_data = kmalloc(sizeof(unsigned int), GFP_ATOMIC);

	if (NULL == file->private_data) {
		ALMK_WRN("Not enough entry for ALMK open operation\n");
		return -ENOMEM;
	}

	pStatus = (unsigned int *)file->private_data;
	*pStatus = 0;

	return 0;
}

static ssize_t almk_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	ALMK_MSG("almk driver read\n");
	return 0;
}

static int almk_release(struct inode *inode, struct file *file)
{
	if (NULL != file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}
	return 0;
}

static int almk_flush(struct file *a_pstFile, fl_owner_t a_id)
{
	unsigned int *pStatus;

	pStatus = (unsigned int *)a_pstFile->private_data;

	if (NULL == pStatus) {
		ALMK_WRN("Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
		return -EFAULT;
	}


	return 0;
}

/* Kernel interface */
static struct file_operations almk_fops = {
	.owner = THIS_MODULE,
	/* .ioctl                = almk_ioctl, */
	.unlocked_ioctl = almk_unlocked_ioctl,
	.open = almk_open,
	.release = almk_release,
	.flush = almk_flush,
	.read = almk_read,
};

static int almk_probe(struct platform_device *pdev)
{
	struct class_device;

	int ret;
	struct class_device *class_dev = NULL;

	ALMK_MSG("-------------almk driver probe-------\n");
	ret = alloc_chrdev_region(&almk_devno, 0, 1, ALMK_DEVNAME);

	if (ret) {
		ALMK_ERR("Error: Can't Get Major number for ALMK Device\n");
	} else {
		ALMK_MSG("Get ALMK Device Major number (%d)\n", almk_devno);
	}

	almk_cdev = cdev_alloc();
	almk_cdev->owner = THIS_MODULE;
	almk_cdev->ops = &almk_fops;

	ret = cdev_add(almk_cdev, almk_devno, 1);

	almk_class = class_create(THIS_MODULE, ALMK_DEVNAME);
	class_dev =
	    (struct class_device *)device_create(almk_class, NULL, almk_devno, NULL, ALMK_DEVNAME);

	spin_lock_init(&almk_lock);

	/* initial driver, register driver ISR */
	almk_status = 0;
	_almk_int_status = 0;

	ALMK_MSG("ALMK Probe Done\n");

	/* NOT_REFERENCED(class_dev); */
	return 0;
}

static int almk_remove(struct platform_device *pdev)
{
	ALMK_MSG("ALMK driver remove\n");
	ALMK_MSG("Done\n");
	return 0;
}

static void almk_shutdown(struct platform_device *pdev)
{
	ALMK_MSG("ALMK driver shutdown\n");
	/* Nothing yet */
}

/* PM suspend */
static int almk_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	/* almk_drv_dec_deinit(); */
	/* almk_drv_enc_deinit(); */
	return 0;
}

/* PM resume */
static int almk_resume(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver almk_driver = {
	.probe = almk_probe,
	.remove = almk_remove,
	.shutdown = almk_shutdown,
	.suspend = almk_suspend,
	.resume = almk_resume,
	.driver = {
		   .name = ALMK_DEVNAME,
		   },
};

static void almk_device_release(struct device *dev)
{
	/* Nothing to release? */
}

static u64 jpegdec_dmamask = ~(u32) 0;

static struct platform_device almk_device = {
	.name = ALMK_DEVNAME,
	.id = 0,
	.dev = {
		.release = almk_device_release,
		.dma_mask = &jpegdec_dmamask,
		.coherent_dma_mask = 0xffffffff,
		},
	.num_resources = 0,
};

static int __init almk_init(void)
{
	int ret;

	ALMK_MSG("ALMK driver initialize\n");

	ALMK_MSG("Register the ALMK driver device\n");
	if (platform_device_register(&almk_device)) {
		ALMK_ERR("failed to register jpeg driver device\n");
		ret = -ENODEV;
		return ret;
	}

	ALMK_MSG("Register the ALMK driver\n");
	if (platform_driver_register(&almk_driver)) {
		ALMK_ERR("failed to register jpeg driver\n");
		platform_device_unregister(&almk_device);
		ret = -ENODEV;
		return ret;
	}

	return 0;
}

static void __exit almk_exit(void)
{
	cdev_del(almk_cdev);
	unregister_chrdev_region(almk_devno, 1);
	/* ALMK_MSG("Unregistering driver\n"); */
	platform_driver_unregister(&almk_driver);
	platform_device_unregister(&almk_device);

	device_destroy(almk_class, almk_devno);
	class_destroy(almk_class);

	ALMK_MSG("Done\n");
}
module_init(almk_init);
module_exit(almk_exit);
MODULE_AUTHOR("Otis, Huang <otis.huang@mediatek.com>");
MODULE_DESCRIPTION("ALMK driver");
MODULE_LICENSE("GPL");
