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
#include <linux/stacktrace.h>
#include "ccmni.h"
#include "ccci_debug.h"
#include <mt-plat/met_drv.h>


struct ccmni_ctl_block *ccmni_ctl_blk[MAX_MD_NUM];

/* Time in nano seconds. This number must be less than a second. */
#ifdef ENABLE_WQ_GRO
long int gro_flush_timer __read_mostly = 1000000L;
#else
long int gro_flush_timer;
#endif

#define APP_VIP_MARK		0x80000000

/********************internal function*********************/
static void ccmni_make_etherframe(int md_id, struct net_device *dev,
	void *_eth_hdr, unsigned char *mac_addr, unsigned int packet_type)
{
	struct ethhdr *eth_hdr = _eth_hdr;
	static unsigned char dest_mac[6] = {
		0x0a, 0x1a, 0x2a, 0x3a, 0x4a, 0x5a };
	static unsigned char src_mac[6] = {
		0x06, 0x16, 0x26, 0x36, 0x46, 0x56 };
	struct net_device *br_dev = NULL;

	if (IS_CCMNI_LAN(dev)) {
		memcpy(eth_hdr->h_source, src_mac, sizeof(eth_hdr->h_source));

		br_dev = __dev_get_by_name(dev_net(dev), "mdbr0");
		if (br_dev) {
			memcpy(eth_hdr->h_dest, br_dev->dev_addr,
				sizeof(eth_hdr->h_dest));
		} else {
			CCMNI_DBG_MSG(md_id,
				"%s can't find mdbr0\n", dev->name);
			memcpy(eth_hdr->h_dest, dest_mac,
				sizeof(eth_hdr->h_dest));
		}
	} else {
		memcpy(eth_hdr->h_dest, mac_addr, sizeof(eth_hdr->h_dest));
		memset(eth_hdr->h_source, 0, sizeof(eth_hdr->h_source));
	}

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
	struct md_tag_packet *tag = NULL;
	unsigned int count = 0;

	tag = (struct md_tag_packet *)skb->head;
	if (tag->guard_pattern == MDT_TAG_PATTERN)
		count = sizeof(tag->info);

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		struct ipv6hdr *iph = (struct ipv6hdr *)skb->data;
		u32 total_len =
			sizeof(struct ipv6hdr)
			+
			ntohs(iph->payload_len);

		/* copy md tag into skb->tail and
		 * skb->len > 128B(md recv buf size)
		 */
		/* this case will cause md EE */
		if (total_len <= 128 - sizeof(struct ccci_header) - count) {
			u8 nexthdr = iph->nexthdr;
			__be16 frag_off;
			u32 l4_off = ipv6_skip_exthdr(skb,
				sizeof(struct ipv6hdr),
				&nexthdr, &frag_off);

			tcph = (struct tcphdr *)(skb->data + l4_off);
			if (nexthdr == IPPROTO_TCP &&
				!tcph->syn && !tcph->fin &&
			    !tcph->rst &&
				((total_len - l4_off) == (tcph->doff << 2)))
				ret = 1;
		}
	} else if (packet_type == IPV4_VERSION) {
		struct iphdr *iph = (struct iphdr *)skb->data;

		if (ntohs(iph->tot_len) <=
				128 - sizeof(struct ccci_header) - count) {
			tcph = (struct tcphdr *)(skb->data + (iph->ihl << 2));

			if (iph->protocol == IPPROTO_TCP && !tcph->syn &&
				!tcph->fin && !tcph->rst &&
				(ntohs(iph->tot_len) == (iph->ihl << 2) +
				(tcph->doff << 2)))
				ret = 1;
		}
	}

	return ret;
}

static inline int arp_reply(int md_id, struct net_device *dev,
	struct ethhdr *eth, struct sk_buff *skb)
{
	struct arphdr_in *request, *reply;
	struct sk_buff *new_skb;
	struct ethhdr *new_eth;
	static unsigned char fake_sha[6] = {
		0x06, 0x16, 0x26, 0x36, 0x46, 0x56 };

	request = (struct arphdr_in *)(skb->data + skb_network_offset(skb));
	if (htons(request->ar_hrd) != ARPHRD_ETHER ||
		htons(request->ar_pro) != ETH_P_IP ||
		request->ar_hln != ETH_ALEN || request->ar_pln != 4 ||
		htons(request->ar_op) != ARPOP_REQUEST) {
		CCMNI_DBG_MSG(md_id,
			"arp_reply: %s not_arp_req, ar_hrd=0x%x, ar_pro=0x%x, ar_hln=%d, ar_pln=%d=, ar_op=0x%x\n",
			dev->name, htons(request->ar_hrd),
			htons(request->ar_pro), request->ar_hln,
			request->ar_pln, htons(request->ar_op));
		goto not_arp_req;
	}

	new_skb = netdev_alloc_skb_ip_align(dev, arp_hdr_len(dev));
	if (!new_skb) {
		CCMNI_DBG_MSG(md_id,
			"arp_reply: %s can't alloc new skb\n",
			dev->name);
		goto alloc_skb_fail;
	}

	reply = (struct arphdr_in *)new_skb->data;
	reply->ar_hrd = htons(ARPHRD_ETHER);
	reply->ar_pro = htons(ETH_P_IP);
	reply->ar_hln = ETH_ALEN;
	reply->ar_pln = 4;
	reply->ar_op = htons(ARPOP_REPLY);

	ether_addr_copy(reply->ar_sha, fake_sha);
	memcpy(reply->ar_sip, request->ar_tip, 4);
	ether_addr_copy(reply->ar_tha, request->ar_sha);
	memcpy(reply->ar_tip, request->ar_sip, 4);

	new_eth = (struct ethhdr *)(new_skb->data - 14);
	ether_addr_copy(new_eth->h_dest, eth->h_source);
	ether_addr_copy(new_eth->h_source, reply->ar_sha);
	new_eth->h_proto = htons(ETH_P_ARP);

	new_skb->len = 28;
	new_skb->mac_len = 14;
	skb_set_mac_header(new_skb, -ETH_HLEN);
	skb_reset_network_header(new_skb);
	new_skb->protocol = htons(ETH_P_ARP);

	if (!in_interrupt())
		netif_rx_ni(new_skb);
	else
		netif_rx(new_skb);

	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->len;
	dev_kfree_skb(skb);
	return 0;

 alloc_skb_fail:
	dev->stats.tx_dropped++;
	dev_kfree_skb(skb);
	return 0;

 not_arp_req:
	return 1;
}

#ifdef ENABLE_WQ_GRO
static int is_skb_gro(struct sk_buff *skb)
{
	u32 packet_type;

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV4_VERSION &&
		ip_hdr(skb)->protocol == IPPROTO_TCP)
		return 1;
	else if (packet_type == IPV6_VERSION &&
		ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
		return 1;
	else
		return 0;
}

static void ccmni_gro_flush(struct ccmni_instance *ccmni)
{
	struct timespec curr_time, diff;

	if (!gro_flush_timer)
		return;

	if (unlikely(ccmni->flush_time.tv_sec == 0)) {
		getnstimeofday(&ccmni->flush_time);
	} else {
		getnstimeofday(&(curr_time));
		diff = timespec_sub(curr_time, ccmni->flush_time);
		if ((diff.tv_sec > 0) || (diff.tv_nsec > gro_flush_timer)) {
			napi_gro_flush(ccmni->napi, false);
			getnstimeofday(&ccmni->flush_time);
		}
	}
}
#endif

static inline int ccmni_forward_rx(struct ccmni_instance *ccmni,
	struct sk_buff *skb)
{
	bool flt_ok = false;
	bool flt_flag = true;
	unsigned int pkt_type;
	struct iphdr *iph;
	struct ipv6hdr *iph6;
	struct ccmni_fwd_filter flt_tmp;
	unsigned int i, j;
	u16 mask;
	u32 *addr1, *addr2;

	if (ccmni->flt_cnt) {
		for (i = 0; i < CCMNI_FLT_NUM; i++) {
			flt_tmp = ccmni->flt_tbl[i];
			pkt_type = skb->data[0] & 0xF0;
			if (!flt_tmp.ver || (flt_tmp.ver != pkt_type))
				continue;

			if (pkt_type == IPV4_VERSION) {
				iph = (struct iphdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = &iph->saddr;
				addr2 = &flt_tmp.ipv4.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = &iph->daddr;
					addr2 = &flt_tmp.ipv4.daddr;
				}
			} else if (pkt_type == IPV6_VERSION) {
				iph6 = (struct ipv6hdr *)skb->data;
				mask = flt_tmp.s_pref;
				addr1 = iph6->saddr.s6_addr32;
				addr2 = flt_tmp.ipv6.saddr;
				flt_flag = true;
				for (j = 0; flt_flag && j < 2; j++) {
					if (mask == 0) {
						mask = flt_tmp.d_pref;
						addr1 = iph6->daddr.s6_addr32;
						addr2 = flt_tmp.ipv6.daddr;
						continue;
					}
					if (mask <= 32 &&
						(addr1[0] >> (32 - mask) !=
						addr2[0] >> (32 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 64 &&
						(addr1[0] != addr2[0] ||
						addr1[1] >> (64 - mask) !=
						addr2[1] >> (64 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 96 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] >> (96 - mask) !=
						addr2[2] >> (96 - mask))) {
						flt_flag = false;
						break;
					}
					if (mask <= 128 &&
						(addr1[0] != addr2[0] ||
						addr1[1] != addr2[1] ||
						addr1[2] != addr2[2] ||
						addr1[3] >> (128 - mask) !=
						addr2[3] >> (128 - mask))) {
						flt_flag = false;
						break;
					}
					mask = flt_tmp.d_pref;
					addr1 = iph6->daddr.s6_addr32;
					addr2 = flt_tmp.ipv6.daddr;
				}
			}
			if (flt_flag) {
				flt_ok = true;
				break;
			}
		}

		if (flt_ok) {
			skb->ip_summed = CHECKSUM_NONE;
			skb_set_mac_header(skb, -ETH_HLEN);

			if (!in_interrupt())
				netif_rx_ni(skb);
			else
				netif_rx(skb);
			return NETDEV_TX_OK;
		}
	}

	return -1;
}


/********************netdev register function********************/
static u16 ccmni_select_queue(struct net_device *dev, struct sk_buff *skb,
	void *accel_priv, select_queue_fallback_t fallback)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[ccmni->md_id];

	if (ctlb->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD) {
		if (skb->mark == APP_VIP_MARK)
			return CCMNI_TXQ_FAST;

		if (ccmni->ack_prio_en && is_ack_skb(ccmni->md_id, skb))
			return CCMNI_TXQ_FAST;
		else
			return CCMNI_TXQ_NORMAL;
	} else
		return CCMNI_TXQ_NORMAL;
}

static int ccmni_open(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ccmni_ctl = ccmni_ctl_blk[ccmni->md_id];
	struct ccmni_instance *ccmni_tmp = NULL;
	int usage_cnt = 0;

	if (unlikely(ccmni_ctl == NULL)) {
		CCMNI_PR_DBG(ccmni->md_id,
			"%s_Open: MD%d ctlb is NULL\n",
			dev->name, ccmni->md_id);
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
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}

	CCMNI_INF_MSG(ccmni->md_id,
		"%s_Open:cnt=(%d,%d), md_ab=0x%X, gro=(%llx, %ld), flt_cnt=%d\n",
		dev->name, atomic_read(&ccmni->usage),
		atomic_read(&ccmni_tmp->usage),
		ccmni_ctl->ccci_ops->md_ability,
		dev->features, gro_flush_timer, ccmni->flt_cnt);
	return 0;
}

static int ccmni_close(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ccmni_ctl = ccmni_ctl_blk[ccmni->md_id];
	struct ccmni_instance *ccmni_tmp = NULL;
	int usage_cnt = 0;

	if (unlikely(ccmni_ctl == NULL)) {
		CCMNI_PR_DBG(ccmni->md_id, "%s_Close: MD%d ctlb is NULL\n",
			dev->name, ccmni->md_id);
		return -1;
	}

	atomic_dec(&ccmni->usage);
	ccmni_tmp = ccmni_ctl->ccmni_inst[ccmni->index];
	if (ccmni != ccmni_tmp) {
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
	}

	netif_tx_disable(dev);

	if (unlikely(ccmni_ctl->ccci_ops->md_ability & MODEM_CAP_NAPI))
		napi_disable(ccmni->napi);

	CCMNI_INF_MSG(ccmni->md_id, "%s_Close:cnt=(%d, %d)\n",
		dev->name, atomic_read(&ccmni->usage),
		atomic_read(&ccmni_tmp->usage));

	return 0;
}

static int ccmni_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;
	int skb_len = skb->len;
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[ccmni->md_id];
	unsigned int is_ack = 0;
	int mac_len = 0;
	struct md_tag_packet *tag = NULL;
	unsigned int count = 0;
	struct ethhdr *eth;
	__be16 type;
	struct iphdr *iph;

#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif

	if (ccmni_forward_rx(ccmni, skb) == NETDEV_TX_OK)
		return NETDEV_TX_OK;

	/* if data_len=1500 with mac_header for ccmni-lan,
	 * skb->len>MTU,so it must before MTU judgement
	 */
	if (IS_CCMNI_LAN(dev)) {
		if (skb_network_offset(skb) > 0) {
			eth = (struct ethhdr *)skb_mac_header(skb);
			type = eth->h_proto;

			if (htons(ETH_P_ARP) == type)
				if (!arp_reply(ccmni->md_id, dev, eth, skb))
					return NETDEV_TX_OK;

			mac_len = skb_network_offset(skb);
			skb_pull(skb, skb_network_offset(skb));
			skb_pop_mac_header(skb);
		} else {
			iph = (struct iphdr *)skb_network_header(skb);
			CCMNI_DBG_MSG(ccmni->md_id,
			"ccmni-lan send wrong pkt with eth_len=0, ip_id=%04X\n",
			iph->id);
			dump_stack();
			dev_kfree_skb(skb);
			dev->stats.tx_dropped++;
			return NETDEV_TX_OK;
		}
	}

	/* dev->mtu is changed  if dev->mtu is changed by upper layer */
	if (unlikely(skb->len > dev->mtu)) {
		CCMNI_PR_DBG(ccmni->md_id,
					"CCMNI%d write fail: len(0x%x)>MTU(0x%x, 0x%x)\n",
					ccmni->index, skb->len,
					CCMNI_MTU, dev->mtu);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (unlikely(skb_headroom(skb) < sizeof(struct ccci_header))) {
		CCMNI_PR_DBG(ccmni->md_id,
			"CCMNI%d write fail: header room(%d) < ccci_header(%d)\n",
			ccmni->index, skb_headroom(skb),
			dev->hard_header_len);
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	tag = (struct md_tag_packet *)skb->head;
	if (tag->guard_pattern == MDT_TAG_PATTERN) {
		if (ccmni->md_id == MD_SYS1) {
			count = sizeof(tag->info);
			memcpy(skb_tail_pointer(skb), &(tag->info), count);
			skb->len += count;
		} else {
			CCMNI_DBG_MSG(ccmni->md_id,
				"%s: MD%d not support MDT tag\n",
				dev->name, (ccmni->md_id + 1));
		}
	}

	if (ctlb->ccci_ops->md_ability & MODEM_CAP_DATA_ACK_DVD) {
		iph = (struct iphdr *)skb_network_header(skb);
		if (skb->mark == APP_VIP_MARK)
			is_ack = 1;
		else if (ccmni->ack_prio_en)
			is_ack = is_ack_skb(ccmni->md_id, skb);
	}

	ret = ctlb->ccci_ops->send_pkt(ccmni->md_id, ccmni->index, skb, is_ack);
	if (ret == CCMNI_ERR_MD_NO_READY || ret == CCMNI_ERR_TX_INVAL) {
		dev_kfree_skb(skb);
		dev->stats.tx_dropped++;
		ccmni->tx_busy_cnt[is_ack] = 0;
		CCMNI_DBG_MSG(ccmni->md_id,
			"[TX]CCMNI%d send tx_pkt=%ld(ack=%d) fail: %d\n",
			ccmni->index, (dev->stats.tx_packets + 1),
			is_ack, ret);
		return NETDEV_TX_OK;
	} else if (ret == CCMNI_ERR_TX_BUSY) {
		goto tx_busy;
	}
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb_len;
	if (ccmni->tx_busy_cnt[is_ack] > 10) {
		CCMNI_DBG_MSG(ccmni->md_id,
			"[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry %ld times done\n",
			ccmni->index, dev->stats.tx_packets,
			is_ack, ccmni->tx_busy_cnt[is_ack]);
	}
	ccmni->tx_busy_cnt[is_ack] = 0;

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->tx_met_time == 0) {
		ccmni->tx_met_time = jiffies;
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->tx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		snprintf(tag_name, 32, "%s_tx_bytes", dev->name);
		tag_id = CCMNI_TX_MET_ID + ccmni->index;
		met_tag_oneshot(tag_id, tag_name,
		(dev->stats.tx_bytes - ccmni->tx_met_bytes));
		ccmni->tx_met_bytes = dev->stats.tx_bytes;
		ccmni->tx_met_time = jiffies;
	}
#endif

	return NETDEV_TX_OK;

tx_busy:
	if (unlikely(!(ctlb->ccci_ops->md_ability & MODEM_CAP_TXBUSY_STOP))) {
		if ((ccmni->tx_busy_cnt[is_ack]++) % 100 == 0)
			CCMNI_DBG_MSG(ccmni->md_id,
				"[TX]CCMNI%d TX busy: tx_pkt=%ld(ack=%d) retry_times=%ld\n",
				ccmni->index, dev->stats.tx_packets,
				is_ack, ccmni->tx_busy_cnt[is_ack]);
	} else {
		ccmni->tx_busy_cnt[is_ack]++;
	}

	if (IS_CCMNI_LAN(dev)) {
		skb_push(skb, mac_len);
		skb_reset_mac_header(skb);
		if (skb->len > skb_len)
			skb->len = skb_len;
	}
	return NETDEV_TX_BUSY;
}

static int ccmni_change_mtu(struct net_device *dev, int new_mtu)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	if (new_mtu > CCMNI_MTU)
		return -EINVAL;

	dev->mtu = new_mtu;
	CCMNI_DBG_MSG(ccmni->md_id,
		"CCMNI%d change mtu_siz=%d\n", ccmni->index, new_mtu);
	return 0;
}

static void ccmni_tx_timeout(struct net_device *dev)
{
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);

	CCMNI_DBG_MSG(ccmni->md_id,
		"ccmni%d_tx_timeout: usage_cnt=%d, timeout=%ds\n",
		ccmni->index,
		atomic_read(&ccmni->usage), (ccmni->dev->watchdog_timeo/HZ));

	dev->stats.tx_errors++;
	if (atomic_read(&ccmni->usage) > 0)
		netif_tx_wake_all_queues(dev);
}

static int ccmni_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int md_id, md_id_irat, usage_cnt;
	struct ccmni_instance *ccmni_irat;
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(dev);
	struct ccmni_instance *ccmni_tmp = NULL;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_ctl_block *ctlb_irat = NULL;
	unsigned int timeout = 0;
	struct ccmni_fwd_filter flt_tmp;
	struct ccmni_flt_act flt_act;
	unsigned int i;
	unsigned int cmp_len;

	switch (cmd) {
	case SIOCSTXQSTATE:
		/* ifru_ivalue[3~0]:start/stop; ifru_ivalue[7~4]:reserve; */
		/* ifru_ivalue[15~8]:user id, bit8=rild, bit9=thermal */
		/* ifru_ivalue[31~16]: watchdog timeout value */
		ctlb = ccmni_ctl_blk[ccmni->md_id];
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			/*ignore txq stop/resume if dev is not running */
			if (atomic_read(&ccmni->usage) > 0 &&
				netif_running(dev)) {
				atomic_dec(&ccmni->usage);

				netif_tx_disable(dev);
				/* stop queue won't stop Tx
				 * watchdog (ndo_tx_timeout)
				 */
				timeout = (ifr->ifr_ifru.ifru_ivalue &
					0xFFFF0000) >> 16;
				if (timeout == 0)
					dev->watchdog_timeo = 60 * HZ;
				else
					dev->watchdog_timeo = timeout*HZ;

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		} else {
			if (atomic_read(&ccmni->usage) <= 0 &&
					netif_running(dev)) {
				netif_tx_wake_all_queues(dev);
				dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
				atomic_inc(&ccmni->usage);

				ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
				/* iRAT ccmni */
				if (ccmni_tmp != ccmni) {
					usage_cnt = atomic_read(&ccmni->usage);
					atomic_set(&ccmni_tmp->usage,
								usage_cnt);
				}
			}
		}
		if (likely(ccmni_tmp != NULL)) {
			CCMNI_DBG_MSG(ccmni->md_id,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=(%d, %d)\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage),
				atomic_read(&ccmni_tmp->usage));
		} else {
			CCMNI_DBG_MSG(ccmni->md_id,
				"SIOCSTXQSTATE: %s_state=0x%x, cnt=%d\n",
				dev->name, ifr->ifr_ifru.ifru_ivalue,
				atomic_read(&ccmni->usage));
		}
		break;

	case SIOCCCMNICFG:
		md_id_irat = ifr->ifr_ifru.ifru_ivalue;
		md_id = ccmni->md_id;
		if (md_id_irat < 0 || md_id_irat >= MAX_MD_NUM) {
			CCMNI_DBG_MSG(md_id,
				"SIOCSCCMNICFG: %s invalid md_id(%d)\n",
				dev->name, (ifr->ifr_ifru.ifru_ivalue + 1));
			return -EINVAL;
		}

		if (dev != ccmni->dev) {
			CCMNI_DBG_MSG(md_id,
				"SIOCCCMNICFG: %s iRAT on MD%d, diff dev(%s->%s)\n",
				dev->name, (ifr->ifr_ifru.ifru_ivalue + 1),
				ccmni->dev->name, dev->name);
			ccmni->dev = dev;
			atomic_set(&ccmni->usage, 0);
			ccmni->tx_busy_cnt[0] = 0;
			ccmni->tx_busy_cnt[1] = 0;
			break;
		}

		ctlb_irat = ccmni_ctl_blk[md_id_irat];
		if (ccmni->index >= ctlb_irat->ccci_ops->ccmni_num) {
			CCMNI_PR_DBG(md_id,
			"SIOCSCCMNICFG: %s iRAT(MD%d->MD%d) fail,index(%d)>max_num(%d)\n",
			dev->name, md_id, md_id_irat, ccmni->index,
			ctlb_irat->ccci_ops->ccmni_num);
			break;
		}
		ccmni_irat = ctlb_irat->ccmni_inst[ccmni->index];

		if (md_id_irat == ccmni->md_id) {
			if (ccmni_irat->dev != dev) {
				CCMNI_DBG_MSG(md_id,
					"SIOCCCMNICFG: %s iRAT on MD%d, diff dev(%s->%s)\n",
					dev->name,
					(ifr->ifr_ifru.ifru_ivalue + 1),
					ccmni_irat->dev->name, dev->name);
				ccmni_irat->dev = dev;
				usage_cnt = atomic_read(&ccmni->usage);
				atomic_set(&ccmni_irat->usage, usage_cnt);
			} else
				CCMNI_DBG_MSG(md_id,
					"SIOCCCMNICFG: %s iRAT on the same MD%d, cnt=%d\n",
					dev->name,
					(ifr->ifr_ifru.ifru_ivalue + 1),
					atomic_read(&ccmni->usage));
			break;
		}

		/* backup ccmni info of md_id into ctlb[md_id]->ccmni_inst */
		ctlb = ccmni_ctl_blk[md_id];
		ccmni_tmp = ctlb->ccmni_inst[ccmni->index];
		usage_cnt = atomic_read(&ccmni->usage);
		atomic_set(&ccmni_tmp->usage, usage_cnt);
		ccmni_tmp->tx_busy_cnt[0] = ccmni->tx_busy_cnt[0];
		ccmni_tmp->tx_busy_cnt[1] = ccmni->tx_busy_cnt[1];

		/* fix dev!=ccmni_irat->dev issue
		 * when MD3-CC3MNI -> MD3-CCMNI
		 */
		ccmni_irat->dev = dev;
		atomic_set(&ccmni_irat->usage, usage_cnt);
		memcpy(netdev_priv(dev), ccmni_irat,
			sizeof(struct ccmni_instance));

		CCMNI_DBG_MSG(md_id,
			"SIOCCCMNICFG: %s iRAT MD%d->MD%d, dev_cnt=%d, md_cnt=%d, md_irat_cnt=%d, irat_dev=%s\n",
			dev->name, (md_id + 1),
			(ifr->ifr_ifru.ifru_ivalue + 1),
			atomic_read(&ccmni->usage),
			atomic_read(&ccmni_tmp->usage),
			atomic_read(&ccmni_irat->usage),
			ccmni_irat->dev->name);
		break;

	case SIOCFWDFILTER:
		if (copy_from_user(&flt_act, ifr->ifr_ifru.ifru_data,
				sizeof(struct ccmni_flt_act))) {
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER: %s copy data from user fail\n",
				dev->name);
			return -EFAULT;
		}

		flt_tmp = flt_act.flt;
		if (flt_tmp.ver != 0x40 && flt_tmp.ver != 0x60) {
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER[%d]: %s invalid flt(%x, %x, %x, %x, %x)(%d)\n",
				flt_act.action, dev->name,
				flt_tmp.ver, flt_tmp.s_pref,
				flt_tmp.d_pref, flt_tmp.ipv4.saddr,
				flt_tmp.ipv4.daddr, ccmni->flt_cnt);
			return -EINVAL;
		}

		if (flt_act.action == CCMNI_FLT_ADD) { /* add new filter */
			if (ccmni->flt_cnt >= CCMNI_FLT_NUM) {
				CCMNI_INF_MSG(ccmni->md_id,
					"SIOCFWDFILTER[ADD]: %s flt table full\n",
					dev->name);
				return -ENOMEM;
			}
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					break;
			}
			if (i < CCMNI_FLT_NUM) {
				memcpy(&ccmni->flt_tbl[i], &flt_tmp,
					sizeof(struct ccmni_fwd_filter));
				ccmni->flt_cnt++;
			}
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER[ADD]: %s add flt%d(%x, %x, %x, %x, %x)(%d)\n",
				dev->name, i, flt_tmp.ver,
				flt_tmp.s_pref, flt_tmp.d_pref,
				flt_tmp.ipv4.saddr, flt_tmp.ipv4.daddr,
				ccmni->flt_cnt);
		} else if (flt_act.action == CCMNI_FLT_DEL) {
			if (flt_tmp.ver == IPV4_VERSION)
				cmp_len = offsetof(struct ccmni_fwd_filter,
							ipv4.daddr) + 4;
			else
				cmp_len = sizeof(struct ccmni_fwd_filter);
			for (i = 0; i < CCMNI_FLT_NUM; i++) {
				if (ccmni->flt_tbl[i].ver == 0)
					continue;
				if (!memcmp(&ccmni->flt_tbl[i],
						&flt_tmp, cmp_len)) {
					CCMNI_INF_MSG(ccmni->md_id,
						"SIOCFWDFILTER[DEL]: %s del flt%d(%x, %x, %x, %x, %x)(%d)\n",
						dev->name, i, flt_tmp.ver,
						flt_tmp.s_pref, flt_tmp.d_pref,
						flt_tmp.ipv4.saddr,
						flt_tmp.ipv4.daddr,
						ccmni->flt_cnt);
					memset(
						&ccmni->flt_tbl[i],
						0,
						sizeof(struct ccmni_fwd_filter)
					);
					ccmni->flt_cnt--;
					break;
				}
			}
			if (i >= CCMNI_FLT_NUM) {
				CCMNI_INF_MSG(ccmni->md_id,
					"SIOCFWDFILTER[DEL]: %s no match flt(%x, %x, %x, %x, %x)(%d)\n",
					dev->name, flt_tmp.ver,
					flt_tmp.s_pref, flt_tmp.d_pref,
					flt_tmp.ipv4.saddr,
					flt_tmp.ipv4.daddr,
					ccmni->flt_cnt);
				return -ENXIO;
			}
		} else if (flt_act.action == CCMNI_FLT_FLUSH) {
			ccmni->flt_cnt = 0;
			memset(ccmni->flt_tbl, 0,
			CCMNI_FLT_NUM*sizeof(struct ccmni_fwd_filter));
			CCMNI_INF_MSG(ccmni->md_id,
				"SIOCFWDFILTER[FLUSH]: %s flush filter\n",
				dev->name);
		}
		break;

	case SIOCACKPRIO:
		/* ifru_ivalue[3~0]: enable/disable ack prio feature  */
		ctlb = ccmni_ctl_blk[ccmni->md_id];
		if ((ifr->ifr_ifru.ifru_ivalue & 0xF) == 0) {
			for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				ccmni_tmp->ack_prio_en = 0;
			}
		} else {
			for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
				ccmni_tmp = ctlb->ccmni_inst[i];
				if (ccmni_tmp->ch.multiq)
					ccmni_tmp->ack_prio_en = 1;
			}
		}
		CCMNI_INF_MSG(ccmni->md_id,
			"SIOCACKPRIO: ack_prio_en=%d, ccmni0_ack_en=%d\n",
			ifr->ifr_ifru.ifru_ivalue,
			ctlb->ccmni_inst[i]->ack_prio_en);
		break;

	default:
		CCMNI_DBG_MSG(ccmni->md_id,
			"%s: unknown ioctl cmd=%x\n", dev->name, cmd);
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
#ifdef ENABLE_WQ_GRO
	return 0;
#else
	struct ccmni_instance *ccmni =
		(struct ccmni_instance *)netdev_priv(napi->dev);
	int md_id = ccmni->md_id;
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];

	del_timer(ccmni->timer);

	if (ctlb->ccci_ops->napi_poll)
		return ctlb->ccci_ops->napi_poll(md_id, ccmni->index,
					napi, budget);
	else
		return 0;
#endif
}

static void ccmni_napi_poll_timeout(unsigned long data)
{
	struct ccmni_instance *ccmni = (struct ccmni_instance *)data;

	CCMNI_DBG_MSG(ccmni->md_id,
		"CCMNI%d lost NAPI polling\n", ccmni->index);
}


/********************ccmni driver register  ccci function********************/
static inline int ccmni_inst_init(int md_id, struct ccmni_instance *ccmni,
	struct net_device *dev)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];
	int ret = 0;

	ret = ctlb->ccci_ops->get_ccmni_ch(md_id, ccmni->index, &ccmni->ch);
	if (ret) {
		CCMNI_PR_DBG(md_id,
			"get ccmni%d channel fail\n",
			ccmni->index);
		return ret;
	}

	ccmni->dev = dev;
	ccmni->ctlb = ctlb;
	ccmni->md_id = md_id;
	ccmni->napi = kzalloc(sizeof(struct napi_struct), GFP_KERNEL);
	ccmni->timer = kzalloc(sizeof(struct timer_list), GFP_KERNEL);
	ccmni->spinlock = kzalloc(sizeof(spinlock_t), GFP_KERNEL);
	ccmni->ack_prio_en = ccmni->ch.multiq ? 1 : 0;

	/* register napi device */
	if (dev && (ctlb->ccci_ops->md_ability & MODEM_CAP_NAPI)) {
		init_timer(ccmni->timer);
		ccmni->timer->function = ccmni_napi_poll_timeout;
		ccmni->timer->data = (unsigned long)ccmni;
		netif_napi_add(dev, ccmni->napi, ccmni_napi_poll,
			ctlb->ccci_ops->napi_poll_weigh);
	}
#ifdef ENABLE_WQ_GRO
	if (dev)
		netif_napi_add(dev, ccmni->napi, ccmni_napi_poll,
			ctlb->ccci_ops->napi_poll_weigh);
#endif

	atomic_set(&ccmni->usage, 0);
	spin_lock_init(ccmni->spinlock);

	return ret;
}

static inline void ccmni_dev_init(int md_id, struct net_device *dev)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];

	dev->header_ops = NULL;
	dev->mtu = CCMNI_MTU;
	dev->tx_queue_len = CCMNI_TX_QUEUE;
	dev->watchdog_timeo = CCMNI_NETDEV_WDT_TO;
	/* ccmni is a pure IP device */
	dev->flags = IFF_NOARP &
			(~IFF_BROADCAST & ~IFF_MULTICAST);
	/* not support VLAN */
	dev->features = NETIF_F_VLAN_CHALLENGED;
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
		 * check gro_list_prepare,
		 * GRO needs hard_header_len == ETH_HLEN.
		 * CCCI header can use ethernet header and
		 * padding bytes' region.
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
}

static int ccmni_init(int md_id, struct ccmni_ccci_ops *ccci_info)
{
	int i = 0, j = 0, ret = 0;
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_ctl_block *ctlb_irat_src = NULL;
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_irat_src = NULL;
	struct net_device *dev = NULL;

	if (unlikely(ccci_info->md_ability & MODEM_CAP_CCMNI_DISABLE)) {
		CCMNI_PR_DBG(md_id, "no need init ccmni: md_ability=0x%08X\n",
			ccci_info->md_ability);
		return 0;
	}

	ctlb = kzalloc(sizeof(struct ccmni_ctl_block), GFP_KERNEL);
	if (unlikely(ctlb == NULL)) {
		CCMNI_PR_DBG(md_id, "alloc ccmni ctl struct fail\n");
		return -ENOMEM;
	}

	ctlb->ccci_ops = kzalloc(sizeof(struct ccmni_ccci_ops), GFP_KERNEL);
	if (unlikely(ctlb->ccci_ops == NULL)) {
		CCMNI_PR_DBG(md_id, "alloc ccmni_ccci_ops struct fail\n");
		ret = -ENOMEM;
		goto alloc_mem_fail;
	}

#if defined(CCMNI_MET_DEBUG)
	if (met_tag_init() != 0)
		CCMNI_INF_MSG(md_id, "ccmni_init:met tag init fail\n");
#endif

	ccmni_ctl_blk[md_id] = ctlb;

	memcpy(ctlb->ccci_ops, ccci_info, sizeof(struct ccmni_ccci_ops));

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			/* allocate netdev */
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ)
				/* alloc multiple tx queue, 2 txq and 1 rxq */
				dev =
				alloc_etherdev_mqs(
						sizeof(struct ccmni_instance),
						2,
						1);
			else
				dev =
				alloc_etherdev(sizeof(struct ccmni_instance));
			if (unlikely(dev == NULL)) {
			CCMNI_PR_DBG(md_id, "alloc netdev fail\n");
				ret = -ENOMEM;
				goto alloc_netdev_fail;
			}

			/* init net device */
			ccmni_dev_init(md_id, dev);
			dev->type = ARPHRD_PPP;

			sprintf(dev->name, "%s%d", ctlb->ccci_ops->name, i);

			/* init private structure of netdev */
			ccmni = netdev_priv(dev);
			ccmni->index = i;
			ret = ccmni_inst_init(md_id, ccmni, dev);
			if (ret) {
				CCMNI_PR_DBG(md_id,
					"initial ccmni instance fail\n");
				goto alloc_netdev_fail;
			}
			ctlb->ccmni_inst[i] = ccmni;

			/* register net device */
			ret = register_netdev(dev);
			if (ret)
				goto alloc_netdev_fail;
			}


	if ((ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_IRAT) != 0) {
		if (ctlb->ccci_ops->irat_md_id < 0 ||
				ctlb->ccci_ops->irat_md_id >= MAX_MD_NUM) {
			CCMNI_PR_DBG(md_id,
				"md%d IRAT fail: invalid irat md(%d)\n",
				md_id, ctlb->ccci_ops->irat_md_id);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		ctlb_irat_src = ccmni_ctl_blk[ctlb->ccci_ops->irat_md_id];
		if (!ctlb_irat_src) {
			CCMNI_PR_DBG(md_id,
					"md%d IRAT fail: irat md%d ctlb is NULL\n",
					md_id, ctlb->ccci_ops->irat_md_id);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		if (unlikely(ctlb->ccci_ops->ccmni_num >
				ctlb_irat_src->ccci_ops->ccmni_num)) {
			CCMNI_PR_DBG(md_id,
			"IRAT fail: ccmni number not match(%d, %d)\n",
			ctlb_irat_src->ccci_ops->ccmni_num,
			ctlb->ccci_ops->ccmni_num);
			ret = -EINVAL;
			goto alloc_mem_fail;
		}

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			ccmni = ctlb->ccmni_inst[i];
			ccmni_irat_src = kzalloc(sizeof(struct ccmni_instance),
								GFP_KERNEL);
			if (unlikely(ccmni_irat_src == NULL)) {
				CCMNI_PR_DBG(md_id,
					"alloc ccmni_irat instance fail\n");
				kfree(ccmni);
				ret = -ENOMEM;
				goto alloc_mem_fail;
			}

			/* initial irat ccmni instance */
			dev = ctlb_irat_src->ccmni_inst[i]->dev;
			/* initial irat source ccmni instance */
			memcpy(ccmni_irat_src, ctlb_irat_src->ccmni_inst[i],
			sizeof(struct ccmni_instance));
			ctlb_irat_src->ccmni_inst[i] = ccmni_irat_src;
		}
	}

	if (ctlb->ccci_ops->md_ability & MODEM_CAP_DIRECT_TETHERING) {
		if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ)
			/*alloc multiple tx queue, 2 txq and 1 rxq */
			dev = alloc_etherdev_mqs(
					sizeof(struct ccmni_instance),
					2,
					1);
		else
			dev = alloc_etherdev(sizeof(struct ccmni_instance));
		if (unlikely(dev == NULL)) {
			CCMNI_PR_DBG(md_id, "alloc netdev fail\n");
			ret = -ENOMEM;
			goto alloc_netdev_fail;
		}
		/*init net device */
		ccmni_dev_init(md_id, dev);
		/*ccmni-lan packet displays correct in netlog */
		dev->header_ops = &eth_header_ops;
		/*ccmni-lan need handle ARP packet */
		dev->flags = IFF_BROADCAST | IFF_MULTICAST;
		sprintf(dev->name, "ccmni-lan");

		/*init private structure of netdev */
		ccmni = netdev_priv(dev);
		ccmni->index = i;
		ret = ccmni_inst_init(md_id, ccmni, dev);
		if (ret)
			goto alloc_netdev_fail;

		ctlb->ccmni_inst[i] = ccmni;

		/*register net device */
		ret = register_netdev(dev);
		if (ret) {
			CCMNI_PR_DBG(md_id,
				"CCMNI%d register netdev fail: %d\n",
				i,
				ret);
			goto alloc_netdev_fail;
		}
	}
	snprintf(ctlb->wakelock_name, sizeof(ctlb->wakelock_name),
			"ccmni_md%d", (md_id + 1));
	wakeup_source_init(&ctlb->ccmni_wakelock, ctlb->wakelock_name);

	return 0;

alloc_netdev_fail:
	if (dev) {
		free_netdev(dev);
		ctlb->ccmni_inst[i] = NULL;
	}

	for (j = i - 1; j >= 0; j--) {
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
	struct ccmni_ctl_block *ctlb = NULL;
	struct ccmni_instance *ccmni = NULL;

	CCMNI_DBG_MSG(md_id, "ccmni_exit\n");

	ctlb = ccmni_ctl_blk[md_id];
	if (ctlb) {
		if (ctlb->ccci_ops == NULL)
			goto ccmni_exit_ret;

		for (i = 0; i < ctlb->ccci_ops->ccmni_num; i++) {
			ccmni = ctlb->ccmni_inst[i];
			if (ccmni) {
				CCMNI_DBG_MSG(md_id,
					"ccmni_exit: unregister ccmni%d dev\n",
					i);
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

static int ccmni_rx_callback(int md_id, int ccmni_idx, struct sk_buff *skb,
		void *priv_data)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];
	/* struct ccci_header *ccci_h = (struct ccci_header*)skb->data; */
	struct ccmni_instance *ccmni = NULL;
	struct net_device *dev = NULL;
	int pkt_type, skb_len;
#if defined(CCCI_SKB_TRACE)
	struct iphdr *iph;
#endif
#if defined(CCMNI_MET_DEBUG)
	char tag_name[32] = { '\0' };
	unsigned int tag_id = 0;
#endif

	if (unlikely(ctlb == NULL || ctlb->ccci_ops == NULL)) {
		CCMNI_PR_DBG(md_id,
			"invalid CCMNI%d ctrl/ops struct\n",
			ccmni_idx);
		dev_kfree_skb(skb);
		return -1;
	}

	ccmni = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni->dev;

	pkt_type = skb->data[0] & 0xF0;
	ccmni_make_etherframe(md_id, dev, skb->data - ETH_HLEN, dev->dev_addr,
		pkt_type);
	skb_set_mac_header(skb, -ETH_HLEN);
	skb_reset_network_header(skb);
	skb->dev = dev;
	if (pkt_type == 0x60)
		skb->protocol  = htons(ETH_P_IPV6);
	else
		skb->protocol  = htons(ETH_P_IP);

	skb->ip_summed = CHECKSUM_NONE;
	skb_len = skb->len;

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
		if (is_skb_gro(skb)) {
			preempt_disable();
			spin_lock_bh(ccmni->spinlock);
			napi_gro_receive(ccmni->napi, skb);
			ccmni_gro_flush(ccmni);
			spin_unlock_bh(ccmni->spinlock);
			preempt_enable();
		} else {
			netif_rx_ni(skb);
		}
#else
		if (!in_interrupt())
			netif_rx_ni(skb);
		else
			netif_rx(skb);
#endif
	}
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb_len;

#if defined(CCMNI_MET_DEBUG)
	if (ccmni->rx_met_time == 0) {
		ccmni->rx_met_time = jiffies;
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
	} else if (time_after_eq(jiffies,
		ccmni->rx_met_time + msecs_to_jiffies(MET_LOG_TIMER))) {
		snprintf(tag_name, 32, "%s_rx_bytes", dev->name);
		tag_id = CCMNI_RX_MET_ID + ccmni_idx;
		met_tag_oneshot(tag_id, tag_name,
			(dev->stats.rx_bytes - ccmni->rx_met_bytes));
		ccmni->rx_met_bytes = dev->stats.rx_bytes;
		ccmni->rx_met_time = jiffies;
	}
#endif

	__pm_wakeup_event(&ctlb->ccmni_wakelock, jiffies_to_msecs(HZ));

	return 0;
}

static void ccmni_queue_state_callback(int md_id, int ccmni_idx,
	enum HIF_STATE state, int is_ack)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	struct netdev_queue *net_queue = NULL;

	if (unlikely(ctlb == NULL)) {
		CCMNI_DBG_MSG(md_id,
			"invalid ccmni ctrl when ccmni%d_hif_sta=%d\n",
			ccmni_idx, state);
		return;
	}

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni_tmp->dev;
	ccmni = (struct ccmni_instance *)netdev_priv(dev);

	switch (state) {
#ifdef ENABLE_WQ_GRO
	case RX_FLUSH:
		preempt_disable();
		spin_lock_bh(ccmni->spinlock);
		ccmni->rx_gro_cnt++;
		napi_gro_flush(ccmni->napi, false);
		spin_unlock_bh(ccmni->spinlock);
		preempt_enable();
		break;
#else
	case RX_IRQ:
		mod_timer(ccmni->timer, jiffies + HZ);
		napi_schedule(ccmni->napi);
		__pm_wakeup_event(&ctlb->ccmni_wakelock, jiffies_to_msecs(HZ));
		break;
#endif

	case TX_IRQ:
		if (netif_running(ccmni->dev) &&
				atomic_read(&ccmni->usage) > 0) {
			if (likely(ctlb->ccci_ops->md_ability &
					MODEM_CAP_CCMNI_MQ)) {
				if (is_ack)
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_FAST);
				else
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_NORMAL);
				if (netif_tx_queue_stopped(net_queue))
					netif_tx_wake_queue(net_queue);
			} else {
				is_ack = 0;
				if (netif_queue_stopped(ccmni->dev))
					netif_wake_queue(ccmni->dev);
			}
			ccmni->tx_irq_cnt[is_ack]++;
			if ((ccmni->flags[is_ack] & CCMNI_TX_PRINT_F) ||
				time_after(jiffies,
					ccmni->tx_irq_tick[is_ack] + 2)) {
				ccmni->flags[is_ack] &= ~CCMNI_TX_PRINT_F;
				CCMNI_INF_MSG(md_id,
					"%s(%d), idx=%d, md_sta=TX_IRQ, ack=%d, cnt(%u, %u), time=%lu\n",
					ccmni->dev->name,
					atomic_read(&ccmni->usage),
					ccmni->index,
					is_ack, ccmni->tx_full_cnt[is_ack],
					ccmni->tx_irq_cnt[is_ack],
					(jiffies - ccmni->tx_irq_tick[is_ack]));
			}
		}
		break;

	case TX_FULL:
		if (atomic_read(&ccmni->usage) > 0) {
			if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
				if (is_ack)
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_FAST);
				else
					net_queue =
						netdev_get_tx_queue(ccmni->dev,
							CCMNI_TXQ_NORMAL);
				netif_tx_stop_queue(net_queue);
			} else {
				is_ack = 0;
				netif_stop_queue(ccmni->dev);
			}
			ccmni->tx_full_cnt[is_ack]++;
			ccmni->tx_irq_tick[is_ack] = jiffies;
			if (time_after(jiffies,
					ccmni->tx_full_tick[is_ack] + 4)) {
				ccmni->tx_full_tick[is_ack] = jiffies;
				ccmni->flags[is_ack] |= CCMNI_TX_PRINT_F;
				CCMNI_DBG_MSG(md_id,
					"%s(%d), idx=%d, hif_sta=TX_FULL, ack=%d, cnt(%u, %u)\n",
					ccmni->dev->name,
					atomic_read(&ccmni->usage),
					ccmni->index,
					is_ack, ccmni->tx_full_cnt[is_ack],
					ccmni->tx_irq_cnt[is_ack]);
			}
		}
		break;
	default:
		break;
	}
}

static void ccmni_md_state_callback(int md_id, int ccmni_idx,
	enum MD_STATE state)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
	struct net_device *dev = NULL;
	int i = 0;

	if (unlikely(ctlb == NULL)) {
		CCMNI_DBG_MSG(md_id,
			"invalid ccmni ctrl when ccmni%d_md_sta=%d\n",
			ccmni_idx, state);
		return;
	}

	ccmni_tmp = ctlb->ccmni_inst[ccmni_idx];
	dev = ccmni_tmp->dev;
	ccmni = (struct ccmni_instance *)netdev_priv(dev);

	if (atomic_read(&ccmni->usage) > 0)
		CCMNI_DBG_MSG(md_id,
			"md_state_cb: CCMNI%d, md_sta=%d, usage=%d\n",
			ccmni_idx, state, atomic_read(&ccmni->usage));

	switch (state) {
	case READY:
		/* Only do carrier on for ccmni-lan.
		 * don't carrire on other interface,
		 * MD data link may be not ready.
		 * carrirer on it in ccmni_open
		 */
		if (IS_CCMNI_LAN(ccmni->dev)) {
			netif_tx_start_all_queues(ccmni->dev);
			netif_carrier_on(ccmni->dev);
		}

		for (i = 0; i < 2; i++) {
			ccmni->tx_seq_num[i] = 0;
			ccmni->tx_full_cnt[i] = 0;
			ccmni->tx_irq_cnt[i] = 0;
			ccmni->tx_full_tick[i] = 0;
			ccmni->flags[i] &= ~CCMNI_TX_PRINT_F;
		}
		ccmni->rx_seq_num = 0;
		ccmni->rx_gro_cnt = 0;
		break;

	case EXCEPTION:
	case RESET:
	case WAITING_TO_STOP:
		netif_tx_disable(ccmni->dev);
		netif_carrier_off(ccmni->dev);
		break;
	default:
		break;
	}
}

static void ccmni_dump(int md_id, int ccmni_idx, unsigned int flag)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];
	struct ccmni_instance *ccmni = NULL;
	struct ccmni_instance *ccmni_tmp = NULL;
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

	if ((ccmni_tmp->dev->stats.rx_packets == 0) &&
			(ccmni_tmp->dev->stats.tx_packets == 0))
		return;

	dev = ccmni_tmp->dev;
	/* ccmni diff from ccmni_tmp for MD IRAT */
	ccmni = (struct ccmni_instance *)netdev_priv(dev);
	dev_queue = netdev_get_tx_queue(dev, 0);
	if (ctlb->ccci_ops->md_ability & MODEM_CAP_CCMNI_MQ) {
		ack_queue = netdev_get_tx_queue(dev, CCMNI_TXQ_FAST);
		qdisc = dev_queue->qdisc;
		ack_qdisc = ack_queue->qdisc;
		/* stats.rx_dropped is dropped in ccmni,
		 * dev->rx_dropped is dropped in net device layer
		 */
		/* stats.tx_packets is count by ccmni, bstats.
		 * packets is count by qdisc in net device layer
		 */
		CCMNI_INF_MSG(md_id,
			      "%s(%d,%d), irat_MD%d, rx=(%ld,%ld,%d), tx=(%ld,%d,%d), txq_len=(%d,%d), tx_drop=(%ld,%d,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx,0x%lx)\n",
				  dev->name,
				  atomic_read(&ccmni->usage),
				  atomic_read(&ccmni_tmp->usage),
				  (ccmni->md_id + 1),
			      dev->stats.rx_packets,
				  dev->stats.rx_bytes,
				  ccmni->rx_gro_cnt,
			      dev->stats.tx_packets, qdisc->bstats.packets,
				  ack_qdisc->bstats.packets,
			      qdisc->q.qlen, ack_qdisc->q.qlen,
			      dev->stats.tx_dropped, qdisc->qstats.drops,
				  ack_qdisc->qstats.drops,
			      dev->stats.rx_dropped,
				  atomic_long_read(&dev->rx_dropped),
			      ccmni->tx_busy_cnt[0], ccmni->tx_busy_cnt[1],
			      dev->state, dev->flags, dev_queue->state,
				  ack_queue->state);
	} else
		CCMNI_INF_MSG(md_id,
			      "%s(%d,%d), irat_MD%d, rx=(%ld,%ld,%d), tx=(%ld,%ld), txq_len=%d, tx_drop=(%ld,%d), rx_drop=(%ld,%ld), tx_busy=(%ld,%ld), sta=(0x%lx,0x%x,0x%lx)\n",
			      dev->name, atomic_read(&ccmni->usage),
				  atomic_read(&ccmni_tmp->usage),
						(ccmni->md_id + 1),
			      dev->stats.rx_packets, dev->stats.rx_bytes,
				  ccmni->rx_gro_cnt,
			      dev->stats.tx_packets, dev->stats.tx_bytes,
			      dev->qdisc->q.qlen, dev->stats.tx_dropped,
				  dev->qdisc->qstats.drops,
			      dev->stats.rx_dropped,
				  atomic_long_read(&dev->rx_dropped),
				  ccmni->tx_busy_cnt[0],
			      ccmni->tx_busy_cnt[1], dev->state, dev->flags,
				  dev_queue->state);
}

static void ccmni_dump_rx_status(int md_id, unsigned long long *status)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];

	status[0] = ctlb->net_rx_delay[0];
	status[1] = ctlb->net_rx_delay[1];
	status[2] = ctlb->net_rx_delay[2];
}

static struct ccmni_ch *ccmni_get_ch(int md_id, int ccmni_idx)
{
	struct ccmni_ctl_block *ctlb = ccmni_ctl_blk[md_id];

	return &ctlb->ccmni_inst[ccmni_idx]->ch;
}

struct ccmni_dev_ops ccmni_ops = {
	.skb_alloc_size = 1600,
	.init = &ccmni_init,
	.rx_callback = &ccmni_rx_callback,
	.md_state_callback = &ccmni_md_state_callback,
	.queue_state_callback = &ccmni_queue_state_callback,
	.exit = ccmni_exit,
	.dump = ccmni_dump,
	.dump_rx_status = ccmni_dump_rx_status,
	.get_ch = ccmni_get_ch,
	.is_ack_skb = is_ack_skb,
};
