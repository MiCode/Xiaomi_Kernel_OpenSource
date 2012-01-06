/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/mii.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>
#include <linux/msm_rmnet.h>

#include "rmnet_usb_ctrl.h"

#define RMNET_DATA_LEN			2000
#define HEADROOM_FOR_QOS		8

static int	data_msg_dbg_mask;

enum {
	DEBUG_MASK_LVL0 = 1U << 0,
	DEBUG_MASK_LVL1 = 1U << 1,
	DEBUG_MASK_LVL2 = 1U << 2,
};

#define DBG(m, x...) do { \
		if (data_msg_dbg_mask & m) \
			pr_info(x); \
} while (0)

/*echo dbg_mask > /sys/class/net/rmnet_usbx/dbg_mask*/
static ssize_t dbg_mask_store(struct device *d,
		struct device_attribute *attr,
		const char *buf, size_t n)
{
	unsigned int		dbg_mask;
	struct net_device	*dev = to_net_dev(d);
	struct usbnet		*unet = netdev_priv(dev);

	if (!dev)
		return -ENODEV;

	sscanf(buf, "%u", &dbg_mask);
	/*enable dbg msgs for data driver*/
	data_msg_dbg_mask = dbg_mask;

	/*set default msg level*/
	unet->msg_enable = NETIF_MSG_DRV | NETIF_MSG_PROBE | NETIF_MSG_LINK;

	/*enable netif_xxx msgs*/
	if (dbg_mask & DEBUG_MASK_LVL0)
		unet->msg_enable |= NETIF_MSG_IFUP | NETIF_MSG_IFDOWN;
	if (dbg_mask & DEBUG_MASK_LVL1)
		unet->msg_enable |= NETIF_MSG_TX_ERR | NETIF_MSG_RX_ERR
			| NETIF_MSG_TX_QUEUED | NETIF_MSG_TX_DONE
			| NETIF_MSG_RX_STATUS;

	return n;
}

static ssize_t dbg_mask_show(struct device *d,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", data_msg_dbg_mask);
}

static DEVICE_ATTR(dbg_mask, 0644, dbg_mask_show, dbg_mask_store);

#define DBG0(x...) DBG(DEBUG_MASK_LVL0, x)
#define DBG1(x...) DBG(DEBUG_MASK_LVL1, x)
#define DBG2(x...) DBG(DEBUG_MASK_LVL2, x)

static void rmnet_usb_setup(struct net_device *);
static int rmnet_ioctl(struct net_device *, struct ifreq *, int);

static int rmnet_usb_suspend(struct usb_interface *iface, pm_message_t message)
{
	struct usbnet		*unet;
	struct rmnet_ctrl_dev	*dev;
	int			time = 0;
	int			retval = 0;

	unet = usb_get_intfdata(iface);
	if (!unet) {
		pr_err("%s:data device not found\n", __func__);
		retval = -ENODEV;
		goto fail;
	}

	dev = (struct rmnet_ctrl_dev *)unet->data[1];
	if (!dev) {
		dev_err(&unet->udev->dev, "%s: ctrl device not found\n",
				__func__);
		retval = -ENODEV;
		goto fail;
	}

	retval = usbnet_suspend(iface, message);
	if (!retval) {
		if (message.event & PM_EVENT_SUSPEND) {
			time = usb_wait_anchor_empty_timeout(&dev->tx_submitted,
								1000);
			if (!time)
				usb_kill_anchored_urbs(&dev->tx_submitted);

			retval = rmnet_usb_ctrl_stop_rx(dev);
			iface->dev.power.power_state.event = message.event;
		}
		/*  TBD : do we need to set/clear usbnet->udev->reset_resume*/
		} else
		dev_dbg(&unet->udev->dev,
			"%s: device is busy can not suspend\n", __func__);

fail:
	return retval;
}

static int rmnet_usb_resume(struct usb_interface *iface)
{
	int			retval = 0;
	int			oldstate;
	struct usbnet		*unet;
	struct rmnet_ctrl_dev	*dev;

	unet = usb_get_intfdata(iface);
	if (!unet) {
		pr_err("%s:data device not found\n", __func__);
		retval = -ENODEV;
		goto fail;
	}

	dev = (struct rmnet_ctrl_dev *)unet->data[1];
	if (!dev) {
		dev_err(&unet->udev->dev, "%s: ctrl device not found\n",
				__func__);
		retval = -ENODEV;
		goto fail;
	}
	oldstate = iface->dev.power.power_state.event;
	iface->dev.power.power_state.event = PM_EVENT_ON;

	retval = usbnet_resume(iface);
	if (!retval) {

		if (oldstate & PM_EVENT_SUSPEND)
			retval = rmnet_usb_ctrl_start(dev);
	}
fail:
	return retval;
}

static int rmnet_usb_bind(struct usbnet *usbnet, struct usb_interface *iface)
{
	struct usb_host_endpoint	*endpoint = NULL;
	struct usb_host_endpoint	*bulk_in = NULL;
	struct usb_host_endpoint	*bulk_out = NULL;
	struct usb_host_endpoint	*int_in = NULL;
	struct usb_device		*udev;
	int				status = 0;
	int				i;
	int				numends;

	udev = interface_to_usbdev(iface);
	numends = iface->cur_altsetting->desc.bNumEndpoints;
	for (i = 0; i < numends; i++) {
		endpoint = iface->cur_altsetting->endpoint + i;
		if (!endpoint) {
			dev_err(&udev->dev, "%s: invalid endpoint %u\n",
				__func__, i);
			status = -EINVAL;
			goto out;
		}
		if (usb_endpoint_is_bulk_in(&endpoint->desc))
			bulk_in = endpoint;
		else if (usb_endpoint_is_bulk_out(&endpoint->desc))
			bulk_out = endpoint;
		else if (usb_endpoint_is_int_in(&endpoint->desc))
			int_in = endpoint;
	}

	if (!bulk_in || !bulk_out || !int_in) {
		dev_err(&udev->dev, "%s: invalid endpoints\n", __func__);
		status = -EINVAL;
		goto out;
	}
	usbnet->in = usb_rcvbulkpipe(usbnet->udev,
		bulk_in->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	usbnet->out = usb_sndbulkpipe(usbnet->udev,
		bulk_out->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	usbnet->status = int_in;

	/*change name of net device to rmnet_usbx here*/
	strlcpy(usbnet->net->name, "rmnet_usb%d", IFNAMSIZ);

	/*TBD: update rx_urb_size, curently set to eth frame len by usbnet*/
out:
	return status;
}

static struct sk_buff *rmnet_usb_tx_fixup(struct usbnet *dev,
		struct sk_buff *skb, gfp_t flags)
{
	struct QMI_QOS_HDR_S	*qmih;

	if (test_bit(RMNET_MODE_QOS, &dev->data[0])) {
		qmih = (struct QMI_QOS_HDR_S *)
		skb_push(skb, sizeof(struct QMI_QOS_HDR_S));
		qmih->version = 1;
		qmih->flags = 0;
		qmih->flow_id = skb->mark;
	 }

	DBG1("[%s] Tx packet #%lu len=%d mark=0x%x\n",
	    dev->net->name, dev->net->stats.tx_packets, skb->len, skb->mark);

	return skb;
}

static __be16 rmnet_ip_type_trans(struct sk_buff *skb,
	struct net_device *dev)
{
	__be16	protocol = 0;

	skb->dev = dev;

	switch (skb->data[0] & 0xf0) {
	case 0x40:
		protocol = htons(ETH_P_IP);
		break;
	case 0x60:
		protocol = htons(ETH_P_IPV6);
		break;
	default:
		pr_err("[%s] rmnet_recv() L3 protocol decode error: 0x%02x",
		       dev->name, skb->data[0] & 0xf0);
	}

	return protocol;
}

static int rmnet_usb_rx_fixup(struct usbnet *dev,
	struct sk_buff *skb)
{

	if (test_bit(RMNET_MODE_LLP_IP, &dev->data[0]))
		skb->protocol = rmnet_ip_type_trans(skb, dev->net);
	else /*set zero for eth mode*/
		skb->protocol = 0;

	DBG1("[%s] Rx packet #%lu len=%d\n",
		dev->net->name, dev->net->stats.rx_packets, skb->len);

	return 1;
}

static int rmnet_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || RMNET_DATA_LEN < new_mtu)
		return -EINVAL;

	DBG0("[%s] MTU change: old=%d new=%d\n", dev->name, dev->mtu, new_mtu);

	dev->mtu = new_mtu;

	return 0;
}

static struct net_device_stats *rmnet_get_stats(struct net_device *dev)
{
		return &dev->stats;
}

static const struct net_device_ops rmnet_usb_ops_ether = {
	.ndo_open = usbnet_open,
	.ndo_stop = usbnet_stop,
	.ndo_start_xmit = usbnet_start_xmit,
	.ndo_get_stats = rmnet_get_stats,
	/*.ndo_set_multicast_list = rmnet_set_multicast_list,*/
	.ndo_tx_timeout = usbnet_tx_timeout,
	.ndo_do_ioctl = rmnet_ioctl,
	.ndo_change_mtu = usbnet_change_mtu,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
};

static const struct net_device_ops rmnet_usb_ops_ip = {
	.ndo_open = usbnet_open,
	.ndo_stop = usbnet_stop,
	.ndo_start_xmit = usbnet_start_xmit,
	.ndo_get_stats = rmnet_get_stats,
	/*.ndo_set_multicast_list = rmnet_set_multicast_list,*/
	.ndo_tx_timeout = usbnet_tx_timeout,
	.ndo_do_ioctl = rmnet_ioctl,
	.ndo_change_mtu = rmnet_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};


static int rmnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct usbnet	*unet = netdev_priv(dev);
	u32		old_opmode;
	int		prev_mtu = dev->mtu;
	int		rc = 0;

	old_opmode = unet->data[0]; /*data[0] saves operation mode*/
	/* Process IOCTL command */
	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_ETHERNET:	/*Set Ethernet protocol*/
		/* Perform Ethernet config only if in IP mode currently*/
		if (test_bit(RMNET_MODE_LLP_IP, &unet->data[0])) {
			ether_setup(dev);
			random_ether_addr(dev->dev_addr);
			dev->mtu = prev_mtu;
			dev->netdev_ops = &rmnet_usb_ops_ether;
			clear_bit(RMNET_MODE_LLP_IP, &unet->data[0]);
			set_bit(RMNET_MODE_LLP_ETH, &unet->data[0]);
			DBG0("[%s] rmnet_ioctl(): set Ethernet protocol mode\n",
					dev->name);
		}
		break;

	case RMNET_IOCTL_SET_LLP_IP:		/* Set RAWIP protocol*/
		/* Perform IP config only if in Ethernet mode currently*/
		if (test_bit(RMNET_MODE_LLP_ETH, &unet->data[0])) {

			/* Undo config done in ether_setup() */
			dev->header_ops = 0;  /* No header */
			dev->type = ARPHRD_RAWIP;
			dev->hard_header_len = 0;
			dev->mtu = prev_mtu;
			dev->addr_len = 0;
			dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
			dev->needed_headroom = HEADROOM_FOR_QOS;
			dev->netdev_ops = &rmnet_usb_ops_ip;
			clear_bit(RMNET_MODE_LLP_ETH, &unet->data[0]);
			set_bit(RMNET_MODE_LLP_IP, &unet->data[0]);
			DBG0("[%s] rmnet_ioctl(): set IP protocol mode\n",
					dev->name);
		}
		break;

	case RMNET_IOCTL_GET_LLP:	/* Get link protocol state */
		ifr->ifr_ifru.ifru_data = (void *)(unet->data[0]
						& (RMNET_MODE_LLP_ETH
						| RMNET_MODE_LLP_IP));
		break;

	case RMNET_IOCTL_SET_QOS_ENABLE:	/* Set QoS header enabled*/
		set_bit(RMNET_MODE_QOS, &unet->data[0]);
		DBG0("[%s] rmnet_ioctl(): set QMI QOS header enable\n",
				dev->name);
		break;

	case RMNET_IOCTL_SET_QOS_DISABLE:	/* Set QoS header disabled */
		clear_bit(RMNET_MODE_QOS, &unet->data[0]);
		DBG0("[%s] rmnet_ioctl(): set QMI QOS header disable\n",
				dev->name);
		break;

	case RMNET_IOCTL_GET_QOS:		/* Get QoS header state */
		ifr->ifr_ifru.ifru_data = (void *)(unet->data[0]
						& RMNET_MODE_QOS);
		break;

	case RMNET_IOCTL_GET_OPMODE:		/* Get operation mode*/
		ifr->ifr_ifru.ifru_data = (void *)unet->data[0];
		break;

	case RMNET_IOCTL_OPEN:			/* Open transport port */
		rc = usbnet_open(dev);
		DBG0("[%s] rmnet_ioctl(): open transport port\n", dev->name);
		break;

	case RMNET_IOCTL_CLOSE:			/* Close transport port*/
		rc = usbnet_stop(dev);
		DBG0("[%s] rmnet_ioctl(): close transport port\n", dev->name);
		break;

	default:
		dev_err(&unet->udev->dev, "[%s] error: "
			"rmnet_ioct called for unsupported cmd[%d]",
			dev->name, cmd);
		return -EINVAL;
	}

	DBG2("[%s] %s: cmd=0x%x opmode old=0x%08x new=0x%08lx\n",
		dev->name, __func__, cmd, old_opmode, unet->data[0]);

	return rc;
}

static void rmnet_usb_setup(struct net_device *dev)
{
	/* Using Ethernet mode by default */
	dev->netdev_ops = &rmnet_usb_ops_ether;

	/* set this after calling ether_setup */
	dev->mtu = RMNET_DATA_LEN;

	dev->needed_headroom = HEADROOM_FOR_QOS;
	random_ether_addr(dev->dev_addr);
	dev->watchdog_timeo = 1000; /* 10 seconds? */
}

static int rmnet_usb_probe(struct usb_interface *iface,
		const struct usb_device_id *prod)
{
	struct usbnet		*unet;
	struct usb_device	*udev;
	struct driver_info	*info;
	unsigned int		iface_num;
	static int		first_rmnet_iface_num = -EINVAL;
	int			status = 0;

	udev = interface_to_usbdev(iface);
	iface_num = iface->cur_altsetting->desc.bInterfaceNumber;
	if (iface->num_altsetting != 1) {
		dev_err(&udev->dev, "%s invalid num_altsetting %u\n",
			__func__, iface->num_altsetting);
		status = -EINVAL;
		goto out;
	}

	info = (struct driver_info *)prod->driver_info;
	if (!test_bit(iface_num, &info->data))
		return -ENODEV;

	status = usbnet_probe(iface, prod);
	if (status < 0) {
		dev_err(&udev->dev, "usbnet_probe failed %d\n", status);
		goto out;
	}
	unet = usb_get_intfdata(iface);

	/*set rmnet operation mode to eth by default*/
	set_bit(RMNET_MODE_LLP_ETH, &unet->data[0]);

	/*update net device*/
	rmnet_usb_setup(unet->net);

	/*create /sys/class/net/rmnet_usbx/dbg_mask*/
	status = device_create_file(&unet->net->dev, &dev_attr_dbg_mask);
	if (status)
		goto out;

	if (first_rmnet_iface_num == -EINVAL)
		first_rmnet_iface_num = iface_num;

	/*save control device intstance */
	unet->data[1] = (unsigned long)ctrl_dev	\
			[iface_num - first_rmnet_iface_num];

	status = rmnet_usb_ctrl_probe(iface, unet->status,
		(struct rmnet_ctrl_dev *)unet->data[1]);
out:
	return status;
}

static void rmnet_usb_disconnect(struct usb_interface *intf)
{
	struct usbnet		*unet;
	struct usb_device	*udev;
	struct rmnet_ctrl_dev	*dev;

	udev = interface_to_usbdev(intf);

	unet = usb_get_intfdata(intf);
	if (!unet) {
		dev_err(&udev->dev, "%s:data device not found\n", __func__);
		return;
	}

	dev = (struct rmnet_ctrl_dev *)unet->data[1];
	if (!dev) {
		dev_err(&udev->dev, "%s:ctrl device not found\n", __func__);
		return;
	}
	unet->data[0] = 0;
	unet->data[1] = 0;
	rmnet_usb_ctrl_disconnect(dev);
	device_remove_file(&unet->net->dev, &dev_attr_dbg_mask);
	usbnet_disconnect(intf);
}

/*bit position represents interface number*/
#define PID9034_IFACE_MASK	0xF0
#define PID9048_IFACE_MASK	0x1E0

static const struct driver_info rmnet_info_pid9034 = {
	.description   = "RmNET net device",
	.bind          = rmnet_usb_bind,
	.tx_fixup      = rmnet_usb_tx_fixup,
	.rx_fixup      = rmnet_usb_rx_fixup,
	.data          = PID9034_IFACE_MASK,
};

static const struct driver_info rmnet_info_pid9048 = {
	.description   = "RmNET net device",
	.bind          = rmnet_usb_bind,
	.tx_fixup      = rmnet_usb_tx_fixup,
	.rx_fixup      = rmnet_usb_rx_fixup,
	.data          = PID9048_IFACE_MASK,
};

static const struct usb_device_id vidpids[] = {
	{
		USB_DEVICE(0x05c6, 0x9034), /* MDM9x15*/
		.driver_info = (unsigned long)&rmnet_info_pid9034,
	},
	{
		USB_DEVICE(0x05c6, 0x9048), /* MDM9x15*/
		.driver_info = (unsigned long)&rmnet_info_pid9048,
	},

	{ }, /* Terminating entry */
};

MODULE_DEVICE_TABLE(usb, vidpids);

static struct usb_driver rmnet_usb = {
	.name       = "rmnet_usb",
	.id_table   = vidpids,
	.probe      = rmnet_usb_probe,
	.disconnect = rmnet_usb_disconnect,
	.suspend    = rmnet_usb_suspend,
	.resume     = rmnet_usb_resume,
	.supports_autosuspend = true,
};

static int __init rmnet_usb_init(void)
{
	int	retval;

	retval = usb_register(&rmnet_usb);
	if (retval) {
		err("usb_register failed: %d", retval);
		return retval;
	}
	/* initialize rmnet ctrl device here*/
	retval = rmnet_usb_ctrl_init();
	if (retval) {
		usb_deregister(&rmnet_usb);
		err("rmnet_usb_cmux_init failed: %d", retval);
		return retval;
	}

	return 0;
}
module_init(rmnet_usb_init);

static void __exit rmnet_usb_exit(void)
{
	rmnet_usb_ctrl_exit();
	usb_deregister(&rmnet_usb);
}
module_exit(rmnet_usb_exit);

MODULE_DESCRIPTION("msm rmnet usb device");
MODULE_LICENSE("GPL v2");
