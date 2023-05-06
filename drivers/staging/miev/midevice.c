#define DEBUG 1
#define pr_fmt(fmt) "miev: " fmt

#include "midevice.h"
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/poll.h>

static int miev_open(struct inode *inode, struct file *filp);
static int miev_release(struct inode *inode, struct file *filp);
static long miev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static ssize_t miev_read(struct file *filp, char __user *ubuf, size_t size,
			 loff_t *offset);
static ssize_t miev_write(struct file *filp, const char __user *ubuf,
			  size_t size, loff_t *offset);

/* Kfifo buffer size */
#define FIFO_SIZE (64 * 1024)
/* write and read buffer max size */
#define BUF_MAX_SIZE 4096
static char *buf_in;
static int tmp_in_size;
static int tmp_out_size;

static int miev_open(struct inode *inode, struct file *filp)
{
	// Module count plus one
	try_module_get(THIS_MODULE);
	pr_devel("This chrdev is in open");
	return 0;
}

static int miev_release(struct inode *inode, struct file *filp)
{
	// Module count minus one
	module_put(THIS_MODULE);
	wake_up_interruptible(&miev_dev->wait_queue);
	pr_devel("This chrdev is in release");
	return 0;
}

int write_kbuf(char __kernel *kbuf, int size)
{
	int ret = 0;
	char *mievent_in;
	mutex_lock(&miev_dev->lock);
	size = size < BUF_MAX_SIZE ? size : BUF_MAX_SIZE;
	mievent_in = buf_in;
	while (kfifo_avail(&miev_dev->fifo) < size + sizeof(int)) {
		if (kfifo_out(&miev_dev->fifo, &tmp_in_size, sizeof(int))) {
			kfifo_out(&miev_dev->fifo, mievent_in, tmp_in_size);
		}
	}
	mievent_in = kbuf;
	if (kfifo_in(&miev_dev->fifo, &size, sizeof(int))) {
		ret = kfifo_in(&miev_dev->fifo, mievent_in, size);
	}
	mutex_unlock(&miev_dev->lock);
  	wake_up_interruptible(&miev_dev->wait_queue);
	pr_devel("mievent_in:%s", mievent_in);
	pr_devel("kfifo_len=%u,ret:%d", kfifo_len(&miev_dev->fifo), ret);
	pr_devel("write kernel success!!");
	return ret;
}

static long miev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (_IOC_TYPE(cmd) != MIEV_IOC_MAGIC)
		return -EINVAL;
	if (_IOC_NR(cmd) > MIEV_IOC_MAXNR)
		return -EINVAL;

	if (!access_ok((void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case MIEV_IOC_WRITE:
		pr_devel("MIEV_IOC_WRITE");
		break;
	case MIEV_IOC_READ:
		pr_devel("MIEV_IOC_READ");
		break;
	case MIEV_IOC_NONE:
		pr_devel("miev ioctl call success");
		break;
	default:
		return -ENOTTY;
	}
	return ret;
}

static __poll_t miev_poll(struct file *filp, poll_table *wait)
{
	__poll_t mask = 0;
	poll_wait(filp, &miev_dev->wait_queue, wait);
	mutex_lock(&miev_dev->lock);
	if (!kfifo_is_empty(&miev_dev->fifo)) {
		mask |= POLLIN | POLLRDNORM;
	}
	mutex_unlock(&miev_dev->lock);
	return mask;
}

static ssize_t miev_read(struct file *filp, char __user *ubuf, size_t size,
			 loff_t *offset)
{
	int ret = 0;
	int fifo_len = 0;
	mutex_lock(&miev_dev->lock);
	if (!kfifo_is_empty(&miev_dev->fifo)) {
		fifo_len = kfifo_len(&miev_dev->fifo);
		size = size < BUF_MAX_SIZE ? size : BUF_MAX_SIZE;
		size = size < fifo_len ? size : fifo_len;
		if (kfifo_out(&miev_dev->fifo, &tmp_out_size, sizeof(int))) {
			if (kfifo_to_user(&miev_dev->fifo, ubuf, tmp_out_size, &ret)) {
				mutex_unlock(&miev_dev->lock);
				pr_err("copy_to_user failed");
				return -EFAULT;
			}
		}
	}
	pr_devel("kfifo_len=%u,ret:%d", kfifo_len(&miev_dev->fifo), ret);
	mutex_unlock(&miev_dev->lock);
	pr_devel("read success!!");
	return ret;
}

static ssize_t miev_write(struct file *filp, const char __user *ubuf,
			  size_t size, loff_t *offset)
{
	int ret = 0;
	char *mievent_in;
	mutex_lock(&miev_dev->lock);
	size = size < BUF_MAX_SIZE ? size : BUF_MAX_SIZE;
	mievent_in = buf_in;
	while (kfifo_avail(&miev_dev->fifo) < size + sizeof(int)) {
		if (kfifo_out(&miev_dev->fifo, &tmp_in_size, sizeof(int))) {
			kfifo_out(&miev_dev->fifo, mievent_in, tmp_in_size);
		}
	}
	if (copy_from_user(mievent_in, (char __user *)ubuf, size)) {
		pr_err("copy miev_buffer from user failed");
		mutex_unlock(&miev_dev->lock);
		return -EFAULT;
	}
	*(mievent_in + size) = '\0';
	if (kfifo_in(&miev_dev->fifo, &size, sizeof(int))) {
		ret = kfifo_in(&miev_dev->fifo, mievent_in, size);
	}
	pr_devel("kfifo_len=%u,ret: %d", kfifo_len(&miev_dev->fifo), ret);
	mutex_unlock(&miev_dev->lock);
	wake_up_interruptible(&miev_dev->wait_queue);
	pr_devel("mievent_in:%s", mievent_in);
	pr_devel("write success!!");
	return ret;
}

static struct file_operations miev_fops = {
	.owner = THIS_MODULE,
	.open = miev_open,
	.release = miev_release,
	.read = miev_read,
	.write = miev_write,
	.unlocked_ioctl = miev_ioctl,
	.poll = miev_poll,
};

static int __init miev_init_module(void)
{
	int ret = 0;
	struct device *my_device;
	miev_dev = kmalloc(sizeof(struct miev_device), GFP_KERNEL);
	if (!miev_dev) {
		pr_err("Fail to create miev_device");
		ret = -ENOMEM;
		goto err;
	}
	miev_dev->dev_no = MKDEV(DEV_MAJOR, DEV_MINOR);
	ret = register_chrdev_region(miev_dev->dev_no, 1, DEV_NAME);
	if (ret < 0) {
		ret = alloc_chrdev_region(&miev_dev->dev_no, 0, 1, DEV_NAME);
		if (ret < 0) {
			ret = -ENODEV;
			pr_err("Fail to register_chrdev_region");
			goto err_register;
		}
	}
	cdev_init(&miev_dev->chrdev, &miev_fops);
	miev_dev->chrdev.owner = THIS_MODULE;
	ret = cdev_add(&miev_dev->chrdev, miev_dev->dev_no, 1);
	if (ret < 0) {
		pr_err("Fail to add miev_dev");
		goto err_cdev;
	} else {
		pr_devel("miev register success");
		if (kfifo_alloc(&miev_dev->fifo, FIFO_SIZE, GFP_KERNEL)) {
			pr_err("kfifo_alloc create failed");
			ret = -ENOMEM;
			goto err_kfifo;
		}

		pr_devel("queue size: %u", kfifo_size(&miev_dev->fifo));
	}

	miev_dev->my_class = class_create(THIS_MODULE, DEV_NAME);
	if (IS_ERR(miev_dev->my_class)) {
		pr_err("class create failed!");
		ret = PTR_ERR(miev_dev->my_class);
		goto err_class;
	}

	my_device = device_create(miev_dev->my_class, NULL, miev_dev->dev_no,
				  NULL, DEV_NAME);
	if (IS_ERR(my_device)) {
		pr_err("device create failed!");
		ret = PTR_ERR(my_device);
		goto err_device;
	}

	mutex_init(&miev_dev->lock);
	init_waitqueue_head(&miev_dev->wait_queue);

	buf_in = kmalloc(BUF_MAX_SIZE + 1, GFP_KERNEL);
	if (!buf_in) {
		ret = -ENOMEM;
		goto err_buf_in;
	}
	return 0;

err_buf_in:
	device_destroy(miev_dev->my_class, miev_dev->dev_no);
err_device:
	class_destroy(miev_dev->my_class);
err_class:
	kfifo_free(&miev_dev->fifo);
err_kfifo:
	cdev_del(&miev_dev->chrdev);
err_cdev:
	unregister_chrdev_region(miev_dev->dev_no, 1);
err_register:
	kfree(miev_dev);
err:
	return ret;
}

static void __exit miev_exit_module(void)
{
	device_destroy(miev_dev->my_class, miev_dev->dev_no);
	class_destroy(miev_dev->my_class);
	kfifo_free(&miev_dev->fifo);
	cdev_del(&miev_dev->chrdev);
	unregister_chrdev_region(miev_dev->dev_no, 1);
	kfree(miev_dev);
	kfree(buf_in);
}

module_init(miev_init_module);
module_exit(miev_exit_module);

MODULE_AUTHOR("huangqibo <huangqibo@xiaomi.com>");
MODULE_DESCRIPTION("exception log transfer.");
MODULE_LICENSE("GPL");