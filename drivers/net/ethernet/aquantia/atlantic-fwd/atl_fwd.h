/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2018 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_FWD_H_
#define _ATL_FWD_H_

#include "atl_common.h"

struct atl_fwd_event;

struct atl_fwd_buf_page {
	struct page *page;
	dma_addr_t daddr;
};

/**
 *	atl_fwd_rxbufs - offload engine's ring's Rx buffers
 *
 *	Buffers are allocated by the driver when a ring is created
 *
 *	The entire buffer space for the ring may optionally be
 *	allocated as a single physically-contiguous block.
 *
 *	Descriptors are overwritten with the write-back descriptor
 *	format on Rx and optionally on Tx. To simplify Rx descriptor
 *	refill by the offload engine, vectors containing virtual addresses and
 *	DMA-addresses of each buffer are provided in @vaddr_vec and
 *	@daddr_vec respectively if @ATL_FWR_WANT_BUF_VECS flag is set
 *	on @atl_fwd_request_ring().
 *
 *	If @ATL_FWR_WANT_BUF_VECS is not set, @daddr_vec_base contains
 *	the DMA address of the first buffer page and @vaddr_vec
 *	contains its virtual address.
 *
 *	@daddr_vec_base:	DMA address of the base of the @daddr_vec
 *    	@daddr_vec:		A vector of buffers' DMA ddresses
 *    	@vaddr_vec:		A vector of buffers' virtual addresses
 *    				or first buffer's virtual address
 *    				depending on ring flags
 *    	@paddr:			Physical address of the first (or
 *    				only) buffer page
 */
struct atl_fwd_bufs {
	dma_addr_t daddr_vec_base;
	dma_addr_t *daddr_vec;
	void **vaddr_vec;
	phys_addr_t paddr;

	/* The following is not part of API and subject to change */
	int num_pages;
	int order;
	struct atl_fwd_buf_page bpgs[0];
};

union atl_desc;

/**
 * 	atl_hw_ring - low leverl descriptor ring structure
 *
 * 	@descs:		Pointer to the descriptor ring
 * 	@size:		Number of descriptors in the ring
 * 	@reg_base:	Offset of ring's register block from start of
 * 			BAR 0
 * 	@daddr:		DMA address of the ring
 */
/* atl_hw_ring defined in "atl_hw.h" */

/**
 *	atl_fwd_ring - Offload engine-controlled ring
 *
 *	Buffer space is allocated by the driver on ring creation.
 *
 *	@hw:    	Low-level ring information
 *	@evt:		Ring's event, either an MSI-X vector (either
 *			Tx or Rx) or head pointer writeback address
 *			(Tx ring only). NULL on ring allocation, set
 *			by atl_fwd_request_event()
 *	@bufs:		Ring's buffers. Allocated only if
 *			@ATL_FWR_ALLOC_BUFS flag is set on ring
 *			request.
 *	@nic:		struct atl_nic backreference
 *	@idx:		Ring index
 *	@desc_paddr:	Physical address of the descriptor ring
 */
struct atl_fwd_ring {
	struct atl_hw_ring hw;
	struct atl_fwd_event *evt;
	struct atl_fwd_bufs *bufs;
	struct atl_nic *nic;
	int idx;
	phys_addr_t desc_paddr;

	/* The following is not part of API and subject to change */
	unsigned int flags;
	unsigned long state;
	int buf_size;
	unsigned intr_mod_min;
	unsigned intr_mod_max;
};

enum atl_fwd_event_flags {
	ATL_FWD_EVT_TXWB = BIT(0), /* Event type: 0 for MSI, 1 for Tx
				    * head WB */
	ATL_FWD_EVT_AUTOMASK = BIT(1), /* Disable event after
					* raising, MSI only. */
};

/**
 * 	atl_fwd_event - Ring's notification event
 *
 * 	@flags		Event type and flags
 * 	@ring		Ring backreference
 * 	@msi_addr	MSI message address
 * 	@msi_data	MSI message data
 * 	@idx		MSI index (0 .. 31)
 * 	@tx_head_wrb	Tx head writeback location
 */
struct atl_fwd_event {
	enum atl_fwd_event_flags flags;
	struct atl_fwd_ring *ring;
	union {
		struct {
			dma_addr_t msi_addr;
			uint32_t msi_data;
			int idx;
		};
		dma_addr_t tx_head_wrb;
	};
};

enum atl_fwd_ring_flags {
	ATL_FWR_TX = BIT(0),	/* Direction: 0 for Rx, 1 for Tx */
	ATL_FWR_VLAN = BIT(1),	/* Enable VLAN tag stripping / insertion */
	ATL_FWR_LXO = BIT(2),	/* Enable LRO / LSO */
	ATL_FWR_ALLOC_BUFS = BIT(3), /* Allocate buffers */
	ATL_FWR_CONTIG_BUFS = BIT(4), /* Alloc buffers as physically
				       * contiguous. May fail if
				       * total buffer space required
				       * is larger than a max-order
				       * compound page. */
	ATL_FWR_WANT_BUF_VECS = BIT(5), /* Alloc and fill per-buffer
					 * DMA and virt address
					 * vectors. If unset, first
					 * buffer's daddr and vaddr
					 * are provided in ring's
					 * @daddr_vec_base and @vaddr_vec */
};

/**
 * atl_fwd_request_ring() - Create a ring for an offload engine
 *
 * 	@ndev:		network device
 * 	@flags:		ring flags
 * 	@ring_size:	number of descriptors
 * 	@buf_size:	individual buffer's size
 * 	@page_order:	page order to use when @ATL_FWR_CONTIG_BUFS is
 * 			not set
 *
 * atl_fwd_request_ring() creates a ring for an offload engine,
 * allocates buffer memory if @ATL_FWR_ALLOC_BUFS flag is set,
 * initializes ring's registers and fills the address fields in
 * descriptors. Ring is inactive until explicitly enabled via
 * atl_fwd_enable_ring().
 *
 * Buffers can be allocated either as a single physically-contiguous
 * compound page, or as a sequence of compound pages of @page_order
 * order. In the latter case, depending on the requested buffer size,
 * tweaking the page order allows to pack buffers into buffer pages
 * with less wasted space.
 *
 * Returns the ring pointer on success, ERR_PTR(error code) on failure
 */
struct atl_fwd_ring *atl_fwd_request_ring(struct net_device *ndev,
	int flags, int ring_size, int buf_size, int page_order);

/**
 * atl_fwd_release_ring() - Free offload engine's ring
 *
 * 	@ring:	ring to be freed
 *
 * Stops the ring, frees buffers if they were allocated, disables and
 * releases ring's event if non-NULL, and frees the ring.
 */
void atl_fwd_release_ring(struct atl_fwd_ring *ring);

/**
 * atl_fwd_set_ring_intr_mod() - Set ring's interrupt moderation
 * delays
 *
 * 	@ring:	ring
 * 	@min:	min delay
 * 	@max:	max delay
 *
 * Each ring has two configurable interrupt moderation timers. When an
 * interrupt condition occurs (write-back of the final descriptor of a
 * packet on receive, or writeback on a transmit descriptor with WB
 * bit set), the min timer is restarted unconditionally and max timer
 * is started only if it's not running yet. When any of the timers
 * expires, the interrupt is signalled.
 *
 * Thus if a single interrupt event occurs it will be subjected to min
 * delay. If subsequent events keep occuring with intervals less than
 * min_delay between each other, the interrupt will be triggered
 * max_delay after the initial event.
 *
 * When called with negative @min or @max, the corresponding setting
 * is left unchanged.
 *
 * Interrupt moderation is only supported for MSI-X vectors, not head
 * pointer writeback events.
 *
 * Returns 0 on success or -EINVAL on attempt to set moderation delays
 * for a ring with attached Tx WB event.
 */
int atl_fwd_set_ring_intr_mod(struct atl_fwd_ring *ring, int min, int max);

/**
 * atl_fwd_enable_channel() - Enable offload engine's ring
 *
 * 	@ring: ring to be enabled
 *
 * Starts the ring. Returns 0 on success or negative error code.
 */
int atl_fwd_enable_ring(struct atl_fwd_ring *ring);
/**
 * atl_fwd_disable_channel() - Disable offload engine's ring
 *
 * 	@ring: ring to be disabled
 *
 * Stops and resets the ring. On next ring enable head and tail
 * pointers will be zero.
 */
void atl_fwd_disable_ring(struct atl_fwd_ring *ring);

/**
 * atl_fwd_request_event() - Creates and attaches a ring notification
 * event
 *
 * 	@evt:		event structure
 *
 * Caller must allocate a struct atl_fwd_event and fill the @flags,
 * @ring and either @tx_head_wrb or @msi_addr and @msi_data depending
 * on the type bit in @flags. Event is created in disabled state.
 *
 * For an MSI event type, an MSI vector table slot is
 * allocated and programmed, and it's index is saved in @evt->idx.
 *
 * @evt is then attached to the ring.
 *
 * Returns 0 on success or negative error code.
 */
int atl_fwd_request_event(struct atl_fwd_event *evt);

/**
 * atl_fwd_release_event() - Release a ring notification event
 *
 * 	@evt:		event structure
 *
 * Disables the event if enabled, frees the MSI vector for an MSI-type
 * event and detaches @evt from the ring. The @evt structure itself is
 * not freed.
 */
void atl_fwd_release_event(struct atl_fwd_event *evt);

/**
 * atl_fwd_enable_event() - Enable a ring event
 *
 * 	@evt:		event structure
 *
 * Enables the event.
 *
 * Returns 0 on success or negative error code.
 */
int atl_fwd_enable_event(struct atl_fwd_event *evt);

/**
 * atl_fwd_disable_event() - Disable a ring event
 *
 * 	@evt:		event structure
 *
 * Disables the event.
 *
 * Returns 0 on success or negative error code.
 */
int atl_fwd_disable_event(struct atl_fwd_event *evt);

int atl_fwd_receive_skb(struct net_device *ndev, struct sk_buff *skb);
int atl_fwd_transmit_skb(struct net_device *ndev, struct sk_buff *skb);

enum atl_fwd_ring_state {
	ATL_FWR_ST_ENABLED = BIT(0),
};

#endif
