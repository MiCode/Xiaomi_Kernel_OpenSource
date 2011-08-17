/* drivers/usb/function/diag.c
 *
 * Diag Function Device - Route DIAG frames between SMD and USB
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/err.h>

#include <mach/msm_smd.h>
#include <mach/usbdiag.h>

#include "usb_function.h"

#define WRITE_COMPLETE 0
#define READ_COMPLETE  0
static struct usb_interface_descriptor intf_desc = {
	.bLength            =	sizeof intf_desc,
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bNumEndpoints      =	2,
	.bInterfaceClass    =	0xFF,
	.bInterfaceSubClass =	0xFF,
	.bInterfaceProtocol =	0xFF,
};

static struct usb_endpoint_descriptor hs_bulk_in_desc = {
	.bLength 			=	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType 	=	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes 		=	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize 	=	__constant_cpu_to_le16(512),
	.bInterval 			=	0,
};
static struct usb_endpoint_descriptor fs_bulk_in_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor hs_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(512),
	.bInterval        =	0,
};

static struct usb_endpoint_descriptor fs_bulk_out_desc = {
	.bLength          =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType  =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes     =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize   = __constant_cpu_to_le16(64),
	.bInterval        =	0,
};

/* list of requests */
struct diag_req_entry {
	struct list_head re_entry;
	struct usb_request *usb_req;
	void *diag_request;
};
struct diag_context {
	struct usb_endpoint *epout;
	struct usb_endpoint *epin;
	spinlock_t dev_lock;
	/* linked list of read requets*/
	struct list_head dev_read_req_list;
	/* linked list of write requets*/
	struct list_head dev_write_req_list;
	struct diag_operations *operations;
	struct workqueue_struct *diag_wq;
	struct work_struct usb_config_work;
	unsigned configured;
	unsigned bound;
	int diag_opened;
};

static struct usb_function usb_func_diag;
static struct diag_context _context;
static void diag_write_complete(struct usb_endpoint *,
		struct usb_request *);
static struct diag_req_entry *diag_alloc_req_entry(struct usb_endpoint *,
		unsigned len, gfp_t);
static void diag_free_req_entry(struct usb_endpoint *, struct diag_req_entry *);
static void diag_read_complete(struct usb_endpoint *, struct usb_request *);


static void diag_unbind(void *context)
{

	struct diag_context *ctxt = context;

	if (!ctxt)
		return;
	if (!ctxt->bound)
		return;
	if (ctxt->epin) {
		usb_ept_fifo_flush(ctxt->epin);
		usb_ept_enable(ctxt->epin, 0);
		usb_free_endpoint(ctxt->epin);
		}
	if (ctxt->epout) {
		usb_ept_fifo_flush(ctxt->epout);
		usb_ept_enable(ctxt->epout, 0);
		usb_free_endpoint(ctxt->epout);
		}
	ctxt->bound = 0;
}
static void diag_bind(void *context)
{
	struct diag_context *ctxt = context;

	if (!ctxt)
		return;

	intf_desc.bInterfaceNumber =
		usb_msm_get_next_ifc_number(&usb_func_diag);

	ctxt->epin = usb_alloc_endpoint(USB_DIR_IN);
	if (ctxt->epin) {
		hs_bulk_in_desc.bEndpointAddress =
			USB_DIR_IN | ctxt->epin->num;
		fs_bulk_in_desc.bEndpointAddress =
			USB_DIR_IN | ctxt->epin->num;
	}

	ctxt->epout = usb_alloc_endpoint(USB_DIR_OUT);
	if (ctxt->epout) {
		hs_bulk_out_desc.bEndpointAddress =
			USB_DIR_OUT | ctxt->epout->num;
		fs_bulk_out_desc.bEndpointAddress =
			USB_DIR_OUT | ctxt->epout->num;
	}

	ctxt->bound = 1;
}
static void diag_configure(int configured, void *_ctxt)

{
	struct diag_context *ctxt = _ctxt;

	if (!ctxt)
		return;
	if (configured) {
		if (usb_msm_get_speed() == USB_SPEED_HIGH) {
			usb_configure_endpoint(ctxt->epin, &hs_bulk_in_desc);
			usb_configure_endpoint(ctxt->epout, &hs_bulk_out_desc);
		} else {
			usb_configure_endpoint(ctxt->epin, &fs_bulk_in_desc);
			usb_configure_endpoint(ctxt->epout, &fs_bulk_out_desc);
		}
		usb_ept_enable(ctxt->epin,  1);
		usb_ept_enable(ctxt->epout, 1);
		ctxt->configured = 1;
		queue_work(_context.diag_wq, &(_context.usb_config_work));
	} else {
		/* all pending requests will be canceled */
		ctxt->configured = 0;
		if (ctxt->epin) {
			usb_ept_fifo_flush(ctxt->epin);
			usb_ept_enable(ctxt->epin, 0);
		}
		if (ctxt->epout) {
			usb_ept_fifo_flush(ctxt->epout);
			usb_ept_enable(ctxt->epout, 0);
		}
		if ((ctxt->operations) &&
			(ctxt->operations->diag_disconnect))
				ctxt->operations->diag_disconnect();
	}

}
static struct usb_function usb_func_diag = {
	.bind = diag_bind,
	.configure = diag_configure,
	.unbind = diag_unbind,


	.name = "diag",
	.context = &_context,
};
int diag_usb_register(struct diag_operations *func)
{
	struct diag_context *ctxt = &_context;

	if (func == NULL) {
		printk(KERN_ERR "diag_usb_register:registering"
				"diag char operations NULL\n");
		return -1;
	}
	ctxt->operations = func;
	if (ctxt->configured == 1)
		if ((ctxt->operations) &&
			(ctxt->operations->diag_connect))
				ctxt->operations->diag_connect();
	return 0;
}
EXPORT_SYMBOL(diag_usb_register);

int diag_usb_unregister(void)
{
	struct diag_context *ctxt = &_context;

	ctxt->operations = NULL;
	return 0;
}
EXPORT_SYMBOL(diag_usb_unregister);

int diag_open(int num_req)
{
	struct diag_context *ctxt = &_context;
	struct diag_req_entry *write_entry;
	struct diag_req_entry *read_entry;
	int i = 0;

	for (i = 0; i < num_req; i++) {
		write_entry = diag_alloc_req_entry(ctxt->epin, 0, GFP_KERNEL);
		if (write_entry) {
			write_entry->usb_req->complete = diag_write_complete;
			write_entry->usb_req->device = (void *)ctxt;
			list_add(&write_entry->re_entry,
					&ctxt->dev_write_req_list);
		} else
			goto write_error;
	}

	for (i = 0; i < num_req ; i++) {
		read_entry = diag_alloc_req_entry(ctxt->epout, 0 , GFP_KERNEL);
		if (read_entry) {
			read_entry->usb_req->complete = diag_read_complete;
			read_entry->usb_req->device = (void *)ctxt;
			list_add(&read_entry->re_entry ,
					&ctxt->dev_read_req_list);
		} else
			goto read_error;
		}
	ctxt->diag_opened = 1;
	return 0;
read_error:
	printk(KERN_ERR "%s:read requests allocation failure\n", __func__);
	while (!list_empty(&ctxt->dev_read_req_list)) {
		read_entry = list_entry(ctxt->dev_read_req_list.next,
				struct diag_req_entry, re_entry);
		list_del(&read_entry->re_entry);
		diag_free_req_entry(ctxt->epout, read_entry);
	}
write_error:
	printk(KERN_ERR "%s: write requests allocation failure\n", __func__);
	while (!list_empty(&ctxt->dev_write_req_list)) {
		write_entry = list_entry(ctxt->dev_write_req_list.next,
				struct diag_req_entry, re_entry);
		list_del(&write_entry->re_entry);
		diag_free_req_entry(ctxt->epin, write_entry);
	}
	ctxt->diag_opened = 0;
	return -ENOMEM;
}
EXPORT_SYMBOL(diag_open);

void diag_close(void)
{
	struct diag_context *ctxt = &_context;
	struct diag_req_entry *req_entry;
	/* free write requests */

	while (!list_empty(&ctxt->dev_write_req_list)) {
		req_entry = list_entry(ctxt->dev_write_req_list.next,
				struct diag_req_entry, re_entry);
		list_del(&req_entry->re_entry);
		diag_free_req_entry(ctxt->epin, req_entry);
	}

	/* free read requests */
	while (!list_empty(&ctxt->dev_read_req_list)) {
		req_entry = list_entry(ctxt->dev_read_req_list.next,
				struct diag_req_entry, re_entry);
		list_del(&req_entry->re_entry);
		diag_free_req_entry(ctxt->epout, req_entry);
	}
	return;
}
EXPORT_SYMBOL(diag_close);

static void diag_free_req_entry(struct usb_endpoint *ep,
		struct diag_req_entry *req)
{
	if (ep != NULL && req != NULL) {
		if (req->usb_req != NULL)
			usb_ept_free_req(ep, req->usb_req);
		kfree(req);
	}
}


static struct diag_req_entry *diag_alloc_req_entry(struct usb_endpoint *ep,
		unsigned len, gfp_t kmalloc_flags)
{
	struct diag_req_entry *req;

	req = kmalloc(sizeof(struct diag_req_entry), kmalloc_flags);
	if (req == NULL)
		return ERR_PTR(-ENOMEM);


	req->usb_req = usb_ept_alloc_req(ep , 0);
	if (req->usb_req == NULL) {
		kfree(req);
		return ERR_PTR(-ENOMEM);
	}

	req->usb_req->context = req;
	return req;
}

int diag_read(struct diag_request *d_req)
{
	unsigned long flags;
	struct usb_request *req = NULL;
	struct diag_req_entry *req_entry = NULL;
	struct diag_context *ctxt = &_context;


	if (ctxt->diag_opened != 1)
		return -EIO;
	spin_lock_irqsave(&ctxt->dev_lock , flags);
	if (!list_empty(&ctxt->dev_read_req_list)) {
		req_entry = list_entry(ctxt->dev_read_req_list.next ,
				struct diag_req_entry , re_entry);
		req_entry->diag_request = d_req;
		req = req_entry->usb_req;
		list_del(&req_entry->re_entry);
	}
	spin_unlock_irqrestore(&ctxt->dev_lock , flags);
	if (req) {
		req->buf = d_req->buf;
		req->length = d_req->length;
		req->device = ctxt;
		if (usb_ept_queue_xfer(ctxt->epout, req)) {
			/* If error add the link to the linked list again. */
			spin_lock_irqsave(&ctxt->dev_lock , flags);
			list_add_tail(&req_entry->re_entry ,
					&ctxt->dev_read_req_list);
			spin_unlock_irqrestore(&ctxt->dev_lock , flags);
			printk(KERN_ERR "diag_read:can't queue the request\n");
			return -EIO;
		}
	} else {
		printk(KERN_ERR
				"diag_read:no requests avialable\n");
		return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL(diag_read);

int diag_write(struct diag_request *d_req)
{
	unsigned long flags;
	struct usb_request *req = NULL;
	struct diag_req_entry *req_entry = NULL;
	struct diag_context *ctxt = &_context;

	if (ctxt->diag_opened != 1)
		return -EIO;
	spin_lock_irqsave(&ctxt->dev_lock , flags);
	if (!list_empty(&ctxt->dev_write_req_list)) {
		req_entry = list_entry(ctxt->dev_write_req_list.next ,
				struct diag_req_entry , re_entry);
		req_entry->diag_request = d_req;
		req = req_entry->usb_req;
		list_del(&req_entry->re_entry);
	}
	spin_unlock_irqrestore(&ctxt->dev_lock, flags);
	if (req) {
		req->buf = d_req->buf;
		req->length = d_req->length;
		req->device = ctxt;
		if (usb_ept_queue_xfer(ctxt->epin, req)) {
			/* If error add the link to linked list again*/
			spin_lock_irqsave(&ctxt->dev_lock, flags);
			list_add_tail(&req_entry->re_entry ,
					&ctxt->dev_write_req_list);
			spin_unlock_irqrestore(&ctxt->dev_lock, flags);
			printk(KERN_ERR "diag_write: cannot queue"
					" read request\n");
			return -EIO;
		}
	} else {
		printk(KERN_ERR	"diag_write: no requests available\n");
		return -EIO;
	}
	return 0;
}
EXPORT_SYMBOL(diag_write);

static void diag_write_complete(struct usb_endpoint *ep ,
		struct usb_request *req)
{
	struct diag_context *ctxt = (struct diag_context *)req->device;
	struct diag_req_entry *diag_req = req->context;
	struct diag_request *d_req = (struct diag_request *)
						diag_req->diag_request;
	unsigned long flags;

	if (ctxt == NULL) {
		printk(KERN_ERR "diag_write_complete : requesting"
				"NULL device pointer\n");
		return;
	}
	if (req->status == WRITE_COMPLETE) {
		if ((req->length >= ep->max_pkt) &&
				((req->length % ep->max_pkt) == 0)) {
			req->length = 0;
			req->device = ctxt;
			d_req->actual = req->actual;
			d_req->status = req->status;
			/* Queue zero length packet */
			usb_ept_queue_xfer(ctxt->epin, req);
			return;
		}
			/* normal completion*/
		spin_lock_irqsave(&ctxt->dev_lock, flags);
		list_add_tail(&diag_req->re_entry ,
				&ctxt->dev_write_req_list);
		if (req->length != 0) {
			d_req->actual = req->actual;
			d_req->status = req->status;
		}
		spin_unlock_irqrestore(&ctxt->dev_lock , flags);
		if ((ctxt->operations) &&
			(ctxt->operations->diag_char_write_complete))
				ctxt->operations->diag_char_write_complete(
					d_req);
	} else {
		spin_lock_irqsave(&ctxt->dev_lock, flags);
		list_add_tail(&diag_req->re_entry ,
			&ctxt->dev_write_req_list);
		d_req->actual = req->actual;
		d_req->status = req->status;
		spin_unlock_irqrestore(&ctxt->dev_lock , flags);
		if ((ctxt->operations) &&
			(ctxt->operations->diag_char_write_complete))
				ctxt->operations->diag_char_write_complete(
					d_req);
	}
}
static void diag_read_complete(struct usb_endpoint *ep ,
		struct usb_request *req)
{
	 struct diag_context *ctxt = (struct diag_context *)req->device;
	 struct diag_req_entry *diag_req = req->context;
	 struct diag_request *d_req = (struct diag_request *)
							diag_req->diag_request;
	 unsigned long flags;

	if (ctxt == NULL) {
		printk(KERN_ERR "diag_read_complete : requesting"
				"NULL device pointer\n");
		return;
	}
	if (req->status == READ_COMPLETE) {
			/* normal completion*/
		spin_lock_irqsave(&ctxt->dev_lock, flags);
		list_add_tail(&diag_req->re_entry ,
				&ctxt->dev_read_req_list);
		d_req->actual = req->actual;
		d_req->status = req->status;
		spin_unlock_irqrestore(&ctxt->dev_lock, flags);
		if ((ctxt->operations) &&
			(ctxt->operations->diag_char_read_complete))
				ctxt->operations->diag_char_read_complete(
					d_req);
	} else {
		spin_lock_irqsave(&ctxt->dev_lock, flags);
		list_add_tail(&diag_req->re_entry ,
				&ctxt->dev_read_req_list);
		d_req->actual = req->actual;
		d_req->status = req->status;
		spin_unlock_irqrestore(&ctxt->dev_lock, flags);
		if ((ctxt->operations) &&
			(ctxt->operations->diag_char_read_complete))
				ctxt->operations->diag_char_read_complete(
					d_req);
	}
}
void usb_config_work_func(struct work_struct *work)
{
	struct diag_context *ctxt = &_context;
	if ((ctxt->operations) &&
		(ctxt->operations->diag_connect))
			ctxt->operations->diag_connect();
}

struct usb_descriptor_header *diag_hs_descriptors[4];
struct usb_descriptor_header *diag_fs_descriptors[4];

static int __init diag_init(void)
{
	int r;
	struct diag_context *ctxt = &_context;

	diag_hs_descriptors[0] = (struct usb_descriptor_header *)&intf_desc;
	diag_hs_descriptors[1] =
		(struct usb_descriptor_header *)&hs_bulk_in_desc;
	diag_hs_descriptors[2] =
		(struct usb_descriptor_header *)&hs_bulk_out_desc;
	diag_hs_descriptors[3] = NULL;

	diag_fs_descriptors[0] = (struct usb_descriptor_header *)&intf_desc;
	diag_fs_descriptors[1] =
		(struct usb_descriptor_header *)&fs_bulk_in_desc;
	diag_fs_descriptors[2] =
		(struct usb_descriptor_header *)&fs_bulk_out_desc;
	diag_fs_descriptors[3] = NULL;
	INIT_LIST_HEAD(&ctxt->dev_read_req_list);
	INIT_LIST_HEAD(&ctxt->dev_write_req_list);
	ctxt->diag_wq  = create_singlethread_workqueue("diag");
	if (ctxt->diag_wq == NULL)
		return -1;
	INIT_WORK(&_context.usb_config_work , usb_config_work_func);

	usb_func_diag.hs_descriptors = diag_hs_descriptors;
	usb_func_diag.fs_descriptors = diag_fs_descriptors;
	spin_lock_init(&_context.dev_lock);
	r = usb_function_register(&usb_func_diag);
	if (r < 0)
		destroy_workqueue(ctxt->diag_wq);
	return r;
}

module_init(diag_init);
static void __exit diag_exit(void)
{
	struct diag_context *ctxt = &_context;
	if (!ctxt)
		return;
	if (!ctxt)
		BUG_ON(1);

	usb_function_unregister(&usb_func_diag);
	destroy_workqueue(ctxt->diag_wq);
}
module_exit(diag_exit);

MODULE_LICENSE("GPL v2");
