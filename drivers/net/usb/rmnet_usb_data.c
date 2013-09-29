/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/ratelimit.h>
#include <linux/usb/usbnet.h>
#include <linux/msm_rmnet.h>

#include "rmnet_usb.h"

#define RMNET_DATA_LEN			2000
#define RMNET_HEADROOM_W_MUX		(sizeof(struct mux_hdr) + \
					sizeof(struct QMI_QOS_HDR_S))
#define RMNET_HEADROOM			sizeof(struct QMI_QOS_HDR_S)
#define RMNET_TAILROOM			MAX_PAD_BYTES(4);

static unsigned int override_data_muxing = 1;
module_param(override_data_muxing, uint, S_IRUGO | S_IWUSR);

static unsigned int no_rmnet_devs = 1;
module_param(no_rmnet_devs, uint, S_IRUGO | S_IWUSR);

unsigned int no_rmnet_insts_per_dev = 4;
module_param(no_rmnet_insts_per_dev, uint, S_IRUGO | S_IWUSR);

/*
 * To support  mux on multiple devices, bit position represents device
 * and value represnts if mux is enabled or disabled.
 * e.g. bit 0: mdm over HSIC, bit1: mdm over hsusb
 */
static unsigned long mux_enabled;
module_param(mux_enabled, ulong, S_IRUGO | S_IWUSR);

static unsigned int no_fwd_rmnet_links;
module_param(no_fwd_rmnet_links, uint, S_IRUGO | S_IWUSR);

struct usbnet	*unet_list[TOTAL_RMNET_DEV_COUNT];

/* net device name prefixes, indexed by driver_info->data */
static const char * const rmnet_names[] = {
	"rmnet_usb%d",
	"rmnet2_usb%d",
};

/* net device reverse link name prefixes, indexed by driver_info->data */
static const char * const rev_rmnet_names[] = {
	"rev_rmnet_usb%d",
	"rev_rmnet2_usb%d",
};
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

static int rmnet_data_start(void);
static bool rmnet_data_init;

static int rmnet_init(const char *val, const struct kernel_param *kp)
{
	int ret = 0;

	if (rmnet_data_init) {
		pr_err("dynamic setting rmnet params currently unsupported\n");
		return -EINVAL;
	}

	ret = param_set_bool(val, kp);
	if (ret)
		return ret;

	rmnet_data_start();

	return ret;
}

static struct kernel_param_ops rmnet_init_ops = {
	.set = rmnet_init,
	.get = param_get_bool,
};
module_param_cb(rmnet_data_init, &rmnet_init_ops, &rmnet_data_init,
		S_IRUGO | S_IWUSR);

static void rmnet_usb_setup(struct net_device *, int mux_enabled);
static int rmnet_ioctl(struct net_device *, struct ifreq *, int);

static int rmnet_usb_suspend(struct usb_interface *iface, pm_message_t message)
{
	struct usbnet		*unet = usb_get_intfdata(iface);
	struct rmnet_ctrl_dev	*dev;
	int			i, n, rdev_cnt, unet_id;
	int			retval = 0;

	rdev_cnt = unet->data[4] ? no_rmnet_insts_per_dev : 1;

	for (n = 0; n < rdev_cnt; n++) {
		unet_id = n + unet->driver_info->data * no_rmnet_insts_per_dev;
		unet =
		unet->data[4] ? unet_list[unet_id] : usb_get_intfdata(iface);

		dev = (struct rmnet_ctrl_dev *)unet->data[1];
		spin_lock_irq(&unet->txq.lock);
		if (work_busy(&dev->get_encap_work) || unet->txq.qlen) {
			spin_unlock_irq(&unet->txq.lock);
			retval = -EBUSY;
			goto abort_suspend;
		}

		set_bit(EVENT_DEV_ASLEEP, &unet->flags);
		spin_unlock_irq(&unet->txq.lock);

		usb_kill_anchored_urbs(&dev->rx_submitted);
		if (work_busy(&dev->get_encap_work)) {
			spin_lock_irq(&unet->txq.lock);
			clear_bit(EVENT_DEV_ASLEEP, &unet->flags);
			spin_unlock_irq(&unet->txq.lock);
			retval = -EBUSY;
			goto abort_suspend;
		}
	}

	for (n = 0; n < rdev_cnt; n++) {
		unet_id = n + unet->driver_info->data * no_rmnet_insts_per_dev;
		unet =
		unet->data[4] ? unet_list[unet_id] : usb_get_intfdata(iface);

		dev = (struct rmnet_ctrl_dev *)unet->data[1];
		netif_device_detach(unet->net);
		usbnet_terminate_urbs(unet);
		netif_device_attach(unet->net);
	}

	return 0;

abort_suspend:
	for (i = 0; i < n; i++) {
		unet_id = i + unet->driver_info->data * no_rmnet_insts_per_dev;
		unet =
		unet->data[4] ? unet_list[unet_id] : usb_get_intfdata(iface);

		dev = (struct rmnet_ctrl_dev *)unet->data[1];
		rmnet_usb_ctrl_start_rx(dev);
		spin_lock_irq(&unet->txq.lock);
		clear_bit(EVENT_DEV_ASLEEP, &unet->flags);
		spin_unlock_irq(&unet->txq.lock);
	}
	return retval;
}

static int rmnet_usb_resume(struct usb_interface *iface)
{
	struct usbnet		*unet = usb_get_intfdata(iface);
	struct rmnet_ctrl_dev	*dev;
	int			n, rdev_cnt, unet_id;

	rdev_cnt = unet->data[4] ? no_rmnet_insts_per_dev : 1;

	for (n = 0; n < rdev_cnt; n++) {
		unet_id = n + unet->driver_info->data * no_rmnet_insts_per_dev;
		unet =
		unet->data[4] ? unet_list[unet_id] : usb_get_intfdata(iface);

		dev = (struct rmnet_ctrl_dev *)unet->data[1];
		rmnet_usb_ctrl_start_rx(dev);
		usb_set_intfdata(iface, unet);
		unet->suspend_count = 1;
		usbnet_resume(iface);
	}

	return 0;
}

static int rmnet_usb_bind(struct usbnet *usbnet, struct usb_interface *iface)
{
	struct usb_host_endpoint	*endpoint = NULL;
	struct usb_host_endpoint	*bulk_in = NULL;
	struct usb_host_endpoint	*bulk_out = NULL;
	struct usb_host_endpoint	*int_in = NULL;
	struct driver_info		*info = usbnet->driver_info;
	int				status = 0;
	int				i;
	int				numends;
	bool				mux;

	mux = test_bit(info->data, &mux_enabled);

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
	usbnet->in = usb_rcvbulkpipe(usbnet->udev,
		bulk_in->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	usbnet->out = usb_sndbulkpipe(usbnet->udev,
		bulk_out->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	usbnet->status = int_in;

	/*change name of net device to rmnet_usbx here*/
	if (mux && (info->in > no_fwd_rmnet_links))
		strlcpy(usbnet->net->name, rev_rmnet_names[info->data],
				IFNAMSIZ);
	else
		strlcpy(usbnet->net->name, rmnet_names[info->data],
				IFNAMSIZ);

	if (mux)
		usbnet->rx_urb_size = usbnet->hard_mtu + sizeof(struct mux_hdr)
			+ MAX_PAD_BYTES(4);
out:
	return status;
}

static int rmnet_usb_data_dmux(struct sk_buff *skb,  struct urb *rx_urb)
{
	struct mux_hdr	*hdr;
	size_t		pad_len;
	size_t		total_len;
	unsigned int	mux_id;

	hdr = (struct mux_hdr *)skb->data;
	mux_id = hdr->mux_id;
	if (!mux_id  || mux_id > no_rmnet_insts_per_dev) {
		pr_err_ratelimited("%s: Invalid data channel id %u.\n",
				__func__, mux_id);
		return -EINVAL;
	}

	pad_len = hdr->padding_info >> MUX_PAD_SHIFT;
	if (pad_len > MAX_PAD_BYTES(4)) {
		pr_err_ratelimited("%s: Invalid pad len %d\n",
			__func__, pad_len);
		return -EINVAL;
	}

	total_len = le16_to_cpu(hdr->pkt_len_w_padding);
	if (!total_len || !(total_len - pad_len)) {
		pr_err_ratelimited("%s: Invalid pkt length %d\n", __func__,
				total_len);
		return -EINVAL;
	}

	skb->data = (unsigned char *)(hdr + 1);
	skb_reset_tail_pointer(skb);
	rx_urb->actual_length = total_len - pad_len;

	return mux_id - 1;
}

static struct sk_buff *rmnet_usb_data_mux(struct sk_buff *skb, unsigned int id)
{
	struct	mux_hdr *hdr;
	size_t	len;
	struct sk_buff *new_skb;

	if ((skb->len & 0x3) && (skb_tailroom(skb) < (4 - (skb->len & 0x3)))) {
		new_skb = skb_copy_expand(skb, skb_headroom(skb),
					  4 - (skb->len & 0x3), GFP_ATOMIC);
		dev_kfree_skb_any(skb);
		if (new_skb == NULL) {
			pr_err("%s: cannot allocate skb\n", __func__);
			return NULL;
		}
		skb = new_skb;
	}

	hdr = (struct mux_hdr *)skb_push(skb, sizeof(struct mux_hdr));
	hdr->mux_id = id + 1;
	len = skb->len - sizeof(struct mux_hdr);

	/*add padding if len is not 4 byte aligned*/
	skb_put(skb, ALIGN(len, 4) - len);

	hdr->pkt_len_w_padding = cpu_to_le16(skb->len - sizeof(struct mux_hdr));
	hdr->padding_info = (ALIGN(len, 4) - len) << MUX_PAD_SHIFT;

	return skb;
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

	if (!override_data_muxing && dev->data[4])
		rmnet_usb_data_mux(skb, dev->data[3]);

	if (skb)
		DBG1("[%s] Tx packet #%lu len=%d mark=0x%x\n",
			dev->net->name, dev->net->stats.tx_packets,
			skb->len, skb->mark);

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
		/*
		 * There is no good way to determine if a packet has
		 * a MAP header. For now default to MAP protocol
		 */
		protocol = htons(ETH_P_MAP);
	}

	return protocol;
}

static void rmnet_usb_rx_complete(struct urb *rx_urb)
{
	struct sk_buff	*skb = (struct sk_buff *) rx_urb->context;
	struct skb_data	*entry = (struct skb_data *) skb->cb;
	struct usbnet	*dev = entry->dev;
	unsigned int	unet_offset;
	unsigned int	unet_id;
	int		mux_id;

	unet_offset =  dev->driver_info->data * no_rmnet_insts_per_dev;

	if (!override_data_muxing && !rx_urb->status && dev->data[4]) {
		mux_id = rmnet_usb_data_dmux(skb, rx_urb);
		if (mux_id < 0) {
			/*resubmit urb and free skb in rx_complete*/
			rx_urb->status = -EINVAL;
		} else {
			/*map urb to actual network iface based on mux id*/
			unet_id = unet_offset + mux_id;
			skb->dev = unet_list[unet_id]->net;
			entry->dev = unet_list[unet_id];
		}
	}

	rx_complete(rx_urb);
}

static int rmnet_usb_rx_fixup(struct usbnet *dev, struct sk_buff *skb)
{
	if (test_bit(RMNET_MODE_LLP_IP, &dev->data[0]))
		skb->protocol = rmnet_ip_type_trans(skb, dev->net);
	else /*set zero for eth mode*/
		skb->protocol = 0;

	DBG1("[%s] Rx packet #%lu len=%d\n",
		dev->net->name, dev->net->stats.rx_packets, skb->len);

	return 1;
}

static int rmnet_usb_manage_power(struct usbnet *dev, int on)
{
	dev->intf->needs_remote_wakeup = on;
	return 0;
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

	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		break;

	case RMNET_IOCTL_SET_MRU:
		if (test_bit(EVENT_DEV_OPEN, &unet->flags))
			return -EBUSY;

		/* 16K max */
		if ((size_t)ifr->ifr_ifru.ifru_data > 0x4000)
			return -EINVAL;

		unet->rx_urb_size = (size_t)ifr->ifr_ifru.ifru_data;
		DBG0("[%s] rmnet_ioctl(): SET MRU to %u\n", dev->name,
				unet->rx_urb_size);
		break;

	case RMNET_IOCTL_GET_MRU:
		ifr->ifr_ifru.ifru_data = (void *)unet->rx_urb_size;
		break;

	case RMNET_IOCTL_GET_DRIVER_NAME:
		rc = copy_to_user(ifr->ifr_ifru.ifru_data, unet->driver_name,
				strlen(unet->driver_name));
		break;
	default:
		dev_err(&unet->intf->dev, "[%s] error: "
			"rmnet_ioct called for unsupported cmd[%d]",
			dev->name, cmd);
		return -EINVAL;
	}

	DBG2("[%s] %s: cmd=0x%x opmode old=0x%08x new=0x%08lx\n",
		dev->name, __func__, cmd, old_opmode, unet->data[0]);

	return rc;
}

static void rmnet_usb_setup(struct net_device *dev, int mux_enabled)
{
	/* Using Ethernet mode by default */
	dev->netdev_ops = &rmnet_usb_ops_ether;

	/* set this after calling ether_setup */
	dev->mtu = RMNET_DATA_LEN;

	if (mux_enabled) {
		dev->needed_headroom = RMNET_HEADROOM_W_MUX;

		/*max pad bytes for 4 byte alignment*/
		dev->needed_tailroom = RMNET_TAILROOM;
	} else {
		dev->needed_headroom = RMNET_HEADROOM;
	}

	random_ether_addr(dev->dev_addr);
	dev->watchdog_timeo = 1000; /* 10 seconds? */
}

static int rmnet_usb_data_status(struct seq_file *s, void *unused)
{
	struct usbnet *unet = s->private;

	seq_printf(s, "RMNET_MODE_LLP_IP:  %d\n",
			test_bit(RMNET_MODE_LLP_IP, &unet->data[0]));
	seq_printf(s, "RMNET_MODE_LLP_ETH: %d\n",
			test_bit(RMNET_MODE_LLP_ETH, &unet->data[0]));
	seq_printf(s, "RMNET_MODE_QOS:     %d\n",
			test_bit(RMNET_MODE_QOS, &unet->data[0]));
	seq_printf(s, "Net MTU:            %u\n", unet->net->mtu);
	seq_printf(s, "rx_urb_size:        %u\n", unet->rx_urb_size);
	seq_printf(s, "rx skb q len:       %u\n", unet->rxq.qlen);
	seq_printf(s, "rx skb done q len:  %u\n", unet->done.qlen);
	seq_printf(s, "rx errors:          %lu\n", unet->net->stats.rx_errors);
	seq_printf(s, "rx over errors:     %lu\n",
			unet->net->stats.rx_over_errors);
	seq_printf(s, "rx length errors:   %lu\n",
			unet->net->stats.rx_length_errors);
	seq_printf(s, "rx packets:         %lu\n", unet->net->stats.rx_packets);
	seq_printf(s, "rx bytes:           %lu\n", unet->net->stats.rx_bytes);
	seq_printf(s, "tx skb q len:       %u\n", unet->txq.qlen);
	seq_printf(s, "tx errors:          %lu\n", unet->net->stats.tx_errors);
	seq_printf(s, "tx packets:         %lu\n", unet->net->stats.tx_packets);
	seq_printf(s, "tx bytes:           %lu\n", unet->net->stats.tx_bytes);
	seq_printf(s, "EVENT_DEV_OPEN:     %d\n",
			test_bit(EVENT_DEV_OPEN, &unet->flags));
	seq_printf(s, "EVENT_TX_HALT:      %d\n",
			test_bit(EVENT_TX_HALT, &unet->flags));
	seq_printf(s, "EVENT_RX_HALT:      %d\n",
			test_bit(EVENT_RX_HALT, &unet->flags));
	seq_printf(s, "EVENT_RX_MEMORY:    %d\n",
			test_bit(EVENT_RX_MEMORY, &unet->flags));
	seq_printf(s, "EVENT_DEV_ASLEEP:   %d\n",
			test_bit(EVENT_DEV_ASLEEP, &unet->flags));

	return 0;
}

static int rmnet_usb_data_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, rmnet_usb_data_status, inode->i_private);
}

const struct file_operations rmnet_usb_data_fops = {
	.open = rmnet_usb_data_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int rmnet_usb_data_debugfs_init(struct usbnet *unet)
{
	struct dentry *rmnet_usb_data_dbg_root;
	struct dentry *rmnet_usb_data_dentry;

	rmnet_usb_data_dbg_root = debugfs_create_dir(unet->net->name, NULL);
	if (!rmnet_usb_data_dbg_root || IS_ERR(rmnet_usb_data_dbg_root))
		return -ENODEV;

	rmnet_usb_data_dentry = debugfs_create_file("status",
		S_IRUGO | S_IWUSR,
		rmnet_usb_data_dbg_root, unet,
		&rmnet_usb_data_fops);

	if (!rmnet_usb_data_dentry) {
		debugfs_remove_recursive(rmnet_usb_data_dbg_root);
		return -ENODEV;
	}

	unet->data[2] = (unsigned long)rmnet_usb_data_dbg_root;

	return 0;
}

static void rmnet_usb_data_debugfs_cleanup(struct usbnet *unet)
{
	struct dentry *root = (struct dentry *)unet->data[2];

	if (root) {
		debugfs_remove_recursive(root);
		unet->data[2] = 0;
	}
}

static int rmnet_usb_probe(struct usb_interface *iface,
		const struct usb_device_id *prod)
{
	struct usbnet		*unet;
	struct driver_info	*info = (struct driver_info *)prod->driver_info;
	struct usb_device	*udev;
	int			status = 0;
	unsigned int		i, unet_id, rdev_cnt, n = 0;
	bool			mux;
	struct rmnet_ctrl_dev	*dev;

	udev = interface_to_usbdev(iface);

	if (iface->num_altsetting != 1) {
		dev_err(&iface->dev, "%s invalid num_altsetting %u\n",
			__func__, iface->num_altsetting);
		status = -EINVAL;
		goto out;
	}

	mux = test_bit(info->data, &mux_enabled);
	rdev_cnt = mux ? no_rmnet_insts_per_dev : 1;
	info->in = 0;

	for (n = 0; n < rdev_cnt; n++) {

		/* Use this filed to increment device count this will be
		 * used by bind to determin the forward link and reverse
		 * link network interface names.
		 */
		info->in++;
		status = usbnet_probe(iface, prod);
		if (status < 0) {
			dev_err(&iface->dev, "usbnet_probe failed %d\n",
					status);
			goto out;
		}

		unet_id = n + info->data * no_rmnet_insts_per_dev;

		unet_list[unet_id] = unet = usb_get_intfdata(iface);

		/*store mux id for later access*/
		unet->data[3] = n;

		/*save mux info for control and usbnet devices*/
		unet->data[1] = unet->data[4] = mux;

		/*set rmnet operation mode to eth by default*/
		set_bit(RMNET_MODE_LLP_ETH, &unet->data[0]);

		/*update net device*/
		rmnet_usb_setup(unet->net, mux);

		/*create /sys/class/net/rmnet_usbx/dbg_mask*/
		status = device_create_file(&unet->net->dev,
				&dev_attr_dbg_mask);
		if (status) {
			usbnet_disconnect(iface);
			goto out;
		}

		status = rmnet_usb_ctrl_probe(iface, unet->status, info->data,
				&unet->data[1]);
		if (status) {
			device_remove_file(&unet->net->dev, &dev_attr_dbg_mask);
			usbnet_disconnect(iface);
			goto out;
		}

		status = rmnet_usb_data_debugfs_init(unet);
		if (status)
			dev_dbg(&iface->dev,
					"mode debugfs file is not available\n");
	}

	usb_enable_autosuspend(udev);

	if (udev->parent && !udev->parent->parent) {
		/* allow modem and roothub to wake up suspended system */
		device_set_wakeup_enable(&udev->dev, 1);
		device_set_wakeup_enable(&udev->parent->dev, 1);
	}

	return 0;

out:
	for (i = 0; i < n; i++) {
		/* This cleanup happens only for MUX case */
		unet_id = i + info->data * no_rmnet_insts_per_dev;
		unet = unet_list[unet_id];
		dev = (struct rmnet_ctrl_dev *)unet->data[1];

		rmnet_usb_data_debugfs_cleanup(unet);
		rmnet_usb_ctrl_disconnect(dev);
		device_remove_file(&unet->net->dev, &dev_attr_dbg_mask);
		usb_set_intfdata(iface, unet_list[unet_id]);
		usbnet_disconnect(iface);
		unet_list[unet_id] = NULL;
	}

	return status;
}

static void rmnet_usb_disconnect(struct usb_interface *intf)
{
	struct usbnet		*unet = usb_get_intfdata(intf);
	struct rmnet_ctrl_dev	*dev;
	unsigned int		n, rdev_cnt, unet_id;

	rdev_cnt = unet->data[4] ? no_rmnet_insts_per_dev : 1;

	device_set_wakeup_enable(&unet->udev->dev, 0);

	for (n = 0; n < rdev_cnt; n++) {
		unet_id = n + unet->driver_info->data * no_rmnet_insts_per_dev;
		unet =
		unet->data[4] ? unet_list[unet_id] : usb_get_intfdata(intf);
		device_remove_file(&unet->net->dev, &dev_attr_dbg_mask);

		dev = (struct rmnet_ctrl_dev *)unet->data[1];
		rmnet_usb_ctrl_disconnect(dev);
		unet->data[0] = 0;
		unet->data[1] = 0;
		rmnet_usb_data_debugfs_cleanup(unet);
		usb_set_intfdata(intf, unet);
		usbnet_disconnect(intf);
		unet_list[unet_id] = NULL;
	}
}

static struct driver_info rmnet_info = {
	.description   = "RmNET net device",
	.flags         = FLAG_SEND_ZLP,
	.bind          = rmnet_usb_bind,
	.tx_fixup      = rmnet_usb_tx_fixup,
	.rx_fixup      = rmnet_usb_rx_fixup,
	.rx_complete   = rmnet_usb_rx_complete,
	.manage_power  = rmnet_usb_manage_power,
	.data          = 0,
};

static struct driver_info rmnet_usb_info = {
	.description   = "RmNET net device",
	.flags         = FLAG_SEND_ZLP,
	.bind          = rmnet_usb_bind,
	.tx_fixup      = rmnet_usb_tx_fixup,
	.rx_fixup      = rmnet_usb_rx_fixup,
	.rx_complete   = rmnet_usb_rx_complete,
	.manage_power  = rmnet_usb_manage_power,
	.data          = 1,
};

static const struct usb_device_id vidpids[] = {
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9034, 4),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9034, 5),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9034, 6),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9034, 7),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9048, 5),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9048, 6),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9048, 7),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9048, 8),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x904c, 6),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x904c, 7),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x904c, 8),
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9075, 6), /*mux over hsic mdm*/
	.driver_info = (unsigned long)&rmnet_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9079, 5),
	.driver_info = (unsigned long)&rmnet_usb_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9079, 6),
	.driver_info = (unsigned long)&rmnet_usb_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9079, 7),
	.driver_info = (unsigned long)&rmnet_usb_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x9079, 8),
	.driver_info = (unsigned long)&rmnet_usb_info,
	},
	{ USB_DEVICE_INTERFACE_NUMBER(0x05c6, 0x908A, 6), /*mux over hsic mdm*/
	.driver_info = (unsigned long)&rmnet_info,
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

static int rmnet_data_start(void)
{
	int	retval;

	if (no_rmnet_devs > MAX_RMNET_DEVS) {
		pr_err("ERROR:%s: param no_rmnet_devs(%d) > than maximum(%d)",
			__func__, no_rmnet_devs, MAX_RMNET_DEVS);
		return -EINVAL;
	}

	/* initialize ctrl devices */
	retval = rmnet_usb_ctrl_init(no_rmnet_devs, no_rmnet_insts_per_dev);
	if (retval) {
		pr_err("rmnet_usb_cmux_init failed: %d", retval);
		return retval;
	}

	retval = usb_register(&rmnet_usb);
	if (retval) {
		pr_err("usb_register failed: %d", retval);
		return retval;
	}

	return retval;
}

static void __exit rmnet_usb_exit(void)
{
	usb_deregister(&rmnet_usb);
	rmnet_usb_ctrl_exit(no_rmnet_devs, no_rmnet_insts_per_dev);
}
module_exit(rmnet_usb_exit);

MODULE_DESCRIPTION("msm rmnet usb device");
MODULE_LICENSE("GPL v2");
