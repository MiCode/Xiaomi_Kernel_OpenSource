/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   ccmni.c
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Cross Chip Modem Network Interface
 *
 * Author:
 * -------
 *   Anny.Hu(mtk80401)
 *
 ****************************************************************************/
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/sch_generic.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/version.h>
#include <linux/sockios.h>
#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/preempt.h>
#include "ccmni.h"
#include "ccci_debug.h"

#define IS_CCMNI_LAN(dev)    (strncmp(dev->name, "ccmni-lan", 9) == 0)

ccmni_ctl_block_t *ccmni_ctl_blk[MAX_MD_NUM];
unsigned int ccmni_debug_level;

/********************internal function*********************/
static void ccmni_make_etherframe(void *_eth_hdr, unsigned char *mac_addr, unsigned int packet_type)
{
	struct ethhdr *eth_hdr = _eth_hdr;

	memcpy(eth_hdr->h_dest, mac_addr, sizeof(eth_hdr->h_dest));
	memset(eth_hdr->h_source, 0, sizeof(eth_hdr->h_source));
	if (packet_type == 0x60)
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IPV6);
	else
		eth_hdr->h_proto = cpu_to_be16(ETH_P_IP);
}

static inline int is_ack_skb(int md_id, struct sk_buff *skb)
{
	u32 packet_type;
	struct tcphdr *tcph;
	int ret = 0;

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		struct ipv6hdr *iph = (struct ipv6hdr *)skb->data;
		u32 total_len = sizeof(struct ipv6hdr) + ntohs(iph->payload_len);

		if (total_len <= 128 - sizeof(struct ccci_header)) {
			u8 nexthdr = iph->nexthdr;
			__be16 frag_off;
			u32 l4_off = ipv6_skip_exthdr(skb, sizeof(struct ipv6hdr), &nexthdr, &frag_off);

			tcph = (struct tcphdr *)(skb->data + l4_off);
			if (nexthdr == IPPROTO_TCP && !tcph->syn && !tcph->fin &&
			!tcph->rst && ((total_len - l4_off) == (tcph->doff << 2)))
				ret = 1;

			if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_ACK_SKB)) {
				CCMNI_INF_MSG(md_id,
					"[SKB] ack=%d: proto=%d syn=%d fin=%d rst=%d ack=%d tot_len=%d l4_off=%d doff=%d\n",
					ret, nexthdr, tcph->syn, tcph->fin, tcph->rst,
					tcph->ack, total_len, l4_off, tcph->doff);
			}
		} else {
			if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_ACK_SKB))
				CCMNI_INF_MSG(md_id, "[SKB] ack=%d: tot_len=%d\n", ret, total_len);
		}
	} else if (packet_type == IPV4_VERSION) {
		struct iphdr *iph = (struct iphdr *)skb->data;

		if (ntohs(iph->tot_len) <= 128 - sizeof(struct ccci_header)) {
			tcph = (struct tcphdr *)(skb->data + (iph->ihl << 2));

			if (iph->protocol == IPPROTO_TCP && !tcph->syn && !tcph->fin &&
			!tcph->rst && (ntohs(iph->tot_len) == (iph->ihl << 2) + (tcph->doff << 2)))
				ret = 1;

			if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_ACK_SKB)) {
				CCMNI_INF_MSG(md_id,
					"[SKB] ack=%d: proto=%d syn=%d fin=%d rst=%d ack=%d tot_len=%d ihl=%d doff=%d\n",
					ret, iph->protocol, tcph->syn, tcph->fin, tcph->rst,
					tcph->ack, ntohs(iph->tot_len), iph->ihl, tcph->doff);
			}
		} else {
			if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_ACK_SKB))
				CCMNI_INF_MSG(md_id, "[SKB] ack=%d: tot_len=%d\n", ret, ntohs(iph->tot_len));
		}
	}

	return ret;
}


/********************internal debug function*********************/
#if 1
#if 0
static void ccmni_dbg_skb_addr(int md_id, bool tx, struct sk_buff *skb, int idx)
{
	CCMNI_INF_MSG(md_id, "[SKB][%s] idx=%d addr=%p len=%d data_len=%d, L2_addr=%p L3_addr=%p L4_addr=%p\n",
		tx?"TX":"RX", idx,
		(void *)skb->data, skb->len, skb->data_len, (void *)skb_mac_header(skb),
		(void *)skb_network_header(skb), (void *)skb_transport_header(skb));
}
#endif

static void ccmni_dbg_eth_header(int md_id, bool tx, struct ethhdr *ethh)
{
	if (ethh != NULL) {
		CCMNI_INF_MSG(md_id,
			"[SKB][%s] ethhdr: proto=0x%04x dest_mac=%02x:%02x:%02x:%02x:%02x:%02x src_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
			tx?"TX":"RX", ethh->h_proto, ethh->h_dest[0], ethh->h_dest[1], ethh->h_dest[2],
			ethh->h_dest[3], ethh->h_dest[4], ethh->h_dest[5], ethh->h_source[0], ethh->h_source[1],
			ethh->h_source[2], ethh->h_source[3], ethh->h_source[4], ethh->h_source[5]);
	}
}

static void ccmni_dbg_ip_header(int md_id, bool tx, struct iphdr *iph)
{
	if (iph != NULL) {
		CCMNI_INF_MSG(md_id,
			"[SKB][%s] iphdr: ihl=0x%02x ver=0x%02x tos=0x%02x tot_len=0x%04x id=0x%04x frag_off=0x%04x ttl=0x%02x proto=0x%02x check=0x%04x saddr=0x%08x daddr=0x%08x\n",
			tx?"TX":"RX", iph->ihl, iph->version, iph->tos, iph->tot_len, iph->id,
			iph->frag_off, iph->ttl, iph->protocol, iph->check, iph->saddr, iph->daddr);
	}
}

static void ccmni_dbg_tcp_header(int md_id, bool tx, struct tcphdr *tcph)
{
	if (tcph != NULL) {
		CCMNI_INF_MSG(md_id,
			"[SKB][%s] tcp_hdr: src=0x%04x dest=0x%04x seq=0x%08x ack_seq=0x%08x urg=%d ack=%d psh=%d rst=%d syn=%d fin=%d\n",
			tx?"TX":"RX", ntohl(tcph->source), ntohl(tcph->dest), tcph->seq, tcph->ack_seq,
			tcph->urg, tcph->ack, tcph->psh, tcph->rst, tcph->syn, tcph->fin);
	}
}

static void ccmni_dbg_skb_header(int md_id, bool tx, struct sk_buff *skb)
{
	struct ethhdr *ethh = NULL;
	struct iphdr  *iph = NULL;
	struct ipv6hdr  *ipv6h = NULL;
	struct tcphdr  *tcph = NULL;
	u8 nexthdr;
	__be16 frag_off;
	u32 l4_off;

	if (!tx) {
		ethh = (struct ethhdr *)(skb->data-ETH_HLEN);
		ccmni_dbg_eth_header(md_id, tx, ethh);
	}

	if (skb->protocol == htons(ETH_P_IP)) {
		iph = (struct iphdr *)skb->data;
		ccmni_dbg_ip_header(md_id, tx, iph);

		if (iph->protocol == IPPROTO_TCP) {
			tcph = (struct tcphdr *)(skb->data + (iph->ihl << 2));
			ccmni_dbg_tcp_header(md_id, tx, tcph);
		}
	} else if (skb->protocol == htons(ETH_P_IPV6)) {
		ipv6h = (struct ipv6hdr *)skb->data;

		nexthdr = ipv6h->nexthdr;
		if (nexthdr == IPPROTO_TCP) {
			l4_off = ipv6_skip_exthdr(skb, sizeof(struct ipv6hdr), &nexthdr, &frag_off);
			tcph = (struct tcphdr *)(skb->data + l4_off);
			ccmni_dbg_tcp_header(md_id, tx, tcph);
		}
	}
}
#endif

/* ccmni debug sys file create */
int ccmni_debug_file_init(int md_id)
{
	int result = -1;
	char fname[16];
	struct dentry *dentry1, *dentry2, *dentry3;

	CCMNI_INF_MSG(md_id, "ccmni_debug_file_init\n");

	dentry1 = debugfs_create_dir("ccmni", NULL);
	if (!dentry1) {
		CCMNI_ERR_MSG(md_id, "create /proc/ccmni fail\n");
		return -ENOENT;
	}

	snprintf(fname, 16, "md%d", (md_id+1));

	dentry2 = debugfs_create_dir(fname, dentry1);
	if (!dentry2) {
		CCMNI_ERR_MSG(md_id, "create /proc/ccmni/md%d fail\n", (md_id+1));
		return -ENOENT;
	}

	dentry3 = debugfs_create_u32("debug_level", 0600, dentry2, &ccmni_debug_level);
	result = PTR_ERR(dentry3);
	if (IS_ERR(dentry3) && result != -ENODEV) {
		CCMNI_ERR_MSG(md_id, "create /proc/ccmni/md%d/debug_level fail: %d\n", md_id, result);
		return -ENOENT;
	}

	return 0;
}

/********************netdev register function********************/
static u16 ccmni_select_queue(struct net_device *dev, struct sk_buff *skb,
			    void *accel_priv, select_queue_fallback_t fallback)
{
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(dev);
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[ccmni->md_id];

	if (ctlb->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD) {
		if (ccmni->ch.multiq && is_ack_skb(ccmni->md_id, skb))
			return CCMNI_TXQ_FAST;
		else
			return CCMNI_TXQ_NORMAL;
	} else
		return CCMNI_TXQ_NORMAL;
}

static int ccmni_open(struct net_device *dev)
{
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(dev);
	ccmni_ctl_block_t *ccmni_ctl = ccmni_ctl_blk[ccmni->md_id];
	ccmni_instance_t *ccmni_tmp = NULL;

	if (unlikely(ccmni_ctl == NULL)) {
		CCMNI_ERR_MSG(ccmni->md_id, "%s_Open: MD%d ctlb is NULL\n", dev->name, ccmni->md_id);
		return -1;
	}

	netif_carrier_on(dev);

	netif_tx_start_all_queues(dev);

	if (unlikely(ccmni_ctl->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
		napi_enable(ccmni->napi);
		napi_schedule(ccmni->napi);
	}

	atomic_inc(&ccmni->usage);
	ccmni_tmp = ccmni_ctl->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp)
		atomic_inc(&ccmni_tmp->usage);

	CCMNI_INF_MSG(ccmni->md_id, "%s_Open: cnt=(%d,%d), md_ab=0x%X\n",
		dev->name, atomic_read(&ccmni->usage),
		atomic_read(&ccmni_tmp->usage), ccmni_ctl->ccci_ops->md_ability);
	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(dev);
	ccmni_ctl_block_t *ccmni_ctl = ccmni_ctl_blk[ccmni->md_id];
	ccmni_instance_t *ccmni_tmp = NULL;

	if (unlikely(ccmni_ctl == NULL)) {
		CCMNI_ERR_MSG(ccmni->md_id, "%s_Close: MD%d ctlb is NULL\n", dev->name, ccmni->md_id);
		return -1;
	}

	atomic_dec(&ccmni->usage);
	ccmni_tmp = ccmni_ctl->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp)
		atomic_dec(&ccmni_tmp->usage);

	netif_tx_disable(dev);

	if (unlikely(ccmni_ctl->ccci_ops->md_ability & MODEM_CAP_NAPI))
		napi_disable(ccmni->napi);

	CCMNI_INF_MSG(ccmni->md_id, "%s_Close: cnt=(%d, %d)\n",
		dev->name, atomic_read(&ccmni->usage), atomic_read(&ccmni_tmp->usage));

	return 0;
}

static int ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;
	int skb_len = skb->len;
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(dev);
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[ccmni->md_id];
	unsigned int is_ack = 0;

	/* dev->mtu is changed  if dev->mtu is changed by upper layer */
	if (unlikely(skb->len > dev->mtu)) {
		CCMNI_ERR_MSG(ccmni->md_id, "CCMNI%d write fail: len(0x%x)>MTU(0x%x, 0x%x)\n",
			ccmni->index, skb->len, CCMNI_MTU, dev->mtu);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (unlikely(skb_headroom(skb) < sizeof(struct ccci_header))) {
		CCMNI_ERR_MSG(ccmni->md_id, "CCMNI%d write fail: header room(%d) < ccci_header(%d)\n",
			ccmni->index, skb_headroom(skb), dev->hard_header_len);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (ctlb->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD)
		is_ack = is_ack_skb(ccmni->md_id, skb);

	if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_TX)) {
		CCMNI_INF_MSG(ccmni->md_id, "[TX]CCMNI%d head_len=%d len=%d ack=%d tx_pkt=%ld\n",
			ccmni->index, skb_headroom(skb), skb->len, is_ack, (dev->stats.tx_packets+1));
	}

	if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_TX_SKB))
		ccmni_dbg_skb_header(ccmni->md_id, true, skb);

	ret = ctlb->ccci_ops->send_pkt(ccmni->md_id, ccmni->index, skb, is_ack);
	if (ret == CCMNI_ERR_MD_NO_READY || ret == CCMNI_ERR_TX_INVAL) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		ccmni->tx_busy_cnt[is_ack] = 0;
		CCMNI_ERR_MSG(ccmni->md_id, "[TX]CCMNI%d send tx_pkt=%ld(ack=%d) fail: %d\n",
			ccmni->index, (dev->stats.tx_packets+1), is_ack, ret);
		return NETDEV_TX_OK;
	} else if (ret == CCMNI_ERR_TX_BUSY) {
		goto tx_busy;
	}
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb_len;
	if (ccmni->tx_busy_cnt[is_ack] > 10) {
		CCMNI_ERR_MSG(ccmni->md_id, "[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry %ld times done\n",
			ccmni->index, dev->stats.tx_packets, is_ack, ccmni->tx_busy_cnt[is_ack]);
	}
	ccmni->tx_busy_cnt[is_ack] = 0;

	return NETDEV_TX_OK;

tx_busy:
	if (unlikely(!(ctlb->ccci_ops->md_ability & MODEM_CAP_TXBUSY_STOP))) {
		if ((ccmni->tx_busy_cnt[is_ack]++)%100 == 0)
			CCMNI_ERR_MSG(ccmni->md_id, "[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry_times=%ld\n",
				ccmni->index, dev->stats.tx_packets, is_ack, ccmni->tx_busy_cnt[is_ack]);
	} else {
		ccmni->tx_busy_cnt[is_ack]++;
	}
	return NETDEV_TX_BUSY;
}

static int ccmni_change_mtu(struct net_device *dev, int new_mtu)
{
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(dev);

	if (new_mtu > CCMNI_MTU)
		return -EINVAL;

	dev->mtu = new_mtu;
	CCMNI_INF_MSG(ccmni->md_id, "CCMNI%d change mtu_siz=%d\n", ccmni->index, new_mtu);
	return 0;
}

static void ccmni_tx_timeout(struct net_device *dev)
{
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(dev);
	ccmni_ctl_block_t *ccmni_ctl = ccmni_ctl_blk[ccmni->md_id];

	CCMNI_INF_MSG(ccmni->md_id, "ccmni%d_tx_timeout: usage_cnt=%d, timeout=%ds\n",
		ccmni->index, atomic_read(&ccmni->usage), (ccmni->dev->watchdog_timeo/HZ));

	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0) {
		if (ccmni_ctl->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ)
			netif_tx_wake_all_queues(dev);
		else
			netif_wake_queue(dev);
	}
}

static int ccmni_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int md_id, md_id_irat, usage_cnt;
	ccmni_instance_t *ccmni_irat;
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(dev);
	ccmni_instance_t *ccmni_tmp = NULL;
	ccmni_ctl_block_t *ctlb = NULL;
	ccmni_ctl_block_t *ctlb_irat = NULL;
	unsigned int timeout = 0;

	switch (cmd) {
	case SIOCSTXQSTATE:
		/* ifru_ivalue[3~0]:start/stop; ifru_ivalue[7~4]:reserve; */
		/* ifru_ivalue[15~8]:user id, bit8=rild, bit9=thermal */
		/* ifru_ivalue[31~16]: watchdog timeout value */
		ctlb = ccmni_ctl_blk[ccmni->md_id];
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			/*ignore txq stop/resume if dev is not running */
			if (atomic_read(&ccmni->usage) > 0 && netif_running(dev)) {
				atomic_dec(&ccmni->usage);

				netif_tx_disable(dev);
				/* stop queue won't stop Tx watchdog (ndo_tx_timeout) */
				timeout = (ifr->ifr_ifru.ifru_ivalue & 0xFFFF0000) >> 16;
				if (timeout == 0)
					dev->watchdog_timeo = 60*HZ;
				else
					dev->watchdog_timeo = timeout*HZ;

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				if (ccmni_tmp != ccmni) { /* iRAT ccmni */
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage, usage_cnt);
				}
			}
		} else {
			if (atomic_read(&ccmni->usage) <= 0 && netif_running(dev)) {
				netif_tx_wake_all_queues(dev);
				dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
				atomic_inc(&ccmni->usage);

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				if (ccmni_tmp != ccmni) { /* iRAT ccmni */
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage, usage_cnt);
				}
			}
		}
		if (likely(ccmni_tmp != NULL)) {
			CCMNI_INF_MSG(ccmni->md_id, "SIOCSTXQSTATE: %s_state=0x%x, cnt=(%d, %d)\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue, atomic_read(&ccmni->usage),
				atomic_read(&ccmni_tmp->usage));
		} else {
			CCMNI_INF_MSG(ccmni->md_id, "SIOCSTXQSTATE: %s_state=0x%x, cnt=%d\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue, atomic_read(&ccmni->usage));
		}
		break;

	case SIOCCCMNICFG:
		md_id_irat = ifr->ifr_ifru.ifru_ivalue;
		md_id = ccmni->md_id;
		if (md_id_irat < 0 && md_id_irat >= MAX_MD_NUM) {
			CCMNI_ERR_MSG(md_id, "SIOCSCCMNICFG: %s invalid md_id(%d)\n",
				dev->name, (ifr->ifr_ifru.ifru_ivalue+1));
			return -EINVAL;
		}

		if (dev != ccmni->dev) {
			CCMNI_INF_MSG(md_id, "SIOCCCMNICFG: %s iRAT on MD%d, diff dev(%s->%s)\n",
				dev->name, (ifr->ifr_ifru.ifru_ivalue+1), ccmni->dev->name, dev->name);
			ccmni->dev = dev;
			atomic_set(&ccmni->usage, 0);
			ccmni->tx_busy_cnt[0] = 0;
			ccmni->tx_busy_cnt[1] = 0;
			break;
		}

		ctlb_irat = ccmni_ctl_blk[md_id_irat];
		if (ccmni->index >= ctlb_irat->ccci_ops->ccmni_num) {
			CCMNI_ERR_MSG(md_id, "SIOCSCCMNICFG: %s iRAT(MD%d->MD%d) fail,index(%d)>max_num(%d)\n",
				dev->name, md_id, md_id_irat, ccmni->index, ctlb_irat->ccci_ops->ccmni_num);
			break;
		}
		ccmni_irat = ctlb_irat->ccmni_inst[ccmni->index];

		if (md_id_irat == ccmni->md_id) {
			if (ccmni_irat->dev != dev) {
				CCMNI_INF_MSG(md_id, "SIOCCCMNICFG: iRAT on the same MD%d from %s to %s\n",
					(ifr->ifr_ifru.ifru_ivalue+1), ccmni_irat->dev->name, dev->name);
				ccmni_irat->dev = dev;
			}
			CCMNI_INF_MSG(md_id, "SIOCCCMNICFG: %s iRAT on the same MD%d, cnt=%d\n",
				dev->name, (ifr->ifr_ifru.ifru_ivalue+1), atomic_read(&ccmni->usage));
			break;
		}

		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_irat->usage, usage_cnt);
		/* fix dev!=ccmni_irat->dev issue when MD3-CC3MNI -> MD3-CCMNI */
		ccmni_irat->dev = dev;
		memcpy(netdev_priv(dev), ccmni_irat, sizeof(ccmni_instance_t));

		ctlb = ccmni_ctl_blk[md_id];
		ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
		atomic_set(&ccmni_tmp->usage, usage_cnt);
		ccmni_tmp->tx_busy_cnt[0] = ccmni->tx_busy_cnt[0];
		ccmni_tmp->tx_busy_cnt[1] = ccmni->tx_busy_cnt[1];

		CCMNI_INF_MSG(md_id,
			"SIOCCCMNICFG: %s iRAT MD%d->MD%d, dev_cnt=%d, md_cnt=%d, md_irat_cnt=%d, irat_dev=%s\n",
			dev->name, (md_id+1), (ifr->ifr_ifru.ifru_ivalue+1), atomic_read(&ccmni->usage),
			atomic_read(&ccmni_tmp->usage), atomic_read(&ccmni_irat->usage), ccmni_irat->dev->name);
		break;

	default:
		CCMNI_ERR_MSG(ccmni->md_id, "%s: unknown ioctl cmd=%x\n", dev->name, cmd);
		break;
	}

	return 0;
}

static const struct net_device_ops ccmni_netdev_ops = {
	.ndo_open		= ccmni_open,
	.ndo_stop		= ccmni_close,
	.ndo_start_xmit	= ccmni_start_xmit,
	.ndo_tx_timeout	= ccmni_tx_timeout,
	.ndo_do_ioctl   = ccmni_ioctl,
	.ndo_change_mtu = ccmni_change_mtu,
	.ndo_select_queue = ccmni_select_queue,
};

static int ccmni_napi_poll(struct napi_struct *napi, int budget)
{
	ccmni_instance_t *ccmni = (ccmni_instance_t *)netdev_priv(napi->dev);
	int md_id = ccmni->md_id;
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[md_id];

	del_timer(ccmni->timer);

	if (ctlb->ccci_ops->napi_poll)
		return ctlb->ccci_ops->napi_poll(md_id, ccmni->index, napi, budget);
	else
		return 0;
}

static void ccmni_napi_poll_timeout(unsigned long data)
{
	ccmni_instance_t *ccmni = (ccmni_instance_t *)data;

	CCMNI_ERR_MSG(ccmni->md_id, "CCMNI%d lost NAPI polling\n", ccmni->index);
}


/********************ccmni driver register  ccci function********************/
static inline int ccmni_inst_init(int md_id, ccmni_instance_t *ccmni, struct net_device *dev)
{
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[md_id];
	int ret = 0;

	ret = ctlb->ccci_ops->get_ccmni_ch(md_id, ccmni->index, &ccmni->ch);
	if (ret) {
		CCMNI_ERR_MSG(md_id, "get ccmni%d channel fail\n", ccmni->index);
		return ret;
	}

	ccmni->dev = dev;
	ccmni->ctlb = ctlb;
	ccmni->md_id = md_id;
	ccmni->napi = kzalloc(sizeof(struct napi_struct), GFP_KERNEL);
	ccmni->timer = kzalloc(sizeof(struct timer_list), GFP_KERNEL);

	/* register napi device */
	if (dev && (ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
		init_timer(ccmni->timer);
		ccmni->timer->function = ccmni_napi_poll_timeout;
		ccmni->timer->data = (unsigned long)ccmni;
		netif_napi_add(dev, ccmni->napi, ccmni_napi_poll, ctlb->ccci_ops->napi_poll_weigh);
	}
#ifdef ENABLE_WQ_GRO
	if (dev)
		netif_napi_add(dev, ccmni->napi, ccmni_napi_poll, ctlb->ccci_ops->napi_poll_weigh);
#endif

	atomic_set(&ccmni->usage, 0);
	spin_lock_init(&ccmni->spinlock);

	return ret;
}

static int ccmni_init(int md_id, ccmni_ccci_ops_t *ccci_info)
{
	int i = 0, j = 0, ret = 0;
	ccmni_ctl_block_t *ctlb = NULL;
	ccmni_ctl_block_t *ctlb_irat_src = NULL;
	ccmni_instance_t  *ccmni = NULL;
	ccmni_instance_t  *ccmni_irat_src = NULL;
	struct net_device *dev = NULL;

	if (unlikely(ccci_info->md_ability & MODEM_CAP_CCMNI_DISABLE)) {
		CCMNI_ERR_MSG(md_id, "no need init ccmni: md_ability=0x%08X\n", ccci_info->md_ability);
		return 0;
	}

	ctlb = kzalloc(sizeof(ccmni_ctl_block_t), GFP_KERNEL);
	if (unlikely(ctlb == NULL)) {
		CCMNI_ERR_MSG(md_id, "alloc ccmni ctl struct fail\n");
		return -ENOMEM;
	}

	ctlb->ccci_ops = kzalloc(sizeof(ccmni_ccci_ops_t), GFP_KERNEL);
	if (unlikely(ctlb->ccci_ops == NULL)) {
		CCMNI_ERR_MSG(md_id, "alloc ccmni_ccci_ops struct fail\n");
		ret = -ENOMEM;
		goto alloc_mem_fail;
	}

	ccmni_ctl_blk[md_id] = ctlb;

	memcpy(ctlb->ccci_ops, ccci_info, sizeof(ccmni_ccci_ops_t));

	CCMNI_INF_MSG(md_id,
		"ccmni_init: ccmni_num=%d, md_ability=0x%08x, irat_en=%08x, irat_md_id=%d, send_pkt=%p, get_ccmni_ch=%p, name=%s\n",
		ctlb->ccci_ops->ccmni_num, ctlb->ccci_ops->md_ability,
		(ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_IRAT),
		ctlb->ccci_ops->irat_md_id, ctlb->ccci_ops->send_pkt,
		ctlb->ccci_ops->get_ccmni_ch, ctlb->ccci_ops->name);

	ccmni_debug_file_init(md_id);

	if (((ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_IRAT) == 0) ||
	((ctlb->ccci_ops->md_ability & MODEM_CAP_WORLD_PHONE) != 0)) {
		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			/* allocate netdev */
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ)
				/* alloc multiple tx queue, 2 txq and 1 rxq */
				dev = alloc_etherdev_mqs(sizeof(ccmni_instance_t), 2, 1);
			else
				dev = alloc_etherdev(sizeof(ccmni_instance_t));
			if (unlikely(dev == NULL)) {
				CCMNI_ERR_MSG(md_id, "alloc netdev fail\n");
				ret = -ENOMEM;
				goto alloc_netdev_fail;
			}

			/* init net device */
			dev->header_ops = NULL;
			dev->mtu = CCMNI_MTU;
			dev->tx_queue_len = CCMNI_TX_QUEUE;
			dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
			dev->flags = (IFF_NOARP | IFF_BROADCAST) & /* ccmni is a pure IP device */
					(~IFF_MULTICAST);	/* ccmni is P2P */
			dev->features = NETIF_F_VLAN_CHALLENGED; /* not support VLAN */
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_SGIO) {
				dev->features |= NETIF_F_SG;
				dev->hw_features |= NETIF_F_SG;
			}
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI) {
#ifdef ENABLE_NAPI_GRO
				dev->features |= NETIF_F_GRO;
				dev->hw_features |= NETIF_F_GRO;
#else
				/*
				 * check gro_list_prepare, GRO needs hard_header_len == ETH_HLEN.
				 * CCCI header can use ethernet header and padding bytes' region.
				 */
				dev->hard_header_len += sizeof(struct ccci_header);
#endif
			} else {
#ifdef ENABLE_WQ_GRO
				dev->features |= NETIF_F_GRO;
				dev->hw_features |= NETIF_F_GRO;
#else
				dev->hard_header_len += sizeof(struct ccci_header);
#endif
			}
			dev->addr_len = ETH_ALEN; /* ethernet header size */
			dev->destructor = free_netdev;
			dev->netdev_ops = &ccmni_netdev_ops;
			random_ether_addr((u8 *) dev->dev_addr);

			sprintf(dev->name, "%s%d", ctlb->ccci_ops->name, i);
			CCMNI_INF_MSG(md_id, "register netdev name: %s\n", dev->name);

			/* init private structure of netdev */
			ccmni = netdev_priv(dev);
			ccmni->index = i;
			ret = ccmni_inst_init(md_id, ccmni, dev);
			if (ret) {
				CCMNI_ERR_MSG(md_id, "initial ccmni instance fail\n");
				goto alloc_netdev_fail;
			}
			ctlb->ccmni_inst[i] = ccmni;

			/* register net device */
			ret = register_netdev(dev);
			if (ret) {
				CCMNI_ERR_MSG(md_id, "CCMNI%d register netdev fail: %d\n", i, ret);
				goto alloc_netdev_fail;
			}

			CCMNI_DBG_MSG(ccmni->md_id, "CCMNI%d=%p, ctlb=%p, ctlb_ops=%p, dev=%p\n",
				i, ccmni, ccmni->ctlb, ccmni->ctlb->ccci_ops, ccmni->dev);
		}
	}

	if ((ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_IRAT) != 0) {
		if (ctlb->ccci_ops->irat_md_id < 0 || ctlb->ccci_ops->irat_md_id >= MAX_MD_NUM) {
			CCMNI_ERR_MSG(md_id, "md%d IRAT fail because invalid irat md(%d)\n",
				md_id, ctlb->ccci_ops->irat_md_id);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		ctlb_irat_src = ccmni_ctl_blk[ctlb->ccci_ops->irat_md_id];
		if (!ctlb_irat_src) {
			CCMNI_ERR_MSG(md_id, "md%d IRAT fail because irat md%d ctlb is NULL\n",
				md_id, ctlb->ccci_ops->irat_md_id);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		if (unlikely(ctlb->ccci_ops->ccmni_num > ctlb_irat_src->ccci_ops->ccmni_num)) {
			CCMNI_ERR_MSG(md_id, "IRAT fail because number of src(%d) and dest(%d) ccmni isn't equal\n",
				ctlb_irat_src->ccci_ops->ccmni_num, ctlb->ccci_ops->ccmni_num);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			if ((ctlb->ccci_ops->md_ability & MODEM_CAP_WORLD_PHONE) != 0)
				ccmni = ctlb->ccmni_inst[i];
			else
				ccmni = kzalloc(sizeof(ccmni_instance_t), GFP_KERNEL);
			if (unlikely(ccmni == NULL)) {
				CCMNI_ERR_MSG(md_id, "alloc ccmni instance fail\n");
				ret = -ENOMEM;
				goto alloc_mem_fail;
			}

			ccmni_irat_src = kzalloc(sizeof(ccmni_instance_t), GFP_KERNEL);
			if (unlikely(ccmni_irat_src == NULL)) {
				CCMNI_ERR_MSG(md_id, "alloc ccmni_irat instance fail\n");
				kfree(ccmni);
				ret = -ENOMEM;
				goto alloc_mem_fail;
			}

			/* initial irat ccmni instance */
			ccmni->index = i;
			dev = ctlb_irat_src->ccmni_inst[i]->dev;
			if ((ctlb->ccci_ops->md_ability & MODEM_CAP_WORLD_PHONE) != 0)
				ccmni->dev = dev;
			else {
				ret = ccmni_inst_init(md_id, ccmni, dev);
				if (ret) {
					CCMNI_ERR_MSG(md_id, "initial ccmni instance fail\n");
					kfree(ccmni);
					kfree(ccmni_irat_src);
					goto alloc_mem_fail;
				}
				ctlb->ccmni_inst[i] = ccmni;
			}
			/* initial irat source ccmni instance */
			memcpy(ccmni_irat_src, ctlb_irat_src->ccmni_inst[i], sizeof(ccmni_instance_t));
			ctlb_irat_src->ccmni_inst[i] = ccmni_irat_src;
			CCMNI_DBG_MSG(md_id, "[IRAT]CCMNI%d=%p, ctlb=%p, ctlb_ops=%p, dev=%p\n",
				i, ccmni, ccmni->ctlb, ccmni->ctlb->ccci_ops, ccmni->dev);
		}
	}

	snprintf(ctlb->wakelock_name, sizeof(ctlb->wakelock_name), "ccmni_md%d", (md_id+1));
	wake_lock_init(&ctlb->ccmni_wakelock, WAKE_LOCK_SUSPEND, ctlb->wakelock_name);

	return 0;

alloc_netdev_fail:
	if (dev) {
		free_netdev(dev);
		ctlb->ccmni_inst[i] = NULL;
	}

	for (j = i-1; j >= 0; j--) {
		ccmni = ctlb->ccmni_inst[j];
		unregister_netdev(ccmni->dev);
		/* free_netdev(ccmni->dev); */
		ctlb->ccmni_inst[j] = NULL;
	}

alloc_mem_fail:
	kfree(ctlb->ccci_ops);
	kfree(ctlb);

	ccmni_ctl_blk[md_id] = NULL;
	return ret;
}

static void ccmni_exit(int md_id)
{
	int i = 0;
	ccmni_ctl_block_t *ctlb = NULL;
	ccmni_instance_t  *ccmni = NULL;

	CCMNI_INF_MSG(md_id, "ccmni_exit\n");

	ctlb = ccmni_ctl_blk[md_id];
	if (ctlb) {
		if (ctlb->ccci_ops == NULL)
			goto ccmni_exit_ret;

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			ccmni = ctlb->ccmni_inst[i];
			if (ccmni) {
				CCMNI_INF_MSG(md_id, "ccmni_exit: unregister ccmni%d dev\n", i);
				unregister_netdev(ccmni->dev);
				/* free_netdev(ccmni->dev); */
				ctlb->ccmni_inst[i] = NULL;
			}
		}

		kfree(ctlb->ccci_ops);

ccmni_exit_ret:
		kfree(ctlb);
		ccmni_ctl_blk[md_id] = NULL;
	}
}

static int ccmni_rx_callback(int md_id, int ccmni_idx, struct sk_buff *skb, void *priv_data)
{
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[md_id];
	/* struct ccci_header *ccci_h = (struct ccci_header*)skb->data; */
	ccmni_instance_t *ccmni = NULL;
	struct net_device *dev = NULL;
	int pkt_type, skb_len;
#if defined(CCCI_SKB_TRACE)
	struct iphdr *iph;
#endif

	if (unlikely(ctlb == NULL || ctlb->ccci_ops == NULL)) {
		CCMNI_ERR_MSG(md_id, "invalid CCMNI%d ctrl/ops struct\n", ccmni_idx);
		dev_kfree_skb(skb);
		return -1;
	}

	ccmni = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni->dev;

	/* skb_pull(skb, sizeof(struct ccci_header)); */
	pkt_type = skb->data[0] & 0xF0;
	ccmni_make_etherframe(skb->data-ETH_HLEN, dev->dev_addr, pkt_type);
	skb_set_mac_header(skb, -ETH_HLEN);
	skb_reset_network_header(skb);
	skb->dev = dev;
	if (pkt_type == 0x60)
		skb->protocol  = htons(ETH_P_IPV6);
	else
		skb->protocol  = htons(ETH_P_IP);

	skb->ip_summed = CHECKSUM_NONE;
	skb_len = skb->len;

	if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_RX))
		CCMNI_INF_MSG(md_id, "[RX]CCMNI%d recv data_len=%d\n", ccmni_idx, skb->len);

	if (unlikely(ccmni_debug_level&CCMNI_DBG_LEVEL_RX_SKB))
		ccmni_dbg_skb_header(ccmni->md_id, false, skb);

#if defined(NETDEV_TRACE) && defined(NETDEV_DL_TRACE)
	skb->dbg_flag = 0x1;
#endif

#if defined(CCCI_SKB_TRACE)
	iph = (struct iphdr *)skb->data;
	ctlb->net_rx_delay[2] = iph->id;
	ctlb->net_rx_delay[0] = dev->stats.rx_bytes + skb_len;
	ctlb->net_rx_delay[1] = dev->stats.tx_bytes;
#endif
	if (likely(ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
#ifdef ENABLE_NAPI_GRO
		napi_gro_receive(ccmni->napi, skb);
#else
		netif_receive_skb(skb);
#endif
	} else {
#ifdef ENABLE_WQ_GRO
		spin_lock_bh(&ccmni->spinlock);
		napi_gro_receive(ccmni->napi, skb);
		spin_unlock_bh(&ccmni->spinlock);
#else
		if (!in_interrupt())
			netif_rx_ni(skb);
		else
			netif_rx(skb);
#endif
	}
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;

	wake_lock_timeout(&ctlb->ccmni_wakelock, HZ);

	return 0;
}

static void ccmni_md_state_callback(int md_id, int ccmni_idx, MD_STATE state, int is_ack)
{
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[md_id];
	ccmni_instance_t  *ccmni = NULL;
	struct netdev_queue *net_queue = NULL;

	if (unlikely(ctlb == NULL)) {
		CCMNI_ERR_MSG(md_id, "invalid ccmni ctrl struct when ccmni_idx=%d md_sta=%d\n", ccmni_idx, state);
		return;
	}

	ccmni = ctlb->ccmni_inst[ccmni_idx];

	if ((state != RX_IRQ) && (state != RX_FLUSH) &&
		(state != TX_IRQ) && (state != TX_FULL) &&
		(atomic_read(&ccmni->usage) > 0)) {
		CCMNI_INF_MSG(md_id, "md_state_cb: CCMNI%d, md_sta=%d, usage=%d\n",
			ccmni_idx, state, atomic_read(&ccmni->usage));
	}

	switch (state) {
	case READY:
		/*don't carrire on here, MD data link may be not ready. carrirer on it in ccmni_open*/
		/*netif_carrier_on(ccmni->dev);*/
		ccmni->tx_seq_num[0] = 0;
		ccmni->tx_seq_num[1] = 0;
		ccmni->rx_seq_num = 0;
		break;

	case EXCEPTION:
	case RESET:
	case WAITING_TO_STOP:
		netif_carrier_off(ccmni->dev);
		break;

	case RX_IRQ:
		mod_timer(ccmni->timer, jiffies+HZ);
		napi_schedule(ccmni->napi);
		wake_lock_timeout(&ctlb->ccmni_wakelock, HZ);
		break;
#ifdef ENABLE_WQ_GRO
	case RX_FLUSH:
		spin_lock_bh(&ccmni->spinlock);
		napi_gro_flush(ccmni->napi, false);
		spin_unlock_bh(&ccmni->spinlock);
		break;
#endif

	case TX_IRQ:
		if (netif_running(ccmni->dev) && atomic_read(&ccmni->usage) > 0) {
			if (likely(ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ)) {
				if (is_ack)
					net_queue = netdev_get_tx_queue(ccmni->dev, CCMNI_TXQ_FAST);
				else
					net_queue = netdev_get_tx_queue(ccmni->dev, CCMNI_TXQ_NORMAL);
				if (netif_tx_queue_stopped(net_queue))
					netif_tx_wake_queue(net_queue);
			} else {
				if (netif_queue_stopped(ccmni->dev))
					netif_wake_queue(ccmni->dev);
			}
			CCMNI_INF_MSG(md_id, "md_state_cb: %s, md_sta=TX_IRQ, index=0x%x, usage=%d\n",
				ccmni->dev->name, ccmni->index, atomic_read(&ccmni->usage));
		}
		break;

	case TX_FULL:
		if (atomic_read(&ccmni->usage) > 0) {
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
				if (is_ack)
					net_queue = netdev_get_tx_queue(ccmni->dev, CCMNI_TXQ_FAST);
				else
					net_queue = netdev_get_tx_queue(ccmni->dev, CCMNI_TXQ_NORMAL);
				netif_tx_stop_queue(net_queue);
			} else
				netif_stop_queue(ccmni->dev);
			CCMNI_INF_MSG(md_id, "md_state_cb: %s, md_sta=TX_FULL, index=0x%x, usage=%d\n",
				ccmni->dev->name, ccmni->index, atomic_read(&ccmni->usage));
		}
		break;
	default:
		break;
	}
}

static void ccmni_dump(int md_id, int ccmni_idx, unsigned int flag)
{
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[md_id];
	ccmni_instance_t *ccmni = NULL;
	ccmni_instance_t *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	struct netdev_queue *dev_queue = NULL;
	struct netdev_queue *ack_queue = NULL;
	struct Qdisc *qdisc;
	struct Qdisc *ack_qdisc;

	if (ctlb == NULL)
		return;

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	if (unlikely(ccmni_tmp == NULL))
		return;

	if ((ccmni_tmp->dev->stats.rx_packets == 0) && (ccmni_tmp->dev->stats.tx_packets == 0))
		return;

	dev = ccmni_tmp->dev;
	/*ccmni diff from ccmni_tmp for MD IRAT*/
	ccmni = (ccmni_instance_t *)netdev_priv(dev);
	dev_queue = netdev_get_tx_queue(dev, 0);
	if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
		ack_queue = netdev_get_tx_queue(dev, CCMNI_TXQ_FAST);
		qdisc = dev_queue->qdisc;
		ack_qdisc = ack_queue->qdisc;
		/*stats.rx_dropped is dropped in ccmni, dev->rx_dropped is dropped in net device layer*/
		/*stats.tx_packets is count by ccmni, bstats.packets is count by qdisc in net device layer*/
		CCMNI_INF_MSG(md_id,
			"%s(%d,%d), irat_MD%d, rx=(%ld,%ld), tx=(%ld,%d,%d), txq_len=(%d,%d), tx_drop=(%ld,%d,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx,0x%lx)\n",
			dev->name, atomic_read(&ccmni->usage), atomic_read(&ccmni_tmp->usage), (ccmni->md_id+1),
			dev->stats.rx_packets, dev->stats.rx_bytes,
			dev->stats.tx_packets, qdisc->bstats.packets, ack_qdisc->bstats.packets,
			qdisc->q.qlen, ack_qdisc->q.qlen,
			dev->stats.tx_dropped, qdisc->qstats.drops, ack_qdisc->qstats.drops,
			dev->stats.rx_dropped, atomic_long_read(&dev->rx_dropped),
			ccmni->tx_busy_cnt[0], ccmni->tx_busy_cnt[1],
			dev->state, dev->flags, dev_queue->state, ack_queue->state);
	} else
		CCMNI_INF_MSG(md_id,
			"%s(%d,%d), irat_MD%d, rx=(%ld,%ld), tx=(%ld,%ld), txq_len=%d, tx_drop=(%ld,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx)\n",
			dev->name, atomic_read(&ccmni->usage), atomic_read(&ccmni_tmp->usage), (ccmni->md_id+1),
			dev->stats.rx_packets, dev->stats.rx_bytes, dev->stats.tx_packets, dev->stats.tx_bytes,
			dev->qdisc->q.qlen, dev->stats.tx_dropped, dev->qdisc->qstats.drops, dev->stats.rx_dropped,
			atomic_long_read(&dev->rx_dropped), ccmni->tx_busy_cnt[0], ccmni->tx_busy_cnt[1],
			dev->state, dev->flags, dev_queue->state);
}

static void ccmni_dump_rx_status(int md_id, int ccmni_idx, unsigned long long *status)
{
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[md_id];

	status[0] = ctlb->net_rx_delay[0];
	status[1] = ctlb->net_rx_delay[1];
	status[2] = ctlb->net_rx_delay[2];
}

static struct ccmni_ch *ccmni_get_ch(int md_id, int ccmni_idx)
{
	ccmni_ctl_block_t *ctlb = ccmni_ctl_blk[md_id];

	return &ctlb->ccmni_inst[ccmni_idx]->ch;
}

struct ccmni_dev_ops ccmni_ops = {
	.skb_alloc_size = 1600,
	.init = &ccmni_init,
	.rx_callback = &ccmni_rx_callback,
	.md_state_callback = &ccmni_md_state_callback,
	.exit = ccmni_exit,
	.dump = ccmni_dump,
	.dump_rx_status = ccmni_dump_rx_status,
	.get_ch = ccmni_get_ch,
	.is_ack_skb = is_ack_skb,
};
