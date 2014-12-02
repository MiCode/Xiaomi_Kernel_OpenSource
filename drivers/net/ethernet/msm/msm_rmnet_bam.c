/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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
 * RMNET BAM Module.
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
#include <linux/platform_device.h>
#include <net/pkt_sched.h>

#include <soc/qcom/bam_dmux.h>

/* Debug message support */
static int msm_rmnet_bam_debug_mask;
module_param_named(debug_enable, msm_rmnet_bam_debug_mask,
			int, S_IRUGO | S_IWUSR | S_IWGRP);

static unsigned long int msm_rmnet_bam_headroom_check_failure;
module_param(msm_rmnet_bam_headroom_check_failure, ulong, S_IRUGO);
MODULE_PARM_DESC(msm_rmnet_bam_headroom_check_failure,
		 "Number of packets with insufficient headroom");

#define DEBUG_MASK_LVL0 (1U << 0)
#define DEBUG_MASK_LVL1 (1U << 1)
#define DEBUG_MASK_LVL2 (1U << 2)

#define DBG(m, x...) do {			   \
		if (msm_rmnet_bam_debug_mask & m) \
			pr_info(x);		   \
} while (0)
#define DBG0(x...) DBG(DEBUG_MASK_LVL0, x)
#define DBG1(x...) DBG(DEBUG_MASK_LVL1, x)
#define DBG2(x...) DBG(DEBUG_MASK_LVL2, x)

/* allow larger frames */
#define RMNET_DATA_LEN 2000

#define RMNET_BAM_DRIVER_NAME "rmnet_bam"

#define DEVICE_ID_INVALID   -1

#define DEVICE_INACTIVE      2
#define DEVICE_ACTIVE        1
#define DEVICE_UNINITIALIZED 0

#define HEADROOM_FOR_BAM   8 /* for mux header */
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
	struct sk_buff *waiting_for_ul_skb;
	spinlock_t lock;
	spinlock_t tx_queue_lock;
	struct tasklet_struct tsklt;
	u32 operation_mode; /* IOCTL specified mode (protocol, QoS header) */
	uint8_t device_up;
	uint8_t in_reset;
};

#ifdef CONFIG_MSM_RMNET_DEBUG
static unsigned long timeout_us;

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
	return snprintf(buf, PAGE_SIZE, "%lu\n", p->wakeups_xmit);
}

DEVICE_ATTR(wakeups_xmit, 0444, wakeups_xmit_show, NULL);

static ssize_t wakeups_rcv_show(struct device *d, struct device_attribute *attr,
				char *buf)
{
	struct rmnet_private *p = netdev_priv(to_net_dev(d));
	return snprintf(buf, PAGE_SIZE, "%lu\n", p->wakeups_rcv);
}

DEVICE_ATTR(wakeups_rcv, 0444, wakeups_rcv_show, NULL);

/* Set timeout in us. */
static ssize_t timeout_store(struct device *d, struct device_attribute *attr,
			     const char *buf, size_t n)
{
	struct rmnet_private *p = netdev_priv(to_net_dev(d));
	p->timeout_us = timeout_us = strict_strtoul(buf, NULL, 10);
	return n;
}

static ssize_t timeout_show(struct device *d, struct device_attribute *attr,
			    char *buf)
{
	struct rmnet_private *p = netdev_priv(to_net_dev(d));
	p = netdev_priv(to_net_dev(d));
	return snprintf(buf, PAGE_SIZE, "%lu\n", timeout_us);
}

DEVICE_ATTR(timeout, 0664, timeout_show, timeout_store);
#endif


/* Forward declaration */
static int rmnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
static struct platform_driver bam_rmnet_drivers[BAM_DMUX_NUM_CHANNELS];

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
		protocol = htons(ETH_P_MAP);
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

/* Rx Callback, Called in Work Queue context */
static void bam_recv_notify(void *dev, struct sk_buff *skb)
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
	} else
		pr_err("[%s] %s: No skb received",
			((struct net_device *)dev)->name, __func__);
}

static struct sk_buff *_rmnet_add_headroom(struct sk_buff **skb,
					   struct net_device *dev)
{
	struct sk_buff *skbn;

	if (skb_headroom(*skb) < dev->needed_headroom) {
		msm_rmnet_bam_headroom_check_failure++;
		skbn = skb_realloc_headroom(*skb, dev->needed_headroom);
		kfree_skb(*skb);
		*skb = skbn;
	} else {
		skbn = *skb;
	}

	return skbn;
}

static int _rmnet_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	int bam_ret;
	struct QMI_QOS_HDR_S *qmih;
	u32 opmode;
	unsigned long flags;

	if (unlikely(!_rmnet_add_headroom(&skb, dev))) {
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
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
	/* if write() succeeds, skb access is unsafe in this process */
	bam_ret = msm_bam_dmux_write(p->ch_id, skb);

	if (bam_ret != 0 && bam_ret != -EAGAIN && bam_ret != -EFAULT) {
		pr_err("[%s] %s: write returned error %d",
			dev->name, __func__, bam_ret);
		if (RMNET_IS_MODE_QOS(opmode))
			skb_pull(skb, sizeof(struct QMI_QOS_HDR_S));
		return -EPERM;
	}

	return bam_ret;
}

static void bam_write_done(void *dev, struct sk_buff *skb)
{
	struct rmnet_private *p = netdev_priv(dev);
	u32 opmode = p->operation_mode;
	unsigned long flags;

	DBG1("%s: write complete\n", __func__);
	if (RMNET_IS_MODE_IP(opmode) ||
				count_this_packet(skb->data, skb->len)) {
		p->stats.tx_packets++;
		p->stats.tx_bytes += skb->len;
#ifdef CONFIG_MSM_RMNET_DEBUG
		p->wakeups_xmit += rmnet_cause_wakeup(p);
#endif
	}
	DBG1("[%s] Tx packet #%lu len=%d mark=0x%x\n",
	    ((struct net_device *)(dev))->name, p->stats.tx_packets,
	    skb->len, skb->mark);
	dev_kfree_skb_any(skb);

	spin_lock_irqsave(&p->tx_queue_lock, flags);
	if (netif_queue_stopped(dev) &&
	    msm_bam_dmux_is_ch_low(p->ch_id)) {
		DBG0("%s: Low WM hit, waking queue=%p\n",
		      __func__, skb);
		netif_wake_queue(dev);
	}
	spin_unlock_irqrestore(&p->tx_queue_lock, flags);
}

static void bam_notify(void *dev, int event, unsigned long data)
{
	struct rmnet_private *p = netdev_priv(dev);
	unsigned long flags;

	switch (event) {
	case BAM_DMUX_RECEIVE:
		bam_recv_notify(dev, (struct sk_buff *)(data));
		break;
	case BAM_DMUX_WRITE_DONE:
		bam_write_done(dev, (struct sk_buff *)(data));
		break;
	case BAM_DMUX_UL_CONNECTED:
		spin_lock_irqsave(&p->lock, flags);
		if (p->waiting_for_ul_skb != NULL) {
			struct sk_buff *skb;
			int ret;

			skb = p->waiting_for_ul_skb;
			p->waiting_for_ul_skb = NULL;
			spin_unlock_irqrestore(&p->lock, flags);
			ret = _rmnet_xmit(skb, dev);
			if (ret) {
				pr_err("%s: error %d dropping delayed TX SKB %p\n",
						__func__, ret, skb);
				dev_kfree_skb_any(skb);
			}
			netif_wake_queue(dev);
		} else {
			spin_unlock_irqrestore(&p->lock, flags);
		}
		break;
	case BAM_DMUX_UL_DISCONNECTED:
		break;
	}
}

static int __rmnet_open(struct net_device *dev)
{
	int r;
	struct rmnet_private *p = netdev_priv(dev);

	DBG0("[%s] __rmnet_open()\n", dev->name);

	if (p->device_up == DEVICE_UNINITIALIZED) {
		r = msm_bam_dmux_open(p->ch_id, dev, bam_notify);
		if (r < 0) {
			DBG0("%s: ch=%d failed with rc %d\n",
					__func__, p->ch_id, r);
			return -ENODEV;
		}
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

	if (p->device_up == DEVICE_ACTIVE) {
		/* do not close rmnet port once up,  this causes
		   remote side to hang if tried to open again */
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
	int awake;
	int ret = 0;

	if (netif_queue_stopped(dev)) {
		pr_err("[%s]fatal: rmnet_xmit called when "
			"netif_queue is stopped", dev->name);
		return 0;
	}

	spin_lock_irqsave(&p->lock, flags);
	awake = msm_bam_dmux_ul_power_vote();
	if (!awake) {
		/* send SKB once wakeup is complete */
		netif_stop_queue(dev);
		p->waiting_for_ul_skb = skb;
		spin_unlock_irqrestore(&p->lock, flags);
		ret = 0;
		goto exit;
	}
	spin_unlock_irqrestore(&p->lock, flags);

	ret = _rmnet_xmit(skb, dev);
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

	spin_lock_irqsave(&p->tx_queue_lock, flags);
	if (msm_bam_dmux_is_ch_full(p->ch_id)) {
		netif_stop_queue(dev);
		DBG0("%s: High WM hit, stopping queue=%p\n",    __func__, skb);
	}
	spin_unlock_irqrestore(&p->tx_queue_lock, flags);

exit:
	msm_bam_dmux_ul_power_unvote();
	return ret;
}

static struct net_device_stats *rmnet_get_stats(struct net_device *dev)
{
	struct rmnet_private *p = netdev_priv(dev);
	return &p->stats;
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
	.ndo_tx_timeout = rmnet_tx_timeout,
	.ndo_do_ioctl = rmnet_ioctl,
	.ndo_change_mtu = rmnet_change_mtu,
	.ndo_set_mac_address = 0,
	.ndo_validate_addr = 0,
};

static int rmnet_ioctl_extended(struct net_device *dev, struct ifreq *ifr)
{
	struct rmnet_ioctl_extended_s ext_cmd;
	int rc = 0;
	struct rmnet_private *p = netdev_priv(dev);


	rc = copy_from_user(&ext_cmd, ifr->ifr_ifru.ifru_data,
			    sizeof(ext_cmd));

	if (rc) {
		pr_err("%s: copy_from_user failed ,error %d", __func__, rc);
		return rc;
	}

	switch (ext_cmd.extended_ioctl) {
	case RMNET_IOCTL_SET_MRU:
		/* Transport MRU is fixed, so do nothing */
		break;
	case RMNET_IOCTL_GET_EPID:
		ext_cmd.u.data = p->ch_id;
		break;
	case RMNET_IOCTL_GET_SUPPORTED_FEATURES:
		ext_cmd.u.data = 0;
		break;
	case RMNET_IOCTL_GET_DRIVER_NAME:
		strlcpy(ext_cmd.u.if_name, RMNET_BAM_DRIVER_NAME,
			sizeof(ext_cmd.u.if_name));
		break;
	default:
		rc = -EINVAL;
		break;
	}

	rc = copy_to_user(ifr->ifr_ifru.ifru_data, &ext_cmd, sizeof(ext_cmd));

	if (rc)
		pr_err("%s: copy_to_user failed, error %d", __func__, rc);

	return rc;
}

static int rmnet_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct rmnet_private *p = netdev_priv(dev);
	u32 old_opmode = p->operation_mode;
	unsigned long flags;
	int prev_mtu = dev->mtu;
	int rc = 0;
	struct rmnet_ioctl_data_s ioctl_data;

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

			dev->needed_headroom = HEADROOM_FOR_BAM +
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
		ioctl_data.u.operation_mode = (p->operation_mode &
				 (RMNET_MODE_LLP_ETH|RMNET_MODE_LLP_IP));
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
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

	case RMNET_IOCTL_FLOW_ENABLE:
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
			break;
		}
		tc_qdisc_flow_control(dev, ioctl_data.u.tcm_handle, 1);
		DBG0("[%s] rmnet_ioctl(): enabled flow", dev->name);
		break;

	case RMNET_IOCTL_FLOW_DISABLE:
		if (copy_from_user(&ioctl_data, ifr->ifr_ifru.ifru_data,
			sizeof(struct rmnet_ioctl_data_s))) {
			rc = -EFAULT;
			break;
		}
		tc_qdisc_flow_control(dev, ioctl_data.u.tcm_handle, 0);
		DBG0("[%s] rmnet_ioctl(): disabled flow", dev->name);
		break;

	case RMNET_IOCTL_GET_QOS:           /* Get QoS header state    */
		ioctl_data.u.operation_mode = (p->operation_mode
						& RMNET_MODE_QOS);
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
		break;

	case RMNET_IOCTL_GET_OPMODE:        /* Get operation mode      */
		ioctl_data.u.operation_mode = p->operation_mode;
		if (copy_to_user(ifr->ifr_ifru.ifru_data, &ioctl_data,
			sizeof(struct rmnet_ioctl_data_s)))
			rc = -EFAULT;
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

	case RMNET_IOCTL_EXTENDED:          /* Extended IOCTL's        */
		rc = rmnet_ioctl_extended(dev, ifr);
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

static void rmnet_setup(struct net_device *dev)
{
	/* Using Ethernet mode by default */
	dev->netdev_ops = &rmnet_ops_ether;
	ether_setup(dev);

	/* set this after calling ether_setup */
	dev->mtu = RMNET_DATA_LEN;
	dev->needed_headroom = HEADROOM_FOR_BAM + HEADROOM_FOR_QOS ;
	dev->needed_tailroom = TAILROOM;
	random_ether_addr(dev->dev_addr);

	dev->watchdog_timeo = 1000; /* 10 seconds? */
}

static struct net_device *netdevs[BAM_DMUX_NUM_CHANNELS];

#ifdef CONFIG_MSM_RMNET_DEBUG
static int rmnet_debug_init(struct net_device *dev)
{

	struct device *d;
	struct rmnet_private *p;
	int err = 0;
	d = &(dev->dev);
	p = netdev_priv(dev);
	p->timeout_us = 0;
	p->wakeups_xmit = p->wakeups_rcv = 0;
	err = device_create_file(d, &dev_attr_timeout);
	if (err)
		return err;
	err = device_create_file(d, &dev_attr_wakeups_xmit);
	if (err)
		return err;
	err = device_create_file(d, &dev_attr_wakeups_rcv);
	return err;
}
#else
static int rmnet_debug_init(struct net_device *dev)
{
	return 0;
}
#endif

static int bam_rmnet_probe(struct platform_device *pdev)
{
	int i, ret;
	struct rmnet_private *p;
	struct device *d;
	char name[BAM_DMUX_CH_NAME_MAX_LEN];
	struct net_device *dev;
	const char *dev_name;

	for (i = 0; i < BAM_DMUX_NUM_CHANNELS; ++i) {
		scnprintf(name, BAM_DMUX_CH_NAME_MAX_LEN, "bam_dmux_ch_%d", i);
		if (!strcmp(pdev->name, name))
			break;
	}

	if (((i > BAM_DMUX_DATA_RMNET_7) && (i < BAM_DMUX_DATA_REV_RMNET_0)) ||
	    (i >= BAM_DMUX_NUM_CHANNELS)) {
		pr_err("%s: wrong netdev %s\n", __func__, pdev->name);
		return -ENODEV;
	}

	if (i <= BAM_DMUX_DATA_RMNET_7)
		dev_name = "rmnet%d";
	else
		dev_name = "rev_rmnet%d";

	dev = alloc_netdev(sizeof(*p), dev_name, rmnet_setup);
	if (!dev) {
		pr_err("%s: no memory for netdev %d\n", __func__, i);
		return -ENOMEM;
	}

	netdevs[i] = dev;
	d = &(dev->dev);
	p = netdev_priv(dev);
	/* Initial config uses Ethernet */
	p->operation_mode = RMNET_MODE_LLP_ETH;
	p->ch_id = i;
	p->waiting_for_ul_skb = NULL;
	p->device_up = DEVICE_UNINITIALIZED;
	spin_lock_init(&p->lock);
	spin_lock_init(&p->tx_queue_lock);

	ret = register_netdev(dev);
	if (ret) {
		pr_err("%s: unable to register netdev %d rc=%d\n",
			__func__, i, ret);
		netdevs[i] = NULL;
		free_netdev(dev);
		return ret;
	}

	rmnet_debug_init(dev);

	return 0;
}

static int bam_rmnet_remove(struct platform_device *pdev)
{
	int i;
	struct rmnet_private *p;
	char name[BAM_DMUX_CH_NAME_MAX_LEN];

	for (i = 0; i < BAM_DMUX_NUM_CHANNELS; ++i) {
		scnprintf(name, BAM_DMUX_CH_NAME_MAX_LEN, "bam_dmux_ch_%d", i);
		if (!strcmp(pdev->name, name))
			break;
	}

	if (((i > BAM_DMUX_DATA_RMNET_7) && (i < BAM_DMUX_DATA_REV_RMNET_0)) ||
	    (i >= BAM_DMUX_NUM_CHANNELS)) {
		pr_err("%s: wrong netdev %s\n", __func__, pdev->name);
		return -ENODEV;
	}

	p = netdev_priv(netdevs[i]);
	if (p->waiting_for_ul_skb != NULL) {
		dev_kfree_skb_any(p->waiting_for_ul_skb);
		p->waiting_for_ul_skb = NULL;
	}
	msm_bam_dmux_close(p->ch_id);
	netif_carrier_off(netdevs[i]);
	netif_stop_queue(netdevs[i]);

	unregister_netdev(netdevs[i]);
	free_netdev(netdevs[i]);

	return 0;
}

#ifdef CONFIG_MSM_RMNET_DEBUG
static void rmnet_clear_timeout_us(void)
{
	timeout_us = 0;
}
#else
static void rmnet_clear_timeout_us(void)
{
	return;
}
#endif /* CONFIG_MSM_RMNET_DEBUG */

static int __init rmnet_init(void)
{
	unsigned n;
	char *tempname;

	rmnet_clear_timeout_us();

	n = 0;
	while (n <= BAM_DMUX_DATA_REV_RMNET_8) {
		if ((n > BAM_DMUX_DATA_RMNET_7) &&
		    (n < BAM_DMUX_DATA_REV_RMNET_0)) {
			n++;
			continue;
		}
		bam_rmnet_drivers[n].probe = bam_rmnet_probe;
		bam_rmnet_drivers[n].remove = bam_rmnet_remove;
		tempname = kmalloc(BAM_DMUX_CH_NAME_MAX_LEN, GFP_KERNEL);
		if (tempname == NULL) {
			netdevs[n] = NULL;
			return -ENOMEM;
		}
		scnprintf(tempname, BAM_DMUX_CH_NAME_MAX_LEN, "bam_dmux_ch_%d",
			  n);
		bam_rmnet_drivers[n].driver.name = tempname;
		bam_rmnet_drivers[n].driver.owner = THIS_MODULE;
		platform_driver_register(&bam_rmnet_drivers[n]);
		n++;
	}

	return 0;
}

module_init(rmnet_init);
MODULE_DESCRIPTION("MSM RMNET BAM TRANSPORT");
MODULE_LICENSE("GPL v2");

