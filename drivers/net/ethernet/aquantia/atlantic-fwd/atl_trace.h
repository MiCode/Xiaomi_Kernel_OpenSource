/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM atlnew

#if !defined(_ATL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _ATL_TRACE_H

#include <linux/tracepoint.h>
#include "atl_desc.h"

DECLARE_EVENT_CLASS(atl_dma_map_class,
	TP_PROTO(int frag_idx, int ring_idx, dma_addr_t daddr, size_t size, struct sk_buff *skb,
		void *vaddr),
	TP_ARGS(frag_idx, ring_idx, daddr, size, skb, vaddr),
	TP_STRUCT__entry(
		__field(int, frag_idx)
		__field(int, ring_idx)
		__field(dma_addr_t, daddr)
		__field(size_t, size)
		__field(struct sk_buff *, skb)
		__field(void *, vaddr)
	),
	TP_fast_assign(
		__entry->frag_idx = frag_idx;
		__entry->ring_idx = ring_idx;
		__entry->daddr = daddr;
		__entry->size = size;
		__entry->skb = skb;
		__entry->vaddr = vaddr;
	),
	TP_printk("idx %d ring idx %d daddr %pad len %#zx skb %p vaddr %p",
		__entry->frag_idx, __entry->ring_idx, &__entry->daddr,
		__entry->size, __entry->skb, __entry->vaddr)
);

#define DEFINE_MAP_EVENT(name)						\
	DEFINE_EVENT(atl_dma_map_class, name,				\
		TP_PROTO(int frag_idx, int ring_idx,			\
			dma_addr_t daddr, size_t size,			\
			struct sk_buff *skb, void *vaddr),		\
		TP_ARGS(frag_idx, ring_idx, daddr, size, skb, vaddr))

DEFINE_MAP_EVENT(atl_dma_map_head);
DEFINE_MAP_EVENT(atl_dma_map_frag);
DEFINE_MAP_EVENT(atl_dma_map_rxbuf);

DECLARE_EVENT_CLASS(atl_dma_unmap_class,
	TP_PROTO(int frag_idx, int ring_idx, dma_addr_t daddr, size_t size,
		struct sk_buff *skb),
	TP_ARGS(frag_idx, ring_idx, daddr, size, skb),
	TP_STRUCT__entry(
		__field(int, frag_idx)
		__field(int, ring_idx)
		__field(dma_addr_t, daddr)
		__field(size_t, size)
		__field(struct sk_buff *, skb)
	),
	TP_fast_assign(
		__entry->frag_idx = frag_idx;
		__entry->ring_idx = ring_idx;
		__entry->daddr = daddr;
		__entry->size = size;
		__entry->skb = skb;
	),
	TP_printk("idx %d ring idx %d daddr %pad len %#zx skb %p",
		__entry->frag_idx, __entry->ring_idx, &__entry->daddr,
		__entry->size, __entry->skb)
);

#define DEFINE_UNMAP_EVENT(name)					\
	DEFINE_EVENT(atl_dma_unmap_class, name,				\
		TP_PROTO(int frag_idx, int ring_idx, dma_addr_t daddr,	\
			size_t size, struct sk_buff *skb),		\
		TP_ARGS(frag_idx, ring_idx, daddr, size, skb))

DEFINE_UNMAP_EVENT(atl_dma_unmap_head);
DEFINE_UNMAP_EVENT(atl_dma_unmap_frag);
DEFINE_UNMAP_EVENT(atl_dma_unmap_rxbuf);

TRACE_EVENT(atl_fill_rx_desc,
	TP_PROTO(int ring_idx, struct atl_rx_desc *desc),
	TP_ARGS(ring_idx, desc),
	TP_STRUCT__entry(
		__field(int, ring_idx)
		__field(dma_addr_t, daddr)
		__field(dma_addr_t, haddr)
	),
	TP_fast_assign(
		__entry->ring_idx = ring_idx;
		__entry->daddr = desc->daddr;
		__entry->haddr = desc->haddr;
	),
	TP_printk("[%d] daddr %pad", __entry->ring_idx, &__entry->daddr)
);

TRACE_EVENT(atl_sync_rx_range,
	TP_PROTO(int ring_idx, dma_addr_t daddr, unsigned long pg_off,
		size_t size),
	TP_ARGS(ring_idx, daddr, pg_off, size),
	TP_STRUCT__entry(
		__field(int, ring_idx)
		__field(dma_addr_t, daddr)
		__field(unsigned long, pg_off)
		__field(size_t, size)
	),
	TP_fast_assign(
		__entry->ring_idx = ring_idx;
		__entry->daddr = daddr;
		__entry->pg_off = pg_off;
		__entry->size = size;
	),
	TP_printk("[%d] daddr %pad pg_off %#lx size %#zx", __entry->ring_idx,
		&__entry->daddr, __entry->pg_off, __entry->size)
);

#endif /* _ATL_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef  TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE atl_trace
#include <trace/define_trace.h>
