/* drivers/usb/function/ums.c
 *
 * Function Device for USB Mass Storage
 *
 * Copyright (C) 2007 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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

#include <linux/wait.h>
#include <linux/list.h>

#include <linux/usb/ch9.h>
#include <linux/usb_usual.h>

#include <asm/atomic.h>
#include <asm/uaccess.h>

#include "usb_function.h"

#if 1
#define DBG(x...) do {} while (0)
#else
#define DBG(x...) printk(x)
#endif

#define TXN_MAX 4096

/* UMS setup class requests */
#define USB_BULK_GET_MAX_LUN_REQUEST   0xFE
#define USB_BULK_RESET_REQUEST         0xFF

/* number of rx and tx requests to allocate */
#define RX_REQ_MAX 4
#define TX_REQ_MAX 4

/* FIXME - add ioctl() support for LUN count */
int lun_count = 1;

struct ums_context
{
	int online;
	int error;
	
	atomic_t read_excl;
	atomic_t write_excl;
	atomic_t open_excl;
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
};

static struct ums_context _context;

static inline int _lock(atomic_t *excl)
{
	if(atomic_inc_return(excl) == 1) {
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
static void req_put(struct ums_context *ctxt, struct list_head *head, struct usb_request *req)
{
	unsigned long flags;

	spin_lock_irqsave(&ctxt->lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(&ctxt->lock, flags);
}

/* remove a request from the head of a list */
static struct usb_request *req_get(struct ums_context *ctxt, struct list_head *head)
{
	unsigned long flags;
	struct usb_request *req;
	
	spin_lock_irqsave(&ctxt->lock, flags);
	if(list_empty(head)) {
		req = 0;
	} else {
		req = list_first_entry(head, struct usb_request, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(&ctxt->lock, flags);
	return req;
}

static void ums_complete_in(struct usb_endpoint *ept, struct usb_request *req)
{
	struct ums_context *ctxt = req->context;

	DBG("ums_complete_in length: %d, actual: %d \n", req->length, req->actual);
    
	if(req->status != 0) 
		ctxt->error = 1;

	req_put(ctxt, &ctxt->tx_idle, req);

	wake_up(&ctxt->write_wq);
}

static void ums_complete_out(struct usb_endpoint *ept, struct usb_request *req)
{
	struct ums_context *ctxt = req->context;

	DBG("ums_complete_out length: %d, actual: %d \n", req->length, req->actual);

	if(req->status != 0) {
		ctxt->error = 1;
		req_put(ctxt, &ctxt->rx_idle, req);
	} else {
		req_put(ctxt, &ctxt->rx_done, req);
	}

	wake_up(&ctxt->read_wq);
}

static ssize_t ums_read(struct file *fp, char __user *buf,
                            size_t count, loff_t *pos)
{
	struct ums_context *ctxt = &_context;
	struct usb_request *req;
	int r = count, xfer;
	int ret;

	DBG("ums_read(%d)\n", count);
	
	if(_lock(&ctxt->read_excl))
		return -EBUSY;
	
	/* we will block until we're online */
	while(!(ctxt->online || ctxt->error)) {
		DBG("ums_read: waiting for online state\n");
		ret = wait_event_interruptible(ctxt->read_wq, (ctxt->online || ctxt->error));
		if(ret < 0) {
			_unlock(&ctxt->read_excl);
			return ret;
		}
	}

	if(ctxt->error) {
		r = -EIO;
		goto fail;
	}

		/* if we have idle read requests, get them queued */
	if((req = req_get(ctxt, &ctxt->rx_idle))) {
		req->length = count;
		ret = usb_ept_queue_xfer(ctxt->out, req);
		if(ret < 0) {
			DBG("ums_read: failed to queue req %p (%d)\n", req, ret);
			r = -EIO;
			ctxt->error = 1;
			req_put(ctxt, &ctxt->rx_idle, req);
			goto fail;
		} else {
			DBG("rx %p queue\n", req);
		}
	} else {
		DBG("req_get failed!\n");
		goto fail;
	}

	/* wait for a request to complete */
	req = 0;
	ret = wait_event_interruptible(ctxt->read_wq, 
				       ((req = req_get(ctxt, &ctxt->rx_done)) || ctxt->error));
	
	if(req != 0) {
		ctxt->read_req = req;
		ctxt->read_buf = req->buf;
		DBG("rx %p %d\n", req, req->actual);

		xfer = req->actual;
		if (xfer > count) {
			xfer = count;
		}
		r = xfer;

		if (xfer > 0) {	
			DBG("copy_to_user %d bytes\n", xfer); 
			if(copy_to_user(buf, ctxt->read_buf, xfer)) {
				r = -EFAULT;
			}

		}		
		req_put(ctxt, &ctxt->rx_idle, ctxt->read_req);
		ctxt->read_req = 0;
	} else {
		r = ret;
	}

fail:
	_unlock(&ctxt->read_excl);
	DBG("ums_read returning %d\n", r);
	return r;
} 

static ssize_t ums_write(struct file *fp, const char __user *buf,
                             size_t count, loff_t *pos)
{
	struct ums_context *ctxt = &_context;
	struct usb_request *req = 0;
	int r = count, xfer;
	int ret;

	DBG("ums_write(%d)\n", count);

	if(_lock(&ctxt->write_excl))
		return -EBUSY;

	while(count >= 0) {
		if(ctxt->error) {
			r = -EIO;
			break;
		}

		/* get an idle tx request to use */
		req = 0;
		ret = wait_event_interruptible(ctxt->write_wq, 
					       ((req = req_get(ctxt, &ctxt->tx_idle)) || ctxt->error));
		
		if(ret < 0) {
			r = ret;
			break;
		}

		if(req != 0) {
			xfer = count > TXN_MAX ? TXN_MAX : count;
			if(copy_from_user(req->buf, buf, xfer)){
				r = -EFAULT;
				break;
			}
			
			req->length = xfer;
			ret = usb_ept_queue_xfer(ctxt->in, req);
			if(ret < 0) {
				DBG("ums_write: xfer error %d\n", ret);
				ctxt->error = 1;
				r = -EIO;
				break;
			}

			buf += xfer;
			count -= xfer;

			/* zero this so we don't try to free it on error exit */
			req = 0;
			if (count == 0) {
			    break;
			}
		}
	}


	if(req)
		req_put(ctxt, &ctxt->tx_idle, req);

	_unlock(&ctxt->write_excl);
	DBG("ums_write returning %d\n", r);
	return r;
}

static int ums_open(struct inode *ip, struct file *fp)
{
	struct ums_context *ctxt = &_context;
	
	if(_lock(&ctxt->open_excl))
		return -EBUSY;

	/* clear the error latch */
	ctxt->error = 0;
	
	return 0;
}

static int ums_release(struct inode *ip, struct file *fp)
{
	struct ums_context *ctxt = &_context;

	_unlock(&ctxt->open_excl);
	return 0;
}

static struct file_operations ums_fops = {
	.owner =   THIS_MODULE,
	.read =    ums_read,
	.write =   ums_write,
	.open =    ums_open,
	.release = ums_release,
};
	
static struct miscdevice ums_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "android_ums",
	.fops = &ums_fops,
};

static void ums_bind(struct usb_endpoint **ept, void *_ctxt)
{
	struct ums_context *ctxt = _ctxt;
	struct usb_request *req;
	int n;
	
	ctxt->out = ept[0];
	ctxt->in = ept[1];

	DBG("ums_bind() %p, %p\n", ctxt->out, ctxt->in);
	
	for(n = 0; n < RX_REQ_MAX; n++) {
		req = usb_ept_alloc_req(ctxt->out, 4096);
		if(req == 0) goto fail;
		req->context = ctxt;
		req->complete = ums_complete_out;
		req_put(ctxt, &ctxt->rx_idle, req);
	}

	for(n = 0; n < TX_REQ_MAX; n++) {
		req = usb_ept_alloc_req(ctxt->in, 4096);
		if(req == 0) goto fail;
		req->context = ctxt;
		req->complete = ums_complete_in;
		req_put(ctxt, &ctxt->tx_idle, req);
	}

	printk("ums_bind() allocated %d rx and %d tx requests\n",
	       RX_REQ_MAX, TX_REQ_MAX);
	
	misc_register(&ums_device);
	return;
	
fail:
	printk("ums_bind() could not allocate requests\n");

	/* XXX release any we did allocate */
}

static int ums_setup(struct usb_ctrlrequest* req, void* buf, int len, void *_ctxt)
{
	if ((req->bRequestType & USB_TYPE_MASK) == USB_TYPE_CLASS) {
		if (req->bRequest == USB_BULK_GET_MAX_LUN_REQUEST) {
			if ((req->bRequestType & USB_DIR_IN) != USB_DIR_IN 
					|| req->wValue != 0 || req->wIndex != 0)
			 	return -1;

			((u8*)buf)[0] = lun_count - 1;
			printk("USB_BULK_GET_MAX_LUN_REQUEST returning %d\n", lun_count - 1);
			return 1;
		} else if (req->bRequest == USB_BULK_RESET_REQUEST) {
			if ((req->bRequestType & USB_DIR_OUT) != USB_DIR_IN 
					|| req->wValue != 0 || req->wIndex != 0)
			 	return -1;

			/* FIXME - I'm not sure what to do here */
			printk("USB_BULK_RESET_REQUEST\n");
			return 0;
		}
	}

	return -1;
}

static void ums_configure(int configured, void *_ctxt)
{
	struct ums_context *ctxt = _ctxt;
	struct usb_request *req;
	
	DBG("ums_configure() %d\n", configured);

	if(configured) {
		ctxt->online = 1;

		/* if we have a stale request being read, recycle it */
		ctxt->read_buf = 0;
		if(ctxt->read_req) {
			req_put(ctxt, &ctxt->rx_idle, ctxt->read_req);
			ctxt->read_req = 0;
		}

		/* retire any completed rx requests from previous session */
		while((req = req_get(ctxt, &ctxt->rx_done))) {
			req_put(ctxt, &ctxt->rx_idle, req);
		}
		
	} else {
		ctxt->online = 0;
		ctxt->error = 1;
	}

	/* readers may be blocked waiting for us to go online */
	wake_up(&ctxt->read_wq);
}

static struct usb_function usb_func_ums = {
	.bind = ums_bind,
	.configure = ums_configure,
	.setup = ums_setup,

	.name = "ums",
	.context = &_context,

	.ifc_class = USB_CLASS_MASS_STORAGE,
	.ifc_subclass = US_SC_SCSI,
	.ifc_protocol = US_PR_BULK,

	.ifc_name = "ums",
	
	.ifc_ept_count = 2,
	.ifc_ept_type = { EPT_BULK_OUT, EPT_BULK_IN },
};

static int __init ums_init(void)
{
	struct ums_context *ctxt = &_context;
	DBG("ums_init()\n");

	spin_lock_init(&ctxt->lock);

	init_waitqueue_head(&ctxt->read_wq);
	init_waitqueue_head(&ctxt->write_wq);

	atomic_set(&ctxt->open_excl, 0);
	atomic_set(&ctxt->read_excl, 0);
	atomic_set(&ctxt->write_excl, 0);
	
	INIT_LIST_HEAD(&ctxt->rx_idle);
	INIT_LIST_HEAD(&ctxt->rx_done);
	INIT_LIST_HEAD(&ctxt->tx_idle);
	
	return usb_function_register(&usb_func_ums);
}

module_init(ums_init);
