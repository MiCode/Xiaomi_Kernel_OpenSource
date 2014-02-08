/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

/*
 * WWAN Network Interface.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/wakelock.h>
#include <linux/msm_rmnet.h>
#include <linux/if_arp.h>
#include <linux/platform_device.h>
#include <net/pkt_sched.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/ratelimit.h>
#include <linux/ipa.h>

#define WWAN_DEV_NAME "rmnet%d"
#define WWAN_METADATA_SHFT 16
#define WWAN_METADATA_MASK 0x00FF0000
#define IPA_RM_INACTIVITY_TIMER 1000
#define WWAN_DEVICE_COUNT (8)
#define WWAN_DATA_LEN 2000
#define HEADROOM_FOR_A2_MUX   8 /* for mux header */
#define TAILROOM              8 /* for padding by mux layer */

enum wwan_device_status {
	WWAN_DEVICE_INACTIVE = 0,
	WWAN_DEVICE_ACTIVE   = 1
};
static enum ipa_rm_resource_name
	ipa_rm_resource_by_ch_id[WWAN_DEVICE_COUNT] = {
	IPA_RM_RESOURCE_WWAN_0_PROD,
	IPA_RM_RESOURCE_WWAN_1_PROD,
	IPA_RM_RESOURCE_WWAN_2_PROD,
	IPA_RM_RESOURCE_WWAN_3_PROD,
	IPA_RM_RESOURCE_WWAN_4_PROD,
	IPA_RM_RESOURCE_WWAN_5_PROD,
	IPA_RM_RESOURCE_WWAN_6_PROD,
	IPA_RM_RESOURCE_WWAN_7_PROD
};
static enum a2_mux_logical_channel_id
	a2_mux_lcid_by_ch_id[WWAN_DEVICE_COUNT] = {
	A2_MUX_WWAN_0,
	A2_MUX_WWAN_1,
	A2_MUX_WWAN_2,
	A2_MUX_WWAN_3,
	A2_MUX_WWAN_4,
	A2_MUX_WWAN_5,
	A2_MUX_WWAN_6,
	A2_MUX_WWAN_7
};

/**
 * struct wwan_private - WWAN private data
 * @stats: iface statistics
 * @ch_id: channel id
 * @lock: spinlock for mutual exclusion
 * @device_status: holds device status
 *
 * WWAN private - holds all relevant info about WWAN driver
 */
struct wwan_private {
	struct net_device_stats stats;
	uint32_t ch_id;
	spinlock_t lock;
	struct completion resource_granted_completion;
	enum wwan_device_status device_status;
};

static struct net_device *netdevs[WWAN_DEVICE_COUNT];

static __be16 wwan_ip_type_trans(struct sk_buff *skb)
{
	__be16 protocol = 0;
	/* Determine L3 protocol */
	switch (skb->data[0] & 0xf0) {
	case 0x40:
		protocol = htons(ETH_P_IP);
		break;
	case 0x60:
		protocol = htons(ETH_P_IPV6);
		break;
	default:
		pr_err("[%s] %s() L3 protocol decode error: 0x%02x",
		       skb->dev->name, __func__, skb->data[0] & 0xf0);
		/* skb will be dropped in upper layer for unknown protocol */
		break;
	}
	return protocol;
}

/**
 * a2_mux_recv_notify() - Deliver an RX packet to network stack
 *
 * @skb: skb to be delivered
 * @dev: network device
 *
 * Return codes:
 * None
 */
static void a2_mux_recv_notify(void *dev, struct sk_buff *skb)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);

	skb->dev = dev;
	skb->protocol = wwan_ip_type_trans(skb);
	wwan_ptr->stats.rx_packets++;
	wwan_ptr->stats.rx_bytes += skb->len;
	pr_debug("[%s] Rx packet #%lu len=%d\n",
		skb->dev->name,
		wwan_ptr->stats.rx_packets, skb->len);
	netif_rx(skb);
}

/**
 * wwan_send_packet() - Deliver a TX packet to A2 MUX driver.
 *
 * @skb: skb to be delivered
 * @dev: network device
 *
 * Return codes:
 * 0: success
 * -EAGAIN: A2 MUX is not ready to send the skb. try later
 * -EFAULT: A2 MUX rejected the skb
 * -EPREM: Unknown error
 */
static int wwan_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);
	int ret;

	dev->trans_start = jiffies;
	ret = a2_mux_write(a2_mux_lcid_by_ch_id[wwan_ptr->ch_id], skb);
	if (ret != 0 && ret != -EAGAIN && ret != -EFAULT) {
		pr_err("[%s] %s: write returned error %d",
			dev->name, __func__, ret);
		return -EPERM;
	}
	return ret;
}

/**
 * a2_mux_write_done() - Update device statistics and start
 * network stack queue is was stop and A2 MUX queue is below low
 * watermark.
 *
 * @dev: network device
 * @skb: skb to be delivered
 *
 * Return codes:
 * None
 */
static void a2_mux_write_done(void *dev, struct sk_buff *skb)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);
	unsigned long flags;

	pr_debug("%s: write complete\n", __func__);
	wwan_ptr->stats.tx_packets++;
	wwan_ptr->stats.tx_bytes += skb->len;
	pr_debug("[%s] Tx packet #%lu len=%d mark=0x%x\n",
	    ((struct net_device *)(dev))->name, wwan_ptr->stats.tx_packets,
	    skb->len, skb->mark);
	dev_kfree_skb_any(skb);
	spin_lock_irqsave(&wwan_ptr->lock, flags);
	if (netif_queue_stopped(dev) &&
	    a2_mux_is_ch_low(a2_mux_lcid_by_ch_id[wwan_ptr->ch_id])) {
		pr_debug("%s: Low WM hit, waking queue=%p\n",
		      __func__, skb);
		netif_wake_queue(dev);
	}
	if (a2_mux_is_ch_empty(a2_mux_lcid_by_ch_id[wwan_ptr->ch_id])) {
		if (ipa_emb_ul_pipes_empty())
			ipa_rm_inactivity_timer_release_resource(
				ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
		else
			pr_err_ratelimited("%s: ch=%d empty but UL desc FIFOs not empty\n",
					__func__, wwan_ptr->ch_id);
	}
	spin_unlock_irqrestore(&wwan_ptr->lock, flags);
}

/**
 * a2_mux_notify() - Callback function for A2 MUX events Handles
 * A2_MUX_RECEIVE and A2_MUX_WRITE_DONE events.
 *
 * @dev: network device
 * @event: A2 MUX event
 * @data: Additional data provided by A2 MUX
 *
 * Return codes:
 * None
 */
static void a2_mux_notify(void *dev, enum a2_mux_event_type event,
			  unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;

	switch (event) {
	case A2_MUX_RECEIVE:
		if (!skb) {
			pr_err("[%s] %s: No skb received",
			   ((struct net_device *)dev)->name, __func__);
			return;
		}
		a2_mux_recv_notify(dev, skb);
		break;
	case A2_MUX_WRITE_DONE:
		a2_mux_write_done(dev, skb);
		break;
	default:
		pr_err("%s: unknown event %d\n", __func__, event);
		break;
	}
}

/**
 * ipa_rm_resource_granted() - Called upon
 * IPA_RM_RESOURCE_GRANTED event. Wakes up queue is was stopped.
 *
 * @work: work object supplied ny workqueue
 *
 * Return codes:
 * None
 */
static void ipa_rm_resource_granted(void *dev)
{
	netif_wake_queue(dev);
}
/**
 * ipa_rm_notify() - Callback function for RM events. Handles
 * IPA_RM_RESOURCE_GRANTED and IPA_RM_RESOURCE_RELEASED events.
 * IPA_RM_RESOURCE_GRANTED is handled in the context of shared
 * workqueue.
 *
 * @dev: network device
 * @event: IPA RM event
 * @data: Additional data provided by IPA RM
 *
 * Return codes:
 * None
 */
static void ipa_rm_notify(void *dev, enum ipa_rm_event event,
			  unsigned long data)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);

	pr_debug("%s: event %d\n", __func__, event);
	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		if (wwan_ptr->device_status == WWAN_DEVICE_INACTIVE) {
			complete_all(&wwan_ptr->resource_granted_completion);
			break;
		}
		ipa_rm_resource_granted(dev);
		break;
	case IPA_RM_RESOURCE_RELEASED:
		break;
	default:
		pr_err("%s: unknown event %d\n", __func__, event);
		break;
	}
}

static int wwan_register_to_ipa(struct net_device *dev)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);
	struct ipa_tx_intf tx_properties = {0};
	struct ipa_ioc_tx_intf_prop tx_ioc_properties[2] = { {0}, {0} };
	struct ipa_ioc_tx_intf_prop *tx_ipv4_property;
	struct ipa_ioc_tx_intf_prop *tx_ipv6_property;
	struct ipa_rx_intf rx_properties = {0};
	struct ipa_ioc_rx_intf_prop rx_ioc_properties[2] = { {0}, {0} };
	struct ipa_ioc_rx_intf_prop *rx_ipv4_property;
	struct ipa_ioc_rx_intf_prop *rx_ipv6_property;
	int ret = 0;

	pr_debug("[%s] %s:\n", dev->name, __func__);
	tx_properties.prop = tx_ioc_properties;
	tx_ipv4_property = &tx_properties.prop[0];
	tx_ipv4_property->ip = IPA_IP_v4;
	tx_ipv4_property->dst_pipe = IPA_CLIENT_A2_EMBEDDED_CONS;
	snprintf(tx_ipv4_property->hdr_name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V4_PREF,
		 a2_mux_lcid_by_ch_id[wwan_ptr->ch_id]);
	tx_ipv6_property = &tx_properties.prop[1];
	tx_ipv6_property->ip = IPA_IP_v6;
	tx_ipv6_property->dst_pipe = IPA_CLIENT_A2_EMBEDDED_CONS;
	snprintf(tx_ipv6_property->hdr_name, IPA_RESOURCE_NAME_MAX, "%s%d",
		 A2_MUX_HDR_NAME_V6_PREF,
		 a2_mux_lcid_by_ch_id[wwan_ptr->ch_id]);
	tx_properties.num_props = 2;
	rx_properties.prop = rx_ioc_properties;
	rx_ipv4_property = &rx_properties.prop[0];
	rx_ipv4_property->ip = IPA_IP_v4;
	rx_ipv4_property->attrib.attrib_mask |= IPA_FLT_META_DATA;
	rx_ipv4_property->attrib.meta_data =
		wwan_ptr->ch_id << WWAN_METADATA_SHFT;
	rx_ipv4_property->attrib.meta_data_mask = WWAN_METADATA_MASK;
	rx_ipv4_property->src_pipe = IPA_CLIENT_A2_EMBEDDED_PROD;
	rx_ipv6_property = &rx_properties.prop[1];
	rx_ipv6_property->ip = IPA_IP_v6;
	rx_ipv6_property->attrib.attrib_mask |= IPA_FLT_META_DATA;
	rx_ipv6_property->attrib.meta_data =
		wwan_ptr->ch_id << WWAN_METADATA_SHFT;
	rx_ipv6_property->attrib.meta_data_mask = WWAN_METADATA_MASK;
	rx_ipv6_property->src_pipe = IPA_CLIENT_A2_EMBEDDED_PROD;
	rx_properties.num_props = 2;
	ret = ipa_register_intf(dev->name, &tx_properties, &rx_properties);
	if (ret) {
		pr_err("[%s] %s: ipa_register_intf failed %d\n", dev->name,
		       __func__, ret);
		return ret;
	}
	return 0;
}

static int __wwan_open(struct net_device *dev)
{
	int r;
	struct wwan_private *wwan_ptr = netdev_priv(dev);

	pr_debug("[%s] __wwan_open()\n", dev->name);
	if (wwan_ptr->device_status != WWAN_DEVICE_ACTIVE) {
		INIT_COMPLETION(wwan_ptr->resource_granted_completion);
		r = ipa_rm_inactivity_timer_request_resource(
			ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
		if (r < 0 && r != -EINPROGRESS) {
			pr_err("%s: ipa rm timer request resource failed %d\n",
					__func__, r);
			return -ENODEV;
		}
		if (r == -EINPROGRESS) {
			wait_for_completion(
				&wwan_ptr->resource_granted_completion);
		}
		r = a2_mux_open_channel(a2_mux_lcid_by_ch_id[wwan_ptr->ch_id],
					dev, a2_mux_notify);
		if (r < 0) {
			pr_err("%s: ch=%d failed with rc %d\n",
					__func__, wwan_ptr->ch_id, r);
			ipa_rm_inactivity_timer_release_resource(
				ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
			return -ENODEV;
		}
		ipa_rm_inactivity_timer_release_resource(
			ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
		r = wwan_register_to_ipa(dev);
		if (r < 0) {
			pr_err("%s: ch=%d failed to register to IPA rc %d\n",
					__func__, wwan_ptr->ch_id, r);
			return -ENODEV;
		}
	}
	wwan_ptr->device_status = WWAN_DEVICE_ACTIVE;
	return 0;
}

/**
 * wwan_open() - Opens the wwan network interface. Opens logical
 * channel on A2 MUX driver and starts the network stack queue
 *
 * @dev: network device
 *
 * Return codes:
 * 0: success
 * -ENODEV: Error while opening logical channel on A2 MUX driver
 */
static int wwan_open(struct net_device *dev)
{
	int rc = 0;

	pr_debug("[%s] wwan_open()\n", dev->name);
	rc = __wwan_open(dev);
	if (rc == 0)
		netif_start_queue(dev);
	return rc;
}


static int __wwan_close(struct net_device *dev)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);
	int rc = 0;

	if (wwan_ptr->device_status == WWAN_DEVICE_ACTIVE) {
		wwan_ptr->device_status = WWAN_DEVICE_INACTIVE;
		/* do not close wwan port once up,  this causes
			remote side to hang if tried to open again */
		INIT_COMPLETION(wwan_ptr->resource_granted_completion);
		rc = ipa_rm_inactivity_timer_request_resource(
			ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
		if (rc < 0 && rc != -EINPROGRESS) {
			pr_err("%s: ipa rm timer request resource failed %d\n",
					__func__, rc);
			return -ENODEV;
		}
		if (rc == -EINPROGRESS) {
			wait_for_completion(
				&wwan_ptr->resource_granted_completion);
		}
		rc = a2_mux_close_channel(
			a2_mux_lcid_by_ch_id[wwan_ptr->ch_id]);
		if (rc) {
			pr_err("[%s] %s: a2_mux_close_channel failed %d\n",
			       dev->name, __func__, rc);
			ipa_rm_inactivity_timer_release_resource(
				ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
			return rc;
		}
		ipa_rm_inactivity_timer_release_resource(
			ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
		rc = ipa_deregister_intf(dev->name);
		if (rc) {
			pr_err("[%s] %s: ipa_deregister_intf failed %d\n",
			       dev->name, __func__, rc);
			return rc;
		}
		return rc;
	} else
		return -EBADF;
}

/**
 * wwan_stop() - Stops the wwan network interface. Closes
 * logical channel on A2 MUX driver and stops the network stack
 * queue
 *
 * @dev: network device
 *
 * Return codes:
 * 0: success
 * -ENODEV: Error while opening logical channel on A2 MUX driver
 */
static int wwan_stop(struct net_device *dev)
{
	pr_debug("[%s] wwan_stop()\n", dev->name);
	__wwan_close(dev);
	netif_stop_queue(dev);
	return 0;
}

static int wwan_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || WWAN_DATA_LEN < new_mtu)
		return -EINVAL;
	pr_debug("[%s] MTU change: old=%d new=%d\n",
		dev->name, dev->mtu, new_mtu);
	dev->mtu = new_mtu;
	return 0;
}

/**
 * wwan_xmit() - Transmits an skb. In charge of asking IPA
 * RM needed resources. In case that IPA RM is not ready, then
 * the skb is saved for tranmitting as soon as IPA RM resources
 * are granted.
 *
 * @skb: skb to be transmitted
 * @dev: network device
 *
 * Return codes:
 * 0: success
 * NETDEV_TX_BUSY: Error while transmitting the skb. Try again
 * later
 * -EFAULT: Error while transmitting the skb
 */
static int wwan_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);
	unsigned long flags;
	int ret = 0;

	if (netif_queue_stopped(dev)) {
		pr_err("[%s]fatal: wwan_xmit called when netif_queue stopped\n",
		       dev->name);
		return 0;
	}
	ret = ipa_rm_inactivity_timer_request_resource(
		ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
	if (ret == -EINPROGRESS) {
		netif_stop_queue(dev);
		return NETDEV_TX_BUSY;
	}
	if (ret) {
		pr_err("[%s] fatal: ipa rm timer request resource failed %d\n",
		       dev->name, ret);
		return -EFAULT;
	}
	ret = wwan_send_packet(skb, dev);
	if (ret == -EPERM) {
		ret = NETDEV_TX_BUSY;
		goto exit;
	}
	/*
	 * detected SSR a bit early.  shut some things down now, and leave
	 * the rest to the main ssr handling code when that happens later
	 */
	if (ret == -EFAULT) {
		netif_carrier_off(dev);
		dev_kfree_skb_any(skb);
		ret = 0;
		goto exit;
	}
	if (ret == -EAGAIN) {
		/*
		 * This should not happen
		 * EAGAIN means we attempted to overflow the high watermark
		 * Clearly the queue is not stopped like it should be, so
		 * stop it and return BUSY to the TCP/IP framework.  It will
		 * retry this packet with the queue is restarted which happens
		 * in the write_done callback when the low watermark is hit.
		 */
		netif_stop_queue(dev);
		ret = NETDEV_TX_BUSY;
		goto exit;
	}
	spin_lock_irqsave(&wwan_ptr->lock, flags);
	if (a2_mux_is_ch_full(a2_mux_lcid_by_ch_id[wwan_ptr->ch_id])) {
		netif_stop_queue(dev);
		pr_debug("%s: High WM hit, stopping queue=%p\n",
		       __func__, skb);
	}
	spin_unlock_irqrestore(&wwan_ptr->lock, flags);
	return ret;
exit:
	ipa_rm_inactivity_timer_release_resource(
		ipa_rm_resource_by_ch_id[wwan_ptr->ch_id]);
	return ret;
}

static struct net_device_stats *wwan_get_stats(struct net_device *dev)
{
	struct wwan_private *wwan_ptr = netdev_priv(dev);
	return &wwan_ptr->stats;
}

static void wwan_tx_timeout(struct net_device *dev)
{
	pr_warning("[%s] wwan_tx_timeout(), data stall in UL\n", dev->name);
	ipa_bam_reg_dump();
}

/**
 * wwan_ioctl() - I/O control for wwan network driver.
 *
 * @dev: network device
 * @ifr: ignored
 * @cmd: cmd to be excecuded. can be one of the following:
 * WWAN_IOCTL_OPEN - Open the network interface
 * WWAN_IOCTL_CLOSE - Close the network interface
 *
 * Return codes:
 * 0: success
 * NETDEV_TX_BUSY: Error while transmitting the skb. Try again
 * later
 * -EFAULT: Error while transmitting the skb
 */
static int wwan_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int rc = 0;
	struct rmnet_ioctl_data_s ioctl_data;

	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_IP:        /* Set RAWIP protocol */
		break;
	case RMNET_IOCTL_GET_LLP:           /* Get link protocol state */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_SET_QOS_ENABLE:    /* Set QoS header enabled  */
		pr_debug("[%s] wwan_ioctl(): QOS header addition is not supported\n",
			dev->name);
		return -EPERM;
	case RMNET_IOCTL_SET_QOS_DISABLE:   /* Set QoS header disabled */
		break;
	case RMNET_IOCTL_FLOW_ENABLE:
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
			break;
		}
		tc_qdisc_flow_control(dev, ioctl_data.u.tcm_handle, 1);
		pr_debug("[%s] %s: enabled flow", dev->name, __func__);
		break;
	case RMNET_IOCTL_FLOW_DISABLE:
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
			break;
		}
		tc_qdisc_flow_control(dev, ioctl_data.u.tcm_handle, 0);
		pr_debug("[%s] %s: disabled flow", dev->name, __func__);
		break;
	case RMNET_IOCTL_GET_QOS:           /* Get QoS header state    */
		/* QoS disabled */
		ioctl_data.u.operation_mode = 0;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_GET_OPMODE:        /* Get operation mode      */
		ioctl_data.u.operation_mode = RMNET_MODE_LLP_IP;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;
	case RMNET_IOCTL_OPEN:  /* Open transport port */
		rc = __wwan_open(dev);
		pr_debug("[%s] wwan_ioctl(): open transport port\n",
		     dev->name);
		break;
	case RMNET_IOCTL_CLOSE:  /* Close transport port */
		rc = __wwan_close(dev);
		pr_debug("[%s] wwan_ioctl(): close transport port\n",
		     dev->name);
		break;
	default:
		pr_err("[%s] error: wwan_ioct called for unsupported cmd[%d]",
		       dev->name, cmd);
		return -EINVAL;
	}
	return rc;
}

static const struct net_device_ops wwan_ops_ip = {
	.ndo_open = wwan_open,
	.ndo_stop = wwan_stop,
	.ndo_start_xmit = wwan_xmit,
	.ndo_get_stats = wwan_get_stats,
	.ndo_tx_timeout = wwan_tx_timeout,
	.ndo_do_ioctl = wwan_ioctl,
	.ndo_change_mtu = wwan_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

/**
 * wwan_setup() - Setups the wwan network driver.
 *
 * @dev: network device
 *
 * Return codes:
 * None
 */
static void wwan_setup(struct net_device *dev)
{
	dev->netdev_ops = &wwan_ops_ip;
	ether_setup(dev);
	/* set this after calling ether_setup */
	dev->header_ops = 0;  /* No header */
	dev->type = ARPHRD_RAWIP;
	dev->hard_header_len = 0;
	dev->mtu = WWAN_DATA_LEN;
	dev->addr_len = 0;
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
	dev->needed_headroom = HEADROOM_FOR_A2_MUX;
	dev->needed_tailroom = TAILROOM;
	dev->watchdog_timeo = 30 * HZ;
}

/**
 * wwan_init() - Initialized the module and registers as a
 * network interface to the network stack
 *
 * Return codes:
 * 0: success
 * -ENOMEM: No memory available
 * -EFAULT: Internal error
 */
static int __init wwan_init(void)
{
	int ret;
	struct net_device *dev;
	struct wwan_private *wwan_ptr;
	unsigned n;
	struct ipa_rm_create_params ipa_rm_params;

	pr_info("%s: WWAN devices[%d]\n", __func__, WWAN_DEVICE_COUNT);
	for (n = 0; n < WWAN_DEVICE_COUNT; n++) {
		dev = alloc_netdev(sizeof(struct wwan_private),
				   WWAN_DEV_NAME, wwan_setup);
		if (!dev) {
			pr_err("%s: no memory for netdev %d\n", __func__, n);
			ret = -ENOMEM;
			goto fail;
		}
		netdevs[n] = dev;
		wwan_ptr = netdev_priv(dev);
		wwan_ptr->ch_id = n;
		spin_lock_init(&wwan_ptr->lock);
		init_completion(&wwan_ptr->resource_granted_completion);
		memset(&ipa_rm_params, 0, sizeof(struct ipa_rm_create_params));
		ipa_rm_params.name = ipa_rm_resource_by_ch_id[n];
		ipa_rm_params.reg_params.user_data = dev;
		ipa_rm_params.reg_params.notify_cb = ipa_rm_notify;
		ret = ipa_rm_create_resource(&ipa_rm_params);
		if (ret) {
			pr_err("%s: unable to create resourse %d in IPA RM\n",
			       __func__, ipa_rm_resource_by_ch_id[n]);
			goto fail;
		}
		ret = ipa_rm_inactivity_timer_init(ipa_rm_resource_by_ch_id[n],
						   IPA_RM_INACTIVITY_TIMER);
		if (ret) {
			pr_err("%s: ipa rm timer init failed %d on ins %d\n",
			       __func__, ret, n);
			goto fail;
		}
		ret = ipa_rm_add_dependency(ipa_rm_resource_by_ch_id[n],
					    IPA_RM_RESOURCE_A2_CONS);
		if (ret) {
			pr_err("%s: unable to add dependency %d rc=%d\n",
			       __func__, n, ret);
			goto fail;
		}
		ret = register_netdev(dev);
		if (ret) {
			pr_err("%s: unable to register netdev %d rc=%d\n",
			       __func__, n, ret);
			goto fail;
		}
	}
	return 0;
fail:
	for (n = 0; n < WWAN_DEVICE_COUNT; n++) {
		if (!netdevs[n])
			break;
		unregister_netdev(netdevs[n]);
		ipa_rm_inactivity_timer_destroy(ipa_rm_resource_by_ch_id[n]);
		free_netdev(netdevs[n]);
		netdevs[n] = NULL;
	}
	return ret;
}
late_initcall(wwan_init);

void wwan_cleanup(void)
{
	unsigned n;

	pr_info("%s: WWAN devices[%d]\n", __func__, WWAN_DEVICE_COUNT);
	for (n = 0; n < WWAN_DEVICE_COUNT; n++) {
		unregister_netdev(netdevs[n]);
		ipa_rm_inactivity_timer_destroy(ipa_rm_resource_by_ch_id[n]);
		free_netdev(netdevs[n]);
		netdevs[n] = NULL;
	}
}

MODULE_DESCRIPTION("WWAN Network Interface");
MODULE_LICENSE("GPL v2");
