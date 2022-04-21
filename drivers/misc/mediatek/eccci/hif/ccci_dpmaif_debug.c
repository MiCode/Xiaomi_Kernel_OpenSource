// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "ccci_dpmaif_debug.h"
#include "ccci_dpmaif_com.h"
#include "net_pool.h"
#include "ccci_port.h"
#include "ccci_hif.h"

#define TAG "dbg"


#ifdef ENABLE_DPMAIF_ISR_LOG

struct dpmaif_isr_log {
	u64 ts_start;
	u64 ts_end;
	u32 irq_cnt[64];
};

#define ISR_LOG_DATA_LEN 10

static struct dpmaif_isr_log *g_isr_log;
static unsigned long long g_pre_time;
static unsigned int g_isr_log_idx;


void ccci_dpmaif_print_irq_log(void)
{
	int i, j;
	char string[300];
	int len = 0, pos = 0;
	u64 tss1, tss2, tse1, tse2;

	if (g_isr_log == NULL)
		return;

	CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG,
		"dump dpmaif isr log: L2TISAR0(0~31) L2RISAR0(32~63)\n");

	for (i = 0; i < ISR_LOG_DATA_LEN; i++) {
		if (g_isr_log[i].ts_start == 0)
			continue;

		tss1 = g_isr_log[i].ts_start;
		tss2 = do_div(tss1, 1000000000);

		tse1 = g_isr_log[i].ts_end;
		tse2 = do_div(tse1, 1000000000);

		len = snprintf(string+pos, 300-pos, "%d|%lu.%06lu~%lu.%06lu->",
						i, (unsigned long)tss1, (unsigned long)(tss2/1000),
						(unsigned long)tse1, (unsigned long)(tse2/1000));
		if ((len <= 0) || (len >= 300-pos))
			break;

		pos += len;

		for (j = 0; j < 64; j++) {
			if (g_isr_log[i].irq_cnt[j] == 0)
				continue;

			len = snprintf(string+pos, 300-pos, " %u-%u", j, g_isr_log[i].irq_cnt[j]);
			if ((len <= 0) || (len >= 300-pos))
				break;

			pos += len;
		}

		pos = 0;

		CCCI_BUF_LOG_TAG(0, CCCI_DUMP_DPMAIF, TAG, "%s\n", string);
	}
}

inline int ccci_dpmaif_record_isr_cnt(unsigned long long ts,
		unsigned int L2TISAR0, unsigned int L2RISAR0)
{
	unsigned int i;

	if (g_isr_log == NULL)
		return 0;

	if ((ts - g_pre_time) >= 1000000000) {  // > 1s
		g_isr_log_idx++;
		if (g_isr_log_idx >= ISR_LOG_DATA_LEN)
			g_isr_log_idx = 0;

		memset(g_isr_log[g_isr_log_idx].irq_cnt, 0,
				sizeof(g_isr_log[g_isr_log_idx].irq_cnt));

		g_isr_log[g_isr_log_idx].ts_start = ts;
		g_pre_time = ts;
	}

	g_isr_log[g_isr_log_idx].ts_end = ts;

	for (i = 0; i < 32; i++) {
		if (L2TISAR0 == 0)
			break;

		if (L2TISAR0 & (1<<i)) {
			L2TISAR0 &= (~(1<<i));
			g_isr_log[g_isr_log_idx].irq_cnt[i]++;
			if (g_isr_log[g_isr_log_idx].irq_cnt[i] > 50000)
				return -1;
		}
	}

	for (i = 0; i < 32; i++) {
		if (L2RISAR0 == 0)
			break;

		if (L2RISAR0 & (1<<i)) {
			L2RISAR0 &= (~(1<<i));
			g_isr_log[g_isr_log_idx].irq_cnt[i+32]++;
			if (g_isr_log[g_isr_log_idx].irq_cnt[i+32] > 50000)
				return -1;
		}
	}

	return 0;
}
#endif

#ifdef ENABLE_DPMAIF_DEBUG_LOG
static char *s_mem_buf_ptr;
static u32   s_mem_buf_size;
static u32   s_mem_buf_len;
static int   s_chn_idx = -1;

static spinlock_t s_mem_buf_lock;
static wait_queue_head_t *g_rx_wq;

#define MAX_SKB_TBL_CNT 1000
static struct sk_buff *g_skb_tbl[MAX_SKB_TBL_CNT];
static unsigned int g_skb_tbl_rdx;
static unsigned int g_skb_tbl_wdx;

inline struct sk_buff *ccci_dequeue_debug_skb(void)
{
	struct sk_buff *skb = NULL;

	if (!get_ringbuf_used_cnt(MAX_SKB_TBL_CNT, g_skb_tbl_rdx, g_skb_tbl_wdx))
		return NULL;

	skb = g_skb_tbl[g_skb_tbl_rdx];

	g_skb_tbl_rdx = get_ringbuf_next_idx(MAX_SKB_TBL_CNT, g_skb_tbl_rdx, 1);

	return skb;
}

inline int ccci_get_debug_skb_cnt(void)
{
	return get_ringbuf_used_cnt(MAX_SKB_TBL_CNT, g_skb_tbl_rdx, g_skb_tbl_wdx);
}

static inline int ccci_enqueue_debug_skb(struct sk_buff *skb)
{
	if (get_ringbuf_free_cnt(MAX_SKB_TBL_CNT, g_skb_tbl_rdx, g_skb_tbl_wdx) == 0)
		return -1;

	g_skb_tbl[g_skb_tbl_wdx] = skb;

	g_skb_tbl_wdx = get_ringbuf_next_idx(MAX_SKB_TBL_CNT, g_skb_tbl_wdx, 1);

	return 0;
}

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

		ret = ccci_port_recv_skb(DPMAIF_HIF_ID, skb, CLDMA_NET_DATA);
		if (ret)
			dev_kfree_skb_any(skb);

	} else {
		if (ccci_enqueue_debug_skb(skb))
			dev_kfree_skb_any(skb);
	}

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
	unsigned long flags;

	spin_lock_irqsave(&s_mem_buf_lock, flags);

	dpmaif_push_data_to_stack(1);

	spin_unlock_irqrestore(&s_mem_buf_lock, flags);
}

void ccci_dpmaif_debug_late_init(wait_queue_head_t *rx_wq)
{
	g_rx_wq = rx_wq;
}
#endif

void ccci_dpmaif_debug_init(void)
{
#ifdef ENABLE_DPMAIF_DEBUG_LOG
	spin_lock_init(&s_mem_buf_lock);

	s_mem_buf_size = MAX_DEBUG_BUFFER_LEN;
	s_mem_buf_len  = 0;
	g_skb_tbl_rdx = 0;
	g_skb_tbl_wdx = 0;

	s_mem_buf_ptr = vmalloc(s_mem_buf_size);
	if (!s_mem_buf_ptr) {
		s_mem_buf_size = 0;
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: vmalloc fail\n", __func__);
	}

	ccci_set_dpmaif_debug_cb(dpmaif_md_ee_cb);
#endif

#ifdef ENABLE_DPMAIF_ISR_LOG
	g_isr_log = kzalloc(sizeof(struct dpmaif_isr_log) * ISR_LOG_DATA_LEN, GFP_KERNEL);
	if (!g_isr_log)
		CCCI_ERROR_LOG(-1, TAG, "[%s] error: alloc g_isr_log fail\n", __func__);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
	else
		mrdump_mini_add_extra_file((unsigned long)g_isr_log, __pa_nodebug(g_isr_log),
			(sizeof(struct dpmaif_isr_log) * ISR_LOG_DATA_LEN), "DPMAIF_ISR");
#endif
#endif
}
