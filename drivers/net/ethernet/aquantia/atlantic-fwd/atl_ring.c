// SPDX-License-Identifier: GPL-2.0-only
/* Atlantic Network Driver
 *
 * Copyright (C) 2017 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "atl_ring.h"
#include <linux/skbuff.h>
#include <linux/ipv6.h>
#include <net/ip.h>
#include <linux/tcp.h>
#include <linux/if_vlan.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/cpu.h>
#include <uapi/linux/ip.h>
#include <uapi/linux/tcp.h>
#include <uapi/linux/udp.h>

#include "atl_trace.h"
#include "atl_fwdnl.h"
#include "atl_hw_ptp.h"

static inline uint32_t fetch_tx_head(struct atl_desc_ring *ring)
{
#ifdef ATL_TX_HEAD_WB
	//XXX
#else
	return atl_read(ring_hw(ring), ATL_TX_RING_HEAD(ring));
#endif
}

int atl_tx_full(struct atl_desc_ring *ring, int needed)
{
	struct atl_nic *nic = ring->nic;

	if (likely(ring_space(ring) >= needed))
		return 0;

	netif_stop_subqueue(nic->ndev, ring->qvec->idx);
	atl_nic_dbg("Stopping tx queue\n");

	smp_mb();

	// Check if another CPU freed some space
	if (likely(ring_space(ring) < needed))
		return -EAGAIN;

	netif_start_subqueue(nic->ndev, ring->qvec->idx);
	atl_nic_dbg("Restarting tx queue in %s...\n", __func__);
	atl_update_ring_stat(ring, tx.tx_restart, 1);
	return 0;
}

static void atl_txbuf_free(struct atl_txbuf *txbuf, struct device *dev,
	uint32_t idx)
{
	if (txbuf->skb) {
		if (dma_unmap_len(txbuf, len)) {
			dma_unmap_single(dev, dma_unmap_addr(txbuf, daddr),
					 dma_unmap_len(txbuf, len),
					 DMA_TO_DEVICE);
			trace_atl_dma_unmap_head(-1, idx,
				dma_unmap_addr(txbuf, daddr),
				dma_unmap_len(txbuf, len),
				txbuf->skb);
		}
		dev_kfree_skb_any(txbuf->skb);
	} else if (dma_unmap_len(txbuf, len)) {
		dma_unmap_page(dev, dma_unmap_addr(txbuf, daddr),
			       dma_unmap_len(txbuf, len),
			       DMA_TO_DEVICE);
		trace_atl_dma_unmap_frag(-1, idx, dma_unmap_addr(txbuf, daddr),
			dma_unmap_len(txbuf, len), txbuf->skb);
	}

	txbuf->last = -1;
	txbuf->skb = NULL;
	dma_unmap_len_set(txbuf, len, 0);
}

static inline struct netdev_queue *atl_txq(struct atl_desc_ring *ring)
{
	return netdev_get_tx_queue(ring->nic->ndev, ring->qvec->idx);
}

unsigned int atl_tx_free_low = MAX_SKB_FRAGS + 4;
module_param_named(tx_free_low, atl_tx_free_low, uint, 0644);

unsigned int atl_tx_free_high = MAX_SKB_FRAGS * 3;
module_param_named(tx_free_high, atl_tx_free_high, uint, 0644);

static netdev_tx_t atl_map_xmit_skb(struct sk_buff *skb,
	struct atl_desc_ring *ring, struct atl_txbuf *first_buf)
{
	int idx = ring->tail;
	struct device *dev = &ring->nic->hw.pdev->dev;
	struct atl_tx_desc *desc = &ring->desc.tx;
	skb_frag_t *frag;
	/* Header's DMA mapping must be stored in the txbuf that has
	 * ->skb set, even if it corresponds to the context
	 * descriptor and not the first data descriptor
	 */
	struct atl_txbuf *txbuf = first_buf;
	unsigned int len = skb_headlen(skb);
	unsigned int frags = skb_shinfo(skb)->nr_frags;
	dma_addr_t daddr = dma_map_single(dev, skb->data, len,
					  DMA_TO_DEVICE);
	trace_atl_dma_map_head(-1, idx, daddr, len, skb, skb->data);

	for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
		if (dma_mapping_error(dev, daddr))
			goto err_dma;

		dma_unmap_len_set(txbuf, len, len);
		dma_unmap_addr_set(txbuf, daddr, daddr);

		desc->daddr = cpu_to_le64(daddr);
		while (len > ATL_DATA_PER_TXD) {
			desc->len = cpu_to_le16(ATL_DATA_PER_TXD);
			trace_atl_tx_descr(ring->qvec->idx, idx, (u64 *)desc);
			ring->hw.descs[idx].tx = *desc;
			bump_ptr(idx, ring, 1);
			daddr += ATL_DATA_PER_TXD;
			len -= ATL_DATA_PER_TXD;
			desc->daddr = cpu_to_le64(daddr);
		}
		desc->len = cpu_to_le16(len);

		if (!frags)
			break;

		trace_atl_tx_descr(ring->qvec->idx, idx, (u64 *)desc);
		ring->hw.descs[idx].tx = *desc;
		bump_ptr(idx, ring, 1);
		txbuf = &ring->txbufs[idx];
		len = skb_frag_size(frag);
		daddr = skb_frag_dma_map(dev, frag, 0, len,
					 DMA_TO_DEVICE);
		trace_atl_dma_map_frag(frag - &skb_shinfo(skb)->frags[0], idx,
				       daddr, len, skb, skb_frag_address(frag));

		frags--;
	}

	//Last descriptor
	desc->eop = 1;
#if defined(ATL_TX_DESC_WB) || defined(ATL_TX_HEAD_WB)
	desc->cmd |= tx_desc_cmd_wb;
#endif
	trace_atl_tx_descr(ring->qvec->idx, idx, (u64 *)desc);
	ring->hw.descs[idx].tx = *desc;
	first_buf->last = idx;
	bump_ptr(idx, ring, 1);
	ring->txbufs[idx].last = -1;
	ring->tail = idx;

	/* Stop queue if no space for another packet */
	atl_tx_full(ring, atl_tx_free_low);

	/* Delay bumping the HW tail if another packet is pending and
	 * there's space for it.
	 */
	if (skb_xmit_more(skb) && !netif_xmit_stopped(atl_txq(ring)))
		return NETDEV_TX_OK;

	wmb();
	atl_write(ring_hw(ring), ATL_TX_RING_TAIL(ring), ring->tail);

	return NETDEV_TX_OK;

err_dma:
	dev_err(dev, "atl_map_skb failed\n");
	for (;;) {
		atl_txbuf_free(txbuf, dev, idx);
		if (txbuf == first_buf)
			break;
		bump_ptr(idx, ring, -1);
		txbuf = &ring->txbufs[idx];
	}
	ring->tail = idx;
	atl_update_ring_stat(ring, tx.dma_map_failed, 1);
	return -EFAULT;
}

static uint32_t atl_insert_context(struct atl_txbuf *txbuf,
	struct atl_desc_ring *ring, unsigned int *len)
{
	struct sk_buff *skb = txbuf->skb;
	struct atl_tx_ctx *ctx;
	unsigned int hdr_len;
	uint32_t tx_cmd = 0;
	int mss;
	DECLARE_SCRATCH_DESC(scratch);

	ctx = &DESC_PTR(ring, ring->tail, scratch)->ctx;

	memset(ctx, 0, sizeof(*ctx));

	txbuf->bytes = *len;
	txbuf->packets = 1;

	mss = skb_shinfo(skb)->gso_size;

	if (mss && (skb_shinfo(skb)->gso_type &
		    (SKB_GSO_TCPV4 | SKB_GSO_TCPV6))) {
		tx_cmd |= tx_desc_cmd_lso | tx_desc_cmd_l4cs;
		ctx->mss_len = mss;
		ctx->cmd = ctx_cmd_tcp;

		ctx->l2_len = skb_network_offset(skb);

		if (skb_is_gso_v6(skb))
			ctx->cmd |= ctx_cmd_ipv6;

		ctx->l3_len = skb_transport_offset(skb) - ctx->l2_len;
		ctx->l4_len = tcp_hdrlen(skb);

		hdr_len = ctx->l2_len + ctx->l3_len + ctx->l4_len;

		*len -= hdr_len;
		txbuf->packets = skb_shinfo(skb)->gso_segs;
		txbuf->bytes += (txbuf->packets - 1) * hdr_len;
	}

	if (skb_vlan_tag_present(skb)) {
		tx_cmd |= tx_desc_cmd_vlan;
		ctx->vlan_tag = skb_vlan_tag_get(skb);
	}

	if (tx_cmd) {
		ctx->type = tx_desc_type_context;
		ctx->idx = 0;
		trace_atl_tx_context_descr(ring->qvec->idx, ring->tail, (u64 *)ctx);
		COMMIT_DESC(ring, ring->tail, scratch);
		bump_tail(ring, 1);
	}

	return tx_cmd;
}

netdev_tx_t atl_start_xmit(struct sk_buff *skb, struct net_device *ndev)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_desc_ring *ring = &nic->qvecs[skb->queue_mapping].tx;

	if (unlikely(nic->hw.link_state.ptp_datapath_up)) {
		/* Hardware adds the Timestamp for PTPv2 802.AS1
		 * and PTPv2 IPv4 UDP.
		 * We have to push even general 320 port messages to the ptp
		 * queue explicitly. This is a limitation of current firmware
		 * and hardware PTP design of the chip. Otherwise ptp stream
		 * will fail to sync
		 */
		if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP) ||
		    unlikely((ip_hdr(skb)->version == 4) &&
			     (ip_hdr(skb)->protocol == IPPROTO_UDP) &&
			     ((udp_hdr(skb)->dest == htons(319)) ||
			      (udp_hdr(skb)->dest == htons(320)))) ||
		    unlikely(eth_hdr(skb)->h_proto == htons(ETH_P_1588)))
			return atl_ptp_start_xmit(nic, skb);
	}

	skb_tx_timestamp(skb);
	if (nic->priv_flags & ATL_PF_BIT(LPB_NET_DMA))
		return NETDEV_TX_BUSY;

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	/* atl_max_queues is the number of standard queues.
	 * Extra queue is allocated for FWD processing.
	 */
	if (unlikely(skb->queue_mapping >= nic->nvecs))
		return atlfwd_nl_xmit(skb, ndev);
#endif

	if (atl_tx_full(ring, skb_shinfo(skb)->nr_frags + 4)) {
		atl_update_ring_stat(ring, tx.tx_busy, 1);
		return NETDEV_TX_BUSY;
	}

	return atl_map_skb(skb, ring);
}

netdev_tx_t atl_map_skb(struct sk_buff *skb, struct atl_desc_ring *ring)
{
	unsigned int len = skb->len;
	struct atl_tx_desc *desc;
	struct atl_txbuf *txbuf;
	uint32_t cmd_from_ctx;

	txbuf = &ring->txbufs[ring->tail];

	txbuf->skb = skb;
	cmd_from_ctx = atl_insert_context(txbuf, ring, &len);

	/* use ring->desc unconditionally as it will serve as a
	 * template for all descriptors
	 */
	desc = &ring->desc.tx;

	memset(desc, 0, sizeof(*desc));

	desc->cmd = cmd_from_ctx;
	desc->cmd |= tx_desc_cmd_fcs;
	desc->ct_en = !!cmd_from_ctx;
	desc->type = tx_desc_type_desc;

	desc->pay_len = len;

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		uint8_t l4_proto = 0;

		switch (skb->protocol) {
		case htons(ETH_P_IP):
			desc->cmd |= tx_desc_cmd_ipv4cs;
			l4_proto = ip_hdr(skb)->protocol;
			break;
		case htons(ETH_P_IPV6):
			l4_proto = ipv6_hdr(skb)->nexthdr;
			break;
		}

		switch (l4_proto) {
		case IPPROTO_TCP:
		case IPPROTO_UDP:
			desc->cmd |= tx_desc_cmd_l4cs;
			break;
		}
	}

	return atl_map_xmit_skb(skb, ring, txbuf);
}

unsigned int atl_tx_clean_budget = 256;
module_param_named(tx_clean_budget, atl_tx_clean_budget, uint, 0644);

// Returns true if all work done
static bool atl_clean_tx(struct atl_desc_ring *ring)
{
	struct atl_nic *nic = ring->nic;
	struct device *dev = &nic->hw.pdev->dev;
	uint32_t first = READ_ONCE(ring->head);
#ifndef ATL_TX_DESC_WB
	uint32_t done = atl_get_tx_head(ring);
#endif
	uint32_t budget = atl_tx_clean_budget;
	unsigned int bytes = 0, packets = 0;
	struct atl_tx_desc *last_desc;

	atl_nic_dbg("descs in ring: %d\n", ring_occupied(ring));
	do {
		struct atl_txbuf *txbuf = &ring->txbufs[first];
		struct sk_buff *skb = txbuf->skb;
		uint32_t last = txbuf->last;

		if (last == -1)
			break;

#ifdef ATL_TX_DESC_WB
		last_desc = &ring->hw.descs[last].tx;

		if (!last_desc->dd)
			break;
#else
		if ((first <= last && done >= first && done <= last) ||
		    ((first > last) && (done >= first || done <= last)))
			break;
#endif

		bump_ptr(last, ring, 1);
		napi_consume_skb(txbuf->skb, budget);
		trace_atl_dma_unmap_head(-1, first,
					 dma_unmap_addr(txbuf, daddr),
					 dma_unmap_len(txbuf, len), skb);

		txbuf->skb = NULL;
		txbuf->last = -1;
		dma_unmap_single(dev, dma_unmap_addr(txbuf, daddr),
				 dma_unmap_len(txbuf, len), DMA_TO_DEVICE);
		dma_unmap_len_set(txbuf, len, 0);

		bytes += txbuf->bytes;
		packets += txbuf->packets;

		for (bump_ptr(first, ring, 1); first != last;
		     bump_ptr(first, ring, 1)) {
			txbuf = &ring->txbufs[first];
			if (dma_unmap_len(txbuf, len)) {
				dma_unmap_page(dev,
					dma_unmap_addr(txbuf, daddr),
					dma_unmap_len(txbuf, len),
					DMA_TO_DEVICE);
				trace_atl_dma_unmap_frag(-1, first,
					dma_unmap_addr(txbuf, daddr),
					dma_unmap_len(txbuf, len), skb);
				dma_unmap_len_set(txbuf, len, 0);
			}
		}
	} while (--budget);

	if (likely(ring->qvec->type != ATL_QUEUE_PTP)) {
		u64_stats_update_begin(&ring->syncp);
		ring->stats.tx.bytes += bytes;
		ring->stats.tx.packets += packets;
		u64_stats_update_end(&ring->syncp);
	}

	ring->head = first;

	if (likely(ring->qvec->type != ATL_QUEUE_PTP) &&
	    ring_space(ring) > atl_tx_free_high) {
		struct net_device *ndev = nic->ndev;

		smp_mb();
		if (__netif_subqueue_stopped(ndev, ring->qvec->idx) &&
			test_bit(ATL_ST_RINGS_RUNNING, &nic->hw.state)) {
			atl_nic_dbg("restarting tx queue\n");
			netif_wake_subqueue(ndev, ring->qvec->idx);
			atl_update_ring_stat(ring, tx.tx_restart, 1);
		}
	}

	return !!budget;
}

/* work around HW bugs in checksum calculation:
 * - packets less than 60 octets
 * - ip, tcp or udp checksum is 0xFFFF
 * - non-zero padding
 */
static bool atl_checksum_workaround(struct sk_buff *skb,
				    struct atl_rx_desc_wb *desc)
{
	int ip_header_offset = 14;
	int l4_header_offset = 0;
	struct iphdr *ip;
	struct ipv6hdr *ipv6;
	struct tcphdr *tcp;
	struct udphdr *udp;

	if (desc->pkt_len <= 60)
		return true;

	if (((desc->pkt_type & atl_rx_pkt_type_vlan_msk) ==
	    atl_rx_pkt_type_vlan) &&
	    !(desc->rx_estat & atl_rx_estat_vlan_stripped)) 
		ip_header_offset += sizeof(struct vlan_hdr);

	if ((desc->pkt_type & atl_rx_pkt_type_vlan_msk) ==
	    atl_rx_pkt_type_dbl_vlan) {
	    	if (desc->rx_estat & atl_rx_estat_vlan_stripped)
			ip_header_offset += sizeof(struct vlan_hdr);
		else
			ip_header_offset += sizeof(struct vlan_hdr) * 2;
	}

	switch (desc->pkt_type & atl_rx_pkt_type_l3_msk) {
	case atl_rx_pkt_type_ipv4:
		ip = (struct iphdr *) &skb->data[ip_header_offset];

		if (ip->check == 0xFFFF)
			return true;
		l4_header_offset = ip->ihl << 2;
		/* padding inside Ethernet frame */
		if (ntohs(ip->tot_len) + ip_header_offset < desc->pkt_len)
			return true;
		break;
	case atl_rx_pkt_type_ipv6:
		ipv6 = (struct ipv6hdr *) &skb->data[ip_header_offset];
		l4_header_offset = sizeof(struct ipv6hdr);
		/* padding inside Ethernet frame */
		if (ip_header_offset + sizeof(struct ipv6hdr) +
		    ntohs(ipv6->payload_len) < desc->pkt_len)
			return true;
		break;
	default:
		return false;
	}

	switch (desc->pkt_type & atl_rx_pkt_type_l4_msk) {
	case atl_rx_pkt_type_tcp:
		tcp = (struct tcphdr *) &skb->data[ip_header_offset +
						  l4_header_offset];

		if (tcp->check == 0xFFFF)
			return true;
		break;
	case atl_rx_pkt_type_udp:
		udp = (struct udphdr *) &skb->data[ip_header_offset +
						  l4_header_offset];
		if (udp->check == 0xFFFF)
			return true;
		/* padding inside IP frame */
		if (l4_header_offset + ntohs(udp->len) < desc->pkt_len)
			return true;
		break;
	default:
		return false;
	}

	return false;
}

bool atl_rx_checksum(struct sk_buff *skb, struct atl_rx_desc_wb *desc,
		     struct atl_desc_ring *ring)
{
	struct atl_nic *nic = ring->nic;
	struct net_device *ndev = nic->ndev;
	int csum_ok = 1;

	skb_checksum_none_assert(skb);

	if (desc->rx_stat & atl_rx_stat_mac_err) {
		atl_update_ring_stat(ring, rx.mac_err, 1);
		atl_nic_dbg("rx MAC err: rx_stat %d pkt_type %d len %d\n",
			desc->rx_stat, desc->pkt_type, desc->pkt_len);
		goto drop;
	}

	if (!(ndev->features & NETIF_F_RXCSUM))
		return true;

	switch (desc->pkt_type & atl_rx_pkt_type_l3_msk) {
	case atl_rx_pkt_type_ipv4:
		csum_ok &= !(desc->rx_stat & atl_rx_stat_ipv4_err);
		/* Fallthrough */
	case atl_rx_pkt_type_ipv6:
		break;
	default:
		return true;
	}

	switch (desc->pkt_type & atl_rx_pkt_type_l4_msk) {
	case atl_rx_pkt_type_tcp:
	case atl_rx_pkt_type_udp:
		csum_ok &= !(desc->rx_stat & atl_rx_stat_l4_err);
		break;
	default:
		return true;
	}

	if (csum_ok) {
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		return true;
	} else {
		if (atl_checksum_workaround(skb, desc))
			return true;
	}

	atl_update_ring_stat(ring, rx.csum_err, 1);

	atl_nic_dbg("bad rx checksum: rx_stat %d pkt_type %d len %d\n",
		    desc->rx_stat, desc->pkt_type, desc->pkt_len);

	if (ndev->features & NETIF_F_RXALL)
		return true;

drop:
	dev_kfree_skb_any(skb);
	return false;
}

void atl_rx_hash(struct sk_buff *skb, struct atl_rx_desc_wb *desc,
		 struct net_device *ndev)
{
	uint8_t rss_type = desc->rss_type;

	if (!(ndev->features & NETIF_F_RXHASH) || rss_type < 2 || rss_type > 7)
		return;

	skb_set_hash(skb, le32_to_cpu(desc->rss_hash),
		(rss_type > 3 && rss_type < 8) ? PKT_HASH_TYPE_L4 :
		PKT_HASH_TYPE_L3);
}

static int atl_napi_receive_skb(struct atl_desc_ring *ring, struct sk_buff *skb)
{
	bool is_ptp_ring = atl_is_ptp_ring(ring->nic, ring);
	struct net_device *ndev = ring->nic->ndev;
	struct napi_struct *napi = &ring->qvec->napi;

	/* Send all PTP traffic to 0 queue */
	skb_record_rx_queue(skb, is_ptp_ring ? 0 : ring->qvec->idx);
	skb->protocol = eth_type_trans(skb, ndev);
	napi_gro_receive(napi, skb);

	return 0;
}

static bool atl_rx_packet(struct sk_buff *skb, struct atl_rx_desc_wb *desc,
			  struct atl_desc_ring *ring,
			  rx_skb_handler_t rx_skb_func)
{
	struct net_device *ndev = ring->nic->ndev;

	if (!atl_rx_checksum(skb, desc, ring))
		return false;

	if (!skb_is_nonlinear(skb) && eth_skb_pad(skb))
		return false;

	if ((ndev->features & NETIF_F_HW_VLAN_CTAG_RX) &&
	    (desc->rx_estat & atl_rx_estat_vlan_stripped)) {
		__vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q),
				       le16_to_cpu(desc->vlan_tag));
	}

	atl_rx_hash(skb, desc, ndev);

	if (skb->pkt_type == PACKET_MULTICAST)
		atl_update_ring_stat(ring, rx.multicast, 1);

	rx_skb_func(ring, skb);

	return true;
}

unsigned int atl_rx_linear;
module_param_named(rx_linear, atl_rx_linear, uint, 0444);

/* DMA mappings of buffer pages are accounted via struct
 * atl_rxpage. Being mapped counts as a single additional reference
 * for the target page.
 */
static int atl_get_page(struct atl_pgref *pgref, unsigned int order,
	struct device *dev, bool atomic)
{
	struct atl_rxpage *rxpage;
	struct page *page;
	dma_addr_t daddr;
	int ret = -ENOMEM;
	gfp_t flags = atomic ? GFP_ATOMIC | __GFP_NOWARN : GFP_KERNEL;

	rxpage = kmalloc(sizeof(*rxpage), flags);
	if (unlikely(!rxpage))
		return ret;

	page = __dev_alloc_pages(flags, order);
	if (unlikely(!page))
		goto free_rxpage;

	daddr = dma_map_page(dev, page, 0, PAGE_SIZE << order, DMA_FROM_DEVICE);
	trace_atl_dma_map_rxbuf(-1, -1, daddr, PAGE_SIZE << order, NULL,
		page_to_virt(page));

	if (unlikely(dma_mapping_error(dev, daddr)))
		goto free_page;

	rxpage->page = page;
	rxpage->daddr = daddr;
	rxpage->order = order;
	rxpage->mapcount = 1;

	pgref->rxpage = rxpage;
	pgref->pg_off = 0;

	return 0;

free_page:
	__free_pages(page, order);
free_rxpage:
	kfree(rxpage);

	return ret;
}

static int atl_get_pages(struct atl_rxbuf *rxbuf,
	struct atl_desc_ring *ring, bool atomic)
{
	int ret;
	struct device *dev = &ring->nic->hw.pdev->dev;

	if (likely((rxbuf->head.rxpage || atl_rx_linear)
			&& rxbuf->data.rxpage))
		return 0;

	if (!rxbuf->head.rxpage && !atl_rx_linear) {
		ret = atl_get_page(&rxbuf->head, ATL_RX_HEAD_ORDER,
			dev, atomic);
		if (ret) {
			atl_update_ring_stat(ring,
				rx.alloc_head_page_failed, 1);
			return ret;
		}
		atl_update_ring_stat(ring, rx.alloc_head_page, 1);
	}

	if (!rxbuf->data.rxpage) {
		ret = atl_get_page(&rxbuf->data, ATL_RX_DATA_ORDER,
			dev, atomic);
		if (ret) {
			atl_update_ring_stat(ring,
				rx.alloc_data_page_failed, 1);
			return ret;
		}
		atl_update_ring_stat(ring, rx.alloc_data_page, 1);
	}

	return 0;
}

static inline void atl_fill_rx_desc(struct atl_desc_ring *ring,
	struct atl_rxbuf *rxbuf)
{
	struct atl_rx_desc *desc;
	DECLARE_SCRATCH_DESC(scratch);

	desc  = &DESC_PTR(ring, ring->tail, scratch)->rx;

	desc->daddr = atl_buf_daddr(&rxbuf->data) +
		(atl_rx_linear ? ATL_RX_HEADROOM : 0);

	/* Assigning haddr clears dd as bufs are cacheline-aligned
	 * and ATL_RX_HEADROOM is even
	 */
	desc->haddr = atl_rx_linear ? 0 :
		atl_buf_daddr(&rxbuf->head) + ATL_RX_HEADROOM;

	trace_atl_fill_rx_desc(ring->tail, desc);
	COMMIT_DESC(ring, ring->tail, scratch);
}

static int atl_fill_rx(struct atl_desc_ring *ring, uint32_t count, bool atomic)
{
	int ret = 0;

	while (count) {
		struct atl_rxbuf *rxbuf = &ring->rxbufs[ring->tail];

		ret = atl_get_pages(rxbuf, ring, atomic);
		if (ret)
			break;

		atl_fill_rx_desc(ring, rxbuf);
		bump_tail(ring, 1);
		count--;
	}

	/* If tail ptr passed the next_to_recycle ptr, clamp the
	 * latter to the former.
	 */
	if (ring->next_to_recycle < ring->head ?
		ring->next_to_recycle < ring->tail &&
		ring->tail < ring->head :
		ring->tail > ring->next_to_recycle ||
		ring->tail < ring->head)
		ring->next_to_recycle = ring->tail;

	wmb();
	atl_write(ring_hw(ring), ATL_RX_RING_TAIL(ring), ring->tail);
	return ret;
}

static int atl_fill_hwts_rx(struct atl_desc_ring *ring, uint32_t count, bool atomic)
{
	struct atl_rx_desc *desc;

	while (count) {
		DECLARE_SCRATCH_DESC(scratch);

		desc  = &DESC_PTR(ring, ring->tail, scratch)->rx;
		desc->daddr = ring->hw.daddr + ring->hw.size * sizeof(*ring->hw.descs);
		desc->haddr = 0;

		COMMIT_DESC(ring, ring->tail, scratch);

		bump_tail(ring, 1);
		count--;
	}

	return 0;
}

static inline void atl_get_rxpage(struct atl_pgref *pgref)
{
	pgref->rxpage->mapcount++;
}

static inline void __atl_free_rxpage(struct atl_rxpage *rxpage,
	struct device *dev)
{
	unsigned int len = PAGE_SIZE << rxpage->order;

	dma_unmap_page(dev, rxpage->daddr, len, DMA_FROM_DEVICE);
	trace_atl_dma_unmap_rxbuf(-1, -1, rxpage->daddr, len, NULL);

	/* Drop the ref for dma mapping. */
	__free_pages(rxpage->page, rxpage->order);
	kfree(rxpage);
}

static inline void atl_put_rxpage(struct atl_pgref *pgref, struct device *dev)
{
	struct atl_rxpage *rxpage = pgref->rxpage;

	if (!rxpage)
		return;

	if (--rxpage->mapcount)
		return;

	__atl_free_rxpage(rxpage, dev);
	pgref->rxpage = 0;
}

static bool atl_recycle_or_put_page(struct atl_pgref *pgref,
	unsigned int buf_len, struct device *dev)
{
	unsigned int order = pgref->rxpage->order;
	unsigned int size = PAGE_SIZE << order;
	struct page *page = pgref->rxpage->page;

	if (!page_is_pfmemalloc(page) && pgref->pg_off + buf_len < size)
		return true;

	atl_put_rxpage(pgref, dev);

	return false;
}

static void atl_maybe_recycle_rxbuf(struct atl_desc_ring *ring,
	struct atl_rxbuf *rxbuf)
{
	int reused = 0;
	struct atl_pgref *head = &rxbuf->head, *data = &rxbuf->data;
	struct atl_rxbuf *new = &ring->rxbufs[ring->next_to_recycle];
	unsigned int data_len = ATL_RX_BUF_SIZE +
		(atl_rx_linear ? ATL_RX_HDR_OVRHD : 0);
	struct device *dev = &ring->nic->hw.pdev->dev;

	if (!atl_rx_linear &&
	    atl_recycle_or_put_page(head, ATL_RX_HDR_SIZE + ATL_RX_HDR_OVRHD,
				    dev)) {
		new->head = *head;
		reused = 1;
		atl_update_ring_stat(ring, rx.reused_head_page, 1);
	}
	head->rxpage = 0;

	if (atl_recycle_or_put_page(data, data_len, dev)) {
		new->data = *data;
		reused = 1;
		atl_update_ring_stat(ring, rx.reused_data_page, 1);
	}
	data->rxpage = 0;

	if (reused)
		bump_ptr(ring->next_to_recycle, ring, 1);
}

static unsigned int atl_data_len(struct atl_rx_desc_wb *wb)
{
	unsigned int len = le16_to_cpu(wb->pkt_len);

	if (!wb->eop)
		return ATL_RX_BUF_SIZE;

	if (!wb->rsc_cnt && wb->sph)
		len -= wb->hdr_len;

	len &= ATL_RX_BUF_SIZE - 1;
	return len ?: ATL_RX_BUF_SIZE;
}

static void atl_sync_range(struct atl_desc_ring *ring,
	struct atl_pgref *pgref, unsigned int offt, unsigned int len)
{
	dma_addr_t daddr = pgref->rxpage->daddr;
	unsigned int pg_off = pgref->pg_off + offt;
	struct device *dev = &ring->nic->hw.pdev->dev;

	dma_sync_single_range_for_cpu(dev, daddr, pg_off, len, DMA_FROM_DEVICE);
	trace_atl_sync_rx_range(-1, daddr, pg_off, len);
}

static struct sk_buff *atl_init_skb(struct atl_desc_ring *ring,
	struct atl_rxbuf *rxbuf, struct atl_rx_desc_wb *wb)
{
	struct sk_buff *skb;
	unsigned int hdr_len, alloc, tailroom, len;
	unsigned int data_len = atl_data_len(wb);
	void *hdr;
	struct atl_pgref *pgref;
	struct atl_nic *nic = ring->nic;

	if (atl_rx_linear) {
		if (!wb->eop) {
			atl_nic_err("Multi-frag packet in linear mode\n");
			atl_update_ring_stat(ring, rx.linear_dropped, 1);
			return (void *)-1l;
		}

		hdr_len = len = data_len;
		tailroom = 0;
		pgref = &rxbuf->data;
	} else {
		hdr_len = wb->hdr_len;
		if (hdr_len == 0) {
			atl_nic_err("Header parse error\n");
			return (void *)-1l;
		}

		/* If entire packet fits into ATL_RX_HDR_SIZE, reserve
		 * enough space to pull the data part into skb head
		 * and make it linear, otherwise allocate space for
		 * hdr_len only
		 */
		len = (wb->sph ? hdr_len : 0) + data_len;
		if (!wb->eop || len > ATL_RX_HDR_SIZE)
			len = hdr_len;

		/* reserve space for potential __pskb_pull_tail() */
		tailroom = min(ATL_RX_TAILROOM, ATL_RX_HDR_SIZE - len);
		pgref = &rxbuf->head;
	}

	if (atl_rx_linear || (wb->sph && (wb->eop || !wb->rsc_cnt)))
		atl_sync_range(ring, pgref,
			ATL_RX_HEADROOM, hdr_len);

	alloc = len + tailroom + ATL_RX_HEADROOM;
	alloc += SKB_DATA_ALIGN(sizeof(struct skb_shared_info));
	alloc = SKB_DATA_ALIGN(alloc);

	hdr = atl_buf_vaddr(pgref);
	skb = build_skb(hdr, alloc);
	if (unlikely(!skb)) {
		atl_update_ring_stat(ring, rx.alloc_skb_failed, 1);
		return NULL;
	}

	if (wb->rsc_cnt && !wb->eop) {
		struct atl_cb *atl_cb = ATL_CB(skb);

		/* First frag of a multi-frag RSC packet. Either head or
		 * data buffer, depending on whether the header was
		 * split off by HW, might still be accessed by
		 * RSC. Delay processing till EOP.
		 */
		if (wb->sph) {
			atl_cb->pgref = *pgref;
			atl_cb->head = true;
			/* Safe to sync the data buf. !wb->eop
			 * implies the data buffer is completely filled.
			 */
			atl_sync_range(ring, &rxbuf->data, 0, ATL_RX_BUF_SIZE);
		} else {
			atl_cb->pgref = rxbuf->data;
			atl_cb->head = false;
			/* No need to sync head fragment as nothing
			 * was DMA'd into it
			 */
		}
		atl_get_rxpage(&atl_cb->pgref);
	}

	pgref->pg_off += alloc;
	page_ref_inc(pgref->rxpage->page);

	if (!atl_rx_linear && !wb->sph) {
		atl_nic_dbg("Header not split despite non-zero hdr_len (%d)\n",
			hdr_len);
		/* Make skb head empty -- will copy the real header
		 * from the data buffer later
		 */
		hdr_len = 0;
	}

	skb_reserve(skb, ATL_RX_HEADROOM);
	skb_put(skb, hdr_len);
	return skb;
}

static inline void atl_skb_put_data(struct sk_buff *skb,
	void *data, unsigned int len)
{
	memcpy(skb_tail_pointer(skb), data, len);
	skb->tail += len;
	skb->len += len;
}

static struct sk_buff *atl_process_rx_frag(struct atl_desc_ring *ring,
	struct atl_rxbuf *rxbuf, struct atl_rx_desc_wb *wb)
{
	bool first_frag = false;
	bool hdr_split = !!wb->sph;
	unsigned int hdr_len, data_len, aligned_data_len;
	unsigned int data_offt = 0, to_pull = 0;
	struct sk_buff *skb = rxbuf->skb;
	struct atl_cb *atl_cb;
	struct atl_pgref *headref = &rxbuf->head, *dataref = &rxbuf->data;
	struct device *dev = &ring->nic->hw.pdev->dev;
	bool is_ptp_ring = atl_is_ptp_ring(ring->nic, ring);

	if (unlikely(wb->rdm_err)) {
		if (skb && skb != (void *)-1l)
			dev_kfree_skb_any(skb);

		skb = (void *)-1l;
	}

	if (!skb) {
		 /* First buffer of a packet */
		skb = atl_init_skb(ring, rxbuf, wb);
		first_frag = true;
	} else
		rxbuf->skb = NULL;

	if (unlikely(!skb || skb == (void *)-1l))
		return skb;

	hdr_len = wb->hdr_len;
	data_len = atl_data_len(wb);
	if (is_ptp_ring) {
		data_len -= atl_ptp_extract_ts(ring->nic, skb, atl_buf_vaddr(dataref),
					       data_len);
	}

	if (atl_rx_linear) {
		/* Linear skb mode. The entire packet was DMA'd into
		 * the data buffer and skb has already been built
		 * around it and dataref's pg_off has been increased
		 * in atl_init_skb()
		 */

		atl_maybe_recycle_rxbuf(ring, rxbuf);
		return skb;
	}

	/* Align the start of the next buffer in the page. This also
	 * serves as truesize increment when the paged frag is added
	 * to skb.
	 */
	aligned_data_len = ALIGN(data_len, L1_CACHE_BYTES);

	if (first_frag && !hdr_split)
		/* Header was not split off, so skip over it
		 * when adding the paged frag
		 */
		data_offt = hdr_len;

	if (!first_frag || wb->eop || !wb->rsc_cnt) {
		atl_sync_range(ring, dataref, 0, data_len);

		/* If header was not split off by HW, remember to pull
		 * it into the skb head later. The rest of the data
		 * buffer might need to be pulled too for small
		 * packets, so delay the actual copying till later
		 */
		if (first_frag && !hdr_split)
			to_pull = hdr_len;
	}

	/* If the entire packet fits within ATL_RX_HDR_SIZE bytes,
	 * pull it into the skb head. This handles the header not
	 * having been split by HW case correctly too, as
	 * skb_headlen() will be zero in that case and data_len will
	 * hold the whole packet length.
	 */
	if (first_frag && skb_headlen(skb) + data_len <= ATL_RX_HDR_SIZE) {
		to_pull = data_len;
		/* Recycle the data buffer as we're copying the
		 * contents to skb head.
		 */
		aligned_data_len = 0;
	} else {
		/* Add the data buffer to paged frag list, skipping
		 * the un-split header if any -- it will be copied to
		 * skb head later.
		 */
		skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
			dataref->rxpage->page, dataref->pg_off + data_offt,
			data_len - data_offt, aligned_data_len);
		page_ref_inc(dataref->rxpage->page);
	}

	if (to_pull)
		atl_skb_put_data(skb, atl_buf_vaddr(dataref), to_pull);

	/* Update the data buf's pg_off to point to free
	 * space. Header buf's offset was updated in atl_init_skb()
	 * for first frag of the packet only.
	 */
	dataref->pg_off += aligned_data_len;
	atl_maybe_recycle_rxbuf(ring, rxbuf);

	if (first_frag || !wb->eop || !wb->rsc_cnt)
		return skb;

	/* The last descriptor of RSC packet is done, unmap the head
	 * fragment.
	 */
	atl_cb = ATL_CB(skb);

	headref = &atl_cb->pgref;
	if (unlikely(!headref->rxpage))
		return skb;

	if (likely(atl_cb->head)) {
		atl_sync_range(ring, headref, ATL_RX_HEADROOM, hdr_len);
		atl_put_rxpage(headref, dev);
	} else {
		atl_sync_range(ring, headref, 0, ATL_RX_BUF_SIZE);
		/* Data buf's sync being delayed implies header was
		 * not split off by HW. Fix that now.
		 */
		atl_skb_put_data(skb, atl_buf_vaddr(headref), hdr_len);
		atl_put_rxpage(headref, dev);
	}

	return skb;
}

unsigned int atl_rx_refill_batch = 16;
module_param_named(rx_refill_batch, atl_rx_refill_batch, uint, 0644);

int atl_clean_rx(struct atl_desc_ring *ring, int budget,
		 rx_skb_handler_t rx_skb_func)
{
	unsigned int refill_batch =
		min_t(typeof(atl_rx_refill_batch), atl_rx_refill_batch,
		      ring->hw.size - 1);
	unsigned int packets = 0;
	unsigned int bytes = 0;
	struct sk_buff *skb;

	while (packets < budget) {
		uint32_t space = ring_space(ring);
		struct atl_rx_desc_wb *wb;
		struct atl_rxbuf *rxbuf;
		unsigned int len;
		DECLARE_SCRATCH_DESC(scratch);

		if (space >= refill_batch)
			atl_fill_rx(ring, space, true);

		rxbuf = &ring->rxbufs[ring->head];

		wb = &DESC_PTR(ring, ring->head, scratch)->wb;
		FETCH_DESC(ring, ring->head, scratch);

		if (!wb->dd)
			break;
		DESC_RMB();

		trace_atl_rx_descr(ring->qvec->idx, ring->head, (u64 *)wb);

		skb = atl_process_rx_frag(ring, rxbuf, wb);

		/* Treat allocation errors as transient and retry later */
		if (!skb) {
			struct atl_nic *nic = ring->nic;

			atl_nic_err("failed to alloc skb for RX packet\n");
			break;
		}

		if (skb == (void *)-1l)
			atl_maybe_recycle_rxbuf(ring, rxbuf);

		bump_head(ring, 1);
		if (!wb->eop) {
			uint32_t next = wb->rsc_cnt ?
				le16_to_cpu(wb->next_desp) :
				ring->head;
			/* If atl_process_rx_flags() returned any
			 * other error this propagates the error to
			 * the next descriptor of the packet,
			 * preventing it from being treated as a start
			 * of a new packet later.
			 */
			ring->rxbufs[next].skb = skb;
			atl_update_ring_stat(ring, rx.non_eop_descs, 1);
			continue;
		}

		if (skb == (void *)-1l)
			continue;

		len = skb->len;
		if (atl_rx_packet(skb, wb, ring, rx_skb_func)) {
			packets++;
			bytes += len;
		}
	}

	u64_stats_update_begin(&ring->syncp);
	ring->stats.rx.bytes += bytes;
	ring->stats.rx.packets += packets;
	u64_stats_update_end(&ring->syncp);

	return packets;
}

int atl_clean_hwts_rx(struct atl_desc_ring *ring, int budget)
{
	unsigned int refill_batch =
		min_t(typeof(atl_rx_refill_batch), atl_rx_refill_batch,
		      ring->hw.size - 1);
	unsigned int packets = 0;

	while (packets < budget) {
		uint32_t space = ring_space(ring);
		struct atl_rx_desc_hwts_wb *wb;
		struct atl_rxbuf *rxbuf;
		u64 ns;
		DECLARE_SCRATCH_DESC(scratch);

		if (space >= refill_batch)
			atl_fill_hwts_rx(ring, space, true);

		rxbuf = &ring->rxbufs[ring->head];

		wb = &DESC_PTR(ring, ring->head, scratch)->hwts_wb;
		FETCH_DESC(ring, ring->head, scratch);

		if (!wb->dd)
			break;
		DESC_RMB();

		hw_atl_extract_hwts(&ring->nic->hw, wb, &ns);
		atl_ptp_tx_hwtstamp(ring->nic, ns);

		bump_head(ring, 1);
		packets++;
	}

	return packets;
}

unsigned int atl_min_intr_delay = 10;
module_param_named(min_intr_delay, atl_min_intr_delay, uint, 0644);

static void atl_set_intr_throttle(struct atl_queue_vec *qvec)
{
	struct atl_hw *hw = &qvec->nic->hw;
	atl_write(hw, ATL_INTR_THRTL(atl_qvec_intr(qvec)),
		1 << 0x1f | ((atl_min_intr_delay / 2) & 0x1ff) << 0x10);
}

static int atl_poll(struct napi_struct *napi, int budget)
{
	struct atl_queue_vec *qvec;

	qvec = container_of(napi, struct atl_queue_vec, napi);

	return atl_poll_qvec(qvec, budget);
}

int atl_poll_qvec(struct atl_queue_vec *qvec, int budget)
{
	struct atl_nic *nic;
	bool clean_done;
	int rx_cleaned;

	nic = qvec->nic;

	clean_done = atl_clean_tx(&qvec->tx);
	rx_cleaned = atl_clean_rx(&qvec->rx, budget, atl_napi_receive_skb);

	clean_done &= (rx_cleaned < budget);

	if (!clean_done)
		return budget;

	if (likely(qvec->type != ATL_QUEUE_PTP)) {
		napi_complete_done(&qvec->napi, rx_cleaned);
		atl_intr_enable(&nic->hw, BIT(atl_qvec_intr(qvec)));
		/* atl_set_intr_throttle(&nic->hw, qvec->idx); */
	}
	return rx_cleaned;
}

/* XXX NOTE: only checked on device probe for now */
#ifdef CONFIG_PCI_MSI
bool atl_enable_msi = true;
#else
bool atl_enable_msi /*= false*/;
#endif
module_param_named(msi, atl_enable_msi, bool, 0444);
bool atl_wq_non_msi /*= false*/;
module_param_named(wq_non_msi, atl_wq_non_msi, bool, 0444);

static int atl_config_interrupts(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	unsigned int flags;
	int ret;

	if (atl_enable_msi) {
		int nvecs;

		nvecs = min_t(unsigned int, nic->requested_nvecs,
			      num_present_cpus());
		flags = PCI_IRQ_MSIX | PCI_IRQ_MSI;
		ret = pci_alloc_irq_vectors(hw->pdev,
			ATL_NUM_NON_RING_IRQS + 1,
			ATL_NUM_NON_RING_IRQS + nvecs,
			flags);

		/* pci_alloc_irq_vectors() never allocates less
		 * than min_vectors
		 */
		if (ret > 0) {
			ret -= ATL_NUM_NON_RING_IRQS;
			nic->flags |= ATL_FL_MULTIPLE_VECTORS;
			return ret;
		}
	}

	atl_nic_warn("Couldn't allocate MSI-X / MSI vectors, falling back to legacy interrupts\n");

	flags = PCI_IRQ_LEGACY;
	ret = pci_alloc_irq_vectors(hw->pdev, 1, 1, flags);
	if (ret < 0) {
		atl_nic_err("Couldn't allocate legacy IRQ\n");
		return ret;
	}

	nic->flags &= ~ATL_FL_MULTIPLE_VECTORS;

	return min_t(unsigned int, atl_max_queues_non_msi, num_present_cpus());
}

irqreturn_t atl_ring_irq(int irq, void *priv)
{
	struct napi_struct *napi = priv;

	napi_schedule_irqoff(napi);
	return IRQ_HANDLED;
}

void atl_ring_work(struct work_struct *work)
{
	struct legacy_irq_work *irq_work = to_irq_work(work);

	napi_schedule(irq_work->napi);
}

void atl_clear_datapath(struct atl_nic *nic)
{
	int i;
	struct atl_queue_vec *qvecs = nic->qvecs;

	/* If atl_reconfigure() have failed previously,
	 * atl_clear_datapath() can be called again on
	 * pci_ops->remove(), without an intervening
	 * atl_setup_datapath().
	 */
	if (!test_and_clear_bit(ATL_ST_CONFIGURED, &nic->hw.state))
		return;

	atl_ptp_irq_free(nic);
	atl_free_link_intr(nic);

	if (nic->flags & ATL_FL_MULTIPLE_VECTORS) {
		for (i = 0; i < nic->nvecs; i++) {
			int vector = pci_irq_vector(nic->hw.pdev,
						    i + ATL_NUM_NON_RING_IRQS);
			irq_set_affinity_hint(vector, NULL);
		}
	}

	pci_free_irq_vectors(nic->hw.pdev);

	if (!qvecs)
		return;

	for (i = 0; i < nic->nvecs; i++) {
		if (unlikely(!(nic->flags & ATL_FL_MULTIPLE_VECTORS)))
			cancel_work_sync(qvecs[i].work);
		netif_napi_del(&qvecs[i].napi);
	}

	atl_ptp_ring_stop(nic);

	kfree(to_irq_work(qvecs[0].work));
	kfree(qvecs);
	nic->qvecs = NULL;
}

static void atl_calc_affinities(struct atl_nic *nic)
{
	int i;
	unsigned int cpu;

	get_online_cpus();
	cpu = cpumask_first(cpu_online_mask);

	for (i = 0; i < nic->nvecs; i++) {
		cpumask_t *cpumask = &nic->qvecs[i].affinity_hint;

		/* If more vectors got allocated (based on
		 * cpu_present_mask) than cpus currently online,
		 * spread the remaining vectors among online cpus.
		 */
		if (cpu >= nr_cpumask_bits)
			cpu = cpumask_first(cpu_online_mask);

		cpumask_clear(cpumask);
		cpumask_set_cpu(cpu, cpumask);
		cpu = cpumask_next(cpu, cpu_online_mask);
	}
	put_online_cpus();
}

void atl_init_qvec(struct atl_nic *nic, struct atl_queue_vec *qvec, int idx)
{
	qvec->nic = nic;
	qvec->idx = idx;

	qvec->rx.hw.reg_base = ATL_RX_RING(idx);
	qvec->rx.hw.size = nic->requested_rx_size;
	qvec->rx.nic = nic;
	qvec->rx.qvec = qvec;

	qvec->tx.hw.reg_base = ATL_TX_RING(idx);
	qvec->tx.hw.size = nic->requested_tx_size;
	qvec->tx.nic = nic;
	qvec->tx.qvec = qvec;

	u64_stats_init(&qvec->rx.syncp);
	u64_stats_init(&qvec->tx.syncp);

	if (likely(qvec->type == ATL_QUEUE_REGULAR))
		netif_napi_add(nic->ndev, &qvec->napi, atl_poll, 64);
}

int atl_setup_datapath(struct atl_nic *nic)
{
	struct legacy_irq_work *irq_work = NULL;
	struct atl_queue_vec *qvec;
	int nvecs, i, ret;

	nvecs = atl_config_interrupts(nic);
	if (nvecs < 0)
		return nvecs;
	nic->nvecs = nvecs;

	qvec = kcalloc(nvecs, sizeof(*qvec), GFP_KERNEL);
	if (!qvec) {
		atl_nic_err("Couldn't alloc qvecs\n");
		ret = -ENOMEM;
		goto err_alloc;
	}
	nic->qvecs = qvec;

	if (unlikely(!(nic->flags & ATL_FL_MULTIPLE_VECTORS))) {
		irq_work = kcalloc(nvecs, sizeof(*irq_work), GFP_KERNEL);
		if (!irq_work) {
			ret = -ENOMEM;
			goto err_alloc_work;
		}
	}

	ret = atl_alloc_link_intr(nic);
	if (ret)
		goto err_link_intr;

	ret = atl_ptp_irq_alloc(nic);
	if (ret < 0)
		goto err_ptp_intr;

	ret = atl_ptp_ring_start(nic);
	if (ret < 0)
		goto err_ptp_ring;

	for (i = 0; i < nvecs; i++, qvec++) {
		atl_init_qvec(nic, qvec, i);

		if (unlikely(irq_work)) {
			INIT_WORK(&irq_work[i].work, atl_ring_work);
			irq_work[i].napi = &qvec->napi;
			qvec->work = &irq_work[i].work;
		}
	}

	atl_calc_affinities(nic);

	nic->max_mtu = atl_rx_linear ? ATL_MAX_RX_LINEAR_MTU : ATL_MAX_MTU;

	set_bit(ATL_ST_CONFIGURED, &nic->hw.state);
	return 0;

err_ptp_ring:
	atl_ptp_irq_free(nic);

err_ptp_intr:
	atl_free_link_intr(nic);

err_link_intr:
	kfree(irq_work);

err_alloc_work:
	kfree(nic->qvecs);
	nic->qvecs = NULL;

err_alloc:
	pci_free_irq_vectors(nic->hw.pdev);

	return ret;
}

static inline void atl_free_rxpage(struct atl_pgref *pgref, struct device *dev)
{
	struct atl_rxpage *rxpage = pgref->rxpage;

	if (!rxpage)
		return;

	/* Unmap, dropping the ref for being mapped */
	__atl_free_rxpage(rxpage, dev);
	pgref->rxpage = 0;
}

/* Releases any skbs that may have been queued on ring positions yet
 * to be processes by poll. The buffers are kept to be re-used after
 * resume / thaw. */
void atl_clear_rx_bufs(struct atl_desc_ring *ring)
{
	unsigned int bufs = ring_occupied(ring);
	struct device *dev = &ring->nic->hw.pdev->dev;

	while (bufs) {
		struct atl_rxbuf *rxbuf = &ring->rxbufs[ring->head];
		struct sk_buff *skb = rxbuf->skb;

		if (skb) {
			struct atl_pgref *pgref = &ATL_CB(skb)->pgref;

			atl_put_rxpage(pgref, dev);
			dev_kfree_skb_any(skb);
			rxbuf->skb = NULL;
		}

		bump_head(ring, 1);
		bufs--;
	}
}

static void atl_free_rx_bufs(struct atl_desc_ring *ring)
{
	struct device *dev = &ring->nic->hw.pdev->dev;
	struct atl_rxbuf *rxbuf;

	if (!ring->rxbufs)
		return;

	for (rxbuf = ring->rxbufs;
	     rxbuf < &ring->rxbufs[ring->hw.size]; rxbuf++) {
		atl_free_rxpage(&rxbuf->head, dev);
		atl_free_rxpage(&rxbuf->data, dev);
	}
}

static void atl_free_tx_bufs(struct atl_desc_ring *ring)
{
	unsigned int bufs = ring_occupied(ring);

	if (!ring->txbufs)
		return;

	while (bufs) {
		struct atl_txbuf *txbuf;

		bump_tail(ring, -1);
		txbuf = &ring->txbufs[ring->tail];

		atl_txbuf_free(txbuf, &ring->nic->hw.pdev->dev, ring->tail);
		bufs--;
	}
}

static size_t atl_ring_extra_size(struct atl_desc_ring *ring)
{
	switch (ring->qvec->type) {
	case ATL_QUEUE_REGULAR:
	case ATL_QUEUE_PTP:
		return 0;
	case ATL_QUEUE_HWTS:
		return ATL_RX_BUF_SIZE;
	default:
		WARN_ONCE(true, "Unknown queue type\n");
		break;
	}

	return 0;
}

static void atl_free_ring(struct atl_desc_ring *ring)
{
	size_t extra = atl_ring_extra_size(ring);

	if (ring->bufs) {
		vfree(ring->bufs);
		ring->bufs = 0;
	}

	atl_free_descs(ring->nic, &ring->hw, extra);
}

static int atl_alloc_ring(struct atl_desc_ring *ring, size_t buf_size,
	char *type)
{
	size_t extra = atl_ring_extra_size(ring);
	struct atl_nic *nic = ring->nic;
	int idx = ring->qvec->idx;
	int ret;

	ret = atl_alloc_descs(nic, &ring->hw, extra);
	if (ret) {
		atl_nic_err("Couldn't alloc %s[%d] descriptors\n", type, idx);
		return ret;
	}

	if (likely(ring->qvec->type != ATL_QUEUE_HWTS)) {
		ring->bufs = vzalloc(ring->hw.size * buf_size);
		if (!ring->bufs) {
			ret = -ENOMEM;
			goto free;
		}
	}

	ring->head = ring->tail =
		atl_read(&nic->hw, ATL_RING_HEAD(ring)) & 0x1fff;
	return 0;

free:
	atl_free_ring(ring);
	return ret;
}

static void atl_set_affinity(int vector, struct atl_queue_vec *qvec)
{
	cpumask_t *cpumask = qvec ? &qvec->affinity_hint : NULL;

	irq_set_affinity_hint(vector, cpumask);
}

static int atl_alloc_qvec_intr(struct atl_queue_vec *qvec)
{
	struct atl_nic *nic = qvec->nic;
	int vector;
	int ret;

	snprintf(qvec->name, sizeof(qvec->name), "%s-ring-%d",
		nic->ndev->name, qvec->idx);

	if (!(nic->flags & ATL_FL_MULTIPLE_VECTORS))
		return 0;

	vector = pci_irq_vector(nic->hw.pdev, atl_qvec_intr(qvec));
	ret = request_irq(vector, atl_ring_irq, IRQF_NO_SUSPEND,
			  qvec->name, &qvec->napi);
	if (ret) {
		atl_nic_err("request MSI ring vector failed: %d\n", -ret);
		return ret;
	}

	atl_set_affinity(vector, qvec);

	return 0;
}

static void atl_free_qvec_intr(struct atl_queue_vec *qvec)
{
	struct atl_nic *nic = qvec->nic;
	int vector;

	if (unlikely(!nic))
		return;

	if (!(nic->flags & ATL_FL_MULTIPLE_VECTORS))
		return;

	vector = pci_irq_vector(nic->hw.pdev, atl_qvec_intr(qvec));
	atl_set_affinity(vector, NULL);
	free_irq(vector, &qvec->napi);
}

int atl_alloc_qvec(struct atl_queue_vec *qvec)
{
	struct atl_txbuf *txbuf;
	int count = qvec->tx.hw.size;
	int ret = 0;

	switch (qvec->type) {
	case ATL_QUEUE_REGULAR:
		ret = atl_alloc_qvec_intr(qvec);
		break;
	case ATL_QUEUE_PTP:
	case ATL_QUEUE_HWTS:
		break;
	default:
		WARN_ONCE(true, "Unknown queue type\n");
		break;
	}

	if (ret)
		return ret;

	if (likely(qvec->type != ATL_QUEUE_HWTS)) {
		ret = atl_alloc_ring(&qvec->tx, sizeof(struct atl_txbuf), "tx");
		if (ret)
			goto free_irq;
	}

	ret = atl_alloc_ring(&qvec->rx, sizeof(struct atl_rxbuf), "rx");
	if (ret)
		goto free_tx;

	if (likely(qvec->type != ATL_QUEUE_HWTS)) {
		for (txbuf = qvec->tx.txbufs; count; count--)
			(txbuf++)->last = -1;
	}

	return 0;

free_tx:
	if (likely(qvec->type != ATL_QUEUE_HWTS))
		atl_free_ring(&qvec->tx);
free_irq:
	switch (qvec->type) {
	case ATL_QUEUE_REGULAR:
		atl_free_qvec_intr(qvec);
		break;
	default:
		break;
	}

	return ret;
}

void atl_free_qvec(struct atl_queue_vec *qvec)
{
	struct atl_desc_ring *rx = &qvec->rx;
	struct atl_desc_ring *tx = &qvec->tx;

	atl_free_rx_bufs(rx);
	atl_free_ring(rx);

	if (likely(qvec->type != ATL_QUEUE_HWTS))
		atl_free_ring(tx);

	switch (qvec->type) {
	case ATL_QUEUE_REGULAR:
		atl_free_qvec_intr(qvec);
		break;
	case ATL_QUEUE_PTP:
	case ATL_QUEUE_HWTS:
		break;
	default:
		WARN_ONCE(true, "Unknown queue type\n");
		break;
	}
}

int atl_alloc_rings(struct atl_nic *nic)
{
	struct atl_queue_vec *qvec;
	int ret;

	atl_for_each_qvec(nic, qvec) {
		ret = atl_alloc_qvec(qvec);
		if (ret)
			goto free;
	}

	return 0;

free:
	while(--qvec >= &nic->qvecs[0])
		atl_free_qvec(qvec);

	return ret;
}

void atl_free_rings(struct atl_nic *nic)
{
	struct atl_queue_vec *qvec;

	atl_for_each_qvec(nic, qvec)
		atl_free_qvec(qvec);

}

static unsigned int atl_rx_mod_hyst = 10, atl_tx_mod_hyst = 10;
module_param_named(rx_mod_hyst, atl_rx_mod_hyst, uint, 0644);
module_param_named(tx_mod_hyst, atl_tx_mod_hyst, uint, 0644);

static void atl_set_intr_mod_qvec(struct atl_queue_vec *qvec)
{
	struct atl_nic *nic = qvec->nic;
	struct atl_hw *hw = &nic->hw;
	unsigned int min, max;
	int idx = qvec->idx;
	uint32_t reg;

	min = nic->rx_intr_delay - atl_min_intr_delay;
	max = min + atl_rx_mod_hyst;

	atl_write(hw, ATL_RX_INTR_MOD_CTRL(idx),
		(max / 2) << 0x10 | (min / 2) << 8 | 2);

	min = nic->tx_intr_delay - atl_min_intr_delay;
	max = min + atl_tx_mod_hyst;

	if (hw->chip_id == ATL_ANTIGUA)
		reg = ATL2_TX_INTR_MOD_CTRL(idx);
	else
		reg = ATL_TX_INTR_MOD_CTRL(idx);
	atl_write(hw, reg, (max / 2) << 0x10 | (min / 2) << 8 | 2);
}

void atl_set_intr_mod(struct atl_nic *nic)
{
	struct atl_queue_vec *qvec;

	atl_for_each_qvec(nic, qvec)
		atl_set_intr_mod_qvec(qvec);
}

int atl_init_rx_ring(struct atl_desc_ring *rx, enum atl_queue_type type)
{
	struct atl_hw *hw = &rx->nic->hw;
	struct atl_rxbuf *rxbuf;
	int ret = 0;

	rx->head = rx->tail = atl_read(hw, ATL_RING_HEAD(rx)) & 0xffff;
	if (rx->head > 0x1FFF)
		return -EIO;

	switch (type) {
	case ATL_QUEUE_HWTS:
		ret = atl_fill_hwts_rx(rx, ring_space(rx), false);
		break;
	case ATL_QUEUE_PTP:
	case ATL_QUEUE_REGULAR:
		ret = atl_fill_rx(rx, ring_space(rx), false);
		break;
	default:
		WARN_ONCE(true, "Unknown queue type\n");
		break;
	}

	if (ret)
		return ret;

	if (likely(type != ATL_QUEUE_HWTS)) {
		rx->next_to_recycle = rx->tail;
		/* rxbuf at ->next_to_recycle is always kept empty so that
		 * atl_maybe_recycle_rxbuf() always have a spot to recycle into
		 * without overwriting a pgref to an already allocated page,
		 * leaking memory. It's also the guard element in the ring
		 * that keeps ->tail from overrunning ->head. If it's nonempty
		 * on ring init (e.g. after a sleep-wake cycle) just release
		 * the pages.
		 */
		rxbuf = &rx->rxbufs[rx->next_to_recycle];
		atl_put_rxpage(&rxbuf->head, &hw->pdev->dev);
		atl_put_rxpage(&rxbuf->data, &hw->pdev->dev);
	}

	return 0;
}

int atl_init_tx_ring(struct atl_desc_ring *tx)
{
	struct atl_hw *hw = &tx->nic->hw;

	tx->head = tx->tail = atl_read(hw, ATL_RING_HEAD(tx)) & 0xffff;
	if (tx->head > 0x1FFF)
		return -EIO;

	return 0;
}

static void atl_start_rx_ring(struct atl_desc_ring *ring)
{
	struct atl_hw *hw = &ring->nic->hw;
	int idx = ring->qvec->idx;
	unsigned int rx_ctl;

	atl_write(hw, ATL_RING_BASE_LSW(ring), ring->hw.daddr);
	atl_write(hw, ATL_RING_BASE_MSW(ring), upper_32_bits(ring->hw.daddr));

	atl_write(hw, ATL_RX_RING_TAIL(ring), ring->tail);
	switch (ring->qvec->type) {
	case ATL_QUEUE_REGULAR:
		atl_write(hw, ATL_RX_RING_BUF_SIZE(ring),
			(ATL_RX_HDR_SIZE / 64) << 8 | ATL_RX_BUF_SIZE / 1024);
		break;
	case ATL_QUEUE_PTP:
	case ATL_QUEUE_HWTS:
		atl_write(hw, ATL_RX_RING_BUF_SIZE(ring),
			ATL_RX_BUF_SIZE / 1024);
		break;
	default:
		WARN_ONCE(true, "Unknown queue type\n");
		break;
	}
	atl_write(hw, ATL_RX_RING_THRESH(ring), 8 << 0x10 | 24 << 0x18);

	/* LRO */
	atl_write_bits(hw, ATL_RX_LRO_PKT_LIM(idx),
		(idx & 7) * 4, 2, 3);

	/* Enable ring | VLAN offload */
	rx_ctl = BIT(31) | BIT(29) | ring->hw.size;
	switch (ring->qvec->type) {
	case ATL_QUEUE_REGULAR:
		/* Enable header split in non-linear mode */
		rx_ctl |= (atl_rx_linear ? 0 : BIT(28));
		break;
	case ATL_QUEUE_PTP:
	case ATL_QUEUE_HWTS:
		break;
	default:
		break;
	}
	atl_write(hw, ATL_RX_RING_CTL(ring), rx_ctl);
}

static void atl_start_tx_ring(struct atl_desc_ring *ring)
{
	struct atl_nic *nic = ring->nic;
	struct atl_hw *hw = &nic->hw;

	atl_write(hw, ATL_RING_BASE_LSW(ring), ring->hw.daddr);
	atl_write(hw, ATL_RING_BASE_MSW(ring), upper_32_bits(ring->hw.daddr));

	/* Enable TSO on all active Tx rings */
	atl_write(hw, ATL_TX_LSO_CTRL, BIT(nic->nvecs) - 1);

	atl_write(hw, ATL_TX_RING_TAIL(ring), ring->tail);
	switch (ring->qvec->type) {
	case ATL_QUEUE_REGULAR:
		atl_write(hw, ATL_TX_RING_THRESH(ring), 8 << 8 | 8 << 0x10 |
			24 << 0x18);
		break;
	case ATL_QUEUE_PTP:
	case ATL_QUEUE_HWTS:
		atl_write(hw, ATL_TX_RING_THRESH(ring), 0);
		break;
	default:
		WARN_ONCE(true, "Unknown queue type\n");
		break;
	}
	atl_write(hw, ATL_TX_RING_CTL(ring), BIT(31) | ring->hw.size);
}

int atl_start_qvec(struct atl_queue_vec *qvec)
{
	struct atl_desc_ring *rx = &qvec->rx;
	struct atl_desc_ring *tx = &qvec->tx;
	struct atl_hw *hw = &qvec->nic->hw;
	int intr = atl_qvec_intr(qvec);
	int ret;

	ret = atl_init_rx_ring(rx, qvec->type);
	if (ret)
		return ret;
	if (likely(qvec->type != ATL_QUEUE_HWTS)) {
		ret = atl_init_tx_ring(tx);
		if (ret)
			return ret;
	}

	/* Map ring interrups into corresponding cause bit*/
	atl_set_intr_bits(hw, qvec->idx, intr, intr);
	atl_set_intr_throttle(qvec);

	if (likely(qvec->type == ATL_QUEUE_REGULAR))
		napi_enable(&qvec->napi);
	atl_set_intr_mod_qvec(qvec);
	atl_intr_enable(hw, BIT(intr));

	if (likely(qvec->type != ATL_QUEUE_HWTS))
		atl_start_tx_ring(tx);
	atl_start_rx_ring(rx);

	return 0;
}

void atl_stop_qvec(struct atl_queue_vec *qvec)
{
	struct atl_desc_ring *rx = &qvec->rx;
	struct atl_desc_ring *tx = &qvec->tx;
	struct atl_hw *hw = &qvec->nic->hw;

	/* Disable and reset rings */
	atl_write(hw, ATL_RING_CTL(rx), BIT(25));
	if (likely(qvec->type != ATL_QUEUE_HWTS))
		atl_write(hw, ATL_RING_CTL(tx), BIT(25));
	udelay(10);
	atl_write(hw, ATL_RING_CTL(rx), 0);
	if (likely(qvec->type != ATL_QUEUE_HWTS))
		atl_write(hw, ATL_RING_CTL(tx), 0);

	atl_intr_disable(hw, BIT(atl_qvec_intr(qvec)));
	if (likely(qvec->type == ATL_QUEUE_REGULAR))
		napi_disable(&qvec->napi);

	if (likely(qvec->type != ATL_QUEUE_HWTS)) {
		atl_clear_rx_bufs(rx);
		atl_free_tx_bufs(tx);
	}
}

static void atl_set_lro(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	uint32_t val = nic->ndev->features & NETIF_F_LRO ?
		BIT(nic->nvecs) - 1 : 0;

	if (val)
		atl_nic_warn("There are unresolved issues with LRO, enabling it isn't recommended for now\n");

	atl_write_bits(hw, ATL_RX_LRO_CTRL1, 0, nic->nvecs, val);
	atl_write_bits(hw, ATL_INTR_RSC_EN, 0, nic->nvecs, val);
}

int atl_start_rings(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;
	uint32_t mask;
	struct atl_queue_vec *qvec;
	int ret;

	if (test_bit(ATL_ST_RINGS_RUNNING, &hw->state))
		return 0;

	if (nic->flags & ATL_FL_MULTIPLE_VECTORS) {
		mask = BIT(nic->nvecs + ATL_NUM_NON_RING_IRQS) -
			BIT(ATL_NUM_NON_RING_IRQS);
		/* Enable auto-masking of ring interrupts on intr generation */
		atl_set_bits(hw, ATL_INTR_AUTO_MASK, mask);
		/* Enable status auto-clear on intr generation */
		atl_set_bits(hw, ATL_INTR_AUTO_CLEAR, mask);
	}

	atl_set_lro(nic);
	ret = atl_set_rss_tbl(hw);
	if (ret)
		return ret;

	atl_for_each_qvec(nic, qvec) {
		ret = atl_start_qvec(qvec);
		if (ret)
			goto stop;
	}

	set_bit(ATL_ST_RINGS_RUNNING, &hw->state);
	netif_tx_start_all_queues(nic->ndev);

	return 0;

stop:
	while (--qvec >= &nic->qvecs[0])
		atl_stop_qvec(qvec);

	return ret;
}

void atl_clear_tdm_cache(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	atl_write_bit(hw, 0x7b00, 0, 1);
	udelay(10);
	atl_write_bit(hw, 0x7b00, 0, 0);
}

void atl_clear_rdm_cache(struct atl_nic *nic)
{
	struct atl_hw *hw = &nic->hw;

	atl_write_bit(hw, 0x5a00, 0, 1);
	udelay(10);
	atl_write_bit(hw, 0x5a00, 0, 0);
}

void atl_stop_rings(struct atl_nic *nic)
{
	struct atl_queue_vec *qvec;

	if (!test_and_clear_bit(ATL_ST_RINGS_RUNNING, &nic->hw.state))
		return;

	netif_tx_stop_all_queues(nic->ndev);

	atl_for_each_qvec(nic, qvec)
		atl_stop_qvec(qvec);

	atl_clear_rdm_cache(nic);
	atl_clear_tdm_cache(nic);
}

int atl_set_features(struct net_device *ndev, netdev_features_t features)
{
	netdev_features_t changed = ndev->features ^ features;

	ndev->features = features;

	if (changed & NETIF_F_LRO)
		atl_set_lro(netdev_priv(ndev));

	return 0;
}

void atl_get_ring_stats(struct atl_desc_ring *ring,
	struct atl_ring_stats *stats)
{
	unsigned int start;

	do {
		start = u64_stats_fetch_begin_irq(&ring->syncp);
		memcpy(stats, &ring->stats, sizeof(*stats));
	} while (u64_stats_fetch_retry_irq(&ring->syncp, start));
}

#define atl_add_stats(_dst, _src)				\
do {								\
	int i;							\
	uint64_t *dst = (uint64_t *)(&(_dst));			\
	uint64_t *src = (uint64_t *)(&(_src));			\
								\
	for (i = 0; i < sizeof(_dst) / sizeof(uint64_t); i++)	\
		dst[i] += src[i];				\
} while (0)

void atl_update_global_stats(struct atl_nic *nic)
{
	int i;
	struct atl_ring_stats stats;

	if (!test_bit(ATL_ST_ENABLED, &nic->hw.state) ||
	    test_bit(ATL_ST_RESETTING, &nic->hw.state) ||
	    !test_bit(ATL_ST_CONFIGURED, &nic->hw.state))
		return;

	memset(&stats, 0, sizeof(stats));

	spin_lock(&nic->stats_lock);

	memset(&nic->stats.rx, 0, sizeof(nic->stats.rx));
	memset(&nic->stats.tx, 0, sizeof(nic->stats.tx));


	for (i = 0; i < nic->nvecs; i++) {
		atl_get_ring_stats(&nic->qvecs[i].rx, &stats);
		atl_add_stats(nic->stats.rx, stats.rx);

		atl_get_ring_stats(&nic->qvecs[i].tx, &stats);
		atl_add_stats(nic->stats.tx, stats.tx);
	}

#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	for (i = 0; i < ATL_NUM_FWD_RINGS; i++) {
		if (atlfwd_nl_is_tx_fwd_ring_created(nic->ndev, i)) {
			atl_fwd_get_ring_stats(nic->fwd.rings[ATL_FWDIR_TX][i],
					       &stats);
			atl_add_stats(nic->stats.tx, stats.tx);
		}
		if (atlfwd_nl_is_rx_fwd_ring_created(nic->ndev, i)) {
			atl_fwd_get_ring_stats(nic->fwd.rings[ATL_FWDIR_RX][i],
					       &stats);
			atl_add_stats(nic->stats.rx, stats.rx);
		}
	}
#endif

	spin_unlock(&nic->stats_lock);
}

void atl_get_stats64(struct net_device *ndev,
	struct rtnl_link_stats64 *nstats)
{
	struct atl_nic *nic = netdev_priv(ndev);
	struct atl_global_stats *stats = &nic->stats;

	atl_update_global_stats(nic);

	nstats->rx_bytes = stats->rx.bytes + stats->rx_fwd.bytes;
	nstats->rx_packets = stats->rx.packets + stats->rx_fwd.packets;
	nstats->tx_bytes = stats->tx.bytes;
	nstats->tx_packets = stats->tx.packets;
	nstats->rx_crc_errors = stats->rx.csum_err;
	nstats->rx_frame_errors = stats->rx.mac_err;
	nstats->rx_errors = nstats->rx_crc_errors + nstats->rx_frame_errors;
	nstats->multicast = stats->rx.multicast;
	nstats->tx_aborted_errors = stats->tx.dma_map_failed;
	nstats->tx_errors = nstats->tx_aborted_errors;
}
