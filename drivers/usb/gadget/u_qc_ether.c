/*
 * u_qc_ether.c -- Ethernet-over-USB link layer utilities for Gadget stack
 *
 * Copyright (C) 2003-2005,2008 David Brownell
 * Copyright (C) 2003-2004 Robert Schwebel, Benedikt Spranger
 * Copyright (C) 2008 Nokia Corporation
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
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

#include <linux/kernel.h>
#include <linux/gfp.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>

#include "u_ether.h"


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
 *
 * This utilities is based on Ethernet-over-USB link layer utilities and
 * contains MSM specific implementation.
 */

#define UETH__VERSION	"29-May-2008"

struct eth_qc_dev {
	/* lock is held while accessing port_usb
	 * or updating its backlink port_usb->ioport
	 */
	spinlock_t		lock;
	struct qc_gether		*port_usb;

	struct net_device	*net;
	struct usb_gadget	*gadget;

	unsigned		header_len;

	bool			zlp;
	u8			host_mac[ETH_ALEN];
};

/*-------------------------------------------------------------------------*/

#undef DBG
#undef VDBG
#undef ERROR
#undef INFO

#define xprintk(d, level, fmt, args...) \
	printk(level "%s: " fmt , (d)->net->name , ## args)

#ifdef DEBUG
#undef DEBUG
#define DBG(dev, fmt, args...) \
	xprintk(dev , KERN_DEBUG , fmt , ## args)
#else
#define DBG(dev, fmt, args...) \
	do { } while (0)
#endif /* DEBUG */

#ifdef VERBOSE_DEBUG
#define VDBG	DBG
#else
#define VDBG(dev, fmt, args...) \
	do { } while (0)
#endif /* DEBUG */

#define ERROR(dev, fmt, args...) \
	xprintk(dev , KERN_ERR , fmt , ## args)
#define INFO(dev, fmt, args...) \
	xprintk(dev , KERN_INFO , fmt , ## args)

/*-------------------------------------------------------------------------*/

/* NETWORK DRIVER HOOKUP (to the layer above this driver) */
static int ueth_qc_change_mtu(struct net_device *net, int new_mtu)
{
	struct eth_qc_dev	*dev = netdev_priv(net);
	unsigned long	flags;
	int		status = 0;

	/* don't change MTU on "live" link (peer won't know) */
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb)
		status = -EBUSY;
	else if (new_mtu <= ETH_HLEN || new_mtu > ETH_FRAME_LEN)
		status = -ERANGE;
	else
		net->mtu = new_mtu;
	spin_unlock_irqrestore(&dev->lock, flags);

	return status;
}

static void eth_qc_get_drvinfo(struct net_device *net,
						struct ethtool_drvinfo *p)
{
	struct eth_qc_dev	*dev = netdev_priv(net);

	strlcpy(p->driver, "g_qc_ether", sizeof p->driver);
	strlcpy(p->version, UETH__VERSION, sizeof p->version);
	strlcpy(p->fw_version, dev->gadget->name, sizeof p->fw_version);
	strlcpy(p->bus_info, dev_name(&dev->gadget->dev), sizeof p->bus_info);
}

static const struct ethtool_ops qc_ethtool_ops = {
	.get_drvinfo = eth_qc_get_drvinfo,
	.get_link = ethtool_op_get_link,
};

static netdev_tx_t eth_qc_start_xmit(struct sk_buff *skb,
					struct net_device *net)
{
	return NETDEV_TX_OK;
}

static int eth_qc_open(struct net_device *net)
{
	struct eth_qc_dev	*dev = netdev_priv(net);
	struct qc_gether	*link;

	DBG(dev, "%s\n", __func__);
	if (netif_carrier_ok(dev->net)) {
		/* Force the netif to send the RTM_NEWLINK event
		 * that in use to notify on the USB cable status.
		 */
		netif_carrier_off(dev->net);
		netif_carrier_on(dev->net);
		netif_wake_queue(dev->net);
	}

	spin_lock_irq(&dev->lock);
	link = dev->port_usb;
	if (link && link->open)
		link->open(link);
	spin_unlock_irq(&dev->lock);

	return 0;
}

static int eth_qc_stop(struct net_device *net)
{
	struct eth_qc_dev	*dev = netdev_priv(net);
	unsigned long	flags;
	struct qc_gether	*link = dev->port_usb;

	VDBG(dev, "%s\n", __func__);
	netif_stop_queue(net);

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->port_usb && link->close)
			link->close(link);
	spin_unlock_irqrestore(&dev->lock, flags);

	return 0;
}

/*-------------------------------------------------------------------------*/

/* initial value, changed by "ifconfig usb0 hw ether xx:xx:xx:xx:xx:xx" */
static char *qc_dev_addr;
module_param(qc_dev_addr, charp, S_IRUGO);
MODULE_PARM_DESC(qc_dev_addr, "QC Device Ethernet Address");

/* this address is invisible to ifconfig */
static char *qc_host_addr;
module_param(qc_host_addr, charp, S_IRUGO);
MODULE_PARM_DESC(qc_host_addr, "QC Host Ethernet Address");

static int get_qc_ether_addr(const char *str, u8 *dev_addr)
{
	if (str) {
		unsigned	i;

		for (i = 0; i < 6; i++) {
			unsigned char num;

			if ((*str == '.') || (*str == ':'))
				str++;
			num = hex_to_bin(*str++) << 4;
			num |= hex_to_bin(*str++);
			dev_addr[i] = num;
		}
		if (is_valid_ether_addr(dev_addr))
			return 0;
	}
	random_ether_addr(dev_addr);
	return 1;
}

static struct eth_qc_dev *qc_dev;

static const struct net_device_ops eth_qc_netdev_ops = {
	.ndo_open		= eth_qc_open,
	.ndo_stop		= eth_qc_stop,
	.ndo_start_xmit		= eth_qc_start_xmit,
	.ndo_change_mtu		= ueth_qc_change_mtu,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
};

static struct device_type qc_gadget_type = {
	.name	= "gadget",
};

/**
 * gether_qc_setup - initialize one ethernet-over-usb link
 * @g: gadget to associated with these links
 * @ethaddr: NULL, or a buffer in which the ethernet address of the
 *	host side of the link is recorded
 * Context: may sleep
 *
 * This sets up the single network link that may be exported by a
 * gadget driver using this framework.  The link layer addresses are
 * set up using module parameters.
 *
 * Returns negative errno, or zero on success
 */
int gether_qc_setup(struct usb_gadget *g, u8 ethaddr[ETH_ALEN])
{
	return gether_qc_setup_name(g, ethaddr, "usb");
}

/**
 * gether_qc_setup_name - initialize one ethernet-over-usb link
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
int gether_qc_setup_name(struct usb_gadget *g, u8 ethaddr[ETH_ALEN],
		const char *netname)
{
	struct eth_qc_dev		*dev;
	struct net_device	*net;
	int			status;

	if (qc_dev)
		return -EBUSY;

	net = alloc_etherdev(sizeof *dev);
	if (!net)
		return -ENOMEM;

	dev = netdev_priv(net);
	spin_lock_init(&dev->lock);

	/* network device setup */
	dev->net = net;
	snprintf(net->name, sizeof(net->name), "%s%%d", netname);

	if (get_qc_ether_addr(qc_dev_addr, net->dev_addr))
		dev_warn(&g->dev,
			"using random %s ethernet address\n", "self");
	if (get_qc_ether_addr(qc_host_addr, dev->host_mac))
		dev_warn(&g->dev,
			"using random %s ethernet address\n", "host");

	if (ethaddr)
		memcpy(ethaddr, dev->host_mac, ETH_ALEN);

	net->netdev_ops = &eth_qc_netdev_ops;

	SET_ETHTOOL_OPS(net, &qc_ethtool_ops);

	netif_carrier_off(net);

	dev->gadget = g;
	SET_NETDEV_DEV(net, &g->dev);
	SET_NETDEV_DEVTYPE(net, &qc_gadget_type);

	status = register_netdev(net);
	if (status < 0) {
		dev_dbg(&g->dev, "register_netdev failed, %d\n", status);
		free_netdev(net);
	} else {
		INFO(dev, "MAC %pM\n", net->dev_addr);
		INFO(dev, "HOST MAC %pM\n", dev->host_mac);

		qc_dev = dev;
	}

	return status;
}

/**
 * gether_qc_cleanup - remove Ethernet-over-USB device
 * Context: may sleep
 *
 * This is called to free all resources allocated by @gether_qc_setup().
 */
void gether_qc_cleanup(void)
{
	if (!qc_dev)
		return;

	unregister_netdev(qc_dev->net);
	free_netdev(qc_dev->net);

	qc_dev = NULL;
}


/**
 * gether_qc_connect - notify network layer that USB link is active
 * @link: the USB link, set up with endpoints, descriptors matching
 *	current device speed, and any framing wrapper(s) set up.
 * Context: irqs blocked
 *
 * This is called to let the network layer know the connection
 * is active ("carrier detect").
 */
struct net_device *gether_qc_connect(struct qc_gether *link)
{
	struct eth_qc_dev		*dev = qc_dev;

	if (!dev)
		return ERR_PTR(-EINVAL);

	dev->zlp = link->is_zlp_ok;
	dev->header_len = link->header_len;

	spin_lock(&dev->lock);
	dev->port_usb = link;
	link->ioport = dev;
	if (netif_running(dev->net)) {
		if (link->open)
			link->open(link);
	} else {
		if (link->close)
			link->close(link);
	}
	spin_unlock(&dev->lock);

	netif_carrier_on(dev->net);
	if (netif_running(dev->net))
		netif_wake_queue(dev->net);

	return dev->net;
}

/**
 * gether_qc_disconnect - notify network layer that USB link is inactive
 * @link: the USB link, on which gether_connect() was called
 * Context: irqs blocked
 *
 * This is called to let the network layer know the connection
 * went inactive ("no carrier").
 *
 * On return, the state is as if gether_connect() had never been called.
 */
void gether_qc_disconnect(struct qc_gether *link)
{
	struct eth_qc_dev		*dev = link->ioport;

	if (!dev)
		return;

	DBG(dev, "%s\n", __func__);

	netif_stop_queue(dev->net);
	netif_carrier_off(dev->net);

	spin_lock(&dev->lock);
	dev->port_usb = NULL;
	link->ioport = NULL;
	spin_unlock(&dev->lock);
}
