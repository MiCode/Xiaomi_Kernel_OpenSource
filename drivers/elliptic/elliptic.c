/**
 * Copyright Elliptic Labs
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 */

#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/version.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

/*  includes the file structure, that is, file open read close */
#include<linux/fs.h>

/* include the character device, makes cdev avilable */
#include<linux/cdev.h>
#include<linux/semaphore.h>

/* includes copy_user vice versa */
#include<linux/uaccess.h>

#include<linux/stat.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/device.h>

#include <linux/pm_wakeup.h>

#include "elliptic_device.h"
#include <sound/apr_elliptic.h>

#define ELLIPTIC_PROC_CALIBRATION_DATA_SIZE 64

/**
 * open the character device
 */
static int device_open(struct inode *, struct file *);

/**
 * Close the character device
 */
static int device_close(struct inode *, struct file *);

/**
 * Read data from the device
 */
static ssize_t device_read(struct file *, char *, size_t, loff_t *);

/**
 * Write data to; the device
 */
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);

/**
 * IOCTL mechanism for driver..
 */
static long device_ioctl(struct file *fp, unsigned int number,
	unsigned long param);

struct semaphore sem;

/* Name of the registered char driver */
struct cdev *mcdev;

/* Global variable for the device class */
static struct class *cl;

/* Major number provided by the kernel */
dev_t dev_num;

/* Major number extracted by dev_t */
int major_number;

int ret;

/* inode refers to the actual file on disk */
static int device_open(struct inode *inode, struct file *filp)
{
	if (down_interruptible(&sem) != 0) {
		pr_err("elliptic : the device has been opened by some other device, unable to open lock\n");
		return -EPERM;
	}
	pr_debug("elliptic : Device opened\n");
	return 0;
}

/**
 *
 * @return Number of bytes read.
 */
static ssize_t device_read(struct file *fp, char *buff, size_t length,
	loff_t *ppos)
{
	int bytes_read = 0;

	bytes_read =
		elliptic_data_io(ELLIPTIC_ULTRASOUND_GET_PARAMS, ELLIPTIC_PORT_ID, buff,
		length);

	return bytes_read;
}

/**
 *
 * @return number of bytes actually written
 */
static ssize_t device_write(struct file *fp, const char *buff, size_t length,
	loff_t *ppos)
{
	int bytes_written = 0;

	if ((buff != NULL) && (length != 0)) {
		bytes_written =
			elliptic_data_io(ELLIPTIC_ULTRASOUND_SET_PARAMS, ELLIPTIC_PORT_ID,
			(char *) buff, length);
	}

	return bytes_written;
}

static long device_ioctl(struct file *fp, unsigned int number,
	unsigned long param)
{
	unsigned int mirror_tag, mirror_payload_size;
	unsigned char *data_ptr;

	switch (number) {
	case IOCTL_ELLIPTIC_ACTIVATE_ENGINE:
		pr_debug(" elliptic : (w) IOCTL_ELLIPTIC_ACTIVATE_ENGINE %ld\n",
			param);
		break;
	case IOCTL_ELLIPTIC_DATA_IO_CANCEL:
		pr_debug(" elliptic : () IOCTL_ELLIPTIC_CANCEL_READ %ld\n",
			param);
		elliptic_data_io_cancel();
		break;
	case IOCTL_ELLIPTIC_DATA_IO_MIRROR:
		data_ptr = (unsigned char *) param;
		mirror_tag = *(unsigned int *) data_ptr;
		mirror_payload_size = *((unsigned int *) data_ptr + 1);
		if ((mirror_tag == MIRROR_TAG) &&
			(mirror_payload_size != 0) &&
			(mirror_payload_size <= (ELLIPTIC_SET_PARAMS_SIZE * 4))) {
			pr_debug(" elliptic : () IOCTL_ELLIPTIC_DATA_IO_MIRROR Tag=%x, len=%d\n",
				mirror_tag, mirror_payload_size);
			elliptic_data_io(ELLIPTIC_ULTRASOUND_SET_PARAMS, ELLIPTIC_PORT_ID,
				(data_ptr + 8), mirror_payload_size);
		} else {
			pr_debug(" elliptic : () IOCTL_ELLIPTIC_DATA_IO_MIRROR, TAG or Length is not valid\n");
		}
		break;
	default:
		pr_debug(" elliptic : () UNKNOWN IOCTL number=%d\n", number);
		break;
	}

	return 0;
}

static int device_close(struct inode *inode, struct file *filp)
{
	up(&sem);
	pr_debug("elliptic : Device closed\n");
	return ret;
}

extern void get_elliptic_engine_version(uint32_t *mjr, uint32_t *min, uint32_t *bld, uint32_t *rev);

extern int get_elliptic_calibration_data(uint8_t *caldata, uint32_t max_size);


static int elliptic_version_proc_show(struct seq_file *m, void *v)
{
	uint32_t major, minor, build, revision;

	get_elliptic_engine_version(&major, &minor, &build, &revision);
	seq_printf(m, "%d.%d.%d.%d\n", major, minor, build, revision);
	return 0;
}

static int elliptic_version_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, elliptic_version_proc_show, NULL);
}

static const struct file_operations elliptic_version_proc_fops = {
	.owner = THIS_MODULE,
	.open = elliptic_version_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int elliptic_calibration_data_proc_show(struct seq_file *m, void *v)
{
	int i = 0;
	uint8_t caldata[ELLIPTIC_PROC_CALIBRATION_DATA_SIZE];

	get_elliptic_calibration_data(caldata, ELLIPTIC_PROC_CALIBRATION_DATA_SIZE);
	for (i = 0; i < ELLIPTIC_PROC_CALIBRATION_DATA_SIZE; i++)
		seq_printf(m, "0x%02x ", caldata[i]);
	seq_printf(m, "\n");
	return 0;
}

static int elliptic_calibration_data_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, elliptic_calibration_data_proc_show, NULL);
}

static const struct file_operations elliptic_calibration_data_proc_fops = {
	.owner = THIS_MODULE,
	.open = elliptic_calibration_data_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int elliptic_proc_init(void)
{
	proc_create("elliptic_engine_calib", 0, NULL, &elliptic_calibration_data_proc_fops);
	proc_create("elliptic_engine_version", 0, NULL, &elliptic_version_proc_fops);
	return 0;
}

static void elliptic_proc_exit(void)
{
	remove_proc_entry("elliptic_engine_calib", NULL);
	remove_proc_entry("elliptic_engine_version", NULL);
}

/* defines the file operations provided by the driver */
const struct file_operations fops_elliptic = {
	.owner = THIS_MODULE,		/* prevents unloading when operations are in use */
	.open = device_open,		/*to open the device */
	.write = device_write,		/*to write to the device */
	.read = device_read,		/*to read the device */
	.unlocked_ioctl = device_ioctl,	/* IOCTL calls */
	.release = device_close,	/*to close the device */
};

static struct wakeup_source g_elus_wake_source;

int chardev_init(void)
{
	wakeup_source_init(&g_elus_wake_source, "elus_wakeup_source");

	/* we will get the major number dynamically this is recommended please read ldd3 */
	ret = alloc_chrdev_region(&dev_num, 0, 1, ELLIPTIC_DEVICENAME);
	if (ret < 0) {
		pr_debug(" elliptic : Failed to allocate major number\n");
		return ret;
	}

	major_number = MAJOR(dev_num);
	/* create, allocate and initialize our cdev structure */
	mcdev = cdev_alloc();
	/* configure the supported file operations */
	mcdev->ops = &fops_elliptic;
	mcdev->owner = THIS_MODULE;

	cl = class_create(THIS_MODULE, "chardev");
	if (cl == NULL) {
		pr_debug(" elliptic : Class creation failed\n");
		unregister_chrdev_region(dev_num, 1);
		return -EPERM;
	}

	if (device_create(cl, NULL, dev_num, NULL, "elliptic0") == NULL) {
		pr_debug(" elliptic : Device creation failed\n");
		class_destroy(cl);
		unregister_chrdev_region(dev_num, 1);
		return -EPERM;
	}

	/*we have created and initialized our cdev structure now we need to add it to the kernel */
	ret = cdev_add(mcdev, dev_num, 1);
	if (ret < 0) {
		pr_debug("elliptic : device adding to the kerknel failed\n");
		class_destroy(cl);
		unregister_chrdev_region(dev_num, 1);
		return ret;
	}
	sema_init(&sem, 1);			/* initial value to one */
	elliptic_proc_init();
	return 0;
}

void elliptic_keep_sensor_system_awake(void)
{
	__pm_wakeup_event(&g_elus_wake_source, 250);
}

void chardev_exit(void)
{
/* Removing the structure added previously */
	elliptic_proc_exit();
	cdev_del(mcdev);
	class_destroy(cl);
	unregister_chrdev_region(dev_num, 1);

	wakeup_source_trash(&g_elus_wake_source);
}

MODULE_AUTHOR("Elliptic Labs");
MODULE_DESCRIPTION("Providing Interface to get/set param");
MODULE_LICENSE("GPL");

module_init(chardev_init);
module_exit(chardev_exit);
