/*
 * f_qc_ecm.c -- USB CDC Ethernet (ECM) link function driver
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* #define VERBOSE_DEBUG */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/etherdevice.h>

#include "u_ether.h"
#include "u_qc_ether.h"

#include "u_bam_data.h"
#include <mach/ecm_ipa.h>


/*
 * This function is a "CDC Ethernet Networking Control Model" (CDC ECM)
 * Ethernet link.  The data transfer model is simple (packets sent and
 * received over bulk endpoints using normal short packet termination),
 * and the control model exposes various data and optional notifications.
 *
 * ECM is well standardized and (except for Microsoft) supported by most
 * operating systems with USB host support.  It's the preferred interop
 * solution for Ethernet over USB, at least for firmware based solutions.
 * (Hardware solutions tend to be more minimalist.)  A newer and simpler
 * "Ethernet Emulation Model" (CDC EEM) hasn't yet caught on.
 *
 * Note that ECM requires the use of "alternate settings" for its data
 * interface.  This means that the set_alt() method has real work to do,
 * and also means that a get_alt() method is required.
 *
 * This function is based on USB CDC Ethernet link function driver and
 * contains MSM specific implementation.
 */


enum ecm_qc_notify_state {
	ECM_QC_NOTIFY_NONE,		/* don't notify */
	ECM_QC_NOTIFY_CONNECT,		/* issue CONNECT next */
	ECM_QC_NOTIFY_SPEED,		/* issue SPEED_CHANGE next */
};

struct f_ecm_qc {
	struct qc_gether		port;
	u8				ctrl_id, data_id;
	enum transport_type		xport;
	char				ethaddr[14];

	struct usb_ep			*notify;
	struct usb_request		*notify_req;
	u8				notify_state;
	bool				is_open;
	struct data_port		bam_port;
};

static struct ecm_ipa_params ipa_params;

static inline struct f_ecm_qc *func_to_ecm_qc(struct usb_function *f)
{
	return container_of(f, struct f_ecm_qc, port.func);
}

/* peak (theoretical) bulk transfer rate in bits-per-second */
static inline unsigned ecm_qc_bitrate(struct usb_gadget *g)
{
	if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return 13 * 512 * 8 * 1000 * 8;
	else
		return 19 *  64 * 1 * 1000 * 8;
}

/*-------------------------------------------------------------------------*/

/*
 * Include the status endpoint if we can, even though it's optional.
 *
 * Use wMaxPacketSize big enough to fit CDC_NOTIFY_SPEED_CHANGE in one
 * packet, to simplify cancellation; and a big transfer interval, to
 * waste less bandwidth.
 *
 * Some drivers (like Linux 2.4 cdc-ether!) "need" it to exist even
 * if they ignore the connect/disconnect notifications that real aether
 * can provide.  More advanced cdc configurations might want to support
 * encapsulated commands (vendor-specific, using control-OUT).
 */

#define ECM_QC_LOG2_STATUS_INTERVAL_MSEC	5	/* 1 << 5 == 32 msec */
#define ECM_QC_STATUS_BYTECOUNT		16	/* 8 byte header + data */

/* Currently only one std ecm instance is supported - port index 0. */
#define ECM_QC_NO_PORTS						1
#define ECM_QC_ACTIVE_PORT					0

/* interface descriptor: */

static struct usb_interface_descriptor ecm_qc_control_intf = {
	.bLength =		sizeof ecm_qc_control_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	/* .bInterfaceNumber = DYNAMIC */
	/* status endpoint is optional; this could be patched later */
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ETHERNET,
	.bInterfaceProtocol =	USB_CDC_PROTO_NONE,
	/* .iInterface = DYNAMIC */
};

static struct usb_cdc_header_desc ecm_qc_header_desc = {
	.bLength =		sizeof ecm_qc_header_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,

	.bcdCDC =		cpu_to_le16(0x0110),
};

static struct usb_cdc_union_desc ecm_qc_union_desc = {
	.bLength =		sizeof(ecm_qc_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
	/* .bMasterInterface0 =	DYNAMIC */
	/* .bSlaveInterface0 =	DYNAMIC */
};

static struct usb_cdc_ether_desc ecm_qc_desc = {
	.bLength =		sizeof ecm_qc_desc,
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ETHERNET_TYPE,

	/* this descriptor actually adds value, surprise! */
	/* .iMACAddress = DYNAMIC */
	.bmEthernetStatistics =	cpu_to_le32(0), /* no statistics */
	.wMaxSegmentSize =	cpu_to_le16(ETH_FRAME_LEN),
	.wNumberMCFilters =	cpu_to_le16(0),
	.bNumberPowerFilters =	0,
};

/* the default data interface has no endpoints ... */

static struct usb_interface_descriptor ecm_qc_data_nop_intf = {
	.bLength =		sizeof ecm_qc_data_nop_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bInterfaceNumber =	1,
	.bAlternateSetting =	0,
	.bNumEndpoints =	0,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

/* ... but the "real" data interface has two bulk endpoints */

static struct usb_interface_descriptor ecm_qc_data_intf = {
	.bLength =		sizeof ecm_qc_data_intf,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bInterfaceNumber =	1,
	.bAlternateSetting =	1,
	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =	0,
	.bInterfaceProtocol =	0,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor ecm_qc_fs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(ECM_QC_STATUS_BYTECOUNT),
	.bInterval =		1 << ECM_QC_LOG2_STATUS_INTERVAL_MSEC,
};

static struct usb_endpoint_descriptor ecm_qc_fs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor ecm_qc_fs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *ecm_qc_fs_function[] = {
	/* CDC ECM control descriptors */
	(struct usb_descriptor_header *) &ecm_qc_control_intf,
	(struct usb_descriptor_header *) &ecm_qc_header_desc,
	(struct usb_descriptor_header *) &ecm_qc_union_desc,
	(struct usb_descriptor_header *) &ecm_qc_desc,
	/* NOTE: status endpoint might need to be removed */
	(struct usb_descriptor_header *) &ecm_qc_fs_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &ecm_qc_data_nop_intf,
	(struct usb_descriptor_header *) &ecm_qc_data_intf,
	(struct usb_descriptor_header *) &ecm_qc_fs_in_desc,
	(struct usb_descriptor_header *) &ecm_qc_fs_out_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor ecm_qc_hs_notify_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(ECM_QC_STATUS_BYTECOUNT),
	.bInterval =		ECM_QC_LOG2_STATUS_INTERVAL_MSEC + 4,
};
static struct usb_endpoint_descriptor ecm_qc_hs_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor ecm_qc_hs_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *ecm_qc_hs_function[] = {
	/* CDC ECM control descriptors */
	(struct usb_descriptor_header *) &ecm_qc_control_intf,
	(struct usb_descriptor_header *) &ecm_qc_header_desc,
	(struct usb_descriptor_header *) &ecm_qc_union_desc,
	(struct usb_descriptor_header *) &ecm_qc_desc,
	/* NOTE: status endpoint might need to be removed */
	(struct usb_descriptor_header *) &ecm_qc_hs_notify_desc,
	/* data interface, altsettings 0 and 1 */
	(struct usb_descriptor_header *) &ecm_qc_data_nop_intf,
	(struct usb_descriptor_header *) &ecm_qc_data_intf,
	(struct usb_descriptor_header *) &ecm_qc_hs_in_desc,
	(struct usb_descriptor_header *) &ecm_qc_hs_out_desc,
	NULL,
};

/* string descriptors: */

static struct usb_string ecm_qc_string_defs[] = {
	[0].s = "CDC Ethernet Control Model (ECM)",
	[1].s = NULL /* DYNAMIC */,
	[2].s = "CDC Ethernet Data",
	{  } /* end of list */
};

static struct usb_gadget_strings ecm_qc_string_table = {
	.language =		0x0409,	/* en-us */
	.strings =		ecm_qc_string_defs,
};

static struct usb_gadget_strings *ecm_qc_strings[] = {
	&ecm_qc_string_table,
	NULL,
};

static void ecm_qc_do_notify(struct f_ecm_qc *ecm)
{
	struct usb_request		*req = ecm->notify_req;
	struct usb_cdc_notification	*event;
	struct usb_composite_dev	*cdev = ecm->port.func.config->cdev;
	__le32				*data;
	int				status;

	/* notification already in flight? */
	if (!req)
		return;

	event = req->buf;
	switch (ecm->notify_state) {
	case ECM_QC_NOTIFY_NONE:
		return;

	case ECM_QC_NOTIFY_CONNECT:
		event->bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
		if (ecm->is_open)
			event->wValue = cpu_to_le16(1);
		else
			event->wValue = cpu_to_le16(0);
		event->wLength = 0;
		req->length = sizeof *event;

		DBG(cdev, "notify connect %s\n",
				ecm->is_open ? "true" : "false");
		ecm->notify_state = ECM_QC_NOTIFY_SPEED;
		break;

	case ECM_QC_NOTIFY_SPEED:
		event->bNotificationType = USB_CDC_NOTIFY_SPEED_CHANGE;
		event->wValue = cpu_to_le16(0);
		event->wLength = cpu_to_le16(8);
		req->length = ECM_QC_STATUS_BYTECOUNT;

		/* SPEED_CHANGE data is up/down speeds in bits/sec */
		data = req->buf + sizeof *event;
		data[0] = cpu_to_le32(ecm_qc_bitrate(cdev->gadget));
		data[1] = data[0];

		DBG(cdev, "notify speed %d\n", ecm_qc_bitrate(cdev->gadget));
		ecm->notify_state = ECM_QC_NOTIFY_NONE;
		break;
	}
	event->bmRequestType = 0xA1;
	event->wIndex = cpu_to_le16(ecm->ctrl_id);

	ecm->notify_req = NULL;
	status = usb_ep_queue(ecm->notify, req, GFP_ATOMIC);
	if (status < 0) {
		ecm->notify_req = req;
		DBG(cdev, "notify --> %d\n", status);
	}
}

static void ecm_qc_notify(struct f_ecm_qc *ecm)
{
	/* NOTE on most versions of Linux, host side cdc-ethernet
	 * won't listen for notifications until its netdevice opens.
	 * The first notification then sits in the FIFO for a long
	 * time, and the second one is queued.
	 */
	ecm->notify_state = ECM_QC_NOTIFY_CONNECT;
	ecm_qc_do_notify(ecm);
}

static int ecm_qc_bam_setup(void)
{
	int ret;

	ret = bam_data_setup(ECM_QC_NO_PORTS);
	if (ret) {
		pr_err("bam_data_setup failed err: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ecm_qc_bam_connect(struct f_ecm_qc *dev)
{
	int ret;
	u8 src_connection_idx, dst_connection_idx;
	struct usb_composite_dev *cdev = dev->port.func.config->cdev;
	struct usb_gadget *gadget = cdev->gadget;
	enum peer_bam peer_bam = (dev->xport == USB_GADGET_XPORT_BAM2BAM_IPA) ?
		IPA_P_BAM : A2_P_BAM;

	dev->bam_port.cdev = cdev;
	dev->bam_port.func = &dev->port.func;
	dev->bam_port.in = dev->port.in_ep;
	dev->bam_port.out = dev->port.out_ep;

	/* currently we use the first connection */
	src_connection_idx = usb_bam_get_connection_idx(gadget->name, peer_bam,
		USB_TO_PEER_PERIPHERAL, 0);
	dst_connection_idx = usb_bam_get_connection_idx(gadget->name, peer_bam,
		PEER_PERIPHERAL_TO_USB, 0);
	if (src_connection_idx < 0 || dst_connection_idx < 0) {
		pr_err("%s: usb_bam_get_connection_idx failed\n", __func__);
		return ret;
	}
	ret = bam_data_connect(&dev->bam_port, 0, dev->xport,
		src_connection_idx, dst_connection_idx, USB_FUNC_ECM);
	if (ret) {
		pr_err("bam_data_connect failed: err:%d\n", ret);
		return ret;
	} else {
		pr_debug("ecm bam connected\n");
	}

	dev->is_open = true;
	ecm_qc_notify(dev);

	return 0;
}

static int ecm_qc_bam_disconnect(struct f_ecm_qc *dev)
{
	pr_debug("dev:%p. Disconnect BAM.\n", dev);

	bam_data_disconnect(&dev->bam_port, 0);

	return 0;
}

void *ecm_qc_get_ipa_rx_cb(void)
{
	return ipa_params.ecm_ipa_rx_dp_notify;
}

void *ecm_qc_get_ipa_tx_cb(void)
{
	return ipa_params.ecm_ipa_tx_dp_notify;
}

void *ecm_qc_get_ipa_priv(void)
{
	return ipa_params.private;
}

/*-------------------------------------------------------------------------*/



static void ecm_qc_notify_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_ecm_qc			*ecm = req->context;
	struct usb_composite_dev	*cdev = ecm->port.func.config->cdev;
	struct usb_cdc_notification	*event = req->buf;

	switch (req->status) {
	case 0:
		/* no fault */
		break;
	case -ECONNRESET:
	case -ESHUTDOWN:
		ecm->notify_state = ECM_QC_NOTIFY_NONE;
		break;
	default:
		DBG(cdev, "event %02x --> %d\n",
			event->bNotificationType, req->status);
		break;
	}
	ecm->notify_req = req;
	ecm_qc_do_notify(ecm);
}

static int ecm_qc_setup(struct usb_function *f,
				const struct usb_ctrlrequest *ctrl)
{
	struct f_ecm_qc		*ecm = func_to_ecm_qc(f);
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request	*req = cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	/* composite driver infrastructure handles everything except
	 * CDC class messages; interface activation uses set_alt().
	 */
	switch ((ctrl->bRequestType << 8) | ctrl->bRequest) {
	case ((USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE) << 8)
			| USB_CDC_SET_ETHERNET_PACKET_FILTER:
		/* see 6.2.30: no data, wIndex = interface,
		 * wValue = packet filter bitmap
		 */
		if (w_length != 0 || w_index != ecm->ctrl_id)
			goto invalid;
		DBG(cdev, "packet filter %02x\n", w_value);
		/* REVISIT locking of cdc_filter.  This assumes the UDC
		 * driver won't have a concurrent packet TX irq running on
		 * another CPU; or that if it does, this write is atomic...
		 */
		ecm->port.cdc_filter = w_value;
		value = 0;
		break;

	/* and optionally:
	 * case USB_CDC_SEND_ENCAPSULATED_COMMAND:
	 * case USB_CDC_GET_ENCAPSULATED_RESPONSE:
	 * case USB_CDC_SET_ETHERNET_MULTICAST_FILTERS:
	 * case USB_CDC_SET_ETHERNET_PM_PATTERN_FILTER:
	 * case USB_CDC_GET_ETHERNET_PM_PATTERN_FILTER:
	 * case USB_CDC_GET_ETHERNET_STATISTIC:
	 */

	default:
invalid:
		DBG(cdev, "invalid control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
	}

	/* respond with data transfer or status phase? */
	if (value >= 0) {
		DBG(cdev, "ecm req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			w_value, w_index, w_length);
		req->zero = 0;
		req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
		if (value < 0)
			pr_err("ecm req %02x.%02x response err %d\n",
					ctrl->bRequestType, ctrl->bRequest,
					value);
	}

	/* device either stalls (value < 0) or reports success */
	return value;
}


static int ecm_qc_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct f_ecm_qc		*ecm = func_to_ecm_qc(f);
	struct usb_composite_dev *cdev = f->config->cdev;

	/* Control interface has only altsetting 0 */
	if (intf == ecm->ctrl_id) {
		if (alt != 0)
			goto fail;

		if (ecm->notify->driver_data) {
			VDBG(cdev, "reset ecm control %d\n", intf);
			usb_ep_disable(ecm->notify);
		}
		if (!(ecm->notify->desc)) {
			VDBG(cdev, "init ecm ctrl %d\n", intf);
			if (config_ep_by_speed(cdev->gadget, f, ecm->notify))
				goto fail;
		}
		usb_ep_enable(ecm->notify);
		ecm->notify->driver_data = ecm;

	/* Data interface has two altsettings, 0 and 1 */
	} else if (intf == ecm->data_id) {
		if (alt > 1)
			goto fail;

		if (ecm->port.in_ep->driver_data) {
			DBG(cdev, "reset ecm\n");
			/* ecm->port is needed for disconnecting the BAM data
			 * path. Only after the BAM data path is disconnected,
			 * we can disconnect the port from the network layer.
			 */
			ecm_qc_bam_disconnect(ecm);
			if (ecm->xport != USB_GADGET_XPORT_BAM2BAM_IPA)
				gether_qc_disconnect_name(&ecm->port, "ecm0");
		}

		if (!ecm->port.in_ep->desc ||
		    !ecm->port.out_ep->desc) {
			DBG(cdev, "init ecm\n");
			if (config_ep_by_speed(cdev->gadget, f,
					       ecm->port.in_ep) ||
			    config_ep_by_speed(cdev->gadget, f,
					       ecm->port.out_ep)) {
				ecm->port.in_ep->desc = NULL;
				ecm->port.out_ep->desc = NULL;
				goto fail;
			}
		}

		/* CDC Ethernet only sends data in non-default altsettings.
		 * Changing altsettings resets filters, statistics, etc.
		 */
		if (alt == 1) {
			struct net_device	*net;

			/* Enable zlps by default for ECM conformance;
			 * override for musb_hdrc (avoids txdma ovhead).
			 */
			ecm->port.is_zlp_ok = !(gadget_is_musbhdrc(cdev->gadget)
				);
			ecm->port.cdc_filter = DEFAULT_FILTER;
			DBG(cdev, "activate ecm\n");
			if (ecm->xport != USB_GADGET_XPORT_BAM2BAM_IPA) {
				net = gether_qc_connect_name(&ecm->port,
								"ecm0", true);
				if (IS_ERR(net))
					return PTR_ERR(net);
			}

			if (ecm_qc_bam_connect(ecm))
				goto fail;
		}

		/* NOTE this can be a minor disagreement with the ECM spec,
		 * which says speed notifications will "always" follow
		 * connection notifications.  But we allow one connect to
		 * follow another (if the first is in flight), and instead
		 * just guarantee that a speed notification is always sent.
		 */
		ecm_qc_notify(ecm);
	} else
		goto fail;

	return 0;
fail:
	return -EINVAL;
}

/* Because the data interface supports multiple altsettings,
 * this ECM function *MUST* implement a get_alt() method.
 */
static int ecm_qc_get_alt(struct usb_function *f, unsigned intf)
{
	struct f_ecm_qc		*ecm = func_to_ecm_qc(f);

	if (intf == ecm->ctrl_id)
		return 0;
	return ecm->port.in_ep->driver_data ? 1 : 0;
}

static void ecm_qc_disable(struct usb_function *f)
{
	struct f_ecm_qc		*ecm = func_to_ecm_qc(f);
	struct usb_composite_dev	*cdev = ecm->port.func.config->cdev;

	DBG(cdev, "ecm deactivated\n");

	if (ecm->port.in_ep->driver_data) {
		ecm_qc_bam_disconnect(ecm);
		if (ecm->xport != USB_GADGET_XPORT_BAM2BAM_IPA)
			gether_qc_disconnect_name(&ecm->port, "ecm0");
	}

	if (ecm->notify->driver_data) {
		usb_ep_disable(ecm->notify);
		ecm->notify->driver_data = NULL;
		ecm->notify->desc = NULL;
	}
}

static void ecm_qc_suspend(struct usb_function *f)
{
	pr_debug("ecm suspended\n");

	bam_data_suspend(ECM_QC_ACTIVE_PORT);
}

static void ecm_qc_resume(struct usb_function *f)
{
	pr_debug("ecm resumed\n");

	bam_data_resume(ECM_QC_ACTIVE_PORT);
}

/*-------------------------------------------------------------------------*/

/*
 * Callbacks let us notify the host about connect/disconnect when the
 * net device is opened or closed.
 *
 * For testing, note that link states on this side include both opened
 * and closed variants of:
 *
 *   - disconnected/unconfigured
 *   - configured but inactive (data alt 0)
 *   - configured and active (data alt 1)
 *
 * Each needs to be tested with unplug, rmmod, SET_CONFIGURATION, and
 * SET_INTERFACE (altsetting).  Remember also that "configured" doesn't
 * imply the host is actually polling the notification endpoint, and
 * likewise that "active" doesn't imply it's actually using the data
 * endpoints for traffic.
 */

static void ecm_qc_open(struct qc_gether *geth)
{
	struct f_ecm_qc		*ecm = func_to_ecm_qc(&geth->func);
	DBG(ecm->port.func.config->cdev, "%s\n", __func__);

	ecm->is_open = true;
	ecm_qc_notify(ecm);
}

static void ecm_qc_close(struct qc_gether *geth)
{
	struct f_ecm_qc		*ecm = func_to_ecm_qc(&geth->func);

	DBG(ecm->port.func.config->cdev, "%s\n", __func__);

	ecm->is_open = false;
	ecm_qc_notify(ecm);
}

/*-------------------------------------------------------------------------*/

/* ethernet function driver setup/binding */

static int
ecm_qc_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_ecm_qc		*ecm = func_to_ecm_qc(f);
	int			status;
	struct usb_ep		*ep;

	/* allocate instance-specific interface IDs */
	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;

	ecm->ctrl_id = status;

	ecm_qc_control_intf.bInterfaceNumber = status;
	ecm_qc_union_desc.bMasterInterface0 = status;

	status = usb_interface_id(c, f);
	if (status < 0)
		goto fail;

	ecm->data_id = status;

	ecm_qc_data_nop_intf.bInterfaceNumber = status;
	ecm_qc_data_intf.bInterfaceNumber = status;
	ecm_qc_union_desc.bSlaveInterface0 = status;

	status = -ENODEV;

	/* allocate instance-specific endpoints */
	ep = usb_ep_autoconfig(cdev->gadget, &ecm_qc_fs_in_desc);
	if (!ep)
		goto fail;

	ecm->port.in_ep = ep;
	ep->driver_data = cdev;	/* claim */

	ep = usb_ep_autoconfig(cdev->gadget, &ecm_qc_fs_out_desc);
	if (!ep)
		goto fail;

	ecm->port.out_ep = ep;
	ep->driver_data = cdev;	/* claim */

	/* NOTE:  a status/notification endpoint is *OPTIONAL* but we
	 * don't treat it that way.  It's simpler, and some newer CDC
	 * profiles (wireless handsets) no longer treat it as optional.
	 */
	ep = usb_ep_autoconfig(cdev->gadget, &ecm_qc_fs_notify_desc);
	if (!ep)
		goto fail;
	ecm->notify = ep;
	ep->driver_data = cdev;	/* claim */

	status = -ENOMEM;

	/* allocate notification request and buffer */
	ecm->notify_req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!ecm->notify_req)
		goto fail;
	ecm->notify_req->buf = kmalloc(ECM_QC_STATUS_BYTECOUNT, GFP_KERNEL);
	if (!ecm->notify_req->buf)
		goto fail;
	ecm->notify_req->context = ecm;
	ecm->notify_req->complete = ecm_qc_notify_complete;

	/* copy descriptors, and track endpoint copies */
	f->descriptors = usb_copy_descriptors(ecm_qc_fs_function);
	if (!f->descriptors)
		goto fail;

	/* support all relevant hardware speeds... we expect that when
	 * hardware is dual speed, all bulk-capable endpoints work at
	 * both speeds
	 */
	if (gadget_is_dualspeed(c->cdev->gadget)) {
		ecm_qc_hs_in_desc.bEndpointAddress =
				ecm_qc_fs_in_desc.bEndpointAddress;
		ecm_qc_hs_out_desc.bEndpointAddress =
				ecm_qc_fs_out_desc.bEndpointAddress;
		ecm_qc_hs_notify_desc.bEndpointAddress =
				ecm_qc_fs_notify_desc.bEndpointAddress;

		/* copy descriptors, and track endpoint copies */
		f->hs_descriptors = usb_copy_descriptors(ecm_qc_hs_function);
		if (!f->hs_descriptors)
			goto fail;
	}

	/* NOTE:  all that is done without knowing or caring about
	 * the network link ... which is unavailable to this code
	 * until we're activated via set_alt().
	 */

	ecm->port.open = ecm_qc_open;
	ecm->port.close = ecm_qc_close;

	DBG(cdev, "CDC Ethernet: %s speed IN/%s OUT/%s NOTIFY/%s\n",
			gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full",
			ecm->port.in_ep->name, ecm->port.out_ep->name,
			ecm->notify->name);
	return 0;

fail:
	if (f->descriptors)
		usb_free_descriptors(f->descriptors);

	if (ecm->notify_req) {
		kfree(ecm->notify_req->buf);
		usb_ep_free_request(ecm->notify, ecm->notify_req);
	}

	/* we might as well release our claims on endpoints */
	if (ecm->notify)
		ecm->notify->driver_data = NULL;
	if (ecm->port.out_ep->desc)
		ecm->port.out_ep->driver_data = NULL;
	if (ecm->port.in_ep->desc)
		ecm->port.in_ep->driver_data = NULL;

	pr_err("%s: can't bind, err %d\n", f->name, status);

	return status;
}

static void
ecm_qc_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_ecm_qc		*ecm = func_to_ecm_qc(f);

	DBG(c->cdev, "ecm unbind\n");

	if (gadget_is_dualspeed(c->cdev->gadget))
		usb_free_descriptors(f->hs_descriptors);
	usb_free_descriptors(f->descriptors);

	kfree(ecm->notify_req->buf);
	usb_ep_free_request(ecm->notify, ecm->notify_req);

	ecm_qc_string_defs[1].s = NULL;

	if (ecm->xport == USB_GADGET_XPORT_BAM2BAM_IPA)
		ecm_ipa_cleanup(ipa_params.private);

	kfree(ecm);
}

/**
 * ecm_qc_bind_config - add CDC Ethernet network link to a configuration
 * @c: the configuration to support the network link
 * @ethaddr: a buffer in which the ethernet address of the host side
 *	side of the link was recorded
 * @xport_name: data path transport type name ("BAM2BAM" or "BAM2BAM_IPA")
 * Context: single threaded during gadget setup
 *
 * Returns zero on success, else negative errno.
 *
 * Caller must have called @gether_qc_setup().  Caller is also responsible
 * for calling @gether_cleanup() before module unload.
 */
int
ecm_qc_bind_config(struct usb_configuration *c, u8 ethaddr[ETH_ALEN],
	char *xport_name)
{
	struct f_ecm_qc		*ecm;
	int		status;

	if (!can_support_ecm(c->cdev->gadget) || !ethaddr)
		return -EINVAL;

	status = ecm_qc_bam_setup();
	if (status) {
		pr_err("bam setup failed");
		return status;
	}

	pr_debug("data transport type is %s", xport_name);

	/* maybe allocate device-global string IDs */
	if (ecm_qc_string_defs[0].id == 0) {

		/* control interface label */
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		ecm_qc_string_defs[0].id = status;
		ecm_qc_control_intf.iInterface = status;

		/* data interface label */
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		ecm_qc_string_defs[2].id = status;
		ecm_qc_data_intf.iInterface = status;

		/* MAC address */
		status = usb_string_id(c->cdev);
		if (status < 0)
			return status;
		ecm_qc_string_defs[1].id = status;
		ecm_qc_desc.iMACAddress = status;
	}

	/* allocate and initialize one new instance */
	ecm = kzalloc(sizeof *ecm, GFP_KERNEL);
	if (!ecm)
		return -ENOMEM;

	ecm->xport = str_to_xport(xport_name);
	pr_debug("set xport = %d", ecm->xport);

	/* export host's Ethernet address in CDC format */
	if (ecm->xport == USB_GADGET_XPORT_BAM2BAM_IPA) {
		gether_qc_get_macs(ipa_params.device_ethaddr,
				ipa_params.host_ethaddr);
		snprintf(ecm->ethaddr, sizeof ecm->ethaddr,
		"%02X%02X%02X%02X%02X%02X",
		ipa_params.host_ethaddr[0], ipa_params.host_ethaddr[1],
		ipa_params.host_ethaddr[2], ipa_params.host_ethaddr[3],
		ipa_params.host_ethaddr[4], ipa_params.host_ethaddr[5]);
	} else
		snprintf(ecm->ethaddr, sizeof ecm->ethaddr,
		"%02X%02X%02X%02X%02X%02X",
		ethaddr[0], ethaddr[1], ethaddr[2],
		ethaddr[3], ethaddr[4], ethaddr[5]);

	ecm_qc_string_defs[1].s = ecm->ethaddr;

	ecm->port.cdc_filter = DEFAULT_FILTER;

	ecm->port.func.name = "cdc_ethernet";
	ecm->port.func.strings = ecm_qc_strings;
	/* descriptors are per-instance copies */
	ecm->port.func.bind = ecm_qc_bind;
	ecm->port.func.unbind = ecm_qc_unbind;
	ecm->port.func.set_alt = ecm_qc_set_alt;
	ecm->port.func.get_alt = ecm_qc_get_alt;
	ecm->port.func.setup = ecm_qc_setup;
	ecm->port.func.disable = ecm_qc_disable;
	ecm->port.func.suspend = ecm_qc_suspend;
	ecm->port.func.resume = ecm_qc_resume;

	status = usb_add_function(c, &ecm->port.func);
	if (status) {
		pr_err("failed to add function");
		ecm_qc_string_defs[1].s = NULL;
		kfree(ecm);
		return status;
	}

	if (ecm->xport != USB_GADGET_XPORT_BAM2BAM_IPA)
		return status;

	pr_debug("setting ecm_ipa, host_ethaddr=%pM, device_ethaddr=%pM",
			ipa_params.host_ethaddr, ipa_params.device_ethaddr);
	status = ecm_ipa_init(&ipa_params);
	if (status) {
		pr_err("failed to initialize ecm_ipa");
		ecm_qc_string_defs[1].s = NULL;
		kfree(ecm);
	} else {
		pr_debug("ecm_ipa successful created");
	}

	return status;
}
