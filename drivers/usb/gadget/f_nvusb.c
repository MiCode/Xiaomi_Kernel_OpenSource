/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*#define VERBOSE_DEBUG*/

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/module.h>

#include <linux/usb.h>

#include "g_nvusb.h"
#include "gadget_chips.h"

#include "../class/nv-usb.h"

struct f_nvusb {
	struct usb_function	function;

	struct usb_ep		*in_ep;
	struct usb_ep		*out_ep;

	struct usb_request	*req;
	struct completion	comp_req;
	struct task_struct	*task;
};

static inline struct f_nvusb *func_to_nvusb(struct usb_function *f)
{
	return (struct f_nvusb *)container_of(f, struct f_nvusb, function);
}

int nvusb_pattern = 'a';
module_param(nvusb_pattern, int, S_IRUGO | S_IWUSR);

unsigned nvusb_buflen = 32768;
module_param(nvusb_buflen, uint, S_IRUGO | S_IWUSR);

/*-------------------------------------------------------------------------*/

static struct usb_interface_descriptor nvusb_intf = {
	.bLength =		sizeof nvusb_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
};

/* full speed support: */

static struct usb_endpoint_descriptor nvusbfs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor nvusbfs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *nvusbfs_descs[] = {
	(struct usb_descriptor_header *) &nvusb_intf,
	(struct usb_descriptor_header *) &nvusbfs_out_desc,
	(struct usb_descriptor_header *) &nvusbfs_in_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor nvusbhs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor nvusbhs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *nvusbhs_descs[] = {
	(struct usb_descriptor_header *) &nvusb_intf,
	(struct usb_descriptor_header *) &nvusbhs_in_desc,
	(struct usb_descriptor_header *) &nvusbhs_out_desc,
	NULL,
};

/* super speed support: */

static struct usb_endpoint_descriptor nvusbss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

struct usb_ss_ep_comp_descriptor nvusbss_in_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_endpoint_descriptor nvusbss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

struct usb_ss_ep_comp_descriptor nvusbss_out_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_descriptor_header *nvusbss_descs[] = {
	(struct usb_descriptor_header *) &nvusb_intf,
	(struct usb_descriptor_header *) &nvusbss_in_desc,
	(struct usb_descriptor_header *) &nvusbss_in_comp_desc,
	(struct usb_descriptor_header *) &nvusbss_out_desc,
	(struct usb_descriptor_header *) &nvusbss_out_comp_desc,
	NULL,
};

/* function-specific strings: */

static struct usb_string strings_nvusb[] = {
	[0].s = "nvusb data",
	{  }			/* end of list */
};

static struct usb_gadget_strings stringtab_nvusb = {
	.language	= 0x0409,	/* en-us */
	.strings	= strings_nvusb,
};

static struct usb_gadget_strings *nvusb_strings[] = {
	&stringtab_nvusb,
	NULL,
};

/*-------------------------------------------------------------------------*/

unsigned long
nvusb_calc_time(struct timeval g_sttime, struct timeval g_entime)
{
	unsigned int num_sec = g_entime.tv_sec - g_sttime.tv_sec;

	if (num_sec)
		return (g_entime.tv_usec + (1000000 - g_sttime.tv_usec)
			+ ((num_sec - 1) * 1000000));
	else
		return g_entime.tv_usec - g_sttime.tv_usec;
}

void free_nvusbep_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static int
nvusb_bind_func(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_nvusb	*nvusb = func_to_nvusb(f);
	int	id;

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
		return id;
	nvusb_intf.bInterfaceNumber = id;

	/* allocate endpoints */
	nvusb->in_ep = usb_ep_autoconfig(cdev->gadget, &nvusbfs_in_desc);
	if (!nvusb->in_ep) {
autoconf_fail:
		ERROR(cdev, "%s: can't autoconfigure on %s\n",
			f->name, cdev->gadget->name);
		return -ENODEV;
	}
	nvusb->in_ep->driver_data = cdev;	/* claim */

	nvusb->out_ep = usb_ep_autoconfig(cdev->gadget, &nvusbfs_out_desc);
	if (!nvusb->out_ep)
		goto autoconf_fail;
	nvusb->out_ep->driver_data = cdev;	/* claim */

	/* support high speed hardware */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		nvusbhs_in_desc.bEndpointAddress =
				nvusbfs_in_desc.bEndpointAddress;
		nvusbhs_out_desc.bEndpointAddress =
				nvusbfs_out_desc.bEndpointAddress;
		f->hs_descriptors = nvusbhs_descs;
	}

	/* support super speed hardware */
	if (gadget_is_superspeed(c->cdev->gadget)) {
		nvusbss_in_desc.bEndpointAddress =
				nvusbfs_in_desc.bEndpointAddress;
		nvusbss_out_desc.bEndpointAddress =
				nvusbfs_out_desc.bEndpointAddress;
		f->ss_descriptors = nvusbss_descs;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
	    (gadget_is_superspeed(c->cdev->gadget) ? "super" :
	     (gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full")),
			f->name, nvusb->in_ep->name, nvusb->out_ep->name);
	return 0;
}

static void
nvusb_unbind_func(struct usb_configuration *c, struct usb_function *f)
{
	kfree(func_to_nvusb(f));
}

static void CBS_complete(struct usb_ep *ep, struct usb_request *req)
{
	int			status = req->status;

	switch (status) {
	case 0:
		kfree(req->buf);
		usb_ep_free_request(ep, req);
		break;
	}
}

static int
send_CBS(struct f_nvusb *nvusb,
struct nvusb_cb_wrap *bcb, unsigned int data_transfer_time, int res)
{
	struct usb_ep		*ep;
	struct usb_request	*req;
	int					status;
	struct nvusb_cs_wrap	 *bcs = NULL;

	ep =  nvusb->in_ep;
	req = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (!req)
		return -ENOMEM;

	req->length = US_BULK_CS_WRAP_LEN;
	req->buf = kmalloc(req->length, GFP_ATOMIC);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		req = NULL;
		return -ENOMEM;
	}

	bcs = req->buf;
	bcs->Signature = cpu_to_le32(US_BULK_CS_SIGN);
	bcs->Tag = bcb->Tag;
	bcs->Residue = data_transfer_time;
	bcs->Status = res;

	req->complete = CBS_complete;

	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (status) {
		struct usb_composite_dev	*cdev;

		cdev = nvusb->function.config->cdev;
		ERROR(cdev, "start %s %s --> %d\n",
				 "IN",
				ep->name, status);
		free_nvusbep_req(ep, req);
	}

	return status;
}

static void nvusb_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_nvusb	*nvusb = ep->driver_data;
	struct usb_composite_dev *cdev = nvusb->function.config->cdev;
	int			status = req->status;

	DBG(cdev, "nvusb_complete status = %d\n", status);

	switch (status) {
	case 0:
		nvusb->req = req;
		complete(&nvusb->comp_req);
		break;

	/* this endpoint is normally active while we're configured */
	case -ECONNABORTED:		/* hardware forced ep reset */
	case -ECONNRESET:		/* request dequeued */
	case -ESHUTDOWN:		/* disconnect from host */
		VDBG(cdev, "%s gone (%d), %d/%d\n", ep->name, status,
				req->actual, req->length);
		free_nvusbep_req(ep, req);
		return;

	case -EOVERFLOW:		/* buffer overrun on read means that
					 * we didn't provide a big enough
					 * buffer.
					 */
	default:
		DBG(cdev, "%s nvusb_complete --> %d, %d/%d\n", ep->name,
				status, req->actual, req->length);
	case -EREMOTEIO:		/* short read */
		break;
	}

}


static int nvusb_kthread(void *data)
{
	struct f_nvusb *nvusb = (struct f_nvusb *) data;

	struct usb_composite_dev *cdev = nvusb->function.config->cdev;
	int status = 0;
	int data_length;
	struct nvusb_cb_wrap *bcb = NULL;
	struct timeval g_sttime;
	struct timeval g_entime;
	int data_direction = 0;
	struct usb_request *req = NULL;
	struct usb_request *cbw_req = NULL;
	struct usb_ep *ep = NULL;
	int i;
	int data_err = US_BULK_STAT_OK;
	unsigned char *buf = NULL;


	INFO(cdev, "nvusb_kthread started\n");

	daemonize("nvusb_kthread");

	init_completion(&nvusb->comp_req);

	/* Request delivery of SIGKILL */

	allow_signal(SIGKILL);

	for (;;) {
		wait_for_completion_interruptible(&nvusb->comp_req);

		/* Die I receive SIGKILL */
		if (signal_pending(current))
			break;

		data_err = US_BULK_STAT_OK;

		if (nvusb->req == NULL)
			goto out2;

		req = nvusb->req;
		cbw_req = req;

		INFO(cdev, "CBW state\n");

		bcb = kmalloc(US_BULK_CB_WRAP_LEN, GFP_KERNEL);
		if (!bcb)
			goto out1;

		memcpy(bcb, req->buf, US_BULK_CB_WRAP_LEN);
		INFO(cdev,
		"Signature=%x data len=%d flag=%d tag=%d sub_cmd_length=%d\n",
		bcb->Signature, bcb->DataTransferLength, bcb->Flags, bcb->Tag,
		bcb->Length);

		data_length = bcb->DataTransferLength;
		data_direction = bcb->Flags;

		if (data_length <= 0) {
			g_sttime.tv_sec = g_entime.tv_sec = 0;
			g_sttime.tv_usec = g_entime.tv_usec = 0;
			goto cbs_phase;
		}

		if (data_direction)
			ep = nvusb->in_ep;
		else
			ep = nvusb->out_ep;

		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			goto out;

		req->length = nvusb_buflen;
		if (data_length < nvusb_buflen)
			req->length = data_length;
		req->buf = kmalloc(req->length, GFP_ATOMIC);
		if (!req->buf) {
			usb_ep_free_request(ep, req);
			req = NULL;
			goto out;
		}
		req->complete = nvusb_complete;

		if (data_direction)
			memset(req->buf, nvusb_pattern, req->length);

		do_gettimeofday(&g_sttime);

		status = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (status) {
			ERROR(cdev, "kill %s:  resubmit %d bytes --> %d\n",
					ep->name, req->length, status);
			usb_ep_set_halt(ep);
			goto out;
		}

		INFO(cdev, "data state\n");
		while (data_length > 0) {
			wait_for_completion_interruptible(&nvusb->comp_req);
			if (nvusb->req == NULL)
				goto out;
			req = nvusb->req;

			data_length -= req->actual;

			if (data_direction) {
				memset(req->buf, nvusb_pattern, req->length);
			} else {
				buf = req->buf;
				for (i = 0; i < req->length; i++) {
					if (buf[i] != nvusb_pattern) {
						g_sttime.tv_sec =
						g_entime.tv_sec = 0;
						g_sttime.tv_usec =
						g_entime.tv_usec = 0;
						data_err =
						-US_BULK_STAT_BAD_DATA;
					}
				}
			}

			if (data_length <= 0)
				break;
			if (data_length < nvusb_buflen) {
				kfree(req->buf);
				req->length = data_length;
				req->buf = kmalloc(req->length, GFP_ATOMIC);
				if (!req->buf) {
					usb_ep_free_request(ep, req);
					req = NULL;
					goto out;
				}
			}

			status = usb_ep_queue(ep, req, GFP_ATOMIC);
			if (status) {
				ERROR(cdev,
				"kill %s:  resubmit %d bytes --> %d\n",
						ep->name, req->length, status);
				usb_ep_set_halt(ep);
				goto out;
			}

		}

		if (data_err == US_BULK_STAT_OK)
			do_gettimeofday(&g_entime);

		INFO(cdev, "Time Taken is %ld\n",
		 nvusb_calc_time(g_sttime, g_entime));
		kfree(req->buf);
		usb_ep_free_request(ep, req);

cbs_phase:
		send_CBS(nvusb, bcb,
		 nvusb_calc_time(g_sttime, g_entime), data_err);

		ep = nvusb->out_ep;
		req = cbw_req;
		status = usb_ep_queue(ep, req, GFP_ATOMIC);
		if (status) {
			ERROR(cdev, "kill %s:  resubmit %d bytes --> %d\n",
					ep->name, req->length, status);
			usb_ep_set_halt(ep);
			goto out;
		}

	}

	kfree(bcb);
	return 0;
out:
	kfree(bcb);
out1:
	kfree(cbw_req->buf);
	usb_ep_free_request(nvusb->out_ep, cbw_req);
out2:
	return -1;

}

static int start_ep(struct f_nvusb *nvusb, bool is_in)
{
	struct usb_ep		*ep;
	struct usb_request	*req;
	int			status;

	if (is_in)
		return 0;

	ep =  nvusb->out_ep;

	req = usb_ep_alloc_request(ep, GFP_ATOMIC);
	if (req) {
		req->length = US_BULK_CB_WRAP_LEN;
		req->buf = kmalloc(req->length, GFP_ATOMIC);
		if (!req->buf) {
			usb_ep_free_request(ep, req);
			req = NULL;
		}
	}
	if (!req)
		return -ENOMEM;

	req->complete = nvusb_complete;
	memset(req->buf, 0x0, req->length);

	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (status) {
		struct usb_composite_dev	*cdev;

		cdev = nvusb->function.config->cdev;
		ERROR(cdev, "start %s %s --> %d\n",
				is_in ? "IN" : "OUT",
				ep->name, status);
		free_nvusbep_req(ep, req);
	}

	return status;
}

static void disable_nvusb_ep(struct usb_composite_dev *cdev,
struct usb_ep *ep)
{
	int			value;

	if (ep->driver_data) {
		value = usb_ep_disable(ep);
		if (value < 0)
			DBG(cdev, "disable %s --> %d\n",
					ep->name, value);
		ep->driver_data = NULL;
	}
}

static void disable_nvusb_eps(struct f_nvusb *nvusb)
{
	struct usb_composite_dev	*cdev;

	cdev = nvusb->function.config->cdev;
	disable_nvusb_ep(cdev, nvusb->in_ep);
	disable_nvusb_ep(cdev, nvusb->out_ep);
	VDBG(cdev, "%s disabled\n", nvusb->function.name);
}

static int
enable_nvusb_eps(struct usb_composite_dev *cdev,
struct f_nvusb *nvusb)
{
	int					result = 0;
	struct usb_ep				*ep;

	ep = nvusb->in_ep;
	result = config_ep_by_speed(cdev->gadget, &(nvusb->function), ep);
	if (result)
		return result;
	result = usb_ep_enable(ep);
	if (result < 0)
		return result;
	ep->driver_data = nvusb;

	ep = nvusb->out_ep;
	result = config_ep_by_speed(cdev->gadget, &(nvusb->function), ep);
	if (result)
		goto fail;
	result = usb_ep_enable(ep);
	if (result < 0)
		goto fail;
	ep->driver_data = nvusb;

	result = start_ep(nvusb, false);
	if (result < 0) {
		usb_ep_disable(ep);
		ep->driver_data = NULL;
		goto fail;
	}

	DBG(cdev, "%s enabled\n", nvusb->function.name);
	return result;
fail:
		ep = nvusb->in_ep;
		usb_ep_disable(ep);
		ep->driver_data = NULL;
		return result;

}

static int nvusb_set_alt(struct usb_function *f,
		unsigned intf, unsigned alt)
{
	struct f_nvusb	*nvusb = func_to_nvusb(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	if (nvusb->in_ep->driver_data)
		disable_nvusb_eps(nvusb);
	return enable_nvusb_eps(cdev, nvusb);
}

static void nvusb_disable(struct usb_function *f)
{
	struct f_nvusb	*nvusb = func_to_nvusb(f);

	disable_nvusb_eps(nvusb);
}

/*-------------------------------------------------------------------------*/

static int nvusb_bind_config(struct usb_configuration *c)
{
	struct f_nvusb	*nvusb;
	int			status;

	nvusb = kzalloc(sizeof *nvusb, GFP_KERNEL);
	if (!nvusb)
		return -ENOMEM;
	nvusb->function.name = "nvusb";
	nvusb->function.descriptors = nvusbfs_descs;
	nvusb->function.bind = nvusb_bind_func;
	nvusb->function.unbind = nvusb_unbind_func;
	nvusb->function.set_alt = nvusb_set_alt;
	nvusb->function.disable = nvusb_disable;

	nvusb->task =
	kthread_create(nvusb_kthread, nvusb,
			   "nvusb_kthread");

	if (IS_ERR(nvusb->task)) {
		status = PTR_ERR(nvusb->task);
		return status;
	}

	wake_up_process(nvusb->task);

	status = usb_add_function(c, &nvusb->function);
	if (status)
		kfree(nvusb);
	return status;
}

static int nvusb_setup(struct usb_configuration *c,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_request	*req = c->cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	req->length = USB_BUFSIZ;

	switch (ctrl->bRequest) {

	/*
	 * Add any Specific Control Request if required.
	 * Currently no Control Request are
	 * implemented here.
	 */

	default:
		VDBG(c->cdev,
			"unknown control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	if (value >= 0) {
		VDBG(c->cdev, "req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(c->cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(c->cdev, "response, err %d\n",
					value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static struct usb_configuration nvusb_driver = {
	.label		= "nvusb",
	.strings	= nvusb_strings,
	.setup		= nvusb_setup,
	.bConfigurationValue = 3,
	.bmAttributes	= USB_CONFIG_ATT_SELFPOWER,
};

/**
 * nvusb_add - add a configuration to a device
 * @cdev: the device to support the configuration
 */
int __init nvusb_add(struct usb_composite_dev *cdev)
{
	int id;

	/* allocate string ID(s) */
	id = usb_string_id(cdev);
	if (id < 0)
		return id;
	strings_nvusb[0].id = id;

	nvusb_intf.iInterface = id;
	nvusb_driver.iConfiguration = id;

	return usb_add_config(cdev, &nvusb_driver,
			nvusb_bind_config);
}
