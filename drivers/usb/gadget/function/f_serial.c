/*
 * f_serial.c - generic USB serial function driver
 *
 * Copyright (C) 2003 Al Borchers (alborchers@steinerpoint.com)
 * Copyright (C) 2008 by David Brownell
 * Copyright (C) 2008 by Nokia Corporation
 *
 * This software is distributed under the terms of the GNU General
 * Public License ("GPL") as published by the Free Software Foundation,
 * either version 2 of that License or (at your option) any later version.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>

#include "u_serial.h"


/*
 * This function packages a simple "generic serial" port with no real
 * control mechanisms, just raw data transfer over two bulk endpoints.
 *
 * Because it's not standardized, this isn't as interoperable as the
 * CDC ACM driver.  However, for many purposes it's just as functional
 * if you can arrange appropriate host side drivers.
 */

struct f_gser {
	struct gserial			port;
	u8				data_id;
	u8				port_num;
	spinlock_t			lock;

	struct usb_ep			*notify;
	struct usb_request		*notify_req;

	u8				online;
	u8				pending;
	struct usb_cdc_line_coding	port_line_coding;

	/* SetControlLineState request */
	u16				port_handshake_bits;
#define ACM_CTRL_RTS	(1 << 1)	/* unused with full duplex */
#define ACM_CTRL_DTR	(1 << 0)	/* host is ready for data r/w */

	/* SerialState notification */
	u16				serial_state;
#define ACM_CTRL_OVERRUN	(1 << 6)
#define ACM_CTRL_PARITY		(1 << 5)
#define ACM_CTRL_FRAMING	(1 << 4)
#define ACM_CTRL_RI		(1 << 3)
#define ACM_CTRL_BRK		(1 << 2)
#define ACM_CTRL_DSR		(1 << 1)
#define ACM_CTRL_DCD		(1 << 0)
};

static inline struct f_gser *port_to_gser(struct gserial *p)
{
	return container_of(p, struct f_gser, port);
}

static inline struct f_gser *func_to_gser(struct usb_function *f)
{
	return container_of(f, struct f_gser, port.func);
}

#define GS_LOG2_NOTIFY_INTERVAL		5	/* 1 << 5 == 32 msec */
#define GS_NOTIFY_MAXPACKET		10	/* notification + 2 bytes */
/*-------------------------------------------------------------------------*/

/* interface descriptor: */

static struct usb_interface_descriptor gser_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	/* .bInterfaceNumber = DYNAMIC */
	.bNumEndpoints =	3,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

static struct usb_cdc_header_desc gser_header_desc  = {
	.bLength =		sizeof(gser_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_call_mgmt_descriptor
gser_call_mgmt_descriptor  = {
	.bLength =		sizeof(gser_call_mgmt_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_CALL_MANAGEMENT_TYPE,
	.bmCapabilities =	0,
	/* .bDataInterface = DYNAMIC */
};

static struct usb_cdc_acm_descriptor gser_descriptor  = {
	.bLength =		sizeof(gser_descriptor),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ACM_TYPE,
	.bmCapabilities =	USB_CDC_CAP_LINE,
};

static struct usb_cdc_union_desc gser_union_desc  = {
	.bLength =		sizeof(gser_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor gser_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		1 << GS_LOG2_NOTIFY_INTERVAL,
};

static struct usb_endpoint_descriptor gser_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor gser_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *gser_fs_function[] = {
	(struct usb_descriptor_header *) &gser_interface_desc,
	(struct usb_descriptor_header *) &gser_header_desc,
	(struct usb_descriptor_header *) &gser_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &gser_descriptor,
	(struct usb_descriptor_header *) &gser_union_desc,
	(struct usb_descriptor_header *) &gser_fs_notify_desc,
	(struct usb_descriptor_header *) &gser_fs_in_desc,
	(struct usb_descriptor_header *) &gser_fs_out_desc,
	NULL,
};

/* high speed support: */
static struct usb_endpoint_descriptor gser_hs_notify_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		GS_LOG2_NOTIFY_INTERVAL+4,
};

static struct usb_endpoint_descriptor gser_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor gser_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *gser_hs_function[] = {
	(struct usb_descriptor_header *) &gser_interface_desc,
	(struct usb_descriptor_header *) &gser_header_desc,
	(struct usb_descriptor_header *) &gser_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &gser_descriptor,
	(struct usb_descriptor_header *) &gser_union_desc,
	(struct usb_descriptor_header *) &gser_hs_notify_desc,
	(struct usb_descriptor_header *) &gser_hs_in_desc,
	(struct usb_descriptor_header *) &gser_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor gser_ss_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_endpoint_descriptor gser_ss_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor gser_ss_bulk_comp_desc = {
	.bLength =              sizeof gser_ss_bulk_comp_desc,
	.bDescriptorType =      USB_DT_SS_ENDPOINT_COMP,
};

static struct usb_endpoint_descriptor gser_ss_notify_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
	.bInterval =		GS_LOG2_NOTIFY_INTERVAL+4,
};

static struct usb_ss_ep_comp_descriptor gser_ss_notify_comp_desc = {
	.bLength =		sizeof(gser_ss_notify_comp_desc),
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 2 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
	.wBytesPerInterval =	cpu_to_le16(GS_NOTIFY_MAXPACKET),
};

static struct usb_descriptor_header *gser_ss_function[] = {
	(struct usb_descriptor_header *) &gser_interface_desc,
	(struct usb_descriptor_header *) &gser_header_desc,
	(struct usb_descriptor_header *) &gser_call_mgmt_descriptor,
	(struct usb_descriptor_header *) &gser_descriptor,
	(struct usb_descriptor_header *) &gser_union_desc,
	(struct usb_descriptor_header *) &gser_ss_notify_desc,
	(struct usb_descriptor_header *) &gser_ss_notify_comp_desc,
	(struct usb_descriptor_header *) &gser_ss_in_desc,
	(struct usb_descriptor_header *) &gser_ss_bulk_comp_desc,
	(struct usb_descriptor_header *) &gser_ss_out_desc,
	(struct usb_descriptor_header *) &gser_ss_bulk_comp_desc,
	NULL,
};

/* string descriptors: */

static struct usb_string gser_string_defs[] = {
	[0].s = "DUN over Serial",
	{  } /* end of list */
};

static struct usb_gadget_strings gser_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		gser_string_defs,
};

static struct usb_gadget_strings *gser_strings[] = {
	&gser_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/
static void gser_complete_set_line_coding(struct usb_ep *ep,
					struct usb_request *req)
{
	struct f_gser            *gser = ep->driver_data;
	struct usb_composite_dev *cdev = gser->port.func.config->cdev;

	if (req->status != 0) {
		dev_dbg(&cdev->gadget->dev, "gser ttyGS%d completion, err %d\n",
				gser->port_num, req->status);
		return;
	}

	/* normal completion */
	if (req->actual != sizeof(gser->port_line_coding)) {
		dev_dbg(&cdev->gadget->dev, "gser ttyGS%d short resp, len %d\n",
				gser->port_num, req->actual);
		usb_ep_set_halt(ep);
	} else {
		struct usb_cdc_line_coding *value = req->buf;

		gser->port_line_coding = *value;
	}
}

static int
gser_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_gser            *gser = func_to_gser(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request       *req = cdev->req;
	int                      value = -EOPNOTSUPP;
	u16                      w_index = le16_to_cpu(ctrl->wIndex);
	u16                      w_value = le16_to_cpu(ctrl->wValue);
	u16                      w_length = le16_to_cpu(ctrl->wLength);


	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {

	/* SET_LINE_CODING ... just read and save what the host sends */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_LINE_CODING:
		if (w_length != sizeof(struct usb_cdc_line_coding))
			goto invalid;

		value = w_length;
		cdev->gadget->ep0->driver_data = gser;
		req->complete = gser_complete_set_line_coding;
		break;

	/* GET_LINE_CODING ... return what host sent, or initial value */
	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_GET_LINE_CODING:
		value = min_t(unsigned int, w_length,
				sizeof(struct usb_cdc_line_coding));
		memcpy(req->buf, &gser->port_line_coding, value);
		break;

	/* SET_CONTROL_LINE_STATE ... save what the host sent */
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:

		value = 0;
		gser->port_handshake_bits = w_value;
		pr_debug("%s: USB_CDC_REQ_SET_CONTROL_LINE_STATE: DTR:%d RST:%d\n",
			__func__, w_value & ACM_CTRL_DTR ? 1 : 0,
			w_value & ACM_CTRL_RTS ? 1 : 0);

		if (gser->port.notify_modem)
			gser->port.notify_modem(&gser->port, 0, w_value);

		break;

	default:
invalid:
		dev_dbg(&cdev->gadget->dev,
			"invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		dev_dbg(&cdev->gadget->dev,
			"gser ttyGS%d req%02x.%02x v%04x i%04x l%d\n",
			gser->port_num, ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			ERROR(cdev, "gser response on ttyGS%d, err %d\n",
					gser->port_num, value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}

static int gser_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_gser		*gser = func_to_gser(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	int rc = 0;

	/* we know alt == 0, so this is an activation or a reset */
	if (gser->notify->driver_data) {
		dev_dbg(&cdev->gadget->dev,
			"reset generic ctl ttyGS%d\n", gser->port_num);
		usb_ep_disable(gser->notify);
	}

	if (!gser->notify->desc) {
		if (config_ep_by_speed(cdev->gadget, f, gser->notify)) {
			gser->notify->desc = NULL;
			return -EINVAL;
		}
	}

	rc = usb_ep_enable(gser->notify);
	if (rc) {
		ERROR(cdev, "can't enable %s, result %d\n",
			gser->notify->name, rc);
		return rc;
	}
	gser->notify->driver_data = gser;

	if (gser->port.in->enabled) {
		dev_dbg(&cdev->gadget->dev,
			"reset generic ttyGS%d\n", gser->port_num);
		gserial_disconnect(&gser->port);
	}
	if (!gser->port.in->desc || !gser->port.out->desc) {
		dev_dbg(&cdev->gadget->dev,
			"activate generic ttyGS%d\n", gser->port_num);
		if (config_ep_by_speed(cdev->gadget, f, gser->port.in) ||
		    config_ep_by_speed(cdev->gadget, f, gser->port.out)) {
			gser->port.in->desc = NULL;
			gser->port.out->desc = NULL;
			return -EINVAL;
		}
	}
	gserial_connect(&gser->port, gser->port_num);
	gser->online = 1;

	return rc;
}

static void gser_disable(struct usb_function *f)
{
	struct f_gser	*gser = func_to_gser(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	dev_dbg(&cdev->gadget->dev,
		"generic ttyGS%d deactivated\n", gser->port_num);
	gserial_disconnect(&gser->port);
	usb_ep_disable(gser->notify);
	gser->notify->driver_data = NULL;
	gser->online = 0;
}

static int gser_notify(struct f_gser *gser, u8 type, u16 value,
		void *data, unsigned int length)
{
	struct usb_ep			*ep = gser->notify;
	struct usb_request		*req;
	struct usb_cdc_notification	*notify;
	const unsigned int		len = sizeof(*notify) + length;
	void                            *buf;
	int                             status;
	struct usb_composite_dev *cdev = gser->port.func.config->cdev;

	req = gser->notify_req;
	gser->notify_req = NULL;
	gser->pending = false;

	req->length = len;
	notify = req->buf;
	buf = notify + 1;

	notify->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	notify->bNotificationType = type;
	notify->wValue = cpu_to_le16(value);
	notify->wIndex = cpu_to_le16(gser->data_id);
	notify->wLength = cpu_to_le16(length);
	memcpy(buf, data, length);

	status = usb_ep_queue(ep, req, GFP_ATOMIC);
	if (status < 0) {
		ERROR(cdev, "gser ttyGS%d can't notify serial state, %d\n",
				gser->port_num, status);
		gser->notify_req = req;
	}

	return status;
}

static int gser_notify_serial_state(struct f_gser *gser)
{
	int		status;
	unsigned long	flags;
	struct usb_composite_dev *cdev = gser->port.func.config->cdev;

	spin_lock_irqsave(&gser->lock, flags);
	if (gser->notify_req) {
		DBG(cdev, "gser ttyGS%d serial state %04x\n",
				gser->port_num, gser->serial_state);
		status = gser_notify(gser, USB_CDC_NOTIFY_SERIAL_STATE,
				0, &gser->serial_state,
				sizeof(gser->serial_state));
	} else {
		gser->pending = true;
		status = 0;
	}

	spin_unlock_irqrestore(&gser->lock, flags);
	return status;
}

static void gser_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_gser *gser = req->context;
	u8            doit = false;
	unsigned long flags;

	/* on this call path we do NOT hold the port spinlock,
	 * which is why ACM needs its own spinlock
	 */

	spin_lock_irqsave(&gser->lock, flags);
	if (req->status != -ESHUTDOWN)
		doit = gser->pending;

	gser->notify_req = req;
	spin_unlock_irqrestore(&gser->lock, flags);

	if (doit && gser->online)
		gser_notify_serial_state(gser);
}

static void gser_connect(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	gser->serial_state |= ACM_CTRL_DSR | ACM_CTRL_DCD;
	gser_notify_serial_state(gser);
}

static unsigned int gser_get_dtr(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	if (gser->port_handshake_bits & ACM_CTRL_DTR)
		return 1;
	else
		return 0;
}

static unsigned int gser_get_rts(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	if (gser->port_handshake_bits & ACM_CTRL_RTS)
		return 1;
	else
		return 0;
}

static unsigned int gser_send_carrier_detect(struct gserial *port,
	unsigned int yes)
{
	u16	state;
	struct f_gser *gser = port_to_gser(port);

	state = gser->serial_state;
	state &= ~ACM_CTRL_DCD;
	if (yes)
		state |= ACM_CTRL_DCD;

	gser->serial_state = state;
	return gser_notify_serial_state(gser);
}

static unsigned int gser_send_ring_indicator(struct gserial *port,
	unsigned int yes)
{
	u16	state;
	struct f_gser *gser = port_to_gser(port);

	state = gser->serial_state;
	state &= ~ACM_CTRL_RI;
	if (yes)
		state |= ACM_CTRL_RI;

	gser->serial_state = state;
	return gser_notify_serial_state(gser);
}

static void gser_disconnect(struct gserial *port)
{
	struct f_gser *gser = port_to_gser(port);

	gser->serial_state &= ~(ACM_CTRL_DSR | ACM_CTRL_DCD);
	gser_notify_serial_state(gser);
}

static int gser_send_break(struct gserial *port, int duration)
{
	u16	state;
	struct f_gser *gser = port_to_gser(port);

	state = gser->serial_state;
	state &= ~ACM_CTRL_BRK;
	if (duration)
		state |= ACM_CTRL_BRK;

	gser->serial_state = state;
	return gser_notify_serial_state(gser);
}

static int gser_send_modem_ctrl_bits(struct gserial *port, int ctrl_bits)
{
	struct f_gser *gser = port_to_gser(port);

	gser->serial_state = ctrl_bits;

	return gser_notify_serial_state(gser);
}

/*-------------------------------------------------------------------------*/

/* serial function driver setup/binding */

static int gser_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_gser		*gser = func_to_gser(f);
	int			status;
	struct usb_ep		*ep;

	/* REVISIT might want instance-specific strings to help
	 * distinguish instances ...
	 */

	/* maybe allocate device-global string ID */
	if (gser_string_defs[0].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		gser_string_defs[0].id = status;
	}

	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;
	gser->data_id = status;
	gser_interface_desc.bInterfaceNumber = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &gser_fs_in_desc);
	if (!ep)
		goto fail;
	gser->port.in = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &gser_fs_out_desc);
	if (!ep)
		goto fail;
	gser->port.out = ep;

	ep = usb_ep_autoconfig(cdev->gadget, &gser_fs_notify_desc);
	if (!ep)
		goto fail;
	gser->notify = ep;
	ep->driver_data = cdev;	/* claim */
	/* allocate notification */
	gser->notify_req = gs_alloc_req(ep,
			sizeof(struct usb_cdc_notification) + 2,
			cdev->gadget->extra_buf_alloc, GFP_KERNEL);
	if (!gser->notify_req)
		goto fail;

	gser->notify_req->complete = gser_notify_complete;
	gser->notify_req->context = gser;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	gser_hs_in_desc.bEndpointAddress = gser_fs_in_desc.bEndpointAddress;
	gser_hs_out_desc.bEndpointAddress = gser_fs_out_desc.bEndpointAddress;

	gser_ss_in_desc.bEndpointAddress = gser_fs_in_desc.bEndpointAddress;
	gser_ss_out_desc.bEndpointAddress = gser_fs_out_desc.bEndpointAddress;

	if (gadget_is_dualspeed(c->cdev->gadget)) {
		gser_hs_notify_desc.bEndpointAddress =
				gser_fs_notify_desc.bEndpointAddress;
	}
	if (gadget_is_superspeed(c->cdev->gadget)) {
		gser_ss_notify_desc.bEndpointAddress =
				gser_fs_notify_desc.bEndpointAddress;
	}

	status = usb_assign_descriptors(f, gser_fs_function, gser_hs_function,
			gser_ss_function, NULL);
	if (status)
		goto fail;
	dev_dbg(&cdev->gadget->dev, "generic ttyGS%d: %s speed IN/%s OUT/%s\n",
		gser->port_num,
		gadget_is_superspeed(c->cdev->gadget) ? "super" :
		gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
		gser->port.in->name, gser->port.out->name);
	return 0;

fail:
	if (gser->notify_req)
		gs_free_req(gser->notify, gser->notify_req);

	/* we might as well release our claims on endpoints */
	if (gser->notify)
		gser->notify->driver_data = NULL;
	/* we might as well release our claims on endpoints */
	if (gser->port.out)
		gser->port.out->driver_data = NULL;
	if (gser->port.in)
		gser->port.in->driver_data = NULL;

	ERROR(cdev, "%s: can't bind, err %d\n", f->name, status);

	return status;
}

static inline struct f_serial_opts *to_f_serial_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_serial_opts,
			    func_inst.group);
}

static void serial_attr_release(struct config_item *item)
{
	struct f_serial_opts *opts = to_f_serial_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static struct configfs_item_operations serial_item_ops = {
	.release	= serial_attr_release,
};

static ssize_t f_serial_port_num_show(struct config_item *item, char *page)
{
	return sprintf(page, "%u\n", to_f_serial_opts(item)->port_num);
}

CONFIGFS_ATTR_RO(f_serial_, port_num);

static struct configfs_attribute *acm_attrs[] = {
	&f_serial_attr_port_num,
	NULL,
};

static struct config_item_type serial_func_type = {
	.ct_item_ops	= &serial_item_ops,
	.ct_attrs	= acm_attrs,
	.ct_owner	= THIS_MODULE,
};

static void gser_free_inst(struct usb_function_instance *f)
{
	struct f_serial_opts *opts;

	opts = container_of(f, struct f_serial_opts, func_inst);
	gserial_free_line(opts->port_num);
	kfree(opts);
}

static struct usb_function_instance *gser_alloc_inst(void)
{
	struct f_serial_opts *opts;
	int ret;

	opts = kzalloc(sizeof(*opts), GFP_KERNEL);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	opts->func_inst.free_func_inst = gser_free_inst;
	ret = gserial_alloc_line(&opts->port_num);
	if (ret) {
		kfree(opts);
		return ERR_PTR(ret);
	}
	config_group_init_type_name(&opts->func_inst.group, "",
				    &serial_func_type);

	return &opts->func_inst;
}

static void gser_free(struct usb_function *f)
{
	struct f_gser *serial;

	serial = func_to_gser(f);
	kfree(serial);
}

static void gser_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_gser *gser = func_to_gser(f);

	usb_free_all_descriptors(f);
	gs_free_req(gser->notify, gser->notify_req);
}

static struct usb_function *gser_alloc(struct usb_function_instance *fi)
{
	struct f_gser	*gser;
	struct f_serial_opts *opts;

	/* allocate and initialize one new instance */
	gser = kzalloc(sizeof(*gser), GFP_KERNEL);
	if (!gser)
		return ERR_PTR(-ENOMEM);

	opts = container_of(fi, struct f_serial_opts, func_inst);

	spin_lock_init(&gser->lock);
	gser->port_num = opts->port_num;

	gser->port.func.name = "gser";
	gser->port.func.strings = gser_strings;
	gser->port.func.bind = gser_bind;
	gser->port.func.unbind = gser_unbind;
	gser->port.func.set_alt = gser_set_alt;
	gser->port.func.disable = gser_disable;
	gser->port.func.free_func = gser_free;
	gser->port.func.setup = gser_setup;
	gser->port.connect = gser_connect;
	gser->port.get_dtr = gser_get_dtr;
	gser->port.get_rts = gser_get_rts;
	gser->port.send_carrier_detect = gser_send_carrier_detect;
	gser->port.send_ring_indicator = gser_send_ring_indicator;
	gser->port.send_modem_ctrl_bits = gser_send_modem_ctrl_bits;
	gser->port.disconnect = gser_disconnect;
	gser->port.send_break = gser_send_break;

	return &gser->port.func;
}

DECLARE_USB_FUNCTION_INIT(gser, gser_alloc_inst, gser_alloc);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Al Borchers");
MODULE_AUTHOR("David Brownell");
