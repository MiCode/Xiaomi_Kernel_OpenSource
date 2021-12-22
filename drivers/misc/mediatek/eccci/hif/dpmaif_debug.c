// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "dpmaif_debug.h"
#include "net_pool.h"
#include "ccci_port.h"
#include "ccci_hif.h"

#define TAG "dbg"



static char *s_mem_buf_ptr;
static u32   s_mem_buf_size;
static u32   s_mem_buf_len;
static int   s_chn_idx = -1;

static spinlock_t s_mem_buf_lock;


static u32 s_buf_pkg_count;
static wait_queue_head_t *g_rx_wq;



static inline int add_data_to_buf(struct dpmaif_debug_header *hdr,
		u32 len, void *data)
{
	if (!data)
		len = 0;

	if ((s_mem_buf_len + sizeof(struct dpmaif_debug_header) + len)
				<= s_mem_buf_size) {
		memcpy(s_mem_buf_ptr + s_mem_buf_len, hdr,
				sizeof(struct dpmaif_debug_header));
		s_mem_buf_len += sizeof(struct dpmaif_debug_header);

		if (len > 0) {
			memcpy(s_mem_buf_ptr + s_mem_buf_len, data, len);
			s_mem_buf_len += len;
		}

		return 0;
	}

	return -1;
}

static void dpmaif_push_data_to_stack(int is_md_ee)
{
	struct lhif_header *lhif_h;
	struct sk_buff *skb;
	struct iphdr *iph;
	int ret;

	if (s_mem_buf_len == 0 || s_chn_idx < 0)
		goto alloc_fail;

	skb = __dev_alloc_skb(IPV4_HEADER_LEN + s_mem_buf_size, GFP_ATOMIC);
	if (!skb)
		goto alloc_fail;

	skb->len = 0;
	skb_reset_tail_pointer(skb);
	skb->ip_summed = 0;

	iph = (struct iphdr *)(skb->data);
	iph->version = 0;
	iph->saddr = 0;
	iph->daddr = 0;
	iph->ihl = (sizeof(struct iphdr) >> 2);
	iph->tot_len = htons(IPV4_HEADER_LEN + s_mem_buf_len);

	memcpy(skb->data + IPV4_HEADER_LEN, s_mem_buf_ptr, s_mem_buf_len);
	skb_put(skb, IPV4_HEADER_LEN + s_mem_buf_len);

	lhif_h = (struct lhif_header *)(skb_push(skb, sizeof(struct lhif_header)));
	lhif_h->netif = s_chn_idx;

	if (is_md_ee) {
		if (g_rx_wq)
			wake_up_all(g_rx_wq);

		ret = ccci_port_recv_skb(0, DPMAIF_HIF_ID, skb, CLDMA_NET_DATA);
		if (ret)
			ccci_free_skb(skb);
	} else
		ccci_dl_enqueue(0, skb);

alloc_fail:
	s_mem_buf_len = 0;
}

void dpmaif_debug_add(struct dpmaif_debug_header *hdr, void *data)
{
	unsigned long flags;

	if (!hdr || s_mem_buf_size <= 0)
		return;

	spin_lock_irqsave(&s_mem_buf_lock, flags);

	if (add_data_to_buf(hdr, hdr->len, data)) {
		dpmaif_push_data_to_stack(0);
		add_data_to_buf(hdr, hdr->len, data);
	}

	spin_unlock_irqrestore(&s_mem_buf_lock, flags);
}

void dpmaif_debug_update_rx_chn_idx(int chn_idx)
{
	s_chn_idx = chn_idx;
}

static void dpmaif_md_ee_cb(void)
{
	dpmaif_push_data_to_stack(1);
}

void dpmaif_debug_late_init(wait_queue_head_t *rx_wq)
{
	g_rx_wq = rx_wq;
}

void dpmaif_debug_init(void)
{
	spin_lock_init(&s_mem_buf_lock);

	s_mem_buf_size = MAX_DEBUG_BUFFER_LEN;
	s_mem_buf_len  = 0;
	s_buf_pkg_count = 0;

	s_mem_buf_ptr = vmalloc(s_mem_buf_size);
	if (!s_mem_buf_ptr) {
		s_mem_buf_size = 0;
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: vmalloc fail\n", __func__);
	}

	ccci_set_dpmaif_debug_cb(dpmaif_md_ee_cb);
}
