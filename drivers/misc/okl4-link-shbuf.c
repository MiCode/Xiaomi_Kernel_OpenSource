/*
 * Driver for inter-cell links using the shared-buffer transport.
 *
 * Copyright (c) 2016 Cog Systems Pty Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */
#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/poll.h>
#include <linux/rwsem.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/version.h>
#include <microvisor/microvisor.h>
#include <uapi/linux/okl4-link-shbuf.h>

static const char DEVICE_NAME[] = "okl4_link_shbuf";

/* Created devices will appear as /dev/<DEV_PREFIX><name> */
static const char DEV_PREFIX[] = "okl4-";

static const struct of_device_id okl4_link_shbuf_match[] = {
	{
		.compatible = "okl,microvisor-link-shbuf",
	},
	{},
};
MODULE_DEVICE_TABLE(of, okl4_link_shbuf_match);

static struct class *link_shbuf_class;
static dev_t link_shbuf_dev;

/* A lock used to protect access to link_shbuf_dev */
static spinlock_t device_number_allocate;

/* Sentinel values for indicating missing communication channels */
static const u32 NO_OUTGOING_IRQ = 0;
static const int NO_INCOMING_IRQ = -1;

/* Private data for this driver */
struct link_shbuf_data {

	/* Outgoing vIRQ */
	u32 virqline;

	/* Incoming vIRQ */
	int virq;
	atomic64_t virq_payload;
	bool virq_pending;
	wait_queue_head_t virq_wq;

	/* Shared memory region */
	void *base;
	fmode_t permissions;
	struct resource buffer;

	/* Device data */
	dev_t devt;
	struct device *dev;
	struct cdev cdev;

};

static bool link_shbuf_data_invariant(const struct link_shbuf_data *priv)
{
	if (!priv)
		return false;

	if (!priv->base || (uintptr_t)priv->base % PAGE_SIZE != 0)
		return false;

	if (resource_size(&priv->buffer) == 0)
		return false;

	if (!priv->dev)
		return false;

	return true;
}

static bool link_shbuf_valid_access(size_t size, loff_t pos, size_t count)
{
	return pos < size && count <= size && size - count >= pos;
}

static ssize_t link_shbuf_read(struct file *file, char __user *buffer,
		size_t count, loff_t *ppos)
{
	long remaining;
	const struct link_shbuf_data *priv;

	/* The file should have been opened with read access to reach here */
	if (WARN_ON(!(file->f_mode & FMODE_READ)))
		return -EINVAL;

	priv = file->private_data;
	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return -EINVAL;

	if (!link_shbuf_valid_access(resource_size(&priv->buffer), *ppos, count))
		return -EINVAL;

	remaining = copy_to_user(buffer, priv->base + *ppos, count);
	*ppos += count - remaining;
	return count - remaining;
}

static ssize_t link_shbuf_write(struct file *file, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	long remaining;
	const struct link_shbuf_data *priv;

	/* The file should have been opened with write access to reach here */
	if (WARN_ON(!(file->f_mode & FMODE_WRITE)))
		return -EINVAL;

	priv = file->private_data;
	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return -EINVAL;

	if (!link_shbuf_valid_access(resource_size(&priv->buffer), *ppos, count))
		return -EINVAL;

	remaining = copy_from_user(priv->base + *ppos, buffer, count);
	*ppos += count - remaining;
	return count - remaining;
}

static unsigned int link_shbuf_poll(struct file *file, poll_table *table)
{
	struct link_shbuf_data *priv;
	unsigned int mask;

	priv = file->private_data;
	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return POLLERR;

	poll_wait(file, &priv->virq_wq, table);

	/* The shared memory is always considered ready for reading and writing. */
	mask = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM;

	if (priv->virq_pending)
		mask |= POLLPRI;

	return mask;
}

static long link_shbuf_ioctl_irq_tx(const struct link_shbuf_data *priv,
		unsigned long arg)
{
	okl4_error_t err;
	u64 payload;
	const u64 __user *user_arg = (const u64 __user*)arg;

	if (priv->virqline == NO_OUTGOING_IRQ)
		return -EINVAL;

#if defined(CONFIG_ARM) && (LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0))
	if (copy_from_user(&payload, user_arg, sizeof(payload)))
                return -EFAULT;
#else
	if (get_user(payload, user_arg))
		return -EFAULT;
#endif

	err = _okl4_sys_vinterrupt_raise(priv->virqline, payload);
	if (WARN_ON(err != OKL4_OK))
		return -EINVAL;

	return 0;
}

static long link_shbuf_ioctl_irq_clr(struct link_shbuf_data *priv,
		unsigned long arg)
{
	u64 payload;
	u64 __user *user_arg = (u64 __user*)arg;

	/*
	 * Check validity of the user pointer before clearing the interrupt to avoid
	 * races involved with having to undo the latter.
	 */
	if (!access_ok(VERIFY_WRITE, user_arg, sizeof(*user_arg)))
		return -EFAULT;

	/*
	 * Note that the clearing of the pending flag can race with the setting of
	 * this flag in the IRQ handler. It is up to the user to coordinate these
	 * actions.
	 */
	priv->virq_pending = false;
	smp_rmb();
	payload = atomic64_xchg(&priv->virq_payload, 0);

	/* We've already checked that this access is OK, so no need for put_user. */
	if (__put_user(payload, user_arg))
		return -EFAULT;

	return 0;
}

static long link_shbuf_ioctl(struct file *file, unsigned int request,
		unsigned long arg)
{
	struct link_shbuf_data *priv;

	priv = file->private_data;
	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return -EINVAL;

	/* We only support two ioctls */
	switch (request) {

	case OKL4_LINK_SHBUF_IOCTL_IRQ_TX:
		return link_shbuf_ioctl_irq_tx(priv, arg);

	case OKL4_LINK_SHBUF_IOCTL_IRQ_CLR:
		return link_shbuf_ioctl_irq_clr(priv, arg);

	}

	/*
	 * Handy for debugging when userspace is linking against ioctl headers from
	 * a different kernel revision.
	 */
	dev_dbg(priv->dev, "ioctl request 0x%x received which did not match either "
		"OKL4_LINK_SHBUF_IOCTL_IRQ_TX (0x%x) or OKL4_LINK_SHBUF_IOCTL_IRQ_CLR "
		"(0x%x)\n", request, (unsigned)OKL4_LINK_SHBUF_IOCTL_IRQ_TX,
		(unsigned)OKL4_LINK_SHBUF_IOCTL_IRQ_CLR);

	return -EINVAL;
}

static int link_shbuf_mmap(struct file *file, struct vm_area_struct *vma)
{
	const struct link_shbuf_data *priv;
	unsigned long offset, pfn, flags;
	size_t size;
	pgprot_t prot;

	/* Our caller should have taken the MM semaphore. */
	if (WARN_ON(!rwsem_is_locked(&vma->vm_mm->mmap_sem)))
		return -EINVAL;

	/*
	 * The file should have been opened with a superset of the mmap requested
	 * permissions.
	 */
	flags = vma->vm_flags;
	if (WARN_ON((flags & VM_READ) && !(file->f_mode & FMODE_READ)))
		return -EINVAL;
	if (WARN_ON((flags & VM_WRITE) && !(file->f_mode & FMODE_WRITE)))
		return -EINVAL;
	if (WARN_ON((flags & VM_EXEC) && !(file->f_mode & FMODE_EXEC)))
		return -EINVAL;

	/* Retrieve our private data. */
	priv = file->private_data;
	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return -EINVAL;

	/* Check the mmap request is within bounds. */
	size = vma->vm_end - vma->vm_start;
	offset = vma->vm_pgoff << PAGE_SHIFT;
	if (!link_shbuf_valid_access(resource_size(&priv->buffer), offset, size))
		return -EINVAL;

	pfn = (priv->buffer.start + offset) >> PAGE_SHIFT;
	prot = vm_get_page_prot(flags);

	return remap_pfn_range(vma, vma->vm_start, pfn, size, prot);
}

static bool link_shbuf_access_ok(fmode_t allowed, fmode_t request)
{
	static const fmode_t ACCESS_MASK = FMODE_READ|FMODE_WRITE|FMODE_EXEC;
	fmode_t relevant = request & ACCESS_MASK;
	return (relevant & allowed) == relevant;
}

static int link_shbuf_open(struct inode *inode, struct file *file)
{
	struct cdev *cdev;
	struct link_shbuf_data *priv;

	/* Retrieve a pointer to our private data */
	cdev = inode->i_cdev;
	priv = container_of(cdev, struct link_shbuf_data, cdev);
	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return -EINVAL;

	if (!link_shbuf_access_ok(priv->permissions, file->f_mode))
		return -EACCES;

	file->private_data = priv;

	return 0;
}

static const struct file_operations link_shbuf_ops = {
	.owner = THIS_MODULE,
	.read = link_shbuf_read,
	.write = link_shbuf_write,
	.poll = link_shbuf_poll,
	.unlocked_ioctl = link_shbuf_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = link_shbuf_ioctl,
#endif
#ifdef CONFIG_MMU
	.mmap = link_shbuf_mmap,
#endif
	.open = link_shbuf_open,
};

/*
 * Interrupt handler.
 *
 * This function will be called when our link partner uses the ioctl on their
 * shared memory device to send an outgoing interrupt.
 */
static irqreturn_t link_shbuf_irq_handler(int irq, void *data)
{
	u64 payload, old, new;
	struct _okl4_sys_interrupt_get_payload_return _payload;

	/* Retrieve a pointer to our private data. */
	struct link_shbuf_data *priv = data;
	if (WARN_ON(!link_shbuf_data_invariant(priv)))
		return IRQ_NONE;

	/*
	 * We should only ever be handling a single interrupt, and only if there
	 * was an incoming interrupt in the configuration.
	 */
	if (WARN_ON(priv->virq < 0 || priv->virq != irq))
		return IRQ_NONE;

	_payload = _okl4_sys_interrupt_get_payload(irq);
	payload = (u64)_payload.payload;

	/*
	 * At this point, it is possible the pending flag is already set. It is up to
	 * the user to synchronise their transmission and acknowledgement of
	 * interrupts.
	 */

	/* We open code atomic64_or which is not universally available. */
	do {
		old = atomic64_read(&priv->virq_payload);
		new = old | payload;
	} while (atomic64_cmpxchg(&priv->virq_payload, old, new) != old);
	smp_wmb();
	priv->virq_pending = true;

	wake_up_interruptible(&priv->virq_wq);

	return IRQ_HANDLED;
}

/*
 * Allocate a unique device number for this device.
 *
 * Note that this function needs to lock its access to link_shbuf_dev as there
 * may be multiple threads attempting to acquire a new device number.
 */
static int link_shbuf_allocate_device(dev_t *devt)
{
	int ret = 0;
	dev_t next;

	spin_lock(&device_number_allocate);

	*devt = link_shbuf_dev;
	next = MKDEV(MAJOR(link_shbuf_dev), MINOR(link_shbuf_dev) + 1);
	/* Check for overflow */
	if (MINOR(next) != MINOR(link_shbuf_dev) + 1)
		ret = -ENOSPC;
	else
		link_shbuf_dev = next;

	spin_unlock(&device_number_allocate);

	return ret;
}

/*
 * Discover and add a new shared-buffer link.
 *
 * In the following function, we are expecting to parse device tree entries
 * looking like the following:
 *
 *	hypervisor {
 *		...
 *		interrupt-line@1d {
 *				compatible = "okl,microvisor-interrupt-line",
 *				"okl,microvisor-capability";
 *			phandle = <0x7>;
 *			reg = <0x1d>;
 *			label = "foo_virqline";
 *		};
 *	 ;
 *
 *	foo@41003000 {
 *		compatible = "okl,microvisor-link-shbuf",
 *			"okl,microvisor-shared-memory";
 *		phandle = <0xd>;
 *		reg = <0x0 0x41003000 0x2000>;
 *		label = "foo";
 *		okl,rwx = <0x6>;
 *		okl,interrupt-line = <0x7>;
 *		interrupts = <0x0 0x4 0x1>;
 *		interrupt-parent = <0x1>;
 *	};
 */
static int link_shbuf_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *node, *virqline;
	struct link_shbuf_data *priv;
	const char *name;
	u32 permissions;

	node = pdev->dev.of_node;

	if (!node)
		return -ENODEV;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	/*
	 * Retrieve the outgoing vIRQ cap. Note, this is configurable and we
	 * anticipate that it may not exist.
	 */
	virqline = of_parse_phandle(node, "okl,interrupt-line", 0);
	if (!virqline) {
		priv->virqline = NO_OUTGOING_IRQ;
	} else {
		ret = of_property_read_u32(virqline, "reg", &priv->virqline);
		if (ret < 0 || priv->virqline == OKL4_KCAP_INVALID) {
			of_node_put(virqline);
			ret = -ENODEV;
			goto err_free_dev;
		}
	}
	of_node_put(virqline);

	/* Retrieve the incoming vIRQ number. Again, this is configurable and we
	 * anticipate that it may not exist.
	 */
	priv->virq = platform_get_irq(pdev, 0);
	if (priv->virq < 0)
		priv->virq = NO_INCOMING_IRQ;

	/* If we have a valid incoming vIRQ, register to handle it. */
	if (priv->virq >= 0) {
		ret = devm_request_irq(&pdev->dev, priv->virq, link_shbuf_irq_handler,
			0, dev_name(&pdev->dev), priv);
		if (ret < 0) {
			dev_err(&pdev->dev, "failed request for IRQ\n");
			goto err_free_dev;
		}
	}

	init_waitqueue_head(&priv->virq_wq);
	priv->virq_pending = false;

	/* Retrieve information about the shared memory region. */
	ret = of_address_to_resource(node, 0, &priv->buffer);
	if (ret < 0)
		goto err_free_irq;
	/*
	 * We expect the Elfweaver to have validated that we have a non-NULL,
	 * page-aligned region.
	 */
	if (WARN_ON(priv->buffer.start == 0) ||
			WARN_ON(resource_size(&priv->buffer) % PAGE_SIZE != 0))
		goto err_free_irq;
	if (!devm_request_mem_region(&pdev->dev, priv->buffer.start,
			resource_size(&priv->buffer), dev_name(&pdev->dev))) {
		ret = -ENODEV;
		goto err_free_irq;
	}
	priv->base = devm_ioremap(&pdev->dev, priv->buffer.start,
			resource_size(&priv->buffer));
	if (!priv->base)
		goto err_release_region;

	/* Read the permissions of the shared memory region. */
	ret = of_property_read_u32(node, "okl,rwx", &permissions);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to read shared memory permissions\n");
		goto err_unmap_dev;
	}
	if (permissions & ~S_IRWXO) {
		ret = -EINVAL;
		goto err_unmap_dev;
	}
	priv->permissions = ((permissions & S_IROTH) ? FMODE_READ : 0) |
			((permissions & S_IWOTH) ? FMODE_WRITE : 0) |
			((permissions & S_IXOTH) ? FMODE_EXEC : 0);
	if (WARN_ON(priv->permissions == 0)) {
		ret = -EINVAL;
		goto err_unmap_dev;
	}

	/* Retrieve the label of this device. This will be the "name" attribute of
	 * the corresponding "link" tag in the system's XML specification.
	 */
	ret = of_property_read_string(node, "label", &name);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to read label\n");
		goto err_unmap_dev;
	}

	cdev_init(&priv->cdev, &link_shbuf_ops);
	ret = cdev_add(&priv->cdev, link_shbuf_dev, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to add char dev region\n");
		goto err_unmap_dev;
	}

	ret = link_shbuf_allocate_device(&priv->devt);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to allocate new device number\n");
		goto err_unmap_dev;
	}

	/* We're now ready to create the device itself. */
	BUG_ON(name == NULL);
	priv->dev = device_create(link_shbuf_class, &pdev->dev, priv->devt,
		priv, "%s%s", DEV_PREFIX, name);
	if (IS_ERR(priv->dev)) {
		dev_err(&pdev->dev, "failed to create device\n");
		ret = PTR_ERR(priv->dev);
		goto err_del_dev;
	}

	dev_set_drvdata(&pdev->dev, priv);

	return 0;

err_del_dev:
	cdev_del(&priv->cdev);
err_unmap_dev:
	devm_iounmap(&pdev->dev, priv->base);
err_release_region:
	devm_release_mem_region(&pdev->dev, priv->buffer.start,
			resource_size(&priv->buffer));
err_free_irq:
	if (priv->virq != NO_INCOMING_IRQ)
		devm_free_irq(&pdev->dev, priv->virq, priv);
err_free_dev:
	devm_kfree(&pdev->dev, priv);
	return ret;
}

static int link_shbuf_remove(struct platform_device *pdev)
{
	struct link_shbuf_data *priv;

	priv = dev_get_drvdata(&pdev->dev);
	WARN_ON(!link_shbuf_data_invariant(priv));

	device_destroy(link_shbuf_class, priv->devt);

	cdev_del(&priv->cdev);

	/*
	 * None of the following is strictly required, as these are all managed
	 * resources, but we clean it up anyway for clarity.
	 */

	devm_iounmap(&pdev->dev, priv->base);

	devm_release_mem_region(&pdev->dev, priv->buffer.start,
			resource_size(&priv->buffer));

	if (priv->virq != NO_INCOMING_IRQ)
		devm_free_irq(&pdev->dev, priv->virq, priv);

	devm_kfree(&pdev->dev, priv);

	return 0;
}

static struct platform_driver of_plat_link_shbuf_driver = {
	.driver = {
		.name = "okl4-shbuf",
		.of_match_table = okl4_link_shbuf_match,
	},
	.probe = link_shbuf_probe,
	.remove = link_shbuf_remove,
};

/* Maximum number of minor device numbers */
enum {
	MAX_MINOR = 1 << MINORBITS,
};

static int __init okl4_link_shbuf_init(void)
{
	int ret;

	link_shbuf_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(link_shbuf_class)) {
		pr_err("failed to create class\n");
		ret = PTR_ERR(link_shbuf_class);
		return ret;
	}

	ret = alloc_chrdev_region(&link_shbuf_dev, 0, MAX_MINOR, DEVICE_NAME);
	if (ret < 0) {
		pr_err("failed to allocate char dev region\n");
		goto err_destroy_class;
	}

	ret = platform_driver_register(&of_plat_link_shbuf_driver);
	if (ret < 0) {
		pr_err("failed to register driver\n");
		goto err_unregister_dev_region;
	}

	spin_lock_init(&device_number_allocate);

	return 0;

err_unregister_dev_region:
	unregister_chrdev_region(link_shbuf_dev, MAX_MINOR);
err_destroy_class:
	class_destroy(link_shbuf_class);
	return ret;
}
module_init(okl4_link_shbuf_init);

static void __exit okl4_link_shbuf_exit(void)
{
	platform_driver_unregister(&of_plat_link_shbuf_driver);
	unregister_chrdev_region(link_shbuf_dev, MAX_MINOR);
	class_destroy(link_shbuf_class);
}
module_exit(okl4_link_shbuf_exit);

MODULE_DESCRIPTION("OKL4 shared buffer link driver");
MODULE_AUTHOR("Cog Systems Pty Ltd");
