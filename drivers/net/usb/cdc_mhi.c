/*
 * cdc_mhi.c	MHI over USB CDC - host driver
 *
 * Copyright (C) 2011 Renesas Mobile Corporation. All rights reserved.
 *
 * Author: Petri To. Mattila <petri.to.mattila@renesasmobile.com>
 *
 * Based on work by: Rémi Denis-Courmont
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gfp.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/if_mhi.h>
#include <linux/mhi.h>
#include <linux/l2mux.h>
#include <linux/phonet.h>
#include <linux/socket.h>

#define USE_DEBUG

/* USB constants */
#define USB_ID_VENDOR_NOKIA	0x0421
#define USB_SUB_CLASS_MHI	0xFB

/* USB queue size */
#define RXQ_SIZE		17

/* Safe header size */
#define MHI_HEADER_MIN		4
#define MHI_HEADER_MAX		20

/* Number of TX bands */
#define TX_BANDS		4

#ifndef MIN
# define MIN(a, b)	(((a) < (b)) ? (a) : (b))
#endif

#ifdef USE_DEBUG
# define DPRINTK(...)    printk(KERN_DEBUG "CDC_MHI: " __VA_ARGS__)
#else
# define DPRINTK(...)
#endif


/* Local data structures */

struct mhi_dev {
	struct net_device	*dev;
	struct usb_device	*usb;
	struct usb_interface	*intf;
	struct usb_interface	*data_intf;

	u8			active;
	u8			disconnected;

	spinlock_t		tx_lock;
	unsigned		tx_pipe;
	unsigned		tx_qlen[TX_BANDS];

	spinlock_t		rx_lock;
	unsigned		rx_pipe;
	struct sk_buff		*rx_skb;
	struct urb		*rx_urbs[RXQ_SIZE];
};


/*** Local data ***/

static const char mhi_ifname[] = "mhi%d";


/*** Prototypes ***/

static int mhi_close(struct net_device *dev);
static int mhi_open(struct net_device *dev);

static void mhi_xmit_complete(struct urb *req);
static void mhi_recv_complete(struct urb *req);


/*** Functions ***/

static u16 mhi_select_queue(struct net_device *dev, struct sk_buff *skb)
{
	u16 txq;

	switch (ntohs(skb->protocol)) {
#if 0
	case ETH_P_AUDIO:
	txq = 0;
	break;
#endif
	case ETH_P_MHI:
	case ETH_P_PHONET:
	txq = 1;
	break;

	case ETH_P_MHDP:
	txq = 2;
	break;

	default:
	txq = 3;
	break;
	}

	DPRINTK("mhi_select_queue  txq:%d\n", txq);

	return txq;
}

static netdev_tx_t mhi_xmit_start(struct sk_buff *skb,
				struct net_device *dev) {
	struct mhi_dev		*mhi = netdev_priv(dev);
	struct urb		*req = NULL;
	struct netdev_queue     *txq;

	unsigned long		 flags;
	int			 map;
	int			 err = 0;

	err = l2mux_skb_tx(skb, dev);
	if (unlikely(err))
		goto drop;

	map = skb_get_queue_mapping(skb);
	txq = netdev_get_tx_queue(dev, map);

	DPRINTK("mhi_xmit_start, skb len:%d mapping:%d\n", skb->len, map);

#ifdef USE_DEBUG
	{
		u8 *ptr = (u8 *)skb->data;
		int len = skb_headlen(skb);
		int i;

		printk(KERN_DEBUG "CDC_MHI: TX length:%d\n", len);
		for (i = 0; i < len; i++) {
			if (i%8 == 0)
				printk(KERN_DEBUG "CDC_MHI: TX [%04X] ", i);
			printk(" 0x%02X", ptr[i]);
			if (i%8 == 7 || i == len-1)
				printk("\n");
		}
	}
#endif

	if (skb->data_len > 0)
		goto drop;

	req = usb_alloc_urb(0, GFP_ATOMIC);
	if (unlikely(!req))
		goto drop;

	usb_fill_bulk_urb(req, mhi->usb, mhi->tx_pipe,
			  skb->data, skb->len, mhi_xmit_complete, skb);
	req->transfer_flags = URB_ZERO_PACKET;

	err = usb_submit_urb(req, GFP_ATOMIC);
	if (unlikely(err)) {
		usb_free_urb(req);
		goto drop;
	}

	spin_lock_irqsave(&mhi->tx_lock, flags);
	{
		mhi->tx_qlen[map]++;
		if (mhi->tx_qlen[map] >= dev->tx_queue_len)
			netif_tx_stop_queue(txq);
	}
	spin_unlock_irqrestore(&mhi->tx_lock, flags);

	DPRINTK("mhi_xmit_start OK, tx_qlen[%d]:%d\n", map, mhi->tx_qlen[map]);

	return NETDEV_TX_OK;

drop:
	DPRINTK("mhi_xmit_start, dropping skb len:%d\n", skb->len);

	dev_kfree_skb(skb);
	dev->stats.tx_dropped++;
	return NETDEV_TX_OK;
}


static void mhi_xmit_complete(struct urb *req)
{
	struct sk_buff		*skb = req->context;
	struct net_device	*dev = skb->dev;
	struct mhi_dev		*mhi = netdev_priv(dev);
	struct netdev_queue	*txq;
	int			map;

	map = skb_get_queue_mapping(skb);
	txq = netdev_get_tx_queue(dev, map);

	switch (req->status) {
	case 0:
	dev->stats.tx_bytes += skb->len;
	break;
	case -ENOENT:
	case -ECONNRESET:
	case -ESHUTDOWN:
	dev->stats.tx_aborted_errors++;
	/* Fall through */
	default:
	dev->stats.tx_errors++;
	dev_dbg(&dev->dev, "TX error (%d)\n", req->status);
	}

	dev->stats.tx_packets++;

	spin_lock(&mhi->tx_lock);
	{
		mhi->tx_qlen[map]--;
		netif_tx_wake_queue(txq);
	}
	spin_unlock(&mhi->tx_lock);

	dev_kfree_skb_any(skb);
	usb_free_urb(req);

	DPRINTK("mhi_xmit_complete, tx_qlen[%d]:%d\n", map, mhi->tx_qlen[map]);
}

static int mhi_recv_submit(struct mhi_dev *mhi,
				struct urb *req, gfp_t gfp_flags) {
	struct net_device	*dev = mhi->dev;
	struct page		*page;
	int err = 0;

	page = __netdev_alloc_page(dev, gfp_flags);
	if (unlikely(!page))
		return -ENOMEM;

	usb_fill_bulk_urb(req, mhi->usb, mhi->rx_pipe,
			  page_address(page), PAGE_SIZE,
			  mhi_recv_complete, dev);

	req->transfer_flags = 0;
	err = usb_submit_urb(req, gfp_flags);
	if (unlikely(err)) {
		dev_dbg(&dev->dev, "RX submit error (%d)\n", err);
		netdev_free_page(dev, page);
	}

	return err;
}

static void mhi_recv_complete(struct urb *req)
{
	struct net_device	*dev = req->context;
	struct mhi_dev		*mhi = netdev_priv(dev);
	struct sk_buff		*skb;
	struct page		*page;
	unsigned long		flags;
	unsigned long		offset;

	DPRINTK("mhi_recv_complete, status:%d\n", req->status);

#ifdef USE_DEBUG
	{
		u8 *ptr = (u8 *)req->transfer_buffer;
		int len = req->actual_length;
		int i;
		printk(KERN_DEBUG "CDC_MHI: RX length:%d\n", len);

		for (i = 0; i < len; i++) {
			if (i%8 == 0)
				printk(KERN_DEBUG "CDC_MHI: RX [%04X] ", i);
			printk(" 0x%02X", ptr[i]);
			if (i%8 == 7 || i == len-1)
				printk("\n");
		}
	}
#endif
	if (likely(req->status == 0)) {
		spin_lock_irqsave(&mhi->rx_lock, flags);
		{
			page = virt_to_page(req->transfer_buffer);

			skb = mhi->rx_skb;
			if (!skb) {
				if (req->actual_length < MHI_HEADER_MIN)
					goto error;

				skb = mhi->rx_skb = netdev_alloc_skb(dev,
							MHI_HEADER_MAX);
				if (unlikely(!skb))
					goto error;

				offset = MIN(req->actual_length,
						MHI_HEADER_MAX);
				memcpy(skb_put(skb, offset),
					page_address(page), offset);
			} else
				offset = 0;

			skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags, page,
					offset, req->actual_length-offset);

			goto cont;
error:
			netdev_free_page(dev, page);
cont:
			if (req->actual_length < PAGE_SIZE)
				mhi->rx_skb = NULL;
			else
				skb = NULL;
		}
		spin_unlock_irqrestore(&mhi->rx_lock, flags);

		if (skb) {
			dev->stats.rx_packets++;
			dev->stats.rx_bytes += skb->len;
			l2mux_skb_rx(skb, dev);
		}
	} else {
		switch (req->status) {
		case -ENOENT:
		case -ECONNRESET:
		case -ESHUTDOWN:
		req = NULL;
		break;

		case -EOVERFLOW:
		dev->stats.rx_errors++;
		dev->stats.rx_over_errors++;
		dev_dbg(&dev->dev, "RX overflow\n");
		break;

		case -EILSEQ:
		dev->stats.rx_errors++;
		dev->stats.rx_crc_errors++;
		break;
		}
	}

	if (req)
		mhi_recv_submit(mhi, req, GFP_ATOMIC);
}


static int mhi_open(struct net_device *dev)
{
	struct mhi_dev	*mhi = netdev_priv(dev);
	struct urb	*req;
	unsigned	num;
	unsigned	i;
	int		err;

	DPRINTK("mhi_open, %d queues\n", dev->num_tx_queues);

	num = mhi->data_intf->cur_altsetting->desc.bInterfaceNumber;

	err = usb_set_interface(mhi->usb, num, mhi->active);
	if (unlikely(err))
		return err;

	for (i = 0; i < RXQ_SIZE; i++) {
		req = usb_alloc_urb(0, GFP_KERNEL);
		if (!req)
			goto fail;
		if (mhi_recv_submit(mhi, req, GFP_KERNEL))
			goto fail;
		mhi->rx_urbs[i] = req;
	}

	try_module_get(THIS_MODULE);
	netif_tx_wake_all_queues(dev);
	return 0;

fail:
	mhi_close(dev);
	return -ENOMEM;
}

static int mhi_close(struct net_device *dev)
{
	struct mhi_dev	*mhi = netdev_priv(dev);
	struct urb	*req;
	unsigned	num;
	unsigned	i;
	int		err;

	DPRINTK("mhi_close\n");
	num = mhi->data_intf->cur_altsetting->desc.bInterfaceNumber;
	netif_tx_stop_all_queues(dev);

	for (i = 0; i < RXQ_SIZE; i++) {
		req = mhi->rx_urbs[i];
		mhi->rx_urbs[i] = NULL;
		if (req) {
			usb_kill_urb(req);
			usb_free_urb(req);
		}
	}

	err = usb_set_interface(mhi->usb, num, !mhi->active);
	module_put(THIS_MODULE);
	return err;
}

static int mhi_set_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < MHI_MIN_MTU || new_mtu > MHI_MAX_MTU)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static int mhi_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct if_phonet_req *req = (struct if_phonet_req *)ifr;

	switch (cmd) {
	case SIOCPNGAUTOCONF:
	req->ifr_phonet_autoconf.device = PN_DEV_PC;
	break;
	}
	return 0;
}


static const struct net_device_ops mhi_ops = {
	.ndo_open		= mhi_open,
	.ndo_stop		= mhi_close,
	.ndo_start_xmit		= mhi_xmit_start,
	.ndo_do_ioctl		= mhi_ioctl,
	.ndo_change_mtu		= mhi_set_mtu,
	.ndo_select_queue	= mhi_select_queue,
};

static void mhi_setup(struct net_device *dev)
{
	dev->features		= 0;
	dev->netdev_ops		= &mhi_ops;
	dev->destructor		= free_netdev;
	dev->type		= ARPHRD_MHI;
	dev->flags		= IFF_POINTOPOINT | IFF_NOARP;
	dev->mtu		= MHI_MAX_MTU;
	dev->hard_header_len	= 4;
	dev->dev_addr[0]	= PN_MEDIA_MODEM_HOST_IF;
	dev->addr_len		= 1;
	dev->tx_queue_len	= 3;
}


static struct usb_driver mhi_driver;

int mhi_probe(struct usb_interface *intf, const struct usb_device_id *id)
{

	const struct usb_cdc_union_desc *union_header = NULL;
	const struct usb_cdc_header_desc *mhi_header = NULL;
	const struct usb_host_interface *data_desc;
	struct usb_interface		*data_intf;
	struct usb_device		*usbdev;
	struct net_device		*dev;
	struct mhi_dev			*mhi;

	u8	*alt;
	int	len, cnt;
	int	rxep, txep;
	int	err = 0;

	usbdev = interface_to_usbdev(intf);
	alt = intf->altsetting->extra;
	len = intf->altsetting->extralen;

	while (len >= 3) {
		cnt = alt[0];
		if (cnt < 3)
			return -EINVAL;
		/* bDescriptorType */
		if (alt[1] == USB_DT_CS_INTERFACE) {
			/* bDescriptorSubType */
			switch (alt[2]) {
			case USB_CDC_UNION_TYPE:
			if (union_header || cnt < 5)
				break;
			union_header = (struct usb_cdc_union_desc *)alt;
			break;
			case 0xAB:
			if (mhi_header || cnt < 5)
				break;
			mhi_header = (struct usb_cdc_header_desc *)alt;
			break;
			}
		}

		alt += cnt;
		len -= cnt;
	}

	if (!union_header || !mhi_header)
		return -EINVAL;

	data_intf = usb_ifnum_to_if(usbdev, union_header->bSlaveInterface0);
	if (data_intf == NULL)
		return -ENODEV;

	/* Data interface has one inactive and one active setting */
	if (data_intf->num_altsetting != 2)
		return -EINVAL;

	/* One of them has two endpoints */
	if (data_intf->altsetting[0].desc.bNumEndpoints == 0 &&
	    data_intf->altsetting[1].desc.bNumEndpoints == 2)
		data_desc = &data_intf->altsetting[1];
	else
	if (data_intf->altsetting[0].desc.bNumEndpoints == 2 &&
	    data_intf->altsetting[1].desc.bNumEndpoints == 0)
		data_desc = &data_intf->altsetting[0];
	else
		return -EINVAL;

	dev = alloc_netdev_mq(sizeof(*mhi), mhi_ifname, mhi_setup, TX_BANDS);
	if (dev == NULL)
		return -ENOMEM;

	SET_NETDEV_DEV(dev, &intf->dev);

	mhi = netdev_priv(dev);
	netif_stop_queue(dev);

	mhi->dev = dev;
	mhi->usb = usb_get_dev(usbdev);
	mhi->intf = intf;
	mhi->data_intf = data_intf;

	spin_lock_init(&mhi->tx_lock);
	spin_lock_init(&mhi->rx_lock);

	if (usb_pipein(data_desc->endpoint[0].desc.bEndpointAddress) &&
	     usb_pipeout(data_desc->endpoint[1].desc.bEndpointAddress)) {
		rxep = 0;
		txep = 1;
	} else
	if (usb_pipeout(data_desc->endpoint[0].desc.bEndpointAddress) &&
	     usb_pipein(data_desc->endpoint[1].desc.bEndpointAddress)) {
		rxep = 1;
		txep = 0;
	} else {
		dev_dbg(&dev->dev, "USB CDC L1 pipe mismatch\n");
		goto fail;
	}

	mhi->rx_pipe = usb_rcvbulkpipe(usbdev,
		data_desc->endpoint[rxep].desc.bEndpointAddress);
	mhi->tx_pipe = usb_sndbulkpipe(usbdev,
		data_desc->endpoint[txep].desc.bEndpointAddress);

	mhi->active = data_desc - data_intf->altsetting;

	err = usb_driver_claim_interface(&mhi_driver, data_intf, mhi);
	if (err)
		goto fail;

	/* Force inactive mode until the network device is brought UP */
	usb_set_interface(usbdev, union_header->bSlaveInterface0,
			  !mhi->active);
	usb_set_intfdata(intf, mhi);

	mhi->disconnected = 0;

	err = register_netdev(dev);
	if (err)
		goto fail2;

	dev_dbg(&dev->dev, "USB CDC mhi device found\n");

	return 0;

fail2:
	usb_driver_release_interface(&mhi_driver, data_intf);

fail:
	usb_set_intfdata(intf, NULL);
	free_netdev(dev);

	mhi->disconnected = 1;

	return err;
}

static void mhi_disconnect(struct usb_interface	*intf)
{
	struct mhi_dev		*mhi = usb_get_intfdata(intf);
	struct usb_device	*usb = mhi->usb;

	if (!mhi || mhi->disconnected)
		return;

	mhi->disconnected = 1;

	usb_driver_release_interface(&mhi_driver,
		(mhi->intf == intf) ? mhi->data_intf : mhi->intf);

	unregister_netdev(mhi->dev);
	usb_put_dev(usb);
}


static struct usb_device_id mhi_ids[] = {
	{
	.match_flags = USB_DEVICE_ID_MATCH_VENDOR |
		       USB_DEVICE_ID_MATCH_INT_CLASS |
		       USB_DEVICE_ID_MATCH_INT_SUBCLASS,
	.idVendor = USB_ID_VENDOR_NOKIA,
	.bInterfaceClass = USB_CLASS_COMM,
	.bInterfaceSubClass = USB_SUB_CLASS_MHI,
	},
	{ },
};

MODULE_DEVICE_TABLE(usb, mhi_ids);


static struct usb_driver mhi_driver = {
	.name =		"cdc_mhi",
	.probe =	mhi_probe,
	.disconnect =	mhi_disconnect,
	.id_table =	mhi_ids,
};

static int __init cdc_mhi_init(void)
{
	return usb_register(&mhi_driver);
}

static void __exit cdc_mhi_exit(void)
{
	usb_deregister(&mhi_driver);
}

module_init(cdc_mhi_init);
module_exit(cdc_mhi_exit);

MODULE_AUTHOR("Renesas Mobile Corporation");
MODULE_DESCRIPTION("MHI over USB CDC host interface");
MODULE_LICENSE("GPL");

