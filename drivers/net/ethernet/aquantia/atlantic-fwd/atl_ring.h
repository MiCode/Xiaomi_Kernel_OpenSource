/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2017 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_RING_H_
#define _ATL_RING_H_

#include <linux/compiler.h>

#include "atl_common.h"
#include "atl_desc.h"
#include "atl_ring_desc.h"
#include "atl_ptp.h"

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
		struct atl_desc_ring *__ring = (ring);			\
		uint32_t space = READ_ONCE(__ring->head) -		\
			READ_ONCE(__ring->tail) - 1;			\
		(int32_t)space < 0 ? space + __ring->hw.size : space;	\
	})

#define ring_occupied(ring)						\
	({								\
		struct atl_desc_ring *__ring = (ring);			\
		uint32_t occupied = READ_ONCE(__ring->tail) -		\
			READ_ONCE(__ring->head);			\
		(int32_t)occupied < 0 ? occupied + __ring->hw.size	\
			: occupied;					\
	})

#define bump_ptr(ptr, ring, amount)					\
	({								\
		struct atl_desc_ring *__ring = (ring);			\
		uint32_t __res = offset_ptr(ptr, &__ring->hw, amount);	\
		(ptr) = __res;						\
		__res;							\
	})

/* These don't have to be atomic, because Tx tail is only adjusted
 * in ndo->start_xmit which is serialized by the stack and the rest are
 * only adjusted in NAPI poll which is serialized by NAPI */
#define bump_tail(ring, amount) do {					\
	struct atl_desc_ring *__ring = (ring);				\
	uint32_t __ptr = READ_ONCE(__ring->tail);			\
	__ring->tail = offset_ptr(__ptr, &__ring->hw, amount);\
	} while (0)

#define bump_head(ring, amount) do {					\
	struct atl_desc_ring *__ring = (ring);				\
	uint32_t __ptr = READ_ONCE(__ring->head);			\
	__ring->head = offset_ptr(__ptr, &__ring->hw, amount);\
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

struct legacy_irq_work {
	struct work_struct work;

	struct napi_struct *napi;
};
static inline struct legacy_irq_work *to_irq_work(struct work_struct *work)
{
	return container_of(work, struct legacy_irq_work, work);
};

enum atl_queue_type {
	ATL_QUEUE_REGULAR,
	ATL_QUEUE_PTP,
	ATL_QUEUE_HWTS,
};

struct ____cacheline_aligned atl_queue_vec {
	struct atl_desc_ring tx;
	struct atl_desc_ring rx;
	struct napi_struct napi;
	struct atl_nic *nic;
	unsigned idx;
	char name[IFNAMSIZ + 10];
	cpumask_t affinity_hint;
	enum atl_queue_type type;
	struct work_struct *work;
};

#define atl_for_each_qvec(nic, qvec)				\
	for (qvec = &(nic)->qvecs[0];				\
	     qvec < &(nic)->qvecs[(nic)->nvecs]; qvec++)

static inline struct atl_hw *ring_hw(struct atl_desc_ring *ring)
{
	return &ring->nic->hw;
}

void atl_init_qvec(struct atl_nic *nic, struct atl_queue_vec *qvec, int idx);
int atl_alloc_qvec(struct atl_queue_vec *qvec);
void atl_free_qvec(struct atl_queue_vec *qvec);
int atl_start_qvec(struct atl_queue_vec *qvec);
void atl_stop_qvec(struct atl_queue_vec *qvec);
int atl_poll_qvec(struct atl_queue_vec *qvec, int budget);

static inline int atl_qvec_intr(struct atl_queue_vec *qvec)
{
#if IS_REACHABLE(CONFIG_PTP_1588_CLOCK)
	if (unlikely(qvec->idx >= qvec->nic->nvecs))
		return atl_ptp_qvec_intr(qvec);
#endif

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
#define atl_update_ring_stat(ring, stat, delta)			\
do {								\
	struct atl_desc_ring *_ring = (ring);			\
								\
	u64_stats_update_begin(&_ring->syncp);			\
	_ring->stats.stat += (delta);				\
	u64_stats_update_end(&_ring->syncp);			\
} while (0)

int atl_init_rx_ring(struct atl_desc_ring *rx, enum atl_queue_type type);
int atl_init_tx_ring(struct atl_desc_ring *tx);

netdev_tx_t atl_map_skb(struct sk_buff *skb, struct atl_desc_ring *ring);
int atl_tx_full(struct atl_desc_ring *ring, int needed);

typedef int (*rx_skb_handler_t)(struct atl_desc_ring *ring,
				struct sk_buff *skb);
int atl_clean_rx(struct atl_desc_ring *ring, int budget,
		 rx_skb_handler_t rx_skb_func);
int atl_clean_hwts_rx(struct atl_desc_ring *ring, int budget);
void atl_clear_rx_bufs(struct atl_desc_ring *ring);

#ifdef ATL_RINGS_IN_UC_MEM

#define DECLARE_SCRATCH_DESC(_name) union atl_desc _name
#define DESC_PTR(_ring, _idx, _scratch) (&(_scratch))
#define COMMIT_DESC(_ring, _idx, _scratch)		\
	(_ring)->hw.descs[_idx] = (_scratch)
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
