/*
 * Gadget Driver for Android MDB
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/miscdevice.h>

#define MDB_BULK_BUFFER_SIZE           4096

#define TX_REQ_MAX 4

static const char mdb_shortname[] = "android_mdb";

struct mdb_dev {
	struct usb_function function;
	struct usb_composite_dev *cdev;
	spinlock_t lock;

	struct usb_ep *ep_in;
	struct usb_ep *ep_out;

	atomic_t online;
	atomic_t error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;

	struct list_head tx_idle;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;
	struct usb_request *rx_req;
	int rx_done;
	bool notify_close;
	bool close_notified;
};

static struct usb_interface_descriptor mdb_interface_desc = {
	.bLength                = USB_DT_INTERFACE_SIZE,
	.bDescriptorType        = USB_DT_INTERFACE,
	.bInterfaceNumber       = 0,
	.bNumEndpoints          = 2,
	.bInterfaceClass        = 0xFF,
	.bInterfaceSubClass     = 0x43,
	.bInterfaceProtocol     = 1,
};

static struct usb_endpoint_descriptor mdb_highspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mdb_highspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize         = __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor mdb_fullspeed_in_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_IN,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor mdb_fullspeed_out_desc = {
	.bLength                = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType        = USB_DT_ENDPOINT,
	.bEndpointAddress       = USB_DIR_OUT,
	.bmAttributes           = USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_mdb_descs[] = {
	(struct usb_descriptor_header *) &mdb_interface_desc,
	(struct usb_descriptor_header *) &mdb_fullspeed_in_desc,
	(struct usb_descriptor_header *) &mdb_fullspeed_out_desc,
	NULL,
};

static struct usb_descriptor_header *hs_mdb_descs[] = {
	(struct usb_descriptor_header *) &mdb_interface_desc,
	(struct usb_descriptor_header *) &mdb_highspeed_in_desc,
	(struct usb_descriptor_header *) &mdb_highspeed_out_desc,
	NULL,
};

static void mdb_ready_callback(void);
static void mdb_closed_callback(void);

static struct mdb_dev *_mdb_dev;

static inline struct mdb_dev *func_to_mdb(struct usb_function *f)
{
	return container_of(f, struct mdb_dev, function);
}


static struct usb_request *mdb_request_new(struct usb_ep *ep, int buffer_size)
{
	struct usb_request *req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	req->buf = kmalloc(buffer_size, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}

	return req;
}

static void mdb_request_free(struct usb_request *req, struct usb_ep *ep)
{
	if (req) {
		kfree(req->buf);
		usb_ep_free_request(ep, req);
	}
}

static inline int mdb_lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void mdb_unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

void mdb_req_put(struct mdb_dev *dev, struct list_head *head,
		struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&dev->lock, flags);
}

struct usb_request *mdb_req_get(struct mdb_dev *dev, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&dev->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&dev->lock, flags);
	return req;
}

static void mdb_complete_in(struct usb_ep *ep, struct usb_request *req)
{
	struct mdb_dev *dev = _mdb_dev;

	if (req->status != 0)
		atomic_set(&dev->error, 1);

	mdb_req_put(dev, &dev->tx_idle, req);

	wake_up(&dev->write_wq);
}

static void mdb_complete_out(struct usb_ep *ep, struct usb_request *req)
{
	struct mdb_dev *dev = _mdb_dev;

	dev->rx_done = 1;
	if (req->status != 0 && req->status != -ECONNRESET)
		atomic_set(&dev->error, 1);

	wake_up(&dev->read_wq);
}

static int mdb_create_bulk_endpoints(struct mdb_dev *dev,
				struct usb_endpoint_descriptor *in_desc,
				struct usb_endpoint_descriptor *out_desc)
{
	struct usb_composite_dev *cdev = dev->cdev;
	struct usb_request *req;
	struct usb_ep *ep;
	int i;

	DBG(cdev, "create_bulk_endpoints dev: %p\n", dev);

	ep = usb_ep_autoconfig(cdev->gadget, in_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_in failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for ep_in got %s\n", ep->name);
	ep->driver_data = dev;
	dev->ep_in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, out_desc);
	if (!ep) {
		DBG(cdev, "usb_ep_autoconfig for ep_out failed\n");
		return -ENODEV;
	}
	DBG(cdev, "usb_ep_autoconfig for mdb ep_out got %s\n", ep->name);
	ep->driver_data = dev;
	dev->ep_out = ep;

	req = mdb_request_new(dev->ep_out, MDB_BULK_BUFFER_SIZE);
	if (!req)
		goto fail;
	req->complete = mdb_complete_out;
	dev->rx_req = req;

	for (i = 0; i < TX_REQ_MAX; i++) {
		req = mdb_request_new(dev->ep_in, MDB_BULK_BUFFER_SIZE);
		if (!req)
			goto fail;
		req->complete = mdb_complete_in;
		mdb_req_put(dev, &dev->tx_idle, req);
	}

	return 0;

fail:
	printk(KERN_ERR "mdb_bind() could not allocate requests\n");
	return -1;
}

static ssize_t mdb_read(struct file *fp, char __user *buf,
				size_t count, loff_t *pos)
{
	struct mdb_dev *dev = fp->private_data;
	struct usb_request *req;
	int r = count, xfer;
	int ret;

	pr_debug("mdb_read(%lu)\n", count);
	if (!_mdb_dev)
		return -ENODEV;

	if (count > MDB_BULK_BUFFER_SIZE)
		return -EINVAL;

	if (mdb_lock(&dev->read_excl))
		return -EBUSY;

	while (!(atomic_read(&dev->online) || atomic_read(&dev->error))) {
		pr_debug("mdb_read: waiting for online state\n");
		ret = wait_event_interruptible(dev->read_wq,
			(atomic_read(&dev->online) ||
			atomic_read(&dev->error)));
		if (ret < 0) {
			mdb_unlock(&dev->read_excl);
			return ret;
		}
	}
	if (atomic_read(&dev->error)) {
		r = -EIO;
		goto done;
	}

requeue_req:
	req = dev->rx_req;
	req->length = MDB_BULK_BUFFER_SIZE;
	dev->rx_done = 0;
	ret = usb_ep_queue(dev->ep_out, req, GFP_ATOMIC);
	if (ret < 0) {
		pr_debug("mdb_read: failed to queue req %p (%d)\n", req, ret);
		r = -EIO;
		atomic_set(&dev->error, 1);
		goto done;
	} else {
		pr_debug("rx %p queue\n", req);
	}

	ret = wait_event_interruptible(dev->read_wq, dev->rx_done ||
				atomic_read(&dev->error));
	if (ret < 0) {
		if (ret != -ERESTARTSYS)
		atomic_set(&dev->error, 1);
		r = ret;
		usb_ep_dequeue(dev->ep_out, req);
		goto done;
	}
	if (!atomic_read(&dev->error)) {
		if (req->actual == 0)
			goto requeue_req;

		pr_debug("rx %p %d\n", req, req->actual);
		xfer = (req->actual < count) ? req->actual : count;
		if (copy_to_user(buf, req->buf, xfer))
			r = -EFAULT;

	} else
		r = -EIO;

done:
	if (atomic_read(&dev->error))
		wake_up(&dev->write_wq);

	mdb_unlock(&dev->read_excl);
	pr_debug("mdb_read returning %d\n", r);
	return r;
}

static ssize_t mdb_write(struct file *fp, const char __user *buf,
				 size_t count, loff_t *pos)
{
	struct mdb_dev *dev = fp->private_data;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	if (!_mdb_dev)
		return -ENODEV;
	pr_debug("mdb_write(%lu)\n", count);

	if (mdb_lock(&dev->write_excl))
		return -EBUSY;

	while (count > 0) {
		if (atomic_read(&dev->error)) {
			pr_debug("mdb_write dev->error\n");
			r = -EIO;
			break;
		}

		req = 0;
		ret = wait_event_interruptible(dev->write_wq,
			((req = mdb_req_get(dev, &dev->tx_idle)) ||
			 atomic_read(&dev->error)));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			if (count > MDB_BULK_BUFFER_SIZE)
				xfer = MDB_BULK_BUFFER_SIZE;
			else
				xfer = count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ep_queue(dev->ep_in, req, GFP_ATOMIC);
			if (ret < 0) {
				pr_debug("mdb_write: xfer error %d\n", ret);
				atomic_set(&dev->error, 1);
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			req = 0;
		}
	}

	if (req)
		mdb_req_put(dev, &dev->tx_idle, req);

	if (atomic_read(&dev->error))
		wake_up(&dev->read_wq);

	mdb_unlock(&dev->write_excl);
	pr_debug("mdb_write returning %d\n", r);
	return r;
}


static int mdb_open(struct inode *ip, struct file *fp)
{
	pr_info("mdb_open\n");
	if (!_mdb_dev)
		return -ENODEV;

	if (mdb_lock(&_mdb_dev->open_excl))
		return -EBUSY;

	fp->private_data = _mdb_dev;

	atomic_set(&_mdb_dev->error, 0);
	if (_mdb_dev->close_notified) {
		_mdb_dev->close_notified = false;
		mdb_ready_callback();
	}

	_mdb_dev->notify_close = true;
	return 0;
}

static int mdb_release(struct inode *ip, struct file *fp)
{
	pr_info("mdb_release\n");

	if (_mdb_dev->notify_close) {
		mdb_closed_callback();
		_mdb_dev->close_notified = true;
	}

	mdb_unlock(&_mdb_dev->open_excl);
	return 0;
}

static const struct file_operations mdb_fops = {
	.owner = THIS_MODULE,
	.read = mdb_read,
	.write = mdb_write,
	.open = mdb_open,
	.release = mdb_release,
};

static struct miscdevice mdb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = mdb_shortname,
	.fops = &mdb_fops,
};




static int
mdb_function_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct mdb_dev	*dev = func_to_mdb(f);
	int			id;
	int			ret;

	dev->cdev = cdev;
	DBG(cdev, "mdb_function_bind dev: %p\n", dev);

	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	mdb_interface_desc.bInterfaceNumber = id;

	ret = mdb_create_bulk_endpoints(dev, &mdb_fullspeed_in_desc,
			&mdb_fullspeed_out_desc);
	if (ret)
		return ret;

	if (gadget_is_dualspeed(c->cdev->gadget)) {
		mdb_highspeed_in_desc.bEndpointAddress =
			mdb_fullspeed_in_desc.bEndpointAddress;
		mdb_highspeed_out_desc.bEndpointAddress =
			mdb_fullspeed_out_desc.bEndpointAddress;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			f->name, dev->ep_in->name, dev->ep_out->name);
	return 0;
}

static void
mdb_function_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct mdb_dev	*dev = func_to_mdb(f);
	struct usb_request *req;

	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);

	wake_up(&dev->read_wq);

	mdb_request_free(dev->rx_req, dev->ep_out);
	while ((req = mdb_req_get(dev, &dev->tx_idle)))
		mdb_request_free(req, dev->ep_in);
}

static int mdb_function_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct mdb_dev	*dev = func_to_mdb(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int ret;

	DBG(cdev, "mdb_function_set_alt intf: %d alt: %d\n", intf, alt);

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_in);
	if (ret) {
		dev->ep_in->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
				dev->ep_in->name, ret);
		return ret;
	}
	ret = usb_ep_enable(dev->ep_in);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
			dev->ep_in->name, ret);
		return ret;
	}

	ret = config_ep_by_speed(cdev->gadget, f, dev->ep_out);
	if (ret) {
		dev->ep_out->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
			dev->ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		return ret;
	}
	ret = usb_ep_enable(dev->ep_out);
	if (ret) {
		ERROR(cdev, "failed to enable ep %s, result %d\n",
				dev->ep_out->name, ret);
		usb_ep_disable(dev->ep_in);
		return ret;
	}

	atomic_set(&dev->online, 1);

	wake_up(&dev->read_wq);
	return 0;
}

static void mdb_function_disable(struct usb_function *f)
{
	struct mdb_dev	*dev = func_to_mdb(f);
	struct usb_composite_dev	*cdev = dev->cdev;

	DBG(cdev, "mdb_function_disable cdev %p\n", cdev);
	dev->notify_close = false;
	atomic_set(&dev->online, 0);
	atomic_set(&dev->error, 1);
	usb_ep_disable(dev->ep_in);
	usb_ep_disable(dev->ep_out);

	wake_up(&dev->read_wq);

	VDBG(cdev, "%s disabled\n", dev->function.name);
}

static int mdb_bind_config(struct usb_configuration *c)
{
	struct mdb_dev *dev = _mdb_dev;

	printk(KERN_INFO "mdb_bind_config\n");

	dev->cdev = c->cdev;
	dev->function.name = "mdb";
	dev->function.fs_descriptors = fs_mdb_descs;
	dev->function.hs_descriptors = hs_mdb_descs;
	dev->function.bind = mdb_function_bind;
	dev->function.unbind = mdb_function_unbind;
	dev->function.set_alt = mdb_function_set_alt;
	dev->function.disable = mdb_function_disable;

	return usb_add_function(c, &dev->function);
}

static int mdb_setup(void)
{
	struct mdb_dev *dev;
	int ret;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);

	init_waitqueue_head(&dev->read_wq);
	init_waitqueue_head(&dev->write_wq);

	atomic_set(&dev->open_excl, 0);
	atomic_set(&dev->read_excl, 0);
	atomic_set(&dev->write_excl, 0);

	dev->close_notified = true;

	INIT_LIST_HEAD(&dev->tx_idle);

	_mdb_dev = dev;

	ret = misc_register(&mdb_device);
	if (ret)
		goto err;

	return 0;

err:
	kfree(dev);
	printk(KERN_ERR "mdb gadget driver failed to initialize\n");
	return ret;
}

static void mdb_cleanup(void)
{
	misc_deregister(&mdb_device);

	kfree(_mdb_dev);
	_mdb_dev = NULL;
}
