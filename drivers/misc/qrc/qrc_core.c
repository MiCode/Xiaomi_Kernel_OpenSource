// SPDX-License-Identifier: GPL-2.0-only
/* driver/misc/qrc/qrc_core.c
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "qrc_core.h"

#define FIFO_CLEAR 0x1

#define QRC_DEVICE_NAME "qrc"

static dev_t qrc_devt;
static struct class *qrc_class;

static int qrc_cdev_fasync(int fd, struct file *filp, int mode)
{
	struct qrc_dev *qrc;

	qrc = filp->private_data;
	return fasync_helper(fd, filp, mode, &qrc->async_queue);
}

static int qrc_cdev_open(struct inode *inode, struct file *filp)
{
	struct qrc_dev *qrc;

	qrc = container_of(inode->i_cdev,
						struct qrc_dev, cdev);
	filp->private_data = qrc;
	if (qrc->qrc_ops != NULL)
		qrc->qrc_ops->qrcops_open(qrc);
	return 0;
}

static int qrc_cdev_release(struct inode *inode, struct file *filp)
{
	struct qrc_dev *qrc;

	qrc = filp->private_data;
	if (qrc->qrc_ops != NULL)
		qrc->qrc_ops->qrcops_close(qrc);

	return 0;
}

/* GPIO control  */
static int
qrc_control_gpio_init(struct qrc_dev *qdev, struct device_node *node)
{
	int ret;

	/* QRC BOOT0 GPIO */
	qdev->qrc_boot0_gpio = of_get_named_gpio(node,
	"qcom,qrc-boot-gpio", 0);
	if (qdev->qrc_boot0_gpio < 0)
		pr_err("qrc_boot0_gpio is not available\n");

	/* UART RESET GPIO */
	qdev->qrc_reset_gpio = of_get_named_gpio(node,
	"qcom,qrc-reset-gpio", 0);
	if (qdev->qrc_reset_gpio < 0)
		pr_err("qrc_reset_gpio is not available\n");

	if (gpio_is_valid(qdev->qrc_boot0_gpio)) {
		ret = gpio_request(qdev->qrc_boot0_gpio, "QRC_BOOT0_GPIO");
		if (unlikely(ret)) {
			pr_err("gpio request qrc_boot0_gpio failed for:%d\n",
					qdev->qrc_boot0_gpio);
			return ret;
		}
	}

	if (gpio_is_valid(qdev->qrc_reset_gpio)) {
		ret = gpio_request(qdev->qrc_reset_gpio, "QRC_RESET_GPIO");
		if (unlikely(ret)) {
			pr_err("gpio request qrc_reset_gpio failed for:%d\n",
			qdev->qrc_reset_gpio);
			return ret;
		}
	}

	ret = gpio_direction_output(qdev->qrc_reset_gpio, 0);
	ret += gpio_export(qdev->qrc_reset_gpio, 0);

	if (ret) {
		pr_err("Unable to configure GPIO%d (RESET)\n",
			qdev->qrc_reset_gpio);
		ret = -EBUSY;
		gpio_free(qdev->qrc_reset_gpio);
		return ret;
	}

	ret = gpio_direction_output(qdev->qrc_boot0_gpio, 1);
	ret += gpio_export(qdev->qrc_boot0_gpio, 0);
	if (ret) {
		pr_err("Unable to configure GPIO%d (BOOT0)\n",
			qdev->qrc_boot0_gpio);
		ret = -EBUSY;
		gpio_free(qdev->qrc_boot0_gpio);
		return ret;
	}
	/* default config gpio status.boot=1,reset=0 */
	gpio_set_value(qdev->qrc_boot0_gpio, 1);
	gpio_set_value(qdev->qrc_reset_gpio, 0);

	return 0;
}

static void
qrc_control_gpio_uninit(struct qrc_dev *qdev)
{
	gpio_free(qdev->qrc_boot0_gpio);
	gpio_free(qdev->qrc_reset_gpio);
}

static void qrc_gpio_reboot(struct qrc_dev *qdev)
{
	gpio_set_value(qdev->qrc_reset_gpio, 0);
	msleep(100);
	gpio_set_value(qdev->qrc_reset_gpio, 1);
}
static long qrc_cdev_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct qrc_dev *qdev;

	qdev = filp->private_data;
	switch (cmd) {
	case QRC_FIFO_CLEAR:
		mutex_lock(&qdev->mutex);
		qdev->qrc_ops->qrcops_data_clean(qdev);
		mutex_unlock(&qdev->mutex);
		return 0;
	case QRC_REBOOT:
		if (gpio_is_valid(qdev->qrc_reset_gpio)) {
			qrc_gpio_reboot(qdev);
			return 0;
		} else
			return -EFAULT;
	case QRC_BOOT_TO_MEM:
		if (gpio_is_valid(qdev->qrc_boot0_gpio)) {
			gpio_set_value(qdev->qrc_boot0_gpio, 1);
			qrc_gpio_reboot(qdev);
			return 0;
		} else
			return -EFAULT;
	case QRC_BOOT_TO_FLASH:
		if (gpio_is_valid(qdev->qrc_boot0_gpio)) {
			gpio_set_value(qdev->qrc_boot0_gpio, 0);
			qrc_gpio_reboot(qdev);
			return 0;
		} else
			return -EFAULT;
	default:
		return -EINVAL;
	}
	return 0;
}

static unsigned int qrc_cdev_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	struct qrc_dev *qrc;

	qrc = filp->private_data;
	mutex_lock(&qrc->mutex);

	poll_wait(filp, &qrc->r_wait, wait);

	if (qrc->qrc_ops->qrcops_data_status(qrc) != 0)
		mask |= POLLIN | POLLRDNORM;

	mutex_unlock(&qrc->mutex);
	return mask;
}

static ssize_t qrc_cdev_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *ppos)
{
	int ret;
	struct qrc_dev *qrc;

	qrc = filp->private_data;
	DECLARE_WAITQUEUE(wait, current);

	mutex_lock(&qrc->mutex);
	add_wait_queue(&qrc->r_wait, &wait);

	while (qrc->qrc_ops->qrcops_data_status(qrc) == 0) {

		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&qrc->mutex);

		schedule();
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out2;
		}

		mutex_lock(&qrc->mutex);
	}

	ret = qrc->qrc_ops->qrcops_receive(qrc, buf, count);

 out:
	mutex_unlock(&qrc->mutex);
 out2:
	remove_wait_queue(&qrc->r_wait, &wait);
	set_current_state(TASK_RUNNING);
	return ret;
}

static ssize_t qrc_cdev_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *ppos)
{
	struct qrc_dev *qrc;
	struct qrcuart *qrcuart;
	enum qrcdev_tx ret;

	qrc = filp->private_data;
	ret = qrc->qrc_ops->qrcops_xmit(buf, count, qrc);
	if (ret == QRCDEV_TX_OK)
		return count;

	return 0;
}

static const struct file_operations qrc_cdev_fops = {
	.owner = THIS_MODULE,
	.read = qrc_cdev_read,
	.write = qrc_cdev_write,
	.unlocked_ioctl = qrc_cdev_ioctl,
	.poll = qrc_cdev_poll,
	.fasync = qrc_cdev_fasync,
	.open = qrc_cdev_open,
	.release = qrc_cdev_release,
};

/*-------Interface for qrc device ---------*/
int qrc_register_device(struct qrc_dev *qdev, struct device *dev)
{
	int ret;
	dev_t devt;

	if (!qdev)
		return -ENOMEM;

	mutex_init(&qdev->mutex);
	init_waitqueue_head(&qdev->r_wait);
	init_waitqueue_head(&qdev->w_wait);

	//register cdev
	qrc_class = class_create(THIS_MODULE, "qrc_class");
	if (IS_ERR(qrc_class)) {
		pr_err("failed to allocate class\n");
		return PTR_ERR(qrc_class);
	}
	ret = alloc_chrdev_region(&qrc_devt, 0, 1, "qrc");
	if (ret < 0) {
		pr_err("failed to allocate char device region\n");
		class_destroy(qrc_class);
		return ret;
	}

	devt = MKDEV(MAJOR(qrc_devt), 0);

	cdev_init(&qdev->cdev, &qrc_cdev_fops);
	ret = qrc_control_gpio_init(qdev, dev->of_node);

	ret = cdev_add(&qdev->cdev, devt, 1);
	if (ret) {
		pr_err("qrc failed to add char device\n");
		return ret;
	}

	qdev->dev = device_create(qrc_class, dev, devt, qdev,
							"qrc");
	if (IS_ERR(qdev->dev)) {
		ret = PTR_ERR(qdev->dev);
		goto del_cdev;
	}

	dev_info(dev, "qrc device  registered\n");
	return 0;

del_cdev:
	cdev_del(&qdev->cdev);
	return ret;
}

void qrc_unregister(struct qrc_dev *qdev)
{
	device_destroy(qrc_class, qdev->dev->devt);
	qrc_control_gpio_uninit(qdev);
	dev_info(qdev->dev, "qrc drv unregistered\n");
}
