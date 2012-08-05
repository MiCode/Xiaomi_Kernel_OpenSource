/*
 * f_qdss.c -- QDSS function Driver
 *
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/usb/usb_qdss.h>
#include <linux/usb/msm_hsusb.h>

#include "f_qdss.h"
#include "u_qdss.c"

static DEFINE_SPINLOCK(d_lock);
static LIST_HEAD(usb_qdss_ch_list);

static struct usb_interface_descriptor qdss_data_intf_desc = {
	.bLength            =	sizeof qdss_data_intf_desc,
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bAlternateSetting  =   0,
	.bNumEndpoints      =	1,
	.bInterfaceClass    =	0xff,
	.bInterfaceSubClass =	0xff,
	.bInterfaceProtocol =	0xff,
};

static struct usb_endpoint_descriptor qdss_hs_data_desc = {
	.bLength              =	 USB_DT_ENDPOINT_SIZE,
	.bDescriptorType      =	 USB_DT_ENDPOINT,
	.bEndpointAddress     =	 USB_DIR_IN,
	.bmAttributes         =	 USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize       =	 __constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor qdss_ss_data_desc = {
	.bLength              =	 USB_DT_ENDPOINT_SIZE,
	.bDescriptorType      =	 USB_DT_ENDPOINT,
	.bEndpointAddress     =	 USB_DIR_IN,
	.bmAttributes         =  USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize       =	 __constant_cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor qdss_data_ep_comp_desc = {
	.bLength              =	 sizeof qdss_data_ep_comp_desc,
	.bDescriptorType      =	 USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst            =	 1,
	.bmAttributes         =	 0,
	.wBytesPerInterval    =	 0,
};

static struct usb_interface_descriptor qdss_ctrl_intf_desc = {
	.bLength            =	sizeof qdss_ctrl_intf_desc,
	.bDescriptorType    =	USB_DT_INTERFACE,
	.bAlternateSetting  =   0,
	.bNumEndpoints      =	2,
	.bInterfaceClass    =	0xff,
	.bInterfaceSubClass =	0xff,
	.bInterfaceProtocol =	0xff,
};

static struct usb_endpoint_descriptor qdss_hs_ctrl_in_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_IN,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor qdss_ss_ctrl_in_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_IN,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	__constant_cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor qdss_hs_ctrl_out_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_OUT,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	__constant_cpu_to_le16(512),
};

static struct usb_endpoint_descriptor qdss_ss_ctrl_out_desc = {
	.bLength            =	USB_DT_ENDPOINT_SIZE,
	.bDescriptorType    =	USB_DT_ENDPOINT,
	.bEndpointAddress   =	USB_DIR_OUT,
	.bmAttributes       =	USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize     =	__constant_cpu_to_le16(0x400),
};

static struct usb_ss_ep_comp_descriptor qdss_ctrl_in_ep_comp_desc = {
	.bLength            =	sizeof qdss_ctrl_in_ep_comp_desc,
	.bDescriptorType    =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst          =	0,
	.bmAttributes       =	0,
	.wBytesPerInterval  =	0,
};

static struct usb_ss_ep_comp_descriptor qdss_ctrl_out_ep_comp_desc = {
	.bLength            =	sizeof qdss_ctrl_out_ep_comp_desc,
	.bDescriptorType    =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst          =	0,
	.bmAttributes       =	0,
	.wBytesPerInterval  =	0,
};

static struct usb_descriptor_header *qdss_hs_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_hs_data_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_intf_desc,
	(struct usb_descriptor_header *) &qdss_hs_ctrl_in_desc,
	(struct usb_descriptor_header *) &qdss_hs_ctrl_out_desc,
	NULL,
};

static struct usb_descriptor_header *qdss_ss_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_ss_data_desc,
	(struct usb_descriptor_header *) &qdss_data_ep_comp_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_intf_desc,
	(struct usb_descriptor_header *) &qdss_ss_ctrl_in_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_in_ep_comp_desc,
	(struct usb_descriptor_header *) &qdss_ss_ctrl_out_desc,
	(struct usb_descriptor_header *) &qdss_ctrl_out_ep_comp_desc,
	NULL,
};

/* string descriptors: */
#define QDSS_DATA_IDX	0
#define QDSS_CTRL_IDX	1

static struct usb_string qdss_string_defs[] = {
	[QDSS_DATA_IDX].s = "QDSS DATA",
	[QDSS_CTRL_IDX].s = "QDSS CTRL",
	{}, /* end of list */
};

static struct usb_gadget_strings qdss_string_table = {
	.language =		0x0409,
	.strings =		qdss_string_defs,
};

static struct usb_gadget_strings *qdss_strings[] = {
	&qdss_string_table,
	NULL,
};

static inline struct f_qdss *func_to_qdss(struct usb_function *f)
{
	return container_of(f, struct f_qdss, function);
}

/*----------------------------------------------------------------------*/

static void qdss_ctrl_write_complete(struct usb_ep *ep,
	struct usb_request *req)
{
	struct f_qdss *qdss = ep->driver_data;
	struct qdss_request *d_req = req->context;
	unsigned long flags;

	pr_debug("qdss_ctrl_write_complete\n");

	if (!req->status) {
		/* send zlp */
		if ((req->length >= ep->maxpacket) &&
				((req->length % ep->maxpacket) == 0)) {
			req->length = 0;
			d_req->actual = req->actual;
			d_req->status = req->status;
			usb_ep_queue(qdss->ctrl_in, req, GFP_ATOMIC);
			return;
		}
	}

	spin_lock_irqsave(&qdss->lock, flags);
	list_add_tail(&req->list, &qdss->ctrl_write_pool);
	if (req->length != 0) {
		d_req->actual = req->actual;
		d_req->status = req->status;
	}
	spin_unlock_irqrestore(&qdss->lock, flags);

	if (qdss->ch.notify)
		qdss->ch.notify(qdss->ch.priv, USB_QDSS_CTRL_WRITE_DONE, d_req,
			NULL);
}

static void qdss_ctrl_read_complete(struct usb_ep *ep,
	struct usb_request *req)
{
	struct f_qdss *qdss = ep->driver_data;
	struct qdss_request *d_req = req->context;
	unsigned long flags;

	pr_debug("qdss_ctrl_read_complete\n");

	d_req->actual = req->actual;
	d_req->status = req->status;

	spin_lock_irqsave(&qdss->lock, flags);
	list_add_tail(&req->list, &qdss->ctrl_read_pool);
	spin_unlock_irqrestore(&qdss->lock, flags);

	if (qdss->ch.notify)
		qdss->ch.notify(qdss->ch.priv, USB_QDSS_CTRL_READ_DONE, d_req,
			NULL);
}

void usb_qdss_free_req(struct usb_qdss_ch *ch)
{
	struct f_qdss *qdss;
	struct usb_request *req;
	struct list_head *act, *tmp;

	pr_debug("usb_qdss_free_req\n");

	qdss = ch->priv_usb;
	if (!qdss) {
		pr_err("usb_qdss_free_req: qdss ctx is NULL\n");
		return;
	}

	list_for_each_safe(act, tmp, &qdss->ctrl_write_pool) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(qdss->ctrl_in, req);
	}

	list_for_each_safe(act, tmp, &qdss->ctrl_read_pool) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(qdss->ctrl_out, req);
	}
}
EXPORT_SYMBOL(usb_qdss_free_req);

int usb_qdss_alloc_req(struct usb_qdss_ch *ch, int no_write_buf,
	int no_read_buf)
{
	struct f_qdss *qdss = ch->priv_usb;
	struct usb_request *req;
	int i;

	pr_debug("usb_qdss_alloc_req\n");

	if (no_write_buf <= 0 || no_read_buf <= 0 || !qdss) {
		pr_err("usb_qdss_alloc_req: missing params\n");
		return -ENODEV;
	}

	for (i = 0; i < no_write_buf; i++) {
		req = usb_ep_alloc_request(qdss->ctrl_in, GFP_ATOMIC);
		if (!req) {
			pr_err("usb_qdss_alloc_req: ctrl_in allocation err\n");
			goto fail;
		}
		req->complete = qdss_ctrl_write_complete;
		list_add_tail(&req->list, &qdss->ctrl_write_pool);
	}

	for (i = 0; i < no_read_buf; i++) {
		req = usb_ep_alloc_request(qdss->ctrl_out, GFP_ATOMIC);
		if (!req) {
			pr_err("usb_qdss_alloc_req:ctrl_out allocation err\n");
			goto fail;
		}
		req->complete = qdss_ctrl_read_complete;
		list_add_tail(&req->list, &qdss->ctrl_read_pool);
	}

	return 0;

fail:
	usb_qdss_free_req(ch);
	return -ENOMEM;
}
EXPORT_SYMBOL(usb_qdss_alloc_req);

static void clear_eps(struct usb_function *f)
{
	struct f_qdss *qdss = func_to_qdss(f);

	pr_debug("clear_eps\n");

	if (qdss->ctrl_in)
		qdss->ctrl_in->driver_data = NULL;
	if (qdss->ctrl_out)
		qdss->ctrl_out->driver_data = NULL;
	if (qdss->data)
		qdss->data->driver_data = NULL;
}

static void clear_desc(struct usb_gadget *gadget, struct usb_function *f)
{
	pr_debug("clear_desc\n");

	if (gadget_is_superspeed(gadget) && f->ss_descriptors)
		usb_free_descriptors(f->ss_descriptors);

	if (gadget_is_dualspeed(gadget) && f->hs_descriptors)
		usb_free_descriptors(f->hs_descriptors);
}

static int qdss_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_gadget *gadget = c->cdev->gadget;
	struct f_qdss *qdss = func_to_qdss(f);
	struct usb_ep *ep;
	int iface;

	pr_debug("qdss_bind\n");

	if (!gadget_is_dualspeed(gadget) && !gadget_is_superspeed(gadget)) {
		pr_err("qdss_bind: full-speed is not supported\n");
		return -ENOTSUPP;
	}

	/* Allocate data I/F */
	iface = usb_interface_id(c, f);
	if (iface < 0) {
		pr_err("interface allocation error\n");
		return iface;
	}
	qdss_data_intf_desc.bInterfaceNumber = iface;
	qdss->data_iface_id = iface;

	/* Allocate ctrl I/F */
	iface = usb_interface_id(c, f);
	if (iface < 0) {
		pr_err("interface allocation error\n");
		return iface;
	}
	qdss_ctrl_intf_desc.bInterfaceNumber = iface;
	qdss->ctrl_iface_id = iface;

	ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_data_desc,
		&qdss_data_ep_comp_desc);
	if (!ep) {
		pr_err("ep_autoconfig error\n");
		goto fail;
	}
	qdss->data = ep;
	ep->driver_data = qdss;

	ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_ctrl_in_desc,
		&qdss_ctrl_in_ep_comp_desc);
	if (!ep) {
		pr_err("ep_autoconfig error\n");
		goto fail;
	}
	qdss->ctrl_in = ep;
	ep->driver_data = qdss;

	ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_ctrl_out_desc,
		&qdss_ctrl_out_ep_comp_desc);
	if (!ep) {
		pr_err("ep_autoconfig error\n");
		goto fail;
	}
	qdss->ctrl_out = ep;
	ep->driver_data = qdss;

	/*update descriptors*/
	qdss_hs_data_desc.bEndpointAddress =
		qdss_ss_data_desc.bEndpointAddress;
	qdss_hs_ctrl_in_desc.bEndpointAddress =
		qdss_ss_ctrl_in_desc.bEndpointAddress;
	qdss_hs_ctrl_out_desc.bEndpointAddress =
		qdss_ss_ctrl_out_desc.bEndpointAddress;

	f->hs_descriptors = usb_copy_descriptors(qdss_hs_desc);
	if (!f->hs_descriptors) {
		pr_err("usb_copy_descriptors error\n");
		goto fail;
	}

	/* update ss descriptors */
	if (gadget_is_superspeed(gadget)) {
		f->ss_descriptors = usb_copy_descriptors(qdss_ss_desc);
		if (!f->ss_descriptors) {
			pr_err("usb_copy_descriptors error\n");
			goto fail;
		}
	}

	return 0;
fail:
	clear_eps(f);
	clear_desc(gadget, f);
	return -ENOTSUPP;
}


static void qdss_unbind(struct usb_configuration *c, struct usb_function *f)
{
	pr_debug("qdss_unbind\n");

	clear_desc(c->cdev->gadget, f);
}

static void qdss_eps_disable(struct usb_function *f)
{
	struct f_qdss  *qdss = func_to_qdss(f);

	pr_debug("qdss_eps_disable\n");

	if (qdss->ctrl_in_enabled) {
		usb_ep_disable(qdss->ctrl_in);
		qdss->ctrl_in_enabled = 0;
		qdss->ctrl_in->driver_data = NULL;
	}

	if (qdss->ctrl_out_enabled) {
		usb_ep_disable(qdss->ctrl_out);
		qdss->ctrl_out_enabled = 0;
		qdss->ctrl_out->driver_data = NULL;
	}

	if (qdss->data_enabled) {
		usb_ep_disable(qdss->data);
		qdss->data_enabled = 0;
		qdss->data->driver_data = NULL;
	}
}

static void qdss_disable(struct usb_function *f)
{
	struct f_qdss	*qdss = func_to_qdss(f);
	unsigned long flags;
	int status;

	pr_debug("qdss_disable\n");

	spin_lock_irqsave(&qdss->lock, flags);
	qdss->usb_connected = 0;
	spin_unlock_irqrestore(&qdss->lock, flags);

	/*cancell all active xfers*/
	qdss_eps_disable(f);

	/* notify qdss to cancell all active transfers*/
	if (qdss->ch.notify) {
		qdss->ch.notify(qdss->ch.priv, USB_QDSS_DISCONNECT, NULL,
			NULL);
		/* If the app was never started, we can skip USB BAM reset */
		status = set_qdss_data_connection(qdss->data,
			qdss->data->address, 0);
		if (status)
			pr_err("qdss_disable error");
	}
}

static void usb_qdss_work_func(struct work_struct *work)
{
	struct f_qdss *qdss = container_of(work, struct f_qdss, qdss_work);
	int status;

	pr_debug("usb_qdss_work_func\n");

	status = init_data(qdss->data);
	if (status) {
		pr_err("init_data error");
		return;
	}

	status = set_qdss_data_connection(qdss->data,
		qdss->data->address, 1);
	if (status) {
		pr_err("set_qdss_data_connection error");
		return;
	}
	if (qdss->ch.notify)
		qdss->ch.notify(qdss->ch.priv, USB_QDSS_CONNECT, NULL,
			&qdss->ch);

	status = send_sps_req(qdss->data);
	if (status) {
		pr_err("send_sps_req error\n");
		return;
	}
}

static int qdss_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_qdss  *qdss = func_to_qdss(f);
	struct usb_gadget *gadget = f->config->cdev->gadget;
	struct usb_qdss_ch *ch = &qdss->ch;
	int ret = 0;

	pr_debug("qdss_set_alt\n");

	if (alt != 0)
		goto fail;

	if (gadget->speed != USB_SPEED_SUPER &&
		gadget->speed != USB_SPEED_HIGH) {
			pr_err("qdss_st_alt: qdss supportes HS or SS only\n");
			goto fail;
	}

	if (intf == qdss->data_iface_id) {
		if (config_ep_by_speed(gadget, f, qdss->data))
			return -EINVAL;

		ret = usb_ep_enable(qdss->data);
		if (ret)
			goto fail;

		qdss->data->driver_data = qdss;
		qdss->data_enabled = 1;

	} else if (intf == qdss->ctrl_iface_id) {
		if (config_ep_by_speed(gadget, f, qdss->ctrl_in))
			return -EINVAL;

		ret = usb_ep_enable(qdss->ctrl_in);
		if (ret)
			goto fail;

		qdss->ctrl_in->driver_data = qdss;
		qdss->ctrl_in_enabled = 1;

		if (config_ep_by_speed(gadget, f, qdss->ctrl_out))
			return -EINVAL;

		ret = usb_ep_enable(qdss->ctrl_out);
		if (ret)
			goto fail;

		qdss->ctrl_out->driver_data = qdss;
		qdss->ctrl_out_enabled = 1;
	}

	if (qdss->ctrl_out_enabled && qdss->ctrl_in_enabled &&
		qdss->data_enabled)
		qdss->usb_connected = 1;

	if (qdss->usb_connected && ch->app_conn)
		schedule_work(&qdss->qdss_work);

	return 0;
fail:
	pr_err("qdss_set_alt failed\n");
	qdss_eps_disable(f);
	return ret;
}

static int qdss_bind_config(struct usb_configuration *c, const char *name)
{
	struct f_qdss *qdss;
	int status, found = 0;
	struct usb_qdss_ch *ch;
	unsigned long flags;

	pr_debug("qdss_bind_config\n");

	if (qdss_string_defs[QDSS_DATA_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		qdss_string_defs[QDSS_DATA_IDX].id = status;
		qdss_data_intf_desc.iInterface = status;

		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		qdss_string_defs[QDSS_CTRL_IDX].id = status;
		qdss_ctrl_intf_desc.iInterface = status;
	}

	spin_lock_irqsave(&d_lock, flags);
	list_for_each_entry(ch, &usb_qdss_ch_list, list) {
		if (!strncmp(name, ch->name, sizeof(ch->name))) {
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("qdss_bind_config allocating channel\n");
		qdss = kzalloc(sizeof *qdss, GFP_ATOMIC);
		if (!qdss) {
			pr_err("qdss_bind_config: allocating channel failed\n");
			spin_unlock_irqrestore(&d_lock, flags);
			return -ENOMEM;
		}

		ch = &qdss->ch;
		ch->name = name;
		list_add_tail(&ch->list, &usb_qdss_ch_list);
	} else {
		qdss = container_of(ch, struct f_qdss, ch);
		ch->priv_usb = qdss;
	}
	spin_unlock_irqrestore(&d_lock, flags);
	qdss->cdev = c->cdev;
	qdss->function.name = name;
	qdss->function.descriptors = qdss_hs_desc;
	qdss->function.hs_descriptors = qdss_hs_desc;
	qdss->function.strings = qdss_strings;
	qdss->function.bind = qdss_bind;
	qdss->function.unbind = qdss_unbind;
	qdss->function.set_alt = qdss_set_alt;
	qdss->function.disable = qdss_disable;
	INIT_LIST_HEAD(&qdss->ctrl_read_pool);
	INIT_LIST_HEAD(&qdss->ctrl_write_pool);
	INIT_WORK(&qdss->qdss_work, usb_qdss_work_func);

	status = usb_add_function(c, &qdss->function);
	if (status) {
		pr_err("qdss usb_add_function failed\n");
		ch->priv_usb = NULL;
		kfree(qdss);
	}

	return status;
}

int usb_qdss_ctrl_read(struct usb_qdss_ch *ch, struct qdss_request *d_req)
{
	struct f_qdss *qdss = ch->priv_usb;
	unsigned long flags;
	struct usb_request *req = NULL;

	pr_debug("usb_qdss_ctrl_read\n");

	if (!qdss)
		return -ENODEV;

	spin_lock_irqsave(&qdss->lock, flags);

	if (qdss->usb_connected == 0) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EIO;
	}

	if (list_empty(&qdss->ctrl_read_pool)) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		pr_err("error: usb_qdss_ctrl_read list is empty\n");
		return -EAGAIN;
	}

	req = list_first_entry(&qdss->ctrl_read_pool, struct usb_request, list);
	list_del(&req->list);
	spin_unlock_irqrestore(&qdss->lock, flags);

	req->buf = d_req->buf;
	req->length = d_req->length;
	req->context = d_req;

	if (usb_ep_queue(qdss->ctrl_out, req, GFP_ATOMIC)) {
		/* If error add the link to linked list again*/
		spin_lock_irqsave(&qdss->lock, flags);
		list_add_tail(&req->list, &qdss->ctrl_read_pool);
		spin_unlock_irqrestore(&qdss->lock, flags);
		pr_err("qdss usb_ep_queue failed\n");
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(usb_qdss_ctrl_read);

int usb_qdss_ctrl_write(struct usb_qdss_ch *ch, struct qdss_request *d_req)
{
	struct f_qdss *qdss = ch->priv_usb;
	unsigned long flags;
	struct usb_request *req = NULL;

	pr_debug("usb_qdss_ctrl_write\n");

	if (!qdss)
		return -ENODEV;

	spin_lock_irqsave(&qdss->lock, flags);

	if (qdss->usb_connected == 0) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EIO;
	}

	if (list_empty(&qdss->ctrl_write_pool)) {
		pr_err("error: usb_qdss_ctrl_write list is empty\n");
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EAGAIN;
	}

	req = list_first_entry(&qdss->ctrl_write_pool, struct usb_request,
		list);
	list_del(&req->list);
	spin_unlock_irqrestore(&qdss->lock, flags);

	req->buf = d_req->buf;
	req->length = d_req->length;
	req->context = d_req;
	if (usb_ep_queue(qdss->ctrl_in, req, GFP_ATOMIC)) {
		spin_lock_irqsave(&qdss->lock, flags);
		list_add_tail(&req->list, &qdss->ctrl_write_pool);
		spin_unlock_irqrestore(&qdss->lock, flags);
		pr_err("qdss usb_ep_queue failed\n");
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(usb_qdss_ctrl_write);

struct usb_qdss_ch *usb_qdss_open(const char *name, void *priv,
	void (*notify)(void *, unsigned, struct qdss_request *,
		struct usb_qdss_ch *))
{
	struct usb_qdss_ch *ch;
	struct f_qdss *qdss;
	unsigned long flags;
	int found = 0;

	pr_debug("usb_qdss_open\n");

	if (!notify) {
		pr_err("usb_qdss_open: notification func is missing\n");
		return NULL;
	}

	spin_lock_irqsave(&d_lock, flags);
	/* Check if we already have a channel with this name */
	list_for_each_entry(ch, &usb_qdss_ch_list, list) {
		if (!strncmp(name, ch->name, sizeof(ch->name))) {
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("usb_qdss_open: allocation qdss ctx\n");
		qdss = kzalloc(sizeof(*qdss), GFP_ATOMIC);
		if (!qdss) {
			spin_unlock_irqrestore(&d_lock, flags);
			return ERR_PTR(-ENOMEM);
		}
		ch = &qdss->ch;
		list_add_tail(&ch->list, &usb_qdss_ch_list);
	} else {
		pr_debug("usb_qdss_open: qdss ctx found\n");
		qdss = container_of(ch, struct f_qdss, ch);
		ch->priv_usb = qdss;
	}

	ch->name = name;
	ch->priv = priv;
	ch->notify = notify;
	ch->app_conn = 1;
	spin_unlock_irqrestore(&d_lock, flags);

	/* the case USB cabel was connected befor qdss called  qdss_open*/
	if (qdss->usb_connected == 1)
		schedule_work(&qdss->qdss_work);

	return ch;
}
EXPORT_SYMBOL(usb_qdss_open);

void usb_qdss_close(struct usb_qdss_ch *ch)
{
	struct f_qdss *qdss = ch->priv_usb;
	unsigned long flags;

	pr_debug("usb_qdss_close\n");

	spin_lock_irqsave(&d_lock, flags);
	/*free not used reqests*/
	usb_qdss_free_req(ch);
	usb_ep_dequeue(qdss->data, qdss->endless_req);
	qdss->endless_req = NULL;
	spin_unlock_irqrestore(&d_lock, flags);
}
EXPORT_SYMBOL(usb_qdss_close);

static void qdss_cleanup(void)
{
	struct f_qdss *qdss;
	struct list_head *act, *tmp;
	struct usb_qdss_ch *_ch;
	unsigned long flags;

	pr_debug("qdss_cleanup\n");

	list_for_each_safe(act, tmp, &usb_qdss_ch_list) {
		_ch = list_entry(act, struct usb_qdss_ch, list);
		qdss = container_of(_ch, struct f_qdss, ch);
		spin_lock_irqsave(&d_lock, flags);

		if (!_ch->priv) {
			list_del(&_ch->list);
			kfree(qdss);
		}
		spin_unlock_irqrestore(&d_lock, flags);
	}
}

static int qdss_setup(void)
{
	return 0;
}

