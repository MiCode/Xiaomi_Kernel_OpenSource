#define pr_fmt(fmt) "MI_EXCEPTION_LOG: " fmt

#include "mi_exception_log.h"
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/uio.h>

#define EXCEPTION_LOG_DEV_NAME      "mi_exception_log"

static struct exception_log_dev mi_exception_log_dev;

static struct class *exception_log_class;
static unsigned int exception_log_major;
static DEFINE_RAW_SPINLOCK(exception_log_lock);

static ssize_t exception_log_read(struct file *fp, char __user *buff, size_t length, loff_t *ppos)
{
	ssize_t bytes_read = 0;
	unsigned long lock_flags;
	struct exception_log_dev *tmp_exception_log_dev = &mi_exception_log_dev;
	char usrbuf[DATASIZE] = {};
	int data_front;
	int ret = 0;

	raw_spin_lock_irqsave(&exception_log_lock, lock_flags);
	data_front = tmp_exception_log_dev->data_front;

	if (tmp_exception_log_dev->data_rear != tmp_exception_log_dev->data_front) {
		int line_feed_index, string_end_index;
		if (strlen(tmp_exception_log_dev->ais_data[data_front]) + 1 < DATASIZE) {
			line_feed_index = strlen(tmp_exception_log_dev->ais_data[data_front]);
			string_end_index = line_feed_index + 1;
		} else {
			line_feed_index = DATASIZE - 2;
			string_end_index = line_feed_index - 1;
		}
		*(tmp_exception_log_dev->ais_data[data_front] + line_feed_index) = '\n';
		*(tmp_exception_log_dev->ais_data[data_front] + string_end_index) = '\0';
		tmp_exception_log_dev->data_front = (tmp_exception_log_dev->data_front + 1) % MAXSIZE;
	} else {
		strcpy(tmp_exception_log_dev->ais_data[data_front], "empty\n");
	}

	bytes_read = MIN(strlen(tmp_exception_log_dev->ais_data[data_front]), length);
	strncpy(usrbuf, tmp_exception_log_dev->ais_data[data_front], bytes_read);
	raw_spin_unlock_irqrestore(&exception_log_lock, lock_flags);

	ret = copy_to_user(buff, usrbuf, bytes_read);

	return bytes_read;
}

static ssize_t exception_log_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	size_t len = iov_iter_count(from);
	unsigned long lock_flags;
	struct exception_log_dev *tmp_exception_log_dev = &mi_exception_log_dev;
	int data_rear;
	char usrbuf[DATASIZE] = {};

	len = MIN(len, DATASIZE - 2);
	if (!copy_from_iter_full(usrbuf, len, from)) {
		return -EFAULT;
	}

	raw_spin_lock_irqsave(&exception_log_lock, lock_flags);
	data_rear = tmp_exception_log_dev->data_rear;
	if ((tmp_exception_log_dev->data_rear + 1) % MAXSIZE == tmp_exception_log_dev->data_front) {
		//exception log is full, overwrite data_front
		tmp_exception_log_dev->data_front = (tmp_exception_log_dev->data_front + 1) % MAXSIZE;
	} else {
		tmp_exception_log_dev->data_rear = (tmp_exception_log_dev->data_rear + 1) % MAXSIZE;
	}
	strncpy(tmp_exception_log_dev->ais_data[data_rear], usrbuf, len);
	raw_spin_unlock_irqrestore(&exception_log_lock, lock_flags);

	wake_up_interruptible(&tmp_exception_log_dev->exception_log_is_not_empty);

	return len;
}

static int exception_log_release(struct inode *inode, struct file *filp)
{
	struct exception_log_dev *tmp_exception_log_dev = &mi_exception_log_dev;
	pr_debug(" exception_log_release!\n");
	wake_up_interruptible(&tmp_exception_log_dev->exception_log_is_not_empty);
	return 0;
}

static unsigned int exception_log_poll(struct file *file, struct poll_table_struct *poll_table)
{
	unsigned int mask = DEFAULT_POLLMASK;
	struct exception_log_dev *tmp_exception_log_dev = &mi_exception_log_dev;
	poll_wait(file, &tmp_exception_log_dev->exception_log_is_not_empty, poll_table);

	if (tmp_exception_log_dev->data_rear != tmp_exception_log_dev->data_front) {
		mask |= EPOLLPRI;
	}

	return mask;
}

static const struct file_operations exception_log_fops = {
	.owner = THIS_MODULE,
	.release = exception_log_release,
	.poll = exception_log_poll,
	.read = exception_log_read,
	.write_iter = exception_log_write_iter,
};

static int __init mi_exception_log_init(void)
{
	struct device *device;
	struct exception_log_dev *tmp_exception_log_dev = &mi_exception_log_dev;
	exception_log_major = register_chrdev(0, EXCEPTION_LOG_DEV_NAME, &exception_log_fops);
	if (exception_log_major < 0) {
		pr_err("register_chrdev failed\n");
		return -EINVAL;
	}
	exception_log_class = class_create(THIS_MODULE, EXCEPTION_LOG_DEV_NAME);
	if (IS_ERR(exception_log_class)) {
		unregister_chrdev(exception_log_major,  EXCEPTION_LOG_DEV_NAME);
		pr_warn("Failed to create class.\n");
		return PTR_ERR(exception_log_class);
	}
	tmp_exception_log_dev->devt = MKDEV(exception_log_major, 0);
	device = device_create(exception_log_class, NULL, tmp_exception_log_dev->devt,
			NULL, EXCEPTION_LOG_DEV_NAME);

	if (IS_ERR(device)) {
		class_destroy(exception_log_class);
		unregister_chrdev(exception_log_major,  EXCEPTION_LOG_DEV_NAME);
		pr_err("error while trying to create %s\n", EXCEPTION_LOG_DEV_NAME);
		return -EINVAL;
	}
	init_waitqueue_head(&tmp_exception_log_dev->exception_log_is_not_empty);

	pr_info("mi_exception_log init ok");
	return 0;
}

module_init(mi_exception_log_init);

static void __exit mi_exception_log_exit(void)
{
	struct exception_log_dev *tmp_exception_log_dev = &mi_exception_log_dev;
	device_destroy(exception_log_class, tmp_exception_log_dev->devt);
	class_destroy(exception_log_class);
	unregister_chrdev(exception_log_major, EXCEPTION_LOG_DEV_NAME);
}

module_exit(mi_exception_log_exit);

MODULE_DESCRIPTION("mi exception log device driver");
MODULE_LICENSE("GPL v2");
