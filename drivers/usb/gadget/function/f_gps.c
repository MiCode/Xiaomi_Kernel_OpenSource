/*
 * Copyright (c) 2011-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/spinlock.h>

#include "usb_gadget_xport.h"
#include "u_rmnet.h"
#include "gadget_chips.h"

#define GPS_NOTIFY_INTERVAL	5
#define GPS_MAX_NOTIFY_SIZE	64


#define ACM_CTRL_DTR	(1 << 0)

/* TODO: use separate structures for data and
 * control paths
 */
struct f_gps {
	struct grmnet			port;
	u8				port_num;
	int				ifc_id;
	atomic_t			online;
	atomic_t			ctrl_online;
	struct usb_composite_dev	*cdev;

	spinlock_t			lock;

	/* usb eps */
	struct usb_ep			*notify;
	struct usb_request		*notify_req;

	/* control info */
	struct list_head		cpkt_resp_q;
	atomic_t			notify_count;
	unsigned long			cpkts_len;

	/* remote wakeup info */
	bool				is_suspended;
	bool				is_rw_allowed;
};

static struct gps_ports {
	enum transport_type		ctrl_xport;
	struct f_gps			*port;
} gps_port;

static struct usb_interface_descriptor gps_interface_desc = {
	.bLength =		USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass =	USB_CLASS_VENDOR_SPEC,
	.bInterfaceProtocol =	USB_CLASS_VENDOR_SPEC,
	/* .iInterface = DYNAMIC */
};

/* Full speed support */
static struct usb_endpoint_descriptor gps_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(GPS_MAX_NOTIFY_SIZE),
	.bInterval =		1 << GPS_NOTIFY_INTERVAL,
};

static struct usb_descriptor_header *gps_fs_function[] = {
	(struct usb_descriptor_header *) &gps_interface_desc,
	(struct usb_descriptor_header *) &gps_fs_notify_desc,
	NULL,
};

/* High speed support */
static struct usb_endpoint_descriptor gps_hs_notify_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(GPS_MAX_NOTIFY_SIZE),
	.bInterval =		GPS_NOTIFY_INTERVAL + 4,
};

static struct usb_descriptor_header *gps_hs_function[] = {
	(struct usb_descriptor_header *) &gps_interface_desc,
	(struct usb_descriptor_header *) &gps_hs_notify_desc,
	NULL,
};

/* Super speed support */
static struct usb_endpoint_descriptor gps_ss_notify_desc  = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,
	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	__constant_cpu_to_le16(GPS_MAX_NOTIFY_SIZE),
	.bInterval =		GPS_NOTIFY_INTERVAL + 4,
};

static struct usb_ss_ep_comp_descriptor gps_ss_notify_comp_desc = {
	.bLength =		sizeof gps_ss_notify_comp_desc,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,

	/* the following 3 values can be tweaked if necessary */
	/* .bMaxBurst =		0, */
	/* .bmAttributes =	0, */
	.wBytesPerInterval =	cpu_to_le16(GPS_MAX_NOTIFY_SIZE),
};

static struct usb_descriptor_header *gps_ss_function[] = {
	(struct usb_descriptor_header *) &gps_interface_desc,
	(struct usb_descriptor_header *) &gps_ss_notify_desc,
	(struct usb_descriptor_header *) &gps_ss_notify_comp_desc,
	NULL,
};

/* String descriptors */

static struct usb_string gps_string_defs[] = {
	[0].s = "GPS",
	{  } /* end of list */
};

static struct usb_gadget_strings gps_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		gps_string_defs,
};

static struct usb_gadget_strings *gps_strings[] = {
	&gps_string_table,
	NULL,
};

static void gps_ctrl_response_available(struct f_gps *dev);

/* ------- misc functions --------------------*/

static inline struct f_gps *func_to_gps(struct usb_function *f)
{
	return container_of(f, struct f_gps, port.func);
}

static inline struct f_gps *port_to_gps(struct grmnet *r)
{
	return container_of(r, struct f_gps, port);
}

static struct usb_request *
gps_alloc_req(struct usb_ep *ep, unsigned len, gfp_t flags)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, flags);
	if (!req)
		return ERR_PTR(-ENOMEM);

	req->buf = kmalloc(len, flags);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return ERR_PTR(-ENOMEM);
	}

	req->length = len;

	return req;
}

void gps_free_req(struct usb_ep *ep, struct usb_request *req)
{
	kfree(req->buf);
	usb_ep_free_request(ep, req);
}

static struct rmnet_ctrl_pkt *gps_alloc_ctrl_pkt(unsigned len, gfp_t flags)
{
	struct rmnet_ctrl_pkt *pkt;

	pkt = kzalloc(sizeof(struct rmnet_ctrl_pkt), flags);
	if (!pkt)
		return ERR_PTR(-ENOMEM);

	pkt->buf = kmalloc(len, flags);
	if (!pkt->buf) {
		kfree(pkt);
		return ERR_PTR(-ENOMEM);
	}
	pkt->len = len;

	return pkt;
}

static void gps_free_ctrl_pkt(struct rmnet_ctrl_pkt *pkt)
{
	kfree(pkt->buf);
	kfree(pkt);
}

/* -------------------------------------------*/

static int gps_gport_setup(void)
{
	u8 base;
	int res;

	res = gsmd_ctrl_setup(GPS_CTRL_CLIENT, 1, &base);
	gps_port.port->port_num = base;
	return res;
}

static int gport_ctrl_connect(struct f_gps *dev)
{
	return gsmd_ctrl_connect(&dev->port, dev->port_num);
}

static int gport_gps_disconnect(struct f_gps *dev)
{
	gsmd_ctrl_disconnect(&dev->port, dev->port_num);
	return 0;
}

static void gps_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_gps *dev = func_to_gps(f);

	pr_debug("%s: portno:%d\n", __func__, dev->port_num);

	if (gadget_is_superspeed(c->cdev->gadget))
		usb_free_descriptors(f->ss_descriptors);
	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->fs_descriptors);

	gps_free_req(dev->notify, dev->notify_req);

	kfree(f->name);
}

static void gps_purge_responses(struct f_gps *dev)
{
	unsigned long flags;
	struct rmnet_ctrl_pkt *cpkt;

	pr_debug("%s: port#%d\n", __func__, dev->port_num);

	usb_ep_dequeue(dev->notify, dev->notify_req);
	spin_lock_irqsave(&dev->lock, flags);
	while (!list_empty(&dev->cpkt_resp_q)) {
		cpkt = list_first_entry(&dev->cpkt_resp_q,
				struct rmnet_ctrl_pkt, list);

		list_del(&cpkt->list);
		rmnet_free_ctrl_pkt(cpkt);
	}
	atomic_set(&dev->notify_count, 0);
	spin_unlock_irqrestore(&dev->lock, flags);
}

static void gps_suspend(struct usb_function *f)
{
	struct f_gps *dev = func_to_gps(f);

	pr_debug("%s: suspending gps function\n", __func__);
	if (f->config->cdev->gadget->speed == USB_SPEED_SUPER)
		dev->is_rw_allowed = f->func_wakeup_allowed;
	else
		dev->is_rw_allowed = f->config->cdev->gadget->remote_wakeup;

	gps_purge_responses(dev);
	dev->is_suspended = true;
}

static void gps_resume(struct usb_function *f)
{
	struct f_gps *dev = func_to_gps(f);

	pr_debug("%s: resume gps function, func_is_supended:%d\n",
			__func__, f->func_is_suspended);

	/* In SS mode, bus resume doesn't implies function
	 * resume. If the host has selectively suspended this
	 * function, handle resume only when host selectively
	 * resumes this function */
	if (f->config->cdev->gadget->speed == USB_SPEED_SUPER &&
			f->func_is_suspended)
		return;

	dev->is_suspended = false;
	spin_lock(&dev->lock);
	if (list_empty(&dev->cpkt_resp_q)) {
		spin_unlock(&dev->lock);
		return;
	}
	spin_unlock(&dev->lock);
	gps_ctrl_response_available(dev);
}

static int gps_get_status(struct usb_function *f)
{
	unsigned remote_wakeup_en_status = f->func_wakeup_allowed ? 1 : 0;

	return (remote_wakeup_en_status << FUNC_WAKEUP_ENABLE_SHIFT) |
		(1 << FUNC_WAKEUP_CAPABLE_SHIFT);
}

static int gps_func_suspend(struct usb_function *f, u8 options)
{
	bool func_wakeup_allowed;

	func_wakeup_allowed =
		((options & FUNC_SUSPEND_OPT_RW_EN_MASK) != 0);

	pr_debug("%s: func_wakeup_allowed:%d func_suspended:%d\n",
			__func__, func_wakeup_allowed, f->func_is_suspended);
	if (options & FUNC_SUSPEND_OPT_SUSP_MASK) {
		pr_debug("%s: calling function suspend\n", __func__);
		f->func_wakeup_allowed = func_wakeup_allowed;
		if (!f->func_is_suspended) {
			f->func_is_suspended = true;
			gps_suspend(f);
		}
	} else {
		pr_debug("%s: calling function resume\n", __func__);
		if (f->func_is_suspended) {
			f->func_is_suspended = false;
			gps_resume(f);
		}
		f->func_wakeup_allowed = func_wakeup_allowed;
	}

	return 0;
}

static int gps_wakeup_host(struct f_gps *dev)
{
	int ret;
	struct usb_gadget *gadget;
	struct usb_function *f;

	f = &dev->port.func;
	gadget = f->config->cdev->gadget;

	if (!gadget) {
		pr_err("%s: failed gadget=NULL", __func__);
		return -ENODEV;
	}

	pr_debug("%s: func_is_suspended: %d\n", __func__,
			f->func_is_suspended);
	if ((gadget->speed == USB_SPEED_SUPER) && f->func_is_suspended)
		ret = usb_func_wakeup(f);
	else
		ret = usb_gadget_wakeup(gadget);

	if ((ret == -EBUSY) || (ret == -EAGAIN))
		pr_err("%s: remote wakeup delayed due to LPM exit", __func__);
	else if (ret)
		pr_err("%s:wakeup failed, ret=%d", __func__, ret);

	return ret;
}

static void gps_disable(struct usb_function *f)
{
	struct f_gps *dev = func_to_gps(f);

	usb_ep_disable(dev->notify);
	dev->notify->driver_data = NULL;

	atomic_set(&dev->online, 0);

	gps_purge_responses(dev);

	gport_gps_disconnect(dev);
}

static int
gps_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_gps			*dev = func_to_gps(f);
	struct usb_composite_dev	*cdev = dev->cdev;
	int				ret;
	struct list_head *cpkt;

	pr_debug("%s:dev:%pK\n", __func__, dev);

	if (dev->notify->driver_data)
		usb_ep_disable(dev->notify);

	ret = config_ep_by_speed(cdev->gadget, f, dev->notify);
	if (ret) {
		dev->notify->desc = NULL;
		ERROR(cdev, "config_ep_by_speed failes for ep %s, result %d\n",
					dev->notify->name, ret);
		return ret;
	}
	ret = usb_ep_enable(dev->notify);

	if (ret) {
		pr_err("%s: usb ep#%s enable failed, err#%d\n",
				__func__, dev->notify->name, ret);
		return ret;
	}
	dev->notify->driver_data = dev;

	ret = gport_ctrl_connect(dev);

	atomic_set(&dev->online, 1);

	/* In case notifications were aborted, but there are pending control
	   packets in the response queue, re-add the notifications */
	list_for_each(cpkt, &dev->cpkt_resp_q)
		gps_ctrl_response_available(dev);

	return ret;
}

static void gps_ctrl_response_available(struct f_gps *dev)
{
	struct usb_request		*req = dev->notify_req;
	struct usb_cdc_notification	*event;
	unsigned long			flags;
	int				ret;
	struct rmnet_ctrl_pkt	*cpkt;

	pr_debug("%s:dev:%pK\n", __func__, dev);

	spin_lock_irqsave(&dev->lock, flags);
	if (!atomic_read(&dev->online) || !req || !req->buf) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	if (atomic_inc_return(&dev->notify_count) != 1) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}

	event = req->buf;
	event->bmRequestType = USB_DIR_IN | USB_TYPE_CLASS
			| USB_RECIP_INTERFACE;
	event->bNotificationType = USB_CDC_NOTIFY_RESPONSE_AVAILABLE;
	event->wValue = cpu_to_le16(0);
	event->wIndex = cpu_to_le16(dev->ifc_id);
	event->wLength = cpu_to_le16(0);
	spin_unlock_irqrestore(&dev->lock, flags);

	ret = usb_ep_queue(dev->notify, dev->notify_req, GFP_ATOMIC);
	if (ret) {
		if (ret == -EBUSY) {
			pr_err("%s: notify_count:%u\n",
				__func__, atomic_read(&dev->notify_count));
			WARN_ON(1);
		}
		spin_lock_irqsave(&dev->lock, flags);
		if (!list_empty(&dev->cpkt_resp_q)) {
			atomic_dec(&dev->notify_count);
			cpkt = list_first_entry(&dev->cpkt_resp_q,
					struct rmnet_ctrl_pkt, list);
			list_del(&cpkt->list);
			gps_free_ctrl_pkt(cpkt);
		}
		spin_unlock_irqrestore(&dev->lock, flags);
		pr_debug("ep enqueue error %d\n", ret);
	}
}

static void gps_connect(struct grmnet *gr)
{
	struct f_gps			*dev;

	if (!gr) {
		pr_err("%s: Invalid grmnet:%pK\n", __func__, gr);
		return;
	}

	dev = port_to_gps(gr);

	atomic_set(&dev->ctrl_online, 1);
}

static void gps_disconnect(struct grmnet *gr)
{
	struct f_gps			*dev;

	if (!gr) {
		pr_err("%s: Invalid grmnet:%pK\n", __func__, gr);
		return;
	}

	dev = port_to_gps(gr);

	atomic_set(&dev->ctrl_online, 0);

	if (!atomic_read(&dev->online)) {
		pr_debug("%s: nothing to do\n", __func__);
		return;
	}

	/* dequeue any pending notify_req */
	usb_ep_dequeue(dev->notify, dev->notify_req);
	gps_purge_responses(dev);
}

static int
gps_send_cpkt_response(void *gr, void *buf, size_t len)
{
	struct f_gps		*dev;
	struct rmnet_ctrl_pkt	*cpkt;
	unsigned long		flags;

	if (!gr || !buf) {
		pr_err("%s: Invalid grmnet/buf, grmnet:%pK buf:%pK\n",
				__func__, gr, buf);
		return -ENODEV;
	}
	cpkt = gps_alloc_ctrl_pkt(len, GFP_ATOMIC);
	if (IS_ERR(cpkt)) {
		pr_err("%s: Unable to allocate ctrl pkt\n", __func__);
		return -ENOMEM;
	}
	memcpy(cpkt->buf, buf, len);
	cpkt->len = len;

	dev = port_to_gps(gr);

	pr_debug("%s: dev:%pK\n", __func__, dev);

	if (!atomic_read(&dev->online) || !atomic_read(&dev->ctrl_online)) {
		gps_free_ctrl_pkt(cpkt);
		return 0;
	}

	spin_lock_irqsave(&dev->lock, flags);
	list_add_tail(&cpkt->list, &dev->cpkt_resp_q);
	spin_unlock_irqrestore(&dev->lock, flags);

	if (dev->is_suspended && dev->is_rw_allowed) {
		pr_debug("%s: calling gps_wakeup_host\n", __func__);
		gps_wakeup_host(dev);
		goto end;
	}

	gps_ctrl_response_available(dev);

end:
	return 0;
}

static void
gps_cmd_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_gps			*dev = req->context;
	struct usb_composite_dev	*cdev;

	if (!dev) {
		pr_err("%s: dev is null\n", __func__);
		return;
	}

	pr_debug("%s: dev:%pK\n", __func__, dev);

	cdev = dev->cdev;

	if (dev->port.send_encap_cmd)
		dev->port.send_encap_cmd(dev->port_num, req->buf, req->actual);
}

static void gps_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_gps *dev = req->context;
	int status = req->status;
	unsigned long		flags;
	struct rmnet_ctrl_pkt	*cpkt;

	pr_debug("%s: dev:%pK port#%d\n", __func__, dev, dev->port_num);

	switch (status) {
	case -ECONNRESET:
	case -ESHUTDOWN:
		/* connection gone */
		atomic_set(&dev->notify_count, 0);
		break;
	default:
		pr_err("gps notify ep error %d\n", status);
		/* FALLTHROUGH */
	case 0:
		if (!atomic_read(&dev->ctrl_online))
			break;

		pr_debug("%s: decrement notify_count:%u\n", __func__,
				atomic_read(&dev->notify_count));
		if (atomic_dec_and_test(&dev->notify_count))
			break;

		status = usb_ep_queue(dev->notify, req, GFP_ATOMIC);
		if (status) {
			if (status == -EBUSY) {
				pr_err("%s: notify_count:%u\n",
					__func__,
					atomic_read(&dev->notify_count));
				WARN_ON(1);
			}

			spin_lock_irqsave(&dev->lock, flags);
			if (!list_empty(&dev->cpkt_resp_q)) {
				atomic_dec(&dev->notify_count);
				cpkt = list_first_entry(&dev->cpkt_resp_q,
						struct rmnet_ctrl_pkt, list);
				list_del(&cpkt->list);
				gps_free_ctrl_pkt(cpkt);
			}
			spin_unlock_irqrestore(&dev->lock, flags);
			pr_debug("ep enqueue error %d\n", status);
		}
		break;
	}
}

static int
gps_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct f_gps			*dev = func_to_gps(f);
	struct usb_composite_dev	*cdev = dev->cdev;
	struct usb_request		*req = cdev->req;
	u16				w_index = le16_to_cpu(ctrl->wIndex);
	u16				w_value = le16_to_cpu(ctrl->wValue);
	u16				w_length = le16_to_cpu(ctrl->wLength);
	int				ret = -EOPNOTSUPP;

	pr_debug("%s:dev:%pK\n", __func__, dev);

	if (!atomic_read(&dev->online)) {
		pr_debug("%s: usb cable is not connected\n", __func__);
		return -ENOTCONN;
	}

	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SEND_ENCAPSULATED_COMMAND:
		ret = w_length;
		req->complete = gps_cmd_complete;
		req->context = dev;
		break;

	case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_GET_ENCAPSULATED_RESPONSE:
		if (w_value)
			goto invalid;
		else {
			unsigned len;
			struct rmnet_ctrl_pkt *cpkt;

			spin_lock(&dev->lock);
			if (list_empty(&dev->cpkt_resp_q)) {
				spin_unlock(&dev->lock);
				pr_debug("%s: ctrl resp queue empty", __func__);
				ret = 0;
				goto invalid;
			}

			cpkt = list_first_entry(&dev->cpkt_resp_q,
					struct rmnet_ctrl_pkt, list);
			list_del(&cpkt->list);
			spin_unlock(&dev->lock);

			len = min_t(unsigned, w_length, cpkt->len);
			memcpy(req->buf, cpkt->buf, len);
			ret = len;

			gps_free_ctrl_pkt(cpkt);
		}
		break;
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		if (dev->port.notify_modem)
			dev->port.notify_modem(&dev->port,
							dev->port_num, w_value);
		ret = 0;

		break;
	default:

invalid:
		DBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (ret >= 0) {
		VDBG(cdev, "gps req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = (ret < w_length);
		req->length = ret;
		ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (ret < 0)
			ERROR(cdev, "gps ep0 enqueue err %d\n", ret);
	}

	return ret;
}

static int gps_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_gps			*dev = func_to_gps(f);
	struct usb_ep			*ep;
	struct usb_composite_dev	*cdev = c->cdev;
	int				ret = -ENODEV;

	dev->ifc_id = usb_interface_id(c, f);
	if (dev->ifc_id < 0) {
		pr_err("%s: unable to allocate ifc id, err:%d",
				__func__, dev->ifc_id);
		return dev->ifc_id;
	}
	gps_interface_desc.bInterfaceNumber = dev->ifc_id;

	dev->port.in = NULL;
	dev->port.out = NULL;

	ep = usb_ep_autoconfig(cdev->gadget, &gps_fs_notify_desc);
	if (!ep) {
		pr_err("%s: usb epnotify autoconfig failed\n", __func__);
		ret = -ENODEV;
		goto ep_auto_notify_fail;
	}
	dev->notify = ep;
	ep->driver_data = cdev;

	dev->notify_req = gps_alloc_req(ep,
				sizeof(struct usb_cdc_notification),
				GFP_KERNEL);
	if (IS_ERR(dev->notify_req)) {
		pr_err("%s: unable to allocate memory for notify req\n",
				__func__);
		ret = -ENOMEM;
		goto ep_notify_alloc_fail;
	}

	dev->notify_req->complete = gps_notify_complete;
	dev->notify_req->context = dev;

	ret = -ENOMEM;
	f->fs_descriptors = usb_copy_descriptors(gps_fs_function);

	if (!f->fs_descriptors)
		goto fail;

	if (gadget_is_dualspeed(cdev->gadget)) {
		gps_hs_notify_desc.bEndpointAddress =
				gps_fs_notify_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(gps_hs_function);

		if (!f->hs_descriptors)
			goto fail;
	}

	if (gadget_is_superspeed(cdev->gadget)) {
		gps_ss_notify_desc.bEndpointAddress =
				gps_fs_notify_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->ss_descriptors = usb_copy_descriptors(gps_ss_function);

		if (!f->ss_descriptors)
			goto fail;
	}

	pr_info("%s: GPS(%d) %s Speed\n",
			__func__, dev->port_num,
			gadget_is_dualspeed(cdev->gadget) ? "dual" : "full");

	return 0;

fail:
	if (f->ss_descriptors)
		usb_free_descriptors(f->ss_descriptors);
	if (f->hs_descriptors)
		usb_free_descriptors(f->hs_descriptors);
	if (f->fs_descriptors)
		usb_free_descriptors(f->fs_descriptors);
	if (dev->notify_req)
		gps_free_req(dev->notify, dev->notify_req);
ep_notify_alloc_fail:
	dev->notify->driver_data = NULL;
	dev->notify = NULL;
ep_auto_notify_fail:
	return ret;
}

static int gps_bind_config(struct usb_configuration *c)
{
	int			status;
	struct f_gps		*dev;
	struct usb_function	*f;
	unsigned long		flags;

	pr_debug("%s: usb config:%pK\n", __func__, c);

	if (gps_string_defs[0].id == 0) {
		status = usb_string_id(c->cdev);
		if (status < 0) {
			pr_err("%s: failed to get string id, err:%d\n",
					__func__, status);
			return status;
		}
		gps_string_defs[0].id = status;
	}

	dev = gps_port.port;

	spin_lock_irqsave(&dev->lock, flags);
	dev->cdev = c->cdev;
	f = &dev->port.func;
	f->name = kasprintf(GFP_ATOMIC, "gps");
	spin_unlock_irqrestore(&dev->lock, flags);
	if (!f->name) {
		pr_err("%s: cannot allocate memory for name\n", __func__);
		return -ENOMEM;
	}

	f->strings = gps_strings;
	f->bind = gps_bind;
	f->unbind = gps_unbind;
	f->disable = gps_disable;
	f->set_alt = gps_set_alt;
	f->setup = gps_setup;
	f->suspend = gps_suspend;
	f->func_suspend = gps_func_suspend;
	f->get_status = gps_get_status;
	f->resume = gps_resume;
	dev->port.send_cpkt_response = gps_send_cpkt_response;
	dev->port.disconnect = gps_disconnect;
	dev->port.connect = gps_connect;

	status = usb_add_function(c, f);
	if (status) {
		pr_err("%s: usb add function failed: %d\n",
				__func__, status);
		kfree(f->name);
		return status;
	}

	pr_debug("%s: complete\n", __func__);

	return status;
}

static void gps_cleanup(void)
{
	kfree(gps_port.port);
}

static int gps_init_port(void)
{
	struct f_gps			*dev;

	dev = kzalloc(sizeof(struct f_gps), GFP_KERNEL);
	if (!dev) {
		pr_err("%s: Unable to allocate gps device\n", __func__);
		return -ENOMEM;
	}

	spin_lock_init(&dev->lock);
	INIT_LIST_HEAD(&dev->cpkt_resp_q);
	dev->port_num = 0;

	gps_port.port = dev;
	gps_port.ctrl_xport = USB_GADGET_XPORT_SMD;

	return 0;
}
