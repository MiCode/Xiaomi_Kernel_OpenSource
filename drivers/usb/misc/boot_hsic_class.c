/*
 * Boot HSIC Class Driver -
 *
 * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License as
 *	published by the Free Software Foundation, version 2.
 *
 * This driver is based on the 2.6.3 version of drivers/usb/usb-skeleton.c
 * but has been rewritten to be easier to read and use.
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/mutex.h>


/* Version Information */
#define DRIVER_VERSION "v0.1 (2012/01/03)"
#define DRIVER_AUTHOR "Picard Ronan <ronan.picard@renesasmobile.com>"
#define DRIVER_DESC "Modem boot HSIC USB driver for HakuyaS"

/* Define these values to match your devices */
#define USB_RENESAS_VENDOR_ID	0x045B
#define USB_HAKUYAS_PRODUCT_ID	0x0213

/* table of devices that work with this driver */
static const struct usb_device_id renesas_table[] = {
	{ USB_DEVICE(USB_RENESAS_VENDOR_ID, USB_HAKUYAS_PRODUCT_ID) },
	{ }					/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, renesas_table);


/* Get a minor range for your devices from the usb maintainer */
#define USB_MODEM_MINOR_BASE	192

/* our private defines. if this grows any larger, use your own .h file */
#define MAX_TRANSFER		(PAGE_SIZE - 512)
/* MAX_TRANSFER is chosen so that the VM is not stressed by
   allocations > PAGE_SIZE and the number of packets in a page
   is an integer 512 is the largest possible packet on EHCI */
#define WRITES_IN_FLIGHT	8
/* arbitrarily chosen */

/* Structure to hold all of our device specific stuff */
struct boot_hsic {
	/* the usb device for this device */
	struct usb_device	*udev;
	/* the interface for this device */
	struct usb_interface	*interface;
	/* limiting the number of writes in progress */
	struct semaphore	limit_sem;
	/* in case we need to retract our submissions */
	struct usb_anchor	submitted;
	/* the urb to read data with */
	struct urb		*bulk_in_urb;
	/* the buffer to receive data */
	unsigned char           *bulk_in_buffer;
	/* the size of the receive buffer */
	size_t			bulk_in_size;
	/* number of bytes in the buffer */
	size_t			bulk_in_filled;
	/* already copied to user space */
	size_t			bulk_in_copied;
	/* the address of the bulk in endpoint */
	__u8			bulk_in_endpointAddr;
	/* the address of the bulk out endpoint */
	__u8			bulk_out_endpointAddr;
	/* the last request tanked */
	int			errors;
	/* count the number of openers */
	int			open_count;
	/* a read is going on */
	bool			ongoing_read;
	/* indicates we haven't processed the urb */
	bool			processed_urb;
	/* lock for errors */
	spinlock_t		err_lock;

	struct kref		kref;
	/* synchronize I/O with disconnect */
	struct mutex		io_mutex;
	/* to wait for an ongoing read */
	struct completion	bulk_in_completion;
};

static struct usb_driver boot_hsic_driver;

/**
 *	boot_hsic_delete
 */
static void boot_hsic_delete(struct boot_hsic *dev)
{
	printk(KERN_DEBUG "boot_hsic_delete.\n");
	/* free data structures */
	usb_free_urb(dev->bulk_in_urb);
	usb_put_dev(dev->udev);
	kfree(dev->bulk_in_buffer);
	kfree(dev);
}

/**
 *	modem_hsic_open
 */
static int modem_hsic_open(struct inode *inode, struct file *file)
{
	struct boot_hsic *dev = NULL;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);
	printk(KERN_DEBUG "modem_hsic_open");

	interface = usb_find_interface(&boot_hsic_driver, subminor);
	if (!interface) {
		pr_err("%s - error, can't find device for minor %d",
		     __func__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		goto exit;
	}


	/* lock the device to allow correctly handling errors
	 * in resumption */
	mutex_lock(&dev->io_mutex);

	/* allow opening only once */
	if (dev->open_count) {
		retval = -EBUSY;
		mutex_unlock(&dev->io_mutex);
		goto exit;
	}
	dev->open_count = 1;

	/* save our object in the file's private structure */
	file->private_data = dev;
	mutex_unlock(&dev->io_mutex);

exit:
	return retval;
}


/**
 *	modem_hsic_release
 */
static int modem_hsic_release(struct inode *inode, struct file *file)
{
	struct boot_hsic *dev = NULL;
	int retval = 0;

	printk(KERN_DEBUG "modem_hsic_release.\n");
	dev = file->private_data;
	if (dev == NULL)
		return -ENODEV;

	/* allow the device to be autosuspended */
	/* mutex_lock(&dev->io_mutex); */

	if (dev->open_count != 1) {
		dev_err(&dev->udev->dev, "%s - error, device not opened exactly once, %d",
		     __func__, dev->open_count);
		retval = -ENODEV;
		goto unlock_exit;
	}

	if (dev->udev == NULL) {
		/* the device was unplugged before the file was released */
		dev->open_count = 0;
		/* unlock here as frees dev */
		/* mutex_unlock(&dev->io_mutex); */
		boot_hsic_delete(dev);
	} else
		/* decrement the count on our device */
		dev->open_count = 0;

	/* mutex_unlock(&dev->io_mutex); */

	return 0;

unlock_exit:
	/* mutex_unlock(&dev->io_mutex); */
	return retval;
}


static void modem_read_bulk_callback(struct urb *urb)
{
	struct boot_hsic *dev;
	printk(KERN_DEBUG "modem_read_bulk_callback");

	dev = urb->context;

	spin_lock(&dev->err_lock);
	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->udev->dev, "%s - nonzero write bulk status received: %d",
			    __func__, urb->status);

		dev->errors = urb->status;
	} else {
		dev->bulk_in_filled = urb->actual_length;
		printk(KERN_DEBUG "modem_read_bulk_callback, length = %d",
						urb->actual_length);
	}
	dev->ongoing_read = 0;
	spin_unlock(&dev->err_lock);

	complete(&dev->bulk_in_completion);
}

static int modem_do_read_io(struct boot_hsic *dev, size_t count)
{
	int rv;
	printk(KERN_DEBUG "modem_do_read_io");
	/* prepare a read */
	usb_fill_bulk_urb(dev->bulk_in_urb,
			dev->udev,
			usb_rcvbulkpipe(dev->udev,
				dev->bulk_in_endpointAddr),
			dev->bulk_in_buffer,
			min(dev->bulk_in_size, count),
			modem_read_bulk_callback,
			dev);
	/* tell everybody to leave the URB alone */
	spin_lock_irq(&dev->err_lock);
	dev->ongoing_read = 1;
	spin_unlock_irq(&dev->err_lock);

	/* do it */
	rv = usb_submit_urb(dev->bulk_in_urb, GFP_KERNEL);
	if (rv < 0) {
		dev_err(&dev->udev->dev, "%s - failed submitting read urb, error %d",
			__func__, rv);
		dev->bulk_in_filled = 0;
		rv = (rv == -ENOMEM) ? rv : -EIO;
		spin_lock_irq(&dev->err_lock);
		dev->ongoing_read = 0;
		spin_unlock_irq(&dev->err_lock);
	}

	return rv;
}

/**
 *	modem_hsic_read
 */
static ssize_t modem_hsic_read(struct file *file, char *buffer, size_t count,
			       loff_t *ppos)
{
	struct boot_hsic *dev;
	int retval;
	bool ongoing_io;


	printk(KERN_DEBUG "modem_hsic_read.\n");
	dev = file->private_data;
	dev->processed_urb = 0;

	/* verify that the device wasn't unplugged */
	if (dev->udev == NULL) {
		retval = -ENODEV;
		pr_err("%s : No device or device unplugged %d",
						__func__, retval);
		goto exit;
	}

	/* if we cannot read at all, return EOF */
	if (!count) {
		printk(KERN_DEBUG "%s :read request of 0 bytes\n", __func__);
		return 0;
	}

	if (!dev->interface) {		/* disconnect() was called */
		retval = -ENODEV;
		dev_err(&dev->udev->dev, "%s: disconnect %d", __func__, retval);
		goto exit;
	}

	if (!dev->processed_urb) {
		/*
		 * the URB hasn't been processed
		 * do it now
		 */
		dev->processed_urb = 1;

		retval = modem_do_read_io(dev, count);
		if (retval < 0) {
			printk(KERN_DEBUG "%s : modem_do_read_io2, retval = =%d\n",
						__func__, retval);
			goto exit;
		} else
			printk(KERN_DEBUG "%s : modem_do_read_io3, retval = =%d\n",
						__func__, retval);
	}

	/* if IO is under way, we must not touch things */
retry:
	spin_lock_irq(&dev->err_lock);
	ongoing_io = dev->ongoing_read;
	spin_unlock_irq(&dev->err_lock);

	/* errors must be reported */
	retval = dev->errors;
	if (retval < 0) {
		/* any error is reported once */
		dev->errors = 0;
		/* to preserve notifications about reset */
		retval = (retval == -EPIPE) ? retval : -EIO;
		/* no data to deliver */
		dev->bulk_in_filled = 0;
		/* report it */
		goto exit;
	}

	/*
	 * if the buffer is filled we may satisfy the read
	 * else we need to start IO
	 */

	wait_for_completion_interruptible(&dev->bulk_in_completion);

	if (dev->bulk_in_filled) {
		/* we had read data */
		if (copy_to_user(buffer, dev->bulk_in_buffer,
					dev->bulk_in_filled)) {
			retval = -EFAULT;
			printk(KERN_DEBUG "%s : copy_to_user retval =%d\n",
						__func__, retval);
			dev->bulk_in_filled = 0;
			goto exit;
		} else {
			retval = dev->bulk_in_filled;
			printk(KERN_DEBUG "%s : copy_to_user bytes_read =%d\n",
						__func__, dev->bulk_in_filled);
			dev->bulk_in_filled = 0;
			goto success;
		}
	}
	goto retry;
exit:
	return retval;

success:
	usb_free_urb(dev->bulk_in_urb);
	kfree(dev->bulk_in_buffer);

	dev->bulk_in_buffer = kmalloc(dev->bulk_in_size, GFP_KERNEL);
	if (!dev->bulk_in_buffer) {
		dev_err(&dev->udev->dev, "Could not allocate bulk_in_buffer");
		retval = -ENOMEM;
		goto exit;
	}
	dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!dev->bulk_in_urb) {
		dev_err(&dev->udev->dev, "Could not allocate bulk_in_urb");
		retval = -ENOMEM;
		goto exit;
	}
	/* mutex_unlock(&dev->io_mutex); */
	return retval;

}

static void modem_write_bulk_callback(struct urb *urb)
{
	struct boot_hsic *dev;

	/* printk(KERN_DEBUG "modem_write_bulk_callback : enter\n"); */

	dev = urb->context;

	/* sync/async unlink faults aren't errors */
	if (urb->status) {
		if (!(urb->status == -ENOENT ||
		    urb->status == -ECONNRESET ||
		    urb->status == -ESHUTDOWN))
			dev_err(&dev->udev->dev, "%s - nonzero write bulk status received: %d",
			    __func__, urb->status);

		spin_lock(&dev->err_lock);
		dev->errors = urb->status;
		spin_unlock(&dev->err_lock);
	}

	/* free up our allocated buffer */
	usb_free_coherent(urb->dev, urb->transfer_buffer_length,
			  urb->transfer_buffer, urb->transfer_dma);

	up(&dev->limit_sem);
}

/**
 *	modem_hsic_write
 */
static ssize_t modem_hsic_write(struct file *file,
				const char __user *user_buffer,
				size_t count, loff_t *ppos)
{
	struct boot_hsic *dev;
	int retval = 0;
	struct urb *urb = NULL;
	char *buf = NULL;
	size_t writesize = min_t(size_t, count, 512);
	dev = file->private_data;

	/* printk(KERN_DEBUG "modem_hsic_write : enter\n"); */

	/* verify that we actually have some data to write */
	if (count == 0) {
		dev_err(&dev->udev->dev, "modem_hsic_write: count == 0");
		goto exit;
	}

	/*
	 * limit the number of URBs in flight to stop a user from using up all
	 * RAM
	 */
	if (!(file->f_flags & O_NONBLOCK)) {
		retval = -ERESTARTSYS;
		dev_err(&dev->udev->dev, "modem_hsic_write: interruptible retval = %d", retval);
		goto exit;
	} else {
		if (down_trylock(&dev->limit_sem)) {
			retval = -EAGAIN;
			goto exit;
		}
	}

	spin_lock_irq(&dev->err_lock);
	retval = dev->errors;
	if (retval < 0) {
		/* any error is reported once */
		dev->errors = 0;
		/* to preserve notifications about reset */
		retval = (retval == -EPIPE) ? retval : -EIO;
	}
	spin_unlock_irq(&dev->err_lock);
	if (retval < 0) {
		dev_err(&dev->udev->dev, "modem_hsic_write: misc error retval = %d",
						retval);
		goto error;
	}

	/* create a urb, and a buffer for BULK OUT,
	and copy the data to the urb */
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		dev_err(&dev->udev->dev, "modem_hsic_write: urb alloc retval = %d",
						retval);
		goto error;
	}

	buf = usb_alloc_coherent(dev->udev, writesize, GFP_KERNEL,
				 &urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		dev_err(&dev->udev->dev, "modem_hsic_write: usb_alloc_coherent retval = %d",
						retval);
		goto error;
	}

	if (copy_from_user(buf, user_buffer, writesize)) {
		retval = -EFAULT;
		dev_err(&dev->udev->dev, "modem_hsic_write: copy_from_user retval = %d",
						retval);
		goto error;
	}

	/* this lock makes sure we don't submit URBs to gone devices */
	mutex_lock(&dev->io_mutex);
	if (!dev->interface) {		/* disconnect() was called */
		mutex_unlock(&dev->io_mutex);
		dev_err(&dev->udev->dev, "modem_hsic_write: interface retval = %d", retval);
		retval = -ENODEV;
		goto error;
	}

	/* initialize the urb properly */
	usb_fill_bulk_urb(urb, dev->udev,
				usb_sndbulkpipe(dev->udev,
				dev->bulk_out_endpointAddr),
				buf, writesize,
				modem_write_bulk_callback, dev);

	/* urb->transfer_flags |= URB_ZERO_PACKET; */
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);
	mutex_unlock(&dev->io_mutex);
	if (retval) {
		dev_err(&dev->udev->dev, "%s - failed submitting write urb, error %d", __func__,
		    retval);
		goto error;
	}

	/*
	 * release our reference to this urb, the USB core will eventually free
	 * it entirely
	 */
	usb_free_urb(urb);

	return writesize;

error:
	printk(KERN_DEBUG "modem_hsic_write : error\n");
	if (urb) {
		printk(KERN_DEBUG "modem_hsic_write : error free URB\n");

		usb_free_coherent(dev->udev, writesize, buf, urb->transfer_dma);
		usb_free_urb(urb);
	}
	up(&dev->limit_sem);
exit:
	return retval;
}

/* file operations needed when we register this driver */
static const struct file_operations boot_hsic_fops = {
	.owner =	THIS_MODULE,
	.read =		modem_hsic_read,
	.write =	modem_hsic_write,
	.open =		modem_hsic_open,
	.release =	modem_hsic_release,
};

/*
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver boot_hsic_class = {
	.name =		"boot_hsic%d",
	.fops =		&boot_hsic_fops,
	.minor_base =	USB_MODEM_MINOR_BASE,
};

/**
 *	boot_hsic_probe
 *
 *	Called by the usb core when a new device is connected that it thinks
 *	this driver might be interested in.
 */
static int boot_hsic_probe(struct usb_interface *interface,
				const struct usb_device_id *id)
{
	struct boot_hsic *dev = NULL;
	struct usb_device *udev = interface_to_usbdev(interface);
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	size_t buffer_size;
	int i;
	int retval = -ENOMEM;

	/* allocate memory for our device state and initialize it */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&udev->dev, "Out of memory");
		goto error;
	}

	mutex_init(&dev->io_mutex);
	spin_lock_init(&dev->err_lock);
	init_completion(&dev->bulk_in_completion);
	sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);

	dev->udev = usb_get_dev(udev);
	dev->interface = interface;

	/* set up the endpoint information */
	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!dev->bulk_in_endpointAddr &&
		    usb_endpoint_is_bulk_in(endpoint)) {
			/* we found a bulk in endpoint */
			buffer_size = le16_to_cpu(endpoint->wMaxPacketSize);
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				dev_err(&udev->dev, "Could not allocate bulk_in_buffer");
				goto error;
			}
			dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
			if (!dev->bulk_in_urb) {
				dev_err(&udev->dev, "Could not allocate bulk_in_urb");
				goto error;
			}
		}

		if (!dev->bulk_out_endpointAddr &&
		    usb_endpoint_is_bulk_out(endpoint)) {
			/* we found a bulk out endpoint */
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}
	if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
		dev_err(&udev->dev, "Could not find both bulk-in and bulk-out endpoints");
		goto error;
	}

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &boot_hsic_class);
	if (retval) {
		/* something prevented us from registering this driver */
		dev_err(&udev->dev, "Not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* let the user know what node this device is now attached to */
	dev_info(&interface->dev,
		 "USB HAKUYA-S device now attached to USBRenesas-%d"
		 "vid 0x%4.4X pid 0x%4.4X\n",
		 interface->minor,
		 le16_to_cpu(udev->descriptor.idVendor),
		 le16_to_cpu(udev->descriptor.idProduct));
	return 0;

error:
	if (dev)
		/* this frees allocated memory */
		boot_hsic_delete(dev);
	return retval;
}

/**
 *	boot_hsic_disconnect
 *
 *	Called by the usb core when the device is removed from the system.
 */
static void boot_hsic_disconnect(struct usb_interface *interface)
{
	struct boot_hsic *dev;
	int minor = interface->minor;

	printk(KERN_DEBUG "boot_hsic_disconnect.\n");
	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &boot_hsic_class);

	/* prevent more I/O from starting */
	mutex_lock(&dev->io_mutex);
	dev->interface = NULL;
	/* mutex_unlock(&dev->io_mutex); */

	/* if the device is not opened, then we clean up right now */
	if (!dev->open_count) {
		mutex_unlock(&dev->io_mutex);
		boot_hsic_delete(dev);
	} else {
		dev->udev = NULL;
		mutex_unlock(&dev->io_mutex);
	}

	/* decrement our usage count */
	/* boot_hsic_delete(dev); */

	dev_info(&interface->dev,
			"USB Hakuya Renesas Modem #%d now disconnected",
			minor);
}

/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver boot_hsic_driver = {
	.name =		"modem_boot_hsic",
	.probe =	boot_hsic_probe,
	.disconnect =	boot_hsic_disconnect,
	.id_table =	renesas_table,
};

static int __init boot_hsic_init(void)
{
	int result;

	/* register this driver with the USB subsystem */
	result = usb_register(&boot_hsic_driver);
	if (result)
		pr_err("usb_register failed. Error number %d", result);

	return result;
}

static void __exit boot_hsic_exit(void)
{
	/* deregister this driver with the USB subsystem */
	usb_deregister(&boot_hsic_driver);
}

module_init(boot_hsic_init);
module_exit(boot_hsic_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
