/*
 * f_charger.c -- USB HID function driver
 *
 * Copyright (C) 2010 Fabien Chouteau <fabien.chouteau@barco.com>
 * Copyright (C) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2 license
 * terms, and distributes only under these terms.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/hid.h>

struct f_charger {
	struct usb_ep *in_ep;
	struct usb_function func;
};

static inline struct f_charger *func_to_charger(struct usb_function *f)
{
	return container_of(f, struct f_charger, func);
}

static const uint8_t the_report_descriptor[] = {
	0x06, 0xA0, 0xFF, 0x09, 0xA5, 0xA1, 0x01, 0x09,
	0xA6, 0x09, 0xA7, 0x15, 0x80, 0x25, 0x7F, 0x75,
	0x08, 0x95, 0x02, 0x81, 0x02, 0x09, 0xA9, 0x15,
	0x80, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x02, 0x91,
	0x02, 0xC0,
};


static struct usb_interface_descriptor charger_interface_desc = {
	.bLength		= sizeof(charger_interface_desc),
	.bDescriptorType	= USB_DT_INTERFACE,
	/* .bInterfaceNumber	= DYNAMIC */
	.bAlternateSetting	= 0,
	.bNumEndpoints		= 1,
	.bInterfaceClass	= USB_CLASS_HID,
	.bInterfaceSubClass	= 0,
	.bInterfaceProtocol	= 0,
	/* .iInterface		= DYNAMIC */
};

static struct hid_descriptor charger_hid_desc = {
	.bLength			= sizeof(charger_hid_desc),
	.bDescriptorType		= 0x21,
	.bcdHID				= 0x0111,
	.bCountryCode			= 0x00,
	.bNumDescriptors		= 0x1,
	.desc[0].bDescriptorType	= 0x22,
	.desc[0].wDescriptorLength = sizeof(the_report_descriptor),
};

/* Super-Speed Support */

static struct usb_endpoint_descriptor charger_ss_in_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize	= 1 ,
	.bInterval		= 16,
};

static struct usb_ss_ep_comp_descriptor charger_ss_intr_comp_desc = {
	.bLength		= sizeof(charger_ss_intr_comp_desc),
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst		= 0, */
	/* .bmAttributes	= 0, */
};

static struct usb_descriptor_header *charger_ss_descriptors[] = {
	(struct usb_descriptor_header *)&charger_interface_desc,
	(struct usb_descriptor_header *)&charger_hid_desc,
	(struct usb_descriptor_header *)&charger_ss_in_ep_desc,
	(struct usb_descriptor_header *)&charger_ss_intr_comp_desc,
	NULL,
};

/* High-Speed Support */
static struct usb_endpoint_descriptor charger_hs_in_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize	= 1 ,
	.bInterval		= 16,
};

static struct usb_descriptor_header *charger_hs_descriptors[] = {
	(struct usb_descriptor_header *)&charger_interface_desc,
	(struct usb_descriptor_header *)&charger_hid_desc,
	(struct usb_descriptor_header *)&charger_hs_in_ep_desc,
	NULL,
};

/* Full-Speed Support */

static struct usb_endpoint_descriptor charger_fs_in_ep_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize	= 1,
	.bInterval		= 16,
};

static struct usb_descriptor_header *charger_fs_descriptors[] = {
	(struct usb_descriptor_header *)&charger_interface_desc,
	(struct usb_descriptor_header *)&charger_hid_desc,
	(struct usb_descriptor_header *)&charger_fs_in_ep_desc,
	NULL,
};

/*       Strings          */

#define CT_FUNC_HID_IDX	0

static struct usb_string ct_func_string_defs[] = {
	[CT_FUNC_HID_IDX].s	= "HID Interface",
	{},			/* end of list */
};

static struct usb_gadget_strings ct_func_string_table = {
	.language	= 0x0409,	/* en-US */
	.strings	= ct_func_string_defs,
};

static struct usb_gadget_strings *ct_func_strings[] = {
	&ct_func_string_table,
	NULL,
};


static void charger_disable(struct usb_function *f)
{
	struct f_charger	*charger = func_to_charger(f);
	usb_ep_disable(charger->in_ep);
	charger->in_ep->driver_data = NULL;
}

static int hid_setup(struct usb_function *f,
		const struct usb_ctrlrequest *ctrl)
{
	struct usb_composite_dev	*cdev = f->config->cdev;
	struct usb_request		*req  = cdev->req;
	int status = 0;
	__u16 value, length;
	bool resp_stall = false;

	value	= le16_to_cpu(ctrl->wValue);
	length	= le16_to_cpu(ctrl->wLength);

	VDBG(cdev,
		"hid_setup crtl_request : bRequestType:0x%x bRequest:0x%x Value:0x%x\n",
		ctrl->bRequestType, ctrl->bRequest, value);

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_GET_REPORT):
		VDBG(cdev, "get_report\n");

		/* send an empty report */
		length = min_t(unsigned, length,
				charger_hid_desc.desc[0].wDescriptorLength);
		memset(req->buf, 0x0, length);
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_GET_PROTOCOL):
		VDBG(cdev, "get_protocol\n");
		resp_stall = true;
		break;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_SET_REPORT):
		VDBG(cdev, "set_report | wLenght=%d\n", ctrl->wLength);
		resp_stall = true;
		break;

	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8
		  | HID_REQ_SET_PROTOCOL):
		VDBG(cdev, "set_protocol\n");
		resp_stall = true;
		break;

	case ((USB_DIR_IN | USB_TYPE_STANDARD | USB_RECIP_INTERFACE) << 8
		  | USB_REQ_GET_DESCRIPTOR):
		switch (value >> 8) {

		case HID_DT_HID:
			VDBG(cdev, "USB_REQ_GET_DESCRIPTOR: HID\n");
			length = min_t(unsigned short, length,
						   charger_hid_desc.bLength);
			memcpy(req->buf, &charger_hid_desc, length);
			break;

		case HID_DT_REPORT:
			VDBG(cdev, "USB_REQ_GET_DESCRIPTOR: REPORT\n");
			length = min_t(unsigned short, length,
				charger_hid_desc.desc[0].wDescriptorLength);
			memcpy(req->buf, &the_report_descriptor, length);
			break;

		default:
			VDBG(cdev, "Unknown descriptor request 0x%x\n",
				 value >> 8);
			resp_stall = true;
			break;
		}
		break;

	default:
		VDBG(cdev, "Unknown request 0x%x\n",
			 ctrl->bRequest);
		resp_stall = true;
		break;
	}

	if (resp_stall) {
		ERROR(cdev, "usb ep stall\n");
		return -EOPNOTSUPP;
	}

	req->zero = 0;
	req->length = length;
	status = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
	if (status < 0)
		ERROR(cdev, "usb_ep_queue error on ep0 %d\n", value);
	return status;
}

static int charger_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct usb_composite_dev		*cdev = f->config->cdev;
	struct f_charger		*charger = func_to_charger(f);
	int status = 0;

	VDBG(cdev, "charger_set_alt intf:%d alt:%d\n", intf, alt);

	if (charger->in_ep != NULL) {
		/* restart endpoint */
		if (charger->in_ep->driver_data != NULL)
			usb_ep_disable(charger->in_ep);

		status = config_ep_by_speed(f->config->cdev->gadget, f,
					    charger->in_ep);
		if (status) {
			charger->in_ep->desc = NULL;
			ERROR(cdev, "config_ep_by_speed FAILED!\n");
			goto fail;
		}
		status = usb_ep_enable(charger->in_ep);
		if (status < 0) {
			ERROR(cdev, "Enable IN endpoint FAILED!\n");
			goto fail;
		}
		charger->in_ep->driver_data = charger;
	}
fail:
	return status;
}



static int  charger_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_ep		*ep;
	struct f_charger		*charger = func_to_charger(f);
	int			status;

	/* allocate instance-specific interface IDs, and patch descriptors */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;

	charger_interface_desc.bInterfaceNumber = status;

	/* allocate instance-specific endpoints */
	status = -ENODEV;
	ep = usb_ep_autoconfig(c->cdev->gadget, &charger_fs_in_ep_desc);
	if (!ep)
		goto fail;
	ep->driver_data = c->cdev;	/* claim */
	charger->in_ep = ep;

	/* copy descriptors */
	f->fs_descriptors = usb_copy_descriptors(charger_fs_descriptors);
	if (!f->fs_descriptors)
		goto fail;

	if (gadget_is_dualspeed(c->cdev->gadget)) {
		charger_hs_in_ep_desc.bEndpointAddress =
				charger_fs_in_ep_desc.bEndpointAddress;

		f->hs_descriptors =
			usb_copy_descriptors(charger_hs_descriptors);
		if (!f->hs_descriptors)
			goto fail;
	}

	if (gadget_is_superspeed(c->cdev->gadget)) {
		charger_ss_in_ep_desc.bEndpointAddress =
				charger_fs_in_ep_desc.bEndpointAddress;

		f->ss_descriptors =
			usb_copy_descriptors(charger_ss_descriptors);
		if (!f->ss_descriptors)
			goto fail;
	}

	return 0;

fail:
	ERROR(f->config->cdev, "charger_bind FAILED\n");

	if (f->ss_descriptors)
		usb_free_descriptors(f->ss_descriptors);
	if (f->hs_descriptors)
		usb_free_descriptors(f->hs_descriptors);
	if (f->fs_descriptors)
		usb_free_descriptors(f->fs_descriptors);

	return status;
}

static void charger_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_charger *charger = func_to_charger(f);

	/* disable/free request and end point */
	usb_ep_disable(charger->in_ep);

	/* free descriptors copies */
	if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);

	usb_free_descriptors(f->fs_descriptors);

	kfree(charger);
}

static int  charger_bind_config(struct usb_configuration *c)
{
	struct f_charger *charger;
	int status;

	/* allocate and initialize one new instance */
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->func.name    = "charging";
	charger->func.strings = ct_func_strings;
	charger->func.bind    = charger_bind;
	charger->func.unbind  = charger_unbind;
	charger->func.set_alt = charger_set_alt;
	charger->func.disable = charger_disable;
	charger->func.setup   = hid_setup;

	status = usb_add_function(c, &charger->func);
	if (status)
		kfree(charger);

	return status;
}
