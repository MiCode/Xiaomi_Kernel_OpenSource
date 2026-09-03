// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "sprd-seth: " fmt

#include <asm/byteorder.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/icmp.h>
#include <linux/icmpv6.h>
#include <linux/if_arp.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/sipc.h>
#include <linux/skbuff.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/tcp.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

/* Defination of sblock length, set to 1600 for non-zerocopy solution, */
/* otherwise, modem(wcdma) will assert */

#define SETH_BLOCK_SIZE	1600

/* Device status */
#define DEV_ON 1
#define DEV_OFF 0

/* Tx pkt return value */
#define SETH_TX_SUCCESS 0
#define SETH_TX_NO_BLK -1
#define SETH_TX_FAILED -2

#define SETH_NAPI_WEIGHT 64
#define SETH_TX_WEIGHT 16

#define SETH_NAME_SIZE 16

#define SMSG_EVENT_SBLOCK_SEND	0x1


/* The number of uids from userspace about vip data */
#define MAX_UID_NUM 10

#define SIOC_SETH_SET_UID (SIOCDEVPRIVATE + 0)
#define SIOC_SETH_DEL_UID (SIOCDEVPRIVATE + 1)

int seth_vip_uids[MAX_UID_NUM];

/* Struct of data transfer statistics */
struct seth_dtrans_stats {
	u32 rx_pkt_max;
	u32 rx_pkt_min;
	u32 rx_sum;
	u32 rx_cnt;
	u32 rx_alloc_fails;

	u32 tx_pkt_max;
	u32 tx_pkt_min;
	u32 tx_sum;
	u32 tx_cnt;
	u32 tx_ping_cnt;
	u32 tx_ack_cnt;
};

struct seth_init_data {
	char *name;
	u8 dst;
	u8 channel;
	u32 blocknum;
	u32 poolsize;
};

/* struct seth: device instance data for seth
 * @stats: net statistics
 * @netdev: linux net device
 * @pdata: platform data
 * @state: device state
 * @txstate: device txstate
 * @is_rawip : whether is rawip solution
 * @rx_busy: whether seth rx is busy
 * @rx_timer: timer for seth rx
 * @txpending: seth tx resend count
 * @tx_timer: timer for seth tx
 * @napi: napi instance
 * @dt_stats: record data_transfer statistics
 */
struct seth {
	struct net_device_stats stats;
	struct net_device *netdev;
	struct seth_init_data *pdata;
	int state;
	int txstate;
	int is_rawip;

	atomic_t rx_busy;
	struct timer_list rx_timer;

	atomic_t txpending;
	struct timer_list tx_timer;
	struct napi_struct napi;
	struct seth_dtrans_stats dt_stats;
};

/* For VIP data */
struct pdcp {
	bool priority;
	u8 discard_timer;
};

static u32 seth_vip_enable;

/* We decide disable GRO, since it alawys conflit with others */
static u32 gro_enable;
#ifdef CONFIG_DEBUG_FS
static struct dentry *root_gl;
static int seth_debugfs_mknod(void *root, void *data);
#endif
module_param(gro_enable, uint, 0644);
static void seth_rx_timer_handler(struct timer_list *t);

#ifdef CONFIG_DEBUG_FS
static int seth_debug_show(struct seq_file *m, void *v)
{
	struct seth *seth = (struct seth *)(m->private);
	struct seth_dtrans_stats *stats;
	struct seth_init_data *pdata;

	if (!seth)
		return -EINVAL;

	pdata = seth->pdata;
	stats = &seth->dt_stats;

	seq_puts(m, "******************************************************************\n");
	seq_printf(m, "DEVICE: %s, state %d, NET_SKB_PAD %u\n",
		   pdata->name,
		   seth->state,
		   NET_SKB_PAD);
	seq_puts(m, "\nRX statistics:\n");
	seq_printf(m, "rx_pkt_max=%u, rx_pkt_min=%u, rx_sum=%u, rx_cnt=%u\n",
		   stats->rx_pkt_max,
		   stats->rx_pkt_min,
		   stats->rx_sum,
		   stats->rx_cnt);
	seq_printf(m, "rx_alloc_fails=%u, rx_busy=%d\n",
		   stats->rx_alloc_fails,
		   atomic_read(&seth->rx_busy));

	seq_puts(m, "\nTX statistics:\n");
	seq_printf(m, "tx_pkt_max=%u, tx_pkt_min=%u, tx_sum=%u, tx_cnt=%u\n",
		   stats->tx_pkt_max,
		   stats->tx_pkt_min,
		   stats->tx_sum,
		   stats->tx_cnt);
	seq_printf(m, "tx_ping_cnt=%u, tx_ack_cnt=%u\n",
		   stats->tx_ping_cnt, stats->tx_ack_cnt);
	seq_puts(m, "******************************************************************\n");
	return 0;
}

static int seth_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, seth_debug_show, inode->i_private);
}

static const struct file_operations seth_debug_fops = {
	.open = seth_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int seth_debugfs_mknod(void *root, void *data)
{
	struct seth *seth = (struct seth *)data;
	struct seth_init_data *pdata;

	if (!seth)
		return -ENODEV;

	pdata = seth->pdata;

	if (!root_gl)
		return -ENXIO;

	debugfs_create_file(pdata->name, 0444,
			    (struct dentry *)root_gl,
			    data, &seth_debug_fops);

	return 0;
}

static int debugfs_gro_enable_get(void *data, u64 *val)
{
	*val = gro_enable;
	return 0;
}

static int debugfs_gro_enable_set(void *data, u64 val)
{
	gro_enable = (u32)val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_gro_enable,
			debugfs_gro_enable_get,
			debugfs_gro_enable_set,
			"%llu\n");
#endif /* CONFIG_DEBUG_FS */

static inline void seth_dt_stats_init(struct seth_dtrans_stats *stats)
{
	memset(stats, 0, sizeof(struct seth_dtrans_stats));
	stats->rx_pkt_min = 0xff;
	stats->tx_pkt_min = 0xff;
}

static inline void seth_rx_stats_update(struct seth_dtrans_stats *stats,
					u32 cnt)
{
	stats->rx_pkt_max = max(stats->rx_pkt_max, cnt);
	stats->rx_pkt_min = min(stats->rx_pkt_min, cnt);
	stats->rx_sum += cnt;
	stats->rx_cnt++;
}

static inline void seth_tx_stats_update(struct seth_dtrans_stats *stats,
					u32 cnt)
{
	stats->tx_pkt_max = max(stats->tx_pkt_max, cnt);
	stats->tx_pkt_min = min(stats->tx_pkt_min, cnt);
	stats->tx_sum += cnt;
	stats->tx_cnt++;
}

static inline void pkt_info_print(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);

	if (iph->version == 4)
		dev_dbg(NULL, "%pI4->%pI4, id=%d, len=%d\n",
			&iph->saddr, &iph->daddr, ntohs(iph->id),
			ntohs(iph->tot_len));
}

static void
seth_rx_prepare_skb(struct seth *seth, struct sk_buff *skb, struct sblock *blk)
{
	struct ethhdr *peth;
	struct iphdr *iph;

	if (seth->is_rawip) {
		skb_reserve(skb, NET_IP_ALIGN);
		skb_reset_mac_header(skb);
		peth = (struct ethhdr *)skb->data;
		skb_reserve(skb, ETH_HLEN);
		skb_reset_network_header(skb);
		unalign_memcpy(skb->data, blk->addr, blk->length);
		skb->dev = seth->netdev;
		iph = ip_hdr(skb);
		if (iph->version == 4)
			skb->protocol = htons(ETH_P_IP);
		else
			skb->protocol = htons(ETH_P_IPV6);

		/* Add an identical fake ethernet hdr
		 * to avoid out-of-order by GRO
		 */
		ether_addr_copy(peth->h_source, "000000");
		ether_addr_copy(peth->h_dest, "000001");
		peth->h_proto = skb->protocol;
		skb_put(skb, blk->length);
	} else {
		skb_reserve(skb, NET_IP_ALIGN);
		unalign_memcpy(skb->data, blk->addr, blk->length);
		skb_put(skb, blk->length);
		skb->protocol = eth_type_trans(skb, seth->netdev);
		skb_reset_network_header(skb);
	}
	skb->pkt_type = PACKET_HOST;
	skb->ip_summed = CHECKSUM_NONE;
}

static int seth_rx_poll_handler(struct napi_struct *napi, int budget)
{
	struct seth *seth = container_of(napi, struct seth, napi);
	struct sk_buff *skb;
	struct seth_init_data *pdata;
	struct sblock blk = {};
	struct seth_dtrans_stats *dt_stats;
	int skb_cnt, blk_ret, ret;

	if (!seth) {
		dev_err(NULL, "%s no seth device\n", __func__);
		return 0;
	}

	pdata = seth->pdata;
	dt_stats = &seth->dt_stats;
	blk_ret = 0;
	skb_cnt = 0;
	/* Keep polling, until the sblock rx ring is empty */
	while ((budget - skb_cnt) && !blk_ret) {
		blk_ret = SBLOCK_RECEIVE(pdata->dst, pdata->channel, &blk, 0);
		if (blk_ret) {
			dev_dbg(&seth->netdev->dev,
				"receive sblock error %d\n", blk_ret);
			continue;
		}
		if (seth->is_rawip)
			skb = dev_alloc_skb(blk.length
					+ ETH_HLEN + NET_IP_ALIGN);
		else
			skb = dev_alloc_skb(blk.length + NET_IP_ALIGN);
		if (!skb) {
			seth->stats.rx_dropped++;
			dev_err(&seth->netdev->dev, "failed to alloc skb!\n");
			ret = SBLOCK_RELEASE(pdata->dst, pdata->channel, &blk);
			if (ret)
				dev_err(&seth->netdev->dev,
					"release sblock failed %d\n", ret);
			dt_stats->rx_alloc_fails++;
			continue;
		}
		/* Prepare skb for IP layer */
		seth_rx_prepare_skb(seth, skb, &blk);
		/* Print debug info: ipid for v4 */
		pkt_info_print(skb);
		/* Release sblock */
		ret = SBLOCK_RELEASE(pdata->dst, pdata->channel, &blk);
		if (ret)
			dev_err(&seth->netdev->dev,
				"release sblock error %d\n", ret);
		/* Update fifo rd_ptr */
		seth->stats.rx_bytes += skb->len;
		seth->stats.rx_packets++;
		/* Send to IP layer */
		if (gro_enable)
			napi_gro_receive(napi, skb);
		else
			netif_receive_skb(skb);
		/* Update skb counter */
		skb_cnt++;
	}

	/* Update rx statistics */
	seth_rx_stats_update(dt_stats, skb_cnt);

	if (skb_cnt >= 0 && budget > skb_cnt) {
		napi_complete(napi);
		atomic_dec(&seth->rx_busy);

		/* To guarantee that any arrived sblock(s) can
		 * be processed even if there are no events issued by CP
		 */
		if (SBLOCK_GET_ARRIVED_COUNT(pdata->dst, pdata->channel) > 0) {
			/* Start a timer with 2 jiffies expries (20 ms) */
			mod_timer(&seth->rx_timer, jiffies + HZ / 50);
			dev_dbg(&seth->netdev->dev,
				"start rx_timer, jiffies %lu.\n", jiffies);
		}
	}
	return skb_cnt;
}

/* Tx_ready handler. */
static void seth_tx_ready_handler(void *data)
{
	struct seth *seth = (struct seth *)data;

	if (seth->state != DEV_ON) {
		seth->state = DEV_ON;
		seth->txstate = DEV_ON;
		if (!netif_carrier_ok(seth->netdev))
			netif_carrier_on(seth->netdev);
	} else {
		seth->state = DEV_OFF;
		seth->txstate = DEV_OFF;
		if (netif_carrier_ok(seth->netdev))
			netif_carrier_off(seth->netdev);
	}
}

/* Tx_open handler. */
static void seth_tx_open_handler(void *data)
{
	struct seth *seth = (struct seth *)data;

	seth->txstate = DEV_ON;
}

/* Tx_close handler. */
static void seth_tx_close_handler(void *data)
{
	struct seth *seth = (struct seth *)data;

	if (seth->state == DEV_OFF)
		return;
	seth->state = DEV_OFF;
	seth->txstate = DEV_OFF;
	if (netif_carrier_ok(seth->netdev))
		netif_carrier_off(seth->netdev);
}

static void seth_rx_handler(void *data)
{
	struct seth *seth = (struct seth *)data;

	if (!seth)
		return;

	if (seth->state != DEV_ON) {
		dev_err(&seth->netdev->dev, "dev is OFF, state=%d\n",
			seth->state);
		return;
	}

	/* If the poll handler has been done, trigger to schedule */
	if (!atomic_cmpxchg(&seth->rx_busy, 0, 1)) {
		/* Update rx stats */
		napi_schedule(&seth->napi);
		/* Trigger a NET_RX_SOFTIRQ softirq directly */
		//raise_softirq(NET_RX_SOFTIRQ);
	}
}

static void seth_rx_timer_handler(struct timer_list *t)
{
	struct seth *seth = from_timer(seth, t, rx_timer);

	seth_rx_handler((void *)seth);
}

/* Tx_close handler. */
static void seth_tx_pre_handler(void *data)
{
	struct seth *seth = (struct seth *)data;

	if (seth->txstate == DEV_ON)
		return;
	seth->txstate = DEV_ON;
	if (netif_queue_stopped(seth->netdev))
		netif_wake_queue(seth->netdev);
}

static void seth_handler(int event, void *data)
{
	struct seth *seth = (struct seth *)data;

	if (!seth)
		return;

	WARN_ON(!seth);

	switch (event) {
	case SBLOCK_NOTIFY_GET:
		seth_tx_pre_handler(seth);
		break;
	case SBLOCK_NOTIFY_RECV:
		del_timer(&seth->rx_timer);
		seth_rx_handler(seth);
		break;
	case SBLOCK_NOTIFY_STATUS:
		seth_tx_ready_handler(seth);
		break;
	case SBLOCK_NOTIFY_OPEN:
		seth_tx_open_handler(seth);
		break;
	case SBLOCK_NOTIFY_CLOSE:
		seth_tx_close_handler(seth);
		break;
	default:
		dev_err(&seth->netdev->dev,
			"Received event is invalid(event=%d)\n", event);
	}
}

static bool has_ethernet_header(struct sk_buff *skb)
{
	if (!skb) {
		return false;
	}

	if (skb_mac_header_was_set(skb) && (skb->len >= ETH_HLEN) && ((skb_network_header(skb) - skb_mac_header(skb)) >= ETH_HLEN)) {
		return true;
	}
	return false;
}

static int seth_tx_pkt(void *data, struct sk_buff *skb, int is_ack)
{
	struct sblock blk = {};
	struct seth *seth = netdev_priv(data);
	struct seth_init_data *pdata = seth->pdata;
	struct iphdr *iph;
	int seth_uid = 0;
	int ret, uid;
	struct pdcp *p;

	/* Get a free sblock. */
	ret = SBLOCK_GET(pdata->dst, pdata->channel, &blk, is_ack, 0);

	if (ret) {
		dev_err(&seth->netdev->dev,
			"Get free sblock failed(%d), drop data!\n", ret);
		seth->stats.tx_fifo_errors++;
		return SETH_TX_FAILED;
	}

	if (blk.length < skb->len) {
		dev_err(&seth->netdev->dev,
			"Sblock %d too small, skb %d\n",
			blk.length, skb->len);
		goto send_fail;
	}
	if (seth->is_rawip && has_ethernet_header(skb)) {
		skb_pull_inline(skb, ETH_HLEN);
	}
	blk.length = skb->len;
	unalign_memcpy(blk.addr, skb->data, skb->len);

	/* Struct pdcp must be initialized. */
	p = (struct pdcp *)((char *)blk.addr - sizeof(struct pdcp));
	memset(p, 0, sizeof(struct pdcp));

	/* For LAN forwarding to seth, sk is null. */
	if (skb->sk && seth_vip_enable) {
		seth_uid = skb->sk->sk_uid.val;
		dev_dbg(&seth->netdev->dev,
			"%s seth_uid is %d\n",
			__func__, seth_uid);

		/* For vip data we need to transmit skb firstly in band0. */
		for (uid = 0; uid < MAX_UID_NUM; uid++) {
			if (seth_uid == seth_vip_uids[uid]) {
				iph = ip_hdr(skb);

				dev_dbg(&seth->netdev->dev,
					"Vip skb %40ph ip.id 0x%x\n",
					skb->data, htons(iph->id));

				p->priority = true;
				/* PDCP discard timer come from
				 * cp value, cp doesn't care about
				 * how big discard_timer is, so
				 * set it to 0.
				 */
				p->discard_timer = 0;
				break;
			}
		}
	}

	/* Copy the content into smem and trigger a smsg to the peer side */
	if (seth->is_rawip)
		ret = SBLOCK_SEND_PREPARE(pdata->dst, pdata->channel, &blk);
	else
		ret = SBLOCK_SEND(pdata->dst, pdata->channel, &blk);
	if (ret < 0) {
		dev_err(&seth->netdev->dev,
			"Sblock_send fail, error %d\n", ret);
		goto send_fail;
	}

	/* Update the statistics */
	seth->stats.tx_bytes += skb->len;
	seth->stats.tx_packets++;

	dev_kfree_skb_any(skb);

	atomic_inc(&seth->txpending);
	return SETH_TX_SUCCESS;
send_fail:
	/* Recycle the unused sblock */
	SBLOCK_PUT(pdata->dst, pdata->channel, &blk);
	seth->stats.tx_fifo_errors++;
	netif_wake_queue(seth->netdev);

	return SETH_TX_FAILED;
}

static void seth_tx_flush(struct timer_list *t)
{
	int ret;
	u32 cnt;
	struct seth *seth = from_timer(seth, t, tx_timer);
	struct seth_init_data *pdata = seth->pdata;

	cnt = (u32)atomic_read(&seth->txpending);
	ret = SBLOCK_SEND_FINISH(pdata->dst, pdata->channel);
	seth_tx_stats_update(&seth->dt_stats, cnt);
	if (ret)
		dev_err(&seth->netdev->dev, "seth tx failed(%d)!\n", ret);
	else
		atomic_set(&seth->txpending, 0);
}

static int get_pkt_proto(struct sk_buff *skb)
{
	struct iphdr *iph = ip_hdr(skb);
	struct ipv6hdr *ipv6_hdr;

	if (iph->version == 4)
		return iph->protocol;

	ipv6_hdr = (struct ipv6hdr *)iph;
	return ipv6_hdr->nexthdr;
}

static struct tcphdr *get_pkt_tcphdr(struct sk_buff *skb)
{
	u8 iph_len;
	struct iphdr *iph = ip_hdr(skb);

	if (iph->version == 4)
		iph_len = iph->ihl * 4;
	else
		iph_len = sizeof(struct ipv6hdr);
	return (struct tcphdr *)((char *)iph + iph_len);
}

/* The orginal idea of pkt_is_ack() is to delay a pkt with data len > 0
 * for pkt aggregation purpose.
 * however, except syn/rst/fin, the ack flag is always 1.
 * So actually this function does not work at all.
 * Since it remains untested, and we do not want to take the risk,
 * we just keep it simple as below, that is we do not delay all tcp pkts.
 */

static bool pkt_need_nodelay(struct sk_buff *skb)
{
	u8 protocol = get_pkt_proto(skb);

	if (protocol == IPPROTO_ICMP || protocol == IPPROTO_ICMPV6 ||
	    protocol == IPPROTO_TCP)
		return true;
	return false;
}

static bool pkt_use_ackpool(struct sk_buff *skb)
{
	struct tcphdr *tcph;
	struct ipv6hdr *ip6h;
	struct iphdr *iph = ip_hdr(skb);
	u8 protocol = get_pkt_proto(skb);

	if (skb->len > SIPX_ACK_BLK_LEN)
		return false;

	/* TODO what if ipv6(TCP) nexthdr is not TCP */
	if (protocol == IPPROTO_TCP) {
		tcph = get_pkt_tcphdr(skb);

		/* we simply consider a tcp ack
		 * is a tcp pkt with data_len = 0
		 */
		if (iph->version == 4) {
			if ((ntohs(iph->tot_len) - iph->ihl * 4)
				== tcph->doff * 4)
				return true;
		} else if (iph->version == 6) {
			ip6h = (struct ipv6hdr *)iph;
			if (ntohs(ip6h->payload_len) == tcph->doff * 4)
				return true;
		}
	}
	return false;
}

/* Transmit interface */
static netdev_tx_t seth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct seth *seth = netdev_priv(dev);
	struct seth_dtrans_stats *dt_stats;
	int ret, blk_cnt;
	bool nodelay, ack_pool;

	if (seth->state != DEV_ON) {
		dev_err(&dev->dev, "xmit the state is off\n");
		netif_carrier_off(dev);
		seth->stats.tx_carrier_errors++;
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	/* Update tx statistics */
	dt_stats = &seth->dt_stats;
	nodelay = pkt_need_nodelay(skb);
	ack_pool = pkt_use_ackpool(skb);

	ret = seth_tx_pkt(dev, skb, ack_pool);
	if (ret == SETH_TX_NO_BLK) {
		/* if there are no available sblks, enter flow control */
		seth->txstate = DEV_OFF;
		netif_stop_queue(dev);

		/* anyway, flush the stored sblks */
		seth_tx_flush(&seth->tx_timer);
		return NETDEV_TX_BUSY;
	} else if (ret == SETH_TX_FAILED) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}
	/* If there are no available sblock for
	 * subsequent skb, start flow control
	 */
	blk_cnt = SBLOCK_GET_FREE_COUNT(seth->pdata->dst, seth->pdata->channel);
	if (!blk_cnt) {
		dev_dbg(&dev->dev, "start flow control\n");
		seth->txstate = DEV_OFF;
		netif_stop_queue(dev);
	}

	if ((atomic_read(&seth->txpending) >= SETH_TX_WEIGHT) || nodelay) {
		del_timer(&seth->tx_timer);
		seth_tx_flush(&seth->tx_timer);
		dev_dbg(&dev->dev, "%s:at once\n", __func__);
	} else if (atomic_read(&seth->txpending) == 1) {
		mod_timer(&seth->tx_timer, jiffies + HZ / 50);
		dev_dbg(&dev->dev, "%s:Timer\n", __func__);
	}
	return NETDEV_TX_OK;
}

/* Open interface */
static int seth_open(struct net_device *dev)
{
	struct seth *seth = netdev_priv(dev);
	struct seth_init_data *pdata;
	struct sblock blk = {};
	int ret = 0, num = 0;

	dev_dbg(&dev->dev, "open %s!\n", dev->name);

	if (!seth)
		return -ENODEV;

	pdata = seth->pdata;

	/* clean the resident sblocks */

	while (!ret && num < pdata->blocknum) {
		ret = SBLOCK_RECEIVE(pdata->dst, pdata->channel, &blk, 0);
		if (!ret) {
			SBLOCK_RELEASE(pdata->dst, pdata->channel, &blk);
			num++;
		}
	}
	dev_dbg(&dev->dev, "%s clean %d resident sblocks\n", __func__, num);

	/* Reset stats */
	memset(&seth->stats, 0, sizeof(seth->stats));

	if (!netif_carrier_ok(seth->netdev)) {
		dev_dbg(&dev->dev, "%s netif_carrier_on\n", __func__);
		netif_carrier_on(seth->netdev);
	}

	atomic_set(&seth->rx_busy, 0);
	napi_enable(&seth->napi);
	seth_dt_stats_init(&seth->dt_stats);
	seth->txstate = DEV_ON;
	seth->state = DEV_ON;
	netif_start_queue(dev);

	/* In case some pkts arrive before set DEV_ON */
	seth_rx_handler(seth);

	return 0;
}

/* Close interface */
static int seth_close(struct net_device *dev)
{
	struct seth *seth = netdev_priv(dev);

	dev_dbg(&dev->dev, "close %s!\n", dev->name);

	seth->txstate = DEV_OFF;
	seth->state = DEV_OFF;
	napi_disable(&seth->napi);
	netif_stop_queue(dev);

	return 0;
}

static struct net_device_stats *seth_get_stats(struct net_device *dev)
{
	struct seth *seth = netdev_priv(dev);

	return &seth->stats;
}

static void seth_tx_timeout(struct net_device *dev, unsigned int txqueue)
{
	struct seth *seth = netdev_priv(dev);

	if (seth->txstate != DEV_ON) {
		seth->txstate = DEV_ON;
		netif_wake_queue(dev);
	}
}

static int seth_ioctl(struct net_device *dev, struct ifreq *ifr, void __user *data, int cmd)
{
	int vip_uid;
	int i;
	int ret = 0;

	switch (cmd) {
	case SIOC_SETH_SET_UID:
		seth_vip_enable = true;

		ret = copy_from_user(&vip_uid, data, sizeof(vip_uid));

		dev_info(&dev->dev, "copy from userspace vip_uid is %d\n", vip_uid);

		if (ret)
			return -EFAULT;

		for (i = 0; i < MAX_UID_NUM; i++) {
			if (!seth_vip_uids[i]) {
				seth_vip_uids[i] = vip_uid;
				break;
			}
		}

		break;
	case SIOC_SETH_DEL_UID:
		dev_info(&dev->dev, "delete all the vip uids");

		seth_vip_enable = false;

		memset(seth_vip_uids, 0, sizeof(seth_vip_uids));

		break;
	default:
		dev_err(&dev->dev, "Ioctl cmd not support.\n");
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct net_device_ops seth_ops = {
	.ndo_open = seth_open,
	.ndo_stop = seth_close,
	.ndo_start_xmit = seth_start_xmit,
	.ndo_get_stats = seth_get_stats,
	.ndo_tx_timeout = seth_tx_timeout,
	.ndo_siocdevprivate = seth_ioctl,
};

static int seth_parse_dt(struct seth_init_data **init,
			 struct device_node *np, struct device *dev)
{
	struct device_node *parent_np;
	struct seth_init_data *pdata = NULL;
	int ret, dev_id;
	u32 data;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->name = devm_kzalloc(dev, SETH_NAME_SIZE, GFP_KERNEL);
	if (!pdata->name)
		return -ENOMEM;

	/* Get name */
	dev_id = of_alias_get_id(np, "seth");
	if (dev_id < 0) {
		dev_err(dev, "failed to get seth device id, ret= %d\n", dev_id);
		ret = dev_id;
		goto error;
	}
	snprintf(pdata->name, SETH_NAME_SIZE, "seth_lte%d", dev_id);

	/* Get the channel id for the node of spipe */
	ret = of_property_read_u32(np, "reg", (u32 *)&data);
	if (ret)
		goto error;
	pdata->channel = (u8)data;
	dev_dbg(dev, "channel =%d\n", pdata->channel);

	/* Get dst, the dst is share sipc dst
	 * Get the parent node
	 */
	parent_np = of_get_parent(np);
	if (parent_np) {
		ret = of_property_read_u32(parent_np, "reg", (u32 *)&data);
		if (ret)
			goto error;
		pdata->dst = (u8)data;
		dev_dbg(dev, "dst    =%d\n", pdata->dst);
	}
	of_node_put(parent_np);

	ret = of_property_read_u32(np, "sprd,blknum", &pdata->blocknum);
	if (ret)
		goto error;
	dev_dbg(dev, "sprd,blknum =%d\n", pdata->blocknum);
	*init = pdata;

	return 0;
error:
	return ret;
}

static void seth_setup(struct net_device *dev)
{
	ether_setup(dev);
	/* avoid mdns to be send */
	dev->flags &= ~(IFF_BROADCAST | IFF_MULTICAST);
}

static void seth_recv_handler(const struct smsg *msg, void *data)
{
	if (((msg->channel >= SMSG_CH_DATA0 && msg->channel <= SMSG_CH_DATA2) ||
	    (msg->channel >= SMSG_CH_DATA3 && msg->channel <= SMSG_CH_DATA5) ||
	    (msg->channel >= SMSG_CH_DATA6 && msg->channel <= SMSG_CH_DATA13)) &&
	     msg->type == SMSG_TYPE_EVENT && msg->flag == SMSG_EVENT_SBLOCK_SEND)
		seth_handler(SBLOCK_NOTIFY_RECV, data);
}

static ssize_t seth_vip_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", seth_vip_enable);
}

static ssize_t seth_vip_enable_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf,
				     size_t count)
{
	int flag, ret;

	ret = kstrtoint(buf, 10, &flag);
	if (ret)
		return ret;

	seth_vip_enable = flag;

	return count;
}
static DEVICE_ATTR_RW(seth_vip_enable);

static struct attribute *seth_ctrl_attrs[] = {
	&dev_attr_seth_vip_enable.attr,
	NULL,
};
ATTRIBUTE_GROUPS(seth_ctrl);

static int seth_probe(struct platform_device *pdev)
{
	struct seth_init_data *pdata = pdev->dev.platform_data;
	struct net_device *netdev;
	struct seth *seth;
	char ifname[IFNAMSIZ];
	int ret;
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;

	ret = seth_parse_dt(&pdata, np, &pdev->dev);
	if (ret) {
		dev_err(dev, "failed parse seth device tree, ret= %d\n", ret);
		return ret;
	}

	dev_dbg(dev, "after parse dt, name=%s, dst=%u, channel=%u, blocknum=%u\n",
		pdata->name, pdata->dst, pdata->channel, pdata->blocknum);

	if (pdata->name[0])
		strscpy(ifname, pdata->name, IFNAMSIZ);
	else
		strcpy(ifname, "veth%d");

	netdev = alloc_netdev(sizeof(struct seth), ifname,
			      NET_NAME_UNKNOWN, seth_setup);

	if (!netdev) {
		dev_err(dev, "alloc_netdev() failed.\n");
		return -ENOMEM;
	}
	/* Set the seth_lte net_device's type to ARPHRD_NONE here to meet the test
	 * requirements which need to change ipv6 address for each PDN connection,
	 * otherwise it uses eui64 suffix and the ipv6 address keeps the same.
	 */
	netdev->type = ARPHRD_NONE;
	netdev->flags |= IFF_NOARP;

	seth = netdev_priv(netdev);
	seth->pdata = pdata;
	seth->netdev = netdev;
	seth->state = DEV_OFF;
	seth->is_rawip = 1;

	atomic_set(&seth->rx_busy, 0);
	atomic_set(&seth->txpending, 0);

	timer_setup(&seth->rx_timer, seth_rx_timer_handler, 0);
	timer_setup(&seth->tx_timer, seth_tx_flush, 0);
	seth_dt_stats_init(&seth->dt_stats);
	netdev->netdev_ops = &seth_ops;
	netdev->watchdog_timeo = 1 * HZ;
	netdev->irq = 0;
	netdev->dma = 0;

	random_ether_addr(netdev->dev_addr);

	netif_napi_add(netdev, &seth->napi,
		       seth_rx_poll_handler, SETH_NAPI_WEIGHT);

	ret = SBLOCK_CREATE(pdata->dst, pdata->channel,
			    pdata->blocknum, SETH_BLOCK_SIZE, pdata->poolsize,
			    pdata->blocknum, SETH_BLOCK_SIZE, pdata->poolsize);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "create sblock failed (%d)\n", ret);
		goto out;
	}
	ret = SBLOCK_REGISTER_NOTIFIER(pdata->dst, pdata->channel,
				       seth_handler, seth);

	if (ret) {
		dev_err(dev, "regitster notifier failed (%d)\n", ret);
		goto out1;
	}
	smsg_callback_register(pdata->dst, pdata->channel, seth_recv_handler, seth);

	/* Register new Ethernet interface */
	ret = register_netdev(netdev);
	if (ret) {
		dev_err(dev, "register_netdev() failed (%d)\n", ret);
		goto out1;
	}

	/* Set link as disconnected */
	netif_carrier_off(netdev);

	platform_set_drvdata(pdev, seth);

	ret = sysfs_create_groups(&dev->kobj, seth_ctrl_groups);
	if (ret) {
		dev_err(dev, "failed to create sysfs group, ret %d\n", ret);
		goto out2;
	}

#ifdef CONFIG_DEBUG_FS
	if (!root_gl) {
		root_gl = debugfs_create_dir("seth", NULL);
		if (!root_gl) {
			ret = -ENOMEM;
			goto out3;
		}
	}
	debugfs_create_file("gro_enable", 0600,
			    root_gl, &gro_enable,
			    &fops_gro_enable);
	seth_debugfs_mknod(root_gl, (void *)seth);
#endif
	return 0;

#ifdef CONFIG_DEBUG_FS
out3:
	debugfs_remove_recursive(root_gl);
#endif
out2:
	sysfs_remove_groups(&dev->kobj, seth_ctrl_groups);
out1:
	SBLOCK_DESTROY(pdata->dst, pdata->channel);
out:
	netif_napi_del(&seth->napi);
	free_netdev(netdev);

	return ret;
}

/* Cleanup Ethernet device driver. */
static int seth_remove(struct platform_device *pdev)
{
	struct seth *seth = platform_get_drvdata(pdev);
	struct seth_init_data *pdata = seth->pdata;

	sysfs_remove_groups(&pdev->dev.kobj, seth_ctrl_groups);
	netif_napi_del(&seth->napi);
	del_timer_sync(&seth->rx_timer);
	del_timer_sync(&seth->tx_timer);
	SBLOCK_DESTROY(pdata->dst, pdata->channel);
	unregister_netdev(seth->netdev);
	free_netdev(seth->netdev);
	smsg_callback_unregister(pdata->dst, pdata->channel);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

static const struct of_device_id seth_match_table[] = {
	{ .compatible = "sprd,seth"},
	{ }
};

static struct platform_driver sprd_seth_driver = {
	.probe = seth_probe,
	.remove = seth_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "sprd-seth",
		.of_match_table = seth_match_table
	}
};

module_platform_driver(sprd_seth_driver);

MODULE_AUTHOR("Qiu Yi <qiu.yi@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum Ethernet device driver");
MODULE_LICENSE("GPL");


