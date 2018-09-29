/**
 * Copyright Elliptic Labs
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/mm.h>


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
/*  includes the file structure, that is, file open read close */
#include <linux/fs.h>

/* include the character device, makes cdev avilable */
#include <linux/cdev.h>
#include <linux/semaphore.h>

/* includes copy_user vice versa */
#include <linux/uaccess.h>

#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/device.h>


#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <linux/errno.h>

#include "elliptic_data_io.h"
#include "elliptic_device.h"


static dev_t elliptic_usermode_io_major;
#define USERMODE_IO_DEVICE_NAME "elliptic_um_io"
#define UM_IO_PING_PONG_BUFFER_SIZE 512
struct elliptic_usermode_io_device {
	struct cdev cdev;
	struct semaphore sem;
	struct mutex lock;
	int ping_pong_idx;
	char tmp_buffer[2][UM_IO_PING_PONG_BUFFER_SIZE];
};

static struct elliptic_usermode_io_device io_device;

static int device_open(struct inode *inode, struct file *filp)
{
	if (inode->i_cdev != &io_device.cdev) {
		pr_warn("elliptic : dev pointer mismatch\n");
		return -ENODEV; /* No such device */
	}

	if (down_interruptible(&io_device.sem) != 0)
		return -EEXIST;
	return 0;
}

static ssize_t device_write(struct file *fp, const char __user *buff,
	size_t length, loff_t *ppos)
{
	size_t write_len;
	size_t copy_result;
	int push_result;
	int err;

	err = mutex_lock_interruptible(&io_device.lock);
	if (err == -EINTR) {
		pr_warn("%s : locking interrupted\n", __func__);
		return 0;
	} else if (err < 0) {
		pr_warn("%s : failed to acquire lock\n", __func__);
		return 0;
	}

	write_len = min_t(size_t, UM_IO_PING_PONG_BUFFER_SIZE, length);
	copy_result = copy_from_user(
		io_device.tmp_buffer[io_device.ping_pong_idx], buff, write_len);

	if (copy_result > 0) {
		mutex_unlock(&io_device.lock);
		return (write_len - copy_result);
	}

	push_result = elliptic_data_push(
		io_device.tmp_buffer[io_device.ping_pong_idx], write_len);

	if (push_result) {
		pr_err("elliptic: %s failed to push fifo data\n", __func__);
		mutex_unlock(&io_device.lock);
		return 0;
	}
	io_device.ping_pong_idx = 1 - io_device.ping_pong_idx;
	mutex_unlock(&io_device.lock);
	return write_len;
}

static int device_close(struct inode *inode, struct file *filp)
{
	up(&io_device.sem);
	return 0;
}

static const struct file_operations
elliptic_usermode_io_fops = {
	.owner		= THIS_MODULE,
	.open		= device_open,
	.write		= device_write,
	.release	= device_close,
};

static int elliptic_usermode_io_driver_init(void)
{
	struct device *device;
	dev_t device_number;
	int err;

	err = alloc_chrdev_region(
		&device_number, 0, 1, USERMODE_IO_DEVICE_NAME);

	if (err < 0) {
		pr_err("failed to allocate chrdev region\n");
		return err;
	}

	elliptic_usermode_io_major = MAJOR(device_number);
	device_number = MKDEV(elliptic_usermode_io_major, 0);
	device = device_create(
		elliptic_class, NULL, device_number,
		NULL, USERMODE_IO_DEVICE_NAME);

	if (IS_ERR(device)) {
		unregister_chrdev(
			elliptic_usermode_io_major, USERMODE_IO_DEVICE_NAME);
		pr_err("Failed to create the device\n");
		return PTR_ERR(device);
	}

	cdev_init(&io_device.cdev, &elliptic_usermode_io_fops);
	io_device.cdev.owner = THIS_MODULE;
	err = cdev_add(&io_device.cdev, device_number, 1);
	if (err) {
		pr_warn("elliptic : error %d while trying to add %s%d",
			err, ELLIPTIC_DEVICENAME, 0);
		return err;
	}

	sema_init(&io_device.sem, 1);
	mutex_init(&io_device.lock);
	return 0;
}

static void elliptic_usermode_io_driver_exit(void)
{
	BUG_ON(elliptic_class == NULL);
	device_destroy(elliptic_class, MKDEV(elliptic_usermode_io_major, 0));
	cdev_del(&io_device.cdev);
	unregister_chrdev(elliptic_usermode_io_major, USERMODE_IO_DEVICE_NAME);
	up(&io_device.sem);
}


int elliptic_data_io_initialize(void)
{
	elliptic_usermode_io_driver_init();
	return 0;
}

int elliptic_data_io_cleanup(void)
{
	elliptic_usermode_io_driver_exit();
	return 0;
}

void elliptic_data_io_cancel(void)
{

}

size_t elliptic_data_io_write(uint32_t message_id, const char *data,
	size_t data_size)
{
	return 0;
}

size_t elliptic_data_io_transact(uint32_t message_id, const char *data,
	size_t data_size, char *output_data, size_t output_data_size)
{
	return 0;
}
