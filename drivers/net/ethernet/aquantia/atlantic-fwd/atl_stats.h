/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_STATS_H_
#define _ATL_STATS_H_

#include <linux/types.h>

struct atl_rx_ring_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t linear_dropped;
	uint64_t alloc_skb_failed;
	uint64_t reused_head_page;
	uint64_t reused_data_page;
	uint64_t alloc_head_page;
	uint64_t alloc_data_page;
	uint64_t alloc_head_page_failed;
	uint64_t alloc_data_page_failed;
	uint64_t non_eop_descs;
	uint64_t mac_err;
	uint64_t csum_err;
	uint64_t multicast;
};

struct atl_tx_ring_stats {
	uint64_t packets;
	uint64_t bytes;
	uint64_t tx_busy;
	uint64_t tx_restart;
	uint64_t dma_map_failed;
};

struct atl_ring_stats {
	union {
		struct atl_rx_ring_stats rx;
		struct atl_tx_ring_stats tx;
	};
};

struct atl_ether_stats {
	uint64_t rx_pause;
	uint64_t tx_pause;
	uint64_t rx_ether_drops;
	uint64_t rx_ether_octets;
	uint64_t rx_ether_pkts;
	uint64_t rx_ether_broacasts;
	uint64_t rx_ether_multicasts;
	uint64_t rx_ether_crc_align_errs;
	uint64_t rx_filter_host;
	uint64_t rx_filter_lost;
};

struct atl_global_stats {
	struct atl_rx_ring_stats rx;
	struct atl_tx_ring_stats tx;

	/* MSM counters can't be reset without full HW reset, so
	 * store them in relative form:
	 * eth[i] == HW_counter - eth_base[i]
	 */
	struct atl_ether_stats eth;
	struct atl_ether_stats eth_base;
};

struct atl_fwd_ring;

#ifdef CONFIG_ATLFWD_FWD_NETLINK
void atl_fwd_get_ring_stats(struct atl_fwd_ring *ring,
			    struct atl_ring_stats *stats);
#endif

#endif /* _ATL_STATS_H_ */
