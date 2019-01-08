/*
 * Test driver for OKL4 Microvisor inter-cell communication
 *
 * Copyright (c) 2012-2014 General Dynamics
 * Copyright (c) 2014 Open Kernel Labs, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <microvisor/microvisor.h>

/* Linux version compatibility */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
#define OLD_ATTRS
#endif

struct ringbuf_data {
	struct platform_device *pdev;
	unsigned int irq;
	u32 virqline;
	struct mutex lock;

	struct resource buffer_res;
	void *buffer_map;

	u32 *head;
	u32 *tail;
	char *base;
	size_t size;

	size_t count;

	union {
		wait_queue_head_t tx_wq;
		struct work_struct rx_work;
	};

	struct cdev cdev;
};

dev_t ringbuf_tx_dev, ringbuf_rx_dev;

/* VIRQ payload flags */
#define VIRQ_FLAG_RESET 1UL /* Head has been set to 0 */
#define VIRQ_FLAG_DATA  2UL /* Head or tail has been advanced */

/*
 * Common probe / remove code
 */
static int ringbuf_probe(struct platform_device *pdev, const char *shbuf_name,
		const char *virqline_name, struct ringbuf_data **ringbuf_data)
{
	struct device_node *node = pdev->dev.of_node;
	struct device_node *virqline_node = NULL;
	struct ringbuf_data *priv;
	int r = 0;

	if (!node)
		return -ENODEV;
	if (of_property_match_string(node, "label", shbuf_name) < 0)
		return -ENODEV;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pdev = pdev;
	mutex_init(&priv->lock);

	r = of_address_to_resource(node, 0, &priv->buffer_res);
	if (r < 0)
		return r;

	/*
	 * For a useful ring buffer, we need enough space for the head
	 * and tail pointers, plus at least two bytes of data.
	 */
	if (resource_size(&priv->buffer_res) < (2 * sizeof(u32) + 2))
		return -ENODEV;

	/*
	 * Locate the virqline. Since we don't have a microvisor bus yet,
	 * we have to do this by searching the DT manually.
	 */
	/* FIXME: Jira ticket SDK-5565 - philipd. */
	for_each_compatible_node(virqline_node, NULL,
			"okl,microvisor-interrupt-line")
		if (of_property_match_string(virqline_node, "label",
					virqline_name) >= 0)
			break;
	if (!virqline_node)
		return -ENODEV;
	if (of_property_read_u32(virqline_node, "reg", &priv->virqline) < 0)
		return -ENODEV;

	/* Map the shared buffer's memory. */
	if (!devm_request_mem_region(&pdev->dev, priv->buffer_res.start,
			resource_size(&priv->buffer_res),
			dev_name(&pdev->dev)))
		return -EBUSY;

	dev_dbg(&pdev->dev, "Ring buffer @ 0x%llx, size 0x%llx\n",
			priv->buffer_res.start,
			resource_size(&priv->buffer_res));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,14,0)) || \
			defined(CONFIG_NO_DEPRECATED_IOREMAP)
        priv->buffer_map = ioremap_cache(priv->buffer_res.start,
			resource_size(&priv->buffer_res));
#else
        priv->buffer_map = ioremap_cached(priv->buffer_res.start,
			resource_size(&priv->buffer_res));
#endif
	if (priv->buffer_map == NULL)
		return -EIO;

	priv->head = (u32 *)priv->buffer_map;
	priv->tail = (u32 *)priv->buffer_map + 1;
	priv->base = (char *)priv->buffer_map + (2 * sizeof(u32));
	priv->size = resource_size(&priv->buffer_res) - (2 * sizeof(u32));
	priv->count = 0;

	dev_set_drvdata(&pdev->dev, priv);

	*ringbuf_data = priv;

	return 0;
}

static int ringbuf_remove(struct platform_device *pdev)
{
	struct ringbuf_data *priv = dev_get_drvdata(&pdev->dev);

	iounmap(priv->buffer_map);

	return 0;
}

/*
 * Sending side
 *
 * This registers a simple character device to accept writes from userland.
 * There is also a sysfs interface to ask the kernel to send arbitrary data.
 */
static size_t ringbuf_write_room(struct ringbuf_data *priv)
{
	return (priv->size + *priv->tail - *priv->head - 1) % priv->size;
}

static int ringbuf_tx_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	struct inode *inode = file_inode(file);
	struct cdev *cdev = inode->i_cdev;
	struct ringbuf_data *priv;
	int r;
	u32 head, new_head;
	size_t to_send;

	priv = container_of(cdev, struct ringbuf_data, cdev);

	/* Truncate the message if it's too big. */
	count = min(count, priv->size - 1);

	/* Wait until there is space to write the data. */
	mutex_lock(&priv->lock);
	r = wait_event_interruptible(priv->tx_wq,
			ringbuf_write_room(priv) >= count);
	if (r < 0) {
		mutex_unlock(&priv->lock);
		return r;
	}

	/*
	 * Note: wait_event_* has an implicit memory barrier, which prevents the
	 * copy below beginning before the wait completes. This matches the
	 * mb() before the cmpxchg() at the end of the read.
	 */

	head = *priv->head;
	to_send = count;
	new_head = (head + to_send) % priv->size;

	if (new_head < head) {
		if (copy_from_user(&priv->base[head], buffer,
				priv->size - head)) {
			mutex_unlock(&priv->lock);
			return -EFAULT;
		}
		buffer += priv->size - head;
		to_send -= priv->size - head;
		head = 0;
	}

	if (to_send > 0) {
		if (copy_from_user(&priv->base[head], buffer, to_send)) {
			mutex_unlock(&priv->lock);
			return -EFAULT;
		}
	}

	/*
	 * Ensure that the other side sees the copy before the head update.
	 * This matches the rmb() at the start of the read.
	 */
	wmb();

	*priv->head = new_head;
	mutex_unlock(&priv->lock);

	_okl4_sys_vinterrupt_raise(priv->virqline, VIRQ_FLAG_DATA);

	return count;
}

static int ringbuf_tx_fsync(struct file *file, loff_t start, loff_t end,
		int datasync)
{
	struct inode *inode = file_inode(file);
	struct cdev *cdev = inode->i_cdev;
	struct ringbuf_data *priv;

	priv = container_of(cdev, struct ringbuf_data, cdev);

	/* Wait until the ring buffer has emptied. */
	return wait_event_interruptible(priv->tx_wq,
			*priv->head == *priv->tail);
}

static const struct file_operations ringbuf_tx_fops = {
	.owner	= THIS_MODULE,
	.write	= ringbuf_tx_write,
	.fsync	= ringbuf_tx_fsync,
};

static int ringbuf_tx_send_zeros(struct ringbuf_data *priv, size_t count)
{
	int r;
	u32 head, new_head;
	size_t to_send;

	/* Truncate the message if it's too big. */
	count = min(count, priv->size - 1);

	/* Wait until there is space to write the data. */
	mutex_lock(&priv->lock);
	r = wait_event_interruptible(priv->tx_wq,
			ringbuf_write_room(priv) >= count);
	if (r < 0) {
		mutex_unlock(&priv->lock);
		return r;
	}

	/*
	 * Note: wait_event_* has an implicit memory barrier, which prevents the
	 * copy below beginning before the wait completes. This matches the
	 * mb() before the cmpxchg() at the end of the read.
	 */

	head = *priv->head;
	to_send = count;
	new_head = (head + to_send) % priv->size;

	if (new_head < head) {
		memset(&priv->base[head], 0, priv->size - head);
		to_send -= priv->size - head;
		head = 0;
	}

	if (to_send > 0)
		memset(&priv->base[head], 0, to_send);

	/*
	 * Ensure that the other side sees the copy before the head update.
	 * This matches the rmb() at the start of the read.
	 */
	wmb();

	*priv->head = new_head;
	mutex_unlock(&priv->lock);

	_okl4_sys_vinterrupt_raise(priv->virqline, VIRQ_FLAG_DATA);

	return count;
}

static irqreturn_t ringbuf_tx_irq_handler(int irq, void *data)
{
	struct ringbuf_data *priv = data;

	wake_up_interruptible(&priv->tx_wq);

	return IRQ_HANDLED;
}

static ssize_t send_zeros_store(struct device *dev,
		struct device_attribute *attr, const char *buffer, size_t count)
{
	unsigned long val;
	struct ringbuf_data *priv = dev_get_drvdata(dev);
	int r;

	r = kstrtoul(buffer, 0, &val);
	if (r < 0)
		return -EINVAL;

	r = ringbuf_tx_send_zeros(priv, val);
	if (r < 0)
		return r;

	return count;
}

static ssize_t flush_store(struct device *dev,
		struct device_attribute *attr, const char *buffer, size_t count)
{
	struct ringbuf_data *priv = dev_get_drvdata(dev);

	/* Wait until the ring buffer has emptied. */
	return wait_event_interruptible(priv->tx_wq,
			*priv->head == *priv->tail);
}

#ifndef OLD_ATTRS
static DEVICE_ATTR_WO(send_zeros);
static DEVICE_ATTR_WO(flush);

static struct attribute *ringbuf_tx_test_attrs[] = {
	&dev_attr_send_zeros.attr,
	&dev_attr_flush.attr,
	NULL
};
ATTRIBUTE_GROUPS(ringbuf_tx_test);
#else
static struct device_attribute ringbuf_tx_test_attrs[] = {
	__ATTR(send_zeros, S_IWUGO, NULL, send_zeros_store),
	__ATTR(flush, S_IWUGO, NULL, flush_store),
	__ATTR_NULL
};
#endif

static struct class *ringbuf_tx_class;

static int ringbuf_tx_probe(struct platform_device *pdev)
{
	struct ringbuf_data *priv;
	int r;
	okl4_error_t err;
	struct device *child;

	r = ringbuf_probe(pdev, "ringbuf", "ringbuf_tx", &priv);
	if (r < 0)
		return r;

	/*
	 * Make the tail temporarily invalid to prevent the receiver getting
	 * confused when we clear head and tail, in case it's still reading data
	 * sent by a previous instance of the driver.
	 */
	*priv->tail = ~0;
	wmb();

	/*
	 * Zero the head and tail, and ensure that the writes are visible
	 * remotely (and in that order) before any new data is written.
	 */
	*priv->head = 0;
	wmb();
	*priv->tail = 0;
	wmb();

	init_waitqueue_head(&priv->tx_wq);

	cdev_init(&priv->cdev, &ringbuf_tx_fops);
	r = cdev_add(&priv->cdev, ringbuf_tx_dev, 1);
	if (r < 0)
		goto fail_cdev;

#ifndef OLD_ATTRS
	child = device_create_with_groups(ringbuf_tx_class, &pdev->dev,
                        ringbuf_tx_dev, priv, ringbuf_tx_test_groups, "ringbuf");
#else
	child = device_create(ringbuf_tx_class, &pdev->dev, ringbuf_tx_dev,
			priv, "ringbuf");
#endif
	if (IS_ERR(child)) {
		r = PTR_ERR(child);
		goto fail_device_create;
	}

	/* Hard-coded IRQ number; also hard-coded in system XML */
	/* FIXME: Jira ticket SDK-5566 - philipd. */
	priv->irq = 128;
	r = devm_request_irq(&pdev->dev, priv->irq, ringbuf_tx_irq_handler, 0,
			dev_name(&pdev->dev), priv);
	if (r < 0)
		goto fail_request_irq;

	/*
	 * Signal to the receiver that the head and tail have been reset.
	 * Note that the rx driver below doesn't actually make use of this.
	 */
	err = _okl4_sys_vinterrupt_raise(priv->virqline, VIRQ_FLAG_RESET);
	if (err != OKL4_OK) {
		r = -EIO;
		goto fail_raise;
	}

	return 0;

fail_request_irq:
	device_destroy(ringbuf_tx_class, ringbuf_tx_dev);
fail_device_create:
fail_raise:
	cdev_del(&priv->cdev);
fail_cdev:
	ringbuf_remove(pdev);

	return r;
}

static int ringbuf_tx_remove(struct platform_device *pdev)
{
	struct ringbuf_data *priv = dev_get_drvdata(&pdev->dev);

	device_destroy(ringbuf_tx_class, ringbuf_tx_dev);
	cdev_del(&priv->cdev);
	ringbuf_remove(pdev);

	return 0;
}

static const struct of_device_id of_platform_shared_buffer_table[] = {
	{ .compatible = "okl,ringbuffer" },
	{ /* end */ },
};
MODULE_DEVICE_TABLE(of, of_platform_shared_buffer_table);

static struct platform_driver of_platform_ringbuf_tx_driver = {
	.driver = {
		.name = "okl4_ringbuf_tx",
		.owner = THIS_MODULE,
		.of_match_table = of_platform_shared_buffer_table,
	},
	.probe = ringbuf_tx_probe,
	.remove = ringbuf_tx_remove,
};

static irqreturn_t ringbuf_rx_irq_handler(int irq, void *data)
{
	struct ringbuf_data *priv = data;

	schedule_work(&priv->rx_work);

	return IRQ_HANDLED;
}

/*
 * Read the incoming data, in order to measure the cost of doing so, and then
 * discard it.
 */
static void read_and_discard(const char *buf, size_t count)
{
	while (count && ((uintptr_t)buf & (sizeof(u32) - 1))) {
		const char *data = buf++;
		(void)ACCESS_ONCE(data);
		count--;
	}

	while (count > sizeof(u32)) {
		const u32 *data = (const u32 *)buf;
		(void)ACCESS_ONCE(data);
		count -= sizeof(u32);
		buf += sizeof(u32);
	}

	while (count) {
		const char *data = buf++;
		(void)ACCESS_ONCE(data);
		count--;
	}
}

static void ringbuf_rx_work(struct work_struct *work)
{
	struct ringbuf_data *priv = container_of(work, struct ringbuf_data,
			rx_work);
	u32 head, tail, old_tail;

	head = *priv->head;
	old_tail = tail = *priv->tail;

	/* If the tail is invalid, stop here */
	if (tail == ~0UL)
		return;

	/* Ensure we can see all data written before the head was updated */
	rmb();

	if (head < tail) {
		read_and_discard(&priv->base[tail], priv->size - tail);
		tail = 0;
	}

	if (head != tail) {
		read_and_discard(&priv->base[tail], head - tail);
		tail = head;
	}

	/* The tail update must occur after the data has been read. */
	mb();

	/*
	 * We must use cmpxchg to update the tail in case it was reset to 0
	 * by the sending side.
	 */
	if (cmpxchg(priv->tail, old_tail, tail) == old_tail) {
		_okl4_sys_vinterrupt_raise(priv->virqline, VIRQ_FLAG_DATA);
		priv->count += (priv->size + tail - old_tail) % priv->size;
	} else {
		dev_err(&priv->pdev->dev, "Read stale data!\n");
	}
}

static int ringbuf_rx_probe(struct platform_device *pdev)
{
	struct ringbuf_data *priv;
	int r;

	r = ringbuf_probe(pdev, "ringbuf_remote", "ringbuf_rx", &priv);
	if (r < 0)
		return r;

	INIT_WORK(&priv->rx_work, ringbuf_rx_work);

	/* Hard-coded IRQ number; also hard-coded in system XML */
	/* FIXME: Jira ticket SDK-5566 - philipd. */
	priv->irq = 129;
	r = devm_request_irq(&pdev->dev, priv->irq, ringbuf_rx_irq_handler, 0,
			dev_name(&pdev->dev), priv);
	if (r < 0) {
		ringbuf_remove(pdev);
		return r;
	}

	return 0;
}

static struct platform_driver of_platform_ringbuf_rx_driver = {
	.driver = {
		.name = "okl4_ringbuf_rx",
		.owner = THIS_MODULE,
		.of_match_table = of_platform_shared_buffer_table,
	},
	.probe = ringbuf_rx_probe,
	.remove = ringbuf_remove,
};

static int __init ringbuf_init(void)
{
	int err = 0;
	dev_t dev;

	err = alloc_chrdev_region(&dev, 0, 2, "ringbuf");
	if (err)
		goto fail_chrdev_region;

	ringbuf_tx_dev = dev;
	ringbuf_rx_dev = MKDEV(MAJOR(dev), MINOR(dev) + 1);

	ringbuf_tx_class = class_create(THIS_MODULE, "ringbuf_tx");
	if (!ringbuf_tx_class)
		goto fail_class_create;
#ifdef OLD_ATTRS
	ringbuf_tx_class->dev_attrs = ringbuf_tx_test_attrs;
#endif
	err = platform_driver_register(&of_platform_ringbuf_tx_driver);
	if (err)
		goto fail_tx_driver;

	err = platform_driver_register(&of_platform_ringbuf_rx_driver);
	if (err)
		goto fail_rx_driver;

	return 0;

fail_rx_driver:
	platform_driver_unregister(&of_platform_ringbuf_tx_driver);
fail_tx_driver:
	class_destroy(ringbuf_tx_class);
fail_class_create:
	unregister_chrdev_region(ringbuf_tx_dev, 2);
fail_chrdev_region:

	return err;
}
module_init(ringbuf_init);

static void __exit ringbuf_exit(void)
{
	platform_driver_unregister(&of_platform_ringbuf_rx_driver);
	platform_driver_unregister(&of_platform_ringbuf_tx_driver);
	class_destroy(ringbuf_tx_class);
	unregister_chrdev_region(ringbuf_tx_dev, 2);
}
module_exit(ringbuf_exit);

MODULE_AUTHOR("Philip Derrin <philipd@ok-labs.com>");
MODULE_DESCRIPTION("Test driver for OKL4 Microvisor inter-cell communication");
