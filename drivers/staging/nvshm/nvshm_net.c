/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/device.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/semaphore.h>
#include "nvshm_types.h"
#include "nvshm_if.h"
#include "nvshm_priv.h"
#include "nvshm_iobuf.h"

#define MAX_XMIT_SIZE 1500

#define NVSHM_NETIF_PREFIX "wwan"

/* This structure holds the per network port information like
 * nvshm_iobuf queues, and the back reference to nvshm_channel */
struct nvshm_net_line {
	int use;
	int nvshm_chan;
	struct net_device_stats stats;

	/* iobuf queues for nvshm flow control support */
	struct nvshm_iobuf *q_head;
	struct nvshm_iobuf *q_tail;
	struct net_device *net;
	struct nvshm_channel *pchan; /* contains (struct net_device *)data */
	int errno;
	spinlock_t lock;
};

struct nvshm_net_device {
	struct nvshm_handle *handle;
	struct nvshm_net_line *line[NVSHM_MAX_CHANNELS];
	int nlines;
};

static struct nvshm_net_device netdev;


/* rx_event() is called when a packet of data is received.
 * The receiver should consume all iobuf in the given list.
 */
void nvshm_netif_rx_event(struct nvshm_channel *chan,
			   struct nvshm_iobuf *iobuf)
{
	struct net_device *dev = (struct net_device *)chan->data;
	struct nvshm_net_line *priv = netdev_priv(dev);
	struct nvshm_iobuf *bb_iob, *bb_next, *ap_iob, *ap_next;
	unsigned char *src;	 /* AP address for BB source buffer */
	unsigned char *dst;	 /* AP address for skb */
	unsigned int datagram_len;	/* datagram total data length */
	struct sk_buff *skb;

	pr_debug("%s()\n", __func__);
	if (!priv) {
		pr_err("%s() no private info on iface!\n", __func__);
		return;
	}

	if (!iobuf) {
		pr_err("%s() null input buffer address\n", __func__);
		return;
	}

	ap_next = iobuf;
	bb_next = NVSHM_A2B(netdev.handle, iobuf);
	while (bb_next) {
		datagram_len = 0;
		ap_iob = ap_next;
		bb_iob = bb_next;
		while (bb_iob) {
			datagram_len += ap_iob->length;
			bb_iob = ap_iob->sg_next;
			ap_iob = NVSHM_B2A(netdev.handle, bb_iob);
		}
		if (datagram_len > dev->mtu) {
			pr_err("%s: MTU %d>%d\n", __func__,
			       dev->mtu, datagram_len);
			priv->stats.rx_errors++;
			/* move to next datagram - drop current one */
			ap_iob = ap_next;
			bb_next = ap_next->next;
			ap_next = NVSHM_B2A(netdev.handle, bb_next);
			/* Break ->next chain before free */
			ap_iob->next = NULL;
			nvshm_iobuf_free_cluster(ap_iob);
			continue;
		}
		/* construct the skb */
		skb = (struct sk_buff *) __netdev_alloc_skb(dev,
							    datagram_len,
							    GFP_KERNEL);
		if (!skb) {
			/* Out of memory - nothing to do except */
			/* free current iobufs and return */
			pr_err("%s: skb alloc failed!\n", __func__);
			priv->stats.rx_errors++;
			nvshm_iobuf_free_cluster(ap_next);
			return;
		}
		dst = skb_put(skb, datagram_len);

		ap_iob = ap_next;
		bb_iob = bb_next;
		bb_next = ap_next->next;
		ap_next = NVSHM_B2A(netdev.handle, bb_next);
		while (bb_iob) {
			src = NVSHM_B2A(netdev.handle, ap_iob->npdu_data)
			      + ap_iob->data_offset;
			memcpy(dst, src, ap_iob->length);
			dst += ap_iob->length;
			bb_iob = ap_iob->sg_next;
			nvshm_iobuf_free(ap_iob);
			ap_iob = NVSHM_B2A(netdev.handle, bb_iob);
		}
		/* deliver skb to netif */
		skb->dev = dev;
		skb_reset_mac_header(skb);
		skb_reset_transport_header(skb);
		switch (skb->data[0] & 0xf0) {
		case 0x40:
			skb->protocol = htons(ETH_P_IP);
			break;
		case 0x60:
			skb->protocol = htons(ETH_P_IPV6);
			break;
		default:
			pr_err("%s() Non IP packet received!\n", __func__);
			priv->stats.rx_errors++;
			/* Drop packet */
			kfree_skb(skb);
			/* move to next datagram */
			continue;
		}
		skb->pkt_type = PACKET_HOST;
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		priv->stats.rx_packets++;
		priv->stats.rx_bytes += datagram_len;
		if (netif_rx(skb) == NET_RX_DROP)
			pr_debug("%s() : dropped packet\n", __func__);
	}
}

/* error_event() is called when an error event is received */
void nvshm_netif_error_event(struct nvshm_channel *chan,
	enum nvshm_error_id error)
{
	struct net_device *dev = (struct net_device *)chan->data;
	struct nvshm_net_line *priv = netdev_priv(dev);

	pr_debug("%s()\n", __func__);
	if (chan->data == NULL) {
		pr_err("%s() chan->data is null pointer\n", __func__);
		return;
	}

	priv->errno = error;

	spin_lock(&priv->lock);
	priv->stats.tx_errors++;
	spin_unlock(&priv->lock);
	pr_err("%s() : error on nvshm net interface!\n", __func__);
}

/* start_tx() is called to restart the transmit */
void nvshm_netif_start_tx(struct nvshm_channel *chan)
{
	struct net_device *dev = (struct net_device *)chan->data;
	struct nvshm_net_line *priv = netdev_priv(dev);

	pr_debug("%s()\n", __func__);

	if (!priv)
		return;

	/* Wake up queue */
	netif_wake_queue(dev);
}

static struct nvshm_if_operations nvshm_netif_ops = {
	.rx_event    = nvshm_netif_rx_event,	/* nvshm_queue.c */
	.error_event = nvshm_netif_error_event,	/* nvshm_iobuf.c */
	.start_tx    = nvshm_netif_start_tx,
};


/* called when ifconfig <if> up */
static int nvshm_netops_open(struct net_device *dev)
{
	struct nvshm_net_line *priv = netdev_priv(dev);
	int ret = 0;

	pr_debug("%s()\n", __func__);
	if (!priv)
		return -EINVAL;

	spin_lock(&priv->lock);
	if (!priv->use) {
		priv->pchan = nvshm_open_channel(priv->nvshm_chan,
			&nvshm_netif_ops,
			dev);
		if (priv->pchan == NULL)
			ret = -EINVAL;
	}
	if (!ret)
		priv->use++;
	spin_unlock(&priv->lock);

	/* Start if queue */
	netif_start_queue(dev);
	return ret;
}

/* called when ifconfig <if> down */
static int nvshm_netops_close(struct net_device *dev)
{
	struct nvshm_net_line *priv = netdev_priv(dev);

	pr_debug("%s()\n", __func__);
	if (!priv)
		return -EINVAL;

	spin_lock(&priv->lock);
	if (priv->use > 0)
		priv->use--;
	spin_unlock(&priv->lock);

	if (!priv->use) {
		/* Cleanup if data are still present in io queue */
		if (priv->q_head) {
			pr_debug("%s: still some data in queue!\n", __func__);
			nvshm_iobuf_free_cluster(
				(struct nvshm_iobuf *)priv->q_head);
			priv->q_head = priv->q_tail = NULL;
		}
		nvshm_close_channel(priv->pchan);
	}

	netif_stop_queue(dev);
	return 0;
}

static int nvshm_netops_xmit_frame(struct sk_buff *skb, struct net_device *dev)
{
	struct nvshm_net_line *priv = netdev_priv(dev);
	struct nvshm_iobuf *iob, *leaf = NULL, *list = NULL;
	int to_send = 0, remain;
	int len;
	char *data;

	pr_debug("Transmit frame\n");
	pr_debug("%s()\n", __func__);
	if (!priv)
		return -EINVAL;

	len = skb->len;
	data = skb->data;

	/* write a frame to an nvshm channel */
	pr_debug("len=%d\n", len);

	/* write data from skb (data,len) to net_device dev */
	remain = len;
	while (remain) {
		pr_debug("remain=%d\n", remain);
		to_send = remain < MAX_XMIT_SIZE ? remain : MAX_XMIT_SIZE;
		iob = nvshm_iobuf_alloc(priv->pchan, to_send);
		if (!iob) {
			pr_warn("%s iobuf alloc failed\n", __func__);
			netif_stop_queue(dev);
			if (list)
				nvshm_iobuf_free_cluster(list);
			return NETDEV_TX_BUSY; /* was return -ENOMEM; */
		}

		iob->length = to_send;
		remain -= to_send;

		memcpy(NVSHM_B2A(netdev.handle,
				 iob->npdu_data + iob->data_offset),
		       data,
		       to_send);

		data += to_send;

		if (!list) {
			leaf = list = iob;
		} else {
			leaf->sg_next = NVSHM_A2B(netdev.handle, iob);
			leaf = iob;
		}
	}
	if (nvshm_write(priv->pchan, list)) {
		/* no more transmit possible - stop queue */
		pr_warning("%s rate limit hit on channel %d\n",
			   __func__, priv->nvshm_chan);
		netif_stop_queue(dev);
		return NETDEV_TX_BUSY;
	}

	/* successfully written len data bytes */
	priv->stats.tx_packets++;
	priv->stats.tx_bytes += len;

	pr_debug("packets=%ld, tx_bytes=%ld\n", priv->stats.tx_packets,
			priv->stats.tx_bytes);

	/* free skb now as nvshm is a ref on it now */
	kfree_skb(skb);

	return NETDEV_TX_OK;
}

static int nvshm_netops_change_mtu(struct net_device *dev, int new_mtu)
{
	struct nvshm_net_line *priv = netdev_priv(dev);

	pr_debug("%s()\n", __func__);
	if (!priv)
		return -EINVAL;

	return 0;
}

struct net_device_stats *nvshm_netops_get_stats(struct net_device *dev)
{
	struct nvshm_net_line *priv = netdev_priv(dev);

	pr_debug("%s()\n", __func__);
	if (priv)
		return &priv->stats;
	else
		return NULL;
}

static void nvshm_netops_tx_timeout(struct net_device *dev)
{
	struct nvshm_net_line *priv = netdev_priv(dev);

	pr_debug("%s()\n", __func__);
	if (!priv)
		return;

	spin_lock(&priv->lock);
	priv->stats.tx_errors++;
	spin_unlock(&priv->lock);
	netif_wake_queue(dev);
}

static const struct net_device_ops nvshm_netdev_ops = {
	.ndo_open   = nvshm_netops_open,
	.ndo_stop   = nvshm_netops_close,
	.ndo_start_xmit = nvshm_netops_xmit_frame,
	.ndo_get_stats  = nvshm_netops_get_stats,
	.ndo_change_mtu = nvshm_netops_change_mtu,
	.ndo_tx_timeout = nvshm_netops_tx_timeout,
};

static void nvshm_nwif_init_dev(struct net_device *dev)
{
	dev->netdev_ops = &nvshm_netdev_ops;
	dev->mtu = 1500;
	dev->type = ARPHRD_NONE;
	/* No hardware address */
	dev->hard_header_len = 0;
	/* Should be tuned as soon as framer allow multiple frames */
	dev->tx_queue_len    = 10;
	/* No hardware address */
	dev->addr_len        = 0;
	dev->watchdog_timeo = HZ;
	dev->flags |= IFF_POINTOPOINT | IFF_NOARP;
}

int nvshm_net_init(struct nvshm_handle *handle)
{
	struct net_device *dev;
	int chan;
	int ret = 0;
	struct nvshm_net_line *priv;

	pr_debug("%s()\n", __func__);
	memset(&netdev, 0, sizeof(netdev));
	netdev.handle = handle;

	/* check nvshm_channels[] for net devices */
	for (chan = 0; chan < NVSHM_MAX_CHANNELS; chan++) {

		if (handle->chan[chan].map.type == NVSHM_CHAN_NET) {
			pr_debug("Registering %s%d\n",
				NVSHM_NETIF_PREFIX,
				netdev.nlines);
			dev =  alloc_netdev(sizeof(struct nvshm_net_line),
				NVSHM_NETIF_PREFIX"%d",
				nvshm_nwif_init_dev);
			if (!dev)
				goto err_exit;

			dev->base_addr = netdev.nlines;

			priv = netdev_priv(dev);
			netdev.line[netdev.nlines] = priv;
			netdev.line[netdev.nlines]->net = dev;
			priv->nvshm_chan = chan;
			spin_lock_init(&priv->lock);

			ret = register_netdev(dev);
			if (ret) {
				pr_err("Error %i registering %s%d device\n",
					ret,
					NVSHM_NETIF_PREFIX,
					netdev.nlines);
				goto err_exit;
			}

			netdev.nlines++;
		}
	}

	return ret;

err_exit:
	nvshm_net_cleanup();
	return ret;
}

void nvshm_net_cleanup(void)
{
	int chan;

	pr_debug("%s()\n", __func__);

	for (chan = 0; chan < netdev.nlines; chan++) {
		if (netdev.line[chan]->net) {
			pr_debug("%s free %s%d\n",
				__func__,
				NVSHM_NETIF_PREFIX,
				chan);
			unregister_netdev(netdev.line[chan]->net);
			free_netdev(netdev.line[chan]->net);
		}
	}
}

