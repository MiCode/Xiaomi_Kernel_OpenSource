/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_RING_DESC_H_
#define _ATL_RING_DESC_H_

#include <linux/types.h>
#include <linux/u64_stats_sync.h>

#include "atl_desc.h"
#include "atl_hw.h"
#include "atl_stats.h"

struct atl_nic;
struct atl_fwd_event;

struct atl_desc_ring {
	struct atl_hw_ring hw;
	struct atl_nic *nic;
	uint32_t head;
	uint32_t tail;
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
#if IS_ENABLED(CONFIG_ATLFWD_FWD_NETLINK)
	u32 tx_hw_head;
	union {
		struct atl_fwd_event *tx_evt;
		struct atl_fwd_event *rx_evt;
	};
	/* RX ring polling */
	struct timer_list *rx_poll_timer;
#endif
};

#endif /* _ATL_RING_DESC_H_ */
