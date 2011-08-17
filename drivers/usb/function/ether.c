/* drivers/usb/function/ether.c
 *
 * Simple Ethernet Function Device
 *
 * Copyright (C) 2008 Google, Inc.
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
 * Implements the "cdc_subset" bulk-only protocol supported by Linux.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include "usb_function.h"

/* Ethernet frame is 1514 + FCS, but round up to 512 * 3 so we
 * always queue a multiple of the USB max packet size (64 or 512)
 */
#define USB_MTU 1536

#define MAX_TX 8
#define MAX_RX 8

struct ether_context {
	spinlock_t lock;
	struct net_device *dev;
	struct usb_endpoint *out;
	struct usb_endpoint *in;

	struct list_head rx_reqs;
	struct list_head tx_reqs;

	struct net_device_stats stats;
};

static int ether_queue_out(struct ether_context *ctxt,
			   struct usb_request *req);
static void ether_in_complete(struct usb_endpoint *ept,
			      struct usb_request *req);
static void ether_out_complete(struct usb_endpoint *ept,
			       struct usb_request *req);

static void ether_bind(struct usb_endpoint **ept, void *_ctxt)
{
	struct ether_context *ctxt = _ctxt;
	struct usb_request *req;
	unsigned long flags;
	int n;

	ctxt->out = ept[0];
	ctxt->in = ept[1];

	for (n = 0; n < MAX_RX; n++) {
		req = usb_ept_alloc_req(ctxt->out, 0);
		if (!req)
			break;
		req->complete = ether_out_complete;
		spin_lock_irqsave(&ctxt->lock, flags);
		list_add_tail(&req->list, &ctxt->rx_reqs);
		spin_unlock_irqrestore(&ctxt->lock, flags);
	}
	for (n = 0; n < MAX_TX; n++) {
		req = usb_ept_alloc_req(ctxt->in, 0);
		if (!req)
			break;
		req->complete = ether_in_complete;
		spin_lock_irqsave(&ctxt->lock, flags);
		list_add_tail(&req->list, &ctxt->tx_reqs);
		spin_unlock_irqrestore(&ctxt->lock, flags);
	}
}

static void ether_in_complete(struct usb_endpoint *ept,
			      struct usb_request *req)
{
	unsigned long flags;
	struct sk_buff *skb = req->context;
	struct ether_context *ctxt = *((void **) skb->cb);

	if (req->status == 0) {
		ctxt->stats.tx_packets++;
		ctxt->stats.tx_bytes += req->actual;
	} else {
		ctxt->stats.tx_errors++;
	}

	dev_kfree_skb_any(skb);

	spin_lock_irqsave(&ctxt->lock, flags);
	if (list_empty(&ctxt->tx_reqs))
		netif_start_queue(ctxt->dev);
	list_add_tail(&req->list, &ctxt->tx_reqs);
	spin_unlock_irqrestore(&ctxt->lock, flags);
}

static void ether_out_complete(struct usb_endpoint *ept,
			       struct usb_request *req)
{
	struct sk_buff *skb = req->context;
	struct ether_context *ctxt = *((void **) skb->cb);

	if (req->status == 0) {
		skb_put(skb, req->actual);
		skb->protocol = eth_type_trans(skb, ctxt->dev);
		ctxt->stats.rx_packets++;
		ctxt->stats.rx_bytes += req->actual;
		netif_rx(skb);
	} else {
		dev_kfree_skb_any(skb);
		ctxt->stats.rx_errors++;
	}

	/* don't bother requeuing if we just went offline */
	if (req->status == -ENODEV) {
		unsigned long flags;
		spin_lock_irqsave(&ctxt->lock, flags);
		list_add_tail(&req->list, &ctxt->rx_reqs);
		spin_unlock_irqrestore(&ctxt->lock, flags);
	} else {
		if (ether_queue_out(ctxt, req))
			pr_err("ether_out: cannot requeue\n");
	}
}

static int ether_queue_out(struct ether_context *ctxt,
			   struct usb_request *req)
{
	unsigned long flags;
	struct sk_buff *skb;
	int ret;

	skb = alloc_skb(USB_MTU + NET_IP_ALIGN, GFP_ATOMIC);
	if (!skb) {
		pr_err("ether_queue_out: failed to alloc skb\n");
		ret = -ENOMEM;
		goto fail;
	}

	skb_reserve(skb, NET_IP_ALIGN);

	*((void **) skb->cb) = ctxt;
	req->buf = skb->data;
	req->length = USB_MTU;
	req->context = skb;

	ret = usb_ept_queue_xfer(ctxt->out, req);
	if (ret) {
fail:
		spin_lock_irqsave(&ctxt->lock, flags);
		list_add_tail(&req->list, &ctxt->rx_reqs);
		spin_unlock_irqrestore(&ctxt->lock, flags);
	}

	return ret;
}

static void ether_configure(int configured, void *_ctxt)
{
	unsigned long flags;
	struct ether_context *ctxt = _ctxt;
	struct usb_request *req;

	pr_info("ether_configure() %d\n", configured);

	if (configured) {
		/* we're online -- get all rx requests queued */
		for (;;) {
			spin_lock_irqsave(&ctxt->lock, flags);
			if (list_empty(&ctxt->rx_reqs)) {
				req = 0;
			} else {
				req = list_first_entry(&ctxt->rx_reqs,
						       struct usb_request,
						       list);
				list_del(&req->list);
			}
			spin_unlock_irqrestore(&ctxt->lock, flags);
			if (!req)
				break;
			if (ether_queue_out(ctxt, req))
				break;
		}
	} else {
		/* all pending requests will be canceled */
	}
}

static struct usb_function usb_func_ether = {
	.bind = ether_bind,
	.configure = ether_configure,

	.name = "ether",

	.ifc_class = 0x02,
	.ifc_subclass = 0x0a,
	.ifc_protocol = 0x00,

	.ifc_name = "ether",

	.ifc_ept_count = 2,
	.ifc_ept_type = { EPT_BULK_OUT, EPT_BULK_IN },
};

static int usb_ether_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct ether_context *ctxt = netdev_priv(dev);
	struct usb_request *req;
	unsigned long flags;
	unsigned len;

	spin_lock_irqsave(&ctxt->lock, flags);
	if (list_empty(&ctxt->tx_reqs)) {
		req = 0;
	} else {
		req = list_first_entry(&ctxt->tx_reqs,
				       struct usb_request, list);
		list_del(&req->list);
		if (list_empty(&ctxt->tx_reqs))
			netif_stop_queue(dev);
	}
	spin_unlock_irqrestore(&ctxt->lock, flags);

	if (!req) {
		pr_err("usb_ether_xmit: could not obtain tx request\n");
		return 1;
	}

	/* ensure that we end with a short packet */
	len = skb->len;
	if (!(len & 63) || !(len & 511))
		len++;

	*((void **) skb->cb) = ctxt;
	req->context = skb;
	req->buf = skb->data;
	req->length = len;

	if (usb_ept_queue_xfer(ctxt->in, req)) {
		spin_lock_irqsave(&ctxt->lock, flags);
		list_add_tail(&req->list, &ctxt->tx_reqs);
		netif_start_queue(dev);
		spin_unlock_irqrestore(&ctxt->lock, flags);

		dev_kfree_skb_any(skb);
		ctxt->stats.tx_dropped++;

		pr_err("usb_ether_xmit: could not queue tx request\n");
	}

	return 0;
}

static int usb_ether_open(struct net_device *dev)
{
	return 0;
}

static int usb_ether_stop(struct net_device *dev)
{
	return 0;
}

static struct net_device_stats *usb_ether_get_stats(struct net_device *dev)
{
	struct ether_context *ctxt = netdev_priv(dev);
	return &ctxt->stats;
}

static void __init usb_ether_setup(struct net_device *dev)
{
	struct ether_context *ctxt = netdev_priv(dev);

	pr_info("usb_ether_setup()\n");

	INIT_LIST_HEAD(&ctxt->rx_reqs);
	INIT_LIST_HEAD(&ctxt->tx_reqs);
	spin_lock_init(&ctxt->lock);
	ctxt->dev = dev;

	dev->open = usb_ether_open;
	dev->stop = usb_ether_stop;
	dev->hard_start_xmit = usb_ether_xmit;
	dev->get_stats = usb_ether_get_stats;
	dev->watchdog_timeo = 20;

	ether_setup(dev);

	random_ether_addr(dev->dev_addr);
}

static int __init ether_init(void)
{
	struct net_device *dev;
	int ret;

	dev = alloc_netdev(sizeof(struct ether_context),
			   "usb%d", usb_ether_setup);
	if (!dev)
		return -ENOMEM;

	ret = register_netdev(dev);
	if (ret) {
		free_netdev(dev);
	} else {
		struct ether_context *ctxt = netdev_priv(dev);
		usb_func_ether.context = ctxt;
		usb_function_register(&usb_func_ether);
	}
	return ret;
}

module_init(ether_init);
