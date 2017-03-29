/*
 * ASIX AX88179 based USB 3.0 Ethernet Devices
 * Copyright (C) 2003-2005 David Hollis <dhollis@davehollis.com>
 * Copyright (C) 2005 Phil Chang <pchang23@sbcglobal.net>
 * Copyright (c) 2002-2003 TiVo Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* debug messages, extra info */
/* #define	DEBUG */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/usb.h>
#include <linux/crc32.h>
#include <linux/if_vlan.h>

#include <linux/usb/usbnet.h>

#include "ax88179_178a.h"

#define DRV_VERSION	"1.16.0"

static char version[] =
KERN_INFO "ASIX USB Ethernet Adapter:v" DRV_VERSION
/*	" " __TIME__ " " __DATE__ "\n" */
"		http://www.asix.com.tw\n";

static int msg_enable;
module_param(msg_enable, int, 0);
MODULE_PARM_DESC(msg_enable, "usbnet msg_enable");

static int bsize = -1;
module_param(bsize, int, 0);
MODULE_PARM_DESC(bsize, "RX Bulk IN Queue Size");

static int ifg = -1;
module_param(ifg, int, 0);
MODULE_PARM_DESC(ifg, "RX Bulk IN Inter Frame Gap");

/* EEE advertisement is disabled in default setting */
static int bEEE;
module_param(bEEE, int, 0);
MODULE_PARM_DESC(bEEE, "EEE advertisement configuration");

/* Green ethernet advertisement is disabled in default setting */
static int bGETH;
module_param(bGETH, int, 0);
MODULE_PARM_DESC(bGETH, "Green ethernet configuration");
/* ASIX AX88179/178A based USB 3.0/2.0 Gigabit Ethernet Devices */

static int __ax88179_read_cmd(struct usbnet *dev, u8 cmd, u16 value, u16 index,
			      u16 size, void *data, int in_pm)
{
	int ret;
	int (*fn)(struct usbnet *, u8, u8, u16, u16, void *, u16);

	BUG_ON(!dev);

	if (!in_pm)
		fn = usbnet_read_cmd;
	else
		fn = usbnet_read_cmd_nopm;

	ret = fn(dev, cmd, USB_DIR_IN | USB_TYPE_VENDOR |
		 USB_RECIP_DEVICE, value, index, data, size);

	if (unlikely(ret < 0))
		netdev_warn(dev->net, "Failed to read reg index 0x%04x: %d\n",
			    index, ret);
	return ret;
}

static int __ax88179_write_cmd(struct usbnet *dev, u8 cmd, u16 value, u16 index,
			       u16 size, void *data, int in_pm)
{
	int ret;
	int (*fn)(struct usbnet *, u8, u8, u16, u16, const void *, u16);

	BUG_ON(!dev);

	if (!in_pm)
		fn = usbnet_write_cmd;
	else
		fn = usbnet_write_cmd_nopm;

	ret = fn(dev, cmd, USB_DIR_OUT | USB_TYPE_VENDOR |
		 USB_RECIP_DEVICE, value, index, data, size);

	if (unlikely(ret < 0))
		netdev_warn(dev->net, "Failed to write reg index 0x%04x: %d\n",
			    index, ret);
	return ret;
}

static int ax88179_read_cmd_nopm(struct usbnet *dev, u8 cmd, u16 value,
				 u16 index, u16 size, void *data, int eflag)
{
	int ret;

	if (eflag && (2 == size)) {
		u16 buf = 0;
		ret = __ax88179_read_cmd(dev, cmd, value, index, size, &buf, 1);
		le16_to_cpus(&buf);
		*((u16 *)data) = buf;
	} else if (eflag && (4 == size)) {
		u32 buf = 0;
		ret = __ax88179_read_cmd(dev, cmd, value, index, size, &buf, 1);
		le32_to_cpus(&buf);
		*((u32 *)data) = buf;
	} else {
		ret = __ax88179_read_cmd(dev, cmd, value, index, size, data, 1);
	}

	return ret;
}

static int ax88179_write_cmd_nopm(struct usbnet *dev, u8 cmd, u16 value,
				  u16 index, u16 size, void *data)
{
	int ret;

	if (2 == size) {
		u16 buf = 0;
		buf = *((u16 *)data);
		cpu_to_le16s(&buf);
		ret = __ax88179_write_cmd(dev, cmd, value, index,
					  size, &buf, 1);
	} else {
		ret = __ax88179_write_cmd(dev, cmd, value, index,
					  size, data, 1);
	}

	return ret;
}

static int ax88179_read_cmd(struct usbnet *dev, u8 cmd, u16 value, u16 index,
			    u16 size, void *data, int eflag)
{
	int ret;

	if (eflag && (2 == size)) {
		u16 buf = 0;
		ret = __ax88179_read_cmd(dev, cmd, value, index, size, &buf, 0);
		le16_to_cpus(&buf);
		*((u16 *)data) = buf;
	} else if (eflag && (4 == size)) {
		u32 buf = 0;
		ret = __ax88179_read_cmd(dev, cmd, value, index, size, &buf, 0);
		le32_to_cpus(&buf);
		*((u32 *)data) = buf;
	} else {
		ret = __ax88179_read_cmd(dev, cmd, value, index, size, data, 0);
	}

	return ret;
}

static int ax88179_write_cmd(struct usbnet *dev, u8 cmd, u16 value, u16 index,
			     u16 size, void *data)
{
	int ret;

	if (2 == size) {
		u16 buf = 0;
		buf = *((u16 *)data);
		cpu_to_le16s(&buf);
		ret = __ax88179_write_cmd(dev, cmd, value, index,
					  size, &buf, 0);
	} else {
		ret = __ax88179_write_cmd(dev, cmd, value, index,
					  size, data, 0);
	}

	return ret;
}

static void ax88179_async_cmd_callback(struct urb *urb)
{
	struct ax88179_async_handle *asyncdata =
		(struct ax88179_async_handle *)urb->context;

	if (urb->status < 0)
		pr_err("ax88179_async_cmd_callback() failed with %d",
		       urb->status);

	kfree(asyncdata->req);
	kfree(asyncdata);
	usb_free_urb(urb);
}

static void
ax88179_write_cmd_async(struct usbnet *dev, u8 cmd, u16 value, u16 index,
			u16 size, void *data)
{
	struct usb_ctrlrequest *req = NULL;
	int status = 0;
	struct urb *urb = NULL;
	void *buf = NULL;
	struct ax88179_async_handle *asyncdata = NULL;

	urb = usb_alloc_urb(0, GFP_ATOMIC);
	if (urb == NULL) {
		netdev_err(dev->net, "Error allocating URB in write_cmd_async!");
		return;
	}

	req = kmalloc(sizeof(*req), GFP_ATOMIC);
	if (req == NULL) {
		netdev_err(dev->net, "Failed to allocate memory for control request");
		usb_free_urb(urb);
		return;
	}

	asyncdata = (struct ax88179_async_handle *)
		kmalloc(sizeof(struct ax88179_async_handle), GFP_ATOMIC);
	if (asyncdata == NULL) {
		netdev_err(dev->net, "Failed to allocate memory for async data");
		kfree(req);
		usb_free_urb(urb);
		return;
	}

	asyncdata->req = req;

	if (size == 2) {
		asyncdata->rxctl = *((u16 *)data);
		cpu_to_le16s(&asyncdata->rxctl);
		buf = &asyncdata->rxctl;
	} else {
		memcpy(asyncdata->m_filter, data, size);
		buf = asyncdata->m_filter;
	}

	req->bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE;
	req->bRequest = cmd;
	req->wValue = cpu_to_le16(value);
	req->wIndex = cpu_to_le16(index);
	req->wLength = cpu_to_le16(size);

	usb_fill_control_urb(urb, dev->udev,
			     usb_sndctrlpipe(dev->udev, 0),
			     (void *)req, buf, size,
			     ax88179_async_cmd_callback, asyncdata);

	status = usb_submit_urb(urb, GFP_ATOMIC);
	if (status < 0) {
		netdev_err(dev->net, "Error submitting the control message: status=%d",
			   status);
		kfree(req);
		kfree(asyncdata);
		usb_free_urb(urb);
	}
}

static void ax88179_status(struct usbnet *dev, struct urb *urb)
{
	struct ax88179_int_data *event = NULL;
	int link = 0;

	if (urb->actual_length < 8)
		return;

	event = urb->transfer_buffer;
	link = event->link & AX_INT_PPLS_LINK;

	if (netif_carrier_ok(dev->net) != link) {
		if (link)
			usbnet_defer_kevent(dev, EVENT_LINK_RESET);
		else
			netif_carrier_off(dev->net);
		netdev_info(dev->net, "ax88179_178a - Link status is: %d\n",
			    link);
	}
}

static int ax88179_mdio_read(struct net_device *netdev, int phy_id, int loc)
{
	struct usbnet *dev = netdev_priv(netdev);
	u16 res = 0;

	ax88179_read_cmd(dev, AX_ACCESS_PHY, phy_id, (__u16)loc, 2, &res, 1);
	return res;
}

static void ax88179_mdio_write(struct net_device *netdev, int phy_id, int loc,
			       int val)
{
	struct usbnet *dev = netdev_priv(netdev);
	u16 res = (u16)val;

	ax88179_write_cmd(dev, AX_ACCESS_PHY, phy_id, (__u16)loc, 2, &res);
}

static int ax88179_suspend(struct usb_interface *intf,
			   pm_message_t message)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	u16 tmp16 = 0;
	u8 tmp8 = 0;

	usbnet_suspend(intf, message);

	/* Disable RX path */
	ax88179_read_cmd_nopm(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			      2, 2, &tmp16, 1);
	tmp16 &= ~AX_MEDIUM_RECEIVE_EN;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC,  AX_MEDIUM_STATUS_MODE,
			       2, 2, &tmp16);

	/* Force bz */
	ax88179_read_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			      2, 2, &tmp16, 1);
	tmp16 |= AX_PHYPWR_RSTCTL_BZ | AX_PHYPWR_RSTCTL_IPRL;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			       2, 2, &tmp16);

	/* change clock */
	tmp8 = 0;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8);

	/* Configure RX control register => stop operation */
	tmp16 = AX_RX_CTL_STOP;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &tmp16);

	return 0;
}

static void ax88179_EEE_setting(struct usbnet *dev)
{
	u16 tmp16;

	if (bEEE) {
		/* Enable EEE */
		tmp16 = 0x07;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MACR, 2, &tmp16);

		tmp16 = 0x3c;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MAADR, 2, &tmp16);

		tmp16 = 0x4007;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MACR, 2, &tmp16);

		tmp16 = 0x06;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MAADR, 2, &tmp16);
	} else {
		/* Disable EEE */
		tmp16 = 0x07;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MACR, 2, &tmp16);

		tmp16 = 0x3c;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MAADR, 2, &tmp16);

		tmp16 = 0x4007;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MACR, 2, &tmp16);

		tmp16 = 0x00;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  GMII_PHY_MAADR, 2, &tmp16);
	}
}

static void ax88179_Gether_setting(struct usbnet *dev)
{

	u16 tmp16;

	if (bGETH) {
		/* Enable Green Ethernet */
		tmp16 = 0x03;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  31, 2, &tmp16);

		tmp16 = 0x3247;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  25, 2, &tmp16);

		tmp16 = 0x05;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  31, 2, &tmp16);

		tmp16 = 0x0680;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  1, 2, &tmp16);

		tmp16 = 0;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  31, 2, &tmp16);
	} else {
		/* Disable Green Ethernet */
		tmp16 = 0x03;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  31, 2, &tmp16);

		tmp16 = 0x3246;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  25, 2, &tmp16);

		tmp16 = 0;
		ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				  31, 2, &tmp16);
	}
}

static int ax88179_resume(struct usb_interface *intf)
{
	struct usbnet *dev = usb_get_intfdata(intf);
	u16 tmp16 = 0;
	u8 tmp8 = 0;

	netif_carrier_off(dev->net);

	/* Power up ethernet PHY */
	tmp16 = 0;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			       2, 2, &tmp16);
	usleep_range(1000, 2000);
	tmp16 = AX_PHYPWR_RSTCTL_IPRL;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
			       2, 2, &tmp16);
	msleep(200);

	/* Ethernet PHY Auto Detach*/
	ax88179_AutoDetach(dev, 1);

	/* change clock */
	ax88179_read_cmd_nopm(dev, AX_ACCESS_MAC,  AX_CLK_SELECT,
			      1, 1, &tmp8, 0);
	tmp8 |= AX_CLK_SELECT_ACS | AX_CLK_SELECT_BCS;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8);
	msleep(100);

	/* Configure RX control register => start operation */
	tmp16 = AX_RX_CTL_DROPCRCERR | AX_RX_CTL_START | AX_RX_CTL_AP |
		 AX_RX_CTL_AMALL | AX_RX_CTL_AB;
	if (NET_IP_ALIGN == 0)
		tmp16 |= AX_RX_CTL_IPE;
	ax88179_write_cmd_nopm(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &tmp16);

	return usbnet_resume(intf);
}

static void
ax88179_get_wol(struct net_device *net, struct ethtool_wolinfo *wolinfo)
{
	struct usbnet *dev = netdev_priv(net);
	u8 *opt = NULL;

	opt = kmalloc(1, GFP_KERNEL);
	if (!opt)
		return;

	if (ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MODE,
			     1, 1, opt, 0) < 0) {
		wolinfo->supported = 0;
		wolinfo->wolopts = 0;
		kfree(opt);
		return;
	}

	wolinfo->supported = WAKE_PHY | WAKE_MAGIC;

	if (*opt & AX_MONITOR_MODE_RWLC)
		wolinfo->wolopts |= WAKE_PHY;
	if (*opt & AX_MONITOR_MODE_RWMP)
		wolinfo->wolopts |= WAKE_MAGIC;

	kfree(opt);
}

static int
ax88179_set_wol(struct net_device *net, struct ethtool_wolinfo *wolinfo)
{
	struct usbnet *dev = netdev_priv(net);
	u8 *opt = NULL;

	opt = kmalloc(1, GFP_KERNEL);
	if (!opt)
		return -ENOMEM;

	*opt = 0;

	if (wolinfo->wolopts & WAKE_PHY)
		*opt |= AX_MONITOR_MODE_RWLC;
	else
		*opt &= ~AX_MONITOR_MODE_RWLC;

	if (wolinfo->wolopts & WAKE_MAGIC)
		*opt |= AX_MONITOR_MODE_RWMP;
	else
		*opt &= ~AX_MONITOR_MODE_RWMP;

	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, opt);

	kfree(opt);

	return 0;
}

static int ax88179_get_eeprom_len(struct net_device *net)
{
	return AX_EEPROM_LEN;
}

static int ax88179_get_eeprom(struct net_device *net,
			      struct ethtool_eeprom *eeprom, u8 *data)
{
	struct usbnet *dev = netdev_priv(net);
	u16 *eeprom_buff = NULL;
	int first_word = 0, last_word = 0;
	int i = 0;

	if (eeprom->len == 0)
		return -EINVAL;

	eeprom->magic = AX88179_EEPROM_MAGIC;

	first_word = eeprom->offset >> 1;
	last_word = (eeprom->offset + eeprom->len - 1) >> 1;
	eeprom_buff = kmalloc(sizeof(u16) * (last_word - first_word + 1),
			      GFP_KERNEL);
	if (!eeprom_buff)
		return -ENOMEM;

	/* ax88179/178A returns 2 bytes from eeprom on read */
	for (i = first_word; i <= last_word; i++) {
		if (ax88179_read_cmd(dev, AX_ACCESS_EEPROM, i, 1, 2,
				     &eeprom_buff[i - first_word], 0) < 0) {
			kfree(eeprom_buff);
			return -EIO;
		}
	}

	memcpy(data, (u8 *)eeprom_buff + (eeprom->offset & 1), eeprom->len);
	kfree(eeprom_buff);
	return 0;
}

static void ax88179_get_drvinfo(struct net_device *net,
				struct ethtool_drvinfo *info)
{
	/* Inherit standard device info */
	usbnet_get_drvinfo(net, info);
	info->eedump_len = 0x3e;
}

static int ax88179_get_settings(struct net_device *net, struct ethtool_cmd *cmd)
{
	struct usbnet *dev = netdev_priv(net);
	return mii_ethtool_gset(&dev->mii, cmd);
}

static int ax88179_set_settings(struct net_device *net, struct ethtool_cmd *cmd)
{
	struct usbnet *dev = netdev_priv(net);
	return mii_ethtool_sset(&dev->mii, cmd);
}

static int ax88179_ioctl(struct net_device *net, struct ifreq *rq, int cmd)
{
	struct usbnet *dev = netdev_priv(net);

	return  generic_mii_ioctl(&dev->mii, if_mii(rq), cmd, NULL);
}

static struct ethtool_ops ax88179_ethtool_ops = {
	.get_drvinfo		= ax88179_get_drvinfo,
	.get_link		= ethtool_op_get_link,
	.get_msglevel		= usbnet_get_msglevel,
	.set_msglevel		= usbnet_set_msglevel,
	.get_wol		= ax88179_get_wol,
	.set_wol		= ax88179_set_wol,
	.get_eeprom_len		= ax88179_get_eeprom_len,
	.get_eeprom		= ax88179_get_eeprom,
	.get_settings		= ax88179_get_settings,
	.set_settings		= ax88179_set_settings,
};

static void ax88179_set_multicast(struct net_device *net)
{
	struct usbnet *dev = netdev_priv(net);
	struct ax88179_data *data = (struct ax88179_data *)&dev->data;
	u8 *m_filter = ((u8 *)dev->data) + 12;
	int mc_count = 0;

	mc_count = netdev_mc_count(net);

	data->rxctl = (AX_RX_CTL_START | AX_RX_CTL_AB);
	if (NET_IP_ALIGN == 0)
		data->rxctl |= AX_RX_CTL_IPE;

	if (net->flags & IFF_PROMISC) {
		data->rxctl |= AX_RX_CTL_PRO;
	} else if (net->flags & IFF_ALLMULTI ||
			mc_count > AX_MAX_MCAST) {
		data->rxctl |= AX_RX_CTL_AMALL;
	} else if (mc_count == 0) {
		/* just broadcast and directed */
	} else {
		/* We use the 20 byte dev->data
		 * for our 8 byte filter buffer
		 * to avoid allocating memory that
		 * is tricky to free later
		 */
		u32 crc_bits = 0;

		struct netdev_hw_addr *ha = NULL;

		memset(m_filter, 0, AX_MCAST_FILTER_SIZE);
		netdev_for_each_mc_addr(ha, net) {
			crc_bits = ether_crc(ETH_ALEN, ha->addr) >> 26;
			*(m_filter + (crc_bits >> 3)) |=
				1 << (crc_bits & 7);
		}
		ax88179_write_cmd_async(dev, AX_ACCESS_MAC,
					AX_MULTI_FILTER_ARRY,
					AX_MCAST_FILTER_SIZE,
					AX_MCAST_FILTER_SIZE, m_filter);

		data->rxctl |= AX_RX_CTL_AM;
	}

	ax88179_write_cmd_async(dev, AX_ACCESS_MAC, AX_RX_CTL,
				2, 2, &data->rxctl);
}

static int
ax88179_set_features(struct net_device *net, netdev_features_t features)
{
	u8 tmp = 0;
	struct usbnet *dev = netdev_priv(net);
	netdev_features_t changed = net->features ^ features;

	if (changed & NETIF_F_IP_CSUM) {
		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_TXCOE_CTL,
				 1, 1, &tmp, 0);
		tmp ^= AX_TXCOE_TCP | AX_TXCOE_UDP;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_TXCOE_CTL, 1, 1, &tmp);
	}

	if (changed & NETIF_F_IPV6_CSUM) {
		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_TXCOE_CTL,
				 1, 1, &tmp, 0);
		tmp ^= AX_TXCOE_TCPV6 | AX_TXCOE_UDPV6;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_TXCOE_CTL, 1, 1, &tmp);
	}

	if (changed & NETIF_F_RXCSUM) {
		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_RXCOE_CTL,
				 1, 1, &tmp, 0);
		tmp ^= AX_RXCOE_IP | AX_RXCOE_TCP | AX_RXCOE_UDP |
		       AX_RXCOE_TCPV6 | AX_RXCOE_UDPV6;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RXCOE_CTL, 1, 1, &tmp);
	}

	return 0;
}

static int ax88179_change_mtu(struct net_device *net, int new_mtu)
{
	struct usbnet *dev = netdev_priv(net);
	u16 tmp16 = 0;

	if (new_mtu <= 0 || new_mtu > 4088)
		return -EINVAL;

	net->mtu = new_mtu;
	dev->hard_mtu = net->mtu + net->hard_header_len;

	if (net->mtu > 1500) {
		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
				 2, 2, &tmp16, 1);
		tmp16 |= AX_MEDIUM_JUMBO_EN;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
				  2, 2, &tmp16);
	} else {
		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
				 2, 2, &tmp16, 1);
		tmp16 &= ~AX_MEDIUM_JUMBO_EN;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
				  2, 2, &tmp16);
	}

	return 0;
}

static int ax88179_set_mac_addr(struct net_device *net, void *p)
{
	struct usbnet *dev = netdev_priv(net);
	struct sockaddr *addr = p;

	if (netif_running(net))
		return -EBUSY;
	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	memcpy(net->dev_addr, addr->sa_data, ETH_ALEN);

	/* Set the MAC address */
	return ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
				 ETH_ALEN, net->dev_addr);

}

static const struct net_device_ops ax88179_netdev_ops = {
	.ndo_open		= usbnet_open,
	.ndo_stop		= usbnet_stop,
	.ndo_start_xmit		= usbnet_start_xmit,
	.ndo_tx_timeout		= usbnet_tx_timeout,
	.ndo_change_mtu		= ax88179_change_mtu,
	.ndo_do_ioctl		= ax88179_ioctl,
	.ndo_set_mac_address	= ax88179_set_mac_addr,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_rx_mode	= ax88179_set_multicast,
	.ndo_set_features	= ax88179_set_features,
};

static int ax88179_check_eeprom(struct usbnet *dev)
{
	u8 i = 0;
	u8 buf[2] = {0};
	u8 eeprom[20] = {0};
	u16 csum = 0, delay = HZ / 10;
	unsigned long jtimeout = 0;

	/* Read EEPROM content */
	for (i = 0 ; i < 6; i++) {
		buf[0] = i;
		if (ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_SROM_ADDR,
				      1, 1, buf) < 0)
			return -EINVAL;

		buf[0] = EEP_RD;
		if (ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_SROM_CMD,
				      1, 1, buf) < 0)
			return -EINVAL;

		jtimeout = jiffies + delay;
		do {
			ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_SROM_CMD,
					 1, 1, buf, 0);

			if (time_after(jiffies, jtimeout))
				return -EINVAL;
		} while (buf[0] & EEP_BUSY);

		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_SROM_DATA_LOW,
				 2, 2, &eeprom[i * 2], 0);

		if ((i == 0) && (eeprom[0] == 0xFF))
			return -EINVAL;
	}

	csum = eeprom[6] + eeprom[7] + eeprom[8] + eeprom[9];
	csum = (csum >> 8) + (csum & 0xff);

	if ((csum + eeprom[10]) == 0xff)
		return AX_EEP_EFUSE_CORRECT;
	else
		return -EINVAL;
}

static int ax88179_check_efuse(struct usbnet *dev, void *ledmode)
{
	u8	i = 0;
	u8	efuse[64] = {0x00};
	u16	csum = 0;

	if (ax88179_read_cmd(dev, AX_ACCESS_EFUSE, 0, 64, 64, efuse, 0) < 0)
		return -EINVAL;

	if (efuse[0] == 0xFF)
		return -EINVAL;

	for (i = 0; i < 64; i++)
		csum = csum + efuse[i];

	while (csum > 255)
		csum = (csum & 0x00FF) + ((csum >> 8) & 0x00FF);

	if (csum == 0xFF) {
		memcpy((u8 *)ledmode, &efuse[51], 2);
		return AX_EEP_EFUSE_CORRECT;
	} else {
		return -EINVAL;
	}
}

static int ax88179_convert_old_led(struct usbnet *dev, u8 efuse, void *ledvalue)
{
	u8 ledmode = 0;
	u16 tmp = 0;
	u16 led = 0;

	/* loaded the old eFuse LED Mode */
	if (efuse) {
		if (ax88179_read_cmd(dev, AX_ACCESS_EFUSE, 0x18,
				     1, 2, &tmp, 1) < 0)
			return -EINVAL;
		ledmode = (u8)(tmp & 0xFF);
	} else { /* loaded the old EEprom LED Mode */
		if (ax88179_read_cmd(dev, AX_ACCESS_EEPROM, 0x3C,
				     1, 2, &tmp, 1) < 0)
			return -EINVAL;
		ledmode = (u8)(tmp >> 8);
	}
	netdev_dbg(dev->net, "Old LED Mode = %02X\n", ledmode);
	switch (ledmode) {
	case 0xFF:
		led = LED0_ACTIVE | LED1_LINK_10 | LED1_LINK_100 |
		      LED1_LINK_1000 | LED2_ACTIVE | LED2_LINK_10 |
		      LED2_LINK_100 | LED2_LINK_1000 | LED_VALID;
		break;
	case 0xFE:
		led = LED0_ACTIVE | LED1_LINK_1000 | LED2_LINK_100 | LED_VALID;
		break;
	case 0xFD:
		led = LED0_ACTIVE | LED1_LINK_1000 | LED2_LINK_100 |
		      LED2_LINK_10 | LED_VALID;
		break;
	case 0xFC:
		led = LED0_ACTIVE | LED1_ACTIVE | LED1_LINK_1000 | LED2_ACTIVE |
		      LED2_LINK_100 | LED2_LINK_10 | LED_VALID;
		break;
	default:
		led = LED0_ACTIVE | LED1_LINK_10 | LED1_LINK_100 |
		      LED1_LINK_1000 | LED2_ACTIVE | LED2_LINK_10 |
		      LED2_LINK_100 | LED2_LINK_1000 | LED_VALID;
		break;
	}

	memcpy((u8 *)ledvalue, &led, 2);

	return 0;
}

static int ax88179_led_setting(struct usbnet *dev)
{
	u8 ledfd = 0, value = 0;
	u16 tmp = 0, ledact = 0, ledlink = 0, ledvalue = 0, delay = HZ / 10;
	unsigned long jtimeout = 0;

	/* Check AX88179 version. UA1 or UA2 */
	ax88179_read_cmd(dev, AX_ACCESS_MAC, GENERAL_STATUS, 1, 1, &value, 0);

	/* UA1 */
	if (!(value & AX_SECLD)) {
		value = AX_GPIO_CTRL_GPIO3EN | AX_GPIO_CTRL_GPIO2EN |
			AX_GPIO_CTRL_GPIO1EN;
		if (ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_GPIO_CTRL,
				      1, 1, &value) < 0)
			return -EINVAL;
	}

	/* check EEprom */
	if (ax88179_check_eeprom(dev) == AX_EEP_EFUSE_CORRECT) {
		value = 0x42;
		if (ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_SROM_ADDR,
				      1, 1, &value) < 0)
			return -EINVAL;

		value = EEP_RD;
		if (ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_SROM_CMD,
				      1, 1, &value) < 0)
			return -EINVAL;

		jtimeout = jiffies + delay;
		do {
			ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_SROM_CMD,
					 1, 1, &value, 0);

			ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_SROM_CMD,
					 1, 1, &value, 0);

			if (time_after(jiffies, jtimeout))
				return -EINVAL;
		} while (value & EEP_BUSY);

		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_SROM_DATA_HIGH,
				 1, 1, &value, 0);
		ledvalue = (value << 8);
		ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_SROM_DATA_LOW,
				 1, 1, &value, 0);
		ledvalue |= value;

		/* load internal ROM for defaule setting */
		if ((ledvalue == 0xFFFF) || ((ledvalue & LED_VALID) == 0))
			ax88179_convert_old_led(dev, 0, &ledvalue);

	} else if (ax88179_check_efuse(dev, &ledvalue) ==
				       AX_EEP_EFUSE_CORRECT) { /* check efuse */
		if ((ledvalue == 0xFFFF) || ((ledvalue & LED_VALID) == 0))
			ax88179_convert_old_led(dev, 0, &ledvalue);
	} else {
		ax88179_convert_old_led(dev, 0, &ledvalue);
	}

	tmp = GMII_PHY_PAGE_SELECT_EXT;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &tmp);

	tmp = 0x2c;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHYPAGE, 2, &tmp);

	ax88179_read_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			 GMII_LED_ACTIVE, 2, &ledact, 1);

	ax88179_read_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			 GMII_LED_LINK, 2, &ledlink, 1);

	ledact &= GMII_LED_ACTIVE_MASK;
	ledlink &= GMII_LED_LINK_MASK;

	if (ledvalue & LED0_ACTIVE)
		ledact |= GMII_LED0_ACTIVE;
	if (ledvalue & LED1_ACTIVE)
		ledact |= GMII_LED1_ACTIVE;
	if (ledvalue & LED2_ACTIVE)
		ledact |= GMII_LED2_ACTIVE;

	if (ledvalue & LED0_LINK_10)
		ledlink |= GMII_LED0_LINK_10;
	if (ledvalue & LED1_LINK_10)
		ledlink |= GMII_LED1_LINK_10;
	if (ledvalue & LED2_LINK_10)
		ledlink |= GMII_LED2_LINK_10;

	if (ledvalue & LED0_LINK_100)
		ledlink |= GMII_LED0_LINK_100;
	if (ledvalue & LED1_LINK_100)
		ledlink |= GMII_LED1_LINK_100;
	if (ledvalue & LED2_LINK_100)
		ledlink |= GMII_LED2_LINK_100;

	if (ledvalue & LED0_LINK_1000)
		ledlink |= GMII_LED0_LINK_1000;
	if (ledvalue & LED1_LINK_1000)
		ledlink |= GMII_LED1_LINK_1000;
	if (ledvalue & LED2_LINK_1000)
		ledlink |= GMII_LED2_LINK_1000;

	tmp = ledact;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_LED_ACTIVE, 2, &tmp);

	tmp = ledlink;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_LED_LINK, 2, &tmp);

	tmp = GMII_PHY_PAGE_SELECT_PAGE0;
	ax88179_write_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
			  GMII_PHY_PAGE_SELECT, 2, &tmp);

	/* LED full duplex setting */
	ledfd = 0;
	if (ledvalue & LED0_FD)
		ledfd |= 0x01;
	else if ((ledvalue & LED0_USB3_MASK) == 0)
		ledfd |= 0x02;

	if (ledvalue & LED1_FD)
		ledfd |= 0x04;
	else if ((ledvalue & LED1_USB3_MASK) == 0)
		ledfd |= 0x08;

	if (ledvalue & LED2_FD) /* LED2_FD */
		ledfd |= 0x10;
	else if ((ledvalue & LED2_USB3_MASK) == 0) /* LED2_USB3 */
		ledfd |= 0x20;

	ax88179_write_cmd(dev, AX_ACCESS_MAC, 0x73, 1, 1, &ledfd);

	return 0;
}

static int ax88179_AutoDetach(struct usbnet *dev, int in_pm)
{
	u16 tmp16 = 0;
	u8 tmp8 = 0;
	int (*fnr)(struct usbnet *, u8, u16, u16, u16, void *, int);
	int (*fnw)(struct usbnet *, u8, u16, u16, u16, void *);

	if (!in_pm) {
		fnr = ax88179_read_cmd;
		fnw = ax88179_write_cmd;
	} else {
		fnr = ax88179_read_cmd_nopm;
		fnw = ax88179_write_cmd_nopm;
	}

	if (fnr(dev, AX_ACCESS_EEPROM, 0x43, 1, 2, &tmp16, 1) < 0)
		return 0;

	if ((tmp16 == 0xFFFF) || (!(tmp16 & 0x0100)))
		return 0;

	/* Enable Auto Detach bit */
	tmp8 = 0;
	fnr(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8, 0);
	tmp8 |= AX_CLK_SELECT_ULR;
	fnw(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, &tmp8);

	fnr(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &tmp16, 1);
	tmp16 |= AX_PHYPWR_RSTCTL_AUTODETACH;
	fnw(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, &tmp16);

	return 0;
}

static int access_eeprom_mac(struct usbnet *dev, u8 *buf, u8 offset, int wflag)
{
	int ret = 0, i;
	u16 *tmp = (u16 *)buf;

	for (i = 0; i < (ETH_ALEN >> 1); i++) {
		if (wflag) {
			u16 wd = cpu_to_le16(*(tmp + i));

			ret = ax88179_write_cmd(dev, AX_ACCESS_EEPROM,
						offset + i, 1, 2, &wd);
			if (ret < 0)
				break;

			mdelay(15);
		} else {
			ret = ax88179_read_cmd(dev, AX_ACCESS_EEPROM,
					       offset + i, 1, 2, tmp + i, 0);
			if (ret < 0)
				break;
		}
	}

	if (!wflag) {
		if (ret < 0) {
			netdev_dbg(dev->net, "Failed to read MAC address from EEPROM: %d\n"
					, ret);
			return ret;
		}
		memcpy(dev->net->dev_addr, buf, ETH_ALEN);
	} else {
		/* reload eeprom data */
		ret = ax88179_write_cmd(dev, AX_RELOAD_EEPROM_EFUSE,
					0, 0, 0, 0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int ax88179_check_ether_addr(struct usbnet *dev)
{
	unsigned char *tmp = (unsigned char *)dev->net->dev_addr;
	u8 default_mac[6] = {0, 0x0e, 0xc6, 0x81, 0x79, 0x01};
	u8 default_mac_178a[6] = {0, 0x0e, 0xc6, 0x81, 0x78, 0x01};

	if (((*((u8 *)tmp) == 0) && (*((u8 *)tmp + 1) == 0) &&
	     (*((u8 *)tmp + 2) == 0)) ||
		!is_valid_ether_addr((u8 *)tmp) ||
		!memcmp(dev->net->dev_addr, default_mac, ETH_ALEN) ||
		!memcmp(dev->net->dev_addr, default_mac_178a, ETH_ALEN)) {
		int i;

		pr_err("Found invalid EEPROM MAC address value ");

		for (i = 0; i < ETH_ALEN; i++) {
			pr_info("%02X", *((u8 *)tmp + i));
			if (i != 5)
				printk("-");
		}
		printk("\n");
		eth_hw_addr_random(dev->net);

		*tmp = 0;
		*(tmp + 1) = 0x0E;
		*(tmp + 2) = 0xC6;
		*(tmp + 3) = 0x8E;

		return -EADDRNOTAVAIL;
	}
	return 0;
}

static int ax88179_get_mac(struct usbnet *dev, u8 *buf)
{
	int ret, i;

	ret = access_eeprom_mac(dev, buf, 0x0, 0);
	if (ret < 0)
		goto out;

	if (ax88179_check_ether_addr(dev)) {
		ret = access_eeprom_mac(dev, dev->net->dev_addr, 0x0, 1);
		if (ret < 0) {
			netdev_err(dev->net, "Failed to write MAC to EEPROM: %d"
					, ret);
			goto out;
		}

		msleep(5);

		ret = ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_NODE_ID,
				       ETH_ALEN, ETH_ALEN, buf, 0);
		if (ret < 0) {
			netdev_err(dev->net, "Failed to read MAC address: %d"
					, ret);
			goto out;
		}

		for (i = 0; i < ETH_ALEN; i++)
			if (*(dev->net->dev_addr + i) != *((u8 *)buf + i)) {
				netdev_warn(dev->net, "Found invalid EEPROM part or non-EEPROM");
				break;
			}
	}

	memcpy(dev->net->perm_addr, dev->net->dev_addr, ETH_ALEN);

	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
			  ETH_ALEN, dev->net->dev_addr);

	if (ret < 0) {
		netdev_err(dev->net, "Failed to write MAC address: %d", ret);
		goto out;
	}

	return 0;
out:
	return ret;
}

static int ax88179_bind(struct usbnet *dev, struct usb_interface *intf)
{
	void *buf = NULL;
	u16 *tmp16 = NULL;
	u8 *tmp = NULL;
	int ret;

	struct ax88179_data *ax179_data = (struct ax88179_data *)dev->data;

	usbnet_get_endpoints(dev, intf);

	if (msg_enable != 0)
		dev->msg_enable = msg_enable;

	buf = kmalloc(6, GFP_KERNEL);
	if (!buf) {
		netdev_err(dev->net, "Cannot allocate memory for buffer");
		return -ENOMEM;
	}
	tmp16 = (u16 *)buf;
	tmp = (u8 *)buf;

	memset(ax179_data, 0, sizeof(*ax179_data));

	/* Power up ethernet PHY */
	*tmp16 = 0;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, tmp16);
	*tmp16 = AX_PHYPWR_RSTCTL_IPRL;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, tmp16);
	msleep(200);

	*tmp = AX_CLK_SELECT_ACS | AX_CLK_SELECT_BCS;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, tmp);
	msleep(100);

	/* Get the MAC address */
	memset(buf, 0, ETH_ALEN);
	ret = ax88179_get_mac(dev, buf);
	if (ret)
		goto out;

		netdev_dbg(dev->net, "MAC [%02x-%02x-%02x-%02x-%02x-%02x]\n",
			   dev->net->dev_addr[0], dev->net->dev_addr[1],
			   dev->net->dev_addr[2], dev->net->dev_addr[3],
			   dev->net->dev_addr[4], dev->net->dev_addr[5]);

	/* RX bulk configuration, default for USB3.0 to Giga*/
	memcpy(tmp, &AX88179_BULKIN_SIZE[0], 5);
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_BULKIN_QCTRL, 5, 5, tmp);

	dev->rx_urb_size = 1024 * 20;

	tmp[0] = 0x34;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_LOW, 1, 1, tmp);

	tmp[0] = 0x52;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_HIGH,
			  1, 1, tmp);

	/* Disable auto-power-OFF GigaPHY after ethx down*/
	ax88179_write_cmd(dev, 0x91, 0, 0, 0, NULL);

	dev->net->netdev_ops = &ax88179_netdev_ops;

	dev->net->ethtool_ops = &ax88179_ethtool_ops;
	dev->net->needed_headroom = 8;

	/* Initialize MII structure */
	dev->mii.dev = dev->net;
	dev->mii.mdio_read = ax88179_mdio_read;
	dev->mii.mdio_write = ax88179_mdio_write;
	dev->mii.phy_id_mask = 0xff;
	dev->mii.reg_num_mask = 0xff;
	dev->mii.phy_id = 0x03;
	dev->mii.supports_gmii = 1;

	dev->net->features |= NETIF_F_IP_CSUM;
	dev->net->features |= NETIF_F_IPV6_CSUM;
	if (usb_device_no_sg_constraint(dev->udev))
		dev->can_dma_sg = 1;
	dev->net->features |= NETIF_F_SG | NETIF_F_TSO;

	dev->net->hw_features |= NETIF_F_IP_CSUM;
	dev->net->hw_features |= NETIF_F_IPV6_CSUM;
	dev->net->hw_features |= NETIF_F_SG | NETIF_F_TSO;

	/* Enable checksum offload */
	*tmp = AX_RXCOE_IP | AX_RXCOE_TCP | AX_RXCOE_UDP |
	       AX_RXCOE_TCPV6 | AX_RXCOE_UDPV6;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RXCOE_CTL, 1, 1, tmp);

	*tmp = AX_TXCOE_IP | AX_TXCOE_TCP | AX_TXCOE_UDP |
	       AX_TXCOE_TCPV6 | AX_TXCOE_UDPV6;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_TXCOE_CTL, 1, 1, tmp);

	ax179_data->checksum |= AX_RX_CHECKSUM | AX_TX_CHECKSUM;

	/* Configure RX control register => start operation */
	*tmp16 = AX_RX_CTL_DROPCRCERR | AX_RX_CTL_START | AX_RX_CTL_AP |
		 AX_RX_CTL_AMALL | AX_RX_CTL_AB;
	if (NET_IP_ALIGN == 0)
		*tmp16 |= AX_RX_CTL_IPE;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, tmp16);

	*tmp = AX_MONITOR_MODE_PMETYPE | AX_MONITOR_MODE_PMEPOL |
						AX_MONITOR_MODE_RWMP;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, tmp);

	ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, tmp, 0);
	netdev_dbg(dev->net, "Monitor mode = 0x%02x\n", *tmp);
	/* Configure default medium type => giga */
	*tmp16 = AX_MEDIUM_RECEIVE_EN	 | AX_MEDIUM_TXFLOW_CTRLEN |
		 AX_MEDIUM_RXFLOW_CTRLEN | AX_MEDIUM_FULL_DUPLEX   |
		 AX_MEDIUM_GIGAMODE;

	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			  2, 2, tmp16);

	ax88179_led_setting(dev);

	ax88179_EEE_setting(dev);

	ax88179_Gether_setting(dev);

	/* Restart autoneg */
	mii_nway_restart(&dev->mii);

	netif_carrier_off(dev->net);

	kfree(buf);
	printk(version);
		netdev_info(dev->net, "mtu %d\n", dev->net->mtu);
	return 0;

out:
	kfree(buf);
	return ret;

}

static void ax88179_unbind(struct usbnet *dev, struct usb_interface *intf)
{
	u16 tmp16 = 0;
	u8 tmp8 = 0;
	struct ax88179_data *ax179_data = (struct ax88179_data *)dev->data;

	if (ax179_data) {
		/* Configure RX control register => stop operation */
		tmp16 = AX_RX_CTL_STOP;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &tmp16);

		tmp8 = 0x0;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_CLK_SELECT,
				  1, 1, &tmp8);

		/* Power down ethernet PHY */
		tmp16 = AX_PHYPWR_RSTCTL_BZ | AX_PHYPWR_RSTCTL_IPRL;
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL,
				  2, 2, &tmp16);
		msleep(200);
	}
}

static void ax88179_rx_checksum(struct sk_buff *skb, u32 *pkt_hdr)
{
	skb->ip_summed = CHECKSUM_NONE;

	/* checksum error bit is set */
	if ((*pkt_hdr & AX_RXHDR_L3CSUM_ERR) ||
	    (*pkt_hdr & AX_RXHDR_L4CSUM_ERR))
		return;

	/* It must be a TCP or UDP packet with a valid checksum */
	if (((*pkt_hdr & AX_RXHDR_L4_TYPE_MASK) == AX_RXHDR_L4_TYPE_TCP) ||
	    ((*pkt_hdr & AX_RXHDR_L4_TYPE_MASK) == AX_RXHDR_L4_TYPE_UDP))
		skb->ip_summed = CHECKSUM_UNNECESSARY;
}

static int ax88179_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	struct sk_buff *ax_skb = NULL;
	int pkt_cnt = 0;
	u32 rx_hdr = 0;
	u16 hdr_off = 0;
	u32 *pkt_hdr = NULL;

	skb_trim(skb, skb->len - 4);
	memcpy(&rx_hdr, skb_tail_pointer(skb), sizeof(rx_hdr));
	le32_to_cpus(&rx_hdr);

	pkt_cnt = (u16)rx_hdr;
	hdr_off = (u16)(rx_hdr >> 16);
	pkt_hdr = (u32 *)(skb->data + hdr_off);

	/* Ignore abnormal USB transfer data */
	if (pkt_cnt > 100)
		return 0;

	while (pkt_cnt--) {
		u16 pkt_len;

		le32_to_cpus(pkt_hdr);
		pkt_len = (*pkt_hdr >> 16) & 0x1fff;

		/* Check CRC or runt packet */
		if ((*pkt_hdr & AX_RXHDR_CRC_ERR) ||
		    (*pkt_hdr & AX_RXHDR_DROP_ERR)) {
			skb_pull(skb, (pkt_len + 7) & 0xFFF8);
			pkt_hdr++;
			continue;
		}

		if (pkt_cnt == 0) {
			skb->len = pkt_len;

			/* Skip IP alignment pseudo header */
			if (NET_IP_ALIGN == 0)
				skb_pull(skb, 2);

			skb_set_tail_pointer(skb, skb->len);
			skb->truesize = skb->len + sizeof(struct sk_buff);
			ax88179_rx_checksum(skb, pkt_hdr);

			return 1;
		}

		ax_skb = skb_clone(skb, GFP_ATOMIC);

		if (ax_skb) {
			ax_skb->len = pkt_len;

			/* Skip IP alignment pseudo header */
			if (NET_IP_ALIGN == 0)
				skb_pull(ax_skb, 2);

			skb_set_tail_pointer(ax_skb, ax_skb->len);

			ax_skb->truesize = ax_skb->len + sizeof(struct sk_buff);
			ax88179_rx_checksum(ax_skb, pkt_hdr);
			usbnet_skb_return(dev, ax_skb);
		} else {
			return 0;
		}

		skb_pull(skb, (pkt_len + 7) & 0xFFF8);
		pkt_hdr++;
	}
	return 1;
}

static struct sk_buff *
ax88179_tx_fixup(struct usbnet *dev, struct sk_buff *skb, gfp_t flags)
{
	u32 tx_hdr1 = 0, tx_hdr2 = 0;
	int frame_size = dev->maxpacket;
	int mss = skb_shinfo(skb)->gso_size;
	int headroom = 0;
	int tailroom = 0;

	tx_hdr1 = skb->len;
	tx_hdr2 = mss;
	if (((skb->len + 8) % frame_size) == 0)
		tx_hdr2 |= 0x80008000;	/* Enable padding */

	if (!dev->can_dma_sg && (dev->net->features & NETIF_F_SG) &&
	    skb_linearize(skb))
		return NULL;

	headroom = skb_headroom(skb);
	tailroom = skb_tailroom(skb);

	if ((headroom + tailroom) >= 8) {
		if (headroom < 8) {
			skb->data = memmove(skb->head + 8, skb->data, skb->len);
			skb_set_tail_pointer(skb, skb->len);
		}
	} else {
		struct sk_buff *skb2 = NULL;

		skb2 = skb_copy_expand(skb, 8, 0, flags);
		dev_kfree_skb_any(skb);
		skb = skb2;
		if (!skb)
			return NULL;
	}

	skb_push(skb, 4);
	cpu_to_le32s(&tx_hdr2);
	skb_copy_to_linear_data(skb, &tx_hdr2, 4);

	skb_push(skb, 4);
	cpu_to_le32s(&tx_hdr1);
	skb_copy_to_linear_data(skb, &tx_hdr1, 4);

	return skb;
}

static int ax88179_link_reset(struct usbnet *dev)
{
	struct ax88179_data *data = (struct ax88179_data *)&dev->data;
	u8 tmp[5] = {0}, link_sts = 0;
	u16 mode = 0, tmp16 = 0, delay = 10 * HZ;
	u32 tmp32;
	unsigned long jtimeout = 0;

	mode = AX_MEDIUM_TXFLOW_CTRLEN | AX_MEDIUM_RXFLOW_CTRLEN;

	ax88179_read_cmd(dev, AX_ACCESS_MAC, PHYSICAL_LINK_STATUS,
			 1, 1, &link_sts, 0);

	jtimeout = jiffies + delay;
	while (time_before(jiffies, jtimeout)) {

		ax88179_read_cmd(dev, AX_ACCESS_PHY, AX88179_PHY_ID,
				 GMII_PHY_PHYSR, 2, &tmp16, 1);

		if (tmp16 & GMII_PHY_PHYSR_LINK)
			break;
	}

	if (!(tmp16 & GMII_PHY_PHYSR_LINK))
		return 0;
	else if (GMII_PHY_PHYSR_GIGA == (tmp16 & GMII_PHY_PHYSR_SMASK)) {
		mode |= AX_MEDIUM_GIGAMODE | AX_MEDIUM_EN_125MHZ;
		if (dev->net->mtu > 1500)
			mode |= AX_MEDIUM_JUMBO_EN;

		if (link_sts & AX_USB_SS)
			memcpy(tmp, &AX88179_BULKIN_SIZE[0], 5);
		else if (link_sts & AX_USB_HS)
			memcpy(tmp, &AX88179_BULKIN_SIZE[1], 5);
		else
			memcpy(tmp, &AX88179_BULKIN_SIZE[3], 5);
	} else if (GMII_PHY_PHYSR_100 == (tmp16 & GMII_PHY_PHYSR_SMASK)) {
		mode |= AX_MEDIUM_PS;	/* Bit 9 : PS */
		if (link_sts & (AX_USB_SS | AX_USB_HS))
			memcpy(tmp, &AX88179_BULKIN_SIZE[2], 5);
		else
			memcpy(tmp, &AX88179_BULKIN_SIZE[3], 5);
	} else
		memcpy(tmp, &AX88179_BULKIN_SIZE[3], 5);

	if (bsize != -1) {
		if (bsize > 24) {
			bsize = 24;
		} else if (bsize == 0) {
			tmp[1] = 0;
			tmp[2] = 0;
		}

		tmp[3] = (u8)bsize;
	}

	if (ifg != -1) {
		if (ifg > 255)
			ifg = 255;
		tmp[4] = (u8)ifg;
	}

	/* RX bulk configuration */
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_BULKIN_QCTRL, 5, 5, tmp);

	if (tmp16 & GMII_PHY_PHYSR_FULL)
		mode |= AX_MEDIUM_FULL_DUPLEX;	/* Bit 1 : FD */

	dev->rx_urb_size = (1024 * (tmp[3] + 2));

	netdev_info(dev->net, "Write medium type: 0x%04x\n", mode);

	ax88179_read_cmd(dev, 0x81, 0x8c, 0, 4, &tmp32, 1);
	delay = HZ / 2;
	if (tmp32 & 0x40000000) {
		u16 tmp = 0;

		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, &tmp);

		/* Configure default medium type => giga */
		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
				  2, 2, &mode);

		jtimeout = jiffies + delay;

		while (time_before(jiffies, jtimeout)) {
			ax88179_read_cmd(dev, 0x81, 0x8c, 0, 4, &tmp32, 1);

			if (!(tmp32 & 0x40000000))
				break;

			tmp32 = 0x80000000;
			ax88179_write_cmd(dev, 0x81, 0x8c, 0, 4, &tmp32);
		}

		ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_CTL,
				  2, 2, &data->rxctl);
	}

	mode |= AX_MEDIUM_RECEIVE_EN;

	/* Configure default medium type => giga */
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			  2, 2, &mode);
	mii_check_media(&dev->mii, 1, 1);

	return 0;
}

static int ax88179_reset(struct usbnet *dev)
{
	void *buf = NULL;
	u16 *tmp16 = NULL;
	u8 *tmp = NULL;
	struct ax88179_data *ax179_data = (struct ax88179_data *)dev->data;
	buf = kmalloc(6, GFP_KERNEL);

	if (!buf) {
		netdev_err(dev->net, "Cannot allocate memory for buffer");
		return -ENOMEM;
	}

	tmp16 = (u16 *)buf;
	tmp = (u8 *)buf;

	/* Power up ethernet PHY */
	*tmp16 = 0;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, tmp16);
	*tmp16 = AX_PHYPWR_RSTCTL_IPRL;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PHYPWR_RSTCTL, 2, 2, tmp16);
	msleep(200);

	*tmp = AX_CLK_SELECT_ACS | AX_CLK_SELECT_BCS;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_CLK_SELECT, 1, 1, tmp);
	msleep(100);

	/* Ethernet PHY Auto Detach*/
	ax88179_AutoDetach(dev, 0);

	/* Set the MAC address */
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_NODE_ID, ETH_ALEN,
			  ETH_ALEN, dev->net->dev_addr);

	netdev_dbg(dev->net, "MAC [%02x-%02x-%02x-%02x-%02x-%02x]\n",
		   dev->net->dev_addr[0], dev->net->dev_addr[1],
		   dev->net->dev_addr[2], dev->net->dev_addr[3],
		   dev->net->dev_addr[4], dev->net->dev_addr[5]);

	/* RX bulk configuration */
	memcpy(tmp, &AX88179_BULKIN_SIZE[0], 5);
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_BULKIN_QCTRL, 5, 5, tmp);

	dev->rx_urb_size = 1024 * 20;

	tmp[0] = 0x34;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_LOW, 1, 1, tmp);

	tmp[0] = 0x52;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_PAUSE_WATERLVL_HIGH,
			  1, 1, tmp);

	dev->net->features |= NETIF_F_IP_CSUM;
	dev->net->features |= NETIF_F_IPV6_CSUM;
	if (usb_device_no_sg_constraint(dev->udev))
		dev->can_dma_sg = 1;
	dev->net->features |= NETIF_F_SG | NETIF_F_TSO;

	dev->net->hw_features |= NETIF_F_IP_CSUM;
	dev->net->hw_features |= NETIF_F_IPV6_CSUM;
	dev->net->hw_features |= NETIF_F_SG | NETIF_F_TSO;

	/* Enable checksum offload */
	*tmp = AX_RXCOE_IP | AX_RXCOE_TCP | AX_RXCOE_UDP |
	       AX_RXCOE_TCPV6 | AX_RXCOE_UDPV6;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RXCOE_CTL, 1, 1, tmp);

	*tmp = AX_TXCOE_IP | AX_TXCOE_TCP | AX_TXCOE_UDP |
	       AX_TXCOE_TCPV6 | AX_TXCOE_UDPV6;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_TXCOE_CTL, 1, 1, tmp);

	ax179_data->checksum |= AX_RX_CHECKSUM | AX_TX_CHECKSUM;

	/* Configure RX control register => start operation */
	*tmp16 = AX_RX_CTL_DROPCRCERR | AX_RX_CTL_START | AX_RX_CTL_AP |
		 AX_RX_CTL_AMALL | AX_RX_CTL_AB;
	if (NET_IP_ALIGN == 0)
		*tmp16 |= AX_RX_CTL_IPE;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_RX_CTL, 2, 2, tmp16);

	*tmp = AX_MONITOR_MODE_PMETYPE | AX_MONITOR_MODE_PMEPOL |
						AX_MONITOR_MODE_RWMP;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, tmp);

	ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_MONITOR_MODE, 1, 1, tmp, 0);
	netdev_dbg(dev->net, "Monitor mode = 0x%02x\n", *tmp);

	/* Configure default medium type => giga */
	*tmp16 = AX_MEDIUM_TXFLOW_CTRLEN | AX_MEDIUM_RXFLOW_CTRLEN |
		 AX_MEDIUM_FULL_DUPLEX | AX_MEDIUM_GIGAMODE;

	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			  2, 2, tmp16);

	ax88179_led_setting(dev);

	ax88179_EEE_setting(dev);

	ax88179_Gether_setting(dev);

	/* Restart autoneg */
	mii_nway_restart(&dev->mii);

	netif_carrier_off(dev->net);

	kfree(buf);
	netdev_dbg(dev->net, "mtu %d\n", dev->net->mtu);

	return 0;

}

static int ax88179_stop(struct usbnet *dev)
{
	u16 tmp16 = 0;

	ax88179_read_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			 2, 2, &tmp16, 1);
	tmp16 &= ~AX_MEDIUM_RECEIVE_EN;
	ax88179_write_cmd(dev, AX_ACCESS_MAC, AX_MEDIUM_STATUS_MODE,
			  2, 2, &tmp16);
	return 0;
}

static const struct driver_info ax88179_info = {
	.description = "ASIX AX88179 USB 3.0 Gigabit Ethernet",
	.bind	= ax88179_bind,
	.unbind	= ax88179_unbind,
	.status	= ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset	= ax88179_reset,
	.stop	= ax88179_stop,
	.flags	= FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct driver_info ax88178a_info = {
	.description = "ASIX AX88178A USB 2.0 Gigabit Ethernet",
	.bind	= ax88179_bind,
	.unbind	= ax88179_unbind,
	.status	= ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset	= ax88179_reset,
	.stop	= ax88179_stop,
	.flags	= FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct driver_info sitecom_info = {
	.description = "Sitecom USB 3.0 to Gigabit Adapter",
	.bind	= ax88179_bind,
	.unbind	= ax88179_unbind,
	.status	= ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset	= ax88179_reset,
	.stop	= ax88179_stop,
	.flags	= FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct driver_info lenovo_info = {
	.description = "ThinkPad OneLinkDock USB GigaLAN",
	.bind	= ax88179_bind,
	.unbind	= ax88179_unbind,
	.status	= ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset	= ax88179_reset,
	.stop	= ax88179_stop,
	.flags	= FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct driver_info toshiba_info = {
	.description = "Toshiba USB 3.0 to Gigabit LAN Adapter",
	.bind	= ax88179_bind,
	.unbind	= ax88179_unbind,
	.status	= ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset	= ax88179_reset,
	.stop	= ax88179_stop,
	.flags	= FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct driver_info samsung_info = {
	.description = "Samsung USB Ethernet Adapter",
	.bind	= ax88179_bind,
	.unbind = ax88179_unbind,
	.status = ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset	= ax88179_reset,
	.stop	= ax88179_stop,
	.flags	= FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct driver_info dlink_info = {
	.description = "DUB-1312/1332 USB3.0 to Gigabit Ethernet Adapter",
	.bind	= ax88179_bind,
	.unbind = ax88179_unbind,
	.status = ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset	= ax88179_reset,
	.stop	= ax88179_stop,
	.flags	= FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct driver_info mct_info = {
	.description = "USB 3.0 to Gigabit Ethernet Adapter",
	.bind   = ax88179_bind,
	.unbind = ax88179_unbind,
	.status = ax88179_status,
	.link_reset = ax88179_link_reset,
	.reset  = ax88179_reset,
	.stop   = ax88179_stop,
	.flags  = FLAG_ETHER | FLAG_FRAMING_AX | FLAG_AVOID_UNLINK_URBS,
	.rx_fixup = ax88179_rx_fixup,
	.tx_fixup = ax88179_tx_fixup,
};

static const struct usb_device_id	products[] = {
{
	/* ASIX AX88179 10/100/1000 */
	USB_DEVICE(0x0b95, 0x1790),
	.driver_info = (unsigned long)&ax88179_info,
}, {
	/* ASIX AX88178A 10/100/1000 */
	USB_DEVICE(0x0b95, 0x178a),
	.driver_info = (unsigned long)&ax88178a_info,
}, {
	/* Sitecom USB 3.0 to Gigabit Adapter */
	USB_DEVICE(0x0df6, 0x0072),
	.driver_info = (unsigned long)&sitecom_info,
}, {
	/* ThinkPad OneLinkDock USB GigaLAN */
	USB_DEVICE(0x17ef, 0x304b),
	.driver_info = (unsigned long)&lenovo_info,
}, {
	/* Toshiba USB3.0 to Gigabit LAN Adapter */
	USB_DEVICE(0x0930, 0x0a13),
	.driver_info = (unsigned long)&toshiba_info,
}, {
	/* Samsung USB Ethernet Adapter */
	USB_DEVICE(0x04e8, 0xa100),
	.driver_info = (unsigned long)&samsung_info,
}, {
	/* D-Link DUB-13x2 Ethernet Adapter */
	USB_DEVICE(0x2001, 0x4a00),
	.driver_info = (unsigned long)&dlink_info,
}, {
	/* MCT USB 3.0 to Gigabit Ethernet Adapter */
	USB_DEVICE(0x0711, 0x0179),
	.driver_info = (unsigned long)&mct_info,
},
	{ },		/* END */
};
MODULE_DEVICE_TABLE(usb, products);

static struct usb_driver asix_driver = {
	.name =		"ax88179_178a",
	.id_table =	products,
	.probe =	usbnet_probe,
	.suspend =	ax88179_suspend,
	.resume =	ax88179_resume,
	.disconnect =	usbnet_disconnect,
};


static int __init asix_init(void)
{
	return usb_register(&asix_driver);
}
module_init(asix_init);

static void __exit asix_exit(void)
{
	usb_deregister(&asix_driver);
}
module_exit(asix_exit);

MODULE_AUTHOR("David Hollis");
MODULE_DESCRIPTION("ASIX AX88179_178A based USB 2.0/3.0 Gigabit Ethernet Devices");
MODULE_LICENSE("GPL");

