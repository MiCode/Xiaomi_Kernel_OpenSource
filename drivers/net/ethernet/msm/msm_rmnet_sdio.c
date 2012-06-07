/* Copyright (c) 2010-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/*
 * RMNET SDIO Module.
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
#include <linux/if_arp.h>
#include <linux/msm_rmnet.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#include <mach/sdio_dmux.h>

/* Debug message support */
static int msm_rmnet_sdio_debug_mask;
module_param_named(debug_enable, msm_rmnet_sdio_debug_mask,
			int, S_IRUGO | S_IWUSR | S_IWGRP);

#define DEBUG_MASK_LVL0 (1U << 0)
#define DEBUG_MASK_LVL1 (1U << 1)
#define DEBUG_MASK_LVL2 (1U << 2)

#define DBG(m, x...) do {			   \
		if (msm_rmnet_sdio_debug_mask & m) \
			pr_info(x);		   \
} while (0)
#define DBG0(x...) DBG(DEBUG_MASK_LVL0, x)
#define DBG1(x...) DBG(DEBUG_MASK_LVL1, x)
#define DBG2(x...) DBG(DEBUG_MASK_LVL2, x)

/* Configure device instances */
#define RMNET_DEVICE_COUNT (8)

/* allow larger frames */
#define RMNET_DATA_LEN 2000

#define DEVICE_ID_INVALID   -1

#define DEVICE_INACTIVE      0
#define DEVICE_ACTIVE        1

#define HEADROOM_FOR_SDIO   8 /* for mux header */
#define HEADROOM_FOR_QOS    8
#define TAILROOM            8 /* for padding by mux layer */

struct rmnet_private {
	struct net_device_stats stats;
	uint32_t ch_id;
#ifdef CONFIG_MSM_RMNET_DEBUG
	ktime_t last_packet;
	unsigned long wakeups_xmit;
	unsigned long wakeups_rcv;
	unsigned long timeout_us;
#endif
	struct sk_buff *skb;
	spinlock_t lock;
	spinlock_t tx_queue_lock;
	struct tasklet_struct tsklt;
	u32 operation_mode; /* IOCTL specified mode (protocol, QoS header) */
	uint8_t device_up;
	uint8_t in_reset;
};

#ifdef CONFIG_MSM_RMNET_DEBUG
static unsigned long timeout_us;

#ifdef CONFIG_HAS_EARLYSUSPEND
/*
 * If early suspend is enabled then we specify two timeout values,
 * screen on (default), and screen is off.
 */
static unsigned long timeout_suspend_us;
static struct device *rmnet0;

/* Set timeout in us when the screen is off. */
static ssize_t timeout_suspend_store(struct device *d,
				     struct device_attribute *attr,
				     const char *buf, size_t n)
{
	timeout_suspend_us = strict_strtoul(buf, NULL, 10);
	return n;
}

static ssize_t timeout_suspend_show(struct device *d,
				    struct device_attribute *attr,
				    char *buf)
{
	return sprintf(buf, "%lu\n", (unsigned long) timeout_suspend_us);
}

static DEVICE_ATTR(timeout_suspend, 0664, timeout_suspend_show,
		   timeout_suspend_store);

static void rmnet_early_suspend(struct early_suspend *handler)
{
	if (rmnet0) {
		struct rmnet_private *p = netdev_priv(to_net_dev(rmnet0));
		p->timeout_us = timeout_suspend_us;
	}
}

static void rmnet_late_resume(struct early_suspend *handler)
{
	if (rmnet0) {
		struct rmnet_private *p = netdev_priv(to_net_dev(rmnet0));
		p->timeout_us = timeout_us;
	}
}

static struct early_suspend rmnet_power_suspend = {
	.suspend = rmnet_early_suspend,
	.resume = rmnet_late_resume,
};

static int __init rmnet_late_init(void)
{
	register_early_suspend(&rmnet_power_suspend);
	return 0;
}

late_initcall(rmnet_late_init);
#endif

/* Returns 1 if packet caused rmnet to wakeup, 0 otherwise. */
static int rmnet_cause_wakeup(struct rmnet_private *p)
{
	int ret = 0;
	ktime_t now;
	if (p->timeout_us == 0) /* Check if disabled */
		return 0;

	/* Use real (wall) time. */
	now = ktime_get_real();

	if (ktime_us_delta(now, p->last_packet) > p->timeout_us)
		ret = 1;

	p->last_packet = now;
	return ret;
}

static ssize_t wakeups_xmit_show(struct device *d,
				 struct device_attribute *attr,
				 char *buf)
{
	struct rmnet_private *p = netdev_priv(to_net_dev(d));
	return sprintf(buf, "%lu\n", p->wakeups_xmit);
}

DEVICE_ATTR(wakeups_xmit, 0444, wakeups_xmit_show, NULL);

static ssize_t wakeups_rcv_show(struct device *d, struct device_attribute *attr,
				char *buf)
{
	struct rmnet_private *p = netdev_priv(to_net_dev(d));
	return sprintf(buf, "%lu\n", p->wakeups_rcv);
}

DEVICE_ATTR(wakeups_rcv, 0444, wakeups_rcv_show, NULL);

/* Set timeout in us. */
static ssize_t timeout_store(struct device *d, struct device_attribute *attr,
			     const char *buf, size_t n)
{
#ifndef CONFIG_HAS_EARLYSUSPEND
	struct rmnet_private *p = netdev_priv(to_net_dev(d));
	p->timeout_us = timeout_us = strict_strtoul(buf, NULL, 10);
#else
/* If using early suspend/resume hooks do not write the value on store. */
	timeout_us = strict_strtoul(buf, NULL, 10);
#endif
	return n;
}

static ssize_t timeout_show(struct device *d, struct device_attribute *attr,
			    char *buf)
{
	struct rmnet_private *p = netdev_priv(to_net_dev(d));
	p = netdev_priv(to_net_dev(d));
	return sprintf(buf, "%lu\n", timeout_us);
}

DEVICE_ATTR(timeout, 0664, timeout_show, timeout_store);
#endif


/* Forward declaration */
static int rmnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

static __be16 rmnet_ip_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	__be16 protocol = 0;

	skb->dev = dev;

	/* Determine L3 protocol */
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
		/* skb will be dropped in upper layer for unknown protocol */
	}
	return protocol;
}

static int count_this_packet(void *_hdr, int len)
{
	struct ethhdr *hdr = _hdr;

	if (len >= ETH_HLEN && hdr->h_proto == htons(ETH_P_ARP))
		return 0;

	return 1;
}

static int sdio_update_reset_state(struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	int	new_state;

	new_state = msm_sdio_is_channel_in_reset(p->ch_id);

	if (p->in_reset != new_state) {
		p->in_reset = (uint8_t)new_state;

		if (p->in_reset)
			netif_carrier_off(dev);
		else
			netif_carrier_on(dev);
		return 1;
	}
	return 0;
}

/* Rx Callback, Called in Work Queue context */
static void sdio_recv_notify(void *dev, struct sk_buff *skb)
{
	struct rmnet_private *p = netdev_priv(dev);
	unsigned long flags;
	u32 opmode;

	if (skb) {
		skb->dev = dev;
		/* Handle Rx frame format */
		spin_lock_irqsave(&p->lock, flags);
		opmode = p->operation_mode;
		spin_unlock_irqrestore(&p->lock, flags);

		if (RMNET_IS_MODE_IP(opmode)) {
			/* Driver in IP mode */
			skb->protocol = rmnet_ip_type_trans(skb, dev);
		} else {
			/* Driver in Ethernet mode */
			skb->protocol = eth_type_trans(skb, dev);
		}
		if (RMNET_IS_MODE_IP(opmode) ||
		    count_this_packet(skb->data, skb->len)) {
#ifdef CONFIG_MSM_RMNET_DEBUG
			p->wakeups_rcv += rmnet_cause_wakeup(p);
#endif
			p->stats.rx_packets++;
			p->stats.rx_bytes += skb->len;
		}
		DBG1("[%s] Rx packet #%lu len=%d\n",
			((struct net_device *)dev)->name,
			p->stats.rx_packets, skb->len);

		/* Deliver to network stack */
		netif_rx(skb);
	} else {
		spin_lock_irqsave(&p->lock, flags);
		if (!sdio_update_reset_state((struct net_device *)dev))
			pr_err("[%s] %s: No skb received",
				((struct net_device *)dev)->name, __func__);
		spin_unlock_irqrestore(&p->lock, flags);
	}
}

static int _rmnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	int sdio_ret;
	struct QMI_QOS_HDR_S *qmih;
	u32 opmode;
	unsigned long flags;

	if (!netif_carrier_ok(dev)) {
		pr_err("[%s] %s: channel in reset",
			dev->name, __func__);
		goto xmit_out;
	}

	/* For QoS mode, prepend QMI header and assign flow ID from skb->mark */
	spin_lock_irqsave(&p->lock, flags);
	opmode = p->operation_mode;
	spin_unlock_irqrestore(&p->lock, flags);

	if (RMNET_IS_MODE_QOS(opmode)) {
		qmih = (struct QMI_QOS_HDR_S *)
			skb_push(skb, sizeof(struct QMI_QOS_HDR_S));
		qmih->version = 1;
		qmih->flags = 0;
		qmih->flow_id = skb->mark;
	}

	dev->trans_start = jiffies;
	sdio_ret = msm_sdio_dmux_write(p->ch_id, skb);

	if (sdio_ret != 0) {
		pr_err("[%s] %s: write returned error %d",
			dev->name, __func__, sdio_ret);
		goto xmit_out;
	}

	if (count_this_packet(skb->data, skb->len)) {
		p->stats.tx_packets++;
		p->stats.tx_bytes += skb->len;
#ifdef CONFIG_MSM_RMNET_DEBUG
		p->wakeups_xmit += rmnet_cause_wakeup(p);
#endif
	}
	DBG1("[%s] Tx packet #%lu len=%d mark=0x%x\n",
	    dev->name, p->stats.tx_packets, skb->len, skb->mark);

	return 0;
xmit_out:
	dev_kfree_skb_any(skb);
	p->stats.tx_errors++;
	return 0;
}

static void sdio_write_done(void *dev, struct sk_buff *skb)
{
	struct rmnet_private *p = netdev_priv(dev);
	unsigned long flags;

	if (skb)
		dev_kfree_skb_any(skb);

	if (!p->in_reset) {
		DBG1("%s: write complete skb=%p\n",	__func__, skb);

		spin_lock_irqsave(&p->tx_queue_lock, flags);
		if (netif_queue_stopped(dev) &&
				msm_sdio_dmux_is_ch_low(p->ch_id)) {
			DBG0("%s: Low WM hit, waking queue=%p\n",
					__func__, skb);
			netif_wake_queue(dev);
		}
		spin_unlock_irqrestore(&p->tx_queue_lock, flags);
	} else {
		DBG1("%s: write in reset skb=%p\n",	__func__, skb);
	}
}

static int __rmnet_open(struct net_device *dev)
{
	int r;
	struct rmnet_private *p = netdev_priv(dev);

	DBG0("[%s] __rmnet_open()\n", dev->name);

	if (!p->device_up) {
		r = msm_sdio_dmux_open(p->ch_id, dev,
				       sdio_recv_notify, sdio_write_done);

		if (r < 0)
			return -ENODEV;
	}

	p->device_up = DEVICE_ACTIVE;
	return 0;
}

static int rmnet_open(struct net_device *dev)
{
	int rc = 0;

	DBG0("[%s] rmnet_open()\n", dev->name);

	rc = __rmnet_open(dev);

	if (rc == 0)
		netif_start_queue(dev);

	return rc;
}


static int __rmnet_close(struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	int rc = 0;

	if (p->device_up) {
		/* do not close rmnet port once up,  this causes
		   remote side to hang if tried to open again */
		/* rc = msm_sdio_dmux_close(p->ch_id); */
		p->device_up = DEVICE_INACTIVE;
		return rc;
	} else
		return -EBADF;
}


static int rmnet_stop(struct net_device *dev)
{
	DBG0("[%s] rmnet_stop()\n", dev->name);

	__rmnet_close(dev);
	netif_stop_queue(dev);

	return 0;
}

static int rmnet_change_mtu(struct net_device *dev, int new_mtu)
{
	if (0 > new_mtu || RMNET_DATA_LEN < new_mtu)
		return -EINVAL;

	DBG0("[%s] MTU change: old=%d new=%d\n",
		dev->name, dev->mtu, new_mtu);
	dev->mtu = new_mtu;

	return 0;
}

static int rmnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	unsigned long flags;

	if (netif_queue_stopped(dev)) {
		pr_err("[%s]fatal: rmnet_xmit called when "
			"netif_queue is stopped", dev->name);
		return 0;
	}

	_rmnet_xmit(skb, dev);

	spin_lock_irqsave(&p->tx_queue_lock, flags);
	if (msm_sdio_dmux_is_ch_full(p->ch_id)) {
		netif_stop_queue(dev);
		DBG0("%s: High WM hit, stopping queue=%p\n",	__func__, skb);
	}
	spin_unlock_irqrestore(&p->tx_queue_lock, flags);

	return 0;
}

static struct net_device_stats *rmnet_get_stats(struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	return &p->stats;
}

static void rmnet_set_multicast_list(struct net_device *dev)
{
}

static void rmnet_tx_timeout(struct net_device *dev)
{
	pr_warning("[%s] rmnet_tx_timeout()\n", dev->name);
}

static const struct net_device_ops rmnet_ops_ether = {
	.ndo_open = rmnet_open,
	.ndo_stop = rmnet_stop,
	.ndo_start_xmit = rmnet_xmit,
	.ndo_get_stats = rmnet_get_stats,
	.ndo_set_rx_mode = rmnet_set_multicast_list,
	.ndo_tx_timeout = rmnet_tx_timeout,
	.ndo_do_ioctl = rmnet_ioctl,
	.ndo_change_mtu = rmnet_change_mtu,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_validate_addr = eth_validate_addr,
};

static const struct net_device_ops rmnet_ops_ip = {
	.ndo_open = rmnet_open,
	.ndo_stop = rmnet_stop,
	.ndo_start_xmit = rmnet_xmit,
	.ndo_get_stats = rmnet_get_stats,
	.ndo_set_rx_mode = rmnet_set_multicast_list,
	.ndo_tx_timeout = rmnet_tx_timeout,
	.ndo_do_ioctl = rmnet_ioctl,
	.ndo_change_mtu = rmnet_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

static int rmnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rmnet_private *p = netdev_priv(dev);
	u32 old_opmode = p->operation_mode;
	unsigned long flags;
	int prev_mtu = dev->mtu;
	int rc = 0;

	/* Process IOCTL command */
	switch (cmd) {
	case RMNET_IOCTL_SET_LLP_ETHERNET:  /* Set Ethernet protocol   */
		/* Perform Ethernet config only if in IP mode currently*/
		if (p->operation_mode & RMNET_MODE_LLP_IP) {
			ether_setup(dev);
			random_ether_addr(dev->dev_addr);
			dev->mtu = prev_mtu;

			dev->netdev_ops = &rmnet_ops_ether;
			spin_lock_irqsave(&p->lock, flags);
			p->operation_mode &= ~RMNET_MODE_LLP_IP;
			p->operation_mode |= RMNET_MODE_LLP_ETH;
			spin_unlock_irqrestore(&p->lock, flags);
			DBG0("[%s] rmnet_ioctl(): "
				"set Ethernet protocol mode\n",
				dev->name);
		}
		break;

	case RMNET_IOCTL_SET_LLP_IP:        /* Set RAWIP protocol      */
		/* Perform IP config only if in Ethernet mode currently*/
		if (p->operation_mode & RMNET_MODE_LLP_ETH) {

			/* Undo config done in ether_setup() */
			dev->header_ops         = 0;  /* No header */
			dev->type               = ARPHRD_RAWIP;
			dev->hard_header_len    = 0;
			dev->mtu                = prev_mtu;
			dev->addr_len           = 0;
			dev->flags              &= ~(IFF_BROADCAST|
						     IFF_MULTICAST);

			dev->needed_headroom = HEADROOM_FOR_SDIO +
			  HEADROOM_FOR_QOS;
			dev->needed_tailroom = TAILROOM;
			dev->netdev_ops = &rmnet_ops_ip;
			spin_lock_irqsave(&p->lock, flags);
			p->operation_mode &= ~RMNET_MODE_LLP_ETH;
			p->operation_mode |= RMNET_MODE_LLP_IP;
			spin_unlock_irqrestore(&p->lock, flags);
			DBG0("[%s] rmnet_ioctl(): "
				"set IP protocol mode\n",
				dev->name);
		}
		break;

	case RMNET_IOCTL_GET_LLP:           /* Get link protocol state */
		ifr->ifr_ifru.ifru_data =
			(void *)(p->operation_mode &
				 (RMNET_MODE_LLP_ETH|RMNET_MODE_LLP_IP));
		break;

	case RMNET_IOCTL_SET_QOS_ENABLE:    /* Set QoS header enabled  */
		spin_lock_irqsave(&p->lock, flags);
		p->operation_mode |= RMNET_MODE_QOS;
		spin_unlock_irqrestore(&p->lock, flags);
		DBG0("[%s] rmnet_ioctl(): set QMI QOS header enable\n",
			dev->name);
		break;

	case RMNET_IOCTL_SET_QOS_DISABLE:   /* Set QoS header disabled */
		spin_lock_irqsave(&p->lock, flags);
		p->operation_mode &= ~RMNET_MODE_QOS;
		spin_unlock_irqrestore(&p->lock, flags);
		DBG0("[%s] rmnet_ioctl(): set QMI QOS header disable\n",
			dev->name);
		break;

	case RMNET_IOCTL_GET_QOS:           /* Get QoS header state    */
		ifr->ifr_ifru.ifru_data =
			(void *)(p->operation_mode & RMNET_MODE_QOS);
		break;

	case RMNET_IOCTL_GET_OPMODE:        /* Get operation mode      */
		ifr->ifr_ifru.ifru_data = (void *)p->operation_mode;
		break;

	case RMNET_IOCTL_OPEN:              /* Open transport port     */
		rc = __rmnet_open(dev);
		DBG0("[%s] rmnet_ioctl(): open transport port\n",
			dev->name);
		break;

	case RMNET_IOCTL_CLOSE:             /* Close transport port    */
		rc = __rmnet_close(dev);
		DBG0("[%s] rmnet_ioctl(): close transport port\n",
			dev->name);
		break;

	default:
		pr_err("[%s] error: rmnet_ioct called for unsupported cmd[%d]",
			dev->name, cmd);
		return -EINVAL;
	}

	DBG2("[%s] %s: cmd=0x%x opmode old=0x%08x new=0x%08x\n",
		dev->name, __func__, cmd, old_opmode, p->operation_mode);
	return rc;
}

static void __init rmnet_setup(struct net_device *dev)
{
	/* Using Ethernet mode by default */
	dev->netdev_ops = &rmnet_ops_ether;
	ether_setup(dev);

	/* set this after calling ether_setup */
	dev->mtu = RMNET_DATA_LEN;
	dev->needed_headroom = HEADROOM_FOR_SDIO + HEADROOM_FOR_QOS ;
	dev->needed_tailroom = TAILROOM;
	random_ether_addr(dev->dev_addr);

	dev->watchdog_timeo = 1000; /* 10 seconds? */
}


static int __init rmnet_init(void)
{
	int ret;
	struct device *d;
	struct net_device *dev;
	struct rmnet_private *p;
	unsigned n;

	pr_info("%s: SDIO devices[%d]\n", __func__, RMNET_DEVICE_COUNT);

#ifdef CONFIG_MSM_RMNET_DEBUG
	timeout_us = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
	timeout_suspend_us = 0;
#endif
#endif

	for (n = 0; n < RMNET_DEVICE_COUNT; n++) {
		dev = alloc_netdev(sizeof(struct rmnet_private),
				   "rmnet_sdio%d", rmnet_setup);

		if (!dev)
			return -ENOMEM;

		d = &(dev->dev);
		p = netdev_priv(dev);
		/* Initial config uses Ethernet */
		p->operation_mode = RMNET_MODE_LLP_ETH;
		p->ch_id = n;
		spin_lock_init(&p->lock);
		spin_lock_init(&p->tx_queue_lock);
#ifdef CONFIG_MSM_RMNET_DEBUG
		p->timeout_us = timeout_us;
		p->wakeups_xmit = p->wakeups_rcv = 0;
#endif

		ret = register_netdev(dev);
		if (ret) {
			free_netdev(dev);
			return ret;
		}

#ifdef CONFIG_MSM_RMNET_DEBUG
		if (device_create_file(d, &dev_attr_timeout))
			continue;
		if (device_create_file(d, &dev_attr_wakeups_xmit))
			continue;
		if (device_create_file(d, &dev_attr_wakeups_rcv))
			continue;
#ifdef CONFIG_HAS_EARLYSUSPEND
		if (device_create_file(d, &dev_attr_timeout_suspend))
			continue;

		/* Only care about rmnet0 for suspend/resume tiemout hooks. */
		if (n == 0)
			rmnet0 = d;
#endif
#endif
	}
	return 0;
}

module_init(rmnet_init);
MODULE_DESCRIPTION("MSM RMNET SDIO TRANSPORT");
MODULE_LICENSE("GPL v2");

