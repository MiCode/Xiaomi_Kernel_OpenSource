/*
 * Simple driver for userspace access to inter-VM interrupts under the OKL4
 * Microvisor.
 *
 * Copyright (c) 2016 Cog Systems Pty Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/sysfs.h>
#include <linux/interrupt.h>
#include <linux/bitmap.h>
#include <linux/version.h>
#include <microvisor/microvisor.h>
#include <asm-generic/okl4_virq.h>

/* Linux version compatibility */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
#define OLD_ATTRS
#endif

#define VIRQ_MINORBITS	9

#define VIRQ_MINORS	(1 << VIRQ_MINORBITS)
#define VIRQ_MINOR_MAX	(VIRQ_MINORS - 1)

struct virq_data {
	wait_queue_head_t wq;

	int irqno;

	bool raised;
	unsigned long payload;
};

struct source_data {
	okl4_kcap_t kcap;
};

struct virq_device {
	struct device *dev;
	struct cdev cdev;

	int type;
	struct virq_data virq;
	struct source_data source;

	int minor;
	const char *label;
};

static irqreturn_t okl4_virq_handler(int irq, void *dev_id)
{
	struct virq_device *dev = dev_id;
	unsigned long payload = okl4_get_virq_payload(irq);

	dev->virq.payload |= payload;
	smp_wmb();
	dev->virq.raised = true;

	wake_up_interruptible(&dev->virq.wq);
	return IRQ_HANDLED;
}

static int virq_dev_open(struct inode *inode, struct file *file)
{
	struct virq_device *dev;

	dev = container_of(inode->i_cdev, struct virq_device, cdev);
	if (!dev)
		return -ENODEV;

	file->private_data = dev;

	return nonseekable_open(inode, file);
}

static int virq_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t virq_read(struct file *file, char __user *buf, size_t length,
			loff_t *off)
{
	struct virq_device *dev = file->private_data;
	unsigned long payload;
	int ret;

	if (!dev->virq.raised && (file->f_flags & O_NONBLOCK)) {
		ret = -EAGAIN;
		goto err_wait;
	}

	do {
		ret = wait_event_interruptible(dev->virq.wq, dev->virq.raised);
		if (ret < 0)
			goto err_wait;
	} while(!xchg(&dev->virq.raised, 0));

	smp_rmb();
	payload = xchg(&dev->virq.payload, 0);

	if (length >= sizeof(payload)) {
		if (copy_to_user(buf, &payload, sizeof(payload))) {
			dev_dbg(dev->dev, "failed to copy data to userland\n");
			return -EFAULT;
		}
		length = sizeof(payload);
	} else if (length != 0) {
		return -EIO;
	}

	return length;

err_wait:
	return ret;
}

static unsigned int virq_poll(struct file *file, poll_table *table)
{
	int mask = 0;
	struct virq_device *dev = file->private_data;

	poll_wait(file, &dev->virq.wq, table);

	if (dev->virq.raised)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t source_write(struct file *file, const char __user *ubuf,
			 size_t length, loff_t *offset)
{
	struct virq_device *dev = file->private_data;
	okl4_error_t err;
	unsigned long payload;

	if (length == 0)
		payload = 0;
	else if (length == sizeof(payload)) {
		if (copy_from_user(&payload, ubuf, length)) {
			dev_dbg(dev->dev, "failed to copy data from userland\n");
			return -EFAULT;
		}
	} else {
		return -EIO;
	}

	err =_okl4_sys_vinterrupt_raise(dev->source.kcap, payload);
	if (err != OKL4_OK) {
		dev_dbg(dev->dev, "failed to raise virtual interrupt\n");
		return  -EIO;
	}

	return length;
}

static ssize_t is_virq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virq_device *priv = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", priv->virq.irqno >= 0);
}

static ssize_t is_source_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virq_device *priv = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			priv->source.kcap != OKL4_KCAP_INVALID);
}

static ssize_t label_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct virq_device *priv = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", priv->label);
}

/* Device registration */

/*
 * file operations structure will be used for virq device.
 */
static const struct file_operations virq_r_fops = {
	.owner = THIS_MODULE,
	.open = virq_dev_open,
	.release = virq_dev_release,
	.read = virq_read,
	.poll = virq_poll,
};

/*
 * file operations structure will be used for virtual_interrupt_line device.
 */
static const struct file_operations virq_w_fops = {
	.owner = THIS_MODULE,
	.open = virq_dev_open,
	.release = virq_dev_release,
	.write = source_write,
};

/*
 * file operations structure will be used for combined virq+source device.
 */
static const struct file_operations virq_rw_fops = {
	.owner = THIS_MODULE,
	.open = virq_dev_open,
	.release = virq_dev_release,
	.read = virq_read,
	.poll = virq_poll,
	.write = source_write,
};

#ifndef OLD_ATTRS
static DEVICE_ATTR_RO(is_virq);
static DEVICE_ATTR_RO(is_source);
static DEVICE_ATTR_RO(label);

static struct attribute *virq_attrs[] = {
	&dev_attr_is_virq.attr,
	&dev_attr_is_source.attr,
	&dev_attr_label.attr,
	NULL
};
ATTRIBUTE_GROUPS(virq);
#else
static struct device_attribute virq_attrs[] = {
	__ATTR(is_virq, S_IRUGO, is_virq_show, NULL),
	__ATTR(is_source, S_IRUGO, is_source_show, NULL),
	__ATTR(label, S_IRUGO, label_show, NULL),
	__ATTR_NULL,
};
#endif

static struct class *virq_class;
static dev_t virq_devt;
static DEFINE_MUTEX(virq_minor_lock);
unsigned long virq_ids[BITS_TO_LONGS(VIRQ_MINORBITS)];

/**
 * virq_minor_get - obtain next free device minor number
 *
 * @dev:  device pointer
 *
 * Return: allocated minor, or -ENOSPC if no free minor left
 */
static int virq_minor_get(struct virq_device *dev)
{
	int ret;

	mutex_lock(&virq_minor_lock);

	ret = find_first_zero_bit(virq_ids, VIRQ_MINORS);

	if (ret >= VIRQ_MINORS) {
		dev_err(dev->dev, "too many virq devices\n");
		ret = -ENODEV;
	} else {
		dev->minor = ret;
		set_bit(ret, virq_ids);
		ret = 0;
	}
	mutex_unlock(&virq_minor_lock);
	return ret;
}

/**
 * virq_minor_free - mark device minor number as free
 *
 * @dev:  device pointer
 */
static void virq_minor_free(struct virq_device *dev)
{
	mutex_lock(&virq_minor_lock);
	clear_bit(dev->minor, virq_ids);
	mutex_unlock(&virq_minor_lock);
}

static void virq_dev_deregister(struct virq_device *dev)
{
	int devno;

	devno = dev->cdev.dev;
	cdev_del(&dev->cdev);

	device_destroy(virq_class, devno);

	virq_minor_free(dev);
}

static int okl4_virq_probe(struct platform_device *pdev)
{
	int ret, devno, irq;
	okl4_kcap_t kcap;
	struct virq_device *dev;
	struct device *parent;
	struct device *clsdev; /* class device */
	const struct file_operations *fops;
	struct device_node *node;
	char devname[32];

	parent = &pdev->dev;
	node = parent->of_node;

	dev = devm_kzalloc(&pdev->dev, sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->virq.raised = false;
	dev->virq.payload = 0;

	irq = platform_get_irq(pdev, 0);
	dev->virq.irqno = irq;

	init_waitqueue_head(&dev->virq.wq);

	ret = virq_minor_get(dev);
	if (ret < 0)
		goto err;

	if (irq >= 0) {
		ret = devm_request_irq(parent, irq, okl4_virq_handler,
				IRQF_TRIGGER_RISING, dev_name(parent), dev);
		if (ret < 0)
			goto err_free_minor;
	}

	kcap = OKL4_KCAP_INVALID;
	if (of_device_is_compatible(node, "okl,microvisor-interrupt-line")) {
		ret = of_property_read_u32(node, "reg", &kcap);
		if (ret < 0)
			goto err_free_minor;
	}
	dev->source.kcap = kcap;

	ret = of_property_read_string(node, "label", &dev->label);
	if (ret)
		dev->label = NULL;

	/* Fill in the data structures */
	devno = MKDEV(MAJOR(virq_devt), dev->minor);

	ret = -ENODEV;
	if (irq >= 0 && kcap != OKL4_KCAP_INVALID)
		fops = &virq_rw_fops;
	else if (irq >= 0)
		fops = &virq_r_fops;
	else if (kcap != OKL4_KCAP_INVALID)
		fops = &virq_w_fops;
	else
		goto err_free_minor;

	cdev_init(&dev->cdev, fops);
	dev->cdev.owner = fops->owner;

	/* Add the device */
	ret = cdev_add(&dev->cdev, devno, 1);
	if (ret) {
		dev_err(parent, "unable to add device %d:%d\n",
			MAJOR(virq_devt), dev->minor);
		goto err_free_minor;
	}

	scnprintf(devname, sizeof(devname), "virq%d", dev->minor);
#ifndef OLD_ATTRS
	clsdev = device_create_with_groups(virq_class, parent, devno, dev,
			virq_groups, devname, dev->minor);
#else
	clsdev = device_create(virq_class, parent, devno, dev, devname,
			dev->minor);
#endif
	if (IS_ERR(clsdev)) {
		dev_err(parent, "unable to create device %d:%d\n",
			MAJOR(virq_devt), dev->minor);
		ret = PTR_ERR(clsdev);
		goto err_dev_create;
	}

	dev_set_drvdata(parent, dev);

	return 0;

err_dev_create:
	cdev_del(&dev->cdev);
err_free_minor:
	virq_minor_free(dev);
err:
	devm_kfree(&pdev->dev, dev);
	return ret;
}

static int okl4_virq_remove(struct platform_device *pdev)
{
	struct virq_device *priv = dev_get_drvdata(&pdev->dev);

	virq_dev_deregister(priv);

	return 0;
}

/* Driver registration */

static const struct of_device_id of_plat_virq_table[] = {
	{ .compatible = "okl,user-virq" },
	{ /* end */ },
};

static struct platform_driver of_plat_virq_driver = {
	.driver = {
		.name = "okl4_virq",
		.owner = THIS_MODULE,
		.of_match_table = of_plat_virq_table,
	},
	.probe = okl4_virq_probe,
	.remove = okl4_virq_remove,
};

static int __init okl4_virq_init(void)
{
	int ret;

	memset(virq_ids, 0, sizeof(virq_ids));

	virq_class = class_create(THIS_MODULE, "okl4_virq");
	if (IS_ERR(virq_class)) {
		pr_err("failed to create class\n");
		ret = PTR_ERR(virq_class);
		goto err;
	}
#ifdef OLD_ATTRS
	virq_class->dev_attrs = virq_attrs;
#endif

	ret = alloc_chrdev_region(&virq_devt, 0, MINORMASK, "okl4_virq");
	if (ret < 0) {
		pr_err("failed to allocate char dev region\n");
		goto err_class;
	}

	ret = platform_driver_register(&of_plat_virq_driver);
	if (ret)
		goto err_virq_driver;

	return 0;

err_virq_driver:
	unregister_chrdev_region(virq_devt, MINORMASK);
err_class:
	class_destroy(virq_class);
err:
	return ret;
}

static void __exit okl4_virq_exit(void)
{
	platform_driver_unregister(&of_plat_virq_driver);

	unregister_chrdev_region(virq_devt, MINORMASK);
	class_destroy(virq_class);
}

module_init(okl4_virq_init);
module_exit(okl4_virq_exit);

MODULE_AUTHOR("Cog Systems Pty Ltd");
MODULE_DESCRIPTION("OKL4 userspace VIRQ interface");
