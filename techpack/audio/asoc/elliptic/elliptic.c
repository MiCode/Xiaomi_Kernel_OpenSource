/**
* Copyright Elliptic Labs
*
*/
/* #define DEBUG*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
/*  includes the file structure, that is, file open read close */
#include <linux/fs.h>

#include <asoc/apr_elliptic.h>

/* include the character device, makes cdev avilable */
#include <linux/cdev.h>
#include <linux/semaphore.h>

/* includes copy_user vice versa */
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/device.h>

#include <linux/pm_wakeup.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/kobject.h>

#include "elliptic_sysfs.h"
#include "elliptic_device.h"
#include "elliptic_data_io.h"

static struct elliptic_device *elliptic_devices;

/* Global variable for the device class*/
struct class *elliptic_class;

typedef uint32_t el_fifo_size_t;

/* Major number provided by the kernel*/
static dev_t elliptic_major;

static struct wakeup_source *wake_source;


void elliptic_data_reset_debug_counters(struct elliptic_data
	*elliptic_data) {

	elliptic_data->isr_fifo_flush_count = 0;
	elliptic_data->userspace_fifo_flush_count = 0;
}

void elliptic_data_print_debug_counters(struct elliptic_data
	*elliptic_data) {

	if (elliptic_data->isr_fifo_flush_count > 0) {
		EL_PRINT_E("isr fifo flushed %u times",
			elliptic_data->isr_fifo_flush_count);
	}

	if (elliptic_data->userspace_fifo_flush_count > 0) {
		EL_PRINT_E("userspace fifo flushed %u times",
			elliptic_data->userspace_fifo_flush_count);
	}

	if (elliptic_data->userspace_read_total !=
		elliptic_data->isr_write_total) {
		EL_PRINT_I("user space reads / isr writes : %u / %u",
			elliptic_data->userspace_read_total,
			elliptic_data->isr_write_total);
	}

	EL_PRINT_I(
		"total isr fifo flushed count : %u",
		elliptic_data->isr_fifo_flush_count_total);

	EL_PRINT_I("total userspace fifo flushed count : %u",
		elliptic_data->userspace_fifo_flush_count_total);


}

void elliptic_data_update_debug_counters(struct elliptic_data
	*elliptic_data)
{
	elliptic_data->isr_fifo_flush_count_total +=
	 elliptic_data->isr_fifo_flush_count;
	elliptic_data->userspace_fifo_flush_count_total +=
	 elliptic_data->userspace_fifo_flush_count;
}


/* spin lock for isr must be held prior to calling */
static void elliptic_data_flush_isr_fifo(struct elliptic_data
	*elliptic_data)
{
	kfifo_reset(&elliptic_data->fifo_isr);
}

/* mutex lock for user space copy must be held prior to calling */
static void elliptic_data_flush_userspace_fifo(struct elliptic_data
	*elliptic_data)
{
	kfifo_reset(&elliptic_data->fifo_userspace);
}

/* inode refers to the actual file on disk */
static int device_open(struct inode *inode, struct file *filp)
{
	unsigned int major;
	unsigned int minor;
	struct elliptic_device *dev;
	struct elliptic_data *elliptic_data;

	major = imajor(inode);
	minor = iminor(inode);

	if (major != elliptic_major || minor < 0
		|| minor >= ELLIPTIC_NUM_DEVICES) {
		EL_PRINT_W("no device found with minor=%d and major=%d",
			major, minor);
		return -ENODEV; /* No such device */
	}

	dev = NULL;
	dev = &elliptic_devices[minor];
	filp->private_data = dev;

	if (inode->i_cdev != &dev->cdev) {
		EL_PRINT_W("dev pointer mismatch");
		return -ENODEV; /* No such device */
	}

	if (down_interruptible(&dev->sem) != 0) {
		EL_PRINT_E("the device has been opened, unable to open lock");
		return -EINVAL;
	}

	elliptic_data = &dev->el_data;
	spin_lock(&elliptic_data->fifo_isr_spinlock);
	elliptic_data_flush_isr_fifo(elliptic_data);
	spin_unlock(&elliptic_data->fifo_isr_spinlock);

	mutex_lock(&elliptic_data->fifo_usp_lock);
	elliptic_data_flush_userspace_fifo(elliptic_data);
	mutex_unlock(&elliptic_data->fifo_usp_lock);

	elliptic_data_reset_debug_counters(elliptic_data);

	EL_PRINT_I("Opened device elliptic%u", minor);
	dev->opened = 1;
	return 0;
}

static void elliptic_data_work_handler(struct work_struct *ws)
{
	struct elliptic_data *elliptic_data;
	unsigned long flags;

	unsigned int fifo_result = 0;
	size_t available_space = 0;

	elliptic_data = container_of(ws, struct elliptic_data, work);

	if (kfifo_is_empty(&elliptic_data->fifo_isr)) {
		EL_PRINT_W("work handler called when isr fifo is empty");
		return;
	}

	mutex_lock(&elliptic_data->fifo_usp_lock);

	spin_lock_irqsave(&elliptic_data->fifo_isr_spinlock, flags);

	fifo_result = kfifo_out(&elliptic_data->fifo_isr,
		elliptic_data->isr_swap_buffer, ELLIPTIC_MSG_BUF_SIZE);
	if (fifo_result == 0) {
		EL_PRINT_E("failed to copy from fifo isr to swap buffer %u",
			fifo_result);
		goto fail;
	}

	available_space = kfifo_avail(&elliptic_data->fifo_userspace);

	if (available_space < ELLIPTIC_MSG_BUF_SIZE) {
		EL_PRINT_E("available_space %lu, entry_size %lu. Flushing fifo",
			available_space, (size_t)ELLIPTIC_MSG_BUF_SIZE);

		++elliptic_data->userspace_fifo_flush_count;
		elliptic_data_flush_userspace_fifo(elliptic_data);
		goto fail;
	}

	fifo_result = kfifo_in(&elliptic_data->fifo_userspace,
		elliptic_data->isr_swap_buffer, ELLIPTIC_MSG_BUF_SIZE);
	if (fifo_result == 0) {
		EL_PRINT_E("failed to copy from swap to fifo user space: %u",
			fifo_result);
		goto fail;
	}

	spin_unlock_irqrestore(&elliptic_data->fifo_isr_spinlock, flags);
	mutex_unlock(&elliptic_data->fifo_usp_lock);
	wake_up_interruptible(&elliptic_data->fifo_usp_not_empty);
	__pm_wakeup_event(wake_source, elliptic_data->wakeup_timeout);
	return;

fail:
	spin_unlock_irqrestore(&elliptic_data->fifo_isr_spinlock, flags);
	mutex_unlock(&elliptic_data->fifo_usp_lock);

}

#define WORK_QUEUE_HANDLER_NAME_LENGTH 64
int elliptic_data_initialize(struct elliptic_data
	*elliptic_data, size_t queue_size,
	unsigned int wakeup_timeout, int id)
{
	int is_power_of_two;

	char name[WORK_QUEUE_HANDLER_NAME_LENGTH] = {0};

	is_power_of_two = (queue_size != 0) && !(queue_size & (queue_size - 1));

	if (is_power_of_two != 1) {
		EL_PRINT_E("non power of 2 fifo size");
		return -EINVAL;
	}

	if (kfifo_alloc(&elliptic_data->fifo_isr,
		queue_size, GFP_KERNEL) != 0) {
		EL_PRINT_E("failed to allocate fifo isr");
		return -EINVAL;
	}

	if (kfifo_alloc(&elliptic_data->fifo_userspace,
		queue_size, GFP_KERNEL) != 0) {

		EL_PRINT_E("failed to allocate fifo user space");
		return -EINVAL;
	}


	atomic_set(&elliptic_data->abort_io, 0);
	spin_lock_init(&elliptic_data->fifo_isr_spinlock);

	INIT_WORK(&elliptic_data->work, elliptic_data_work_handler);
	mutex_init(&elliptic_data->fifo_usp_lock);
	init_waitqueue_head(&elliptic_data->fifo_usp_not_empty);

	snprintf(name, WORK_QUEUE_HANDLER_NAME_LENGTH,
		"%s_%d", "ELLIPTIC_DATA_WORK_HANDLER", id);
	elliptic_data->wq = create_singlethread_workqueue(name);

	return 0;
}

int elliptic_data_cleanup(struct elliptic_data *elliptic_data)
{
	spin_unlock(&elliptic_data->fifo_isr_spinlock);
	kfifo_free(&elliptic_data->fifo_isr);
	return 0;
}

size_t elliptic_data_pop(struct elliptic_data
	*elliptic_data, char __user *buffer, size_t buffer_size)
{
	int result;
	unsigned int num_copied;

	if (buffer_size < ELLIPTIC_MSG_BUF_SIZE) {
		EL_PRINT_E("buffer_size : %lu smaller than %lu",
		 buffer_size, (size_t)ELLIPTIC_MSG_BUF_SIZE);
		return 0;
	}

	result = wait_event_interruptible(elliptic_data->fifo_usp_not_empty,
			  (kfifo_is_empty(&elliptic_data->fifo_userspace) == 0)
			  || (atomic_read(&elliptic_data->abort_io) == 1));

	if (atomic_read(&elliptic_data->abort_io) == 1) {
		atomic_set(&elliptic_data->abort_io, 0);
		EL_PRINT_D("pop cancelled");
		return 0;
	}


	if (result == 0) {
		mutex_lock(&elliptic_data->fifo_usp_lock);

		num_copied = 0;
		result = kfifo_to_user(&elliptic_data->fifo_userspace, buffer,
					   ELLIPTIC_MSG_BUF_SIZE, &num_copied);

		if (result == -EFAULT) {
			EL_PRINT_E("failed kfifo_to_user");
			mutex_unlock(&elliptic_data->fifo_usp_lock);
			return 0;
		}

		mutex_unlock(&elliptic_data->fifo_usp_lock);
		++elliptic_data->userspace_read_total;

		if ((size_t)num_copied != ELLIPTIC_MSG_BUF_SIZE) {
			EL_PRINT_E("copied less than entry size : %u"
				, num_copied);
			return (size_t)num_copied;
		}
	} else {
		if (-ERESTARTSYS == result)
			EL_PRINT_D("wait interrupted");
		else
			EL_PRINT_E("wait error = %d", result);
	}

	return (size_t)ELLIPTIC_MSG_BUF_SIZE;
}

int elliptic_data_push(const char *buffer, size_t buffer_size)
{
	size_t available_space;
	size_t space_required;
	size_t zeros_to_pad;
	int err;
	int i;
	unsigned long flags;
	struct elliptic_device *device;
	struct elliptic_data *elliptic_data;
	unsigned int fifo_result;
	static uint8_t zero_pad_buffer[ELLIPTIC_MSG_BUF_SIZE];

	err = 0;
	fifo_result = 0;

	if (buffer_size > ELLIPTIC_MSG_BUF_SIZE) {
		EL_PRINT_E("buffer size %lu is larger than max buffer size %lu",
			buffer_size, (size_t)ELLIPTIC_MSG_BUF_SIZE);
		return -EINVAL;
	}


	zeros_to_pad = ELLIPTIC_MSG_BUF_SIZE - buffer_size;

	for (i = 0; i < ELLIPTIC_NUM_DEVICES; ++i) {
		device = &elliptic_devices[i];
		elliptic_data = &device->el_data;

		if ((!device->opened))
			continue;

		available_space = kfifo_avail(&elliptic_data->fifo_isr);
		space_required = ELLIPTIC_MSG_BUF_SIZE;

		spin_lock_irqsave(&elliptic_data->fifo_isr_spinlock, flags);

		if (available_space < space_required) {
			EL_PRINT_W("fifo space too small: %lu, flushing fifo",
				available_space);

			++elliptic_data->isr_fifo_flush_count;
			elliptic_data_flush_isr_fifo(elliptic_data);
		}

		fifo_result = kfifo_in(&elliptic_data->fifo_isr,
			buffer, buffer_size);
		if (fifo_result == 0) {
			EL_PRINT_W("failed to push buffer to fifo");
			spin_unlock_irqrestore(
				&elliptic_data->fifo_isr_spinlock, flags);
			continue;
		}

		if (zeros_to_pad > 0) {
			fifo_result = kfifo_in(
				&elliptic_data->fifo_isr, zero_pad_buffer,
				zeros_to_pad);
			if (fifo_result == 0) {
				EL_PRINT_W("zero pad failed, flushing fifo");
				spin_unlock_irqrestore(
					&elliptic_data->fifo_isr_spinlock,
					flags);

				++elliptic_data->isr_fifo_flush_count;
				elliptic_data_flush_isr_fifo(elliptic_data);
				continue;
			}
		}

		++elliptic_data->isr_write_total;
		spin_unlock_irqrestore(
			&elliptic_data->fifo_isr_spinlock, flags);

		queue_work(elliptic_data->wq, &elliptic_data->work);
	}

	return err;
}


/**
*
* @return Number of bytes read.
*/
static ssize_t device_read(struct file *fp, char __user *buff,
	size_t length, loff_t *ppos)
{
	ssize_t bytes_read = 0;
	struct elliptic_device *elliptic_device;
	struct elliptic_data *elliptic_data;

	elliptic_device = (struct elliptic_device *)fp->private_data;
	elliptic_data = (struct elliptic_data *)&elliptic_device->el_data;

	bytes_read = elliptic_data_pop(elliptic_data, buff, length);

	return bytes_read;
}

/**
*
* @return number of bytes actually written
*/
static ssize_t device_write(struct file *fp, const char *buff,
	size_t length, loff_t *ppos)
{
	ssize_t ret_val;

	ret_val = 0;
	if ((buff != NULL) && (length != 0))
		ret_val = elliptic_data_io_write(ELLIPTIC_ULTRASOUND_SET_PARAMS,
			buff, length);

	return ret_val >= 0 ? (ssize_t)length : 0;
}


static long device_ioctl(struct file *fp, unsigned int number,
	unsigned long param)
{
	struct elliptic_device *device;
	struct elliptic_data *elliptic_data;
	int err;
	unsigned int mirror_tag, mirror_payload_size;
	unsigned char *data_ptr;

	device = (struct elliptic_device *)(fp->private_data);
	elliptic_data = &device->el_data;

	switch (number) {
	case IOCTL_ELLIPTIC_DATA_IO_CANCEL:
		EL_PRINT_D("IOCTL_ELLIPTIC_CANCEL_READ %ld",
			 param);
		elliptic_data_io_cancel(elliptic_data);
		break;

	case IOCTL_ELLIPTIC_DATA_IO_MIRROR:
		data_ptr = (unsigned char *) param;
		mirror_tag = *(unsigned int *) data_ptr;
		mirror_payload_size = *((unsigned int *) data_ptr + 1);

		if ((mirror_tag == MIRROR_TAG) &&
			(mirror_payload_size != 0) &&
			(mirror_payload_size <=
			(ELLIPTIC_SET_PARAMS_SIZE * 4))) {

			err = elliptic_data_io_write(
				ELLIPTIC_ULTRASOUND_SET_PARAMS,
				(data_ptr + 8), mirror_payload_size);

			if (err != 0) {
				EL_PRINT_E("elliptic_data_io_write failed");
				return err;
			}

		} else {
			EL_PRINT_E("TAG or Length is not valid");
		}

		break;

	default:
		EL_PRINT_W("UNKNOWN IOCTL number=%d", number);
		break;
	}

	return 0;
}


static unsigned int device_poll(struct file *file,
	struct poll_table_struct *poll_table)
{
	unsigned int mask;

	struct elliptic_device *device;
	struct elliptic_data *elliptic_data;

	mask = 0;
	device = (struct elliptic_device *)file->private_data;
	elliptic_data = (struct elliptic_data *)&device->el_data;

	poll_wait(file, &elliptic_data->fifo_usp_not_empty, poll_table);

	if (!kfifo_is_empty(&elliptic_data->fifo_userspace))
		mask = POLLIN | POLLRDNORM;

	return mask;
}


static int device_close(struct inode *inode, struct file *filp)
{
	struct elliptic_device *device;
	struct elliptic_data *elliptic_data;
	unsigned int minor;

	device = filp->private_data;
	elliptic_data = &device->el_data;
	minor = iminor(inode);
	if (device == NULL) {
		EL_PRINT_E("device not found");
		return -ENODEV;
	}

	device->opened = 0;
	elliptic_data_update_debug_counters(elliptic_data);
	elliptic_data_print_debug_counters(elliptic_data);

	up(&device->sem);

	EL_PRINT_I("Closed device elliptic%u", minor);
	return 0;
}

/* defines the file operations provided by the driver */
static const struct file_operations elliptic_fops = {
	.owner = THIS_MODULE, /* prevents unloading when operations are in use*/
	.open = device_open,  /*to open the device*/
	.write = device_write, /*to write to the device*/
	.read = device_read, /*to read the device*/
	.poll = device_poll,
	.unlocked_ioctl = device_ioctl, /* IOCTL calls */
	.release = device_close, /*to close the device*/
};


static int elliptic_device_initialize(struct elliptic_device
	*elliptic_device, int minor, struct class *class)
{
	int err;
	dev_t device_number;
	struct device *device;

	BUG_ON(elliptic_device == NULL || class == NULL);

	err = 0;
	device = NULL;
	device_number = MKDEV(elliptic_major, minor);
	/* Memory is to be allocated when the device is opened the first time */
	sema_init(&elliptic_device->sem, 1);
	cdev_init(&elliptic_device->cdev, &elliptic_fops);
	elliptic_device->cdev.owner = THIS_MODULE;

	err = cdev_add(&elliptic_device->cdev, device_number, 1);

	if (err) {
		EL_PRINT_E("error %d while trying to add %s%d",
			err, ELLIPTIC_DEVICENAME, minor);
		return err;
	}

	device = device_create(class, NULL, device_number,
				   NULL, ELLIPTIC_DEVICENAME "%d", minor);

	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		EL_PRINT_E("error %d while trying to create %s%d",
			err, ELLIPTIC_DEVICENAME, minor);
		cdev_del(&elliptic_device->cdev);
		return err;
	}

	if (err) {
		EL_PRINT_E("failed device initialize");
		return err;
	}

	return 0;
}

static void elliptic_device_cleanup(struct elliptic_device *dev, int minor,
					struct class *class)

{
	BUG_ON(dev == NULL || class == NULL);
	device_destroy(class, MKDEV(elliptic_major, minor));
	cdev_del(&dev->cdev);
	up(&dev->sem);
}

static void elliptic_driver_cleanup(int devices_to_destroy)
{
	int i;

	if (elliptic_devices) {
		elliptic_data_io_cleanup();

		for (i = 0; i < devices_to_destroy; ++i) {
			elliptic_data_cleanup(&elliptic_devices[i].el_data);
			elliptic_device_cleanup(
				&elliptic_devices[i], i, elliptic_class);
		}

		kfree(elliptic_devices);
	}

	if (elliptic_class)
		class_destroy(elliptic_class);

	unregister_chrdev_region(
		MKDEV(elliptic_major, 0), ELLIPTIC_NUM_DEVICES);
}
static int __init elliptic_driver_init(void)
{
	int err;
	int i;
	int devices_to_destroy = 0;
	dev_t device_number;

	err = alloc_chrdev_region(&device_number, 0, ELLIPTIC_NUM_DEVICES,
		ELLIPTIC_DEVICENAME);

	if (err < 0) {
		EL_PRINT_E("Failed to allocate cdev region");
		return err;
	}

	elliptic_major = MAJOR(device_number);
	elliptic_class = class_create(THIS_MODULE, "chardev");

	if (elliptic_class == NULL) {
		EL_PRINT_E("Class creation failed");
		goto fail;
	}

	err = elliptic_initialize_sysfs();

	if (err)
		goto fail;

	elliptic_devices = (struct elliptic_device *)
		kzalloc(sizeof(struct elliptic_device) * ELLIPTIC_NUM_DEVICES,
		GFP_KERNEL);

	if (elliptic_devices == NULL) {
		err = -ENOMEM;
		goto fail;
	}

	if (elliptic_data_io_initialize())
		goto fail;

	for (i = 0; i < ELLIPTIC_NUM_DEVICES; ++i) {
		if (elliptic_device_initialize(&elliptic_devices[i], i,
			elliptic_class)) {
			devices_to_destroy = i;
			goto fail;
		}

		if (elliptic_data_initialize(&elliptic_devices[i].el_data,
			ELLIPTIC_DATA_FIFO_SIZE, ELLIPTIC_WAKEUP_TIMEOUT, i)) {
			goto fail;
		}
	}

	wake_source = kmalloc(sizeof(struct wakeup_source), GFP_KERNEL);

	if (!wake_source) {
		EL_PRINT_E("failed to allocate wake source");
		return -ENOMEM;
	}

	wakeup_source_init(wake_source, "elliptic_wake_source");

	return 0;

fail:
	elliptic_driver_cleanup(devices_to_destroy);
	return err;
}

static void elliptic_driver_exit(void)
{
	if (wake_source) {
		wakeup_source_trash(wake_source);
		kfree(wake_source);
	}

	elliptic_cleanup_sysfs();
	elliptic_driver_cleanup(ELLIPTIC_NUM_DEVICES);
}

MODULE_AUTHOR("Elliptic Labs");
MODULE_DESCRIPTION("Providing Interface to UPS data");
MODULE_LICENSE("GPL");

module_init(elliptic_driver_init);
module_exit(elliptic_driver_exit);
