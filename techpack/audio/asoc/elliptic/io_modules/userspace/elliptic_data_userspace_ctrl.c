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

#include <elliptic/elliptic_data_io.h>
#include "elliptic_device.h"


static dev_t elliptic_userspace_ctrl_major;
#define USERSPACE_CTRL_IO_DEVICE_NAME "elliptic_us_ctrl_io"
struct elliptic_userspace_ctrl_device {
	struct cdev cdev;
	struct semaphore sem;
	int ping_pong_idx;
	size_t ping_pong_buffer_size[2];
	uint8_t ping_pong_buffer[2][ELLIPTIC_MSG_BUF_SIZE];
	wait_queue_head_t data_available;
	struct mutex data_lock;
	atomic_t data_state;
};



static struct elliptic_userspace_ctrl_device ctrl_device;

static uint8_t *get_ping_buffer(struct elliptic_userspace_ctrl_device *dev,
	/*out parameter*/ size_t *data_size)
{
	if (data_size != NULL)
		*data_size = dev->ping_pong_buffer_size[dev->ping_pong_idx];

	return dev->ping_pong_buffer[dev->ping_pong_idx];
}

static uint8_t *get_pong_buffer(struct elliptic_userspace_ctrl_device *dev,
/*out parameter*/ size_t *data_size)
{
	if (data_size != NULL)
		*data_size = dev->ping_pong_buffer_size[1 - dev->ping_pong_idx];

	return dev->ping_pong_buffer[1 - dev->ping_pong_idx];
}


static void set_pong_buffer_size(struct elliptic_userspace_ctrl_device *dev,
	size_t data_size)
{
	dev->ping_pong_buffer_size[1 - dev->ping_pong_idx] = data_size;
}

static void swap_ping_pong(struct elliptic_userspace_ctrl_device *dev)
{
	dev->ping_pong_idx = 1 - dev->ping_pong_idx;
}

static int device_open(struct inode *inode, struct file *filp)
{
	if (inode->i_cdev != &ctrl_device.cdev) {
		pr_warn("elliptic : dev pointer mismatch\n");
		return -ENODEV; /* No such device */
	}

	if (down_interruptible(&ctrl_device.sem) != 0)
		return -EEXIST;
	EL_PRINT_I("Opened device %s", USERSPACE_CTRL_IO_DEVICE_NAME);
	return 0;
}

static ssize_t device_read(struct file *fp, char __user *buff,
	size_t user_buf_length, loff_t *ppos)
{
	size_t bytes_read;
	unsigned long copy_result;
	uint8_t *ping_buffer;
	int result;

	if (user_buf_length < ELLIPTIC_MSG_BUF_SIZE)
		EL_PRINT_E("user space buffer user_buf_length too small : %zu",
		user_buf_length);

	bytes_read = 0;
	copy_result = 0;
	ping_buffer = NULL;

	result = wait_event_interruptible(ctrl_device.data_available,
		atomic_read(&ctrl_device.data_state) != 0);
	if (result == 0) {
		const int state = atomic_read(&ctrl_device.data_state);

		if (state > 0) {
			result = mutex_lock_interruptible(
				&ctrl_device.data_lock);
			if (result == 0) {
				swap_ping_pong(&ctrl_device);

				ping_buffer = get_ping_buffer(
					&ctrl_device, &bytes_read);

				if (bytes_read > user_buf_length) {
					EL_PRINT_E(
						"ping buffer size %zu larger than user buffer",
						bytes_read);
					goto fail;
				}

				copy_result = copy_to_user(buff, ping_buffer,
					bytes_read);
				if (copy_result > 0) {
					EL_PRINT_E("Failed copy to user");
					goto fail;
				}

				atomic_set(&ctrl_device.data_state, 0);

				mutex_unlock(&ctrl_device.data_lock);
			} else if (result == -EINTR) {
				EL_PRINT_E("lock interrupted");
			} else {
				EL_PRINT_E("lock error = %d", result);
			}
		} else {
			EL_PRINT_W("state = %d", state);
			atomic_set(&ctrl_device.data_state, 0);
		}
	} else if (result == -ERESTARTSYS) {
		EL_PRINT_E("interrupted");
	} else {
		EL_PRINT_E("wait_event error = %d", result);
	}
	return bytes_read;
fail:
	atomic_set(&ctrl_device.data_state, 0);
	mutex_unlock(&ctrl_device.data_lock);
	return 0;
}

static int device_close(struct inode *inode, struct file *filp)
{
	up(&ctrl_device.sem);
	EL_PRINT_I("Closed device %s", USERSPACE_CTRL_IO_DEVICE_NAME);
	return 0;
}

static const struct file_operations
elliptic_userspace_ctrl_fops = {
	.owner      = THIS_MODULE,
	.open       = device_open,
	.read       = device_read,
	.release    = device_close,
};

int elliptic_userspace_ctrl_driver_init(void)
{
	struct device *device;
	dev_t device_number;
	int err;

	err = alloc_chrdev_region(
		&device_number, 0, 1, USERSPACE_CTRL_IO_DEVICE_NAME);

	if (err < 0) {
		pr_err("failed to allocate chrdev region\n");
		return err;
	}

	elliptic_userspace_ctrl_major = MAJOR(device_number);

	device_number = MKDEV(elliptic_userspace_ctrl_major, 0);
	device = device_create(
		elliptic_class, NULL, device_number,
		NULL, USERSPACE_CTRL_IO_DEVICE_NAME);

	if (IS_ERR(device)) {
		unregister_chrdev(
			elliptic_userspace_ctrl_major,
			USERSPACE_CTRL_IO_DEVICE_NAME);
		EL_PRINT_E("Failed to create the device\n");
		return PTR_ERR(device);
	}

	cdev_init(&ctrl_device.cdev, &elliptic_userspace_ctrl_fops);
	ctrl_device.cdev.owner = THIS_MODULE;
	err = cdev_add(&ctrl_device.cdev, device_number, 1);
	if (err) {
		EL_PRINT_W("error %d while trying to add %s%d",
			err, ELLIPTIC_DEVICENAME, 0);
		return err;
	}

	sema_init(&ctrl_device.sem, 1);
	mutex_init(&ctrl_device.data_lock);
	init_waitqueue_head(&ctrl_device.data_available);
	return 0;
}

void elliptic_userspace_ctrl_driver_exit(void)
{
	BUG_ON(elliptic_class == NULL);
	device_destroy(elliptic_class, MKDEV(elliptic_userspace_ctrl_major, 0));
	cdev_del(&ctrl_device.cdev);
	unregister_chrdev(elliptic_userspace_ctrl_major,
		USERSPACE_CTRL_IO_DEVICE_NAME);
	up(&ctrl_device.sem);
}

int32_t elliptic_userspace_ctrl_write(uint32_t message_id,
	const char *data, size_t data_size){
	uint8_t *pong_buffer;

	if (data_size > ELLIPTIC_MSG_BUF_SIZE) {
		EL_PRINT_E("data size : %zu larger than buf size : %zu",
			data_size, (size_t)ELLIPTIC_MSG_BUF_SIZE);

		return -EINVAL;
	}
	mutex_lock(&ctrl_device.data_lock);

	pong_buffer = get_pong_buffer(&ctrl_device, NULL);
	set_pong_buffer_size(&ctrl_device, data_size);

	memcpy(pong_buffer, data, data_size);
	wake_up_interruptible(&ctrl_device.data_available);
	atomic_set(&ctrl_device.data_state, 1);
	mutex_unlock(&ctrl_device.data_lock);

	return 0;
}


