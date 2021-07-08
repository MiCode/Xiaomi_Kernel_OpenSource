// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "dpmaif_debug.h"
#include "net_pool.h"

#define TAG "dbg"



void dpmaif_debug_init_data(struct dpmaif_debug_data_t *dbg_data,
		u8 type, u8 verion, u8 qidx)
{
	struct dpmaif_debug_header *dbg_header;

	memset(dbg_data, 0, sizeof(struct dpmaif_debug_data_t));
	dbg_data->iph = (struct iphdr *)(dbg_data->data);
	dbg_data->iph->version = 0;
	dbg_data->iph->saddr = 0;
	dbg_data->iph->daddr = dbg_data->iph->saddr;

	dbg_header = (struct dpmaif_debug_header *)(dbg_data->data +
			sizeof(struct iphdr));
	dbg_header->type = type;
	dbg_header->version = verion;
	dbg_header->qidx = qidx;

	dbg_data->pdata = (void *)(dbg_data->data) + DEBUG_HEADER_LEN;
}

inline int dpmaif_debug_push_data(
		struct dpmaif_debug_data_t *dbg_data,
		u32 qno, unsigned int chn_idx)
{
	struct lhif_header *lhif_h;
	struct sk_buff *skb;

	if (dbg_data->idx == 0)
		return 0;

	skb = __dev_alloc_skb(DEBUG_HEADER_LEN + MAX_DEBUG_BUFFER_LEN,
					GFP_ATOMIC);
	if (!skb)
		return -1;

	skb->len = 0;
	skb_reset_tail_pointer(skb);
	skb->ip_summed = 0;

	dbg_data->iph->ihl = (sizeof(struct iphdr) >> 2);
	dbg_data->iph->tot_len = htons(DEBUG_HEADER_LEN + dbg_data->idx);

	memcpy(skb->data, dbg_data->data, DEBUG_HEADER_LEN + dbg_data->idx);
	skb_put(skb, DEBUG_HEADER_LEN + dbg_data->idx);

	lhif_h = (struct lhif_header *)(skb_push(skb,
					sizeof(struct lhif_header)));
	lhif_h->netif = chn_idx;

	ccci_dl_enqueue(qno, skb);

	dbg_data->idx = 0;
	return 0;
}

inline int dpmaif_debug_add_data(struct dpmaif_debug_data_t *dbg_data,
		void *sdata, int len)
{
	unsigned int *time;

	if ((dbg_data->idx + 4 + len) > MAX_DEBUG_BUFFER_LEN)
		return -1;

	time = (unsigned int *)(dbg_data->pdata + dbg_data->idx);
	(*time) = (unsigned int)(local_clock() / 1000000);
	dbg_data->idx += 4;

	memcpy(dbg_data->pdata + dbg_data->idx, sdata, len);
	dbg_data->idx += len;

	return 0;
}


