// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/dma-mapping.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <linux/clk.h>

#include "modem_secure_base.h"
#include "ccci_dpmaif_com.h"
#include "ccci_dpmaif_bat.h"
#include "ccci_dpmaif_drv_com.h"
#include "ccci_dpmaif_resv_mem.h"

#include "net_speed.h"
#include "net_pool.h"
#include "md_spd_dvfs_fn.h"
#include "md_spd_dvfs_method.h"
#include "ccci_hif_internal.h"


#define UIDMASK 0x80000000
#define TAG "dpmf"

#ifdef CCCI_KMODULE_ENABLE
/*
 * for debug log:
 * 0 to disable; 1 for print to ram; 2 for print to uart
 * other value to desiable all log
 */
#ifndef CCCI_LOG_LEVEL /* for platform override */
#define CCCI_LOG_LEVEL CCCI_LOG_CRITICAL_UART
#endif
unsigned int ccci_debug_enable = CCCI_LOG_LEVEL;
#endif


unsigned int            g_dpmf_ver;
unsigned int            g_plat_inf;

struct dpmaif_ctrl     *g_dpmaif_ctl;

static atomic_t         g_tx_busy_assert_on;

#ifdef DPMAIF_REDUCE_RX_FLUSH
int                     g_rx_flush_pkt_cnt;
#endif


inline u32 get_ringbuf_used_cnt(u32 len, u32 rdx, u32 wdx)
{
	if (wdx >= rdx)
		return (wdx - rdx);

	return (len - rdx + wdx);
}

inline u32 get_ringbuf_free_cnt(u32 len, u32 rdx, u32 wdx)
{
	if (wdx >= rdx)
		return len - wdx + rdx - 1;

	return (rdx - wdx) - 1;
}

inline u32 get_ringbuf_next_idx(u32 len, u32 idx, u32 cnt)
{
	idx += cnt;

	if (idx >= len)
		idx -= len;

	return idx;
}

inline u32 get_ringbuf_release_cnt(u32 len, u32 rel_idx, u32 rd_idx)
{
	if (rel_idx <= rd_idx)
		return (rd_idx - rel_idx);

	return (len + rd_idx - rel_idx);
}

static void dpmaif_rxq_lro_info_init(struct dpmaif_rx_queue *rxq)
{
	memset(&rxq->lro_info, 0, (sizeof(struct dpmaif_rx_lro_info)));
}

static inline void dpmaif_set_cpu_mask(struct cpumask *cpu_mask,
		u32 cpus, int cpu_nr)
{
	int i;

	cpumask_clear(cpu_mask);

	for (i = 0; i < cpu_nr; i++) {
		if (cpus & (1 << i))
			cpumask_set_cpu(i, cpu_mask);
	}
}

static void dpmaif_handle_wakeup_skb(struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct tcphdr *tcph = NULL;
	struct udphdr *udph = NULL;
	int ip_offset = 0;
	u32 version  = 0;
	u32 protocol = 0;
	u32 src_port = 0;
	u32 dst_port = 0;
	u32 skb_len  = 0;

	if (!skb || !(skb->data))
		goto err;

	iph = (struct iphdr *)skb->data;
	ip6h = (struct ipv6hdr *)skb->data;

	skb_len = skb->len;
	version = iph->version;
	if (version == 4) {
		protocol = iph->protocol;
		ip_offset = (iph->ihl << 2);
	} else if (version == 6) {
		protocol = ip6h->nexthdr;
		ip_offset = 40;
	} else
		goto err;
	if (protocol == IPPROTO_TCP) {
		tcph = (struct tcphdr *)((void *)iph + ip_offset);
		src_port = ntohs(tcph->source);
		dst_port = ntohs(tcph->dest);
	} else if (protocol == IPPROTO_UDP) {
		udph = (struct udphdr *)((void *)iph + ip_offset);
		src_port = ntohs(udph->source);
		dst_port = ntohs(udph->dest);
	} else
		goto err;

err:
	CCCI_NORMAL_LOG(0, TAG,
		"[%s] ver: %u; pro: %u; spt: %u; dpt: %u; len: %u\n",
		__func__, version, protocol, src_port, dst_port, skb_len);
}

static void tx_force_md_assert(char buf[])
{
	if (atomic_inc_return(&g_tx_busy_assert_on) <= 1) {
		CCCI_NORMAL_LOG(0, TAG,
			"[%s] error: force assert md, because: %s\n",
			__func__, buf);
		ccci_md_force_assert(MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED, "TX", 3);
	}
}

static inline int dpmaif_skb_gro_receive(struct sk_buff *p, struct sk_buff *skb)
{
	struct skb_shared_info *pinfo, *skbinfo = skb_shinfo(skb);
	unsigned int offset = skb_gro_offset(skb);
	unsigned int headlen = skb_headlen(skb);
	unsigned int len = skb_gro_len(skb);
	unsigned int delta_truesize;
	struct sk_buff *lp;

	if (unlikely(p->len + len >= 65536 || NAPI_GRO_CB(skb)->flush))
		return -1;

	lp = NAPI_GRO_CB(p)->last;
	pinfo = skb_shinfo(lp);

	if (headlen <= offset) {
		skb_frag_t *frag;
		skb_frag_t *frag2;
		int i = skbinfo->nr_frags;
		int nr_frags = pinfo->nr_frags + i;

		if (nr_frags > MAX_SKB_FRAGS)
			goto merge;

		offset -= headlen;
		pinfo->nr_frags = nr_frags;
		skbinfo->nr_frags = 0;

		frag = pinfo->frags + nr_frags;
		frag2 = skbinfo->frags + i;
		do {
			*--frag = *--frag2;
		} while (--i);

		skb_frag_off_add(frag, offset);
		skb_frag_size_sub(frag, offset);

		/* all fragments truesize : remove (head size + sk_buff) */
		delta_truesize = skb->truesize -
				 SKB_TRUESIZE(skb_end_offset(skb));

		skb->truesize -= skb->data_len;
		skb->len -= skb->data_len;
		skb->data_len = 0;

		NAPI_GRO_CB(skb)->free = NAPI_GRO_FREE;
		goto done;

	} else if (skb->head_frag) {
		int nr_frags = pinfo->nr_frags;
		skb_frag_t *frag = pinfo->frags + nr_frags;
		struct page *page = virt_to_head_page(skb->head);
		unsigned int first_size = headlen - offset;
		unsigned int first_offset;

		if (nr_frags + 1 + skbinfo->nr_frags > MAX_SKB_FRAGS)
			goto merge;

		first_offset = skb->data -
			       (unsigned char *)page_address(page) +
			       offset;

		pinfo->nr_frags = nr_frags + 1 + skbinfo->nr_frags;

		__skb_frag_set_page(frag, page);
		skb_frag_off_set(frag, first_offset);
		skb_frag_size_set(frag, first_size);

		memcpy(frag + 1, skbinfo->frags, sizeof(*frag) * skbinfo->nr_frags);
		/* We dont need to clear skbinfo->nr_frags here */

		delta_truesize = skb->truesize - SKB_DATA_ALIGN(sizeof(struct sk_buff));
		NAPI_GRO_CB(skb)->free = NAPI_GRO_FREE_STOLEN_HEAD;
		goto done;
	}

merge:
	delta_truesize = skb->truesize;
	if (offset > headlen) {
		unsigned int eat = offset - headlen;

		skb_frag_off_add(&skbinfo->frags[0], eat);
		skb_frag_size_sub(&skbinfo->frags[0], eat);
		skb->data_len -= eat;
		skb->len -= eat;
		offset = headlen;
	}

	__skb_pull(skb, offset);

	if (NAPI_GRO_CB(p)->last == p)
		skb_shinfo(p)->frag_list = skb;
	else
		NAPI_GRO_CB(p)->last->next = skb;
	NAPI_GRO_CB(p)->last = skb;
	__skb_header_release(skb);
	lp = p;

done:
	NAPI_GRO_CB(p)->count++;
	p->data_len += len;
	p->truesize += delta_truesize;
	p->len += len;
	if (lp != p) {
		lp->data_len += len;
		lp->truesize += delta_truesize;
		lp->len += len;
	}
	NAPI_GRO_CB(skb)->same_flow = 1;
	return 0;
}

static inline void dpmaif_rxq_lro_handle_bid(
		struct dpmaif_rx_queue *rxq, int free_skb)
{
	int i;
	struct dpmaif_bat_skb *bat_skb;
	struct dpmaif_rx_lro_info *lro_info = &rxq->lro_info;

	for (i = 0; i < lro_info->count; i++) {
		if (free_skb)
			dev_kfree_skb_any(lro_info->data[i].skb);

		bat_skb = ((struct dpmaif_bat_skb *)
				dpmaif_ctl->bat_skb->bat_pkt_addr +
				lro_info->data[i].bid);
		bat_skb->skb = NULL;
	}

	lro_info->count = 0;
}

static inline void dpmaif_rxq_push_all_skb(struct dpmaif_rx_queue *rxq)
{
	struct lhif_header *lhif_h = NULL;
	struct dpmaif_rx_lro_info *lro_info = &rxq->lro_info;
	struct sk_buff *skb;
	int i;

	for (i = 0; i < lro_info->count; i++) {
		skb = lro_info->data[i].skb;

		lhif_h = (struct lhif_header *)(skb_push(skb,
				sizeof(struct lhif_header)));
		lhif_h->netif = rxq->cur_chn_idx;

		ccci_dl_enqueue(rxq->index, skb);
	}

	dpmaif_rxq_lro_handle_bid(rxq, 0);
}

static void dpmaif_calc_ip_len_and_check(struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;

	if (skb && skb->data) {
		iph = (struct iphdr *)skb->data;

		if (iph->version == 4) {
			iph->tot_len = htons(skb->len);
			iph->check = 0;
			iph->check = ip_fast_csum((const void *)iph, iph->ihl);

		} else if (iph->version == 6) {
			ip6h = (struct ipv6hdr *)iph;
			ip6h->payload_len = htons(skb->len - 40);

		}
	}
}

static inline void dpmaif_rxq_lro_join_skb(
		struct dpmaif_rx_queue *rxq,
		struct sk_buff *skb,
		unsigned int bid, unsigned int header_offset)
{
	struct dpmaif_rx_lro_info *lro_info = &rxq->lro_info;

	if (lro_info->count >= DPMAIF_MAX_LRO) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] lro skb count is too much.\n",
			__func__);

		dpmaif_rxq_push_all_skb(rxq);
		lro_info->count = 0;
	}

	lro_info->data[lro_info->count].bid = bid;
	lro_info->data[lro_info->count].skb = skb;
	lro_info->data[lro_info->count].hof = header_offset;

	lro_info->count++;
}

static inline void dpmaif_lro_update_gro_info(
		struct sk_buff *skb,
		unsigned int total_len,
		int gro_skb_num,
		unsigned int mss)
{
	struct iphdr *iph = (struct iphdr *)skb->data;
	struct ipv6hdr *ip6h = (struct ipv6hdr *)skb->data;
	unsigned int gso_type;

	if (iph->version == 4) {
		gso_type = SKB_GSO_TCPV4;

		if (unlikely(total_len)) {
			iph->tot_len = htons(total_len);
			iph->check = 0;
			iph->check = ip_fast_csum((const void *)iph, iph->ihl);
		}

	} else if (iph->version == 6) {
		gso_type = SKB_GSO_TCPV6;

		if (unlikely(total_len))
			ip6h->payload_len = htons(total_len - 40);

	} else
		return;

	skb->ip_summed = CHECKSUM_UNNECESSARY;

	skb_shinfo(skb)->gso_type |= gso_type;
	skb_shinfo(skb)->gso_size = mss;
	skb_shinfo(skb)->gso_segs = gro_skb_num;
}

static inline void dpmaif_rxq_lro_end(struct dpmaif_rx_queue *rxq)
{
	struct lhif_header *lhif_h = NULL;
	struct sk_buff *skb0 = NULL, *skb1 = NULL;
	int i, start_idx = 0, data_len = 0;
	unsigned int total_len = 0, lro_num = 0;
	unsigned int hof, max_mss = 0, first_skb_len = 0, no_need_gro = 0;
	struct dpmaif_rx_lro_info *lro_info = &rxq->lro_info;

	if (rxq->pit_dp) {
		dpmaif_rxq_lro_handle_bid(rxq, 1);
		return;
	}

	if (lro_info->count == 0) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: pit data abnormal; lro count is 0\n", __func__);
		return;
	}

lro_continue:
	for (i = start_idx; i < lro_info->count; i++) {
		if (i == start_idx) {
			/* this is the first skb */
			skb0 = lro_info->data[i].skb;
			lro_num = 1;

			if ((lro_info->count - i) == 1)
				break;

			total_len = skb0->len;
			NAPI_GRO_CB(skb0)->data_offset = 0;
			NAPI_GRO_CB(skb0)->last = skb0;

			max_mss = 0;
			no_need_gro = 0;
			first_skb_len = skb0->len;

			continue;
		}

		skb1 = lro_info->data[i].skb;
		if (unlikely(skb1->len > first_skb_len)) {
			/* this is a wrong lro skb, push the previous lro skb */
			start_idx = i;
			if (lro_num == 1) {
				dpmaif_calc_ip_len_and_check(skb0);
				NAPI_GRO_CB(skb0)->last = NULL;
			}

			goto gro_too_much_skb;
		}
		if (unlikely(skb1->len < first_skb_len))
			no_need_gro = 1;

		hof = lro_info->data[i].hof;
		data_len = skb1->len - hof;
		max_mss = first_skb_len - hof;

		NAPI_GRO_CB(skb1)->data_offset = hof;

		if (unlikely(dpmaif_skb_gro_receive(skb0, skb1))) {
			start_idx = i;
			goto gro_too_much_skb;
		}

		total_len += data_len;
		lro_num++;

		if (unlikely(no_need_gro)) {
			start_idx = i + 1;
			goto gro_too_much_skb;
		}
	}
	start_idx = lro_info->count;

gro_too_much_skb:
	if (lro_num > 1) {
		if (likely(lro_num == lro_info->count))
			total_len = 0;

		dpmaif_lro_update_gro_info(skb0, total_len, lro_num, max_mss);
	}

	if (atomic_cmpxchg(&dpmaif_ctl->wakeup_src, 1, 0) == 1) {
		CCCI_NOTICE_LOG(0, TAG,
			"DPMA_MD wakeup source:(%d/%d)\n",
			rxq->index, rxq->cur_chn_idx);
		dpmaif_handle_wakeup_skb(skb0);
	}

	if (g_debug_flags & DEBUG_RX_DONE_SKB) {
		struct debug_rx_done_skb_hdr hdr;

		hdr.type = TYPE_RX_DONE_SKB_ID;
		hdr.qidx = rxq->index;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.bid  = rxq->skb_idx;
		hdr.len  = skb0->len;
		hdr.cidx = rxq->cur_chn_idx;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}

	lhif_h = (struct lhif_header *)(skb_push(skb0,
			sizeof(struct lhif_header)));
	lhif_h->netif = rxq->cur_chn_idx;

	ccci_dl_enqueue(rxq->index, skb0);

	if (start_idx < lro_info->count)
		goto lro_continue;

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_ctl->rx_tfc_pkgs[rxq->index] += lro_info->count;
#endif

	dpmaif_rxq_lro_handle_bid(rxq, 0);

	return;
}

static int dpmaif_read_infra_ao_mem_base(struct device *dev)
{
	struct device_node *node = NULL;

	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao_mem");
	if (node == NULL) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: No infracfg_ao_mem node in dtsi\n",
			__func__);
		return -1;
	}

	dpmaif_ctl->infra_ao_mem_base = of_iomap(node, 0);
	if (!dpmaif_ctl->infra_ao_mem_base) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: iomap infracfg_ao_mem fail.\n",
			__func__);
		return -1;
	}

	CCCI_ERROR_LOG(0, TAG,
		"[%s] infra_ao_mem_base: 0x%p\n",
		__func__, dpmaif_ctl->infra_ao_mem_base);

	return 0;
}

static int dpmaif_init_clk(struct device *dev, struct dpmaif_clk_node *clk)
{
	if ((!dev) || (!clk)) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: dev(%p) or clk(%p) is NULL.\n",
			__func__, dev, clk);
		return -1;
	}

	while (clk->clk_name) {
		clk->clk_ref = devm_clk_get(dev, clk->clk_name);
		if (IS_ERR(clk->clk_ref)) {
			CCCI_ERROR_LOG(0, TAG,
				 "[%s] error: dpmaif get %s failed.\n",
				 __func__, clk->clk_name);

			clk->clk_ref = NULL;
			return -1;
		}

		clk += 1;
	}

	return 0;
}

static void dpmaif_set_clk(unsigned int on, struct dpmaif_clk_node *clk)
{
	int ret;

	if (!clk) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: clk is NULL.\n", __func__);
		return;
	}

	while (clk->clk_name) {
		if (!clk->clk_ref) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: clock: %s is NULL.\n",
				__func__, clk->clk_name);

			clk += 1;
			continue;
		}

		if (on) {
			ret = clk_prepare_enable(clk->clk_ref);
			if (ret)
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: prepare %s fail. %d\n",
					__func__, clk->clk_name, ret);

		} else
			clk_disable_unprepare(clk->clk_ref);

		clk += 1;
	}
}

static void dpmaif_dump_txq_data(int qno)
{
	int i = 0;
	struct dpmaif_tx_queue *txq;

	if ((qno < 0) || (qno >= DPMAIF_TXQ_NUM))  //dump all txq
		qno = -1;

	for (; i < DPMAIF_TXQ_NUM; i++) {
		if ((qno >= 0) && (i != qno))
			continue;

		txq = &dpmaif_ctl->txq[i];

		CCCI_MEM_LOG_TAG(0, TAG, "dpmaif: dump txq(%d): data ------->\n", i);

		CCCI_MEM_LOG(0, TAG,
			"dpmaif: drb(%d) base: 0x%p(%zu*%d); txq pos: w/r/rel=(%u, %u, %u)\n",
			txq->index, txq->drb_base, sizeof(struct dpmaif_drb_pd), txq->drb_cnt,
			atomic_read(&txq->drb_wr_idx), atomic_read(&txq->drb_rd_idx),
			atomic_read(&txq->drb_rel_rd_idx));

		ccci_util_mem_dump(CCCI_DUMP_MEM_DUMP,
			txq->drb_base, txq->drb_cnt * sizeof(struct dpmaif_drb_pd));
	}
}

static void dpmaif_dump_rxq_data(int qno)
{
	int i = 0;
	struct dpmaif_rx_queue *rxq;

	if ((qno < 0) || (qno >= dpmaif_ctl->real_rxq_num))  //dump all rxq
		qno = -1;

	for (; i < dpmaif_ctl->real_rxq_num; i++) {
		if ((qno >= 0) && (i != qno))
			continue;

		rxq = &dpmaif_ctl->rxq[i];

		CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG,
			"dpmaif: dump rxq(%d) data ------->\n", i);

		/* PIT mem dump */
		CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG,
			"dpmaif: pit(%d) base: 0x%p(%d*%d); pit pos: w/r=(%u,%u)\n",
			rxq->index, rxq->pit_base, drv.normal_pit_size, rxq->pit_cnt,
			atomic_read(&rxq->pit_wr_idx), atomic_read(&rxq->pit_rd_idx));

		ccci_util_mem_dump(CCCI_DUMP_DPMAIF,
			rxq->pit_base, rxq->pit_cnt * drv.normal_pit_size);
	}

	CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG, "dpmaif: dump bat skb data ------->\n");

	if (dpmaif_ctl->bat_skb) {
		CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG,
			"dpmaif: bat skb base: 0x%p(%d*%d); bat skb pos: w/r=(%u,%u)\n",
			dpmaif_ctl->bat_skb->bat_base,
			(int)sizeof(struct dpmaif_bat_base), dpmaif_ctl->bat_skb->bat_cnt,
			atomic_read(&dpmaif_ctl->bat_skb->bat_wr_idx),
			atomic_read(&dpmaif_ctl->bat_skb->bat_rd_idx));

		ccci_util_mem_dump(CCCI_DUMP_DPMAIF,
			dpmaif_ctl->bat_skb->bat_base,
			dpmaif_ctl->bat_skb->bat_cnt * sizeof(struct dpmaif_bat_base));
	}
}

static inline unsigned int dpmaif_get_rxq_pit_read_cnt(struct dpmaif_rx_queue *rxq)
{
	unsigned int pit_wr_idx = ops.drv_dl_get_wridx(rxq->index);

	atomic_set(&rxq->pit_wr_idx, pit_wr_idx);

	return get_ringbuf_used_cnt(rxq->pit_cnt,
			atomic_read(&rxq->pit_rd_idx), pit_wr_idx);
}

static inline void rxq_pit_cache_memory_flush(struct dpmaif_rx_queue *rxq,
	unsigned short read_cnt)
{
	dma_addr_t cache_start_addr;
	unsigned int cur_pit = atomic_read(&rxq->pit_rd_idx);

	/* flush pit base memory cache for read pit data */
	cache_start_addr = rxq->pit_phy_addr + ((dma_addr_t)drv.normal_pit_size * cur_pit);

	if ((cur_pit + read_cnt) <= rxq->pit_cnt) {
		dma_sync_single_for_cpu(dpmaif_ctl->dev, cache_start_addr,
			drv.normal_pit_size * read_cnt, DMA_FROM_DEVICE);

	} else {
		dma_sync_single_for_cpu(dpmaif_ctl->dev, cache_start_addr,
			drv.normal_pit_size * (rxq->pit_cnt - cur_pit),
			DMA_FROM_DEVICE);

		dma_sync_single_for_cpu(dpmaif_ctl->dev, rxq->pit_phy_addr,
			drv.normal_pit_size * (cur_pit + read_cnt - rxq->pit_cnt),
			DMA_FROM_DEVICE);
	}
}

static inline void dpmaif_rxq_handle_msg_pit(struct dpmaif_rx_queue *rxq,
	struct dpmaif_msg_pit_base *msg_pit)
{
	rxq->cur_chn_idx = msg_pit->channel_id;
	rxq->check_sum = msg_pit->check_sum;
	rxq->pit_dp = msg_pit->dp;

	rxq->skb_idx = -1;
}

static inline int dpmaif_rxq_set_data_to_skb(struct dpmaif_rx_queue *rxq,
	unsigned int buffer_id, struct dpmaif_normal_pit_v2 *nml_pit_v2)
{
	struct sk_buff *skb;
	struct dpmaif_bat_skb *cur_skb_info =
		((struct dpmaif_bat_skb *)(dpmaif_ctl->bat_skb->bat_pkt_addr) + buffer_id);
	unsigned int header_offset = 0;

	skb = cur_skb_info->skb;

	/* 3. record to skb for user: wapper, enqueue */
	 /* get skb which data contained pkt data */
	if (skb == NULL) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: get null skb(0x%x) from skb table.\n",
			__func__, buffer_id);
		return DATA_CHECK_FAIL;
	}

	/* rx current skb data unmapping */
	dma_unmap_single(dpmaif_ctl->dev, cur_skb_info->data_phy_addr,
		cur_skb_info->data_len, DMA_FROM_DEVICE);

	skb->len = 0;
	skb_reset_tail_pointer(skb);

	/* for debug: */
	if (unlikely((skb->tail + nml_pit_v2->data_len) > skb->end)) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: pkt(%d/%d): len = 0x%x, skb(%p, %p, 0x%x, 0x%x)\n",
			__func__, atomic_read(&rxq->pit_rd_idx), buffer_id, nml_pit_v2->data_len,
			skb->head, skb->data, skb->tail, skb->end);

		return DATA_CHECK_FAIL;
	}

	if (dpmaif_ctl->support_2rxq) {
		header_offset = (((struct dpmaif_normal_pit_v3 *)nml_pit_v2)->header_offset << 2);
		dpmaif_rxq_lro_join_skb(rxq, skb, buffer_id, header_offset);
		skb_put(skb, nml_pit_v2->data_len + header_offset);
	} else
		skb_put(skb, nml_pit_v2->data_len);

	skb->ip_summed = (rxq->check_sum == 0) ? CHECKSUM_UNNECESSARY : CHECKSUM_NONE;

	return 0;
}

static inline int dpmaif_rxq_set_frg_to_skb(struct dpmaif_rx_queue *rxq,
	unsigned int buffer_id, struct dpmaif_normal_pit_v2 *nml_pit_v2)
{
	struct dpmaif_bat_skb *cur_skb_info =
		((struct dpmaif_bat_skb *)(dpmaif_ctl->bat_skb->bat_pkt_addr) + rxq->skb_idx);
	struct dpmaif_bat_page *cur_page_info =
		((struct dpmaif_bat_page *)(dpmaif_ctl->bat_frg->bat_pkt_addr) + buffer_id);
	struct sk_buff *skb = cur_skb_info->skb;
	struct page *page = cur_page_info->page;

	if (!page) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: frag check fail; buffer_id:%u; skb_idx: %d\n",
			__func__, buffer_id, rxq->skb_idx);
		return DATA_CHECK_FAIL;
	}

	/* rx current frag data unmapping */
	dma_unmap_page(dpmaif_ctl->dev, cur_page_info->data_phy_addr,
		cur_page_info->data_len, DMA_FROM_DEVICE);

	/* 3. record to skb for user: fragment data to nr_frags */
	skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
		page, cur_page_info->offset,
		nml_pit_v2->data_len, cur_page_info->data_len);

	cur_page_info->page = NULL;

	return 0;
}

static inline void dpmaif_rxq_handle_skb_wakeup(struct dpmaif_rx_queue *rxq,
		struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct tcphdr *tcph = NULL;
	struct udphdr *udph = NULL;
	int ip_offset = 0;
	u32 version  = 0;
	u32 protocol = 0;
	u32 src_port = 0;
	u32 dst_port = 0;
	u32 skb_len  = 0;

	if (!skb || !(skb->data))
		goto err;

	iph = (struct iphdr *)skb->data;
	ip6h = (struct ipv6hdr *)skb->data;

	skb_len = skb->len;
	version = iph->version;

	if (version == 4) {
		protocol = iph->protocol;
		ip_offset = (iph->ihl << 2);

	} else if (version == 6) {
		protocol = ip6h->nexthdr;
		ip_offset = 40;

	} else
		goto err;

	if (protocol == IPPROTO_TCP) {
		tcph = (struct tcphdr *)((void *)iph + ip_offset);
		src_port = ntohs(tcph->source);
		dst_port = ntohs(tcph->dest);

	} else if (protocol == IPPROTO_UDP) {
		udph = (struct udphdr *)((void *)iph + ip_offset);
		src_port = ntohs(udph->source);
		dst_port = ntohs(udph->dest);

	}

err:
	CCCI_NORMAL_LOG(0, TAG, "[%s] ver: %u; pro: %u; spt: %u; dpt: %u; len: %u\n",
		__func__, version, protocol, src_port, dst_port, skb_len);
}

static inline void dpmaif_rxq_add_skb_to_rx_push_thread(struct dpmaif_rx_queue *rxq)
{
	struct dpmaif_bat_skb *cur_skb_info =
		((struct dpmaif_bat_skb *)(dpmaif_ctl->bat_skb->bat_pkt_addr) + rxq->skb_idx);
	struct lhif_header *lhif_h = NULL;

	if ((g_dpmf_ver > 1) && rxq->pit_dp) {
		dev_kfree_skb_any(cur_skb_info->skb);
		goto send_end;
	}

	if (atomic_cmpxchg(&dpmaif_ctl->wakeup_src, 1, 0) == 1) {
		CCCI_NORMAL_LOG(0, TAG,
			"[%s] DPMA_MD wakeup source:(%d/%d)\n",
			__func__, rxq->index, rxq->cur_chn_idx);

		dpmaif_rxq_handle_skb_wakeup(rxq, cur_skb_info->skb);
	}

	if (g_debug_flags & DEBUG_RX_DONE_SKB) {
		struct debug_rx_done_skb_hdr hdr;

		hdr.type = TYPE_RX_DONE_SKB_ID;
		hdr.qidx = rxq->index;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.bid  = rxq->skb_idx;
		hdr.len  = cur_skb_info->skb->len;
		hdr.cidx = rxq->cur_chn_idx;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}

	/* md put the ccmni_index to the msg pkt,
	 * so we need push it by self. maybe no need
	 */
	lhif_h = (struct lhif_header *)(skb_push(cur_skb_info->skb, sizeof(struct lhif_header)));
	lhif_h->netif = rxq->cur_chn_idx;

	ccci_dl_enqueue(rxq->index, cur_skb_info->skb);

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_ctl->rx_tfc_pkgs[rxq->index]++;
#endif

send_end:
	cur_skb_info->skb = NULL;
}

static inline int dpmaif_rxq_handle_normal_pit(struct dpmaif_rx_queue *rxq,
	struct dpmaif_normal_pit_v2 *nml_pit_v2)
{
	struct dpmaif_normal_pit_v3 *nml_pit_v3 =
		(struct dpmaif_normal_pit_v3 *)nml_pit_v2;
	unsigned int buffer_id;
	int ret = 0;

	if (g_dpmf_ver == 3)
		buffer_id = (nml_pit_v3->buffer_id | (nml_pit_v3->h_bid << 13));
	else
		buffer_id = nml_pit_v2->buffer_id;

	if (nml_pit_v2->buffer_type != PKT_BUF_FRAG) {  //is skb linear data
		rxq->skb_idx = (int)buffer_id;

		ret = dpmaif_rxq_set_data_to_skb(rxq, buffer_id, nml_pit_v2);
		if (ret < 0)
			return ret;

	} else {  //is skb fragment data
		if (rxq->skb_idx < 0) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: skb_idx < 0; buffer_id: %u\n",
				__func__, buffer_id);
			return DATA_CHECK_FAIL;
		}

		ret = dpmaif_rxq_set_frg_to_skb(rxq, buffer_id, nml_pit_v2);
		if (ret < 0)
			return ret;
	}

	if (nml_pit_v2->c_bit == 0) {  //is last one, push skb to rx push queue
		if (dpmaif_ctl->support_lro)
			dpmaif_rxq_lro_end(rxq);
		else
			dpmaif_rxq_add_skb_to_rx_push_thread(rxq);

		rxq->enqueue_skb_cnt++;
	}

	return 0;
}

static inline int dpmaifq_rxq_notify_hw_pit_cnt(struct dpmaif_rx_queue *rxq,
	unsigned short pit_remain_cnt)
{
	int ret = 0;

	if (rxq->started == false)
		return 0;

	ret = rxq->rxq_drv_dl_add_pit_remain_cnt(pit_remain_cnt);
	if (ret < 0) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error(%d): update pit cnt fail\n",
			__func__, ret);
		return ret;
	}

	if (g_dpmf_ver == 2) {
		ret = ccci_drv2_rxq_update_apit_dummy(rxq);
		if (ret)
			return ret;
	}

	return 0;
}

#define NOTIFY_RX_PUSH(rxq)  wake_up_all(&rxq->rxq_wq)

static int dpmaif_rxq_start_read_from_pit(struct dpmaif_rx_queue *rxq,
	unsigned short read_cnt)
{
	unsigned long time_limit = jiffies + msecs_to_jiffies(20);
	int rx_cnt, ret = 0, recv_skb_cnt = 0;
	struct dpmaif_normal_pit_v2 *nml_pit_v2;
	unsigned short pit_hw_update_cnt = 0, pit_rd_idx;
	unsigned int pit_size = drv.normal_pit_size;

	rxq_pit_cache_memory_flush(rxq, read_cnt);

#ifdef DPMAIF_REDUCE_RX_FLUSH
	atomic_set(&rxq->rxq_need_flush, 0);
#endif

	pit_rd_idx = atomic_read(&rxq->pit_rd_idx);

	if (g_debug_flags & DEBUG_RX_START) {
		struct debug_rx_start_hdr hdr;

		hdr.type = TYPE_RX_START_ID;
		hdr.qidx = rxq->index;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.pcnt = read_cnt;
		hdr.ridx = (u16)pit_rd_idx;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}

	rxq->enqueue_skb_cnt = 0;

	for (rx_cnt = 0; rx_cnt < read_cnt; rx_cnt++) {
		if (time_after_eq(jiffies, time_limit))
			break;

		nml_pit_v2 = (struct dpmaif_normal_pit_v2 *)(rxq->pit_base +
				(pit_rd_idx * pit_size));

		if (nml_pit_v2->packet_type == DES_PT_MSG) {  //is message pit
			dpmaif_rxq_handle_msg_pit(rxq, (struct dpmaif_msg_pit_base *)nml_pit_v2);

		} else {  //is normal pit
			if (g_dpmf_ver == 2 && nml_pit_v2->ig)
				ccci_drv2_rxq_handle_ig(rxq, nml_pit_v2);

			else {
				ret = dpmaif_rxq_handle_normal_pit(rxq, nml_pit_v2);
				if (ret)
					goto occur_err;
			}

			recv_skb_cnt++;
			if ((rxq->enqueue_skb_cnt > 1) && (recv_skb_cnt > 14)) {
				NOTIFY_RX_PUSH(rxq);
				recv_skb_cnt = 0;
				rxq->enqueue_skb_cnt = 0;
			}
		}

		pit_rd_idx = get_ringbuf_next_idx(rxq->pit_cnt, pit_rd_idx, 1);

		pit_hw_update_cnt++;

		if (pit_hw_update_cnt == 0x40)
			ccci_dpmaif_bat_wakeup_thread(0x40);

		if (pit_hw_update_cnt == 0x80) {
			ccci_dpmaif_bat_wakeup_thread(0x40);
			ret = dpmaifq_rxq_notify_hw_pit_cnt(rxq, pit_hw_update_cnt);
			if (ret)
				goto occur_err;
			pit_hw_update_cnt = 0;
		}
	}  //for()

	atomic_set(&rxq->pit_rd_idx, pit_rd_idx);
#ifdef DPMAIF_REDUCE_RX_FLUSH
	atomic_set(&rxq->rxq_need_flush, 1);
#endif

	if (recv_skb_cnt)
		NOTIFY_RX_PUSH(rxq);

	/* update to HW */
	if (pit_hw_update_cnt) {
		ccci_dpmaif_bat_wakeup_thread(0);
		ret = dpmaifq_rxq_notify_hw_pit_cnt(rxq, pit_hw_update_cnt);
		if (ret)
			goto occur_err;
	}

	return rx_cnt;

occur_err:
	atomic_set(&rxq->pit_rd_idx, pit_rd_idx);

	CCCI_ERROR_LOG(0, TAG,
		"[%s] error(%d): pit_rd_idx: %u; (%d/%d)\n",
		__func__, ret, pit_rd_idx, rx_cnt, read_cnt);

	dpmaif_dump_rxq_data(rxq->index);

	return ret;
}

static inline void dpmaif_updata_max_bat_skb_cnt(struct dpmaif_rx_queue *rxq)
{
	unsigned int max_bat_skb_cnt = 0;

	max_bat_skb_cnt = DPMA_READ_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW);
	if (max_bat_skb_cnt & DPMAIF_DL_MAX_BAT_SKB_CNT_STS) {
		DPMA_WRITE_PD_DL(NRL2_DPMAIF_DL_RESERVE_RW,
				(max_bat_skb_cnt & (~DPMAIF_DL_MAX_BAT_SKB_CNT_STS)));

		max_bat_skb_cnt = ((max_bat_skb_cnt & DPMAIF_DL_MAX_BAT_SKB_CNT_MSK) >> 16);
		if ((max_bat_skb_cnt >= MIN_ALLOC_SKB_CNT) &&
				(g_max_bat_skb_cnt_for_md != max_bat_skb_cnt)) {
			g_max_bat_skb_cnt_for_md = max_bat_skb_cnt;

			if (g_debug_flags & DEBUG_MAX_SKB_CNT) {
				struct debug_max_skb_cnt_hdr hdr = {0};

				hdr.type = TYPE_MAX_SKB_CNT_ID;
				hdr.qidx = rxq->index;
				hdr.time = (unsigned int)(local_clock() >> 16);
				hdr.value = (u16)max_bat_skb_cnt;
				ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
			}
		}
	}
}

static int dpmaif_rxq_data_collect(struct dpmaif_rx_queue *rxq)
{
	int ret = ALL_CLEAR, real_cnt = 0;
	unsigned int L2RISAR0, rd_cnt;

	if (rxq->index == 0)
		dpmaif_updata_max_bat_skb_cnt(rxq);

	rd_cnt = dpmaif_get_rxq_pit_read_cnt(rxq);
	if (rd_cnt) {
		real_cnt = dpmaif_rxq_start_read_from_pit(rxq, rd_cnt);
		if (real_cnt >= 0) {
			/* hw interrupt ack. */
			L2RISAR0 = ccci_drv_get_dl_isr_event();
			if (dpmaif_ctl->support_2rxq) {
				if (rxq->index == 0)
					L2RISAR0 &= DP_DL_INT_LRO0_QDONE_SET;
				else
					L2RISAR0 &= DP_DL_INT_LRO1_QDONE_SET;
			} else
				L2RISAR0 &= DPMAIF_DL_INT_QDONE_MSK;

			if (L2RISAR0) {
				DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_DL_L2TISAR0, L2RISAR0);
				ret = ONCE_MORE;
			} else if (real_cnt == rd_cnt)
				ret = ALL_CLEAR;
			else
				ret = ONCE_MORE;

		} else if (real_cnt < LOW_MEMORY_TYPE_MAX) {
			ret = LOW_MEMORY;
			CCCI_ERROR_LOG(-1, TAG, "[%s] error: rx low mem: %d\n",
				__func__, real_cnt);

		} else if (real_cnt <= ERROR_STOP_MAX) {
			ret = ERROR_STOP;
			CCCI_ERROR_LOG(-1, TAG, "[%s] error: rx ERR_STOP: %d\n",
				__func__, real_cnt);
		}
	}

	return ret;
}

static void dpmaif_rxq_tasklet(unsigned long data)
{
	struct dpmaif_rx_queue *rxq = (struct dpmaif_rx_queue *)data;
	int ret = 0;

	atomic_set(&rxq->rxq_processing, 1);

	smp_mb(); /* for cpu exec. */

	if (rxq->started == false)
		goto processing_done;

	ret = dpmaif_rxq_data_collect(rxq);
	if (ret == ALL_CLEAR) {
		/* clear IP busy register wake up cpu case */
		ccci_drv_clear_ip_busy();
		rxq->rxq_drv_unmask_dl_interrupt();

	} else if (ret == ONCE_MORE) {
		atomic_set(&rxq->rxq_processing, 0);
		tasklet_hi_schedule(&rxq->rxq_task);
		return;

	} else
		ccci_md_force_assert(MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED, NULL, 0);

processing_done:
	atomic_set(&rxq->rxq_processing, 0);
}

static int dpmaif_rxq_push_thread(void *arg)
{
	struct dpmaif_rx_queue *rxq = (struct dpmaif_rx_queue *)arg;
	struct sk_buff *skb = NULL;
	int ret, hif_id = dpmaif_ctl->hif_id, qno = rxq->index;
	struct debug_rx_push_skb_hdr hdr = {0};
	u16 pkg_count = 0;
#ifdef DPMAIF_REDUCE_RX_FLUSH
	int need_rx_flush = 0;
#endif

	while (1) {
		if (ccci_dl_queue_len(qno) == 0) {
#ifdef DPMAIF_REDUCE_RX_FLUSH
			need_rx_flush = atomic_read(&rxq->rxq_need_flush);
			if (need_rx_flush || (pkg_count > g_rx_flush_pkt_cnt)) {
				pkg_count = 0;
				ccci_port_queue_status_notify(hif_id, qno, IN, RX_FLUSH);
			}
			if (!need_rx_flush)
				continue;
#else
			pkg_count = 0;
			ccci_port_queue_status_notify(hif_id, qno, IN, RX_FLUSH);
#endif
			ret = wait_event_interruptible(rxq->rxq_wq,
				(ccci_dl_queue_len(qno) || kthread_should_stop()));

			ccmni_clr_flush_timer();

			if (ret == -ERESTARTSYS)
				continue;
		}

		if (kthread_should_stop())
			break;

		skb = (struct sk_buff *)ccci_dl_dequeue(qno);
		if (!skb)
			continue;

		mtk_ccci_add_dl_pkt_bytes(qno, skb->len);

		if (g_debug_flags & DEBUG_RX_PUSH_SKB) {
			hdr.type = TYPE_RX_PUSH_SKB_ID;
			hdr.qidx = qno;
			hdr.time = (unsigned int)(local_clock() >> 16);
			hdr.ipid = ((struct iphdr *)(skb->data + sizeof(struct lhif_header)))->id;
			hdr.fcnt = pkg_count;
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
		}

		ccci_port_recv_skb(hif_id, skb, CLDMA_NET_DATA);

		pkg_count++;
	}

	return 0;
}

static void dpmaif_disable_rxq_irq(struct dpmaif_rx_queue *rxq)
{
	if (atomic_cmpxchg(&rxq->irq_enabled, 1, 0) == 1)
		disable_irq(rxq->irq_id);
}

static void dpmaif_disable_all_irq(void)
{
	int i;

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++)
		dpmaif_disable_rxq_irq(&dpmaif_ctl->rxq[i]);
}

static void dpmaif_enable_rxq_irq(struct dpmaif_rx_queue *rxq)
{
	if (atomic_cmpxchg(&rxq->irq_enabled, 0, 1) == 0)
		enable_irq(rxq->irq_id);
}

static void dpmaif_enable_all_irq(void)
{
	int i;

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++)
		dpmaif_enable_rxq_irq(&dpmaif_ctl->rxq[i]);
}

static int dpmaif_rxq_init_buf(struct dpmaif_rx_queue *rxq)
{
	int ret = 0;

	/* PIT buffer init */
	rxq->pit_cnt = dpmaif_ctl->dl_pit_entry_size;
	/* alloc buffer for HW && AP SW */

	if (!ccci_dpmaif_get_resv_cache_mem(&rxq->pit_base, &rxq->pit_phy_addr,
			(rxq->pit_cnt * dpmaif_ctl->dl_pit_byte_size)))
		return 0;

	rxq->pit_base = kzalloc((rxq->pit_cnt * dpmaif_ctl->dl_pit_byte_size), GFP_KERNEL);
	if (!rxq->pit_base) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] kmalloc PIT memory fail.\n", __func__);
		return LOW_MEMORY_PIT;
	}

	rxq->pit_phy_addr = virt_to_phys(rxq->pit_base);

	return ret;
}

static int dpmaif_rxq_irq_init(struct dpmaif_rx_queue *rxq)
{
	int ret = 0;

	scnprintf(rxq->irq_name, sizeof(rxq->irq_name), "DPMAIF_AP_RX%d", rxq->index);

	/* request IRQ */
	ret = request_irq(rxq->irq_id, rxq->rxq_isr, rxq->irq_flags, rxq->irq_name, rxq);
	if (ret) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: request_irq(%d) fail -> %d\n",
			__func__, rxq->irq_id, ret);
		return ret;
	}

	ret = irq_set_irq_wake(rxq->irq_id, 1);
	if (ret)
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: irq_set_irq_wake(%d) fail -> %d\n",
			__func__, rxq->irq_id, ret);

	atomic_set(&rxq->irq_enabled, 1);

	dpmaif_disable_rxq_irq(rxq);

	return 0;
}

static int dpmaif_rxq_sw_init(struct dpmaif_rx_queue *rxq)
{
	int ret;

	ret = dpmaif_rxq_irq_init(rxq);
	if (ret)
		return ret;

	if (dpmaif_ctl->support_lro)
		dpmaif_rxq_lro_info_init(rxq);

	ret = dpmaif_rxq_init_buf(rxq);
	if (ret)
		return ret;

	/* rx tasklet */
	tasklet_init(&rxq->rxq_task, dpmaif_rxq_tasklet, (unsigned long)rxq);

	/* rx push */
	init_waitqueue_head(&rxq->rxq_wq);

	rxq->rxq_push_thread = kthread_run(dpmaif_rxq_push_thread,
			rxq, "dpmaif_rxq%d_push", rxq->index);

	return 0;
}

static int dpmaif_rxqs_sw_init(void)
{
	struct dpmaif_rx_queue *rxq;
	int i, ret;

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];
		rxq->index = i;
		rxq->skb_idx = -1;

		ret = dpmaif_rxq_sw_init(rxq);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static enum hrtimer_restart txq_done_timer_action(struct hrtimer *timer)
{
	struct dpmaif_tx_queue *txq =
		container_of(timer, struct dpmaif_tx_queue, txq_done_timer);

	atomic_set(&txq->txq_done, 1);
	wake_up_all(&txq->txq_done_wait);

	return HRTIMER_NORESTART;
}

static int dpmaif_txq_init_buf(struct dpmaif_tx_queue *txq)
{
	int ret = 0, len = 0;
	int cache_mem_from_dts = 0, nocache_mem_from_dts = 0;

	/* DRB buffer init */
	txq->drb_cnt = DPMAIF_UL_DRB_ENTRY_SIZE;
	/* alloc buffer for HW && AP SW */

	if (!ccci_dpmaif_get_resv_nocache_mem(&txq->drb_base, &txq->drb_phy_addr,
		(txq->drb_cnt * sizeof(struct dpmaif_drb_pd))))
		nocache_mem_from_dts = 1;

	if (!nocache_mem_from_dts)
		txq->drb_base = dma_alloc_coherent(
			dpmaif_ctl->dev,
			(txq->drb_cnt * sizeof(struct dpmaif_drb_pd)),
			&txq->drb_phy_addr, GFP_KERNEL);

	if (!txq->drb_base) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: dma_alloc_coherent drb base fail.\n",
			__func__);
		return LOW_MEMORY_DRB;
	}

	if (!nocache_mem_from_dts)
		memset(txq->drb_base, 0, txq->drb_cnt * sizeof(struct dpmaif_drb_pd));

	/* alloc buffer for AP SW */
	len = txq->drb_cnt * sizeof(struct dpmaif_drb_skb);
	if (!ccci_dpmaif_get_resv_cache_mem(&txq->drb_skb_base, NULL, len))
		cache_mem_from_dts = 1;

	if (!cache_mem_from_dts)
		txq->drb_skb_base = kzalloc(len, GFP_KERNEL);

	if (!txq->drb_skb_base) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc drb skb base fail.\n",
			__func__);
		return LOW_MEMORY_DRB;
	}

	return ret;
}

static int dpmaif_txq_sw_init(struct dpmaif_tx_queue *txq)
{
	int ret;

	init_waitqueue_head(&txq->req_wq);
	atomic_set(&txq->txq_budget, DPMAIF_UL_DRB_ENTRY_SIZE);

	ret = dpmaif_txq_init_buf(txq);
	if (ret)
		return ret;

	init_waitqueue_head(&txq->txq_done_wait);
	atomic_set(&txq->txq_done, 0);
	hrtimer_init(&txq->txq_done_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	txq->txq_done_timer.function = txq_done_timer_action;
	txq->txq_done_thread = NULL;

	spin_lock_init(&txq->txq_lock);

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	txq->busy_count = 0;
#endif

	return 0;
}

static int dpmaif_txqs_sw_init(void)
{
	struct dpmaif_tx_queue *txq;
	int i, ret;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];
		txq->index = i;

		ret = dpmaif_txq_sw_init(txq);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int dpmaif_wakeup_source_init(void)
{
	int ret;
	unsigned int reg_val;

	/* wakeup source init */
	ret = regmap_read(dpmaif_ctl->infra_ao_base, INFRA_DPMAIF_CTRL_REG, &reg_val);
	if (ret) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: read INFRA_DPMAIF_CTRL_REG fail, %d\n",
			__func__, ret);
		return ret;
	}

	reg_val |= DPMAIF_IP_BUSY_MASK;
	reg_val &= ~(1 << 13); /* MD to AP wakeup event */
	regmap_write(dpmaif_ctl->infra_ao_base, INFRA_DPMAIF_CTRL_REG, reg_val);

	return 0;
}

static int dpmaif_late_init(void)
{
	int ret;

	ret = dpmaif_rxqs_sw_init();
	if (ret)
		return ret;

	ret = ccci_dpmaif_bat_late_init();
	if (ret)
		return ret;

	ret = dpmaif_txqs_sw_init();
	if (ret)
		return ret;

	ret = dpmaif_wakeup_source_init();
	if (ret)
		return ret;

	if (dpmaif_ctl->real_rxq_num > 0) {
		mtk_ccci_spd_qos_set_task(
			dpmaif_ctl->rxq[0].rxq_push_thread,
			dpmaif_ctl->bat_alloc_thread,
			dpmaif_ctl->rxq[0].irq_id);
	}

	return 0;
}

static int dpmaif_rxq_hw_init(struct dpmaif_rx_queue *rxq)
{
	if (rxq->started != true)
		return 0;

	if (g_dpmf_ver >= 3) {
		if (rxq->index == 0) {
			ccci_drv3_dl_set_remain_minsz(DPMAIF_HW_BAT_REMAIN);
			ccci_drv3_dl_set_bid_maxcnt(DPMAIF_HW_PKT_BIDCNT);
			ccci_drv3_dl_set_pkt_align(true, DPMAIF_PKT_ALIGN64_MODE);
			ccci_drv3_dl_set_mtu(DPMAIF_HW_MTU_SIZE);
			ccci_drv3_dl_set_pit_chknum();
			ccci_drv3_dl_set_chk_rbnum(DPMAIF_HW_CHK_RB_PIT_NUM);
			ccci_drv3_dl_set_performance();
		}

	} else {  //version 1,2
		ccci_drv2_dl_set_remain_minsz(DPMAIF_HW_BAT_REMAIN);
		ccci_drv2_dl_set_bid_maxcnt(DPMAIF_HW_PKT_BIDCNT);
		ccci_drv2_dl_set_pkt_align(true, DPMAIF_PKT_ALIGN64_MODE);
		ccci_drv2_dl_set_mtu(DPMAIF_HW_MTU_SIZE);

		if (g_dpmf_ver == 2) {
			ccci_drv2_dl_set_pit_chknum();
			ccci_drv2_dl_set_chk_rbnum(DPMAIF_HW_CHK_RB_PIT_NUM);
			ccci_drv2_dl_set_performance();
			ccci_drv2_rxq_hw_int_apit(rxq);

		} else  //version 1
			ccci_drv1_dl_set_pit_chknum();
	}

	if (dpmaif_ctl->support_2rxq)
		ccci_drv3_dl_config_lro_hw(rxq->pit_phy_addr,
			rxq->pit_cnt, true, rxq->index);

	else if (rxq->index == 0) {
		ccci_drv_dl_set_pit_base_addr(rxq->pit_phy_addr);
		ccci_drv_dl_set_pit_size(rxq->pit_cnt);
		ccci_drv_dl_pit_en(true);
		ccci_drv_dl_pit_init_done();
	}

	return 0;
}

static int dpmaif_rxqs_start(void)
{
	struct dpmaif_rx_queue *rxq;
	int i;

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];
		rxq->started = true;
		rxq->budget  = dpmaif_ctl->dl_bat_entry_size - 1;

		if (dpmaif_rxq_hw_init(rxq))
			return -1;
	}

	return 0;
}

static inline int dpmaif_wait_resume_done(void)
{
	int cnt = 0;

	while (atomic_read(&dpmaif_ctl->suspend_flag) == 1) {
		cnt++;
		if (cnt >= 1600000) {
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] warning: suspend_flag = 1; (cnt: %d)\n",
				__func__, cnt);
			return -1;
		}
	}

	if (cnt)
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] suspend_flag = 0; wait cnt: %d\n",
			__func__, cnt);

	return 0;
}

static inline unsigned int dpmaif_get_txq_drb_release_cnt(struct dpmaif_tx_queue *txq)
{
	unsigned int drb_rd_idx = (ops.drv_ul_get_rdidx(txq->index) / DPMAIF_UL_DRB_ENTRY_WORD);

	atomic_set(&txq->drb_rd_idx, drb_rd_idx);

	return get_ringbuf_release_cnt(txq->drb_cnt,
			atomic_read(&txq->drb_rel_rd_idx), drb_rd_idx);
}

static inline unsigned int dpmaif_txq_release_buffer(struct dpmaif_tx_queue *txq,
	unsigned int release_cnt)
{
	unsigned int idx, cur_idx;
	struct dpmaif_drb_pd *cur_drb;
	struct dpmaif_drb_pd *drb_base = (struct dpmaif_drb_pd *)(txq->drb_base);
	struct dpmaif_drb_skb *cur_drb_skb;

	if (release_cnt <= 0)
		return 0;

	cur_idx = atomic_read(&txq->drb_rel_rd_idx);

	for (idx = 0 ; idx < release_cnt; idx++) {
		cur_drb = drb_base + cur_idx;

		if (cur_drb->dtyp == DES_DTYP_PD) {
			cur_drb_skb = ((struct dpmaif_drb_skb *)(txq->drb_skb_base) + cur_idx);

			if (g_debug_flags & DEBUG_TX_DONE_SKB) {
				struct debug_tx_done_skb_hdr hdr;

				hdr.type = TYPE_TX_DONE_SKB_ID;
				hdr.qidx = txq->index;
				hdr.time = (unsigned int)(local_clock() >> 16);
				hdr.rel = cur_idx;
				ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
			}

			if (cur_drb_skb->skb == NULL) {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: drb%d skb is NULL; cur_idx: %u; txq pos: w/r/rel=(%u, %u, %u)\n",
					__func__, txq->index, cur_idx,
					atomic_read(&txq->drb_wr_idx),
					atomic_read(&txq->drb_rd_idx),
					atomic_read(&txq->drb_rel_rd_idx));

				dpmaif_dump_txq_data(txq->index);
				return DATA_CHECK_FAIL;
			}

			dma_unmap_single(dpmaif_ctl->dev, cur_drb_skb->phy_addr,
					cur_drb_skb->data_len, DMA_TO_DEVICE);

			if (cur_drb->c_bit == 0) {
				/* check wakeup source */
				if (atomic_cmpxchg(&dpmaif_ctl->wakeup_src, 1, 0) == 1) {
					CCCI_NORMAL_LOG(0, TAG,
						"[%s] DPMA_MD wakeup source: txq%d.\n",
						__func__, txq->index);
					dpmaif_handle_wakeup_skb(cur_drb_skb->skb);
				}

				dev_kfree_skb_any(cur_drb_skb->skb);
			}

			cur_drb_skb->skb = NULL;

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
			dpmaif_ctl->tx_tfc_pkgs[txq->index]++;
#endif
		}

		cur_idx = get_ringbuf_next_idx(txq->drb_cnt, cur_idx, 1);
		atomic_set(&txq->drb_rel_rd_idx, cur_idx);
		atomic_inc(&txq->txq_budget);

		if (likely(ccci_md_get_cap_by_id() & MODEM_CAP_TXBUSY_STOP)) {
			if (atomic_read(&txq->txq_budget) > (txq->drb_cnt / 8))
				ccci_port_queue_status_notify(dpmaif_ctl->hif_id,
					txq->index, OUT, TX_IRQ);
		}
	}

	return idx;
}

void ccci_dpmaif_txq_release_skb(struct dpmaif_tx_queue *txq, unsigned int release_cnt)
{
	unsigned int idx, cur_idx;
	struct dpmaif_drb_pd *cur_drb;
	struct dpmaif_drb_pd *drb_base = (struct dpmaif_drb_pd *)(txq->drb_base);
	struct dpmaif_drb_skb *cur_drb_skb;

	if (release_cnt <= 0)
		return;

	cur_idx = atomic_read(&txq->drb_rel_rd_idx);

	for (idx = 0 ; idx < release_cnt; idx++) {
		cur_drb = drb_base + cur_idx;

		if (cur_drb->dtyp == DES_DTYP_PD) {
			cur_drb_skb = ((struct dpmaif_drb_skb *)(txq->drb_skb_base) + cur_idx);

			if (cur_drb_skb->skb) {
				dma_unmap_single(dpmaif_ctl->dev, cur_drb_skb->phy_addr,
					cur_drb_skb->data_len, DMA_TO_DEVICE);

				if (cur_drb->c_bit == 0)
					dev_kfree_skb_any(cur_drb_skb->skb);

				cur_drb_skb->skb = NULL;
			}
		}

		cur_idx = get_ringbuf_next_idx(txq->drb_cnt, cur_idx, 1);
	}
}

static inline int dpmaif_txq_drb_release(struct dpmaif_tx_queue *txq)
{
	unsigned int rel_cnt = 0;
	int real_rel_cnt = 0;

	rel_cnt = dpmaif_get_txq_drb_release_cnt(txq);
	if (rel_cnt) {
		real_rel_cnt = dpmaif_txq_release_buffer(txq, rel_cnt);

		if (real_rel_cnt == rel_cnt)
			return ALL_CLEAR;
		if (real_rel_cnt < 0)
			return ERROR_STOP;
	}

	return ALL_CLEAR;
}

static inline void dpmaif_set_txq_thread_aff(int *affinity_set, struct dpmaif_tx_queue *txq)
{
	int tx_aff, ret;
	struct cpumask tmask;

	if ((*affinity_set) != mtk_ccci_get_tx_done_aff(txq->index)) {
		(*affinity_set) = mtk_ccci_get_tx_done_aff(txq->index);
		if ((*affinity_set) <= 0)
			tx_aff = 0xFF;
		else
			tx_aff = (*affinity_set);

		dpmaif_set_cpu_mask(&tmask, (u32)tx_aff, 8);

		ret = set_cpus_allowed_ptr(txq->txq_done_thread, &tmask);
		CCCI_NORMAL_LOG(0, TAG,
			"[%s] txq%d; aff: 0x%X; ret: %d\n",
			__func__, txq->index, (u8)tx_aff, ret);
	}
}

static int dpmaif_txq_done_thread(void *arg)
{
	struct dpmaif_tx_queue *txq = (struct dpmaif_tx_queue *)arg;
	unsigned int L2TISAR0;
	int ret, affinity_set = -1;

	while (1) {
		ret = wait_event_interruptible(txq->txq_done_wait,
			(atomic_read(&txq->txq_done) || kthread_should_stop()));

		if (kthread_should_stop())
			break;

		if (ret == -ERESTARTSYS)
			continue;

		atomic_set(&txq->txq_done, 0);

		if (dpmaif_ctl->dpmaif_state != DPMAIF_STATE_PWRON)
			continue;

		dpmaif_set_txq_thread_aff(&affinity_set, txq);

		if (g_dpmf_ver < 3) {  //for version 1,2
			if (dpmaif_wait_resume_done()) {
				//if resume not done, will waiting 1ms
				hrtimer_start(&txq->txq_done_timer,
					ktime_set(0, 1000000), HRTIMER_MODE_REL);
				continue;
			}

			if (atomic_read(&txq->txq_resume_done)) {
				CCCI_NORMAL_LOG(0, TAG,
					"[%s] txq%d tx_resume_done = %d\n",
					__func__, txq->index,
					atomic_read(&txq->txq_resume_done));
				continue;
			}
		}

		if (txq->started == false) {
			CCCI_NORMAL_LOG(0, TAG, "[%s] txq%d was stopped!\n",
				__func__, txq->index);
			continue;
		}

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
		dpmaif_ctl->tx_done_last_start_time[txq->index] = local_clock();
#endif

		ret = dpmaif_txq_drb_release(txq);
		if (ret == ALL_CLEAR) {
			L2TISAR0 = ccci_drv_get_ul_isr_event();

			L2TISAR0 &= (drv.ul_int_qdone_msk &
					(1 << (txq->index + UL_INT_DONE_OFFSET)));

			if (L2TISAR0 && (dpmaif_get_txq_drb_release_cnt(txq) > 0)) {
				hrtimer_start(&txq->txq_done_timer,
					ktime_set(0, 500000), HRTIMER_MODE_REL);

				DPMA_WRITE_PD_MISC(DPMAIF_PD_AP_UL_L2TISAR0, L2TISAR0);
			} else {
				/* clear IP busy register wake up cpu case */
				ccci_drv_clear_ip_busy();
				/* enable tx done interrupt */
				ops.drv_unmask_ul_interrupt(txq->index);
			}

		} else
			ccci_md_force_assert(MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED, NULL, 0);
	}

	return 0;
}

inline void ccci_irq_rx_lenerr_handler(unsigned int rx_int_isr)
{
	/*SKB buffer size error*/
	if (rx_int_isr & DPMAIF_DL_INT_SKB_LEN_ERR(0))
		CCCI_NOTICE_LOG(0, TAG, "[%s] error: dpmaif dl skb error L2\n", __func__);

	/*Rx data length more than error*/
	if (rx_int_isr & DPMAIF_DL_INT_MTU_ERR_MSK)
		CCCI_NOTICE_LOG(0, TAG, "[%s] error: dpmaif dl mtu error L2\n", __func__);

	/*PIT table full interrupt*/
	//if (rx_int_isr & DPMAIF_DL_INT_PITCNT_LEN_ERR(0)) {
	//}

	/*BAT table full interrupt*/
	//if (rx_int_isr & DPMAIF_DL_INT_BATCNT_LEN_ERR(0)) {
	//}
}

static int dpmaif_txqs_start(void)
{
	struct dpmaif_tx_queue *txq;
	int i;

	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];
		txq->started = true;

		ops.drv_txq_hw_init(txq);

		/* start kernel thread */
		txq->txq_done_thread = kthread_run(dpmaif_txq_done_thread,
				(void *)txq, "dpmaif_txq%d_done_kt", txq->index);

		if (IS_ERR(txq->txq_done_thread)) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: tx done kthread_run fail %ld\n",
				__func__, (long)txq->txq_done_thread);
			return -1;
		}
	}

	return 0;
}

static inline int dpmaif_tx_send_skb_check(int qno)
{
	if (qno < 0 || qno >= DPMAIF_TXQ_NUM) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: tx qno: %d >= %d\n",
			__func__, qno, DPMAIF_TXQ_NUM);

		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	}

	if (dpmaif_ctl->dpmaif_state != DPMAIF_STATE_PWRON)
		return -CCCI_ERR_HIF_NOT_POWER_ON;

	if (dpmaif_wait_resume_done())
		return -EBUSY;

	if (atomic_read(&g_tx_busy_assert_on)) {
		if (likely(ccci_md_get_cap_by_id() & MODEM_CAP_TXBUSY_STOP))
			ccci_port_queue_status_notify(dpmaif_ctl->hif_id,
				qno, OUT, TX_FULL);

		return HW_REG_CHK_FAIL;
	}

	return 0;
}

static inline int get_skb_checksum_type(struct sk_buff *skb)
{
	u32 packet_type;
	struct iphdr *iph;

	packet_type = skb->data[0] & 0xF0;
	if (packet_type == IPV6_VERSION) {
		if (skb->ip_summed == CHECKSUM_NONE ||
			skb->ip_summed == CHECKSUM_UNNECESSARY ||
			skb->ip_summed == CHECKSUM_COMPLETE)
			return 0;
		else if (skb->ip_summed == CHECKSUM_PARTIAL)
			return 2;

		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: invalid ip_summed :%u; payload_len: %u\n",
			__func__, skb->ip_summed,
			((struct ipv6hdr *)skb->data)->payload_len);

		return 0;

	} else if (packet_type == IPV4_VERSION) {
		if (skb->ip_summed == CHECKSUM_NONE ||
			skb->ip_summed == CHECKSUM_UNNECESSARY ||
			skb->ip_summed == CHECKSUM_COMPLETE)
			return 0;
		else if (skb->ip_summed == CHECKSUM_PARTIAL)
			return 1;

		iph = (struct iphdr *)skb->data;
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: invalid checksum flags ipid: 0x%x\n",
			__func__, ntohs(iph->id));

		return 0;
	}

	CCCI_ERROR_LOG(0, TAG,
		"[%s] error: invalid packet type:%u\n",
		__func__, packet_type);

	return 0;
}

static inline int dpmaif_txq_full_check(struct dpmaif_tx_queue *txq,
	unsigned int send_cnt)
{
	unsigned int remain_cnt;

	if (dpmaif_ctl->dpmaif_state != DPMAIF_STATE_PWRON)
		return -CCCI_ERR_HIF_NOT_POWER_ON;

	/* 2. buffer check */
	remain_cnt = get_ringbuf_free_cnt(txq->drb_cnt,
			atomic_read(&txq->drb_rel_rd_idx), atomic_read(&txq->drb_wr_idx));

	if (remain_cnt < send_cnt) {
		/* buffer check: full */
		if (likely(ccci_md_get_cap_by_id() & MODEM_CAP_TXBUSY_STOP))
			ccci_port_queue_status_notify(dpmaif_ctl->hif_id,
				txq->index, OUT, TX_FULL);
#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
		txq->busy_count++;
#endif
		return -EBUSY;
	}

	return 0;
}

static inline int dpmaif_txq_set_skb_data_to_drb(struct dpmaif_tx_queue *txq,
	struct sk_buff *skb, unsigned int prio_count, unsigned int send_cnt)
{
	unsigned int cur_idx, is_frag, c_bit, skb_check_type, wt_cnt, data_len, payload_cnt;
	struct dpmaif_drb_msg *drb_msg;
	struct dpmaif_drb_pd *drb_pd;
	struct dpmaif_drb_skb *drb_skb;
	short cs_ipv4 = 0, cs_l4 = 0;
	struct ccci_header ccci_h;
	void *data_addr = NULL;
	struct skb_shared_info *shinfo;
	dma_addr_t phy_addr;
	int total_size = 0, ret;

	cur_idx = atomic_read(&txq->drb_wr_idx);

	ccci_h = *(struct ccci_header *)skb->data;
	skb_pull(skb, sizeof(struct ccci_header));

	skb_check_type = get_skb_checksum_type(skb);
	if (skb_check_type == 1) {
		cs_ipv4 = 1;
		cs_l4 = 1;
	} else if (skb_check_type == 2)
		cs_l4 = 1;

	drb_msg = ((struct dpmaif_drb_msg *)(txq->drb_base) + cur_idx);
	drb_msg->dtyp = DES_DTYP_MSG;
	drb_msg->c_bit = 1;
	drb_msg->packet_len = skb->len;
	drb_msg->count_l = prio_count;
	drb_msg->channel_id = ccci_h.data[0];
	drb_msg->network_type = 0;
	drb_msg->ipv4 = cs_ipv4;
	drb_msg->l4 = cs_l4;

	drb_skb = ((struct dpmaif_drb_skb *)(txq->drb_skb_base) + cur_idx);
	drb_skb->skb = skb;
	drb_skb->phy_addr = 0;
	drb_skb->data_len = 0;
	drb_skb->drb_idx = cur_idx;
	drb_skb->is_msg = 1;
	drb_skb->is_frag = 0;
	drb_skb->is_last_one = 0;

	/* get next index. */
	cur_idx = get_ringbuf_next_idx(txq->drb_cnt, cur_idx, 1);

	payload_cnt = send_cnt - 1;
	shinfo = skb_shinfo(skb);

	for (wt_cnt = 0; wt_cnt < payload_cnt; wt_cnt++) {
		if (wt_cnt == 0) {
			data_len = skb_headlen(skb);
			data_addr = skb->data;
			is_frag = 0;

		} else {
			skb_frag_t *frag = shinfo->frags + (wt_cnt - 1);

			data_len = skb_frag_size(frag);
			data_addr = skb_frag_address(frag);
			is_frag = 1;
		}

		if (wt_cnt == payload_cnt - 1)
			c_bit = 0;
		else
			c_bit = 1;

		/* tx mapping */
		phy_addr = dma_map_single(dpmaif_ctl->dev, data_addr, data_len, DMA_TO_DEVICE);
		if (dma_mapping_error(dpmaif_ctl->dev, phy_addr)) {
			CCCI_ERROR_LOG(0, TAG, "[%s] error: dma mapping fail.\n", __func__);
			skb_push(skb, sizeof(struct ccci_header));
			return -1;
		}

		drb_pd = ((struct dpmaif_drb_pd *)(txq->drb_base) + cur_idx);
		drb_pd->dtyp = DES_DTYP_PD;
		drb_pd->c_bit = c_bit;
		drb_pd->data_len = data_len;
		drb_pd->p_data_addr = phy_addr & 0xFFFFFFFF;
		drb_pd->data_addr_ext = (phy_addr >> 32) & 0xFF;

		drb_skb = ((struct dpmaif_drb_skb *)(txq->drb_skb_base) + cur_idx);
		drb_skb->skb = skb;
		drb_skb->phy_addr = phy_addr;
		drb_skb->data_len = data_len;
		drb_skb->drb_idx = cur_idx;
		drb_skb->is_msg = 0;
		drb_skb->is_frag = is_frag;
		drb_skb->is_last_one = (c_bit == 0 ? 1 : 0);

		if (g_debug_flags & DEBUG_TX_SEND_SKB) {
			struct debug_tx_send_skb_hdr hdr;

			hdr.type = TYPE_TX_SEND_SKB_ID;
			hdr.qidx = txq->index;
			hdr.time = (unsigned int)(local_clock() >> 16);
			hdr.wr   = is_frag ? (cur_idx | 0x8000) : cur_idx;
			hdr.ipid = ((struct iphdr *)skb->data)->id;
			hdr.len  = data_len;
			ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
		}

		cur_idx = get_ringbuf_next_idx(txq->drb_cnt, cur_idx, 1);

		total_size += data_len;
	}

	atomic_set(&txq->drb_wr_idx, cur_idx);
	atomic_sub(send_cnt, &txq->txq_budget);

	/* 3.3 submit drb descriptor*/
	wmb();

	ret = ccci_drv_ul_add_wcnt(txq->index, send_cnt * DPMAIF_UL_DRB_ENTRY_WORD);
	if (ret) {
		ops.drv_dump_register(CCCI_DUMP_REGISTER);
		tx_force_md_assert("HW_REG_CHK_FAIL");
	} else
		mtk_ccci_add_ul_pkt_bytes(txq->index, total_size);

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_ctl->tx_pre_tfc_pkgs[txq->index]++;
#endif

	return ret;
}

static int dpmaif_tx_send_skb_to_md(unsigned char hif_id, int qno,
	struct sk_buff *skb, int skb_from_pool, int blocking)
{
	unsigned int prio_count = 0, send_cnt;
	struct dpmaif_tx_queue *txq;
	struct skb_shared_info *shinfo;
	unsigned long flags;
	int ret = 0;

	if (!skb)
		return 0;

	ret = dpmaif_tx_send_skb_check(qno);
	if (ret)
		return -EBUSY;

	if (skb->mark & UIDMASK)
		prio_count = 0x1000;

	shinfo = skb_shinfo(skb);
	if (shinfo->frag_list)
		CCCI_NORMAL_LOG(0, TAG,
			"[%s] error: txq%d skb frag_list not supported!\n",
			__func__, qno);

	/* send_cnt = msg drb(1) + pd drb(n: frag cnt +  1: skb->data) */
	send_cnt = shinfo->nr_frags + 2;

	txq = &dpmaif_ctl->txq[qno];

	spin_lock_irqsave(&txq->txq_lock, flags);

	atomic_set(&txq->txq_processing, 1);

	smp_mb(); /* for cpu exec. */
	if (txq->started == false)
		goto __EXIT_FUN;

	ret = dpmaif_txq_full_check(txq, send_cnt);
	if (ret)
		goto __EXIT_FUN;

	ret = dpmaif_txq_set_skb_data_to_drb(txq, skb, prio_count, send_cnt);
	if (ret)
		goto __EXIT_FUN;

__EXIT_FUN:
	atomic_set(&txq->txq_processing, 0);
	spin_unlock_irqrestore(&txq->txq_lock, flags);

	if (ret)
		return -EBUSY;
	else
		return 0;
}

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
static void dpmaif_clear_traffic_data(void)
{
	memset(dpmaif_ctl->tx_tfc_pkgs, 0, sizeof(dpmaif_ctl->tx_tfc_pkgs));
	memset(dpmaif_ctl->rx_tfc_pkgs, 0, sizeof(unsigned int) * dpmaif_ctl->real_rxq_num);
	memset(dpmaif_ctl->tx_pre_tfc_pkgs, 0, sizeof(dpmaif_ctl->tx_pre_tfc_pkgs));
	memset(dpmaif_ctl->tx_done_last_start_time, 0, sizeof(dpmaif_ctl->tx_done_last_start_time));
}
#endif

static int dpmaif_start(unsigned char hif_id)
{
	int ret = 0, i;
	struct cpumask imask;

	if (dpmaif_ctl->dpmaif_state == DPMAIF_STATE_PWRON)
		return 0;
	else if (dpmaif_ctl->dpmaif_state == DPMAIF_STATE_MIN) {
		ret = dpmaif_late_init();
		if (ret < 0)
			return ret;
	}

	dpmaif_set_clk(1, dpmaif_ctl->clk_tbs);

	ret = ops.drv_start();
	if (ret)
		return -1;

	if (dpmaif_ctl->support_2rxq)
		ccci_drv3_dl_lro_hpc_hw_init();

	if (dpmaif_rxqs_start())
		return -1;

	if (ccci_dpmaif_bat_start())
		return -1;

	if (dpmaif_txqs_start())
		return -1;

	if (g_dpmf_ver >= 3) {
		ccci_drv3_hw_init_done();
		ccci_drv_clear_ip_busy();
	}

	dpmaif_enable_all_irq();

	dpmaif_ctl->dpmaif_state = DPMAIF_STATE_PWRON;
	atomic_set(&g_tx_busy_assert_on, 0);

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		cpumask_clear(&imask);
		cpumask_set_cpu(1 + i, &imask);
		irq_set_affinity_hint(dpmaif_ctl->rxq[i].irq_id, &imask);
	}

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	dpmaif_clear_traffic_data();
	mod_timer(&dpmaif_ctl->traffic_monitor, jiffies + DPMAIF_TRAFFIC_MONITOR_INTERVAL * HZ);
#endif

	return 0;
}

static int dpmaif_txqs_hw_stop(void)
{
	struct dpmaif_tx_queue *txq;
	int i, count, ret;

	/* ===Disable UL SW active=== */
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];

		txq->started = false;

		smp_mb(); /* for cpu exec. */

		count = 0;
		do {
			count++;

			if (count >= 3200000) {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: stop ul sw failed\n",
					__func__);
				break;
			}

		} while (atomic_read(&txq->txq_processing) != 0);
	}

	count = 0;
	do {
		/*Disable HW arb and check idle*/
		ops.drv_ul_all_queue_en(false);
		ret = ops.drv_ul_idle_check();

		/*retry handler*/
		count++;
		if (ret && (count >= 1600000)) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: stop arb hw failed\n",
				__func__);

			break;

		}
	} while (ret != 0);

	return ret;
}

static int dpmaif_rxqs_hw_stop(void)
{
	struct dpmaif_rx_queue *rxq;
	int i, count, ret;

	/* ===Disable DL/RX SW active=== */
	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];

		rxq->started = false;

		smp_mb(); /* for cpu exec. */

		count = 0;
		do {
			/*retry handler*/
			count++;
			if (count >= 3200000) {
				CCCI_ERROR_LOG(0, TAG,
					"[%s] error: stop dl sw failed\n",
					__func__);
				break;
			}

		} while (atomic_read(&rxq->rxq_processing) != 0);
	}

	count = 0;
	do {
		/*Disable HW arb and check idle*/
		ret = ccci_drv_dl_all_queue_en(false);
		if (ret)
			return ret;

		ret = ccci_drv_dl_idle_check();

		/*retry handler*/
		count++;
		if (ret && (count >= 1600000)) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: stop Rx failed, %d\n",
				__func__, ret);
			break;
		}

	} while (ret != 0);

	return ret;
}

static void wait_txq_done_thread_finish(struct dpmaif_tx_queue *txq)
{
	int ret;

	hrtimer_cancel(&txq->txq_done_timer);

	msleep(20); /* Make sure hrtimer finish */

	while (!IS_ERR(txq->txq_done_thread)) {
		ret = kthread_stop(txq->txq_done_thread);
		if (ret == -EINTR) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: stop kthread meet EINTR\n", __func__);
			continue;
		} else
			break;
	}

	txq->txq_done_thread = NULL;
}

static void dpmaif_txqs_sw_stop(void)
{
	struct dpmaif_tx_queue *txq;
	unsigned int rel_cnt;
	int i;

	/*flush and release UL descriptor*/
	for (i = 0; i < DPMAIF_TXQ_NUM; i++) {
		txq = &dpmaif_ctl->txq[i];

		if (!txq->drb_base)
			continue;

		wait_txq_done_thread_finish(txq);

		if (atomic_read(&txq->drb_rd_idx) != atomic_read(&txq->drb_rel_rd_idx)) {
			CCCI_NORMAL_LOG(0, TAG,
				"[%s] txq%d tx_release maybe not end; rel/r/w(%u,%u,%u)\n",
				__func__, i,
				atomic_read(&txq->drb_rel_rd_idx),
				atomic_read(&txq->drb_rd_idx),
				atomic_read(&txq->drb_wr_idx));
		}

		if (atomic_read(&txq->drb_wr_idx) != atomic_read(&txq->drb_rel_rd_idx)) {
			rel_cnt = get_ringbuf_release_cnt(txq->drb_cnt,
						atomic_read(&txq->drb_rel_rd_idx),
						atomic_read(&txq->drb_wr_idx));
			ccci_dpmaif_txq_release_skb(txq, rel_cnt);
		}

		atomic_set(&txq->drb_rd_idx, 0);
		atomic_set(&txq->drb_wr_idx, 0);
		atomic_set(&txq->drb_rel_rd_idx, 0);

		atomic_set(&txq->txq_budget, DPMAIF_UL_DRB_ENTRY_SIZE);

		memset(txq->drb_base, 0, (txq->drb_cnt * sizeof(struct dpmaif_drb_pd)));
		memset(txq->drb_skb_base, 0, (txq->drb_cnt * sizeof(struct dpmaif_drb_skb)));

		atomic_set(&txq->txq_resume_done, 0);
	}
}

static void dpmaif_rxq_stop_lro(struct dpmaif_rx_queue *rxq)
{
	int i;
	struct dpmaif_bat_skb *bat_skb = NULL;
	struct dpmaif_rx_lro_info *lro_info = &rxq->lro_info;

	CCCI_ERROR_LOG(0, TAG, "[%s] rxq%d lroinfo count: %u\n",
			__func__, rxq->index, lro_info->count);

	for (i = 0; i < lro_info->count; i++) {
		dev_kfree_skb_any(lro_info->data[i].skb);

		bat_skb = ((struct dpmaif_bat_skb *)
					dpmaif_ctl->bat_skb->bat_pkt_addr + lro_info->data[i].bid);
		bat_skb->skb = NULL;
	}

	rxq->lro_info.count = 0;
}

static void dpmaif_rxqs_sw_stop(void)
{
	struct dpmaif_rx_queue *rxq;
	int i;

	/* rx clear */
	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];

		if (dpmaif_ctl->support_lro)
			dpmaif_rxq_stop_lro(rxq);

		if (!rxq->pit_base)
			continue;

		atomic_set(&rxq->pit_rd_idx, 0);
		atomic_set(&rxq->pit_wr_idx, 0);

		memset(rxq->pit_base, 0, (rxq->pit_cnt * dpmaif_ctl->dl_pit_byte_size));
	}
}

static int dpmaif_pre_stop(unsigned char hif_id)
{
	if (hif_id != DPMAIF_HIF_ID)
		return -1;

	if (dpmaif_txqs_hw_stop())
		CCCI_ERROR_LOG(0, TAG, "[%s] error: dpmaif_txqs_hw_stop() fail.\n", __func__);

	if (dpmaif_rxqs_hw_stop())
		CCCI_ERROR_LOG(0, TAG, "[%s] error: dpmaif_rxqs_hw_stop() fail.\n", __func__);

	return 0;
}

static int dpmaif_stop(unsigned char hif_id)
{
	int txq_err = 0, rxq_err = 0;

	if (dpmaif_ctl->dpmaif_state == DPMAIF_STATE_PWROFF ||
		dpmaif_ctl->dpmaif_state == DPMAIF_STATE_MIN)
		return 0;

	dpmaif_ctl->dpmaif_state = DPMAIF_STATE_PWROFF;

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	/* stop debug mechnism */
	del_timer(&dpmaif_ctl->traffic_monitor);
#endif

	dpmaif_disable_all_irq();

	txq_err = dpmaif_txqs_hw_stop();

	rxq_err = dpmaif_rxqs_hw_stop();

	dpmaif_txqs_sw_stop();

	dpmaif_rxqs_sw_stop();

	/* rx bat buf clear */
	ccci_dpmaif_bat_stop();

	if (g_dpmf_ver > 1) {
		dpmaif_set_clk(0, dpmaif_ctl->clk_tbs);
		ops.drv_hw_reset();
	} else {
		ops.drv_hw_reset();
		dpmaif_set_clk(0, dpmaif_ctl->clk_tbs);
	}

	if (!txq_err && !rxq_err)
		return 0;

	ccci_md_force_assert(MD_FORCE_ASSERT_BY_AP_Q0_BLOCKED, NULL, 0);

	return -1;
}

static void dpmaif_total_spd_cb(u64 total_ul_speed, u64 total_dl_speed)
{
	ccmni_set_cur_speed(total_dl_speed);

	if ((g_debug_flags & DEBUG_UL_DL_TPUT) &&
			(total_ul_speed || total_dl_speed)) {
		struct debug_ul_dl_tput_hdr hdr = {0};

		hdr.type = TYPE_UL_DL_TPUT_ID;
		hdr.time = (unsigned int)(local_clock() >> 16);
		hdr.uput = total_ul_speed;
		hdr.dput = total_dl_speed;
		ccci_dpmaif_debug_add(&hdr, sizeof(hdr));
	}

	if (total_dl_speed > 4000000000LL) {  // dl tput > 4G
		g_alloc_skb_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_skb_tbl_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_tbl_threshold = MAX_ALLOC_BAT_CNT;

		if (dpmaif_ctl->support_lro == 1)
			ccmni_set_tcp_is_need_gro(0);

#ifdef DPMAIF_REDUCE_RX_FLUSH
		g_rx_flush_pkt_cnt = 60;
#endif
	} else if (total_dl_speed > 2000000000LL) {  // dl tput > 2G
		g_alloc_skb_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_skb_tbl_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_tbl_threshold = MAX_ALLOC_BAT_CNT;

		if (dpmaif_ctl->support_lro == 1)
			ccmni_set_tcp_is_need_gro(0);

#ifdef DPMAIF_REDUCE_RX_FLUSH
		g_rx_flush_pkt_cnt = 30;
#endif
	} else if (total_dl_speed > 1000000000LL) {  // dl tput > 1G
		g_alloc_skb_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_skb_tbl_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_tbl_threshold = MAX_ALLOC_BAT_CNT;

		if (dpmaif_ctl->support_lro == 1)
			ccmni_set_tcp_is_need_gro(0);

#ifdef DPMAIF_REDUCE_RX_FLUSH
		g_rx_flush_pkt_cnt = 10;
#endif
	} else if (total_dl_speed > 300000000LL) {  // dl tput > 300M
		g_alloc_skb_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_skb_tbl_threshold = MAX_ALLOC_BAT_CNT;
		g_alloc_frg_tbl_threshold = MAX_ALLOC_BAT_CNT;

		if (dpmaif_ctl->support_lro == 1)
			ccmni_set_tcp_is_need_gro(1);

#ifdef DPMAIF_REDUCE_RX_FLUSH
		g_rx_flush_pkt_cnt = 5;
#endif
	} else {  // dl tput < 300M
		g_alloc_skb_threshold = MIN_ALLOC_SKB_CNT;
		g_alloc_frg_threshold = MIN_ALLOC_FRG_CNT;
		g_alloc_skb_tbl_threshold = MIN_ALLOC_SKB_TBL_CNT;
		g_alloc_frg_tbl_threshold = MIN_ALLOC_FRG_TBL_CNT;

		if (dpmaif_ctl->support_lro == 1)
			ccmni_set_tcp_is_need_gro(1);

#ifdef DPMAIF_REDUCE_RX_FLUSH
		g_rx_flush_pkt_cnt = 0;
#endif
	}
}

static int dpmaif_init_cap(struct device *dev)
{
	u32 len = 0;

	if (of_property_read_u32(dev->of_node,
			"mediatek,dpmaif-cap", &dpmaif_ctl->capability)) {
		dpmaif_ctl->capability = 0;
		CCCI_ERROR_LOG(0, TAG,
			"[%s] read mediatek,dpmaif-cap fail!\n",
			__func__);
	}

	dpmaif_ctl->support_lro = dpmaif_ctl->capability & DPMAIF_CAP_LRO;
	dpmaif_ctl->support_2rxq = (dpmaif_ctl->capability & DPMAIF_CAP_2RXQ);
	if (dpmaif_ctl->support_2rxq || dpmaif_ctl->support_lro) {
		dpmaif_ctl->real_rxq_num = DPMAIF_RXQ_NUM;
		dpmaif_ctl->support_lro = 1;
		dpmaif_ctl->support_2rxq = 1;
		ccmni_set_tcp_is_need_gro(1);
	} else
		dpmaif_ctl->real_rxq_num = 1;

	if (of_property_read_u32(dev->of_node,
			"dl_bat_entry_size", &dpmaif_ctl->dl_bat_entry_size))
		dpmaif_ctl->dl_bat_entry_size = 0;

	CCCI_NORMAL_LOG(0, TAG,
		"[%s] dpmaif capability: 0x%x; dl_bat_entry_size: %u\n",
		__func__, dpmaif_ctl->capability, dpmaif_ctl->dl_bat_entry_size);

	len = sizeof(struct dpmaif_rx_queue) * dpmaif_ctl->real_rxq_num;
	dpmaif_ctl->rxq = kzalloc(len, GFP_KERNEL);
	if (!dpmaif_ctl->rxq) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: kzalloc() rxq fail\n", __func__);
		return -1;
	}

	dpmaif_ctl->rx_tfc_pkgs = kzalloc(sizeof(unsigned int) * dpmaif_ctl->real_rxq_num,
			GFP_KERNEL);
	if (!dpmaif_ctl->rx_tfc_pkgs) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: kzalloc() rx_tfc_pkgs fail\n", __func__);
		return -1;
	}

	mtk_ccci_register_speed_1s_callback(dpmaif_total_spd_cb);

	return 0;
}

static int dpmaif_stop_queue(unsigned char hif_id, unsigned char qno,
		enum DIRECTION dir)
{
	return 0;
}

static int dpmaif_start_queue(unsigned char hif_id, unsigned char qno,
		enum DIRECTION dir)
{
	return 0;
}

static int dpmaif_give_more(unsigned char hif_id, unsigned char qno)
{
	struct dpmaif_rx_queue *rxq;
	int i;

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];
		tasklet_hi_schedule(&rxq->rxq_task);
	}

	return 0;
}

static int dpmaif_write_room(unsigned char hif_id, unsigned char qno)
{
	if (qno >= DPMAIF_TXQ_NUM)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;

	return atomic_read(&dpmaif_ctl->txq[qno].txq_budget);
}

static int dpmaif_debug(unsigned char hif_id,
		enum ccci_hif_debug_flg flag, int *para)
{
	CCCI_ERROR_LOG(0, TAG,
		"[%s] error: didn't do anything; flag: 0x%X\n", __func__, flag);

	return -1;
}

static void dpmaif_dump_rx_data(void)
{
	int i;
	struct dpmaif_rx_queue *rxq = NULL;

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];

		CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG,
			"dpmaif: rxq%d, pit request base: 0x%p(%u*%u); pos: w/r = %u,%u\n",
			rxq->index, rxq->pit_base, rxq->pit_cnt, drv.normal_pit_size,
			atomic_read(&rxq->pit_wr_idx), atomic_read(&rxq->pit_rd_idx));

		ccci_util_mem_dump(CCCI_DUMP_DPMAIF, rxq->pit_base,
			rxq->pit_cnt * drv.normal_pit_size);
	}

	CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG,
		"Current bat request base: 0x%p(%u*%zu); pos: w/r = %u,%u\n",
		dpmaif_ctl->bat_skb->bat_base,
		dpmaif_ctl->bat_skb->bat_cnt, sizeof(struct dpmaif_bat_base),
		atomic_read(&dpmaif_ctl->bat_skb->bat_wr_idx),
		atomic_read(&dpmaif_ctl->bat_skb->bat_rd_idx));

	ccci_util_mem_dump(CCCI_DUMP_DPMAIF,
		dpmaif_ctl->bat_skb->bat_base,
		dpmaif_ctl->bat_skb->bat_cnt * sizeof(struct dpmaif_bat_base));
}

static int dpmaif_dump_status(unsigned char hif_id,
		enum MODEM_DUMP_FLAG flag, void *buff, int length)
{
#if IS_ENABLED(CONFIG_MTK_IRQ_DBG)
	int i;
#endif

	CCCI_MEM_LOG_TAG(0, TAG, "[%s]: flag = 0x%X; length = %d\n", __func__, flag, length);

	if (flag & DUMP_FLAG_TOGGLE_NET_SPD)
		return mtk_ccci_net_spd_cfg(1);

	if ((flag & DUMP_FLAG_REG) &&
			(dpmaif_ctl->dpmaif_state != DPMAIF_STATE_PWROFF
			&& dpmaif_ctl->dpmaif_state != DPMAIF_STATE_MIN)) {

		ops.drv_dump_register(CCCI_DUMP_REGISTER);
		dpmaif_dump_rx_data();
	}

#if IS_ENABLED(CONFIG_MTK_IRQ_DBG)
	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		CCCI_NORMAL_LOG(0, TAG, "Dump AP DPMAIF IRQ%d status:\n", i);
		mt_irq_dump_status(dpmaif_ctl->rxq[i].irq_id);
	}
#endif

	return 0;
}

static int dpmaif_empty_query(int qno)
{
	struct dpmaif_tx_queue *txq;

	if (qno < 0 || qno >= DPMAIF_TXQ_NUM) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: qno(%d) is invalid.\n", __func__, qno);
		return 1;
	}

	txq = &dpmaif_ctl->txq[qno];

	return (atomic_read(&txq->txq_budget) > (txq->drb_cnt / 8));
}

static struct ccci_hif_ops g_dpmaif_ops = {
	.send_skb =    &dpmaif_tx_send_skb_to_md,
	.give_more =   &dpmaif_give_more,
	.write_room =  &dpmaif_write_room,
	.stop_queue =  &dpmaif_stop_queue,
	.start_queue = &dpmaif_start_queue,
	.dump_status = &dpmaif_dump_status,
	.start =       &dpmaif_start,
	.pre_stop =    &dpmaif_pre_stop,
	.stop =        &dpmaif_stop,
	.debug =       &dpmaif_debug,
	.empty_query = &dpmaif_empty_query,
};

static int dpmaif_init_register(struct device *dev)
{
	struct device_node *node = dev->of_node;

	dpmaif_ctl->infra_ao_base = syscon_regmap_lookup_by_phandle(node, "dpmaif-infracfg");
	if (IS_ERR(dpmaif_ctl->infra_ao_base)) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: No dpmaif-infracfg register in dtsi.\n", __func__);
		return -1;
	}

	if (g_dpmf_ver == 2) {
		if (dpmaif_read_infra_ao_mem_base(dev))
			return -1;
	}

	dpmaif_ctl->ao_ul_base = of_iomap(node, 0);
	if (dpmaif_ctl->ao_ul_base == NULL) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: ao_ul_base iomap fail\n", __func__);
		return -1;
	}
	dpmaif_ctl->ao_dl_base = dpmaif_ctl->ao_ul_base + 0x400;

	dpmaif_ctl->pd_ul_base = of_iomap(node, 1);
	if (dpmaif_ctl->pd_ul_base == NULL) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: pd_ul_base iomap fail\n", __func__);
		return -1;
	}
	dpmaif_ctl->pd_dl_base   = dpmaif_ctl->pd_ul_base + 0x100;
	dpmaif_ctl->pd_misc_base = dpmaif_ctl->pd_ul_base + 0x400;

	dpmaif_ctl->pd_md_misc_base = of_iomap(node, 2);
	if (dpmaif_ctl->pd_md_misc_base == NULL) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: pd_md_misc_base iomap fail\n", __func__);
		return -1;
	}

	dpmaif_ctl->pd_sram_base = of_iomap(node, 3);
	if (dpmaif_ctl->pd_sram_base == NULL) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: pd_sram_base iomap fail\n", __func__);
		return -1;
	}

	CCCI_NORMAL_LOG(0, TAG,
		"[%s] register: ao_ul=0x%p, pd_ul=0x%p, pd_md_misc=0x%p; pd_sram: %p\n",
		__func__, dpmaif_ctl->ao_ul_base, dpmaif_ctl->pd_ul_base,
		dpmaif_ctl->pd_md_misc_base, dpmaif_ctl->pd_sram_base);

	return 0;
}

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
static void dpmaif_traffic_monitor_func(struct timer_list *t)
{
	int i, q_state = 0;

	if (3 < DPMAIF_TXQ_NUM) {
		q_state = (dpmaif_ctl->txq[0].started << 0) |
			(dpmaif_ctl->txq[1].started << 1) |
			(dpmaif_ctl->txq[2].started << 2) |
			(dpmaif_ctl->txq[3].started << 3);

		CCCI_NORMAL_LOG(0, TAG,
			"dpmaif-txq: 0-3(status=0x%x)[%d]: %d-%d-%d, %d-%d-%d, %d-%d-%d, %d-%d-%d\n",
			q_state, dpmaif_ctl->txq[0].drb_cnt,
			atomic_read(&dpmaif_ctl->txq[0].txq_budget),
			dpmaif_ctl->tx_pre_tfc_pkgs[0],
			dpmaif_ctl->tx_tfc_pkgs[0],
			atomic_read(&dpmaif_ctl->txq[1].txq_budget),
			dpmaif_ctl->tx_pre_tfc_pkgs[1],
			dpmaif_ctl->tx_tfc_pkgs[1],
			atomic_read(&dpmaif_ctl->txq[2].txq_budget),
			dpmaif_ctl->tx_pre_tfc_pkgs[2],
			dpmaif_ctl->tx_tfc_pkgs[2],
			atomic_read(&dpmaif_ctl->txq[3].txq_budget),
			dpmaif_ctl->tx_pre_tfc_pkgs[3],
			dpmaif_ctl->tx_tfc_pkgs[3]);

		CCCI_NORMAL_LOG(0, TAG,
			"dpmaif-txq pos: w/r/rel=(%d,%d,%d)(%d,%d,%d)(%d,%d,%d)(%d,%d,%d), tx_busy=%d,%d,%d,%d, last_isr=%lx,%lx,%lx,%lx\n",
			atomic_read(&dpmaif_ctl->txq[0].drb_wr_idx),
			atomic_read(&dpmaif_ctl->txq[0].drb_rd_idx),
			atomic_read(&dpmaif_ctl->txq[0].drb_rel_rd_idx),
			atomic_read(&dpmaif_ctl->txq[1].drb_wr_idx),
			atomic_read(&dpmaif_ctl->txq[1].drb_rd_idx),
			atomic_read(&dpmaif_ctl->txq[1].drb_rel_rd_idx),
			atomic_read(&dpmaif_ctl->txq[2].drb_wr_idx),
			atomic_read(&dpmaif_ctl->txq[2].drb_rd_idx),
			atomic_read(&dpmaif_ctl->txq[2].drb_rel_rd_idx),
			atomic_read(&dpmaif_ctl->txq[3].drb_wr_idx),
			atomic_read(&dpmaif_ctl->txq[3].drb_rd_idx),
			atomic_read(&dpmaif_ctl->txq[3].drb_rel_rd_idx),
			dpmaif_ctl->txq[0].busy_count,
			dpmaif_ctl->txq[1].busy_count,
			dpmaif_ctl->txq[2].busy_count,
			dpmaif_ctl->txq[3].busy_count,
			dpmaif_ctl->tx_done_last_start_time[0],
			dpmaif_ctl->tx_done_last_start_time[1],
			dpmaif_ctl->tx_done_last_start_time[2],
			dpmaif_ctl->tx_done_last_start_time[3]);
	}

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		CCCI_NORMAL_LOG(0, TAG,
			"dpmaif-rxq%d: received:%u; q_state=%u; budget:%u; w/r=(%u,%u)\n",
			i, dpmaif_ctl->rx_tfc_pkgs[i],
			dpmaif_ctl->rxq[i].started,
			dpmaif_ctl->rxq[i].budget,
			atomic_read(&dpmaif_ctl->rxq[i].pit_wr_idx),
			atomic_read(&dpmaif_ctl->rxq[i].pit_rd_idx));
	}

	mod_timer(&dpmaif_ctl->traffic_monitor,
			jiffies + DPMAIF_TRAFFIC_MONITOR_INTERVAL * HZ);
}
#endif

static int dpmaif_init_rx_irq_and_tasklet(struct device *dev)
{
	int i;
	struct dpmaif_rx_queue *rxq;

	for (i = 0; i < dpmaif_ctl->real_rxq_num; i++) {
		rxq = &dpmaif_ctl->rxq[i];

		rxq->rxq_tasklet = dpmaif_rxq_tasklet;
		rxq->irq_id = irq_of_parse_and_map(dev->of_node, i);
		if (rxq->irq_id == 0) {
			CCCI_ERROR_LOG(0, TAG,
				"[%s] error: get dpmaif rx[%d] irq fail.\n",
				__func__, i);
			return -1;
		}

		rxq->irq_flags = IRQF_TRIGGER_NONE;
		CCCI_NORMAL_LOG(0, TAG, "[%s] rx[%d]->irq_id: %u\n",
			__func__, i, rxq->irq_id);
	}

	return 0;
}

static u64 g_dpmaif_dmamask = DMA_BIT_MASK(40);

static int dpmaif_init_com(struct device *dev)
{
	int ret;
	struct dpmaif_ctrl *temp_dpmaif_ctl = NULL;

	if ((!dev) || (!dev->of_node)) {
		CCCI_ERROR_LOG(0, TAG, "[%s] error: dev or dev->of_node is NULL.\n", __func__);
		goto DPMAIF_INIT_FAIL;
	}

	temp_dpmaif_ctl = kzalloc(sizeof(struct dpmaif_ctrl), GFP_KERNEL);
	if (!temp_dpmaif_ctl) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: alloc hif_ctrl fail\n", __func__);
		goto DPMAIF_INIT_FAIL;
	}
	g_dpmaif_ctl = temp_dpmaif_ctl;

	dpmaif_ctl->dev = dev;
	dpmaif_ctl->hif_id = DPMAIF_HIF_ID;
	dpmaif_ctl->dpmaif_state = DPMAIF_STATE_MIN;
	atomic_set(&dpmaif_ctl->suspend_flag, -1);

	if (dpmaif_init_register(dev))
		goto DPMAIF_INIT_FAIL;

	arch_atomic_set(&dpmaif_ctl->wakeup_src, 0);

	ret = dpmaif_init_cap(dev);
	if (ret < 0)
		goto DPMAIF_INIT_FAIL;

	ret = dpmaif_init_rx_irq_and_tasklet(dev);
	if (ret < 0)
		goto DPMAIF_INIT_FAIL;

	if (ccci_dl_pool_init(dpmaif_ctl->real_rxq_num))
		goto DPMAIF_INIT_FAIL;

	ret = ccci_dpmaif_bat_init(dev);
	if (ret)
		goto DPMAIF_INIT_FAIL;

	ccci_dpmaif_debug_init();

	dev->dma_mask = &g_dpmaif_dmamask;
	dev->coherent_dma_mask = g_dpmaif_dmamask;
	/* hook up to device */
	dev->platform_data = dpmaif_ctl; /* maybe no need */

	if (g_dpmf_ver >= 3)
		ret = ccci_dpmaif_drv3_init();
	else if (g_dpmf_ver == 2)
		ret = ccci_dpmaif_drv2_init();
	else //version 1
		ret = ccci_dpmaif_drv1_init();
	if (ret)
		goto DPMAIF_INIT_FAIL;

	ccci_dpmaif_resv_mem_init();

	if (dpmaif_init_clk(dev, dpmaif_ctl->clk_tbs))
		goto DPMAIF_INIT_FAIL;

	mtk_ccci_md_spd_qos_init(dev);
	mtk_ccci_spd_qos_method_init();

	ccci_hif_register(DPMAIF_HIF_ID, dpmaif_ctl, &g_dpmaif_ops);

	atomic_set(&dpmaif_ctl->suspend_flag, 0);

#if DPMAIF_TRAFFIC_MONITOR_INTERVAL
	timer_setup(&dpmaif_ctl->traffic_monitor, dpmaif_traffic_monitor_func, 0);
#endif
	mtk_ccci_net_speed_init();

	return 0;  /* dpmaif init succ. */

DPMAIF_INIT_FAIL:
	kfree(g_dpmaif_ctl);
	g_dpmaif_ctl = NULL;

	return -1;
}

static int dpmaif_probe(struct platform_device *pdev)
{
	if (of_property_read_u32(pdev->dev.of_node,
			"mediatek,dpmaif-ver", &g_dpmf_ver)) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: not find mediatek,dpmaif-ver.\n", __func__);
		g_dpmf_ver = MAX_DPMAIF_VER;
	}

	if (g_dpmf_ver == 0 || g_dpmf_ver > MAX_DPMAIF_VER) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: g_dpmf_ver(%d) is invalid.\n",
			__func__, g_dpmf_ver);
		return -1;
	}

	if (of_property_read_u32(pdev->dev.of_node,
			"mediatek,plat-info", &g_plat_inf))
		g_plat_inf = DEFAULT_PLAT_INF;

	CCCI_NORMAL_LOG(0, TAG,
		"[%s] g_dpmf_ver: %u; g_plat_inf: %u\n",
		__func__, g_dpmf_ver, g_plat_inf);

	if (dpmaif_init_com(&pdev->dev))
		return -1;

	return 0;
}

static int dpmaif_suspend_noirq(struct device *dev)
{
	CCCI_NORMAL_LOG(-1, TAG, "[%s] dev: %p\n", __func__, dev);

	if ((!dpmaif_ctl) || (atomic_read(&dpmaif_ctl->suspend_flag) < 0))
		return 0;

	atomic_set(&dpmaif_ctl->suspend_flag, 1);

	if (dpmaif_ctl->dpmaif_state != DPMAIF_STATE_PWRON &&
		dpmaif_ctl->dpmaif_state != DPMAIF_STATE_EXCEPTION)
		return 0;

	/* dpmaif clock on: backup int mask. */
	dpmaif_ctl->suspend_reg_int_mask_bak = ops.drv_get_dl_interrupt_mask();

	return ops.drv_suspend_noirq(dev);
}

static int dpmaif_resume_noirq(struct device *dev)
{
	struct arm_smccc_res res;
	int ret = 0;

	if ((!dpmaif_ctl) || (atomic_read(&dpmaif_ctl->suspend_flag) < 0))
		return 0;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL,
			MD_CLOCK_REQUEST, MD_WAKEUP_AP_SRC,
			WAKE_SRC_HIF_DPMAIF, 0, 0, 0, 0, &res);

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] flag_1=0x%llx, flag_2=0x%llx, flag_3=0x%llx, flag_4=0x%llx\n",
		__func__, res.a0, res.a1, res.a2, res.a3);

	if ((!res.a0) && (res.a1 == WAKE_SRC_HIF_DPMAIF))
		arch_atomic_set(&dpmaif_ctl->wakeup_src, 1);

	if (dpmaif_ctl->dpmaif_state != DPMAIF_STATE_PWRON &&
		dpmaif_ctl->dpmaif_state != DPMAIF_STATE_EXCEPTION)
		goto EXIT_FUNC;

	/*IP don't power down before*/
	if (ops.drv_check_power_down() == false) {
		CCCI_NORMAL_LOG(0, TAG, "[%s] sys_resume no need restore\n", __func__);
		goto EXIT_FUNC;
	}

	ret = ops.drv_resume_noirq(dev);

EXIT_FUNC:
	atomic_set(&dpmaif_ctl->suspend_flag, 0);

	return ret;
}

static const struct dev_pm_ops g_dpmaif_pm_ops = {
	.suspend_noirq = dpmaif_suspend_noirq,
	.resume_noirq = dpmaif_resume_noirq,
};

static const struct of_device_id g_dpmaif_of_ids[] = {
	{.compatible = "mediatek,dpmaif"},
	{}
};

static struct platform_driver g_dpmaif_driver = {
	.driver = {
		.name = "ccci_dpmaif_driver",
		.of_match_table = g_dpmaif_of_ids,
		.pm = &g_dpmaif_pm_ops,
	},

	.probe = dpmaif_probe,
};

static int __init ccci_dpmaif_init(void)
{
	int ret;

	ret = platform_driver_register(&g_dpmaif_driver);
	if (ret) {
		CCCI_ERROR_LOG(0, TAG, "ccci dpmaif driver init fail %d",
			ret);
		return ret;
	}
	return 0;
}

static void __exit ccci_dpmaif_exit(void)
{
}

module_init(ccci_dpmaif_init);
module_exit(ccci_dpmaif_exit);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci dpmaif driver");
MODULE_LICENSE("GPL");

