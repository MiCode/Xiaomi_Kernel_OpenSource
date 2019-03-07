/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2017 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_RING_H_
#define _ATL_RING_H_

#include <linux/compiler.h>

#include "atl_common.h"
#include "atl_desc.h"

//#define ATL_RINGS_IN_UC_MEM

#define ATL_TX_DESC_WB
//#define ATL_TX_HEAD_WB

#define ATL_RX_HEADROOM (NET_SKB_PAD + NET_IP_ALIGN)
#define ATL_RX_TAILROOM 64u
#define ATL_RX_HEAD_ORDER 0
#define ATL_RX_DATA_ORDER 0

/* Header space in skb. Must be a multiple of L1_CACHE_BYTES */
#define ATL_RX_HDR_SIZE 256u
#define ATL_RX_HDR_OVRHD SKB_DATA_ALIGN(ATL_RX_HEADROOM +	\
		SKB_DATA_ALIGN(sizeof(struct skb_shared_info)))
#define ATL_RX_BUF_SIZE 2048

#define ATL_MAX_RX_LINEAR_MTU (ATL_RX_BUF_SIZE - ETH_HLEN)

#define ring_space(ring)						\
	({								\
		typeof(ring) __ring = (ring);				\
		uint32_t space = READ_ONCE(__ring->head) -		\
			READ_ONCE(__ring->tail) - 1;			\
		(int32_t)space < 0 ? space + __ring->hw.size : space;	\
	})

#define ring_occupied(ring)						\
	({								\
		typeof(ring) __ring = (ring);				\
		uint32_t occupied = READ_ONCE(__ring->tail) -		\
			READ_ONCE(__ring->head);			\
		(int32_t)occupied < 0 ? occupied + __ring->hw.size	\
			: occupied;					\
	})

#define bump_ptr(ptr, ring, amount)					\
	({								\
		uint32_t __res = offset_ptr(ptr, ring, amount);		\
		(ptr) = __res;						\
		__res;							\
	})

/* These don't have to be atomic, because Tx tail is only adjusted
 * in ndo->start_xmit which is serialized by the stack and the rest are
 * only adjusted in NAPI poll which is serialized by NAPI */
#define bump_tail(ring, amount) do {					\
	uint32_t __ptr = READ_ONCE((ring)->tail);			\
	WRITE_ONCE((ring)->tail, offset_ptr(__ptr, ring, amount));	\
	} while (0)

#define bump_head(ring, amount) do {					\
	uint32_t __ptr = READ_ONCE((ring)->head);			\
	WRITE_ONCE((ring)->head, offset_ptr(__ptr, ring, amount));	\
	} while (0)

struct atl_rxpage {
	struct page *page;
	dma_addr_t daddr;
	unsigned mapcount; 	/* not atomic_t because accesses are
				 * serialized by NAPI */
	unsigned order;
};

struct atl_pgref {
	struct atl_rxpage *rxpage;
	unsigned pg_off;
};

struct atl_cb {
	struct atl_pgref pgref;
	bool head;
};
#define ATL_CB(skb) ((struct atl_cb *)(skb)->cb)

struct atl_rxbuf {
	struct sk_buff *skb;
	struct atl_pgref head;
	struct atl_pgref data;
};

struct atl_txbuf {
	struct sk_buff *skb;
	uint32_t last; /* index of eop descriptor */
	unsigned bytes;
	unsigned packets;
	DEFINE_DMA_UNMAP_ADDR(daddr);
	DEFINE_DMA_UNMAP_LEN(len);
};

struct atl_desc_ring {
	struct atl_hw_ring hw;
	uint32_t head, tail;
	union {
		/* Rx ring only */
		uint32_t next_to_recycle;
		/* Tx ring only, template desc for atl_map_tx_skb() */
		union atl_desc desc;
	};
	union {
		struct atl_rxbuf *rxbufs;
		struct atl_txbuf *txbufs;
		void *bufs;
	};
	struct atl_queue_vec *qvec;
	struct u64_stats_sync syncp;
	struct atl_ring_stats stats;
};

struct atl_queue_vec {
	struct atl_desc_ring tx;
	struct atl_desc_ring rx;
	struct device *dev;	/* pdev->dev for DMA */
	struct napi_struct napi;
	struct atl_nic *nic;
	unsigned idx;
	char name[IFNAMSIZ + 10];
#ifdef ATL_COMPAT_PCI_ALLOC_IRQ_VECTORS_AFFINITY
	cpumask_t affinity_hint;
#endif
};

#define atl_for_each_qvec(nic, qvec)				\
	for (qvec = &(nic)->qvecs[0];				\
	     qvec < &(nic)->qvecs[(nic)->nvecs]; qvec++)

static inline struct atl_hw *ring_hw(struct atl_desc_ring *ring)
{
	return &ring->qvec->nic->hw;
}

static inline int atl_qvec_intr(struct atl_queue_vec *qvec)
{
	return qvec->idx + ATL_NUM_NON_RING_IRQS;
}

static inline void *atl_buf_vaddr(struct atl_pgref *pgref)
{
	return page_to_virt(pgref->rxpage->page) + pgref->pg_off;
}

static inline dma_addr_t atl_buf_daddr(struct atl_pgref *pgref)
{
	return pgref->rxpage->daddr + pgref->pg_off;
}

void atl_get_ring_stats(struct atl_desc_ring *ring,
	struct atl_ring_stats *stats);

#ifdef ATL_RINGS_IN_UC_MEM

#define DECLARE_SCRATCH_DESC(_name) union atl_desc _name
#define DESC_PTR(_ring, _idx, _scratch) (&(_scratch))
#define COMMIT_DESC(_ring, _idx, _scratch)		\
	WRITE_ONCE((_ring)->hw.descs[_idx], (_scratch))
#define FETCH_DESC(_ring, _idx, _scratch)			\
do {								\
	(_scratch) = READ_ONCE((_ring)->hw.descs[_idx]);	\
	dma_rmb();						\
} while(0)

#define DESC_RMB()

#else // ATL_RINGS_IN_UC_MEM

#define DECLARE_SCRATCH_DESC(_name)
#define DESC_PTR(_ring, _idx, _scratch) (&(_ring)->hw.descs[_idx])
#define COMMIT_DESC(_ring, _idx, _scratch)
#define FETCH_DESC(_ring, _idx, _scratch)
#define DESC_RMB() dma_rmb()

#endif // ATL_RINGS_IN_UC_MEM

#ifdef ATL_TX_HEAD_WB
#error Head ptr writeback not implemented
#elif !defined(ATL_TX_DESC_WB)
static inline uint32_t atl_get_tx_head(struct atl_desc_ring *ring)
{
	return atl_read(ring_hw(ring), ATL_TX_RING_HEAD(ring->idx)) & 0x1fff;
}
#endif

#endif
