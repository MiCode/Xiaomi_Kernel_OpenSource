/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include "nq-ntag.h"

struct nqntag_platform_data {
	unsigned int ntagfd_gpio;
};

const static struct of_device_id msm_match_table[] = {
	{.compatible = "qcom,nq-ntag"},
	{},
};

MODULE_DEVICE_TABLE(of, msm_match_table);

struct nqntag_dev {
	wait_queue_head_t fd_wq;
	struct mutex fd_mutex;
	struct i2c_client *client;
	dev_t devno;
	struct class *nqntag_class;
	struct device *nqntag_device;
	struct cdev c_dev;
	bool irq_enabled;
	bool irq_wake_up;
	spinlock_t irq_enabled_lock;
	char offset;
	unsigned int ntagfd_gpio;
	enum of_gpio_flags fdflag;
	/* read buffer*/
	size_t kbuflen;
	u8 *kbuf;
};

/**
 * nqntag_irq_state()
 *
 * Based on state enable/disable FD interrupt
 *
 * Return: void
 */
static void nqntag_irq_state(struct nqntag_dev *nqntag_dev, unsigned int state)
{
	unsigned long flags;

	spin_lock_irqsave(&nqntag_dev->irq_enabled_lock, flags);
	if (state == FD_DISABLE) {
		if (nqntag_dev->irq_enabled) {
			disable_irq_nosync(nqntag_dev->client->irq);
			nqntag_dev->irq_enabled = false;
		}
	} else {
		if (!nqntag_dev->irq_enabled) {
			nqntag_dev->irq_enabled = true;
			enable_irq(nqntag_dev->client->irq);
		}
	}
	spin_unlock_irqrestore(&nqntag_dev->irq_enabled_lock, flags);
}

static irqreturn_t nqntag_dev_irq_handler(int irq, void *dev_id)
{
	struct nqntag_dev *nqntag_dev = dev_id;

	if (device_may_wakeup(&nqntag_dev->client->dev))
		pm_wakeup_event(&nqntag_dev->client->dev, WAKEUP_SRC_TIMEOUT);

	nqntag_irq_state(nqntag_dev, FD_DISABLE);
	wake_up(&nqntag_dev->fd_wq);
	return IRQ_HANDLED;
}

static ssize_t ntag_read(struct file *file, char __user *buf, size_t count,
		loff_t *offset)
{
	char *readdata;
	int ret = 0;
	size_t tmpcount = 1;
	struct nqntag_dev *nqntag_dev;
	char *bufaddr = NULL;

	if (!file || !file->private_data)
		return -ENODATA;
	nqntag_dev = file->private_data;
	if (nqntag_dev->offset < NTAG_MIN_OFFSET ||
		nqntag_dev->offset >= NTAG_USER_MEM_SPACE_MAX_OFFSET) {
		return -EAGAIN;
	}
	bufaddr = &nqntag_dev->offset;
	ret = i2c_master_send(nqntag_dev->client, bufaddr, tmpcount);
	if (ret < 0) {
		dev_err(&nqntag_dev->client->dev,
			"%s: failed to write %d\n", __func__, ret);
		return -EIO;
	}
	/* count+1 to store NULL byte */
	readdata = kzalloc(count + 1, GFP_KERNEL);
	if (readdata == NULL)
		return -ENOMEM;
	ret = i2c_master_recv(nqntag_dev->client, readdata, count);
	if (ret >= 0)
		ret = copy_to_user(buf, readdata, count) ? -EFAULT : ret;
	kfree(readdata);
	return ret;
}

static ssize_t ntag_write(struct file *file, const char __user *buf,
		size_t count, loff_t *offset)
{
	int ret = 0;
	char *writedata;
	struct nqntag_dev *nqntag_dev;

	if (!file || !file->private_data)
		return -ENODATA;
	nqntag_dev = file->private_data;
	if (nqntag_dev->offset < NTAG_MIN_OFFSET ||
		nqntag_dev->offset >= NTAG_USER_MEM_SPACE_MAX_OFFSET) {
		return -EAGAIN;
	}
	/* count+2 to store Offset and NULL byte */
	writedata = kzalloc(count + 2, GFP_KERNEL);
	if (writedata == NULL)
		return -ENOMEM;
	writedata[0] = nqntag_dev->offset;
	if (copy_from_user(&writedata[1], buf, count)) {
		dev_err(&nqntag_dev->client->dev, "Failed to copy from user\n");
		kfree(writedata);
		return -EFAULT;
	}
	ret = i2c_master_send(nqntag_dev->client, writedata, count + 1);
	if (ret != (count + 1)) {
		dev_err(&nqntag_dev->client->dev,
			"%s: failed to write %d\n", __func__, ret);
		kfree(writedata);
		return -EIO;
	}
	kfree(writedata);
	return count;
}

static int ntag_open(struct inode *inode, struct file *filp)
{
	struct nqntag_dev *nqntag_dev = container_of(inode->i_cdev,
				struct nqntag_dev, c_dev);

	filp->private_data = nqntag_dev;
	dev_dbg(&nqntag_dev->client->dev,
		 "%s: %d,%d\n", __func__, imajor(inode), iminor(inode));
	return 0;
}

/**
 * ntag_ioctl_fd_state()
 * @filp:       pointer to the file descriptor
 * @arg:        mode that we want to move to
 *
 * Device power control. Depending on the arg value, device moves to
 * different states
 * (arg = 0): FD_DISABLE
 * (arg = 1): FD_ENABLE
 *
 * Return: -ENOIOCTLCMD if arg is not supported
 */
static int ntag_ioctl_fd_state(struct file *filp, unsigned long arg)
{
	int r = 0, ret = 0;
	struct nqntag_dev *nqntag_dev = filp->private_data;
	int irq_gpio_val = 0;

	if (arg == 0) {
		/* Disabling FD interrupt */
		nqntag_irq_state(nqntag_dev, FD_DISABLE);
		return ret;
	} else if (arg == 1) {
		/* Enable FD interrupt and wait for RF field detection*/
		nqntag_irq_state(nqntag_dev, FD_ENABLE);
		mutex_lock(&nqntag_dev->fd_mutex);
		irq_gpio_val = gpio_get_value(nqntag_dev->ntagfd_gpio);
		dev_dbg(&nqntag_dev->client->dev,
			"%s: READ GPIO_VAL: %d/n", __func__, irq_gpio_val);
		if (!irq_gpio_val ^ nqntag_dev->fdflag) {
			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				goto err;
			}
			while (1) {
				r = 0;
				nqntag_irq_state(nqntag_dev, FD_ENABLE);
				irq_gpio_val = gpio_get_value(
						nqntag_dev->ntagfd_gpio);
				if (!irq_gpio_val ^ nqntag_dev->fdflag)
					r = wait_event_interruptible(
						nqntag_dev->fd_wq,
						!nqntag_dev->irq_enabled);
				if (r) {
					nqntag_irq_state(nqntag_dev,
							FD_DISABLE);
					ret = -EAGAIN;
					goto err;
				} else {
					break;
				}
			}
		}
	} else {
		ret = -EINVAL;
	}
err:
	mutex_unlock(&nqntag_dev->fd_mutex);
	return ret;
}

static long ntag_ioctl(struct file *pfile, unsigned int cmd,
			unsigned long arg)
{
	long r = 0;
	struct nqntag_dev *nqntag_dev;

	if (!pfile || !pfile->private_data)
		return -ENODATA;
	nqntag_dev = pfile->private_data;
	switch (cmd) {
	case NTAG_FD_STATE:
		r = ntag_ioctl_fd_state(pfile, arg);
		break;
	case NTAG_SET_OFFSET:
		nqntag_dev->offset = (char)arg;
		break;
	default:
		r = -ENOIOCTLCMD;
	}
	return r;
}

static const struct file_operations ntag_dev_fops = {
	.llseek = no_llseek,
	.read  = ntag_read,
	.write = ntag_write,
	.open = ntag_open,
	.unlocked_ioctl = ntag_ioctl,
};

static int nqntag_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int r = 0, irqn = 0;
	struct nqntag_platform_data *pdata;
	struct nqntag_dev *nqntag_dev;
	enum of_gpio_flags fdflag;

	dev_dbg(&client->dev, "%s: enter\n", __func__);
	pdata = devm_kzalloc(&client->dev,
			sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		r = -ENOMEM;
		goto err_probe;
	}
	pdata->ntagfd_gpio = of_get_named_gpio(
				client->dev.of_node, "qcom,nq-ntagfd", 0);
	if ((!gpio_is_valid(pdata->ntagfd_gpio))) {
		r = -EINVAL;
		goto err_probe;
	}
	r = of_get_named_gpio_flags(
		client->dev.of_node, "qcom,nq-ntagfd", 0, &fdflag);
	if (r < 0)
		goto err_probe;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "%s: need I2C_FUNC_I2C\n", __func__);
		r = -ENODEV;
		goto err_probe;
	}
	nqntag_dev = devm_kzalloc(&client->dev, sizeof(*nqntag_dev),
				GFP_KERNEL);
	if (nqntag_dev == NULL) {
		r = -ENOMEM;
		goto err_probe;
	}
	nqntag_dev->client = client;
	nqntag_dev->fdflag = fdflag;
	nqntag_dev->kbuflen = MAX_BUFFER_SIZE;
	nqntag_dev->kbuf = devm_kzalloc(&client->dev, MAX_BUFFER_SIZE,
					GFP_KERNEL);
	if (!nqntag_dev->kbuf) {
		r = -ENOMEM;
		goto err_probe;
	}
	r = devm_gpio_request(&client->dev, pdata->ntagfd_gpio,
			"ntagfd_gpio");
	if (r) {
		r = -ENOMEM;
		goto err_probe;
	}
	r = gpio_direction_input(pdata->ntagfd_gpio);
	if (r) {
		dev_err(&client->dev,
			"%s: unable to set direction for fd gpio [%d]\n",
			__func__, pdata->ntagfd_gpio);
		r = -EINVAL;
		goto err_probe;
	}
	irqn = gpio_to_irq(pdata->ntagfd_gpio);
	if (irqn < 0) {
		r = -EINVAL;
		goto err_probe;
	}
	client->irq = irqn;
	nqntag_dev->ntagfd_gpio = pdata->ntagfd_gpio;
	/* init mutex and wait queues */
	init_waitqueue_head(&nqntag_dev->fd_wq);
	mutex_init(&nqntag_dev->fd_mutex);
	spin_lock_init(&nqntag_dev->irq_enabled_lock);
	r = alloc_chrdev_region(&nqntag_dev->devno, 0, DEV_COUNT, DEVICE_NAME);
	if (r < 0) {
		dev_err(&client->dev,
			"%s: failed to alloc chrdev region\n", __func__);
		goto err_probe;
	}
	nqntag_dev->nqntag_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(nqntag_dev->nqntag_class)) {
		dev_err(&client->dev,
			"%s: failed to register device class\n", __func__);
		r = -EINVAL;
		goto err_class_create;
	}
	cdev_init(&nqntag_dev->c_dev, &ntag_dev_fops);
	r = cdev_add(&nqntag_dev->c_dev, nqntag_dev->devno, DEV_COUNT);
	if (r < 0) {
		dev_err(&client->dev, "%s: failed to add cdev\n", __func__);
		goto err_cdev_add;
	}
	nqntag_dev->nqntag_device = device_create(nqntag_dev->nqntag_class,
					NULL, nqntag_dev->devno, nqntag_dev,
					DEVICE_NAME);
	if (IS_ERR(nqntag_dev->nqntag_device)) {
		dev_err(&client->dev,
			"%s: failed to create the device\n", __func__);
		r = -EINVAL;
		goto err_device_create;
	}
	 /* NTAG_INT IRQ */
	nqntag_dev->irq_enabled = true;
	r = devm_request_irq(&client->dev, client->irq, nqntag_dev_irq_handler,
			IRQ_TYPE_EDGE_FALLING, client->name, nqntag_dev);
	if (r) {
		dev_err(&client->dev, "%s: request_irq failed\n", __func__);
		goto err_request_irq_failed;
	}
	nqntag_irq_state(nqntag_dev, FD_DISABLE);
	device_init_wakeup(&client->dev, true);
	i2c_set_clientdata(client, nqntag_dev);
	nqntag_dev->irq_wake_up = false;
	return 0;

err_request_irq_failed:
	device_destroy(nqntag_dev->nqntag_class, nqntag_dev->devno);
err_device_create:
	cdev_del(&nqntag_dev->c_dev);
err_cdev_add:
	class_destroy(nqntag_dev->nqntag_class);
err_class_create:
	unregister_chrdev_region(nqntag_dev->devno, DEV_COUNT);
err_probe:
	dev_err(&client->dev, "%s: probing NQ NTAG failed ret: %d\n",
		 __func__, r);
	return r;
}

static int nqntag_remove(struct i2c_client *client)
{
	struct nqntag_dev *nqntag_dev;

	nqntag_dev = i2c_get_clientdata(client);
	device_destroy(nqntag_dev->nqntag_class, nqntag_dev->devno);
	cdev_del(&nqntag_dev->c_dev);
	class_destroy(nqntag_dev->nqntag_class);
	unregister_chrdev_region(nqntag_dev->devno, DEV_COUNT);
	return 0;
}

#ifdef CONFIG_PM
/*
 * power management
 */
static int nqntag_suspend(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct nqntag_dev *nqntag_dev = i2c_get_clientdata(client);

	nqntag_irq_state(nqntag_dev, FD_ENABLE);
	if (device_may_wakeup(&client->dev)) {
		if (!enable_irq_wake(client->irq))
			nqntag_dev->irq_wake_up = true;
	}
	return 0;
}

static int nqntag_resume(struct device *device)
{
	struct i2c_client *client = to_i2c_client(device);
	struct nqntag_dev *nqntag_dev = i2c_get_clientdata(client);

	nqntag_irq_state(nqntag_dev, FD_DISABLE);
	if (device_may_wakeup(&client->dev) && nqntag_dev->irq_wake_up) {
		if (!disable_irq_wake(client->irq))
			nqntag_dev->irq_wake_up = false;
	}
	return 0;
}
#endif /* CONFIG_PM */

static const struct i2c_device_id nqntag_id[] = {
	{"nqntag-i2c", 0},
	{},
};
static const struct dev_pm_ops ntag_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(nqntag_suspend,
				nqntag_resume)
};

static struct i2c_driver nqntag = {
	.id_table = nqntag_id,
	.probe = nqntag_probe,
	.remove = nqntag_remove,
	.driver = {
		.name = "nq-ntag",
		.of_match_table = msm_match_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
		.pm = &ntag_pm_ops,
	},
};

static int __init nqntag_dev_init(void)
{
	return i2c_add_driver(&nqntag);
}
module_init(nqntag_dev_init);

static void __exit nqntag_dev_exit(void)
{
	i2c_del_driver(&nqntag);
}
module_exit(nqntag_dev_exit);

MODULE_DESCRIPTION("NTAG nqntag");
MODULE_LICENSE("GPL v2");
