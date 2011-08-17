/* driver/usb/function/null.c
 *
 * Null Function Device - A Data Sink
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

struct null_context
{
	struct usb_endpoint *out;
	struct usb_request *req0;
	struct usb_request *req1;
};

static struct null_context _context;

static void null_bind(struct usb_endpoint **ept, void *_ctxt)
{
	struct null_context *ctxt = _ctxt;
	ctxt->out = ept[0];
	printk(KERN_INFO "null_bind() %p\n", ctxt->out);

	ctxt->req0 = usb_ept_alloc_req(ctxt->out, 4096);
	ctxt->req1 = usb_ept_alloc_req(ctxt->out, 4096);
}

static void null_unbind(void *_ctxt)
{
	struct null_context *ctxt = _ctxt;
	printk(KERN_INFO "null_unbind()\n");
	if (ctxt->req0) {
		usb_ept_free_req(ctxt->out, ctxt->req0);
		ctxt->req0 = 0;
	}
	if (ctxt->req1) {
		usb_ept_free_req(ctxt->out, ctxt->req1);
		ctxt->req1 = 0;
	}
	ctxt->out = 0;
}


static void null_queue_out(struct null_context *ctxt, struct usb_request *req);

static void null_out_complete(struct usb_endpoint *ept, struct usb_request *req)
{
	struct null_context *ctxt = req->context;
	unsigned char *data = req->buf;

	if (req->status != -ENODEV)
		null_queue_out(ctxt, req);
}

static void null_queue_out(struct null_context *ctxt, struct usb_request *req)
{
	req->complete = null_out_complete;
	req->context = ctxt;
	req->length = 4096;

	usb_ept_queue_xfer(ctxt->out, req);
}

static void null_configure(int configured, void *_ctxt)
{
	struct null_context *ctxt = _ctxt;
	printk(KERN_INFO "null_configure() %d\n", configured);

	if (configured) {
		null_queue_out(ctxt, ctxt->req0);
		null_queue_out(ctxt, ctxt->req1);
	} else {
		/* all pending requests will be canceled */
	}
}

static struct usb_function usb_func_null = {
	.bind = null_bind,
	.unbind = null_unbind,
	.configure = null_configure,

	.name = "null",
	.context = &_context,

	.ifc_class = 0xff,
	.ifc_subclass = 0xfe,
	.ifc_protocol = 0x01,

	.ifc_name = "null",

	.ifc_ept_count = 1,
	.ifc_ept_type = { EPT_BULK_OUT },
};

static int __init null_init(void)
{
	printk(KERN_INFO "null_init()\n");
	usb_function_register(&usb_func_null);
	return 0;
}

module_init(null_init);
