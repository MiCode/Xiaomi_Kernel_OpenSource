/*
 * ip_over_tty_net.c
 *
 * Network driver for sending IP packets over tty devices.
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include "ip_over_tty.h"

#define skb_len(_skb) (_skb->len)

static int timeout = IPOTTY_TX_TIMEOUT;
module_param(timeout, int, 0);

/* TODO: support dynamic mtu? */
static int mtu = IPOTTY_MTU;
module_param(mtu, int, 0);

#ifdef VERBOSE_DEBUG
static
void net_hexdump(struct net_device *ndev, const unsigned char *data, int len)
{
#define DATA_PER_LINE	16
#define CHAR_PER_DATA	3 /* in "%02X " format */
	char linebuf[DATA_PER_LINE*CHAR_PER_DATA + 1];
	int i, dump_len;
	char *ptr = &linebuf[0];

	dump_len = len;
	if (dump_len > dumpsize)
		dump_len = dumpsize;

	BUG_ON(ndev == NULL);
	for (i = 0; i < dump_len; i++) {
		sprintf(ptr, " %02x ", data[i]);
		ptr += CHAR_PER_DATA;
		if (((i + 1) % DATA_PER_LINE) == 0) {
			*ptr = '\0';
			n_vdbg(ndev, "%s\n", linebuf);
			ptr = &linebuf[0];
		}
	}

	/* print the last line */
	if (ptr != &linebuf[0]) {
		*ptr = '\0';
		n_vdbg(ndev, "%s\n", linebuf);
	}

}
#else
#define net_hexdump(args...)
#endif

#ifdef IPOTTY_NET_HEADER
/*
 * called to make header checksum
 */
static inline void header_makesum(struct net_header *hdr)
{
	unsigned char *p;
	unsigned char sum = 0;
	int i;

	BUG_ON(hdr == NULL);

	p = (unsigned char *)hdr;
	for (i = 0; i < (IPOTTY_HLEN - 1); i++)
		sum ^= p[i];

	hdr->checksum = sum;
}

/*
 * called to verify header checksum
 */
static inline int header_checksum(struct net_header *hdr)
{
	unsigned char *p;
	unsigned char sum = 0;
	int i;

	BUG_ON(hdr == NULL);

	p = (unsigned char *)hdr;
	for (i = 0; i < (IPOTTY_HLEN - 1); i++)
		sum ^= p[i];

	return (sum == hdr->checksum);
}

static int
header_fill(struct ipotty_priv *priv, int to_size, const unsigned char *data, int count)
{
	int fill_size;
	unsigned char *p;

	BUG_ON(priv == NULL);
	BUG_ON(data == NULL);

	p = (unsigned char *) &priv->rxhdr;

	/* rxhdr has enough data */
	if (priv->rxhdr_len >= to_size)
		return 0;

	fill_size = to_size - priv->rxhdr_len;

	if (fill_size > count)
		fill_size = count;

	memcpy(&p[priv->rxhdr_len], data, fill_size);
	n_dbg(priv->ndev, "[%s] rxhdr_len=%d, fill_size=%d\n",
		__func__, priv->rxhdr_len, fill_size);

	net_hexdump(priv->ndev, p, to_size);

	priv->rxhdr_len += fill_size;

	return fill_size;
}

/*
 * called to check if [0xA5][0x5A] is in skb
 * reutrn: 1 for yes, 0 for no
 */
static inline int header_get_starter(struct ipotty_priv *priv)
{
	int len;
	int i;
	int m;
	unsigned char *p;

	BUG_ON(priv == NULL);

	len = priv->rxhdr_len;
	p = (unsigned char *) &priv->rxhdr;

	n_dbg(priv->ndev, "[%s] rxhdr_len=%d\n", __func__, priv->rxhdr_len);
	net_hexdump(priv->ndev, p, len);

	for (i = 0; i < (len - 1); i++) {
		if ((((int)p[i]) == 0xA5) && (((int)p[i + 1]) == 0x5A)) {
			/* discard garbage data */
			for (m = 0; m < (len - i); m++) {
				p[m] = p[i + m];
			}
			/* get [0xA5][0x5A] */
			return 1;
		}
		priv->rxhdr_len--;
	}

	if (((int)p[len]) == 0xA5) {
		/* keep [0xA5] */
		p[0] = 0xA5;
		priv->rxhdr_len = 1;
	} else
		priv->rxhdr_len--;

	return 0;
}

static int net_create_header(struct sk_buff *skb, struct net_device *ndev,
			unsigned short type, const void *daddr,
			const void *saddr, unsigned len)
{
	struct net_header *hdr = (struct net_header *)skb_push(skb, IPOTTY_HLEN);
	n_dbg(ndev, "[%s] type=%x, len=%d, ndev->hard_header_len=%d\n", __func__,
		type, len, ndev->hard_header_len);

	/* support only IP */
	BUG_ON(type != ETH_P_IP);
	hdr->marker[0] = 0xA5;
	hdr->marker[1] = 0x5A;
	hdr->protocol = htons(ETH_P_IP);
	hdr->size = htons(len + IPOTTY_HLEN);
	hdr->reserved = 0;
	header_makesum(hdr);

	return IPOTTY_HLEN;
}

static const struct header_ops ip_over_tty_headerops = {
	.create = net_create_header,
};

static int
skb_fill(struct sk_buff *skb, int to_size, const unsigned char *data, int count)
{
	int fill_size;
	BUG_ON(skb == NULL);
	BUG_ON(data == NULL);

	/* skb has enough data */
	if (skb_len(skb) >= to_size)
		return 0;

	fill_size = to_size - skb_len(skb);

	if (fill_size > count)
		fill_size = count;

	memcpy(skb_put(skb, fill_size), data, fill_size);
	return fill_size;
}
#endif

/*
 * called by register_netdev() to perform late stage initialization
 */
static int net_do_init(struct net_device *ndev)
{
	n_dbg(ndev, "[%s]\n", __func__);
	return 0;
}

static void net_do_uninit(struct net_device *ndev)
{
	n_dbg(ndev, "[%s]\n", __func__);
}

static int net_do_open(struct net_device *ndev)
{
	n_dbg(ndev, "[%s]\n", __func__);

	netif_start_queue(ndev);
	return 0;
}

static int net_do_stop(struct net_device *ndev)
{
	n_dbg(ndev, "[%s]\n", __func__);

	netif_stop_queue(ndev);
	return 0;
}

#ifdef IPOTTY_NET_HEADER
static netdev_tx_t do_xmit(struct ipotty_priv *priv)
{
	struct tty_struct *tty;
	struct sk_buff *skb;
	int tty_wr, len, room;
	struct sk_buff *tmp;
	int qlen;

	BUG_ON(priv == NULL);
	BUG_ON(priv->tty == NULL);

	/* atomic */
	if (test_and_set_bit(IPOTTY_SENDING, &priv->state))
		return NETDEV_TX_OK;

	tty = priv->tty;
	while ((skb = skb_peek(&priv->txhead)) != NULL) {

		/* in the middle of shutdown, cleanup queue */
		if (test_bit(IPOTTY_SHUTDOWN, &priv->state)) {
			tmp = skb_dequeue(&priv->txhead);
			BUG_ON(tmp != skb);
			dev_kfree_skb_any(skb);
			continue;
		}

		len = skb_len(skb);
		room = tty_write_room(tty);
		n_info(priv->ndev, "[%s] room=%d, skb->len=%d\n",
			__func__, room, len);

		qlen = skb_queue_len(&priv->txhead);
		if (qlen >= IPOTTY_SKB_HIGH) {
			/* in case driver has queue'd too many */
			if (!netif_queue_stopped(priv->ndev)) {
				n_info(priv->ndev,
					"stop tx, queue length=%d\n", qlen);
				netif_stop_queue(priv->ndev);
			}

		}

		if (qlen <= IPOTTY_SKB_LOW) {
			if (netif_queue_stopped(priv->ndev)) {
				n_info(priv->ndev,
					"wake tx, queue length=%d\n", qlen);
				netif_wake_queue(priv->ndev);
			}

		}

		if (!room) {
			if (!netif_queue_stopped(priv->ndev)) {
				/* try later */
				schedule_delayed_work(&priv->txwork,
					msecs_to_jiffies(tx_backoff));
			}
			/* no room, can't go through */
			break;
		}

		if (len > room) {
			len = room;
			n_warn(priv->ndev, "[%s] write partial packet to tty\n",
				__func__);
		}

		tty_wr = tty->ops->write(tty, skb->data, len);
		net_hexdump(priv->ndev, skb->data, len);

		/* TTY Error ?! */
		if (tty_wr < 0) {
			n_err(priv->ndev,
				"[%s] tty->ops->write() failed, err=%d\n",
				__func__, tty_wr);
			goto error;
		}

		priv->ndev->stats.tx_bytes += tty_wr;

		/* consume packet */
		skb_pull(skb, tty_wr);
		if (skb_len(skb) == 0) {
			tmp = skb_dequeue(&priv->txhead);
			BUG_ON(tmp != skb);
			dev_kfree_skb_any(skb);
			priv->ndev->trans_start = jiffies;
			priv->ndev->stats.tx_packets++;
		} else {
			/* packet uncompleted, try later */
			schedule_delayed_work(&priv->txwork,
				msecs_to_jiffies(tx_backoff));
		}
	}

	clear_bit(IPOTTY_SENDING, &priv->state);
	return NETDEV_TX_OK;

error:
	clear_bit(IPOTTY_SENDING, &priv->state);
	return tty_wr; /* TODO: can we return an error? */
}

#else
static netdev_tx_t do_xmit(struct ipotty_priv *priv)
{
	struct tty_struct *tty;
	struct sk_buff *skb;
	int tty_wr, len, room;
	struct sk_buff *tmp;
	int qlen;

	BUG_ON(priv == NULL);
	BUG_ON(priv->tty == NULL);

	/* atomic */
	if (test_and_set_bit(IPOTTY_SENDING, &priv->state))
		return NETDEV_TX_OK;

	tty = priv->tty;
	while ((skb = skb_peek(&priv->txhead)) != NULL) {

		/* in the middle of shutdown, cleanup queue */
		if (test_bit(IPOTTY_SHUTDOWN, &priv->state)) {
			tmp = skb_dequeue(&priv->txhead);
			BUG_ON(tmp != skb);
			dev_kfree_skb_any(skb);
			continue;
		}

		len = skb_len(skb);
		room = tty_write_room(tty);
		n_info(priv->ndev, "[%s] room=%d, skb->len=%d\n",
			__func__, room, len);

		qlen = skb_queue_len(&priv->txhead);
		if (qlen >= IPOTTY_SKB_HIGH) {
			/* in case driver has queue'd too many */
			if (!netif_queue_stopped(priv->ndev)) {
				n_info(priv->ndev,
					"stop tx, queue length=%d\n", qlen);
				netif_stop_queue(priv->ndev);
			}

		}

		if (qlen <= IPOTTY_SKB_LOW) {
			if (netif_queue_stopped(priv->ndev)) {
				n_info(priv->ndev,
					"wake tx, queue length=%d\n", qlen);
				netif_wake_queue(priv->ndev);
			}

		}

		if (len > room) {
			if (!netif_queue_stopped(priv->ndev)) {
				/* try later */
				schedule_delayed_work(&priv->txwork,
					msecs_to_jiffies(tx_backoff));
			}
			/* don't have enough room, can't go through */
			break;
		}

		tty_wr = tty->ops->write(tty, skb->data, len);
		net_hexdump(priv->ndev, skb->data, len);

		/* TTY Error ?! */
		if (tty_wr < 0) {
			n_err(priv->ndev,
				"[%s] tty->ops->write() failed, err=%d\n",
				__func__, tty_wr);
			goto error;
		}

		priv->ndev->stats.tx_bytes += tty_wr;

		/* consume packet */
		tmp = skb_dequeue(&priv->txhead);
		BUG_ON(tmp != skb);
		dev_kfree_skb_any(skb);
		priv->ndev->trans_start = jiffies;
		priv->ndev->stats.tx_packets++;
	}

	clear_bit(IPOTTY_SENDING, &priv->state);
	return NETDEV_TX_OK;

error:
	clear_bit(IPOTTY_SENDING, &priv->state);
	return tty_wr; /* TODO: can we return an error? */
}
#endif

static
netdev_tx_t net_do_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct ipotty_priv *priv = netdev_priv(ndev);

	skb_queue_tail(&priv->txhead, skb);
	schedule_delayed_work(&priv->txwork, 0);

	return NETDEV_TX_OK;
}

static void net_do_tx_timeout(struct net_device *ndev)
{
	struct ipotty_priv *priv = netdev_priv(ndev);

	n_warn(ndev, "[%s] tx timeout at %lu, latency=%lu\n", __func__,
		jiffies, jiffies - ndev->trans_start);

	schedule_delayed_work(&priv->txwork, 0);
}

static struct net_device_ops ip_over_tty_netops = {
	.ndo_init =		net_do_init,
	.ndo_uninit =		net_do_uninit,
	.ndo_open =		net_do_open,
	.ndo_stop =		net_do_stop,
	.ndo_start_xmit =	net_do_start_xmit,
	.ndo_tx_timeout = 	net_do_tx_timeout,

};

static void do_rx(struct ipotty_priv *priv)
{
	struct net_device *ndev;
	int len;
	int ret;
	struct sk_buff *skb;

	BUG_ON(priv == NULL);
	BUG_ON(priv->ndev == NULL);
	ndev = priv->ndev;

	/* atomic */
	if (test_and_set_bit(IPOTTY_RECEIVING, &priv->state))
		return;

	while ((skb = skb_dequeue(&priv->rxhead)) != NULL) {

		len = skb_len(skb);
		/* in the middle of shutdown, cleanup queue */
		if (test_bit(IPOTTY_SHUTDOWN, &priv->state)) {
			dev_kfree_skb_any(skb);
			continue;
		}
		n_info(ndev, "[%s] received %d bytes packet, protocol=0x%X\n",
			__func__, skb_len(skb), ntohs(skb->protocol));
		skb->dev = priv->ndev;
		skb->pkt_type = PACKET_HOST;
		skb->ip_summed = CHECKSUM_NONE; /* check it */

		/* dump packet */
		net_hexdump(ndev, skb->data, len);

		/* Push received packet up the stack. */
		ret = netif_rx_ni(skb);
		if (!ret) {
			priv->ndev->stats.rx_packets++;
			priv->ndev->stats.rx_bytes += len;
		} else
			priv->ndev->stats.rx_dropped++;
	}

	clear_bit(IPOTTY_RECEIVING, &priv->state);
}

static void rx_delayed_work(struct work_struct *work)
{
	struct ipotty_priv *priv =
		container_of(work, struct ipotty_priv, rxwork.work);

	BUG_ON(priv == NULL);
	do_rx(priv);
}

static void tx_delayed_work(struct work_struct *work)
{
	struct ipotty_priv *priv =
		container_of(work, struct ipotty_priv, txwork.work);

	BUG_ON(priv == NULL);
	do_xmit(priv);
}

/*
 * called by the networking layer (alloc_netdev())
 */
static void net_setup(struct net_device *ndev)
{
	struct ipotty_priv *priv = netdev_priv(ndev);

	memset(priv, 0, sizeof(struct ipotty_priv));
	priv->ndev = ndev;

	n_dbg(ndev, "[%s] timeout=%d\n", __func__, timeout);

	ndev->features = 0;
	ndev->type = ARPHRD_NONE;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP;
	ndev->netdev_ops = &ip_over_tty_netops;
	ndev->watchdog_timeo = timeout;
	ndev->mtu = mtu;
	ndev->tx_queue_len = 100;

#ifdef IPOTTY_NET_HEADER
	ndev->hard_header_len = IPOTTY_HLEN;
	ndev->header_ops = &ip_over_tty_headerops;
	priv->rxstate = HEADER_LOOKING;
	priv->rxskb = NULL;
#endif
	skb_queue_head_init(&priv->txhead);
	skb_queue_head_init(&priv->rxhead);
	INIT_DELAYED_WORK(&priv->txwork, tx_delayed_work);
	INIT_DELAYED_WORK(&priv->rxwork, rx_delayed_work);

}

#ifdef IPOTTY_NET_HEADER
/*
 * called by ldisc_receive() to handle a buffer from tty
 */
int
ipotty_net_receive(struct ipotty_priv *priv, const unsigned char *data, int count)
{
	int packet_size = 0;
	int consumed = 0;
	struct sk_buff *skb;
	int max_skb_size = (IPOTTY_HLEN + IPOTTY_MTU) + (16 - IPOTTY_HLEN);
	struct net_device *ndev;
	struct net_header *rxhdr;

	BUG_ON(priv == NULL);
	BUG_ON(priv->ndev == NULL);
	n_dbg(priv->ndev, "[%s] data count=%d\n", __func__, count);

	/* assign short names */
	ndev = priv->ndev;
	rxhdr = &priv->rxhdr;

	skb = priv->rxskb;
	if (!skb) {
		skb = netdev_alloc_skb(ndev, max_skb_size);
		if (!skb) {
			if (printk_ratelimit()) {
				n_warn(ndev, "[%s] netdev_alloc_skb() failed,"
					" packet dropped\n", __func__);
			}
			ndev->stats.rx_dropped++;
			/* still consumed */
			return count;
		}
		priv->rxskb = skb;
	}

	switch (priv->rxstate) {
		/* looking for a valid header */
	case HEADER_LOOKING:
		n_dbg(ndev, "[%s] HEADER_LOOKING, priv->rxhdr_len=%d\n",
			__func__, priv->rxhdr_len);

		if (priv->rxhdr_len < IPOTTY_HLEN)
			return header_fill(priv, IPOTTY_HLEN,
					&data[consumed], count - consumed);

		if (!header_get_starter(priv))
			return header_fill(priv, IPOTTY_HLEN,
					&data[consumed], count - consumed);

		/* get_header_starter() might eat few bytes */
		consumed += header_fill(priv, IPOTTY_HLEN,
					&data[consumed], count - consumed);

		if (priv->rxhdr_len < IPOTTY_HLEN)
			return consumed;

		priv->rxstate = HEADER_VALIDATING;
		/* falling through */

	case HEADER_VALIDATING:
		packet_size = ntohs(rxhdr->size);

		n_dbg(ndev, "[%s] HEADER_VALIDATING, packet_size=%d\n",
			__func__, packet_size);

		if (!header_checksum(rxhdr)) {
			/* checksum failed, discard rx header */
			n_warn(ndev, "[%s] header_checksum() failed,"
				" packet dropped\n", __func__);
			net_hexdump(ndev, (const unsigned char *)rxhdr,
				IPOTTY_HLEN);
			priv->rxhdr_len = 0;
			priv->rxstate = HEADER_LOOKING;
			return consumed;
		}

		if ((packet_size - IPOTTY_HLEN) > IPOTTY_MTU) {
			/* can't support, discard rx header */
			n_warn(ndev, "[%s] packet is bigger than MTU, "
				"dropped\n", __func__);
			net_hexdump(ndev, (const unsigned char *)rxhdr,
				IPOTTY_HLEN);
			priv->rxhdr_len = 0;
			priv->rxstate = HEADER_LOOKING;
			return consumed;
		}

		/* align 16B for IP */
		skb_reserve(skb, 16 - IPOTTY_HLEN);
		skb_reset_mac_header(skb);

		/* copy header */
		memcpy(skb_put(skb, IPOTTY_HLEN), rxhdr, IPOTTY_HLEN);
		priv->rxstate = PACKET_LOOKING;
		/* falling through */

	case PACKET_LOOKING:

		n_dbg(ndev, "[%s] PACKET_LOOKING, skb_len(skb)=%d\n",
			__func__, skb_len(skb));

		packet_size = ntohs(rxhdr->size);

		if (skb_len(skb) < packet_size) {
			consumed += skb_fill(skb, packet_size,
					&data[consumed], count - consumed);
		}

		/* still can't assemble a packet */
		if (skb_len(skb) < packet_size)
			return consumed;

		priv->rxstate = PACKET_DONE;
		/* falling through */

	case PACKET_DONE:
		n_dbg(ndev, "[%s] PACKET_DONE, skb_len(skb)=%d\n",
			__func__, skb_len(skb));

		packet_size = ntohs(rxhdr->size);
		skb->protocol = rxhdr->protocol;

		/* set skb->data to first octet of IP */
		skb_pull(skb, IPOTTY_HLEN);

		skb_queue_tail(&priv->rxhead, skb);

		schedule_delayed_work(&priv->rxwork, 0);

		/* reset state machine and packet header */
		priv->rxhdr_len = 0;
		priv->rxstate = HEADER_LOOKING;
		priv->rxskb = NULL;
		return consumed;

	default:
		n_err(priv->ndev, "[%s] unexpected rxstate=%d\n",
		__func__, priv->rxstate);
		BUG_ON(1);
	}
}
#else
/*
 * called by ldisc_receive() to handle a buffer from tty
 */
int
ipotty_net_receive(struct ipotty_priv *priv, const unsigned char *data, int count)
{
	struct sk_buff *skb = NULL;

	BUG_ON(priv == NULL);
	BUG_ON(priv->ndev == NULL);

	n_dbg(priv->ndev, "[%s] data count=%d\n", __func__, count);

	skb = netdev_alloc_skb(priv->ndev, count);
	if (!skb) {
		if (printk_ratelimit()) {
			n_warn(priv->ndev, "[%s] netdev_alloc_skb() failed,"
				" packet dropped\n", __func__);
		}
		priv->ndev->stats.rx_dropped++;
		/* still consumed packet */
		return count;
	}

	skb->protocol = htons(ETH_P_IP);
	memcpy(skb_put(skb, count), data, count);
	skb_queue_tail(&priv->rxhead, skb);

	schedule_delayed_work(&priv->rxwork, 0);

	return count;
}
#endif

/*
 * called by ldisc_write_wakeup
 */
void ipotty_net_wake_transmit(struct ipotty_priv *priv)
{

	BUG_ON(priv == NULL);
	BUG_ON(priv->ndev == NULL);

	n_info(priv->ndev, "[%s]\n");

	schedule_delayed_work(&priv->txwork, 0);
}

/*
 * called to create a network interface with given interface number
 */
struct ipotty_priv *ipotty_net_create_interface(char *intf_name)
{
	struct net_device *ndev = NULL;
	struct ipotty_priv *priv;
	int err;

	if (!intf_name)
		return NULL;
	INFO("[%s] intf_name=%s\n", __func__, intf_name);

	ndev = alloc_netdev(sizeof(struct ipotty_priv), intf_name, net_setup);
	if (!ndev) {
		ERROR("[%s] alloc_netdev() failed, intf_name=%s\n",
			__func__, intf_name);
		return NULL;
	}

	if ((err = register_netdev(ndev)) != 0) {
		ERROR("[%s] register_netdev() failed, intf_name=%s err=%d\n",
			__func__, intf_name, err);
		free_netdev(ndev);
		return NULL;
	}
	priv = netdev_priv(ndev);
	return priv;
}

/*
 * called to destroy a network interface.
 */
void ipotty_net_destroy_interface(struct ipotty_priv *priv)
{
	int sleep = 0;
	int sleep_unit = 5;
	BUG_ON(priv == NULL);
	BUG_ON(priv->ndev == NULL);

	n_info(priv->ndev, "[%s] queue length tx=%d, rx=%d\n", __func__,
		skb_queue_len(&priv->txhead), skb_queue_len(&priv->rxhead));

	/* clean up tx and rx queue, cease tx and rx worker */
	set_bit(IPOTTY_SHUTDOWN, &priv->state);
	while ((skb_queue_len(&priv->txhead) != 0) &&
		(skb_queue_len(&priv->rxhead) != 0)) {
		n_vdbg(priv->ndev, "[%s] sleep %d ms, tx=%d, rx=%d \n",
			__func__, sleep_unit,
			skb_queue_len(&priv->txhead),
			skb_queue_len(&priv->rxhead));
		msleep(sleep_unit);
		sleep++;
	}

	n_vdbg(priv->ndev, "[%s] takes %d ms to cleanup queue\n",
		__func__, sleep * sleep_unit);

	cancel_delayed_work(&priv->txwork);
	cancel_delayed_work(&priv->rxwork);

	unregister_netdev(priv->ndev);
	free_netdev(priv->ndev);
}

/*
 * called during module initialization.
 */
int ipotty_net_init(void)
{
	int err = 0;

	INFO("[%s]\n", __func__);

	return err;
}

/*
 * called during module de-initialization.
 */
void ipotty_net_remove(void)
{
	INFO("[%s]\n", __func__);
}
