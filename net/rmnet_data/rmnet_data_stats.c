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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include "rmnet_data_private.h"
#include "rmnet_data_stats.h"

enum rmnet_deagg_e {
	RMNET_STATS_AGG_BUFF,
	RMNET_STATS_AGG_PKT,
	RMNET_STATS_AGG_MAX
};

static DEFINE_SPINLOCK(rmnet_skb_free_lock);
unsigned long int skb_free[RMNET_STATS_SKBFREE_MAX];
module_param_array(skb_free, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(skb_free, "SKBs dropped or freed");

static DEFINE_SPINLOCK(rmnet_queue_xmit_lock);
unsigned long int queue_xmit[RMNET_STATS_QUEUE_XMIT_MAX*2];
module_param_array(queue_xmit, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(queue_xmit, "SKBs queued");

static DEFINE_SPINLOCK(rmnet_deagg_count);
unsigned long int deagg_count[RMNET_STATS_AGG_MAX];
module_param_array(deagg_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(deagg_count, "SKBs queued");

static DEFINE_SPINLOCK(rmnet_agg_count);
unsigned long int agg_count[RMNET_STATS_AGG_MAX];
module_param_array(agg_count, ulong, 0, S_IRUGO);
MODULE_PARM_DESC(agg_count, "SKBs queued");

void rmnet_kfree_skb(struct sk_buff *skb, unsigned int reason)
{
	unsigned long flags;

	if (reason >= RMNET_STATS_SKBFREE_MAX)
		reason = RMNET_STATS_SKBFREE_UNKNOWN;

	spin_lock_irqsave(&rmnet_skb_free_lock, flags);
	skb_free[reason]++;
	spin_unlock_irqrestore(&rmnet_skb_free_lock, flags);

	if (skb)
		kfree_skb(skb);
}

void rmnet_stats_queue_xmit(int rc, unsigned int reason)
{
	unsigned long flags;

	if (rc != 0)
		reason += RMNET_STATS_QUEUE_XMIT_MAX;
	if (reason >= RMNET_STATS_QUEUE_XMIT_MAX*2)
		reason = RMNET_STATS_SKBFREE_UNKNOWN;

	spin_lock_irqsave(&rmnet_queue_xmit_lock, flags);
	queue_xmit[reason]++;
	spin_unlock_irqrestore(&rmnet_queue_xmit_lock, flags);
}

void rmnet_stats_agg_pkts(int aggcount)
{
	unsigned long flags;

	spin_lock_irqsave(&rmnet_agg_count, flags);
	agg_count[RMNET_STATS_AGG_BUFF]++;
	agg_count[RMNET_STATS_AGG_PKT] += aggcount;
	spin_unlock_irqrestore(&rmnet_agg_count, flags);
}

void rmnet_stats_deagg_pkts(int aggcount)
{
	unsigned long flags;

	spin_lock_irqsave(&rmnet_deagg_count, flags);
	deagg_count[RMNET_STATS_AGG_BUFF]++;
	deagg_count[RMNET_STATS_AGG_PKT] += aggcount;
	spin_unlock_irqrestore(&rmnet_deagg_count, flags);
}

