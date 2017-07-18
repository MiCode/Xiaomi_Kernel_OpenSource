/*
 * f_qdss.c -- QDSS function Driver
 *
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.

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
#include <linux/usb/cdc.h>

#include "gadget_chips.h"
#include "f_qdss.h"
#include "u_qdss.c"
#include "usb_gadget_xport.h"
#include "u_data_ipa.h"
#include "u_rmnet.h"
#include "u_ether.h"

static unsigned int nr_qdss_ports;
static unsigned int no_data_bam_ports;
static unsigned int data_hsic_ports_no;
static unsigned int no_ipa_ports;
static unsigned int no_bam_dmux_ports;

static struct qdss_ports {
	enum transport_type		data_xport;
	unsigned char			data_xport_num;
	enum transport_type		ctrl_xport;
	unsigned char			ctrl_xport_num;
	unsigned	char		port_num;
	struct f_qdss			*port;
	struct gadget_ipa_port		ipa_port;
	struct grmnet			bam_dmux_port;
	struct gether			gether_port;
} qdss_ports[NR_QDSS_PORTS];


static DEFINE_SPINLOCK(qdss_lock);
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

static struct usb_descriptor_header *qdss_hs_data_only_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_hs_data_desc,
	NULL,
};

static struct usb_descriptor_header *qdss_ss_data_only_desc[] = {
	(struct usb_descriptor_header *) &qdss_data_intf_desc,
	(struct usb_descriptor_header *) &qdss_ss_data_desc,
	(struct usb_descriptor_header *) &qdss_data_ep_comp_desc,
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
	return container_of(f, struct f_qdss, port.function);
}

/*----------------------------------------------------------------------*/

static void qdss_write_complete(struct usb_ep *ep,
	struct usb_request *req)
{
	struct f_qdss *qdss = ep->driver_data;
	struct qdss_request *d_req = req->context;
	struct usb_ep *in;
	struct list_head *list_pool;
	enum qdss_state state;
	unsigned long flags;

	pr_debug("qdss_ctrl_write_complete\n");

	if (qdss->debug_inface_enabled) {
		in = qdss->port.ctrl_in;
		list_pool = &qdss->ctrl_write_pool;
		state = USB_QDSS_CTRL_WRITE_DONE;
	} else {
		in = qdss->port.data;
		list_pool = &qdss->data_write_pool;
		state = USB_QDSS_DATA_WRITE_DONE;
	}

	if (!req->status) {
		/* send zlp */
		if ((req->length >= ep->maxpacket) &&
				((req->length % ep->maxpacket) == 0)) {
			req->length = 0;
			d_req->actual = req->actual;
			d_req->status = req->status;
			if (!usb_ep_queue(in, req, GFP_ATOMIC))
				return;
		}
	}

	spin_lock_irqsave(&qdss->lock, flags);
	list_add_tail(&req->list, list_pool);
	if (req->length != 0) {
		d_req->actual = req->actual;
		d_req->status = req->status;
	}
	spin_unlock_irqrestore(&qdss->lock, flags);

	if (qdss->ch.notify)
		qdss->ch.notify(qdss->ch.priv, state, d_req,
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

	list_for_each_safe(act, tmp, &qdss->data_write_pool) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(qdss->port.data, req);
	}

	list_for_each_safe(act, tmp, &qdss->ctrl_write_pool) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(qdss->port.ctrl_in, req);
	}

	list_for_each_safe(act, tmp, &qdss->ctrl_read_pool) {
		req = list_entry(act, struct usb_request, list);
		list_del(&req->list);
		usb_ep_free_request(qdss->port.ctrl_out, req);
	}
}
EXPORT_SYMBOL(usb_qdss_free_req);

int usb_qdss_alloc_req(struct usb_qdss_ch *ch, int no_write_buf,
	int no_read_buf)
{
	struct f_qdss *qdss = ch->priv_usb;
	struct usb_request *req;
	struct usb_ep *in;
	struct list_head *list_pool;
	int i;

	pr_debug("usb_qdss_alloc_req\n");

	if (!qdss) {
		pr_err("usb_qdss_alloc_req: channel %s closed\n", ch->name);
		return -ENODEV;
	}

	if ((qdss->debug_inface_enabled &&
		(no_write_buf <= 0 || no_read_buf <= 0)) ||
		(!qdss->debug_inface_enabled &&
		(no_write_buf <= 0 || no_read_buf))) {
		pr_err("usb_qdss_alloc_req: missing params\n");
		return -ENODEV;
	}

	if (qdss->debug_inface_enabled) {
		in = qdss->port.ctrl_in;
		list_pool = &qdss->ctrl_write_pool;
	} else {
		in = qdss->port.data;
		list_pool = &qdss->data_write_pool;
	}

	for (i = 0; i < no_write_buf; i++) {
		req = usb_ep_alloc_request(in, GFP_ATOMIC);
		if (!req) {
			pr_err("usb_qdss_alloc_req: ctrl_in allocation err\n");
			goto fail;
		}
		req->complete = qdss_write_complete;
		list_add_tail(&req->list, list_pool);
	}

	for (i = 0; i < no_read_buf; i++) {
		req = usb_ep_alloc_request(qdss->port.ctrl_out, GFP_ATOMIC);
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

	if (qdss->port.ctrl_in)
		qdss->port.ctrl_in->driver_data = NULL;
	if (qdss->port.ctrl_out)
		qdss->port.ctrl_out->driver_data = NULL;
	if (qdss->port.data)
		qdss->port.data->driver_data = NULL;
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
	int iface, ret = -ENOTSUPP;
	struct eth_dev *edev;
	enum transport_type dxport;

	pr_debug("qdss_bind\n");

	dxport = qdss_ports[qdss->port_num].data_xport;

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

	if (qdss->debug_inface_enabled) {
		/* Allocate ctrl I/F */
		iface = usb_interface_id(c, f);
		if (iface < 0) {
			pr_err("interface allocation error\n");
			return iface;
		}
		qdss_ctrl_intf_desc.bInterfaceNumber = iface;
		qdss->ctrl_iface_id = iface;
	}

	ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_data_desc,
		&qdss_data_ep_comp_desc);
	if (!ep) {
		pr_err("ep_autoconfig error\n");
		goto fail;
	}
	qdss->port.data = ep;

	/*
	 * Populate same for u_ether(gether_connect()) which uses
	 * gether_port struct
	 */
	if (dxport == USB_GADGET_XPORT_ETHER)
		qdss_ports[qdss->port_num].gether_port.in_ep = ep;

	ep->driver_data = qdss;

	if (qdss->debug_inface_enabled) {
		ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_ctrl_in_desc,
			&qdss_ctrl_in_ep_comp_desc);
		if (!ep) {
			pr_err("ep_autoconfig error\n");
			goto fail;
		}
		qdss->port.ctrl_in = ep;
		ep->driver_data = qdss;

		ep = usb_ep_autoconfig_ss(gadget, &qdss_ss_ctrl_out_desc,
			&qdss_ctrl_out_ep_comp_desc);
		if (!ep) {
			pr_err("ep_autoconfig error\n");
			goto fail;
		}
		qdss->port.ctrl_out = ep;
		ep->driver_data = qdss;
	}

	/*update descriptors*/
	qdss_hs_data_desc.bEndpointAddress =
		qdss_ss_data_desc.bEndpointAddress;
	if (qdss->debug_inface_enabled) {
		qdss_hs_ctrl_in_desc.bEndpointAddress =
		qdss_ss_ctrl_in_desc.bEndpointAddress;
		qdss_hs_ctrl_out_desc.bEndpointAddress =
		qdss_ss_ctrl_out_desc.bEndpointAddress;
		f->hs_descriptors = usb_copy_descriptors(qdss_hs_desc);
	} else
		f->hs_descriptors = usb_copy_descriptors(
							qdss_hs_data_only_desc);
	if (!f->hs_descriptors) {
		pr_err("usb_copy_descriptors error\n");
		goto fail;
	}

	/* update ss descriptors */
	if (gadget_is_superspeed(gadget)) {
		if (qdss->debug_inface_enabled)
			f->ss_descriptors =
			usb_copy_descriptors(qdss_ss_desc);
		else
			f->ss_descriptors =
			usb_copy_descriptors(qdss_ss_data_only_desc);
		if (!f->ss_descriptors) {
			pr_err("usb_copy_descriptors error\n");
			goto fail;
		}
	}

	if (dxport == USB_GADGET_XPORT_ETHER) {
		pr_debug("USB_GADGET_XPORT_ETHER\n");
		edev = gether_setup_name(c->cdev->gadget, NULL, NULL, NULL,
				QMULT_DEFAULT, "dpl_usb");
		if (IS_ERR(edev)) {
			pr_err("%s: gether_setup failed\n", __func__);
			ret = PTR_ERR(edev);
			goto fail;
		}
		qdss_ports[qdss->port_num].gether_port.ioport = edev;
	}

	return 0;
fail:
	clear_eps(f);
	clear_desc(gadget, f);
	return ret;
}


static void qdss_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_qdss  *qdss = func_to_qdss(f);
	struct usb_gadget *gadget = c->cdev->gadget;
	enum transport_type dxport = qdss_ports[qdss->port_num].data_xport;
	int i;

	pr_debug("qdss_unbind\n");

	flush_workqueue(qdss->wq);
	if (dxport ==  USB_GADGET_XPORT_BAM2BAM_IPA)
		ipa_data_flush_workqueue();

	c->cdev->gadget->bam2bam_func_enabled = false;
	clear_eps(f);
	clear_desc(gadget, f);

	for (i = 0; i < nr_qdss_ports; i++) {
		if (qdss_ports[i].data_xport == USB_GADGET_XPORT_ETHER) {
			gether_cleanup(qdss_ports[i].gether_port.ioport);
			qdss_ports[i].gether_port.ioport = NULL;
		}
	}

}

static void qdss_eps_disable(struct usb_function *f)
{
	struct f_qdss  *qdss = func_to_qdss(f);
	enum transport_type dxport;

	dxport = qdss_ports[qdss->port_num].data_xport;

	pr_debug("qdss_eps_disable\n");

	if (qdss->ctrl_in_enabled) {
		usb_ep_disable(qdss->port.ctrl_in);
		qdss->ctrl_in_enabled = 0;
	}

	if (qdss->ctrl_out_enabled) {
		usb_ep_disable(qdss->port.ctrl_out);
		qdss->ctrl_out_enabled = 0;
	}

	/*
	 * In case of data transport is uether, endpoint will be disabled
	 * in gether_disconnect() too. This will lead to disabling of the
	 * same endpoint twice and will print a warning message.
	 * So for uether data transport disable endpoint only in
	 * gether_disconnect()
	 */
	if (qdss->data_enabled && (dxport != USB_GADGET_XPORT_ETHER)) {
		usb_ep_disable(qdss->port.data);
		qdss->data_enabled = 0;
	}
}

static void usb_qdss_disconnect_work(struct work_struct *work)
{
	struct f_qdss *qdss;
	int status;
	unsigned char portno;
	enum transport_type	dxport;
	enum transport_type     ctrl_xport;
	struct gadget_ipa_port *gp;

	qdss = container_of(work, struct f_qdss, disconnect_w);
	dxport = qdss_ports[qdss->port_num].data_xport;
	ctrl_xport = qdss_ports[qdss->port_num].ctrl_xport;
	portno = qdss_ports[qdss->port_num].data_xport_num;

	if (qdss->port_num >= nr_qdss_ports) {
		pr_err("%s: supporting ports#%u port_id:%u", __func__,
				nr_qdss_ports, portno);
		return;
	}
	pr_debug("usb_qdss_disconnect_work\n");

	if (ctrl_xport == USB_GADGET_XPORT_QTI)
		gqti_ctrl_disconnect(&qdss->port, DPL_QTI_CTRL_PORT_NO);

	switch (dxport) {
	case USB_GADGET_XPORT_BAM2BAM:
		/*
		 * Uninitialized init data i.e. ep specific operation.
		 * Notify qdss to cancel all active transfers.
		 */
		if (qdss->ch.app_conn) {
			status = uninit_data(qdss->port.data);
			if (status)
				pr_err("%s: uninit_data error\n", __func__);

			if (qdss->ch.notify)
				qdss->ch.notify(qdss->ch.priv,
					USB_QDSS_DISCONNECT,
					NULL,
					NULL);

			status = set_qdss_data_connection(
					qdss->cdev->gadget,
					qdss->port.data,
					qdss->port.data->address,
					0);
			if (status)
				pr_err("qdss_disconnect error");
		}
		break;
	case USB_GADGET_XPORT_BAM2BAM_IPA:
		gp = &qdss_ports[qdss->port_num].ipa_port;
		ipa_data_disconnect(gp, qdss->port_num);
		break;
	case USB_GADGET_XPORT_BAM_DMUX:
		gbam_disconnect(&qdss_ports[qdss->port_num].bam_dmux_port,
				portno, USB_GADGET_XPORT_BAM_DMUX);
		break;
	case USB_GADGET_XPORT_HSIC:
		pr_debug("usb_qdss_disconnect_work: HSIC transport\n");
		ghsic_data_disconnect(&qdss->port, portno);
		break;
	case USB_GADGET_XPORT_ETHER:
		pr_debug("usb_qdss_disconnect_work: ETHER transport\n");
		gether_disconnect(&qdss_ports[portno].gether_port);
		break;
	case USB_GADGET_XPORT_PCIE:
			if (qdss->ch.notify)
				qdss->ch.notify(qdss->ch.priv,
					USB_QDSS_DISCONNECT,
					NULL,
					NULL);
		break;
	case USB_GADGET_XPORT_NONE:
		break;
	default:
		pr_err("%s: Un-supported transport: %s\n", __func__,
				xport_to_str(dxport));
	}

	/*
	 * Decrement usage count which was incremented
	 * before calling connect work
	 */
	usb_gadget_autopm_put_async(qdss->gadget);
}

static void qdss_disable(struct usb_function *f)
{
	struct f_qdss	*qdss = func_to_qdss(f);
	unsigned long flags;
	unsigned char portno;
	enum transport_type dxport;

	portno = qdss->port_num;
	if (portno >= nr_qdss_ports) {
		pr_err("%s: supporting ports#%u port_id:%u", __func__,
				nr_qdss_ports, portno);
		return;
	}
	pr_debug("qdss_disable\n");
	spin_lock_irqsave(&qdss->lock, flags);
	if (!qdss->usb_connected) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		return;
	}

	dxport = qdss_ports[qdss->port_num].data_xport;
	qdss->usb_connected = 0;
	switch (dxport) {
	case USB_GADGET_XPORT_BAM2BAM_IPA:
	case USB_GADGET_XPORT_BAM_DMUX:
		spin_unlock_irqrestore(&qdss->lock, flags);
		/* Disable usb irq for CI gadget. It will be enabled in
		 * usb_bam_disconnect_pipe() after disconnecting all pipes
		 * and USB BAM reset is done.
		 */
		if (!gadget_is_dwc3(qdss->cdev->gadget))
			msm_usb_irq_disable(true);
		usb_qdss_disconnect_work(&qdss->disconnect_w);
		return;
	default:
		pr_debug("%s: Un-supported transport: %s\n", __func__,
						xport_to_str(dxport));
	}

	spin_unlock_irqrestore(&qdss->lock, flags);
	/*cancell all active xfers*/
	qdss_eps_disable(f);
	if (!gadget_is_dwc3(qdss->cdev->gadget))
		msm_usb_irq_disable(true);
	queue_work(qdss->wq, &qdss->disconnect_w);
}

static int qdss_dpl_ipa_connect(int port_num)
{
	int ret;
	u8 dst_connection_idx;
	struct f_qdss *qdss;
	struct gqdss *g_qdss;
	struct gadget_ipa_port *gp;
	struct usb_gadget *gadget;
	enum usb_ctrl usb_bam_type;
	unsigned long flags;

	ipa_data_port_select(port_num, USB_GADGET_DPL);
	qdss = qdss_ports[port_num].port;

	spin_lock_irqsave(&qdss->lock, flags);
	g_qdss = &qdss->port;
	gp = &qdss_ports[port_num].ipa_port;
	gp->cdev = qdss->cdev;
	gp->in = g_qdss->data;
	/* For DPL, there is no BULK OUT data transfer. */
	gp->out = NULL;
	gp->func = &g_qdss->function;
	gadget = qdss->cdev->gadget;

	spin_unlock_irqrestore(&qdss->lock, flags);

	usb_bam_type = usb_bam_get_bam_type(gadget->name);
	dst_connection_idx = usb_bam_get_connection_idx(usb_bam_type, IPA_P_BAM,
				PEER_PERIPHERAL_TO_USB, USB_BAM_DEVICE, 1);
	if (dst_connection_idx < 0) {
		pr_err("usb_bam_get_connection_idx failed\n");
		return ret;
	}

	ret = ipa_data_connect(gp, port_num, 0, dst_connection_idx);
	if (ret) {
		pr_err("ipa_data_connect failed: err:%d\n", ret);
		return ret;
	}

	pr_info("dpl_ipa connected\n");
	return 0;
}

static void usb_qdss_connect_work(struct work_struct *work)
{
	struct f_qdss *qdss;
	int status;
	unsigned char port_num;
	enum transport_type	dxport;
	enum transport_type     ctrl_xport;
	struct net_device *net;

	qdss = container_of(work, struct f_qdss, connect_w);
	dxport = qdss_ports[qdss->port_num].data_xport;
	ctrl_xport = qdss_ports[qdss->port_num].ctrl_xport;
	port_num = qdss_ports[qdss->port_num].data_xport_num;
	pr_debug("%s: data xport: %s dev: %pK portno: %d\n",
			__func__, xport_to_str(dxport),
			qdss, qdss->port_num);
	if (qdss->port_num >= nr_qdss_ports) {
		pr_err("%s: supporting ports#%u port_id:%u", __func__,
				nr_qdss_ports, qdss->port_num);
		return;
	}
	/* If cable is already removed, discard connect_work */
	if (qdss->usb_connected == 0) {
		pr_debug("%s: discard connect_work\n", __func__);
		cancel_work_sync(&qdss->disconnect_w);
		return;
	}

	pr_debug("usb_qdss_connect_work\n");

	if (ctrl_xport == USB_GADGET_XPORT_QTI) {
		status = gqti_ctrl_connect(&qdss->port, DPL_QTI_CTRL_PORT_NO,
					qdss->data_iface_id, dxport,
					USB_GADGET_DPL);
		if (status) {
			pr_err("%s: gqti_ctrl_connect failed: err:%d\n",
						__func__, status);
			return;
		}
		qdss->port.send_encap_cmd(DPL_QTI_CTRL_PORT_NO, NULL, 0);
	}

	switch (dxport) {
	case USB_GADGET_XPORT_BAM2BAM:
		status = set_qdss_data_connection(
				qdss->cdev->gadget,
				qdss->port.data,
				qdss->port.data->address,
				1);
		if (status) {
			pr_err("set_qdss_data_connection error");
			break;
		}
		if (qdss->ch.notify)
			qdss->ch.notify(qdss->ch.priv,
			USB_QDSS_CONNECT,
			NULL,
			&qdss->ch);

		if (usb_ep_queue(qdss->port.data, qdss->endless_req,
								GFP_ATOMIC)) {
			pr_err("%s: usb_ep_queue error\n", __func__);
			break;
		}
		break;
	case USB_GADGET_XPORT_BAM2BAM_IPA:
		status = qdss_dpl_ipa_connect(qdss->port_num);
		if (status) {
			pr_err("DPL IPA connect failed with %d\n", status);
			return;
		}
		qdss->data_enabled = 1;
		break;
	case USB_GADGET_XPORT_BAM_DMUX:
		qdss_ports[qdss->port_num].bam_dmux_port.gadget =
						qdss->cdev->gadget;
		qdss_ports[qdss->port_num].bam_dmux_port.in =
						qdss->port.data;
		status = gbam_connect(&qdss_ports[qdss->port_num].bam_dmux_port,
				port_num, USB_GADGET_XPORT_BAM_DMUX, 0, 0);
		if (status)
			pr_err("BAM_DMUX connect failed with %d\n", status);
		break;
	case USB_GADGET_XPORT_HSIC:
		pr_debug("usb_qdss_connect_work: HSIC transport\n");
		status = ghsic_data_connect(&qdss->port, port_num);
		if (status) {
			pr_err("%s: ghsic_data_connect failed: err:%d\n",
					__func__, status);
			return;
		}
		break;
	case USB_GADGET_XPORT_ETHER:
		pr_debug("usb_qdss_connect_work: ETHER transport\n");
		net = gether_connect(&qdss_ports[port_num].gether_port);
		if (IS_ERR(net)) {
			pr_err("%s: gether_connect failed: err:%ld\n", __func__,
				PTR_ERR(net));
			return;
		}
		break;
	case USB_GADGET_XPORT_PCIE:
		if (qdss->ch.notify)
			qdss->ch.notify(qdss->ch.priv,
			USB_QDSS_CONNECT,
			NULL,
			&qdss->ch);
		break;
	case USB_GADGET_XPORT_NONE:
		break;
	default:
		pr_err("%s: Un-supported transport: %s\n", __func__,
				xport_to_str(dxport));
	}
}

static int qdss_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_qdss  *qdss = func_to_qdss(f);
	struct usb_gadget *gadget = f->config->cdev->gadget;
	struct usb_qdss_ch *ch = &qdss->ch;
	int ret = 0;
	enum transport_type	dxport;

	dxport = qdss_ports[qdss->port_num].data_xport;

	pr_debug("qdss_set_alt qdss pointer = %pK\n", qdss);

	qdss->gadget = gadget;

	if (alt != 0)
		goto fail1;

	if (gadget->speed != USB_SPEED_SUPER &&
		gadget->speed != USB_SPEED_HIGH) {
		pr_err("qdss_st_alt: qdss supportes HS or SS only\n");
		ret = -EINVAL;
		goto fail1;
	}

	if (intf == qdss->data_iface_id) {
		/* Increment usage count on connect */
		usb_gadget_autopm_get_async(qdss->gadget);

		if (config_ep_by_speed(gadget, f, qdss->port.data)) {
			ret = -EINVAL;
			goto fail;
		}

		/*
		 * In case of data transport is uether, endpoint will be enabled
		 * in gether_connect() too. This will lead to enabling of the
		 * same endpoint twice and will print a warning message.
		 * So for uether data transport disable endpoint only in
		 * gether_connect()
		 */
		if (dxport == USB_GADGET_XPORT_BAM2BAM_IPA ||
				dxport == USB_GADGET_XPORT_BAM_DMUX) {
			qdss->usb_connected = 1;
			usb_qdss_connect_work(&qdss->connect_w);
			return 0;
		}

		if (dxport != USB_GADGET_XPORT_ETHER) {
			ret = usb_ep_enable(qdss->port.data);
			if (ret)
				goto fail;
		}

		qdss->port.data->driver_data = qdss;
		qdss->data_enabled = 1;


	} else if ((intf == qdss->ctrl_iface_id) &&
	(qdss->debug_inface_enabled)) {

		if (config_ep_by_speed(gadget, f, qdss->port.ctrl_in)) {
			ret = -EINVAL;
			goto fail1;
		}

		ret = usb_ep_enable(qdss->port.ctrl_in);
		if (ret)
			goto fail1;

		qdss->port.ctrl_in->driver_data = qdss;
		qdss->ctrl_in_enabled = 1;

		if (config_ep_by_speed(gadget, f, qdss->port.ctrl_out)) {
			ret = -EINVAL;
			goto fail1;
		}


		ret = usb_ep_enable(qdss->port.ctrl_out);
		if (ret)
			goto fail1;

		qdss->port.ctrl_out->driver_data = qdss;
		qdss->ctrl_out_enabled = 1;
	}

	if (qdss->debug_inface_enabled) {
		if (qdss->ctrl_out_enabled && qdss->ctrl_in_enabled &&
			qdss->data_enabled) {
			qdss->usb_connected = 1;
			pr_debug("qdss_set_alt usb_connected INTF enabled\n");
		}
	} else {
		if (qdss->data_enabled) {
			qdss->usb_connected = 1;
			pr_debug("qdss_set_alt usb_connected INTF disabled\n");
		}
	}
	if (qdss->usb_connected && (ch->app_conn ||
		(dxport == USB_GADGET_XPORT_HSIC) ||
		(dxport == USB_GADGET_XPORT_ETHER) ||
		(dxport == USB_GADGET_XPORT_PCIE))) {
		queue_work(qdss->wq, &qdss->connect_w);
	}
	return 0;
fail:
	/* Decrement usage count in case of failure */
	usb_gadget_autopm_put_async(qdss->gadget);
fail1:
	pr_err("qdss_set_alt failed\n");
	qdss_eps_disable(f);
	return ret;
}

static int qdss_bind_config(struct usb_configuration *c, unsigned char portno)
{
	struct f_qdss *qdss;
	int status, found = 0;
	struct usb_qdss_ch *ch;
	unsigned long flags;
	char *name;
	enum transport_type dxport;
	struct usb_function *f;

	dxport = qdss_ports[portno].data_xport;

	pr_debug("qdss_bind_config\n");
	if (portno >= nr_qdss_ports) {
		pr_err("%s: supporting ports#%u port_id:%u", __func__,
				nr_qdss_ports, portno);
		return -ENODEV;
	}
	qdss = qdss_ports[portno].port;

	if (qdss_string_defs[QDSS_DATA_IDX].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		qdss_string_defs[QDSS_DATA_IDX].id = status;
		qdss_data_intf_desc.iInterface = status;
		if (qdss->debug_inface_enabled) {
			status = usb_string_id(c->cdev);
			if (status < 0)
				return status;
			qdss_string_defs[QDSS_CTRL_IDX].id = status;
			qdss_ctrl_intf_desc.iInterface = status;
		}
	}

	if (qdss_ports[portno].data_xport == USB_GADGET_XPORT_BAM2BAM) {
		name = kasprintf(GFP_ATOMIC, "qdss");
	} else if (dxport == USB_GADGET_XPORT_ETHER) {
		pr_debug("qdss_bind_config: USB_GADGET_XPORT_ETHER\n");
		name = kasprintf(GFP_KERNEL, "qdss_dpl");
	} else if (dxport == USB_GADGET_XPORT_PCIE) {
		pr_debug("qdss_bind_config: USB_GADGET_XPORT_PCIE\n");
		name = kasprintf(GFP_KERNEL, "PCIE");
	} else {
		name = kasprintf(GFP_ATOMIC, "qdss%d", portno);
	}

	if (!name)
		return -ENOMEM;

	spin_lock_irqsave(&qdss_lock, flags);

	list_for_each_entry(ch, &usb_qdss_ch_list, list) {
		if (!strcmp(name, ch->name)) {
			found = 1;
			break;
		}
	}
	if (!found) {
		if (!qdss) {
			spin_unlock_irqrestore(&qdss_lock, flags);
			return -ENOMEM;
		}
		spin_unlock_irqrestore(&qdss_lock, flags);
		qdss->wq = create_singlethread_workqueue(name);
		if (!qdss->wq) {
			kfree(name);
			kfree(qdss);
			return -ENOMEM;
		}
		spin_lock_irqsave(&qdss_lock, flags);
		ch = &qdss->ch;
		ch->name = name;
		list_add_tail(&ch->list, &usb_qdss_ch_list);
	} else {
		qdss = container_of(ch, struct f_qdss, ch);
		ch->priv_usb = qdss;
		qdss->debug_inface_enabled =
		qdss_ports[portno].port->debug_inface_enabled;
		if (qdss != qdss_ports[portno].port) {
			kfree(qdss_ports[portno].port);
			qdss_ports[portno].port = qdss;
		}
	}
	spin_unlock_irqrestore(&qdss_lock, flags);
	qdss->cdev = c->cdev;
	qdss->port_num = portno;
	qdss->port.function.name = name;
	qdss->port.function.fs_descriptors = qdss_hs_desc;
	qdss->port.function.hs_descriptors = qdss_hs_desc;
	/*
	 * Populate the gether_port->usb_function which is needed by
	 * u_ether transport when the interface is brought 'down' by calling
	 * eth_stop()
	 */
	if (dxport == USB_GADGET_XPORT_ETHER) {
		f = &qdss_ports[portno].gether_port.func;
		f->fs_descriptors = qdss_hs_data_only_desc;
		f->hs_descriptors = qdss_hs_data_only_desc;
		f->ss_descriptors = qdss_ss_data_only_desc;
	}
	qdss->port.function.strings = qdss_strings;
	qdss->port.function.bind = qdss_bind;
	qdss->port.function.unbind = qdss_unbind;
	qdss->port.function.set_alt = qdss_set_alt;
	qdss->port.function.disable = qdss_disable;
	spin_lock_init(&qdss->lock);
	INIT_LIST_HEAD(&qdss->ctrl_read_pool);
	INIT_LIST_HEAD(&qdss->ctrl_write_pool);
	INIT_LIST_HEAD(&qdss->data_write_pool);
	INIT_WORK(&qdss->connect_w, usb_qdss_connect_work);
	INIT_WORK(&qdss->disconnect_w, usb_qdss_disconnect_work);
	status = usb_add_function(c, &qdss->port.function);
	if (status) {
		pr_err("qdss usb_add_function failed\n");
		ch->priv_usb = NULL;
		kfree(name);
		kfree(qdss);
	}
	if (dxport == USB_GADGET_XPORT_BAM2BAM_IPA ||
			dxport == USB_GADGET_XPORT_BAM2BAM)
		c->cdev->gadget->bam2bam_func_enabled = true;

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

	if (usb_ep_queue(qdss->port.ctrl_out, req, GFP_ATOMIC)) {
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
	if (usb_ep_queue(qdss->port.ctrl_in, req, GFP_ATOMIC)) {
		spin_lock_irqsave(&qdss->lock, flags);
		list_add_tail(&req->list, &qdss->ctrl_write_pool);
		spin_unlock_irqrestore(&qdss->lock, flags);
		pr_err("qdss usb_ep_queue failed\n");
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(usb_qdss_ctrl_write);

int usb_qdss_data_write(struct usb_qdss_ch *ch, struct qdss_request *d_req)
{
	struct f_qdss *qdss = ch->priv_usb;
	unsigned long flags;
	struct usb_request *req = NULL;

	pr_debug("usb_qdss_data_write\n");

	if (!qdss)
		return -ENODEV;

	spin_lock_irqsave(&qdss->lock, flags);

	if (qdss->usb_connected == 0) {
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EIO;
	}

	if (list_empty(&qdss->data_write_pool)) {
		pr_err_ratelimited("error: usb_qdss_data_write list is empty\n");
		spin_unlock_irqrestore(&qdss->lock, flags);
		return -EAGAIN;
	}

	req = list_first_entry(&qdss->data_write_pool, struct usb_request,
			list);
	list_del(&req->list);
	spin_unlock_irqrestore(&qdss->lock, flags);

	req->buf = d_req->buf;
	req->length = d_req->length;
	req->context = d_req;
	if (usb_ep_queue(qdss->port.data, req, GFP_ATOMIC)) {
			spin_lock_irqsave(&qdss->lock, flags);
			list_add_tail(&req->list, &qdss->data_write_pool);
			spin_unlock_irqrestore(&qdss->lock, flags);
			pr_err("qdss usb_ep_queue failed\n");
			return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(usb_qdss_data_write);

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

	spin_lock_irqsave(&qdss_lock, flags);
	/* Check if we already have a channel with this name */
	list_for_each_entry(ch, &usb_qdss_ch_list, list) {
		if (!strcmp(name, ch->name)) {
			found = 1;
			break;
		}
	}

	if (!found) {
		pr_debug("usb_qdss_open: allocation qdss ctx\n");
		qdss = kzalloc(sizeof(*qdss), GFP_ATOMIC);
		if (!qdss) {
			spin_unlock_irqrestore(&qdss_lock, flags);
			return ERR_PTR(-ENOMEM);
		}
		spin_unlock_irqrestore(&qdss_lock, flags);
		qdss->wq = create_singlethread_workqueue(name);
		if (!qdss->wq) {
			kfree(qdss);
			return ERR_PTR(-ENOMEM);
		}
		spin_lock_irqsave(&qdss_lock, flags);
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
	spin_unlock_irqrestore(&qdss_lock, flags);

	/* the case USB cabel was connected befor qdss called  qdss_open*/
	if (qdss->usb_connected == 1)
		queue_work(qdss->wq, &qdss->connect_w);

	return ch;
}
EXPORT_SYMBOL(usb_qdss_open);

void usb_qdss_close(struct usb_qdss_ch *ch)
{
	struct f_qdss *qdss = ch->priv_usb;
	struct usb_gadget *gadget;
	unsigned long flags;
	enum transport_type	dxport;
	int status;

	pr_debug("usb_qdss_close\n");

	if (!qdss)
		return;

	spin_lock_irqsave(&qdss_lock, flags);
	dxport = qdss_ports[qdss->port_num].data_xport;
	ch->priv_usb = NULL;
	if (!qdss->usb_connected || (dxport == USB_GADGET_XPORT_PCIE)) {
		ch->app_conn = 0;
		spin_unlock_irqrestore(&qdss_lock, flags);
		return;
	}

	usb_ep_dequeue(qdss->port.data, qdss->endless_req);
	usb_ep_free_request(qdss->port.data, qdss->endless_req);
	qdss->endless_req = NULL;
	gadget = qdss->cdev->gadget;
	ch->app_conn = 0;
	spin_unlock_irqrestore(&qdss_lock, flags);

	status = uninit_data(qdss->port.data);
	if (status)
		pr_err("%s: uninit_data error\n", __func__);

	status = set_qdss_data_connection(
				gadget,
				qdss->port.data,
				qdss->port.data->address,
				0);
	if (status)
		pr_err("%s:qdss_disconnect error\n", __func__);
	usb_gadget_restart(gadget);
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
		spin_lock_irqsave(&qdss_lock, flags);
		destroy_workqueue(qdss->wq);
		if (!_ch->priv) {
			list_del(&_ch->list);
			kfree(qdss);
		}
		spin_unlock_irqrestore(&qdss_lock, flags);
	}
}

static int qdss_setup(void)
{
	return 0;
}

static int qdss_init_port(const char *ctrl_name, const char *data_name,
			const char *port_name, bool debug_enable)
{
	struct f_qdss			*dev;
	struct qdss_ports		*qdss_port;
	int				ret;
	int				i;

	if (nr_qdss_ports >= NR_QDSS_PORTS) {
		pr_err("%s: Max-%d instances supported\n",
				__func__, NR_QDSS_PORTS);
		return -EINVAL;
	}

	pr_debug("ctrl name = %s data_name %s port_name %s\n",
			ctrl_name, data_name, port_name);

	pr_debug("%s: port#:%d, data port: %s\n",
		__func__, nr_qdss_ports, data_name);

	dev = kzalloc(sizeof(struct f_qdss), GFP_KERNEL);
	if (!dev) {
		pr_err("%s: Unable to allocate qdss device\n", __func__);
		return -ENOMEM;
	}

	dev->port_num = nr_qdss_ports;
	spin_lock_init(&dev->lock);

	qdss_port = &qdss_ports[nr_qdss_ports];
	qdss_port->port = dev;
	qdss_port->port_num = nr_qdss_ports;
	qdss_port->data_xport = str_to_xport(data_name);
	qdss_port->port->debug_inface_enabled = debug_enable;

	if (ctrl_name) {
		qdss_port->ctrl_xport = str_to_xport(ctrl_name);
		pr_debug("%s(): ctrl_name:%s ctrl_xport:%d\n", __func__,
				ctrl_name, qdss_port->ctrl_xport);
		switch (qdss_port->ctrl_xport) {
		case USB_GADGET_XPORT_QTI:
			pr_debug("USB_GADGET_XPORT_QTI is used.\n");
			break;
		default:
			pr_debug("%s(): No ctrl transport.\n", __func__);
		}
	}

	switch (qdss_port->data_xport) {
	case USB_GADGET_XPORT_BAM2BAM:
		qdss_port->data_xport_num = no_data_bam_ports;
		no_data_bam_ports++;
		pr_debug("USB_GADGET_XPORT_BAM2BAM %d\n", no_data_bam_ports);
		break;
	case USB_GADGET_XPORT_BAM2BAM_IPA:
		qdss_port->data_xport_num = no_ipa_ports;
		no_ipa_ports++;
		pr_debug("USB_GADGET_XPORT_BAM2BAM_IPA %d\n", no_ipa_ports);
		break;
	case USB_GADGET_XPORT_HSIC:
	    pr_debug("%s USB_GADGET_XPORT_HSIC\n", __func__);
		ghsic_data_set_port_name(port_name, data_name);
		qdss_port->data_xport_num = data_hsic_ports_no;
		data_hsic_ports_no++;
		break;
	case USB_GADGET_XPORT_BAM_DMUX:
		qdss_port->data_xport_num = no_bam_dmux_ports;
		no_bam_dmux_ports++;
		pr_debug("USB_GADGET_XPORT_BAM_DMUX %u\n", no_bam_dmux_ports);
		break;
	case USB_GADGET_XPORT_ETHER:
		pr_debug("%s USB_GADGET_XPORT_ETHER\n", __func__);
		break;
	case USB_GADGET_XPORT_PCIE:
		pr_debug("%s USB_GADGET_XPORT_PCIE\n", __func__);
		break;
	case USB_GADGET_XPORT_NONE:
		break;
	default:
		pr_err("%s: Un-supported transport: %u\n", __func__,
				qdss_port->data_xport);
		ret = -ENODEV;
		goto fail_probe;
	}
	nr_qdss_ports++;
	return 0;

fail_probe:
	for (i = 0; i < nr_qdss_ports; i++)
		kfree(qdss_ports[i].port);

	nr_qdss_ports = 0;
	no_data_bam_ports = 0;
	data_hsic_ports_no = 0;
	no_ipa_ports = 0;
	no_bam_dmux_ports = 0;
	return ret;
}

static int qdss_gport_setup(void)
{
	int	port_idx;
	int	i;

	pr_debug("%s: bam ports: %u data hsic ports: %u ipa_ports:%u bam_dmux_port:%u nr_qdss_ports:%u\n",
			__func__, no_data_bam_ports, data_hsic_ports_no,
			no_ipa_ports, no_bam_dmux_ports, nr_qdss_ports);

	if (data_hsic_ports_no) {
		pr_debug("%s: go to setup hsic data\n", __func__);
		port_idx = ghsic_data_setup(data_hsic_ports_no,
				USB_GADGET_QDSS);
		if (port_idx < 0)
			return port_idx;
		for (i = 0; i < nr_qdss_ports; i++) {
			if (qdss_ports[i].data_xport ==
					USB_GADGET_XPORT_HSIC) {
				qdss_ports[i].data_xport_num = port_idx;
				pr_debug("%s: qdss data_xport_num = %d\n",
					__func__, qdss_ports[i].data_xport_num);
				port_idx++;
			}
		}
	}

	if (no_ipa_ports) {
		pr_debug("Inside initializaing ipa data port\n");
		port_idx = ipa_data_setup(no_ipa_ports);
		if (port_idx < 0) {
			pr_err("%s(): error with initializing IPA data setup\n",
								__func__);
			return port_idx;
		}

		for (i = 0; i < no_ipa_ports; i++) {
			if (qdss_ports[i].data_xport ==
					USB_GADGET_XPORT_BAM2BAM_IPA) {
				qdss_ports[i].data_xport_num = port_idx;
				pr_debug("%s: DPL data_xport_num = %d\n",
					__func__, qdss_ports[i].data_xport_num);
				port_idx++;
			}
		}
	}

	if (no_bam_dmux_ports) {
		port_idx = gbam_setup(no_bam_dmux_ports);
		if (port_idx < 0) {
			pr_err("%s(): gbam_setup failed with %d\n",
					__func__, port_idx);
			return port_idx;
		}

		for (i = 0; i < no_bam_dmux_ports; i++) {
			if (qdss_ports[i].data_xport ==
					USB_GADGET_XPORT_BAM_DMUX) {
				qdss_ports[i].data_xport_num = port_idx;
				pr_debug("%s: BAM-DMUX data_xport_num = %d\n",
					__func__, qdss_ports[i].data_xport_num);
				port_idx++;
			}
		}
	}

	return 0;
}
