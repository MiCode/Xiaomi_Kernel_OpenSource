/**
* Copyright MI
* Copyright (C) 2021 XiaoMi, Inc.
*
*/
/* #define DEBUG */
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

#include <mius/mius_sysfs.h>
#include <mius/mius_device.h>
#include <mius/mius_data_io.h>
#include <mius/mius_mixer_controls.h>
#include <dsp/apr_mius.h>


/* Alternative mechanism to load calibration data.
* Read calibration data during driver initialization
* and send message to the DSP
*
* #define MIUS_LOAD_CALIBRATION_DATA_FROM_FILESYSTEM 1
*/
#ifdef MIUS_LOAD_CALIBRATION_DATA_FROM_FILESYSTEM
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#endif

static struct mius_device *mius_devices;

/* Global variable for the device class*/
struct class *mius_class;

typedef uint32_t el_fifo_size_t;

/* Major number provided by the kernel*/
static dev_t mius_major;

static struct wakeup_source *wake_source;


void mius_data_cancel(struct mius_data *mius_data)
{
	atomic_set(&mius_data->abort_io, 1);
	wake_up_interruptible(&mius_data->fifo_isr_not_empty);
}

void mius_data_reset_debug_counters(struct mius_data *mius_data)
{
	mius_data->isr_fifo_discard = 0;
}

void mius_data_print_debug_counters(struct mius_data *mius_data)
{
	if (mius_data->isr_fifo_discard > 0) {
		MI_PRINT_E("isr fifo discarded %u frames",
			mius_data->isr_fifo_discard);
	}

	if (mius_data->userspace_read_total !=
		mius_data->isr_write_total) {
		MI_PRINT_I("user space reads / isr writes : %u / %u",
			mius_data->userspace_read_total,
			mius_data->isr_write_total);
	}

	MI_PRINT_I("total isr fifo discarded frame count : %u",
		mius_data->isr_fifo_discard_total);
}

void mius_data_update_debug_counters(struct mius_data
	*mius_data)
{
	mius_data->isr_fifo_discard_total +=
		mius_data->isr_fifo_discard;
}


/* spin lock for isr must be held prior to calling */
static void mius_data_flush_isr_fifo(struct mius_data
	*mius_data)
{
	kfifo_reset(&mius_data->fifo_isr);
}

/* spin lock for isr must be held prior to calling */
static void mius_data_isr_fifo_pop(struct mius_data
	*mius_data, size_t size)
{
	unsigned int fifo_result;
	static uint8_t temp_buffer[MIUS_MSG_BUF_SIZE];

	if (size > MIUS_MSG_BUF_SIZE)
		MI_PRINT_E("pop size %zu too large", size);

	fifo_result = kfifo_out(&mius_data->fifo_isr,
		temp_buffer, size);

	if (size != fifo_result)
		MI_PRINT_E("failed to pop element");
}


int mius_notify_gain_change_msg(int component_id, int gaindb)
{
	int32_t msg[3] = {MSC_COMPONENT_GAIN_CHANGE, component_id, gaindb};

	return mius_data_write(
		MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)msg, sizeof(msg));
}

/* inode refers to the actual file on disk */
static int device_open(struct inode *inode, struct file *filp)
{
	unsigned int major;
	unsigned int minor;
	struct mius_device *dev;
	struct mius_data *mius_data;

	major = imajor(inode);
	minor = iminor(inode);

	if (major != mius_major || minor < 0
		|| minor >= MIUS_NUM_DEVICES) {
		MI_PRINT_W("no device found with minor=%d and major=%d",
			major, minor);
		return -ENODEV; /* No such device */
	}

	dev = NULL;
	dev = &mius_devices[minor];
	filp->private_data = dev;

	if (inode->i_cdev != &dev->cdev) {
		MI_PRINT_W("dev pointer mismatch");
		return -ENODEV; /* No such device */
	}

	if (down_interruptible(&dev->sem) != 0) {
		MI_PRINT_E("the device has been opened, unable to open lock");
		return -EINVAL;
	}

	mius_data = &dev->el_data;
	spin_lock(&mius_data->fifo_isr_spinlock);
	mius_data_flush_isr_fifo(mius_data);
	spin_unlock(&mius_data->fifo_isr_spinlock);

	atomic_set(&mius_data->abort_io, 0);
	mius_data_reset_debug_counters(mius_data);

	MI_PRINT_I("Opened device mius%u", minor);
	dev->opened = 1;
	return 0;
}


int mius_data_initialize(struct mius_data
	*mius_data, size_t queue_size,
	unsigned int wakeup_timeout, int id)
{
	int is_power_of_two;

	is_power_of_two = (queue_size != 0) && !(queue_size & (queue_size - 1));

	if (is_power_of_two != 1) {
		MI_PRINT_E("non power of 2 fifo size");
		return -EINVAL;
	}

	if (kfifo_alloc(&mius_data->fifo_isr,
		queue_size, GFP_KERNEL) != 0) {
		MI_PRINT_E("failed to allocate fifo isr");
		return -EINVAL;
	}

	atomic_set(&mius_data->abort_io, 0);
	spin_lock_init(&mius_data->fifo_isr_spinlock);

	mius_data->wakeup_timeout = wakeup_timeout;

	mutex_init(&mius_data->user_buffer_lock);
	init_waitqueue_head(&mius_data->fifo_isr_not_empty);

	return 0;
}

int mius_data_cleanup(struct mius_data *mius_data)
{
	spin_unlock(&mius_data->fifo_isr_spinlock);
	kfifo_free(&mius_data->fifo_isr);
	return 0;
}

size_t mius_data_pop(struct mius_data
	*mius_data, char __user *user_buffer, size_t buffer_size)
{
	int result;
	unsigned long num_copied;
	unsigned int fifo_result;
	unsigned long flags;

	if (buffer_size < MIUS_MSG_BUF_SIZE) {
		MI_PRINT_E("buffer_size : %lu smaller than %lu",
			buffer_size, (size_t)MIUS_MSG_BUF_SIZE);
		return 0;
	}

	result = wait_event_interruptible(mius_data->fifo_isr_not_empty,
		(kfifo_is_empty(&mius_data->fifo_isr) == 0)
		|| (atomic_read(&mius_data->abort_io) == 1));

	if (atomic_read(&mius_data->abort_io) == 1) {
		atomic_set(&mius_data->abort_io, 0);
		MI_PRINT_D("pop cancelled");
		return 0;
	}

	if (result == 0) {
		spin_lock_irqsave(&mius_data->fifo_isr_spinlock, flags);

		fifo_result = kfifo_out(&mius_data->fifo_isr,
			mius_data->isr_swap_buffer, MIUS_MSG_BUF_SIZE);

		spin_unlock_irqrestore(&mius_data->fifo_isr_spinlock,
			flags);

		if (fifo_result == 0) {
			MI_PRINT_E("failed to copy: fifo isr -> swap buffer %u",
				fifo_result);
			return 0;
		}

		mutex_lock(&mius_data->user_buffer_lock);

		num_copied = copy_to_user(user_buffer,
						mius_data->isr_swap_buffer,
						MIUS_MSG_BUF_SIZE);

		mutex_unlock(&mius_data->user_buffer_lock);

		if (num_copied != 0) {
			MI_PRINT_E("failed copy to user");
			return 0;
		}
		++mius_data->userspace_read_total;
	} else {
		if (-ERESTARTSYS == result)
			MI_PRINT_I("wait interrupted");
		else
			MI_PRINT_E("wait error = %d", result);

		return 0;
	}

	return (size_t)MIUS_MSG_BUF_SIZE;
}



/* push data to specific device or all devices */
int mius_data_push(int deviceid,
	const char *buffer,
	size_t buffer_size,
	mius_data_push_t data_source)
{
	size_t available_space;
	size_t space_required;
	size_t zeros_to_pad;
	unsigned int copied_from_user;
	int copy_from_user_result;
	int err;
	int i;
	int i_max;

	unsigned long flags;
	struct mius_device *device;
	struct mius_data *mius_data;
	unsigned int fifo_result;
	static uint8_t zero_pad_buffer[MIUS_MSG_BUF_SIZE];

	err = 0;
	fifo_result = 0;
	copied_from_user = 0;
	copy_from_user_result = 0;
	if (buffer_size > MIUS_MSG_BUF_SIZE)
		return -EINVAL;

	zeros_to_pad = MIUS_MSG_BUF_SIZE - buffer_size;

	i = 0;
	i_max = MIUS_NUM_DEVICES;

	if (deviceid != MIUS_ALL_DEVICES) {
		/* Copy to specific device */
		i = deviceid;
		i_max = i + 1;
	}

	for (; i < i_max; ++i) {
		device = &mius_devices[i];
		mius_data = &device->el_data;

		if ((!device->opened))
			continue;

		available_space = kfifo_avail(&mius_data->fifo_isr);
		space_required = MIUS_MSG_BUF_SIZE;

		spin_lock_irqsave(&mius_data->fifo_isr_spinlock, flags);

		if (available_space < space_required) {

			++mius_data->isr_fifo_discard;
			mius_data_isr_fifo_pop(mius_data,
				MIUS_MSG_BUF_SIZE);
		}

		if (data_source == MIUS_DATA_PUSH_FROM_KERNEL) {
			fifo_result = kfifo_in(&mius_data->fifo_isr,
				buffer, buffer_size);

			if (fifo_result == 0) {
				spin_unlock_irqrestore(
					&mius_data->fifo_isr_spinlock,
					flags);
				continue;
			}
		} else if (data_source == MIUS_DATA_PUSH_FROM_USERSPACE) {
			copy_from_user_result = kfifo_from_user(
				&mius_data->fifo_isr, buffer,
				buffer_size, &copied_from_user);

			if (-EFAULT == copy_from_user_result) {
				spin_unlock_irqrestore(
					&mius_data->fifo_isr_spinlock,
					flags);
				continue;
			}
		}


		if (zeros_to_pad > 0) {
			fifo_result = kfifo_in(
				&mius_data->fifo_isr, zero_pad_buffer,
				zeros_to_pad);

			if (fifo_result == 0) {
				mius_data_isr_fifo_pop(mius_data,
					buffer_size);

				spin_unlock_irqrestore(
					&mius_data->fifo_isr_spinlock,
					flags);

				++mius_data->isr_fifo_discard;
				continue;
			}
		}


		++mius_data->isr_write_total;
		spin_unlock_irqrestore(
			&mius_data->fifo_isr_spinlock, flags);
		wake_up_interruptible(&mius_data->fifo_isr_not_empty);
		__pm_wakeup_event(wake_source, mius_data->wakeup_timeout);
	}

	return err;
}

int mius_open_port(int portid)
{
	return mius_io_open_port(portid);
}

int mius_close_port(int portid)
{
	return mius_io_close_port(portid);
}


int32_t mius_data_write(uint32_t message_id,
	const char *data, size_t data_size)
{
	int32_t err_dsp;
	/* int32_t err_us; */

	err_dsp = 0;
	err_dsp = mius_data_io_write(message_id, data, data_size);
	if (err_dsp)
		MI_PRINT_E("Failed write to DSP");
	return err_dsp;

	/*
	* err_us = 0;
	* err_us = mius_userspace_ctrl_write(message_id, data, data_size);
	* if(err_us){
	*	MI_PRINT_E("Failed write to user space");
	*}
	*
	*return (err_dsp | err_us);
	*/
}



/**
*
* @return Number of bytes read.
*/
static ssize_t device_read(struct file *fp, char __user *buff,
	size_t length, loff_t *ppos)
{
	ssize_t bytes_read = 0;
	struct mius_device *mius_device;
	struct mius_data *mius_data;

	mius_device = (struct mius_device *)fp->private_data;
	mius_data = (struct mius_data *)&mius_device->el_data;

	bytes_read = mius_data_pop(mius_data, buff, length);

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
		ret_val = mius_data_io_write(MIUS_ULTRASOUND_SET_PARAMS,
			buff, length);

	return ret_val >= 0 ? (ssize_t)length : 0;
}


static long device_ioctl(struct file *fp, unsigned int number,
	unsigned long param)
{
	struct mius_device *device;
	struct mius_data *mius_data;
	int err;
	unsigned int mirror_tag, mirror_payload_size;
	unsigned char *data_ptr;

	device = (struct mius_device *)(fp->private_data);
	mius_data = &device->el_data;

	switch (number) {
	case IOCTL_MIUS_DATA_IO_CANCEL:
		MI_PRINT_D("IOCTL_MIUS_CANCEL_READ %ld",
			param);
		mius_data_cancel(mius_data);
		break;

	case IOCTL_MIUS_DATA_IO_MIRROR:
		data_ptr = (unsigned char *)param;
		mirror_tag = *(unsigned int *)data_ptr;
		mirror_payload_size = *((unsigned int *)data_ptr + 1);

		if ((mirror_tag == MIRROR_TAG) &&
			(mirror_payload_size != 0) &&
			(mirror_payload_size <=
			(MIUS_SET_PARAMS_SIZE * 4))) {

			err = mius_data_io_write(
				MIUS_ULTRASOUND_SET_PARAMS,
				(data_ptr + 8), mirror_payload_size);

			if (err != 0) {
				MI_PRINT_E("mius_data_io_write failed");
				return err;
			}

		} else {
			MI_PRINT_E("TAG or Length is not valid");
		}

		break;

	default:
		MI_PRINT_W("UNKNOWN IOCTL number=%d", number);
		break;
	}

	return 0;
}


static unsigned int device_poll(struct file *file,
	struct poll_table_struct *poll_table)
{
	unsigned int mask;

	struct mius_device *device;
	struct mius_data *mius_data;

	mask = 0;
	device = (struct mius_device *)file->private_data;
	mius_data = (struct mius_data *)&device->el_data;

	poll_wait(file, &mius_data->fifo_isr_not_empty, poll_table);

	if (!kfifo_is_empty(&mius_data->fifo_isr))
		mask = POLLIN | POLLRDNORM;

	return mask;
}


static int device_close(struct inode *inode, struct file *filp)
{
	struct mius_device *device;
	struct mius_data *mius_data;
	unsigned int minor;

	device = filp->private_data;
	mius_data = &device->el_data;
	minor = iminor(inode);
	if (device == NULL) {
		MI_PRINT_E("device not found");
		return -ENODEV;
	}

	device->opened = 0;
	mius_data_update_debug_counters(mius_data);
	mius_data_print_debug_counters(mius_data);
	mius_data_cancel(mius_data);
	up(&device->sem);

	MI_PRINT_I("Closed device mius%u", minor);
	return 0;
}

/* defines the file operations provided by the driver */
static const struct file_operations mius_fops = {
	.owner = THIS_MODULE, /* prevents unloading when operations are in use*/
	.open = device_open,  /*to open the device*/
	.write = device_write, /*to write to the device*/
	.read = device_read, /*to read the device*/
	.poll = device_poll,
	.unlocked_ioctl = device_ioctl, /* IOCTL calls */
	.release = device_close, /*to close the device*/
};


static int mius_device_initialize(struct mius_device
	*mius_device, int minor, struct class *class)
{
	int err;
	dev_t device_number;
	struct device *device;

	BUG_ON(mius_device == NULL || class == NULL);

	err = 0;
	device = NULL;
	device_number = MKDEV(mius_major, minor);
	/* Memory is to be allocated when the device is opened the first time */
	sema_init(&mius_device->sem, 1);
	cdev_init(&mius_device->cdev, &mius_fops);
	mius_device->cdev.owner = THIS_MODULE;

	err = cdev_add(&mius_device->cdev, device_number, 1);

	if (err) {
		MI_PRINT_E("error %d while trying to add %s%d",
			err, MIUS_DEVICENAME, minor);
		return err;
	}

	device = device_create(class, NULL, device_number,
		NULL, MIUS_DEVICENAME "%d", minor);

	if (IS_ERR(device)) {
		err = PTR_ERR(device);
		MI_PRINT_E("error %d while trying to create %s%d",
			err, MIUS_DEVICENAME, minor);
		cdev_del(&mius_device->cdev);
		return err;
	}

	if (err) {
		MI_PRINT_E("failed device initialize");
		return err;
	}

	return 0;
}

static void mius_device_cleanup(struct mius_device *dev, int minor,
	struct class *class)

{
	BUG_ON(dev == NULL || class == NULL);
	device_destroy(class, MKDEV(mius_major, minor));
	cdev_del(&dev->cdev);
	up(&dev->sem);
}

static void mius_driver_cleanup(int devices_to_destroy)
{
	int i;

	if (mius_devices) {
		mius_data_io_cleanup();

		for (i = 0; i < devices_to_destroy; ++i) {
			mius_data_cleanup(&mius_devices[i].el_data);
			mius_device_cleanup(
				&mius_devices[i], i, mius_class);
		}

		kfree(mius_devices);
	}

	if (mius_class)
		class_destroy(mius_class);

	unregister_chrdev_region(
		MKDEV(mius_major, 0), MIUS_NUM_DEVICES);
}



#ifdef MIUS_LOAD_CALIBRATION_DATA_FROM_FILESYSTEM

#define MIUS_CALIBRATION_MAX_DATA_SIZE (MIUS_CALIBRATION_V2_DATA_SIZE + MIUS_CALIBRATION_DATA_SIZE)
static unsigned char calibration_data[MIUS_CALIBRATION_MAX_DATA_SIZE];
static char *calibration_filename = "/persist/audio/mius_calibration";

/* function to load the calibration from a file (if possible) */
static size_t load_calibration_data(char *filename)
{
	size_t bytes_read = 0;
	int rc = -ENOENT;

	rc = kernel_read_file_from_path(filename, calibration_data, &bytes_read,
					MIUS_CALIBRATION_MAX_DATA_SIZE,
					READING_FIRMWARE);
	if (rc) {
		if (rc == -ENOENT)
			MI_PRINT_E("loading %s failed with error %d\n",
				filename, rc);
		else
			MI_PRINT_E("loading %s failed with error %d\n",
				filename, rc);
	}
	MI_PRINT_I("loading %s\n", filename);


	return bytes_read;
}

static int32_t mius_send_calibration_to_engine(size_t calib_data_size)
{
    mius_set_calibration_data(calibration_data, calib_data_size);
	return mius_data_write(
		MIUS_ULTRASOUND_SET_PARAMS,
		(const char *)calibration_data, calib_data_size);
}

#endif


int __init mius_driver_init(void)
{
	int err;
	int i;
	int devices_to_destroy;
	dev_t device_number;

	err = alloc_chrdev_region(&device_number, 0, MIUS_NUM_DEVICES,
		MIUS_DEVICENAME);

	devices_to_destroy = 0;

	if (err < 0) {
		MI_PRINT_E("Failed to allocate cdev region");
		return err;
	}

	mius_major = MAJOR(device_number);
	mius_class = class_create(THIS_MODULE, "mius_chardev");

	if (mius_class == NULL) {
		MI_PRINT_E("Class creation failed");
		goto fail;
	}

	err = mius_initialize_sysfs();

	if (err)
		goto fail;

	mius_devices = (struct mius_device *)
		kzalloc(sizeof(struct mius_device) * MIUS_NUM_DEVICES,
			GFP_KERNEL);

	if (mius_devices == NULL) {
		err = -ENOMEM;
		goto fail;
	}


	for (i = 0; i < MIUS_NUM_DEVICES; ++i) {
		if (mius_device_initialize(&mius_devices[i], i,
			mius_class)) {
			devices_to_destroy = i;
			goto fail;
		}

		if (mius_data_initialize(&mius_devices[i].el_data,
			MIUS_DATA_FIFO_SIZE, MIUS_WAKEUP_TIMEOUT, i)) {
			goto fail;
		}
	}

	if (mius_data_io_initialize())
		goto fail;

	if (mius_userspace_io_driver_init())
		goto fail;


	if (mius_userspace_ctrl_driver_init())
		goto fail;

	wake_source = wakeup_source_register(NULL, "miui_wake_source");
	if (wake_source == NULL) {
		goto fail;
	}

#ifdef MIUS_LOAD_CALIBRATION_DATA_FROM_FILESYSTEM
	/* Code to send calibration to engine */
	{
		size_t calib_data_size = load_calibration_data(calibration_filename);
		if (calib_data_size > 0)
			mius_send_calibration_to_engine(calib_data_size);
	}
#endif
	return 0;

fail:
	mius_driver_cleanup(devices_to_destroy);
	return err;
}

void mius_driver_exit(void)
{
	wakeup_source_unregister(wake_source);
	mius_cleanup_sysfs();
	mius_driver_cleanup(MIUS_NUM_DEVICES);
	mius_userspace_io_driver_exit();
	mius_userspace_ctrl_driver_exit();
}

MODULE_AUTHOR("MI");
MODULE_DESCRIPTION("Providing Interface to UPS data");
MODULE_LICENSE("GPL");
