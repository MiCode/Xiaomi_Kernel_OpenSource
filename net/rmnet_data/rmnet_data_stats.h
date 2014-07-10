/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *
 * RMNET Data statistics
 *
 */

#ifndef _RMNET_DATA_STATS_H_
#define _RMNET_DATA_STATS_H_

enum rmnet_skb_free_e {
	RMNET_STATS_SKBFREE_UNKNOWN,
	RMNET_STATS_SKBFREE_BRDG_NO_EGRESS,
	RMNET_STATS_SKBFREE_DELIVER_NO_EP,
	RMNET_STATS_SKBFREE_IPINGRESS_NO_EP,
	RMNET_STATS_SKBFREE_MAPINGRESS_BAD_MUX,
	RMNET_STATS_SKBFREE_MAPINGRESS_MUX_NO_EP,
	RMNET_STATS_SKBFREE_MAPINGRESS_AGGBUF,
	RMNET_STATS_SKBFREE_INGRESS_NOT_EXPECT_MAPD,
	RMNET_STATS_SKBFREE_INGRESS_NOT_EXPECT_MAPC,
	RMNET_STATS_SKBFREE_EGR_MAPFAIL,
	RMNET_STATS_SKBFREE_VND_NO_EGRESS,
	RMNET_STATS_SKBFREE_MAPC_BAD_MUX,
	RMNET_STATS_SKBFREE_MAPC_MUX_NO_EP,
	RMNET_STATS_SKBFREE_AGG_CPY_EXPAND,
	RMNET_STATS_SKBFREE_AGG_INTO_BUFF,
	RMNET_STATS_SKBFREE_DEAGG_MALFORMED,
	RMNET_STATS_SKBFREE_DEAGG_CLONE_FAIL,
	RMNET_STATS_SKBFREE_DEAGG_UNKOWN_IP_TYP,
	RMNET_STATS_SKBFREE_DEAGG_DATA_LEN_0,
	RMNET_STATS_SKBFREE_INGRESS_BAD_MAP_CKSUM,
	RMNET_STATS_SKBFREE_MAX
};

enum rmnet_queue_xmit_e {
	RMNET_STATS_QUEUE_XMIT_UNKNOWN,
	RMNET_STATS_QUEUE_XMIT_EGRESS,
	RMNET_STATS_QUEUE_XMIT_AGG_FILL_BUFFER,
	RMNET_STATS_QUEUE_XMIT_AGG_TIMEOUT,
	RMNET_STATS_QUEUE_XMIT_AGG_CPY_EXP_FAIL,
	RMNET_STATS_QUEUE_XMIT_MAX
};

void rmnet_kfree_skb(struct sk_buff *skb, unsigned int reason);
void rmnet_stats_queue_xmit(int rc, unsigned int reason);
void rmnet_stats_deagg_pkts(int aggcount);
void rmnet_stats_agg_pkts(int aggcount);
#endif /* _RMNET_DATA_STATS_H_ */
