/* drivers/usb/function/loopback.c
 *
 * Simple Loopback Function Device
 *
 * Copyright (C) 2007 Google, Inc.
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

#include "usb_function.h"

struct loopback_context
{
	struct usb_endpoint *out;
	struct usb_endpoint *in;
	struct usb_request *req_out;
	struct usb_request *req_in;
};

static struct loopback_context _context;

static void loopback_bind(struct usb_endpoint **ept, void *_ctxt)
{
	struct loopback_context *ctxt = _ctxt;

	ctxt->out = ept[0];
	ctxt->in = ept[1];

	printk(KERN_INFO "loopback_bind() %p, %p\n", ctxt->out, ctxt->in);

	ctxt->req_out = usb_ept_alloc_req(ctxt->out, 4096);
	ctxt->req_in = usb_ept_alloc_req(ctxt->in, 4096);
}

static void loopback_queue_in(struct loopback_context *ctxt, void *data, unsigned len);
static void loopback_queue_out(struct loopback_context *ctxt);

static void loopback_in_complete(struct usb_endpoint *ept, struct usb_request *req)
{
	struct loopback_context *ctxt = req->context;
	printk(KERN_INFO "loopback_out_complete (%d)\n", req->actual);
	loopback_queue_out(ctxt);
}

static void loopback_out_complete(struct usb_endpoint *ept, struct usb_request *req)
{
	struct loopback_context *ctxt = req->context;
	printk(KERN_INFO "loopback_in_complete (%d)\n", req->actual);

	if (req->status == 0) {
		loopback_queue_in(ctxt, req->buf, req->actual);
	} else {
		loopback_queue_out(ctxt);
	}
}

static void loopback_queue_out(struct loopback_context *ctxt)
{
	struct usb_request *req = ctxt->req_out;

	req->complete = loopback_out_complete;
	req->context = ctxt;
	req->length = 4096;

	usb_ept_queue_xfer(ctxt->out, req);
}

static void loopback_queue_in(struct loopback_context *ctxt, void *data, unsigned len)
{
	struct usb_request *req = ctxt->req_in;

	memcpy(req->buf, data, len);
	req->complete = loopback_in_complete;
	req->context = ctxt;
	req->length = len;

	usb_ept_queue_xfer(ctxt->in, req);
}

static void loopback_configure(int configured, void *_ctxt)
{
	struct loopback_context *ctxt = _ctxt;
	printk(KERN_INFO "loopback_configure() %d\n", configured);

	if (configured) {
		loopback_queue_out(ctxt);
	} else {
		/* all pending requests will be canceled */
	}
}

static struct usb_function usb_func_loopback = {
	.bind = loopback_bind,
	.configure = loopback_configure,

	.name = "loopback",
	.context = &_context,

	.ifc_class = 0xff,
	.ifc_subclass = 0xff,
	.ifc_protocol = 0xff,

	.ifc_name = "loopback",

	.ifc_ept_count = 2,
	.ifc_ept_type = { EPT_BULK_OUT, EPT_BULK_IN },
};

static int __init loopback_init(void)
{
	printk(KERN_INFO "loopback_init()\n");
	return usb_function_register(&usb_func_loopback);
}

module_init(loopback_init);
