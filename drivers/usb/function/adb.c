/* drivers/usb/function/adb.c
 *
 * Function Device for the Android ADB Protocol
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>

#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/list.h>

#include <asm/atomic.h>
#include <asm/uaccess.h>

#include "usb_function.h"

#if 1
#define DBG(x...) do {} while (0)
#else
#define DBG(x...) printk(x)
#endif

#define TXN_MAX 4096

/* number of rx and tx requests to allocate */
#define RX_REQ_MAX 4
#define TX_REQ_MAX 4

#define ADB_FUNCTION_NAME "adb"

struct adb_context
{
	int online;
	int error;

	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;
	atomic_t enable_excl;
	spinlock_t lock;

	struct usb_endpoint *out;
	struct usb_endpoint *in;

	struct list_head tx_idle;
	struct list_head rx_idle;
	struct list_head rx_done;

	wait_queue_head_t read_wq;
	wait_queue_head_t write_wq;

	/* the request we're currently reading from */
	struct usb_request *read_req;
	unsigned char *read_buf;
	unsigned read_count;
	unsigned bound;
};

static struct adb_context _context;

static struct usb_interface_descriptor intf_desc = {
	.bLength =		sizeof intf_desc,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	2,
	.bInterfaceClass =	0xff,
	.bInterfaceSubClass =	0x42,
	.bInterfaceProtocol =	0x01,
};

static struct usb_endpoint_descriptor hs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
	.bInterval =		0,
};
static struct usb_endpoint_descriptor fs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(64),
	.bInterval =		0,
};

static struct usb_endpoint_descriptor hs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(512),
	.bInterval =		0,
};

static struct usb_endpoint_descriptor fs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	__constant_cpu_to_le16(64),
	.bInterval =		0,
};

static struct usb_function usb_func_adb;

static inline int _lock(atomic_t *excl)
{
	if (atomic_inc_return(excl) == 1) {
		return 0;
	} else {
		atomic_dec(excl);
		return -1;
	}
}

static inline void _unlock(atomic_t *excl)
{
	atomic_dec(excl);
}

/* add a request to the tail of a list */
void req_put(struct adb_context *ctxt, struct list_head *head, struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&ctxt->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&ctxt->lock, flags);
}

/* remove a request from the head of a list */
struct usb_request *req_get(struct adb_context *ctxt, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;

	spin_lock_irqsave(&ctxt->lock, flags);
	if (list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&ctxt->lock, flags);
	return req;
}

static void adb_complete_in(struct usb_endpoint *ept, struct usb_request *req)
{
	struct adb_context *ctxt = req->context;

	if (req->status != 0)
		ctxt->error = 1;

	req_put(ctxt, &ctxt->tx_idle, req);

	wake_up(&ctxt->write_wq);
}

static void adb_complete_out(struct usb_endpoint *ept, struct usb_request *req)
{
	struct adb_context *ctxt = req->context;

	if (req->status != 0) {
		ctxt->error = 1;
		req_put(ctxt, &ctxt->rx_idle, req);
	} else {
		req_put(ctxt, &ctxt->rx_done, req);
	}

	wake_up(&ctxt->read_wq);
}

static ssize_t adb_read(struct file *fp, char __user *buf,
			size_t count, loff_t *pos)
{
	struct adb_context *ctxt = &_context;
	struct usb_request *req;
	int r = count, xfer;
	int ret;

	DBG("adb_read(%d)\n", count);

	if (_lock(&ctxt->read_excl))
		return -EBUSY;

	/* we will block until we're online */
	while (!(ctxt->online || ctxt->error)) {
		DBG("adb_read: waiting for online state\n");
		ret = wait_event_interruptible(ctxt->read_wq, (ctxt->online || ctxt->error));
		if (ret < 0) {
			_unlock(&ctxt->read_excl);
			return ret;
		}
	}

	while (count > 0) {
		if (ctxt->error) {
			r = -EIO;
			break;
		}

		/* if we have idle read requests, get them queued */
		while ((req = req_get(ctxt, &ctxt->rx_idle))) {
requeue_req:
			req->length = TXN_MAX;
			ret = usb_ept_queue_xfer(ctxt->out, req);
			if (ret < 0) {
				DBG("adb_read: failed to queue req %p (%d)\n", req, ret);
				r = -EIO;
				ctxt->error = 1;
				req_put(ctxt, &ctxt->rx_idle, req);
				goto fail;
			} else {
				DBG("rx %p queue\n", req);
			}
		}

		/* if we have data pending, give it to userspace */
		if (ctxt->read_count > 0) {
			xfer = (ctxt->read_count < count) ? ctxt->read_count : count;

			if (copy_to_user(buf, ctxt->read_buf, xfer)) {
				r = -EFAULT;
				break;
			}
			ctxt->read_buf += xfer;
			ctxt->read_count -= xfer;
			buf += xfer;
			count -= xfer;

			/* if we've emptied the buffer, release the request */
			if (ctxt->read_count == 0) {
				req_put(ctxt, &ctxt->rx_idle, ctxt->read_req);
				ctxt->read_req = 0;
			}
			continue;
		}

		/* wait for a request to complete */
		req = 0;
		ret = wait_event_interruptible(ctxt->read_wq,
					       ((req = req_get(ctxt, &ctxt->rx_done)) || ctxt->error));

		if (req != 0) {
			/* if we got a 0-len one we need to put it back into
			** service.  if we made it the current read req we'd
			** be stuck forever
			*/
			if (req->actual == 0)
				goto requeue_req;

			ctxt->read_req = req;
			ctxt->read_count = req->actual;
			ctxt->read_buf = req->buf;
			DBG("rx %p %d\n", req, req->actual);
		}

		if (ret < 0) {
			r = ret;
			break;
		}
	}

fail:
	_unlock(&ctxt->read_excl);
	return r;
}

static ssize_t adb_write(struct file *fp, const char __user *buf,
			 size_t count, loff_t *pos)
{
	struct adb_context *ctxt = &_context;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	DBG("adb_write(%d)\n", count);

	if (_lock(&ctxt->write_excl))
		return -EBUSY;

	while (count > 0) {
		if (ctxt->error) {
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(ctxt->write_wq,
					       ((req = req_get(ctxt, &ctxt->tx_idle)) || ctxt->error));

		if (ret < 0) {
			r = ret;
			break;
		}

		if (req != 0) {
			xfer = count > TXN_MAX ? TXN_MAX : count;
			if (copy_from_user(req->buf, buf, xfer)) {
				r = -EFAULT;
				break;
			}

			req->length = xfer;
			ret = usb_ept_queue_xfer(ctxt->in, req);
			if (ret < 0) {
				DBG("adb_write: xfer error %d\n", ret);
				ctxt->error = 1;
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
		}
	}


	if (req)
		req_put(ctxt, &ctxt->tx_idle, req);

	_unlock(&ctxt->write_excl);
	return r;
}

static int adb_open(struct inode *ip, struct file *fp)
{
	struct adb_context *ctxt = &_context;

	if (_lock(&ctxt->open_excl))
		return -EBUSY;

	/* clear the error latch */
	ctxt->error = 0;

	return 0;
}

static int adb_release(struct inode *ip, struct file *fp)
{
	struct adb_context *ctxt = &_context;

	_unlock(&ctxt->open_excl);
	return 0;
}

static struct file_operations adb_fops = {
	.owner =   THIS_MODULE,
	.read =    adb_read,
	.write =   adb_write,
	.open =    adb_open,
	.release = adb_release,
};

static struct miscdevice adb_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_adb",
	.fops = &adb_fops,
};

static int adb_enable_open(struct inode *ip, struct file *fp)
{
	struct adb_context *ctxt = &_context;

	if (_lock(&ctxt->enable_excl))
		return -EBUSY;

	printk(KERN_INFO "enabling adb function\n");
	usb_function_enable(ADB_FUNCTION_NAME, 1);
	/* clear the error latch */
	ctxt->error = 0;

	return 0;
}

static int adb_enable_release(struct inode *ip, struct file *fp)
{
	struct adb_context *ctxt = &_context;

	printk(KERN_INFO "disabling adb function\n");
	usb_function_enable(ADB_FUNCTION_NAME, 0);
	_unlock(&ctxt->enable_excl);
	return 0;
}

static struct file_operations adb_enable_fops = {
	.owner =   THIS_MODULE,
	.open =    adb_enable_open,
	.release = adb_enable_release,
};

static struct miscdevice adb_enable_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_adb_enable",
	.fops = &adb_enable_fops,
};

static void adb_unbind(void *_ctxt)
{
	struct adb_context *ctxt = _ctxt;
	struct usb_request *req;

	if (!ctxt->bound)
		return;

	while ((req = req_get(ctxt, &ctxt->rx_idle))) {
		usb_ept_free_req(ctxt->out, req);
	}
	while ((req = req_get(ctxt, &ctxt->tx_idle))) {
		usb_ept_free_req(ctxt->in, req);
	}
	if (ctxt->in) {
		usb_ept_fifo_flush(ctxt->in);
		usb_ept_enable(ctxt->in,  0);
		usb_free_endpoint(ctxt->in);
	}
	if (ctxt->out) {
		usb_ept_fifo_flush(ctxt->out);
		usb_ept_enable(ctxt->out,  0);
		usb_free_endpoint(ctxt->out);
	}

	ctxt->online = 0;
	ctxt->error = 1;

	/* readers may be blocked waiting for us to go online */
	wake_up(&ctxt->read_wq);
	ctxt->bound = 0;
}

static void adb_bind(void *_ctxt)
{
	struct adb_context *ctxt = _ctxt;
	struct usb_request *req;
	int n;

	intf_desc.bInterfaceNumber =
		usb_msm_get_next_ifc_number(&usb_func_adb);

	ctxt->in = usb_alloc_endpoint(USB_DIR_IN);
	if (ctxt->in) {
		hs_bulk_in_desc.bEndpointAddress = USB_DIR_IN | ctxt->in->num;
		fs_bulk_in_desc.bEndpointAddress = USB_DIR_IN | ctxt->in->num;
	}

	ctxt->out = usb_alloc_endpoint(USB_DIR_OUT);
	if (ctxt->out) {
		hs_bulk_out_desc.bEndpointAddress = USB_DIR_OUT|ctxt->out->num;
		fs_bulk_out_desc.bEndpointAddress = USB_DIR_OUT|ctxt->out->num;
	}

	for (n = 0; n < RX_REQ_MAX; n++) {
		req = usb_ept_alloc_req(ctxt->out, 4096);
		if (req == 0) {
			ctxt->bound = 1;
			goto fail;
		}
		req->context = ctxt;
		req->complete = adb_complete_out;
		req_put(ctxt, &ctxt->rx_idle, req);
	}

	for (n = 0; n < TX_REQ_MAX; n++) {
		req = usb_ept_alloc_req(ctxt->in, 4096);
		if (req == 0) {
			ctxt->bound = 1;
			goto fail;
		}
		req->context = ctxt;
		req->complete = adb_complete_in;
		req_put(ctxt, &ctxt->tx_idle, req);
	}
	ctxt->bound = 1;
	return;

fail:
	printk(KERN_ERR "adb_bind() could not allocate requests\n");
	adb_unbind(ctxt);
}

static void adb_configure(int configured, void *_ctxt)
{
	struct adb_context *ctxt = _ctxt;
	struct usb_request *req;

	if (configured) {
		ctxt->online = 1;

		if (usb_msm_get_speed() == USB_SPEED_HIGH) {
			usb_configure_endpoint(ctxt->in, &hs_bulk_in_desc);
			usb_configure_endpoint(ctxt->out, &hs_bulk_out_desc);
		} else {
			usb_configure_endpoint(ctxt->in, &fs_bulk_in_desc);
			usb_configure_endpoint(ctxt->out, &fs_bulk_out_desc);
		}
		usb_ept_enable(ctxt->in,  1);
		usb_ept_enable(ctxt->out, 1);

		/* if we have a stale request being read, recycle it */
		ctxt->read_buf = 0;
		ctxt->read_count = 0;
		if (ctxt->read_req) {
			req_put(ctxt, &ctxt->rx_idle, ctxt->read_req);
			ctxt->read_req = 0;
		}

		/* retire any completed rx requests from previous session */
		while ((req = req_get(ctxt, &ctxt->rx_done)))
			req_put(ctxt, &ctxt->rx_idle, req);

	} else {
		ctxt->online = 0;
		ctxt->error = 1;
	}

	/* readers may be blocked waiting for us to go online */
	wake_up(&ctxt->read_wq);
}

static struct usb_function usb_func_adb = {
	.bind = adb_bind,
	.unbind = adb_unbind,
	.configure = adb_configure,

	.name = ADB_FUNCTION_NAME,
	.context = &_context,

};

struct usb_descriptor_header *adb_hs_descriptors[4];
struct usb_descriptor_header *adb_fs_descriptors[4];
static int __init adb_init(void)
{
	int ret = 0;
	struct adb_context *ctxt = &_context;
	DBG("adb_init()\n");

	init_waitqueue_head(&ctxt->read_wq);
	init_waitqueue_head(&ctxt->write_wq);

	atomic_set(&ctxt->open_excl, 0);
	atomic_set(&ctxt->read_excl, 0);
	atomic_set(&ctxt->write_excl, 0);
	atomic_set(&ctxt->enable_excl, 0);

	spin_lock_init(&ctxt->lock);

	INIT_LIST_HEAD(&ctxt->rx_idle);
	INIT_LIST_HEAD(&ctxt->rx_done);
	INIT_LIST_HEAD(&ctxt->tx_idle);

	adb_hs_descriptors[0] = (struct usb_descriptor_header *)&intf_desc;
	adb_hs_descriptors[1] =
		(struct usb_descriptor_header *)&hs_bulk_in_desc;
	adb_hs_descriptors[2] =
		(struct usb_descriptor_header *)&hs_bulk_out_desc;
	adb_hs_descriptors[3] = NULL;

	adb_fs_descriptors[0] = (struct usb_descriptor_header *)&intf_desc;
	adb_fs_descriptors[1] =
		(struct usb_descriptor_header *)&fs_bulk_in_desc;
	adb_fs_descriptors[2] =
		(struct usb_descriptor_header *)&fs_bulk_out_desc;
	adb_fs_descriptors[3] = NULL;

	usb_func_adb.hs_descriptors = adb_hs_descriptors;
	usb_func_adb.fs_descriptors = adb_fs_descriptors;

	ret = misc_register(&adb_device);
	if (ret) {
		printk(KERN_ERR "adb Can't register misc device  %d \n",
						MISC_DYNAMIC_MINOR);
		return ret;
	}
	ret = misc_register(&adb_enable_device);
	if (ret) {
		printk(KERN_ERR "adb Can't register misc enable device  %d \n",
						MISC_DYNAMIC_MINOR);
		misc_deregister(&adb_device);
		return ret;
	}

	ret = usb_function_register(&usb_func_adb);
	if (ret) {
		misc_deregister(&adb_device);
		misc_deregister(&adb_enable_device);
	}
	return ret;
}

module_init(adb_init);

static void __exit adb_exit(void)
{
	misc_deregister(&adb_device);
	misc_deregister(&adb_enable_device);

	usb_function_unregister(&usb_func_adb);
}
module_exit(adb_exit);
