/*
 * ether_cdc_ecm.c -- Ethernet Function driver, with CDC
 *
 * Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 *
 * This file has been derived from gadget/ether.c
 *
 * Copyright (C) 2003-2005 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

/* #define VERBOSE_DEBUG */

#include <linux/kernel.h>
#include <linux/utsname.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>

#include <linux/usb/ch9.h>
#include <linux/usb/cdc.h>

#include "usb_function.h"

/*-------------------------------------------------------------------------*/

/*
 * Ethernet function driver -- with CDC options
 * Builds on hardware support for a full duplex link.
 *
 * CDC Ethernet is the standard USB solution for sending Ethernet frames
 * using USB.  Real hardware tends to use the same framing protocol but look
 * different for control features.  This driver strongly prefers to use
 * this USB-IF standard as its open-systems interoperability solution;
 * most host side USB stacks (except from Microsoft) support it.
 */

#define DRIVER_DESC		"Ethernet Function CDC ECM"
#define DRIVER_VERSION		"1.0"

static const char shortname[] = "ether";
static const char driver_desc[] = DRIVER_DESC;

static unsigned int string_data;
static unsigned int string_control;
static unsigned int string_ethaddr;
#define RX_EXTRA	20		/* guard against rx overflows */



/* outgoing packet filters. */
#define	DEFAULT_FILTER	(USB_CDC_PACKET_TYPE_BROADCAST \
			| USB_CDC_PACKET_TYPE_ALL_MULTICAST \
			| USB_CDC_PACKET_TYPE_PROMISCUOUS \
			| USB_CDC_PACKET_TYPE_DIRECTED)

/*-------------------------------------------------------------------------*/

struct eth_dev {
	spinlock_t		lock;
	struct usb_request	*req;		/* for control responses */
	struct usb_request	*stat_req;	/* for cdc status */

	unsigned		configured:1;
	struct usb_endpoint	*in_ep, *out_ep, *status_ep;

	spinlock_t		req_lock;
	struct list_head	tx_reqs, rx_reqs;

	struct net_device	*net;
	struct net_device_stats	stats;
	atomic_t		tx_qlen;

	struct work_struct	work;
	unsigned		zlp:1;
	unsigned		suspended:1;
	u16			cdc_filter;
	unsigned long		todo;
#define	WORK_RX_MEMORY		0
	u8			host_mac[ETH_ALEN];

	int alt_set;
};

static struct usb_function usb_func_ether;

/* Ethernet function descriptors */
#define USB_DT_IAD_SIZE		8
struct usb_interface_assoc_descriptor	eth_IAD = {
	.bLength           = USB_DT_IAD_SIZE,
	.bDescriptorType   = USB_DT_INTERFACE_ASSOCIATION,
	.bInterfaceCount   = 2,
	.bFunctionClass    = USB_CLASS_COMM,
	.bFunctionSubClass = USB_CDC_SUBCLASS_ETHERNET,
	.bFunctionProtocol = USB_CDC_PROTO_NONE,
	.iFunction         = 0,
};

struct usb_interface_descriptor		eth_control_intf = {
	.bLength =  USB_DT_INTERFACE_SIZE,
	.bDescriptorType =	USB_DT_INTERFACE,
	.bNumEndpoints =	1,
	.bInterfaceClass =	USB_CLASS_COMM,
	.bInterfaceSubClass =	USB_CDC_SUBCLASS_ETHERNET,
	.bInterfaceProtocol =	USB_CDC_PROTO_NONE,
};

struct usb_cdc_header_desc		eth_header_desc = {
	.bLength =		sizeof(struct usb_cdc_header_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_HEADER_TYPE,
	.bcdCDC =		__constant_cpu_to_le16(0x0110),
};

struct usb_cdc_union_desc		eth_union_desc = {
	.bLength =		sizeof(struct usb_cdc_union_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_UNION_TYPE,
};

struct usb_cdc_ether_desc 		eth_ether_desc = {
	.bLength =		sizeof(struct usb_cdc_ether_desc),
	.bDescriptorType =	USB_DT_CS_INTERFACE,
	.bDescriptorSubType =	USB_CDC_ETHERNET_TYPE,
	/* this descriptor actually adds value, surprise! */
	.bmEthernetStatistics =	__constant_cpu_to_le32(0), /* no statistics */
	.wMaxSegmentSize =	__constant_cpu_to_le16(ETH_FRAME_LEN),
	.wNumberMCFilters =	__constant_cpu_to_le16(0),
	.bNumberPowerFilters =	0,
};

struct usb_endpoint_descriptor 		eth_control_intf_hs_int_in_ep_desc = {
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_INT,
	.bInterval =           4,
	.wMaxPacketSize =       64,
};

struct usb_endpoint_descriptor 		eth_control_intf_fs_int_in_ep_desc = {
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_INT,
	.bInterval =           4,
	.wMaxPacketSize =       64,
};

struct usb_interface_descriptor 	eth_data_alt_zero_intf = {
	.bLength =  USB_DT_INTERFACE_SIZE,
	.bDescriptorType =      USB_DT_INTERFACE,
	.bAlternateSetting =    0,
	.bNumEndpoints =        0,
	.bInterfaceClass =      USB_CLASS_CDC_DATA,
	.bInterfaceSubClass =   0,
	.bInterfaceProtocol =   0,
};

struct usb_interface_descriptor 	eth_data_alt_one_intf = {
	.bLength =              USB_DT_INTERFACE_SIZE,
	.bDescriptorType =      USB_DT_INTERFACE,
	.bAlternateSetting =    1,
	.bNumEndpoints =        2,
	.bInterfaceClass =      USB_CLASS_CDC_DATA ,
	.bInterfaceSubClass =   0,
	.bInterfaceProtocol =   USB_CDC_PROTO_NONE,
};

struct usb_endpoint_descriptor 		eth_data_intf_hs_bulk_out_ep_desc = {
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bEndpointAddress =     USB_DIR_OUT,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =       __constant_cpu_to_le16(512),
};

struct usb_endpoint_descriptor 		eth_data_intf_fs_bulk_out_ep_desc = {
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bEndpointAddress =     USB_DIR_OUT,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =       __constant_cpu_to_le16(64),
};

struct usb_endpoint_descriptor 		eth_data_intf_hs_bulk_in_ep_desc = {
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =       __constant_cpu_to_le16(512),
};

struct usb_endpoint_descriptor 		eth_data_intf_fs_bulk_in_ep_desc = {
	.bDescriptorType =      USB_DT_ENDPOINT,
	.bLength =              USB_DT_ENDPOINT_SIZE,
	.bEndpointAddress =     USB_DIR_IN,
	.bmAttributes =         USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =       __constant_cpu_to_le16(64),
};

struct eth_dev *eth_device;

/* Some systems will want different product identifers published in the
 * device descriptor, either numbers or strings or both.  These string
 * parameters are in UTF-8 (superset of ASCII's 7 bit characters).
 */


/* initial value, changed by "ifconfig usb0 hw ether xx:xx:xx:xx:xx:xx" */
static char *dev_addr;
module_param(dev_addr, charp, S_IRUGO);
MODULE_PARM_DESC(dev_addr, "Device Ethernet Address");

/* this address is invisible to ifconfig */
static char *host_addr;
module_param(host_addr, charp, S_IRUGO);
MODULE_PARM_DESC(host_addr, "Host Ethernet Address");

static char ethaddr[2 * ETH_ALEN + 1];
static int eth_bound;

#define DEFAULT_QLEN	2	/* double buffering by default */

/* peak bulk transfer bits-per-second */
#define	HS_BPS		(13 * 512 * 8 * 1000 * 8)

/* for dual-speed hardware, use deeper queues at highspeed */
#define qlen (DEFAULT_QLEN * 5) /* High Speed */

/*-------------------------------------------------------------------------*/

#define xprintk(d, level, fmt, args...) \
	printk(level "%s: " fmt, (d)->net->name, ## args)

#ifdef DEBUG
#undef DEBUG
#define DEBUG(dev, fmt, args...) \
	xprintk(dev, KERN_DEBUG, fmt, ## args)
#else
#define DEBUG(dev, fmt, args...) \
	do { } while (0)
#endif /* DEBUG */

#ifdef VERBOSE_DEBUG
#define VDEBUG	DEBUG
#else
#define VDEBUG(dev, fmt, args...) \
	do { } while (0)
#endif /* DEBUG */

#define ERROR(dev, fmt, args...) \
	xprintk(dev, KERN_ERR, fmt, ## args)
#ifdef WARN
#undef WARN
#endif
#define WARN(dev, fmt, args...) \
	xprintk(dev, KERN_WARNING, fmt, ## args)
#define INFO(dev, fmt, args...) \
	xprintk(dev, KERN_INFO, fmt, ## args)

/*-------------------------------------------------------------------------*/

/* include the status endpoint if we can, even where it's optional.
 * use wMaxPacketSize big enough to fit CDC_NOTIFY_SPEED_CHANGE in one
 * packet, to simplify cancellation; and a big transfer interval, to
 * waste less bandwidth.
 *
 * some drivers (like Linux 2.4 cdc-ether!) "need" it to exist even
 * if they ignore the connect/disconnect notifications that real ether
 * can provide.  more advanced cdc configurations might want to support
 * encapsulated commands (vendor-specific, using control-OUT).
 */
#define STATUS_BYTECOUNT		16	/* 8 byte header + data */


static void eth_start(struct eth_dev *dev, gfp_t gfp_flags);
static int alloc_requests(struct eth_dev *dev, unsigned n, gfp_t gfp_flags);

static int set_ether_config(struct eth_dev *dev, gfp_t gfp_flags)
{
	int result = 0;

	if (dev->status_ep)
		usb_ept_enable(dev->status_ep, 1);

	result = alloc_requests(dev, qlen , gfp_flags);
	if (result == 0)
		DEBUG(dev, "qlen %d\n", qlen);

	/* caller is responsible for cleanup on error */
	return result;
}

static void eth_reset_config(struct eth_dev *dev)
{
	struct usb_request	*req;
	unsigned long  flags;

	DEBUG(dev, "%s\n", __func__);

	if (!dev)
		return;
	if (!dev->net)
		return;

	if (dev->configured == 0)
		return;
	netif_stop_queue(dev->net);
	netif_carrier_off(dev->net);

	/* disable endpoints, forcing (synchronous) completion of
	 * pending i/o.  then free the requests.
	 */
	if (dev->in_ep) {
		usb_ept_enable(dev->in_ep, 0);
		spin_lock_irqsave(&dev->req_lock, flags);
		while (likely(!list_empty(&dev->tx_reqs))) {
			req = container_of(dev->tx_reqs.next,
						struct usb_request, list);
			list_del(&req->list);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			usb_ept_free_req(dev->in_ep, req);
			spin_lock_irqsave(&dev->req_lock, flags);
		}
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}
	if (dev->out_ep) {
		usb_ept_enable(dev->out_ep, 0);
		spin_lock_irqsave(&dev->req_lock, flags);
		while (likely(!list_empty(&dev->rx_reqs))) {
			req = container_of(dev->rx_reqs.next,
						struct usb_request, list);
			list_del(&req->list);
			spin_unlock_irqrestore(&dev->req_lock, flags);
			usb_ept_free_req(dev->out_ep, req);
			spin_lock_irqsave(&dev->req_lock, flags);
		}
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}

	if (dev->status_ep)
		usb_ept_free_req(dev->status_ep, 0);
	dev->cdc_filter = 0;
	dev->configured = 0;
}

/* change our operational config.  must agree with the code
 * that returns config descriptors, and altsetting code.
 */
static int eth_set_config(struct eth_dev *dev,  gfp_t gfp_flags)
{
	int result = 0;

	eth_reset_config(dev);
	result = set_ether_config(dev, gfp_flags);
	if (result)
		eth_reset_config(dev);
	else
		dev->configured = 1;
	return result;
}

static void eth_configure(int configured, void *_ctxt)
{
	int                     result = 0;
	struct eth_dev *dev = (struct eth_dev *) _ctxt;
	if (!dev)
		return ;
	if (!eth_bound)
		return;

	if (!configured) {
		eth_reset_config(dev);
		return ;
	}
	if (dev->configured == 1)
		return ;
	if (usb_msm_get_speed() == USB_SPEED_HIGH) {
		usb_configure_endpoint(dev->status_ep,
					&eth_control_intf_hs_int_in_ep_desc);
		usb_configure_endpoint(dev->in_ep,
					&eth_data_intf_hs_bulk_in_ep_desc);
		usb_configure_endpoint(dev->out_ep,
					&eth_data_intf_hs_bulk_out_ep_desc);
	} else {
		usb_configure_endpoint(dev->status_ep,
					&eth_control_intf_fs_int_in_ep_desc);
		usb_configure_endpoint(dev->in_ep,
					&eth_data_intf_fs_bulk_in_ep_desc);
		usb_configure_endpoint(dev->out_ep,
					&eth_data_intf_fs_bulk_out_ep_desc);
	}
	result = eth_set_config(dev, GFP_ATOMIC);
}
/* The interrupt endpoint is used in CDC networking models (Ethernet, ATM)
 * only to notify the host about link status changes (which we support)
 * Since we want this CDC Ethernet code to be vendor-neutral, only one
 * status request is ever queued.
 */

static void
eth_status_complete(struct usb_endpoint *ep, struct usb_request *req)
{
	struct usb_cdc_notification	*event = req->buf;
	int				value = req->status;

	/* issue the second notification if host reads the first */
	if (event->bNotificationType == USB_CDC_NOTIFY_NETWORK_CONNECTION
			&& value == 0) {
		__le32	*data = req->buf + sizeof *event;

		event->bmRequestType = 0xA1;
		event->bNotificationType = USB_CDC_NOTIFY_SPEED_CHANGE;
		event->wValue = __constant_cpu_to_le16(0);
		event->wIndex =	__constant_cpu_to_le16(
				eth_data_alt_one_intf.bInterfaceNumber);
		event->wLength = __constant_cpu_to_le16(8);

		/* SPEED_CHANGE data is up/down speeds in bits/sec */
		data[0] = data[1] = cpu_to_le32(HS_BPS);

		req->length = STATUS_BYTECOUNT;
		value = usb_ept_queue_xfer(ep, req);
		DEBUG(dev, "send SPEED_CHANGE --> %d\n", value);
		if (value == 0)
			return;
	} else if (value != -ECONNRESET)
		DEBUG(dev, "event %02x --> %d\n",
			event->bNotificationType, value);
	req->context = NULL;
}

static void issue_start_status(struct eth_dev *dev)
{
	struct usb_request		*req = dev->stat_req;
	struct usb_cdc_notification	*event;
	int				value;

	DEBUG(dev, "%s, flush old status first\n", __func__);

	/* flush old status
	 *
	 * FIXME ugly idiom, maybe we'd be better with just
	 * a "cancel the whole queue" primitive since any
	 * unlink-one primitive has way too many error modes.
	 * here, we "know" toggle is already clear...
	 *
	 * FIXME iff req->context != null just dequeue it
	 */
	usb_ept_enable(dev->status_ep,  0);
	usb_ept_enable(dev->status_ep, 1);

	/* 3.8.1 says to issue first NETWORK_CONNECTION, then
	 * a SPEED_CHANGE.  could be useful in some configs.
	 */
	event = req->buf;
	event->bmRequestType = 0xA1;
	event->bNotificationType = USB_CDC_NOTIFY_NETWORK_CONNECTION;
	event->wValue = __constant_cpu_to_le16(1);	/* connected */
	event->wIndex = __constant_cpu_to_le16(
				eth_data_alt_one_intf.bInterfaceNumber);
	event->wLength = 0;

	req->length = sizeof *event;
	req->complete = eth_status_complete;
	req->context = dev;

	value = usb_ept_queue_xfer(dev->status_ep, req);
	if (value < 0)
		DEBUG(dev, "status buf queue --> %d\n", value);
}

static int  eth_set_interface(int  wIndex, int wValue, void *_ctxt)
{
	struct eth_dev *dev = eth_device;
	unsigned long		flags;

	if (dev == NULL)
		return 1;

	if ((wIndex == eth_data_alt_one_intf.bInterfaceNumber)
			&& (wValue == 1)) {
		dev->alt_set = 1;
		usb_ept_enable(dev->in_ep, 1);
		usb_ept_enable(dev->out_ep, 1);
		dev->cdc_filter = DEFAULT_FILTER;
		netif_carrier_on(dev->net);
		issue_start_status(dev);
		if (netif_running(dev->net)) {
			spin_lock_irqsave(&dev->lock, flags);
			eth_start(dev, GFP_ATOMIC);
			spin_unlock_irqrestore(&dev->lock, flags);
		}
	} else {
		dev->alt_set = 0;
		netif_stop_queue(dev->net);
		netif_carrier_off(dev->net);
	}
	return 0;
}

static int eth_get_interface(int wIndex, void *_ctxt)
{
	struct eth_dev *dev = eth_device;

	return dev->alt_set;
}

/*
 * The setup() callback implements all the ep0 functionality that's not
 * handled lower down.  CDC has a number of less-common features:
 *
 *  - class-specific descriptors for the control interface
 *  - class-specific control requests
 */
static int
eth_setup(struct usb_ctrlrequest *ctrl, void *buf, int len, void *_ctxt)
{
	struct eth_dev	*dev = (struct eth_dev *) _ctxt;
	int		value = -EOPNOTSUPP;
	u16		wIndex = le16_to_cpu(ctrl->wIndex);
	u16		wValue = le16_to_cpu(ctrl->wValue);
	u16		wLength = le16_to_cpu(ctrl->wLength);
	u16		data_int = eth_data_alt_one_intf.bInterfaceNumber;
	u16		ctrl_int = eth_control_intf.bInterfaceNumber;
	switch (ctrl->bRequest) {
	case USB_CDC_SET_ETHERNET_PACKET_FILTER:
		/* see 6.2.30: no data, wIndex = interface,
		 * wValue = packet filter bitmap
		 */
		if (ctrl->bRequestType != (USB_TYPE_CLASS|USB_RECIP_INTERFACE)
			|| wLength != 0
			|| ((wIndex != data_int) && (wIndex != ctrl_int)))
			break;
		DEBUG(dev, "packet filter %02x\n", wValue);
		dev->cdc_filter = wValue;
		value = 0;
		break;

	/* and potentially:
	 * case USB_CDC_SET_ETHERNET_MULTICAST_FILTERS:
	 * case USB_CDC_SET_ETHERNET_PM_PATTERN_FILTER:
	 * case USB_CDC_GET_ETHERNET_PM_PATTERN_FILTER:
	 * case USB_CDC_GET_ETHERNET_STATISTIC:
	 */

	default:
		VDEBUG(dev,
			"unknown control req%02x.%02x v%04x i%04x l%d\n",
			ctrl->bRequestType, ctrl->bRequest,
			wValue, wIndex, wLength);
	}
	return value;
}


static void eth_disconnect(void *_ctxt)
{
	struct eth_dev		*dev = (struct eth_dev *) _ctxt;
	unsigned long		flags;

	printk(KERN_INFO "eth_disconnect()\n");
	spin_lock_irqsave(&dev->lock, flags);
	netif_stop_queue(dev->net);
	netif_carrier_off(dev->net);
	eth_reset_config(dev);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/*-------------------------------------------------------------------------*/

/* NETWORK DRIVER HOOKUP (to the layer above this driver) */

static int usb_eth_change_mtu(struct net_device *net, int new_mtu)
{
	struct eth_dev	*dev = netdev_priv(net);

	if (new_mtu <= ETH_HLEN || new_mtu > ETH_FRAME_LEN)
		return -ERANGE;
	/* no zero-length packet read wanted after mtu-sized packets */
	if (((new_mtu + sizeof(struct ethhdr)) %
			(usb_ept_get_max_packet(dev->in_ep))) == 0)
		return -EDOM;
	net->mtu = new_mtu;
	return 0;
}

static struct net_device_stats *eth_get_stats(struct net_device *net)
{
	return &((struct eth_dev *)netdev_priv(net))->stats;
}

static void eth_get_drvinfo(struct net_device *net, struct ethtool_drvinfo *p)
{
	strlcpy(p->driver, shortname, sizeof p->driver);
	strlcpy(p->version, DRIVER_VERSION, sizeof p->version);
	strlcpy(p->fw_version, "ethernet", sizeof p->fw_version);
}

static u32 eth_get_link(struct net_device *net)
{
	return 1;
}

static struct ethtool_ops ops = {
	.get_drvinfo = eth_get_drvinfo,
	.get_link = eth_get_link
};

static void defer_kevent(struct eth_dev *dev, int flag)
{
	if (test_and_set_bit(flag, &dev->todo))
		return;
	if (!schedule_work(&dev->work))
		ERROR(dev, "kevent %d may have been dropped\n", flag);
	else
		DEBUG(dev, "kevent %d scheduled\n", flag);
}

static void rx_complete(struct usb_endpoint *ep, struct usb_request *req);

static int
rx_submit(struct eth_dev *dev, struct usb_request *req, gfp_t gfp_flags)
{
	struct sk_buff		*skb;
	int			retval = -ENOMEM;
	size_t			size;
	unsigned long		flags;
	/* Padding up to RX_EXTRA handles minor disagreements with host.
	 * Normally we use the USB "terminate on short read" convention;
	 * so allow up to (N*max_pkt), since that memory is normally
	 * already allocated.  Some hardware doesn't deal well with short
	 * reads (e.g. DMA must be N*max_pkt), so for now don't trim a
	 * byte off the end (to force hardware errors on overflow).
	 */
	size = (sizeof(struct ethhdr) + dev->net->mtu + RX_EXTRA);
	size += usb_ept_get_max_packet(dev->out_ep) - 1;
	size -= size % usb_ept_get_max_packet(dev->out_ep);
	skb = alloc_skb(size + NET_IP_ALIGN, gfp_flags);
	if (skb  == NULL) {
		DEBUG(dev, "no rx skb\n");
		goto enomem;
	}

	/* Some platforms perform better when IP packets are aligned,
	 * but on at least one, checksumming fails otherwise.
	 */
	skb_reserve(skb, NET_IP_ALIGN);

	req->buf = skb->data;
	req->length = size;
	req->complete = rx_complete;
	req->context = skb;

	retval = usb_ept_queue_xfer(dev->out_ep, req);
	if (retval == -ENOMEM)
enomem:
		defer_kevent(dev, WORK_RX_MEMORY);
	if (retval) {
		DEBUG(dev, "rx submit --> %d\n", retval);
		if (skb)
			dev_kfree_skb_any(skb);
		spin_lock_irqsave(&dev->req_lock, flags);
		list_add(&req->list, &dev->rx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}
	return retval;
}

static void rx_complete(struct usb_endpoint *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct eth_dev	*dev = eth_device;
	int		status = req->status;
	switch (status) {

	/* normal completion */
	case 0:
		skb_put(skb, req->actual);
		/* we know MaxPacketsPerTransfer == 1 here */
		if (status < 0
				|| ETH_HLEN > skb->len
				|| skb->len > ETH_FRAME_LEN) {
			dev->stats.rx_errors++;
			dev->stats.rx_length_errors++;
			DEBUG(dev, "rx length %d\n", skb->len);
			break;
		}

		skb->protocol = eth_type_trans(skb, dev->net);
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += skb->len;

		/* no buffer copies needed, unless hardware can't
		 * use skb buffers.
		 */
		status = netif_rx(skb);
		skb = NULL;
		break;

	/* software-driven interface shutdown */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		VDEBUG(dev, "rx shutdown, code %d\n", status);
		goto quiesce;

	/* for hardware automagic (such as pxa) */
	case -ECONNABORTED:		/* endpoint reset */
		DEBUG(dev, "rx %s reset\n", ep->name);
		defer_kevent(dev, WORK_RX_MEMORY);
quiesce:
		dev_kfree_skb_any(skb);
		goto clean;

	/* data overrun */
	case -EOVERFLOW:
		dev->stats.rx_over_errors++;
		/* FALLTHROUGH */

	default:
		dev->stats.rx_errors++;
		DEBUG(dev, "rx status %d\n", status);
		break;
	}

	if (skb)
		dev_kfree_skb_any(skb);
	if (!netif_running(dev->net)) {
clean:
		spin_lock(&dev->req_lock);
		list_add(&req->list, &dev->rx_reqs);
		spin_unlock(&dev->req_lock);
		req = NULL;
	}
	if (req)
		rx_submit(dev, req, GFP_ATOMIC);
}

static int prealloc(struct list_head *list, struct usb_endpoint *ep,
			unsigned n, gfp_t gfp_flags)
{
	unsigned		i;
	struct usb_request	*req;

	if (!n)
		return -ENOMEM;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}
	while (i--) {
		/* CDC ECM uses skb buffer pointer for requests */
		req = usb_ept_alloc_req(ep, 0);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;
		list_add(&req->list, list);
	}
	return 0;

extra:
	/* free extras */
	for (;;) {
		struct list_head	*next;

		next = req->list.next;
		list_del(&req->list);
		usb_ept_free_req(ep, req);

		if (next == list)
			break;

		req = container_of(next, struct usb_request, list);
	}
	return 0;
}

static int alloc_requests(struct eth_dev *dev, unsigned n, gfp_t gfp_flags)
{
	int status;
	unsigned long flags;

	spin_lock_irqsave(&dev->req_lock, flags);
	status = prealloc(&dev->tx_reqs, dev->in_ep, n, gfp_flags);
	if (status < 0)
		goto fail;
	status = prealloc(&dev->rx_reqs, dev->out_ep, n, gfp_flags);
	if (status < 0)
		goto fail;
	goto done;
fail:
	DEBUG(dev, "can't alloc requests\n");
done:
	spin_unlock_irqrestore(&dev->req_lock, flags);
	return status;
}

static void rx_fill(struct eth_dev *dev, gfp_t gfp_flags)
{
	struct usb_request	*req;
	unsigned long		flags;
	/* fill unused rxq slots with some skb */
	spin_lock_irqsave(&dev->req_lock, flags);
	while (!list_empty(&dev->rx_reqs)) {
		req = container_of(dev->rx_reqs.next,
				struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&dev->req_lock, flags);

		if (rx_submit(dev, req, gfp_flags) < 0) {
			defer_kevent(dev, WORK_RX_MEMORY);
			return;
		}

		spin_lock_irqsave(&dev->req_lock, flags);
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);
}

static void eth_work(struct work_struct *work)
{
	struct eth_dev	*dev = container_of(work, struct eth_dev, work);

	if (test_and_clear_bit(WORK_RX_MEMORY, &dev->todo)) {
		if (netif_running(dev->net))
			rx_fill(dev, GFP_KERNEL);
	}

	if (dev->todo)
		DEBUG(dev, "work done, flags = 0x%lx\n", dev->todo);
}

static void tx_complete(struct usb_endpoint *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct eth_dev	*dev = eth_device;

	switch (req->status) {
	default:
		dev->stats.tx_errors++;
		VDEBUG(dev, "tx err %d\n", req->status);
		/* FALLTHROUGH */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		break;
	case 0:
		dev->stats.tx_bytes += skb->len;
	}
	dev->stats.tx_packets++;

	spin_lock(&dev->req_lock);
	list_add(&req->list, &dev->tx_reqs);
	spin_unlock(&dev->req_lock);
	dev_kfree_skb_any(skb);

	atomic_dec(&dev->tx_qlen);
	if (netif_carrier_ok(dev->net))
		netif_wake_queue(dev->net);
}

static inline int eth_is_promisc(struct eth_dev *dev)
{
	return dev->cdc_filter & USB_CDC_PACKET_TYPE_PROMISCUOUS;
}

static int eth_start_xmit(struct sk_buff *skb, struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);
	int			length = skb->len;
	int			retval;
	struct usb_request	*req = NULL;
	unsigned long		flags;

	/* apply outgoing CDC filters */
	if (!eth_is_promisc(dev)) {
		u8		*dest = skb->data;

		if (is_multicast_ether_addr(dest)) {
			u16	type;

			/* ignores USB_CDC_PACKET_TYPE_MULTICAST and host
			 * SET_ETHERNET_MULTICAST_FILTERS requests
			 */
			if (is_broadcast_ether_addr(dest))
				type = USB_CDC_PACKET_TYPE_BROADCAST;
			else
				type = USB_CDC_PACKET_TYPE_ALL_MULTICAST;
			if (!(dev->cdc_filter & type)) {
				dev_kfree_skb_any(skb);
				return 0;
			}
		}
		/* ignores USB_CDC_PACKET_TYPE_DIRECTED */
	}

	spin_lock_irqsave(&dev->req_lock, flags);
	/*
	 * this freelist can be empty if an interrupt triggered disconnect()
	 * and reconfigured the function (shutting down this queue) after the
	 * network stack decided to xmit but before we got the spinlock.
	 */
	if (list_empty(&dev->tx_reqs)) {
		spin_unlock_irqrestore(&dev->req_lock, flags);
		return 1;
	}

	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	list_del(&req->list);

	/* temporarily stop TX queue when the freelist empties */
	if (list_empty(&dev->tx_reqs))
		netif_stop_queue(net);
	spin_unlock_irqrestore(&dev->req_lock, flags);

	/* no buffer copies needed, unless the network stack did it
	 * or the hardware can't use skb buffers.
	 */
	req->buf = skb->data;
	req->context = skb;
	req->complete = tx_complete;

	/* use zlp framing on tx for strict CDC-Ether conformance,
	 * though any robust network rx path ignores extra padding.
	 * and some hardware doesn't like to write zlps.
	 */
	if (!dev->zlp && (length % usb_ept_get_max_packet(dev->in_ep)) == 0)
		length++;

	req->length = length;

	retval = usb_ept_queue_xfer(dev->in_ep, req);
	switch (retval) {
	default:
		DEBUG(dev, "tx queue err %d\n", retval);
		break;
	case 0:
		net->trans_start = jiffies;
		atomic_inc(&dev->tx_qlen);
	}
	if (retval) {
		dev->stats.tx_dropped++;
		dev_kfree_skb_any(skb);
		spin_lock_irqsave(&dev->req_lock, flags);
		if (list_empty(&dev->tx_reqs))
			netif_start_queue(net);
		list_add(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}
	return 0;
}


static void eth_start(struct eth_dev *dev, gfp_t gfp_flags)
{
	DEBUG(dev, "%s\n", __func__);

	/* fill the rx queue */
	rx_fill(dev, gfp_flags);

	/* and open the tx floodgates */
	atomic_set(&dev->tx_qlen, 0);
	netif_wake_queue(dev->net);
}

static int eth_open(struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);

	DEBUG(dev, "%s\n", __func__);
	if (netif_carrier_ok(dev->net))
		eth_start(dev, GFP_KERNEL);
	return 0;
}

static int eth_stop(struct net_device *net)
{
	struct eth_dev		*dev = netdev_priv(net);

	VDEBUG(dev, "%s\n", __func__);
	netif_stop_queue(net);

	DEBUG(dev, "stop stats: rx/tx %ld/%ld, errs %ld/%ld\n",
		dev->stats.rx_packets, dev->stats.tx_packets,
		dev->stats.rx_errors, dev->stats.tx_errors
		);

	/* ensure there are no more active requests */
	if (dev->configured) {
		usb_ept_enable(dev->in_ep, 0);
		usb_ept_enable(dev->out_ep, 0);
		if (netif_carrier_ok(dev->net)) {
			DEBUG(dev, "host still using in/out endpoints\n");
			/* FIXME idiom may leave toggle wrong here */
			usb_ept_enable(dev->in_ep, 1);
			usb_ept_enable(dev->out_ep, 1);
		}
		if (dev->status_ep) {
			usb_ept_enable(dev->status_ep, 0);
			usb_ept_enable(dev->status_ep,  1);
		}
	}

	return 0;
}


static u8 __devinit nibble(unsigned char c)
{
	if (likely(isdigit(c)))
		return c - '0';
	c = toupper(c);
	if (likely(isxdigit(c)))
		return 10 + c - 'A';
	return 0;
}

static int __devinit get_ether_addr(const char *str, u8 *dev_addr)
{
	if (str) {
		unsigned	i;

		for (i = 0; i < 6; i++) {
			unsigned char num;

			if ((*str == '.') || (*str == ':'))
				str++;
			num = nibble(*str++) << 4;
			num |= (nibble(*str++));
			dev_addr[i] = num;
		}
		if (is_valid_ether_addr(dev_addr))
			return 0;
	}
	random_ether_addr(dev_addr);
	return 1;
}

static void  eth_unbind(void *_ctxt)
{
	struct eth_dev   *dev = (struct eth_dev *)_ctxt ;

	pr_debug("%s ()\n", __func__);
	if (!dev)
		return ;
	if (!eth_bound)
		return;

	if (dev->in_ep) {
		usb_ept_fifo_flush(dev->in_ep);
		usb_ept_enable(dev->in_ep, 0);
		usb_free_endpoint(dev->in_ep);
	}
	if (dev->out_ep) {
		usb_ept_fifo_flush(dev->out_ep);
		usb_ept_enable(dev->out_ep, 0);
		usb_free_endpoint(dev->out_ep);
	}
	if (dev->status_ep) {
		usb_ept_fifo_flush(dev->status_ep);
		usb_ept_enable(dev->status_ep, 0);
		usb_free_endpoint(dev->status_ep);
	}


	if (dev->net) {
		unregister_netdev(dev->net);
		free_netdev(dev->net);
	}
	eth_bound = 0;
	return ;
}

static void  eth_bind(void *_ctxt)
{
	struct eth_dev		*dev;
	struct net_device	*net;
	u8			zlp = 1;
	struct usb_endpoint     *in_ep, *out_ep, *status_ep = NULL;
	int			status = -ENOMEM;
	int			ret;
	struct device		*get_dev;

	get_dev = usb_get_device();

	ret = usb_msm_get_next_ifc_number(&usb_func_ether);
	eth_control_intf.bInterfaceNumber = ret;
	eth_control_intf.iInterface = string_control;
	eth_IAD.bFirstInterface = ret;
	eth_union_desc.bMasterInterface0 = ret;

	ret = usb_msm_get_next_ifc_number(&usb_func_ether);
	eth_data_alt_zero_intf.bInterfaceNumber = ret;
	eth_data_alt_zero_intf.iInterface = 0;
	eth_data_alt_one_intf.bInterfaceNumber = ret;
	eth_data_alt_one_intf.iInterface = string_data;
	eth_union_desc.bSlaveInterface0 = ret;

	/* Enable IAD */
	usb_msm_enable_iad();

	/* Configuring STATUS endpoint */
	status_ep = usb_alloc_endpoint(USB_DIR_IN);
	status_ep->max_pkt = 64;

	eth_control_intf_hs_int_in_ep_desc.bEndpointAddress =
						USB_DIR_IN | status_ep->num;
	eth_control_intf_hs_int_in_ep_desc.wMaxPacketSize =
						status_ep->max_pkt;
	eth_control_intf_fs_int_in_ep_desc.bEndpointAddress =
						USB_DIR_IN | status_ep->num;
	eth_control_intf_hs_int_in_ep_desc.bInterval = 4;

	/* Configuring OUT endpoint */
	out_ep = usb_alloc_endpoint(USB_DIR_OUT);
	out_ep->max_pkt = 512;
	eth_data_intf_hs_bulk_out_ep_desc.bEndpointAddress =
						USB_DIR_OUT | out_ep->num;
	eth_data_intf_hs_bulk_out_ep_desc.wMaxPacketSize = out_ep->max_pkt;
	eth_data_intf_fs_bulk_out_ep_desc.bEndpointAddress =
						USB_DIR_OUT | out_ep->num;

	/*Configuring IN Endpoint*/
	in_ep = usb_alloc_endpoint(USB_DIR_IN);
	in_ep->max_pkt = 512;
	eth_data_intf_hs_bulk_in_ep_desc.bEndpointAddress =
						USB_DIR_IN | in_ep->num;
	eth_data_intf_hs_bulk_in_ep_desc.wMaxPacketSize = in_ep->max_pkt;
	eth_data_intf_fs_bulk_in_ep_desc.bEndpointAddress =
						USB_DIR_IN | in_ep->num;

	net = alloc_etherdev(sizeof *dev);
	if (!net) {
		printk(KERN_DEBUG "eth_bind: alloc_etherdev failed \n");
		return ;
	}
	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);

	/* network device setup */
	dev->net = net;
	strcpy(net->name, "usb%d");
	dev->zlp = zlp;
	dev->in_ep = in_ep;
	dev->out_ep = out_ep;
	dev->status_ep = status_ep;

	eth_device = dev;
	usb_func_ether.context = eth_device;

	/* Module params for these addresses should come from ID proms.
	 * The host side address is used with CDC, and commonly
	 * ends up in a persistent config database.  It's not clear if
	 * host side code for the SAFE thing cares -- its original BLAN
	 * thing didn't, Sharp never assigned those addresses on Zaurii.
	 */
	if (get_ether_addr(dev_addr, net->dev_addr))
		dev_warn(get_dev,
			"using random %s ethernet address\n", "self");
	if (get_ether_addr(host_addr, dev->host_mac))
		dev_warn(get_dev,
			"using random %s ethernet address\n", "host");
	snprintf(ethaddr, sizeof ethaddr, "%02X%02X%02X%02X%02X%02X",
		dev->host_mac[0], dev->host_mac[1],
		dev->host_mac[2], dev->host_mac[3],
		dev->host_mac[4], dev->host_mac[5]);

	net->change_mtu = usb_eth_change_mtu;
	net->get_stats = eth_get_stats;
	net->hard_start_xmit = eth_start_xmit;
	net->open = eth_open;
	net->stop = eth_stop;
	/* watchdog_timeo, tx_timeout ...
	 * set_multicast_list */
	SET_ETHTOOL_OPS(net, &ops);
	/* ... and maybe likewise for status transfer */
	if (dev->status_ep) {
		dev->stat_req = usb_ept_alloc_req(dev->status_ep,
					STATUS_BYTECOUNT);
		if (!dev->stat_req) {
			usb_ept_free_req(dev->status_ep, dev->req);
			goto fail;
		}
		dev->stat_req->context = NULL;
	}
	/* finish hookup to lower layer ... */
	/* two kinds of host-initiated state changes:
	 *  - iff DATA transfer is active, carrier is "on"
	 *  - tx queueing enabled if open *and* carrier is "on"
	 */
	netif_stop_queue(dev->net);
	netif_carrier_off(dev->net);

	SET_NETDEV_DEV(dev->net, get_dev);
	status = register_netdev(dev->net);
	if (status < 0)
		goto fail1;

	INFO(dev, "%s, version: " DRIVER_VERSION "\n", driver_desc);
	INFO(dev, "MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		net->dev_addr[0], net->dev_addr[1],
		net->dev_addr[2], net->dev_addr[3],
		net->dev_addr[4], net->dev_addr[5]);

	INFO(dev, "HOST MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		dev->host_mac[0], dev->host_mac[1],
		dev->host_mac[2], dev->host_mac[3],
		dev->host_mac[4], dev->host_mac[5]);

	string_data = usb_msm_get_next_strdesc_id("Ethernet Data");
	if (string_data != 0) {
		string_control = usb_msm_get_next_strdesc_id
				 ("CDC Communications Control");
		if (string_control != 0) {
			string_ethaddr = usb_msm_get_next_strdesc_id(ethaddr);
			if (string_ethaddr != 0) {
				eth_ether_desc.iMACAddress = string_ethaddr;
				eth_bound = 1;
				return ;
			}
		}
	}
fail1:
	dev_dbg(get_dev, "register_netdev failed, %d\n", status);
fail:
	eth_bound = 1;
	printk(KERN_INFO"eth_bind: returning from eth_bind\n");
	return ;
}


static struct usb_function usb_func_ether = {
	.name		= "ethernet",
	.bind		= eth_bind,
	.unbind		= eth_unbind,
	.configure	= eth_configure,
	.disconnect	= eth_disconnect,
	.setup		= eth_setup,
	.set_interface	= eth_set_interface,
	.get_interface	= eth_get_interface,
};

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL v2");

#define TOTAL_ETH_DESCRIPTORS 11
struct usb_descriptor_header *eth_hs_descriptors[TOTAL_ETH_DESCRIPTORS];
struct usb_descriptor_header *eth_fs_descriptors[TOTAL_ETH_DESCRIPTORS];

static int __init init(void)
{
	int rc;

	eth_hs_descriptors[0] = (struct usb_descriptor_header *)
				&eth_IAD;
	eth_hs_descriptors[1] = (struct usb_descriptor_header *)
				&eth_control_intf;
	eth_hs_descriptors[2] = (struct usb_descriptor_header *)
				&eth_header_desc;
	eth_hs_descriptors[3] = (struct usb_descriptor_header *)
				&eth_union_desc;
	eth_hs_descriptors[4] = (struct usb_descriptor_header *)
				&eth_ether_desc;
	eth_hs_descriptors[5] = (struct usb_descriptor_header *)
				&eth_control_intf_hs_int_in_ep_desc;
	eth_hs_descriptors[6] = (struct usb_descriptor_header *)
				&eth_data_alt_zero_intf;
	eth_hs_descriptors[7] = (struct usb_descriptor_header *)
				&eth_data_alt_one_intf;
	eth_hs_descriptors[8] = (struct usb_descriptor_header *)
				&eth_data_intf_hs_bulk_out_ep_desc;
	eth_hs_descriptors[9] = (struct usb_descriptor_header *)
				&eth_data_intf_hs_bulk_in_ep_desc;
	eth_hs_descriptors[10] = NULL;

	eth_fs_descriptors[0] = (struct usb_descriptor_header *)&eth_IAD;
	eth_fs_descriptors[1] = (struct usb_descriptor_header *)
				&eth_control_intf;
	eth_fs_descriptors[2] = (struct usb_descriptor_header *)
				&eth_header_desc;
	eth_fs_descriptors[3] = (struct usb_descriptor_header *)&eth_union_desc;
	eth_fs_descriptors[4] = (struct usb_descriptor_header *)&eth_ether_desc;
	eth_fs_descriptors[5] = (struct usb_descriptor_header *)
				&eth_control_intf_fs_int_in_ep_desc;
	eth_fs_descriptors[6] = (struct usb_descriptor_header *)
				&eth_data_alt_zero_intf;
	eth_fs_descriptors[7] = (struct usb_descriptor_header *)
				&eth_data_alt_one_intf;
	eth_fs_descriptors[8] = (struct usb_descriptor_header *)
				&eth_data_intf_fs_bulk_out_ep_desc;
	eth_fs_descriptors[9] = (struct usb_descriptor_header *)
				&eth_data_intf_fs_bulk_in_ep_desc;
	eth_fs_descriptors[10] = NULL;

	usb_func_ether.hs_descriptors = eth_hs_descriptors;
	usb_func_ether.fs_descriptors = eth_fs_descriptors;
	rc = usb_function_register(&usb_func_ether);

	if (rc < 0)
		printk(KERN_INFO "cdcecm init:usb function register failed \n");
	return rc;
}
module_init(init);

static void __exit eth_cleanup(void)
{
	struct eth_dev          *dev = eth_device;

	usb_function_unregister(&usb_func_ether);
	if (dev) {
		dev->net = NULL;
		dev = NULL;
	}
}
module_exit(eth_cleanup);
