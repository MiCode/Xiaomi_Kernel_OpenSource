/*
* mhi_net_device.c
*
* Copyright (C) 2011 Renesas. All rights reserved.
*
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
* 02110-1301 USA
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/netdevice.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>
#include <linux/if_phonet.h>
#include <linux/if_mhi.h>
#include <linux/mhi.h>
#include <linux/phonet.h>
#include <linux/delay.h>
#include <linux/l2mux.h>
#include <linux/etherdevice.h>
#include <linux/platform_device.h>
#include <net/phonet/pn_dev.h>
#include <linux/mii.h>
#include <linux/usb.h>

#include <linux/usb/usbnet.h>
#include <linux/wakelock.h>


/*
#define HSIC_USE_DEBUG
#define DUMP_FRAMES_DEBUG
*/

#ifndef PN_DEV_HOST
#define PN_DEV_HOST     0x00
#endif

#ifdef HSIC_USE_DEBUG
#define DPRINTK(...)    printk(KERN_DEBUG __VA_ARGS__)
#else
#define DPRINTK(...)
#endif

#define EPRINTK(...)    printk(KERN_EMERG __VA_ARGS__)

/*private struct*/
struct mhi_data {
	struct net_device *net_dev;
	struct net_device *master;
};

u32 debug_nb_crash_modem;
struct wake_lock wake_no_suspend;

static int mhi_net_device_xmit(struct sk_buff *skb, struct net_device *dev);
static int mhi_net_device_ioctl(struct net_device *dev,
					struct ifreq *ifr,
					int cmd);
static int mhi_net_device_set_mtu(struct net_device *dev, int new_mtu);
static int mhi_net_device_open(struct net_device *dev);
static int mhi_net_device_close(struct net_device *dev);
static int mhi_net_dev_recv(struct sk_buff *skb,
				struct net_device *dev,
				struct packet_type	*type,
				struct net_device *orig_dev);

static struct packet_type modem_packet_type __read_mostly = {
	.type = cpu_to_be16(0x9876),
	.func = mhi_net_dev_recv,
};

static int mhi_net_dev_recv(
	struct sk_buff	*skb,
	struct net_device	*dev,
	struct packet_type	*type,
	struct net_device	*orig_dev)
{
	DPRINTK("mhi_net_dev_recv\n");
	wake_lock_timeout(&wake_no_suspend, 1 * HZ);
	return l2mux_skb_rx(skb, __dev_get_by_name(&init_net, "mhi0"));
}


static const struct net_device_ops mhi_net_device_ops = {
	.ndo_open           = mhi_net_device_open,
	.ndo_stop           = mhi_net_device_close,
	.ndo_start_xmit     = mhi_net_device_xmit,
	.ndo_do_ioctl       = mhi_net_device_ioctl,
	.ndo_change_mtu     = mhi_net_device_set_mtu,
};

static void mhi_net_device_setup(struct net_device *dev)
{
	dev->features        = 0 ;
	dev->netdev_ops      = &mhi_net_device_ops;
	dev->destructor      = free_netdev;
	dev->type            = ARPHRD_MHI;
	dev->flags           = IFF_POINTOPOINT | IFF_NOARP;
	dev->mtu             = MHI_MAX_MTU;
	dev->hard_header_len = 4;
	dev->dev_addr[0]     = PN_MEDIA_MODEM_HOST_IF;
	dev->addr_len        = 1;
	dev->tx_queue_len    = 2000;
}

static int mhi_net_device_ioctl(struct net_device *dev,
					struct ifreq *ifr,
					int cmd)
{
	struct if_phonet_req *req = (struct if_phonet_req *)ifr;
	DPRINTK("mhi_net_device_ioctl\n");

	switch (cmd) {
	case SIOCPNGAUTOCONF:

		req->ifr_phonet_autoconf.device = PN_DEV_HOST;
		phonet_route_add(dev, 0x60);
		phonet_route_add(dev, 0x44);
		phonet_route_add(dev, 0x64);

	break;
	}
	return 0;
}

static int mhi_net_device_set_mtu(struct net_device *dev, int new_mtu)
{
	DPRINTK("mhi_net_device_set_mtu\n");

	if ((new_mtu < MHI_MIN_MTU) || (new_mtu > MHI_MAX_MTU))
		return -EINVAL;

	dev->mtu = new_mtu;

	return 0;
}


static int mhi_net_device_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct mhi_data *priv = netdev_priv(dev);
	int err;

	/* Call l2mux registered function, according to skb->protocol */
	err = l2mux_skb_tx(skb, dev);
	if (unlikely(err))
		goto drop;

#ifdef HSIC_USE_DEBUG
{
	struct l2muxhdr	 *l2hdr;
	unsigned          l3pid;
	unsigned	  l3len;

	/* L2MUX header */
	l2hdr = l2mux_hdr(skb);

	/* proto id and length in L2 header */
	l3pid = l2mux_get_proto(l2hdr);
	l3len = l2mux_get_length(l2hdr);

	DPRINTK("L2MUX: TX dev:%d skb_len:%d l3_len:%d l3_pid:%d\n",
		       skb->dev->ifindex, skb->len, l3len, l3pid);
}
#endif

	/* Update TX statistics */
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;

	if (skb_shinfo(skb)->nr_frags) {
		EPRINTK("MHI net device doesn't support SG.\n");
		BUG();
	}

	skb->protocol = ARPHRD_ETHER;

	if (!priv->master)
		BUG();

	skb->dev = priv->master;

	eth_header(skb, skb->dev, ARPHRD_ETHER, NULL, NULL, skb->len);

#ifdef DUMP_FRAMES_DEBUG
{
	int i;
	int len = skb->len;
	u8 *ptr = skb->data;

	for (i = 0; i < len; i++) {
		if (i%8 == 0)
			DPRINTK("MHI mhi_net_device_xmit : TX [%04X] ", i);
		DPRINTK(" 0x%02X", ptr[i]);
		if (i%8 == 7 || i == len-1)
			DPRINTK("\n");
	}
}
#endif

	/* Send to NCM class */
	err = dev_queue_xmit(skb);

	if (err) {
		EPRINTK(" mhi_net_device_xmit dev_queue_xmit error DROP\n");
		goto drop;
	}

	return 0;

drop:
	dev->stats.tx_dropped++;
	dev_kfree_skb(skb);

	DPRINTK("\n mhi_net_device_xmit: dropped");

	return 0;
}


static int mhi_net_device_open(struct net_device *dev)
{
	struct mhi_data *priv = netdev_priv(dev);
	struct net_device *usb_net_device;
	int flags;

	DPRINTK("mhi_net_device_open\n");

	priv->master = NULL;

	usb_net_device = __dev_get_by_name(&init_net, "usb0");

	if (usb_net_device &&
		(usb_net_device->dev_addr[0] == 0x74) &&
		(usb_net_device->dev_addr[1] == 0x90) &&
		(usb_net_device->dev_addr[2] == 0x50)) {
		EPRINTK("Renesas HSIC netdevice mounted (usb0)\n");
		priv->master = usb_net_device;
	} else {
		usb_net_device = __dev_get_by_name(&init_net, "usb1");
		if (usb_net_device &&
			(usb_net_device->dev_addr[0] == 0x74) &&
			(usb_net_device->dev_addr[1] == 0x90) &&
			(usb_net_device->dev_addr[2] == 0x50)) {
			EPRINTK("Renesas HSIC netdevice mounted (usb1)\n");
			priv->master = usb_net_device;
		}
	}

	if (!priv->master) {
		DPRINTK(" Renesas HSIC netdevice not mounted yet\n");
		return -EAGAIN;
	}

	usbnet_pause_rx(netdev_priv(priv->master));

	flags = priv->master->flags;
	flags |= IFF_UP | IFF_RUNNING;

    /* UP usb netdevice */
	dev_change_flags(priv->master, flags);

	return 0;
}

static int mhi_device_notify(struct notifier_block *me, unsigned long what,
				void *arg)
{
	struct net_device *dev = arg;

	if ((dev->dev_addr[0] == 0x74) &&
		(dev->dev_addr[1] == 0x90) &&
		(dev->dev_addr[2] == 0x50) &&
		(what == NETDEV_CHANGE)) {
		usbnet_resume_rx(netdev_priv(dev));
	}

	return 0;
}

static struct notifier_block mhi_device_notifier = {
	.notifier_call = mhi_device_notify,
	.priority = 0,
};

static int mhi_net_device_close(struct net_device *dev)
{
	struct mhi_data *priv = netdev_priv(dev);
	int flags;

	EPRINTK("Modem crash occurs - NB modem reboot since power on : %d\n",
			++debug_nb_crash_modem);

	if (!priv->master)
		BUG();

	flags = priv->master->flags;
	flags &= ~(IFF_UP | IFF_RUNNING);

	/* Down usb netdevice*/
	dev_change_flags(priv->master, flags);

	priv->master = NULL;

	return 0;
}


static int mhi_net_device_probe(struct platform_device *dev)
{
	struct mhi_data *priv = NULL;
	struct net_device *ndev;
	int err;

	DPRINTK(" mhi_net_device_probe\n");

	ndev = alloc_netdev(sizeof(struct mhi_data),
						"mhi%d",
						mhi_net_device_setup);

	if (ndev == NULL)
		return -ENOMEM;

	priv = netdev_priv(ndev);
	priv->net_dev = ndev;

	SET_NETDEV_DEV(ndev, &dev->dev);

	err = register_netdev(ndev);

	if (err < 0) {
		dev_err(&dev->dev, "Register netdev failed (%d)\n", err);
		free_netdev(ndev);
		goto out1;
	}

	return 0;

out1:
	unregister_netdev(ndev);

	return err;
}

static int mhi_net_device_remove(struct platform_device *dev)
{
	struct mhi_data *priv = platform_get_drvdata(dev);

	DPRINTK(" mhi_net_device_remove\n");

	unregister_netdev(priv->net_dev);

	return 0;
}


static struct platform_driver mhi_net_device_driver = {
	.driver = {
		.name = "mhi_net_device"
	},
	.probe    = mhi_net_device_probe,
	.remove    = mhi_net_device_remove,
};


static int __init mhi_net_device_init(void)
{
	DPRINTK("mhi_net_device_init\n");

	platform_driver_register(&mhi_net_device_driver);

	register_netdevice_notifier(&mhi_device_notifier);

	dev_add_pack(&modem_packet_type);

	wake_lock_init(&wake_no_suspend,
			WAKE_LOCK_SUSPEND,
			"MHI no suspend after HSIC RX");

	return 0;
}

static void __exit mhi_net_device_exit(void)
{
	platform_driver_unregister(&mhi_net_device_driver);

	unregister_netdevice_notifier(&mhi_device_notifier);

	dev_remove_pack(&modem_packet_type);
}

late_initcall(mhi_net_device_init);
module_exit(mhi_net_device_exit);

MODULE_AUTHOR("RMC");
MODULE_LICENSE("GPL");
