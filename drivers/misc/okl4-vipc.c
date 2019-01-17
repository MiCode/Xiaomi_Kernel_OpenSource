/*
 * Simple driver for userspace access to shared memory buffers with
 * associated inter-VM interrupts under the OKL4 Microvisor.
 *
 * Copyright (c) 2016 Cog Systems Pty Ltd.
 * Copyright (c) 2017 General Dynamics.
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
#include <linux/okl4-vipc.h>
#include <linux/ioport.h>
#include <linux/of_address.h>
#include <linux/mm.h>

/* Linux version compatibility */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,11,0)
#define DEVICE_ATTR_RO(_name) \
	struct device_attribute dev_attr_##_name = __ATTR_RO(_name)
#define OLD_ATTRS
#endif

#define OKL4_IPC_MINORBITS	9

#define OKL4_IPC_MINORS	(1 << OKL4_IPC_MINORBITS)
#define OKL4_IPC_MINOR_MAX	(OKL4_IPC_MINORS - 1)

#define OKL4_IPC_MAX_BUFFERS 10

struct virq_data {
	wait_queue_head_t wq;
	int irqno;
	bool raised;
	unsigned long payload;
};

struct source_data {
	okl4_kcap_t kcap;
};

struct vbuf_data {
	u64 addr;
	u64 size;
	u32 flags;
};

struct okl4_vipc_device {
	struct device *dev;
	struct cdev cdev;

	int type;
	struct virq_data virq;
	struct source_data source;
	struct vbuf_data vbuf[OKL4_IPC_MAX_BUFFERS];
	unsigned int num_bufs;

	int minor;
	const char *label;
};

static irqreturn_t okl4_virq_handler(int irq, void *dev_id)
{
	struct okl4_vipc_device *dev = dev_id;
	unsigned long payload;
	struct _okl4_sys_interrupt_get_payload_return _payload =
		_okl4_sys_interrupt_get_payload(irq);

	payload = _payload.payload;
	dev->virq.payload |= payload;
	smp_wmb();
	dev->virq.raised = true;

	wake_up_interruptible(&dev->virq.wq);
	return IRQ_HANDLED;
}

static int okl4_vipc_dev_open(struct inode *inode, struct file *file)
{
	struct okl4_vipc_device *dev;

	dev = container_of(inode->i_cdev, struct okl4_vipc_device, cdev);
	if (!dev)
		return -ENODEV;

	file->private_data = dev;

	return nonseekable_open(inode, file);
}

static int okl4_vipc_dev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t okl4_vipc_read(struct file *file, char __user *buf,
			size_t length,
			loff_t *off)
{
	struct okl4_vipc_device *dev = file->private_data;
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

static unsigned int okl4_vipc_poll(struct file *file, poll_table *table)
{
	int mask = 0;
	struct okl4_vipc_device *dev = file->private_data;

	poll_wait(file, &dev->virq.wq, table);

	if (dev->virq.raised)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static ssize_t assert_virq(struct file *file, const char __user *ubuf,
			 size_t length, loff_t *offset)
{
	struct okl4_vipc_device *dev = file->private_data;
	okl4_error_t err;
	unsigned long payload;

	if (length == 0) {
		payload = 0;
	} else if (length == sizeof(payload)) {
		if (copy_from_user(&payload, ubuf, length)) {
			dev_dbg(dev->dev, "failed to copy data from userland\n");
			return -EFAULT;
		}
	} else {
		dev_dbg(dev->dev, "length != sizeof(payload\n");
		return -EIO;
	}

	err =_okl4_sys_vinterrupt_raise(dev->source.kcap, payload);
	if (err != OKL4_OK) {
		dev_dbg(dev->dev, "failed to raise virtual interrupt\n");
		return  -EIO;
	}

	return length;
}

static long okl4_vipc_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg)
{
	struct okl4_vipc_device *dev = file->private_data;
	struct okl4_vipc_data *data = (struct okl4_vipc_data *)arg;
	int status = 0;

	/* Check user supplied address valid */
	if (_IOC_DIR(cmd) & _IOC_READ) {
                status = !access_ok(VERIFY_WRITE,
			(void __user *)arg, _IOC_SIZE(cmd));
	} else if (_IOC_DIR(cmd) & _IOC_WRITE) {
		status =  !access_ok(VERIFY_READ,
			 (void __user *)arg, _IOC_SIZE(cmd));
	}
	if (status != 0)
		return -EFAULT;


	switch (cmd) {
	case OKL4_VIPC_GET_BUFFER_COUNT:
		*(unsigned long *)arg = dev->num_bufs;
		break;
	case OKL4_VIPC_GET_BUFFER_INFO:
		if (data->index >= 0 && data->index < dev->num_bufs) {
			status = copy_to_user(&data->size,
				&dev->vbuf[data->index].size,
				sizeof(u64));
			if (status != 0)
				return -EFAULT;
			status = copy_to_user(&data->flags,
				&dev->vbuf[data->index].flags,
				sizeof(u32));
			if (status != 0)
				return -EFAULT;
		} else {
			return -EINVAL;
		}
		break;
	default:
		return -ENOTTY;
		break;
	}

	return 0L;
}

static ssize_t is_virq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct okl4_vipc_device *priv = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", priv->virq.irqno >= 0);
}

static ssize_t is_source_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct okl4_vipc_device *priv = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			priv->source.kcap != OKL4_KCAP_INVALID);
}

static ssize_t label_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct okl4_vipc_device *priv = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", priv->label);
}


static int okl4_vipc_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct okl4_vipc_device *dev = file->private_data;
	unsigned long current_index = 0;

	/*
	 * We are hijacking vm_pgoff(virtual memeory page offset)
	 * When it is passed in from user space it is the byte offset
	 * and must be a multiple of page size
	 * By the time it is stored in vm_pgoff it is divided by page size
	 * For example lets say we want to get index 2 in user space it will
	 * call mmap with the offset set to 2 * PAGE_SIZE
	 * Now once we are in kernel space the kernel will do the division
	 * of (2*PAGE_SIZE)/PAGE_SIZE for us thus we just need to store
	 * vm_pgoff as the buffer offset.  Lastly, we need to reset the
	 * vm_pgoff to 0 to ensure the vm_iomap_memory method maps the
	 * entire shared buffer (and only the shared buffer) into user space
	 */
	current_index = vma->vm_pgoff;
	vma->vm_pgoff = 0;

	if (current_index >= dev->num_bufs)
		return -EINVAL;

	if (dev->vbuf[current_index].addr & (PAGE_SIZE - 1))
		return -ENXIO;

	return vm_iomap_memory(vma, dev->vbuf[current_index].addr,
		dev->vbuf[current_index].size);
}

/* Device registration */

/*
 * This file operations structure will be used for a virq device.
 */
static const struct file_operations okl4_vipc_r_fops = {
	.owner = THIS_MODULE,
	.open = okl4_vipc_dev_open,
	.release = okl4_vipc_dev_release,
	.read = okl4_vipc_read,
	.poll = okl4_vipc_poll,
	.unlocked_ioctl = okl4_vipc_ioctl,
	.mmap = okl4_vipc_mmap,
};

/*
 * This file operations structure will be used for a
 * virtual_interrupt_line device.
 */
static const struct file_operations okl4_vipc_w_fops = {
	.owner = THIS_MODULE,
	.open = okl4_vipc_dev_open,
	.release = okl4_vipc_dev_release,
	.write = assert_virq,
	.unlocked_ioctl = okl4_vipc_ioctl,
	.mmap = okl4_vipc_mmap,
};

/*
 * This file operations structure will be used for a combined
 * virq+source device.
 */
static const struct file_operations okl4_vipc_rw_fops = {
	.owner = THIS_MODULE,
	.open = okl4_vipc_dev_open,
	.release = okl4_vipc_dev_release,
	.read = okl4_vipc_read,
	.poll = okl4_vipc_poll,
	.write = assert_virq,
	.unlocked_ioctl = okl4_vipc_ioctl,
	.mmap = okl4_vipc_mmap,
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

static struct class *okl4_vipc_class;
static dev_t okl4_vipc_devt;
static DEFINE_MUTEX(okl4_vipc_minor_lock);
static unsigned long okl4_vipc_ids[BITS_TO_LONGS(OKL4_IPC_MINORBITS)];

/**
 * okl4_vipc_minor_get - obtain next free device minor number
 *
 * @dev:  device pointer
 *
 * Return: allocated minor, or -ENOSPC if no free minor left
 */
static int okl4_vipc_minor_get(struct okl4_vipc_device *dev)
{
	int ret;

	mutex_lock(&okl4_vipc_minor_lock);

	ret = find_first_zero_bit(okl4_vipc_ids, OKL4_IPC_MINORS);

	if (ret >= OKL4_IPC_MINORS) {
		dev_err(dev->dev, "too many okl4 ipc devices\n");
		ret = -ENODEV;
	} else {
		dev->minor = ret;
		set_bit(ret, okl4_vipc_ids);
		ret = 0;
	}
	mutex_unlock(&okl4_vipc_minor_lock);
	return ret;
}

/**
 * okl4_vipc_minor_free - mark device minor number as free
 *
 * @dev:  device pointer
 */
static void okl4_vipc_minor_free(struct okl4_vipc_device *dev)
{
	mutex_lock(&okl4_vipc_minor_lock);
	clear_bit(dev->minor, okl4_vipc_ids);
	mutex_unlock(&okl4_vipc_minor_lock);
}

static void okl4_vipc_dev_deregister(struct okl4_vipc_device *dev)
{
	int devno;

	devno = dev->cdev.dev;
	cdev_del(&dev->cdev);

	device_destroy(okl4_vipc_class, devno);

	okl4_vipc_minor_free(dev);
}

static int okl4_vipc_probe(struct platform_device *pdev)
{
	int ret, devno, irq;
	okl4_kcap_t kcap;
	struct okl4_vipc_device *dev;
	struct device *parent;
	struct device *clsdev; /* class device */
	const struct file_operations *fops;
	struct device_node *node;
	const char *devname = "vipc%d";
	u32 all_flags[OKL4_IPC_MAX_BUFFERS];

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

	ret = okl4_vipc_minor_get(dev);
	if (ret < 0)
		goto err;

	if (irq >= 0) {
		ret = devm_request_irq(parent, irq, okl4_virq_handler,
				IRQF_TRIGGER_RISING, dev_name(parent), dev);
		if (ret < 0)
			goto err_free_minor;
	}

	/* Set number of buffers to 0 */
	dev->num_bufs = 0;
	kcap = OKL4_KCAP_INVALID;
	if (of_device_is_compatible(node,
		"okl,microvisor-shared-memory-ipc")) {
		struct device_node *virqline_node = NULL;
		struct resource res;

		/*
		 * loop through all avaible indexes and make sure not to go
		 * above 9 (10 with 0 based index)
		 */
		int reg_index = 0;
		while(of_address_to_resource(node, reg_index, &res) >=0 &&
			reg_index <= (OKL4_IPC_MAX_BUFFERS-1))
		{
			/*
			 * Get the start address of the shmem and calculate its
			 * size
			 */
			dev->vbuf[reg_index].addr = res.start;
			dev->vbuf[reg_index].size = (res.end - res.start) + 1;
			dev->num_bufs++;

			reg_index++;
		}

		/*
		 * We didnt find any registers on any index
		 * this is bad, set errno and cleanup
		 */
		if (dev->num_bufs == 0) {
			ret = -ENODEV;
			goto err_free_minor;
		}

		/*
		 * Next, get the access flags
		 * On older kernels we cant use of_property_read_u32_index
		 * since its not there.  So we have to wait until we
		 * know the number of buffers.  Once we know that we
		 * can get an array of the number of buffers size
		 * Then we itterate through that array and store them
		 * in their correct spot
		 */
		ret = of_property_read_u32_array(node, "okl,rwx",
			all_flags, dev->num_bufs);
		if (ret < 0)
			goto err_free_minor;

		for (reg_index=0; reg_index < dev->num_bufs; reg_index++)
		{
			dev->vbuf[reg_index].flags = all_flags[reg_index];
		}

		/* Find the virqline node that corresponds to this node */
		virqline_node = of_parse_phandle(node, "virqline", 0);
		if (!virqline_node) {
			ret = -ENODEV;
			goto err_free_minor;
		}
		ret = of_property_read_u32(virqline_node, "reg", &kcap);
		if (ret < 0)
			goto err_free_minor;
	}
	dev->source.kcap = kcap;

	ret = of_property_read_string(node, "label", &dev->label);
	if (ret != 0) {
		ret = -ENODEV;
                goto err_free_minor;
        }

	/* Fill in the data structures */
	devno = MKDEV(MAJOR(okl4_vipc_devt), dev->minor);

	ret = -ENODEV;
	if (irq >= 0 && kcap != OKL4_KCAP_INVALID)
		fops = &okl4_vipc_rw_fops;
	else if (irq >= 0)
		fops = &okl4_vipc_r_fops;
	else if (kcap != OKL4_KCAP_INVALID)
		fops = &okl4_vipc_w_fops;
	else
		goto err_free_minor;

	cdev_init(&dev->cdev, fops);
	dev->cdev.owner = fops->owner;

	/* Add the device */
	ret = cdev_add(&dev->cdev, devno, 1);
	if (ret) {
		dev_err(parent, "unable to add device %d:%d\n",
			MAJOR(okl4_vipc_devt), dev->minor);
		goto err_free_minor;
	}

#ifndef OLD_ATTRS
	clsdev = device_create_with_groups(okl4_vipc_class, parent, devno,
			dev, virq_groups, devname, dev->minor);
#else
	clsdev = device_create(okl4_vipc_class, parent, devno, dev, devname,
			dev->minor);
#endif
	if (IS_ERR(clsdev)) {
		dev_err(parent, "unable to create device %d:%d\n",
			MAJOR(okl4_vipc_devt), dev->minor);
		ret = PTR_ERR(clsdev);
		goto err_dev_create;
	}
	dev_info(parent, "using /dev/vipc%d\n", dev->minor);
	dev_set_drvdata(parent, dev);

	return 0;

err_dev_create:
	cdev_del(&dev->cdev);
err_free_minor:
	okl4_vipc_minor_free(dev);
err:
	devm_kfree(&pdev->dev, dev);
	return ret;
}

static int okl4_vipc_remove(struct platform_device *pdev)
{
	struct okl4_vipc_device *priv = dev_get_drvdata(&pdev->dev);

	okl4_vipc_dev_deregister(priv);

	return 0;
}

/* Driver registration */

static const struct of_device_id of_plat_okl4_vipc_table[] = {
	{ .compatible = "okl,microvisor-shared-memory-ipc" },
	{ /* end */ },
};

static struct platform_driver of_plat_okl4_vipc_driver = {
	.driver = {
		.name = "okl4_vipc",
		.of_match_table = of_plat_okl4_vipc_table,
	},
	.probe = okl4_vipc_probe,
	.remove = okl4_vipc_remove,
};

static int __init okl4_vipc_init(void)
{
	int ret;

	memset(okl4_vipc_ids, 0, sizeof(okl4_vipc_ids));

	okl4_vipc_class = class_create(THIS_MODULE, "okl4_vipc");
	if (IS_ERR(okl4_vipc_class)) {
		pr_err("failed to create class\n");
		ret = PTR_ERR(okl4_vipc_class);
		goto err;
	}
#ifdef OLD_ATTRS
	okl4_vipc_class->dev_attrs = virq_attrs;
#endif

	ret = alloc_chrdev_region(&okl4_vipc_devt, 0, MINORMASK,
		"okl4_vipc");
	if (ret < 0) {
		pr_err("failed to allocate char dev region\n");
		goto err_class;
	}

	ret = platform_driver_register(&of_plat_okl4_vipc_driver);
	if (ret)
		goto err_vipc_driver;

	return 0;

err_vipc_driver:
	unregister_chrdev_region(okl4_vipc_devt, MINORMASK);
err_class:
	class_destroy(okl4_vipc_class);
err:
	return ret;
}

static void __exit okl4_vipc_exit(void)
{
	platform_driver_unregister(&of_plat_okl4_vipc_driver);

	unregister_chrdev_region(okl4_vipc_devt, MINORMASK);
	class_destroy(okl4_vipc_class);
}

module_init(okl4_vipc_init);
module_exit(okl4_vipc_exit);

MODULE_AUTHOR("General Dynamics");
MODULE_DESCRIPTION("OKL4 userspace VIPC interface");
