/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2019 aQuantia Corporation. All rights reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 */

#ifndef _ATL_FWDNL_H_
#define _ATL_FWDNL_H_

/* family name */
#define ATL_FWD_GENL_NAME "atl_fwd"

/* commands */
enum atlfwd_nl_command {
	ATL_FWD_CMD_UNSPEC,

	ATL_FWD_CMD_REQUEST_RING,
	ATL_FWD_CMD_RELEASE_RING,
	ATL_FWD_CMD_ENABLE_RING,
	ATL_FWD_CMD_DISABLE_RING,

	ATL_FWD_CMD_DISABLE_REDIRECTIONS,
	ATL_FWD_CMD_FORCE_ICMP_TX_VIA,
	ATL_FWD_CMD_FORCE_TX_VIA,

	ATL_FWD_CMD_RING_STATUS,

	ATL_FWD_CMD_DUMP_RING,
	ATL_FWD_CMD_SET_TX_BUNCH,

	ATL_FWD_CMD_REQUEST_EVENT,
	ATL_FWD_CMD_RELEASE_EVENT,
	ATL_FWD_CMD_ENABLE_EVENT,
	ATL_FWD_CMD_DISABLE_EVENT,

	ATL_FWD_CMD_GET_RX_QUEUE,
	ATL_FWD_CMD_GET_TX_QUEUE,

	/* keep last */
	NUM_ATL_FWD_CMD,
	ATL_FWD_CMD_MAX = NUM_ATL_FWD_CMD - 1
};

enum atlfwd_nl_attribute {
	ATL_FWD_ATTR_INVALID,

	ATL_FWD_ATTR_IFNAME,

	/* REQUEST_RING attributes */
	ATL_FWD_ATTR_FLAGS,
	ATL_FWD_ATTR_RING_SIZE,
	ATL_FWD_ATTR_BUF_SIZE,
	ATL_FWD_ATTR_PAGE_ORDER,

	/* RELEASE_RING attributes */
	ATL_FWD_ATTR_RING_INDEX,
	/* ENABLE_RING / DISABLE_RING use RING_INDEX attribute above */

	/* RING_STATUS reply attributes */
	ATL_FWD_ATTR_RING_STATUS,
	ATL_FWD_ATTR_RING_IS_TX,
	ATL_FWD_ATTR_RING_FLAGS,

	/* ATL_FWD_CMD_SET_TX_BUNCH atributes */
	ATL_FWD_ATTR_TX_BUNCH_SIZE,

	/* ATL_FWD_CMD_GET_(RX|TX)_QUEUE attributes */
	ATL_FWD_ATTR_QUEUE_INDEX,

	/* keep last */
	NUM_ATL_FWD_ATTR,
	ATL_FWD_ATTR_MAX = NUM_ATL_FWD_ATTR - 1
};

enum atlfwd_nl_ring_status {
	ATL_FWD_RING_STATUS_INVALID,

	ATL_FWD_RING_STATUS_RELEASED,
	ATL_FWD_RING_STATUS_CREATED_DISABLED,
	ATL_FWD_RING_STATUS_ENABLED,

	/* keep last */
	NUM_ATL_FWD_RING_STATUS,
	ATL_FWD_RING_STATUS_MAX = NUM_ATL_FWD_RING_STATUS - 1
};

#ifdef __KERNEL__
#include <linux/netdevice.h>
#include <linux/version.h>

struct atl_fwd_ring;
struct atl_desc_ring;

int atlfwd_nl_init(void);
void atlfwd_nl_on_probe(struct net_device *ndev);
void atlfwd_nl_on_remove(struct net_device *ndev);
int atlfwd_nl_on_open(struct net_device *ndev);
void atlfwd_nl_exit(void);

netdev_tx_t atlfwd_nl_xmit(struct sk_buff *skb, struct net_device *ndev);
u16 atlfwd_nl_select_queue_fallback(struct net_device *dev, struct sk_buff *skb,
				    struct net_device *arg3,
				    select_queue_fallback_t fallback);

bool atlfwd_nl_is_tx_fwd_ring_created(struct net_device *ndev,
				      const int fwd_ring_index);
bool atlfwd_nl_is_rx_fwd_ring_created(struct net_device *ndev,
				      const int fwd_ring_index);

struct atl_fwd_ring *atlfwd_nl_get_fwd_ring(struct net_device *ndev,
					    const int ring_index);
struct atl_desc_ring *atlfwd_nl_get_fwd_ring_desc(struct atl_fwd_ring *ring);

bool is_atlfwd_device(const struct net_device *dev);
#endif

#endif
