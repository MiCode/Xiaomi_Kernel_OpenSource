/* Copyright (c) 2011-2014, 2018-2020, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/mii.h>
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>
#include <linux/usb/cdc.h>
#include <linux/msm_rmnet.h>

#define RMNET_VENDOR_ID 0x05c6
#define RMNET_USB_DEV_NAME "rmnet_usb"
#define RMNET_DATA_LEN 0x4000
#define WATCHDOG_TIMEOUT (30 * HZ)

static void rmnet_usb_disable_usb_autosuspend(struct usbnet *usbnet,
					      int en_autosuspend)
{
	struct usb_device *usb_dev = usbnet->udev;

	usb_get_dev(usb_dev);
	if (!en_autosuspend)
		usb_disable_autosuspend(usb_dev);
	else
		usb_enable_autosuspend(usb_dev);

	usb_put_dev(usb_dev);
}

static int rmnet_usb_change_dtr(struct usbnet *dev, bool on)
{
	u8 intf = dev->intf->cur_altsetting->desc.bInterfaceNumber;

	return usbnet_write_cmd(dev, USB_CDC_REQ_SET_CONTROL_LINE_STATE,
			USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			on ? 0x01 : 0x00, intf, NULL, 0);
}

static int rmnet_usb_manage_power(struct usbnet *dev, int on)
{
	int rv;

	rv = usb_autopm_get_interface(dev->intf);
	dev->intf->needs_remote_wakeup = on;
	if (!rv)
		usb_autopm_put_interface(dev->intf);

	return 0;
}

static __be16 rmnet_usb_ip_type_trans(struct sk_buff *skb)
{
	__be16 protocol = 0;

	/* determine L3 protocol */
	switch (skb->data[0] & 0xf0) {
	case 0x40:
		protocol = htons(ETH_P_IP);
		break;
	case 0x60:
		protocol = htons(ETH_P_IPV6);
		break;
	default:
		/* default is QMAP */
		protocol = htons(ETH_P_MAP);
		break;
	}

	return protocol;
}

static int rmnet_usb_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	skb->protocol = rmnet_usb_ip_type_trans(skb);
	return 1;
}

static int rmnet_change_mtu(struct net_device *dev, int new_mtu)
{
	if (new_mtu < 0 || RMNET_DATA_LEN < new_mtu)
		return -EINVAL;

	dev->mtu = new_mtu;
	return 0;
}

static struct net_device_stats *rmnet_get_stats(struct net_device *dev)
{
	return &dev->stats;
}

static int rmnet_ioctl_extended(struct net_device *dev, struct ifreq *ifr)
{
	struct rmnet_ioctl_extended_s ext_cmd;
	int rc = 0;
	struct usbnet *unet = netdev_priv(dev);

	rc = copy_from_user(&ext_cmd, ifr->ifr_ifru.ifru_data,
			    sizeof(struct rmnet_ioctl_extended_s));
	if (rc)
		return rc;

	switch (ext_cmd.extended_ioctl) {
	case RMNET_IOCTL_SET_MRU:
		dev_info(&unet->intf->dev, "MRU change request to 0x%x\n",
			 ext_cmd.u.data);
		if (test_bit(EVENT_DEV_OPEN, &unet->flags)) {
			dev_err(&unet->intf->dev,
				"MRU change request failed, device already open\n");
			return -EBUSY;
		}
		/* 16K max */
		if ((size_t)ext_cmd.u.data > 0x4000) {
			dev_err(&unet->intf->dev, "MRU above 16k disallowed\n");
			return -EINVAL;
		}
		unet->rx_urb_size = (size_t)ext_cmd.u.data;
		break;
	case RMNET_IOCTL_GET_MRU:
		ext_cmd.u.data = (uint32_t)unet->rx_urb_size;
		break;
	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		ext_cmd.u.data = 0;
		break;
	case RMNET_IOCTL_GET_DRIVER_NAME:
		strlcpy(ext_cmd.u.if_name, unet->driver_name,
			sizeof(ext_cmd.u.if_name));
		break;
	case RMNET_IOCTL_SET_SLEEP_STATE:
		rmnet_usb_disable_usb_autosuspend(unet, ext_cmd.u.data);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	rc = copy_to_user(ifr->ifr_ifru.ifru_data, &ext_cmd,
			  sizeof(struct rmnet_ioctl_extended_s));
	return rc;
}

static int rmnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int rc = 0;
	struct rmnet_ioctl_data_s ioctl_data;

	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_IP: /* set RAWIP protocol */
		break;
	case RMNET_IOCTL_GET_LLP: /* get link protocol state */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
				 sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_GET_OPMODE: /* get operation mode */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
				 sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_SET_QOS_ENABLE:
		rc = -EINVAL;
		break;
	case RMNET_IOCTL_SET_QOS_DISABLE:
		rc = 0;
		break;
	case RMNET_IOCTL_OPEN:
		rc = usbnet_open(dev);
		break;
	case RMNET_IOCTL_CLOSE:
		rc = usbnet_stop(dev);
		break;
	case RMNET_IOCTL_EXTENDED:
		rc = rmnet_ioctl_extended(dev, ifr);
		break;
	default:
		/* don't fail any IOCTL right now */
		rc = 0;
		break;
	}

	return rc;
}

static const struct net_device_ops rmnet_usb_ops_ip = {
	.ndo_open = usbnet_open,
	.ndo_stop = usbnet_stop,
	.ndo_start_xmit = usbnet_start_xmit,
	.ndo_get_stats = rmnet_get_stats,
	.ndo_tx_timeout = usbnet_tx_timeout,
	.ndo_do_ioctl = rmnet_ioctl,
	.ndo_change_mtu = rmnet_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

static void rmnet_usb_setup(struct net_device *dev)
{
	dev->header_ops	= NULL; /* No header */
	dev->type = ARPHRD_NONE;
	dev->hard_header_len = 0;
	dev->addr_len = 0;
	dev->netdev_ops = &rmnet_usb_ops_ip;
	dev->mtu = RMNET_DATA_LEN;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
	dev->watchdog_timeo = WATCHDOG_TIMEOUT;
}

static int rmnet_usb_status(struct seq_file *s, void *unused)
{
	struct usbnet *unet = s->private;

	seq_printf(s, "Net MTU: %u\n", unet->net->mtu);
	seq_printf(s, "rx_urb_size: %lu\n", unet->rx_urb_size);
	seq_printf(s, "rx skb q len: %u\n", unet->rxq.qlen);
	seq_printf(s, "rx skb done q len: %u\n", unet->done.qlen);
	seq_printf(s, "rx errors: %lu\n", unet->net->stats.rx_errors);
	seq_printf(s, "rx over errors: %lu\n",
		   unet->net->stats.rx_over_errors);
	seq_printf(s, "rx length errors: %lu\n",
		   unet->net->stats.rx_length_errors);
	seq_printf(s, "rx packets: %lu\n", unet->net->stats.rx_packets);
	seq_printf(s, "rx bytes: %lu\n", unet->net->stats.rx_bytes);
	seq_printf(s, "tx skb q len: %u\n", unet->txq.qlen);
	seq_printf(s, "tx errors: %lu\n", unet->net->stats.tx_errors);
	seq_printf(s, "tx packets: %lu\n", unet->net->stats.tx_packets);
	seq_printf(s, "tx bytes: %lu\n", unet->net->stats.tx_bytes);
	seq_printf(s, "EVENT_DEV_OPEN: %d\n",
		   test_bit(EVENT_DEV_OPEN, &unet->flags));
	seq_printf(s, "EVENT_TX_HALT: %d\n",
		   test_bit(EVENT_TX_HALT, &unet->flags));
	seq_printf(s, "EVENT_RX_HALT: %d\n",
		   test_bit(EVENT_RX_HALT, &unet->flags));
	seq_printf(s, "EVENT_RX_MEMORY: %d\n",
		   test_bit(EVENT_RX_MEMORY, &unet->flags));
	seq_printf(s, "EVENT_DEV_ASLEEP: %d\n",
		   test_bit(EVENT_DEV_ASLEEP, &unet->flags));

	return 0;
}

static int rmnet_usb_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, rmnet_usb_status, inode->i_private);
}

const struct file_operations rmnet_usb_fops = {
	.open = rmnet_usb_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int rmnet_usb_debugfs_init(struct usbnet *unet)
{
	struct dentry *rmnet_usb_dbg_root;
	struct dentry *rmnet_usb_dentry;

	rmnet_usb_dbg_root = debugfs_create_dir(unet->net->name, NULL);
	if (!rmnet_usb_dbg_root || IS_ERR(rmnet_usb_dbg_root))
		return -ENODEV;

	rmnet_usb_dentry = debugfs_create_file("status",
					       0644,
					       rmnet_usb_dbg_root,
					       unet,
					       &rmnet_usb_fops);
	if (!rmnet_usb_dentry) {
		debugfs_remove_recursive(rmnet_usb_dbg_root);
		return -ENODEV;
	}

	unet->data[0] = (unsigned long)rmnet_usb_dbg_root;

	return 0;
}

static void rmnet_usb_debugfs_cleanup(struct usbnet *unet)
{
	struct dentry *root = (struct dentry *)unet->data[0];

	debugfs_remove_recursive(root);
	unet->data[0] = 0;
}

static int rmnet_usb_bind(struct usbnet *dev, struct usb_interface *iface)
{
	struct usb_host_endpoint *endpoint = NULL;
	struct usb_host_endpoint *bulk_in = NULL;
	struct usb_host_endpoint *bulk_out = NULL;
	struct usb_host_endpoint *int_in = NULL;
	struct driver_info *info = NULL;
	int status = 0;
	int i;
	int numends;
	u8 intf;

	numends = iface->cur_altsetting->desc.bNumEndpoints;
	for (i = 0; i < numends; i++) {
		endpoint = iface->cur_altsetting->endpoint + i;
		if (!endpoint) {
			dev_err(&iface->dev, "%s: invalid endpoint %u\n",
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
		dev_err(&iface->dev, "%s: invalid endpoints\n", __func__);
		status = -EINVAL;
		goto out;
	}
	dev->in = usb_rcvbulkpipe(dev->udev,
				  bulk_in->desc.bEndpointAddress &
				  USB_ENDPOINT_NUMBER_MASK);
	dev->out = usb_sndbulkpipe(dev->udev,
				   bulk_out->desc.bEndpointAddress &
				   USB_ENDPOINT_NUMBER_MASK);
	dev->status = int_in;
	info = dev->driver_info;
	snprintf(dev->net->name, IFNAMSIZ, "%s%c", RMNET_USB_DEV_NAME,
		 info->description[strlen(info->description) - 1]);
	intf = iface->cur_altsetting->desc.bInterfaceNumber;

	/* Enable remote wakeup and set DTR (data terminal ready)
	 * to high to enable data calls
	 */
	rmnet_usb_manage_power(dev, 1);
	rmnet_usb_change_dtr(dev, true);

	dev->hard_mtu = 2048;

out:
	return status;
}

static void rmnet_usb_unbind(struct usbnet *dev, struct usb_interface *iface)
{
	/* Disable remote wakeup and set DTR (data terminal ready)
	 * to low to disable data calls
	 */
	rmnet_usb_manage_power(dev, 0);
	rmnet_usb_change_dtr(dev, false);
}

static int rmnet_usb_probe(struct usb_interface *iface,
			   const struct usb_device_id *prod)
{
	struct usbnet *unet = NULL;
	struct usb_device *udev = NULL;
	int status = 0;

	udev = interface_to_usbdev(iface);

	if (iface->num_altsetting != 1) {
		dev_err(&iface->dev, "%s invalid num_altsetting %u\n",
			__func__, iface->num_altsetting);
		return -EINVAL;
	}

	status = usbnet_probe(iface, prod);
	if (status < 0) {
		dev_err(&iface->dev, "usbnet_probe failed %d\n",
			status);
		return status;
	}

	unet = usb_get_intfdata(iface);

	/* update net device */
	rmnet_usb_setup(unet->net);

	status = rmnet_usb_debugfs_init(unet);
	if (status)
		dev_dbg(&unet->intf->dev,
			"mode debugfs file is not available\n");

	usb_enable_autosuspend(udev);

	if (udev->parent && !udev->parent->parent) {
		/* allow modem and roothub to wake up suspended system */
		device_set_wakeup_enable(&udev->dev, 1);
		device_set_wakeup_enable(&udev->parent->dev, 1);
	}

	return 0;
}

static void rmnet_usb_disconnect(struct usb_interface *intf)
{
	struct usbnet *unet = usb_get_intfdata(intf);

	device_set_wakeup_enable(&unet->udev->dev, 0);
	rmnet_usb_debugfs_cleanup(unet);
	usbnet_disconnect(intf);
}

static const struct driver_info rmnet_usb0_info = {
	.description = "RmNET device 0",
	.flags = FLAG_SEND_ZLP,
	.bind = rmnet_usb_bind,
	.unbind = rmnet_usb_unbind,
	.manage_power = rmnet_usb_manage_power,
	.rx_fixup = rmnet_usb_rx_fixup,
};

static const struct driver_info rmnet_usb1_info = {
	.description = "RmNET device 1",
	.flags = FLAG_SEND_ZLP,
	.bind = rmnet_usb_bind,
	.unbind = rmnet_usb_unbind,
	.manage_power = rmnet_usb_manage_power,
	.rx_fixup = rmnet_usb_rx_fixup,
};

static const struct usb_device_id rmnet_usb_ids[] = {
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x90EF, 2),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x90F0, 2),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x90F3, 1),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x90FD, 2),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x90FD, 3),
		.driver_info = (unsigned long)&rmnet_usb1_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x9102, 2),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x9102, 3),
		.driver_info = (unsigned long)&rmnet_usb1_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x9103, 2),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x9103, 3),
		.driver_info = (unsigned long)&rmnet_usb1_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x9106, 2),
		.driver_info = (unsigned long)&rmnet_usb1_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x9107, 2),
		.driver_info = (unsigned long)&rmnet_usb1_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x910A, 1),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{
		USB_DEVICE_INTERFACE_NUMBER(RMNET_VENDOR_ID, 0x910B, 1),
		.driver_info = (unsigned long)&rmnet_usb0_info,
	},
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, rmnet_usb_ids);

static struct usb_driver rmnet_usb_driver = {
	.name = "rmnet_usb",
	.id_table = rmnet_usb_ids,
	.probe = rmnet_usb_probe,
	.disconnect = rmnet_usb_disconnect,
	.suspend = usbnet_suspend,
	.resume = usbnet_resume,
	.reset_resume = usbnet_resume,
	.supports_autosuspend = 1,
	.disable_hub_initiated_lpm = 1,
};

module_usb_driver(rmnet_usb_driver);

MODULE_DESCRIPTION("QTI RmNet USB driver");
MODULE_LICENSE("GPL v2");
