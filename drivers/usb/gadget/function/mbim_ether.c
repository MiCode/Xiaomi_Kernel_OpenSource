/*
 * u_ether.c -- Ethernet-over-USB link layer utilities for Gadget stack
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/* #define VERBOSE_DEBUG */
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "["KBUILD_MODNAME"]" fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include "mbim_ether.h"
#include <trace/events/netdev_rx.h>

/*
 * This component encapsulates the Ethernet link glue needed to provide
 * one (!) network link through the USB gadget stack, normally "usb0".
 *
 * The control and data models are handled by the function driver which
 * connects to this code; such as CDC Ethernet (ECM or EEM),
 * "CDC Subset", or RNDIS.  That includes all descriptor and endpoint
 * management.
 *
 * Link level addressing is handled by this component using module
 * parameters; if no such parameters are provided, random link level
 * addresses are used.  Each end of the link uses one address.  The
 * host end address is exported in various ways, and is often recorded
 * in configuration databases.
 *
 * The driver which assembles each configuration using such a link is
 * responsible for ensuring that each configuration includes at most one
 * instance of is network link.  (The network layer provides ways for
 * this single "physical" link to be used by multiple virtual links.)
 */


#define UETH__VERSION	"29-May-2008"

static struct workqueue_struct	*mbim_uether_wq;
static struct workqueue_struct	*mbim_uether_wq1;

struct mbim_eth_dev *mbim_dev;

struct mbim_eth_dev {
	/* lock is held while accessing port_usb
	 */
	spinlock_t		lock;
	struct mbim_gether		*port_usb;

	struct net_device	*net;
	struct usb_gadget	*gadget;

	spinlock_t		req_lock;	/* guard {tx}_reqs */
	spinlock_t		reqrx_lock;	/* guard {rx}_reqs */
	struct list_head	tx_reqs, rx_reqs;
	unsigned		tx_qlen;
/* Minimum number of TX USB request queued to UDC */
#define TX_REQ_THRESHOLD	5
	int			no_tx_req_used;
	int			tx_skb_hold_count;
	u32			tx_req_bufsize;

	struct sk_buff_head	rx_frames;

	unsigned		header_len;
	unsigned int		ul_max_pkts_per_xfer;
	unsigned int		dl_max_pkts_per_xfer;
	struct sk_buff		*(*wrap)(struct mbim_gether *, struct sk_buff *skb, int ifid);
	int			(*unwrap)(struct mbim_gether *,
						struct sk_buff *skb,
						struct sk_buff_head *list);

	struct work_struct	work;
	struct work_struct	rx_work;
	struct work_struct	rx_work1;
	unsigned long		todo;
#define	WORK_RX_MEMORY		0

	bool			zlp;
	u8			host_mac[ETH_ALEN];
};

static void mbim_tx_complete(struct usb_ep *ep, struct usb_request *req)
{
#if 1
	struct sk_buff	*skb;
	struct mbim_eth_dev	*dev;
	struct net_device *net;
	struct usb_request *new_req;
	struct usb_ep *in;
	int length;
	int retval;

	if (!ep->driver_data) {
		usb_ep_free_request(ep, req);
		return;
	}

	dev = ep->driver_data;
	net = dev->net;

	if (!dev->port_usb) {
		usb_ep_free_request(ep, req);
		return;
	}

	switch (req->status) {
	default:
		dev->net->stats.tx_errors++;
		VDBG(dev, "tx err %d\n", req->status);
		/* FALLTHROUGH */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		break;
	case 0:
		if (!req->zero)
			dev->net->stats.tx_bytes += req->length-1;
		else
			dev->net->stats.tx_bytes += req->length;
	}
	dev->net->stats.tx_packets++;
	rndis_test_tx_complete++;

	spin_lock(&dev->req_lock);
	list_add_tail(&req->list, &dev->tx_reqs);

	if (dev->port_usb->multi_pkt_xfer && !req->context) {
		dev->no_tx_req_used--;
		req->length = 0;
		in = dev->port_usb->in_ep;

		if (!list_empty(&dev->tx_reqs)) {
			new_req = container_of(dev->tx_reqs.next,
					struct usb_request, list);
			list_del(&new_req->list);
			spin_unlock(&dev->req_lock);
			if (new_req->length > 0) {
				length = new_req->length;

				/* NCM requires no zlp if transfer is
				 * dwNtbInMaxSize */
				if (dev->port_usb->is_fixed &&
					length == dev->port_usb->fixed_in_len &&
					(length % in->maxpacket) == 0)
					new_req->zero = 0;
				else
					new_req->zero = 1;

				/* use zlp framing on tx for strict CDC-Ether
				 * conformance, though any robust network rx
				 * path ignores extra padding. and some hardware
				 * doesn't like to write zlps.
				 */
				if (new_req->zero && !dev->zlp &&
						(length % in->maxpacket) == 0) {
					new_req->zero = 0;
					length++;
				}

				new_req->length = length;
				retval = usb_ep_queue(in, new_req, GFP_ATOMIC);
				switch (retval) {
				default:
					DBG(dev, "tx queue err %d\n", retval);
					new_req->length = 0;
					spin_lock(&dev->req_lock);
					list_add_tail(&new_req->list,
							&dev->tx_reqs);
					spin_unlock(&dev->req_lock);
					break;
				case 0:
					spin_lock(&dev->req_lock);
					dev->no_tx_req_used++;
					spin_unlock(&dev->req_lock);
				}
			} else {
				spin_lock(&dev->req_lock);
				/*
				 * Put the idle request at the back of the
				 * queue. The xmit function will put the
				 * unfinished request at the beginning of the
				 * queue.
				 */
				list_add_tail(&new_req->list, &dev->tx_reqs);
				spin_unlock(&dev->req_lock);
			}
		} else {
			spin_unlock(&dev->req_lock);
		}
	} else {
		skb = req->context;
		/* Is aggregation already enabled and buffers allocated ? */
		if (dev->port_usb->multi_pkt_xfer && dev->tx_req_bufsize) {
#if defined(CONFIG_64BIT) && defined(CONFIG_MTK_LM_MODE)
			req->buf = kzalloc(dev->tx_req_bufsize, GFP_ATOMIC | GFP_DMA);
#else
			req->buf = kzalloc(dev->tx_req_bufsize, GFP_ATOMIC);
#endif
			req->context = NULL;
		} else {
			req->buf = NULL;
		}

		spin_unlock(&dev->req_lock);
		dev_kfree_skb_any(skb);
	}
#if 0
	if (netif_carrier_ok(dev->net)) {
		spin_lock(&dev->req_lock);
		if (dev->no_tx_req_used < tx_wakeup_threshold)
			netif_wake_queue(dev->net);
		spin_unlock(&dev->req_lock);
	}
#endif
#endif
}

static void mbim_defer_kevent(struct mbim_eth_dev *dev, int flag)
{
	if (test_and_set_bit(flag, &dev->todo))
		return;
	if (!schedule_work(&dev->work))
		ERROR(dev, "kevent %d may have been dropped\n", flag);
	else
		DBG(dev, "kevent %d scheduled\n", flag);
}
static int
mbim_rx_submit(struct mbim_eth_dev *dev, struct usb_request *req, gfp_t gfp_flags)
{
	struct sk_buff	*skb;
	int		retval = -ENOMEM;
	size_t		size = 0;
	struct usb_ep	*out;
	unsigned long	flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb)
		out = dev->port_usb->out_ep;
	else
		out = NULL;
	spin_unlock_irqrestore(&dev->lock, flags);

	if (!out)
		return -ENOTCONN;

	size = 16384;

	pr_debug("rx_fill\n");
	skb = alloc_skb(size + NET_IP_ALIGN, gfp_flags);
	if (skb == NULL) {
		pr_debug("[XLOG_INFO][UTHER]rx_submit : no rx skb\n");
		DBG(dev, "no rx skb\n");
		goto enomem;
	}

	/* Some platforms perform better when IP packets are aligned,
	 * but on at least one, checksumming fails otherwise.  Note:
	 * RNDIS headers involve variable numbers of LE32 values.
	 */
	skb_reserve(skb, NET_IP_ALIGN);

	req->buf = skb->data;
	req->length = size;
	req->context = skb;

	retval = usb_ep_queue(out, req, gfp_flags);
	if (retval == -ENOMEM)
enomem:
		mbim_defer_kevent(dev, WORK_RX_MEMORY);
	if (retval) {
		DBG(dev, "rx submit --> %d\n", retval);
		if (skb)
			dev_kfree_skb_any(skb);
	}
	return retval;
}

/*-------------------------------------------------------------------------*/
static void mbim_rx_fill(struct mbim_eth_dev *dev, gfp_t gfp_flags)
{
	struct usb_request	*req;
	unsigned long		flags;
	int                 req_cnt = 0;

	/* fill unused rxq slots with some skb */
	spin_lock_irqsave(&dev->reqrx_lock, flags);
	while (!list_empty(&dev->rx_reqs)) {
		/* break the nexus of continuous completion and re-submission*/
		if (++req_cnt > 20)
			break;

		req = container_of(dev->rx_reqs.next,
				struct usb_request, list);
		list_del_init(&req->list);
		spin_unlock_irqrestore(&dev->reqrx_lock, flags);

		if (mbim_rx_submit(dev, req, gfp_flags) < 0) {
			spin_lock_irqsave(&dev->reqrx_lock, flags);
			list_add(&req->list, &dev->rx_reqs);
			spin_unlock_irqrestore(&dev->reqrx_lock, flags);
			mbim_defer_kevent(dev, WORK_RX_MEMORY);
			return;
		}

		spin_lock_irqsave(&dev->reqrx_lock, flags);
	}
	spin_unlock_irqrestore(&dev->reqrx_lock, flags);
}



static void process_mbim_rx_w(struct work_struct *work)
{
	struct mbim_eth_dev	*dev = container_of(work, struct mbim_eth_dev, rx_work);
	struct sk_buff	*skb;
	int		status = 0;

	if (!dev->port_usb)
		return;

	while ((skb = skb_dequeue(&dev->rx_frames))) {
		if (status < 0
				|| ETH_HLEN > skb->len
				|| skb->len > VLAN_ETH_FRAME_LEN) {
			dev->net->stats.rx_errors++;
			dev->net->stats.rx_length_errors++;
			rndis_test_rx_error++;
			DBG(dev, "rx length %d\n", skb->len);
			dev_kfree_skb_any(skb);
			continue;
		}
		skb->protocol = eth_type_trans(skb, dev->net);
		dev->net->stats.rx_packets++;
		dev->net->stats.rx_bytes += skb->len;
#ifdef CONFIG_MTK_NET_CCMNI
		status = ccmni_send_mbim_skb(0, skb);
#else
		dev_kfree_skb_any(skb);
#endif
	}
}

static void process_mbim_rx_w1(struct work_struct *work)
{
	struct mbim_eth_dev	*dev = container_of(work, struct mbim_eth_dev, rx_work1);

	if (!dev->port_usb)
		return;

	mbim_rx_fill(dev, GFP_KERNEL);
}

static void mbim_rx_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct sk_buff	*skb = req->context;
	struct mbim_eth_dev	*dev = ep->driver_data;
	int		status = req->status;
	bool    queue = 0;

	switch (status) {

	/* normal completion */
	case 0:
		pr_debug("%s: transferred size: %d", __func__, req->actual);
		skb_put(skb, req->actual);

		if (dev->unwrap) {
			unsigned long	flags;

			spin_lock_irqsave(&dev->lock, flags);
			#if 1
			if (dev->port_usb) {
				status = dev->unwrap(dev->port_usb,
							skb,
							&dev->rx_frames);
				if (status == -EINVAL)
					dev->net->stats.rx_errors++;
				else if (status == -EOVERFLOW)
					dev->net->stats.rx_over_errors++;
			} else {
				dev_kfree_skb_any(skb);
				status = -ENOTCONN;
			}
			#else
			#endif
			spin_unlock_irqrestore(&dev->lock, flags);
		} else {
			skb_queue_tail(&dev->rx_frames, skb);
		}

		if (!status)
			queue = 1;

		rndis_test_rx_usb_in++;
		break;

	/* software-driven interface shutdown */
	case -ECONNRESET:		/* unlink */
	case -ESHUTDOWN:		/* disconnect etc */
		VDBG(dev, "rx shutdown, code %d\n", status);
		goto quiesce;

	/* for hardware automagic (such as pxa) */
	case -ECONNABORTED:		/* endpoint reset */
		DBG(dev, "rx %s reset\n", ep->name);
		mbim_defer_kevent(dev, WORK_RX_MEMORY);
quiesce:
		dev_kfree_skb_any(skb);
		goto clean;

	/* data overrun */
	case -EOVERFLOW:
		dev->net->stats.rx_over_errors++;
		/* FALLTHROUGH */

	default:
		queue = 1;
		dev_kfree_skb_any(skb);
		dev->net->stats.rx_errors++;
		DBG(dev, "rx status %d\n", status);
		break;
	}

clean:
	if (queue && dev->rx_frames.qlen <= 1000) { /*u_ether_rx_pending_thld*/
		if (mbim_rx_submit(dev, req, GFP_ATOMIC) < 0) {
			spin_lock(&dev->reqrx_lock);
			list_add(&req->list, &dev->rx_reqs);
			spin_unlock(&dev->reqrx_lock);
		}
	} else {
		spin_lock(&dev->reqrx_lock);
		list_add(&req->list, &dev->rx_reqs);
		spin_unlock(&dev->reqrx_lock);
	}
	if (queue) {
		queue_work(mbim_uether_wq, &dev->rx_work);
		queue_work(mbim_uether_wq1, &dev->rx_work1);
	}
}

static int alloc_mbim_tx_buffer(struct mbim_eth_dev *dev)
{
	struct list_head	*act;
	struct usb_request	*req;

	dev->tx_req_bufsize = 16384;

	list_for_each(act, &dev->tx_reqs) {
		req = container_of(act, struct usb_request, list);
		if (!req->buf) {
#if defined(CONFIG_64BIT) && defined(CONFIG_MTK_LM_MODE)
			req->buf = kzalloc(dev->tx_req_bufsize,
						GFP_ATOMIC | GFP_DMA);
#else
			req->buf = kzalloc(dev->tx_req_bufsize,
						GFP_ATOMIC);
#endif
			if (!req->buf)
				goto free_buf;
		}
		/* req->context is not used for multi_pkt_xfers */
		req->context = NULL;
	}
	return 0;

free_buf:
	/* tx_req_bufsize = 0 retries mem alloc on next eth_start_xmit */
	dev->tx_req_bufsize = 0;
	list_for_each(act, &dev->tx_reqs) {
		req = container_of(act, struct usb_request, list);
		kfree(req->buf);
		req->buf = NULL;
	}
	return -ENOMEM;
}

int mbim_start_xmit(struct sk_buff *skb, int ifid)
{
	struct mbim_eth_dev		*dev;
	int			length = skb->len;
	int			retval;
	struct usb_request	*req = NULL;
	unsigned long		flags;
	struct usb_ep		*in;
	u16			cdc_filter;
	bool			multi_pkt_xfer = false;

	static unsigned int okCnt, busyCnt;
	static int firstShot = 1, diffSec;
	static struct timeval tv_last, tv_cur;

	dev = mbim_dev;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb) {
		in = dev->port_usb->in_ep;
		cdc_filter = dev->port_usb->cdc_filter;
		multi_pkt_xfer = dev->port_usb->multi_pkt_xfer;
	} else {
		in = NULL;
		cdc_filter = 0;
	}
	spin_unlock_irqrestore(&dev->lock, flags);

	if (!in) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* Allocate memory for tx_reqs to support multi packet transfer */
	spin_lock_irqsave(&dev->req_lock, flags);
	if (multi_pkt_xfer && !dev->tx_req_bufsize) {
		retval = alloc_mbim_tx_buffer(dev);
		if (retval < 0) {
			spin_unlock_irqrestore(&dev->req_lock, flags);
			return -ENOMEM;
		}
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);


	/* apply outgoing CDC or RNDIS filters */
	if (!is_promisc(cdc_filter)) {
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
			if (!(cdc_filter & type)) {
				dev_kfree_skb_any(skb);
				pr_warn("cdc_filter error, cdc_filter is 0x%x , type is 0x%x\n",
					   cdc_filter, type);

				return NETDEV_TX_OK;
			}
		}
		/* ignores USB_CDC_PACKET_TYPE_DIRECTED */
	}

	spin_lock_irqsave(&dev->req_lock, flags);
	/*
	 * this freelist can be empty if an interrupt triggered disconnect()
	 * and reconfigured the gadget (shutting down this queue) after the
	 * network stack decided to xmit but before we got the spinlock.
	 */
	if (list_empty(&dev->tx_reqs)) {

		busyCnt++;
		do_gettimeofday(&tv_cur);

		if (firstShot) {
			tv_last = tv_cur;
			firstShot = 0;
			ERROR(dev,
				   "%s, NETDEV_TX_BUSY returned at firstShot , okCnt : %u, busyCnt : %u\n",
				   __func__, okCnt, busyCnt);
		} else {
			diffSec = tv_cur.tv_sec - tv_last.tv_sec;
			if (diffSec >= 2) {
				tv_last = tv_cur;
				ERROR(dev,
					   "%s, NETDEV_TX_BUSY returned, okCnt : %u, busyCnt : %u\n",
					   __func__, okCnt, busyCnt);
			}
		}


		spin_unlock_irqrestore(&dev->req_lock, flags);
		return NETDEV_TX_BUSY;
	}
	okCnt++;

	req = container_of(dev->tx_reqs.next, struct usb_request, list);
	list_del(&req->list);

	/* temporarily stop TX queue when the freelist empties */
	if (list_empty(&dev->tx_reqs)) {
		/*netif_stop_queue(net);
		   TO DO with stop queue;*/
	}
	spin_unlock_irqrestore(&dev->req_lock, flags);

	/* no buffer copies needed, unless the network stack did it
	 * or the hardware can't use skb buffers.
	 * or there's not enough space for extra headers we need
	 */
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->wrap) {
		if (dev->port_usb)
			skb = dev->wrap(dev->port_usb, skb, ifid);
		if (!skb) {
			spin_unlock_irqrestore(&dev->lock, flags);
			goto drop;
		}
	}

	if (multi_pkt_xfer) {

		pr_debug("req->length:%d header_len:%u\n"
				"skb->len:%d skb->data_len:%d\n",
				req->length, dev->header_len,
				skb->len, skb->data_len);

		if (dev->port_usb == NULL) {
			dev_kfree_skb_any(skb);
			pr_debug("eth_start_xmit, port_usb becomes NULL\n");
			return NETDEV_TX_OK;
		}
		/* Add RNDIS Header */
		memcpy(req->buf + req->length, dev->port_usb->header,
						dev->header_len);
		/* Increment req length by header size */
		req->length += dev->header_len;
		spin_unlock_irqrestore(&dev->lock, flags);
		/* Copy received IP data from SKB */
		memcpy(req->buf + req->length, skb->data, skb->len);
		/* Increment req length by skb data length */
		req->length += skb->len;
		length = req->length;
		dev_kfree_skb_any(skb);

		spin_lock_irqsave(&dev->req_lock, flags);
		dev->tx_skb_hold_count++;
		/* if (dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer) { */
		if ((dev->tx_skb_hold_count < dev->dl_max_pkts_per_xfer)
			&& (length < (dev->port_usb->dl_max_transfer_len - dev->net->mtu))) {
			if (dev->no_tx_req_used > TX_REQ_THRESHOLD) {
				list_add(&req->list, &dev->tx_reqs);
				spin_unlock_irqrestore(&dev->req_lock, flags);
				goto success;
			}
		}

		dev->no_tx_req_used++;
		dev->tx_skb_hold_count = 0;
		spin_unlock_irqrestore(&dev->req_lock, flags);
	} else {
		spin_unlock_irqrestore(&dev->lock, flags);
		length = skb->len;
		req->buf = skb->data;
		req->context = skb;
	}

	if (dev->port_usb == NULL) {
		if (!multi_pkt_xfer)
			dev_kfree_skb_any(skb);
		pr_debug("eth_start_xmit, port_usb becomes NULL\n");
		return NETDEV_TX_OK;
	}

	/* NCM requires no zlp if transfer is dwNtbInMaxSize */
	if (dev->port_usb->is_fixed &&
		length == dev->port_usb->fixed_in_len &&
		(length % in->maxpacket) == 0)
		req->zero = 0;
	else
		req->zero = 1;

	/* use zlp framing on tx for strict CDC-Ether conformance,
	 * though any robust network rx path ignores extra padding.
	 * and some hardware doesn't like to write zlps.
	 */
	if (req->zero && !dev->zlp && (length % in->maxpacket) == 0) {
		req->zero = 0;
		length++;
	}

	req->length = length;

	/* throttle high/super speed IRQ rate back slightly */
	if (gadget_is_dualspeed(dev->gadget) &&
			 (dev->gadget->speed == USB_SPEED_HIGH ||
			  dev->gadget->speed == USB_SPEED_SUPER)) {
		dev->tx_qlen++;
		if (dev->tx_qlen == (20/2)) {
			req->no_interrupt = 0;
			dev->tx_qlen = 0;
		} else {
			req->no_interrupt = 1;
		}
	} else {
		req->no_interrupt = 0;
	}
	retval = usb_ep_queue(in, req, GFP_ATOMIC);
	if (retval) {
		DBG(dev, "tx queue err %d\n", retval);
		pr_debug("[XLOG_INFO][UTHER]eth_start_xmit : tx queue err %d\n", retval);
	}


	if (retval) {
		if (!multi_pkt_xfer)
			dev_kfree_skb_any(skb);
		else
			req->length = 0;
drop:
		dev->net->stats.tx_dropped++;
		spin_lock_irqsave(&dev->req_lock, flags);
		list_add_tail(&req->list, &dev->tx_reqs);
		spin_unlock_irqrestore(&dev->req_lock, flags);
	}
success:
	return NETDEV_TX_OK;
}




static void mbim_eth_start(struct mbim_eth_dev *dev, gfp_t gfp_flags)
{
	DBG(dev, "%s\n", __func__);
	pr_debug("[XLOG_INFO][UTHER]%s\n", __func__);

	/* fill the rx queue */
	mbim_rx_fill(dev, gfp_flags);

	/* and open the tx floodgates */
	dev->tx_qlen = 0;
}

static int mbim_prealloc(struct list_head *list, struct usb_ep *ep, unsigned n)
{
	unsigned		i;
	struct usb_request	*req;
	bool			usb_in;

	if (!n)
		return -ENOMEM;

	/* queue/recycle up to N requests */
	i = n;
	list_for_each_entry(req, list, list) {
		if (i-- == 0)
			goto extra;
	}

	if (ep->desc->bEndpointAddress & USB_DIR_IN)
		usb_in = true;
	else
		usb_in = false;

	while (i--) {
		req = usb_ep_alloc_request(ep, GFP_ATOMIC);
		if (!req)
			return list_empty(list) ? -ENOMEM : 0;
		/* update completion handler */
		if (usb_in)
			req->complete = mbim_tx_complete;
		else
			req->complete = mbim_rx_complete;

		list_add(&req->list, list);
	}
	return 0;

extra:
	/* free extras */
	for (;;) {
		struct list_head	*next;

		next = req->list.next;
		list_del(&req->list);
		usb_ep_free_request(ep, req);

		if (next == list)
			break;

		req = container_of(next, struct usb_request, list);
	}
	return 0;
}


static int alloc_mbim_tx_requests(struct mbim_eth_dev *dev, struct mbim_gether *link, unsigned n)
{
	int	status;

	spin_lock(&dev->req_lock);
	status = mbim_prealloc(&dev->tx_reqs, link->in_ep, n);
	if (status < 0)
		goto fail;

	goto done;
fail:
	DBG(dev, "can't alloc tx requests\n");
	pr_debug("[XLOG_INFO][UTHER]alloc_requests : can't alloc requests\n");
done:
	spin_unlock(&dev->req_lock);
	return status;
}

static int alloc_mbim_rx_requests(struct mbim_eth_dev *dev, struct mbim_gether *link, unsigned n)
{
	int	status;

	spin_lock(&dev->reqrx_lock);

	status = mbim_prealloc(&dev->rx_reqs, link->out_ep, n);
	if (status < 0)
		goto fail;
	goto done;
fail:
	DBG(dev, "can't alloc rx requests\n");
	pr_debug("[XLOG_INFO][UTHER]alloc_requests : can't alloc rxrequests\n");
done:
	spin_unlock(&dev->reqrx_lock);
	return status;
}

/**
 * mbim_connect
 */
struct net_device *mbim_connect(struct mbim_gether *link)
{
	int			result = 0;
	struct mbim_eth_dev		*dev = link->ioport;

	pr_debug("[XLOG_INFO][UTHER]%s\n", __func__);

	link->in_ep->driver_data = dev;
	result = usb_ep_enable(link->in_ep);
	if (result != 0) {
		DBG(dev, "enable %s --> %d\n",
			link->in_ep->name, result);
		goto fail0;
	}

	link->out_ep->driver_data = dev;
	result = usb_ep_enable(link->out_ep);
	if (result != 0) {
		DBG(dev, "enable %s --> %d\n",
			link->out_ep->name, result);
		goto fail1;
	}

	if (result == 0) {
		result = alloc_mbim_tx_requests(dev, link, 20);
		if (result == 0)
			result = alloc_mbim_rx_requests(dev, link, 20);
	}

	if (result == 0) {
		dev->zlp = link->is_zlp_ok;
		dev->header_len = 0;
		dev->unwrap = link->unwrap;
		dev->wrap = link->wrap;
		dev->ul_max_pkts_per_xfer = link->ul_max_pkts_per_xfer;
		dev->dl_max_pkts_per_xfer = link->dl_max_pkts_per_xfer;

		spin_lock(&dev->lock);
		dev->tx_skb_hold_count = 0;
		dev->no_tx_req_used = 0;
		dev->tx_req_bufsize = 0;
		dev->port_usb = link;
		dev->port_usb->multi_pkt_xfer = 1;
		if (link->open)
			link->open(link);

		spin_unlock(&dev->lock);

		mbim_eth_start(dev, GFP_ATOMIC);

	/* on error, disable any endpoints	*/
	} else {
		(void) usb_ep_disable(link->out_ep);
fail1:
		(void) usb_ep_disable(link->in_ep);
	}

fail0:
	/* caller is responsible for cleanup on error */
	if (result < 0)
		return ERR_PTR(result);

	return 0;
}


/**
 * gether_setup_name - initialize one ethernet-over-usb link
 * @g: gadget to associated with these links
 * @ethaddr: NULL, or a buffer in which the ethernet address of the
 *	host side of the link is recorded
 * @netname: name for network device (for example, "usb")
 * Context: may sleep
 *
 * This sets up the single network link that may be exported by a
 * gadget driver using this framework.  The link layer addresses are
 * set up using module parameters.
 *
 * Returns negative errno, or zero on success
 */
struct mbim_eth_dev *mbim_ether_setup_name(struct usb_gadget *g)
{
	struct mbim_eth_dev		*dev;
	struct net_device	*net;

	net = alloc_etherdev(sizeof(*dev));
	if (!net)
		return ERR_PTR(-ENOMEM);

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);
	spin_lock_init(&dev->req_lock);
	spin_lock_init(&dev->reqrx_lock);
	INIT_WORK(&dev->work, eth_work);
	INIT_WORK(&dev->rx_work, process_mbim_rx_w);
	INIT_WORK(&dev->rx_work1, process_mbim_rx_w1);
	INIT_LIST_HEAD(&dev->tx_reqs);
	INIT_LIST_HEAD(&dev->rx_reqs);

	skb_queue_head_init(&dev->rx_frames);
	/* network device setup */
	dev->net = net;
	/* snprintf(net->name, sizeof(net->name), "%s%%d", netname);*/

	dev->gadget = g;
	mbim_dev = dev;

	return dev;
}

/**
 * gether_cleanup - remove Ethernet-over-USB device
 * Context: may sleep
 *
 * This is called to free all resources allocated by @gether_setup().
 */
void mbim_ether_cleanup(struct mbim_eth_dev *dev)
{
	if (!dev)
		return;

	flush_work(&dev->work);
	free_netdev(dev->net);
	mbim_dev = NULL;
}

/**
 * gether_disconnect - notify network layer that USB link is inactive
 * @link: the USB link, on which gether_connect() was called
 * Context: irqs blocked
 *
 * This is called to deactivate endpoints and let the network layer know
 * the connection went inactive ("no carrier").
 *
 * On return, the state is as if gether_connect() had never been called.
 * The endpoints are inactive, and accordingly without active USB I/O.
 * Pointers to endpoint descriptors and endpoint private data are nulled.
 */
void mbim_disconnect(struct mbim_gether *link)
{
	struct mbim_eth_dev		*dev = link->ioport;
	struct usb_request	*req;
	struct sk_buff		*skb;

	WARN_ON(!dev);
	if (!dev)
		return;

	DBG(dev, "%s\n", __func__);
	pr_debug("[XLOG_INFO][UTHER]%s\n", __func__);

	/* disable endpoints, forcing (synchronous) completion
	 * of all pending i/o.  then free the request objects
	 * and forget about the endpoints.
	 */
	usb_ep_disable(link->in_ep);
	spin_lock(&dev->req_lock);
	while (!list_empty(&dev->tx_reqs)) {
		req = container_of(dev->tx_reqs.next,
					struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->req_lock);
		if (link->multi_pkt_xfer)  {
			kfree(req->buf);
			req->buf = NULL;
		}
		usb_ep_free_request(link->in_ep, req);
		spin_lock(&dev->req_lock);
	}
	/* Free rndis header buffer memory */
	kfree(link->header);
	link->header = NULL;
	spin_unlock(&dev->req_lock);
	link->in_ep->driver_data = NULL;
	link->in_ep->desc = NULL;

	usb_ep_disable(link->out_ep);
	spin_lock(&dev->reqrx_lock);
	while (!list_empty(&dev->rx_reqs)) {
		req = container_of(dev->rx_reqs.next,
					struct usb_request, list);
		list_del(&req->list);

		spin_unlock(&dev->reqrx_lock);
		usb_ep_free_request(link->out_ep, req);
		spin_lock(&dev->reqrx_lock);
	}
	spin_unlock(&dev->reqrx_lock);

	spin_lock(&dev->rx_frames.lock);
	while ((skb = __skb_dequeue(&dev->rx_frames)))
		dev_kfree_skb_any(skb);
	spin_unlock(&dev->rx_frames.lock);

	link->out_ep->driver_data = NULL;
	link->out_ep->desc = NULL;

	/* finish forgetting about this USB link episode */
	dev->header_len = 0;
	dev->unwrap = NULL;
	dev->wrap = NULL;

	spin_lock(&dev->lock);
	dev->port_usb = NULL;
	spin_unlock(&dev->lock);
}

static int __init mbim_gether_init(void)
{
	mbim_uether_wq  = create_singlethread_workqueue("mbim_ether");
	if (!mbim_uether_wq) {
		pr_err("%s: Unable to create workqueue: uether\n", __func__);
		return -ENOMEM;
	}
	mbim_uether_wq1  = create_singlethread_workqueue("mbim_ether_rx1");
	if (!mbim_uether_wq1) {
		destroy_workqueue(mbim_uether_wq);
		pr_err("%s: Unable to create workqueue: uether\n", __func__);
		return -ENOMEM;
	}
	return 0;
}
module_init(mbim_gether_init);
