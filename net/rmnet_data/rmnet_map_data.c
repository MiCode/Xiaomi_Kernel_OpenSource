/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
 * RMNET Data MAP protocol
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rmnet_data.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include "rmnet_data_config.h"
#include "rmnet_map.h"
#include "rmnet_data_private.h"

/* ***************** Local Definitions ************************************** */
struct agg_work {
	struct delayed_work work;
	struct rmnet_phys_ep_conf_s *config;
};

/******************************************************************************/

/**
 * rmnet_map_add_map_header() - Adds MAP header to front of skb->data
 * @skb:        Socket buffer ("packet") to modify
 * @hdrlen:     Number of bytes of header data which should not be included in
 *              MAP length field
 *
 * Padding is calculated and set appropriately in MAP header. Mux ID is
 * initialized to 0.
 *
 * Return:
 *      - Pointer to MAP structure
 *      - 0 (null) if insufficient headroom
 *      - 0 (null) if insufficient tailroom for padding bytes
 *
 * todo: Parameterize skb alignment
 */
struct rmnet_map_header_s *rmnet_map_add_map_header(struct sk_buff *skb,
						    int hdrlen)
{
	uint32_t padding, map_datalen;
	uint8_t *padbytes;
	struct rmnet_map_header_s *map_header;

	if (skb_headroom(skb) < sizeof(struct rmnet_map_header_s))
		return 0;

	map_datalen = skb->len - hdrlen;
	map_header = (struct rmnet_map_header_s *)
			skb_push(skb, sizeof(struct rmnet_map_header_s));
	memset(map_header, 0, sizeof(struct rmnet_map_header_s));

	padding = ALIGN(map_datalen, 4) - map_datalen;

	if (skb_tailroom(skb) < padding)
		return 0;

	padbytes = (uint8_t *) skb_put(skb, padding);
	LOGD("pad: %d\n", padding);
	memset(padbytes, 0, padding);

	map_header->pkt_len = htons(map_datalen + padding);
	map_header->pad_len = padding&0x3F;

	return map_header;
}

/**
 * rmnet_map_deaggregate() - Deaggregates a single packet
 * @skb:        Source socket buffer containing multiple MAP frames
 * @config:     Physical endpoint configuration of the ingress device
 *
 * Source skb is cloned with skb_clone(). The new skb data and tail pointers are
 * modified to contain a single MAP frame. Clone happens with GFP_KERNEL flags
 * set. User should keep calling deaggregate() on the source skb until 0 is
 * returned, indicating that there are no more packets to deaggregate.
 *
 * Return:
 *     - Pointer to new skb
 *     - 0 (null) if no more aggregated packets
 */
struct sk_buff *rmnet_map_deaggregate(struct sk_buff *skb,
				      struct rmnet_phys_ep_conf_s *config)
{
	struct sk_buff *skbn;
	struct rmnet_map_header_s *maph;
	uint32_t packet_len;
	uint8_t ip_byte;

	if (skb->len == 0)
		return 0;

	maph = (struct rmnet_map_header_s *) skb->data;
	packet_len = ntohs(maph->pkt_len) + sizeof(struct rmnet_map_header_s);
	if ((((int)skb->len) - ((int)packet_len)) < 0) {
		LOGM("%s(): Got malformed packet. Dropping\n", __func__);
		return 0;
	}

	skbn = skb_clone(skb, GFP_ATOMIC);
	if (!skbn)
		return 0;

	LOGD("Trimming to %d bytes\n", packet_len);
	LOGD("before skbn->len = %d", skbn->len);
	skb_trim(skbn, packet_len);
	skb_pull(skb, packet_len);
	LOGD("after skbn->len = %d", skbn->len);

	/* Sanity check */
	ip_byte = (skbn->data[4]) & 0xF0;
	if (ip_byte != 0x40 && ip_byte != 0x60) {
		LOGM("%s() Unknown IP type: 0x%02X\n", __func__, ip_byte);
		kfree_skb(skbn);
		return 0;
	}

	return skbn;
}

/**
 * rmnet_map_flush_packet_queue() - Transmits aggregeted frame on timeout
 * @work:        struct agg_work containing delayed work and skb to flush
 *
 * This function is scheduled to run in a specified number of jiffies after
 * the last frame transmitted by the network stack. When run, the buffer
 * containing aggregated packets is finally transmitted on the underlying link.
 *
 */
static void rmnet_map_flush_packet_queue(struct work_struct *work)
{
	struct agg_work *real_work;
	struct rmnet_phys_ep_conf_s *config;
	unsigned long flags;
	struct sk_buff *skb;

	skb = 0;
	real_work = (struct agg_work *)work;
	config = real_work->config;
	LOGD("Entering flush thread\n");
	spin_lock_irqsave(&config->agg_lock, flags);
	if (likely(config->agg_state == RMNET_MAP_TXFER_SCHEDULED)) {
		if (likely(config->agg_skb)) {
			/* Buffer may have already been shipped out */
			if (config->agg_count > 1)
				LOGL("Agg count: %d\n", config->agg_count);
			skb = config->agg_skb;
			config->agg_skb = 0;
		}
		config->agg_state = RMNET_MAP_AGG_IDLE;
	} else {
		/* How did we get here? */
		LOGE("%s(): Ran queued command when state %s\n",
			"is idle. State machine likely broken", __func__);
	}

	spin_unlock_irqrestore(&config->agg_lock, flags);
	if (skb)
		dev_queue_xmit(skb);
	kfree(work);
}

/**
 * rmnet_map_aggregate() - Software aggregates multiple packets.
 * @skb:        current packet being transmitted
 * @config:     Physical endpoint configuration of the ingress device
 *
 * Aggregates multiple SKBs into a single large SKB for transmission. MAP
 * protocol is used to separate the packets in the buffer. This funcion consumes
 * the argument SKB and should not be further processed by any other function.
 */
void rmnet_map_aggregate(struct sk_buff *skb,
			 struct rmnet_phys_ep_conf_s *config) {
	uint8_t *dest_buff;
	struct agg_work *work;
	unsigned long flags;
	struct sk_buff *agg_skb;
	int size;


	if (!skb || !config)
		BUG();
	size = config->egress_agg_size-skb->len;

	if (size < 2000) {
		LOGL("Invalid length %d\n", size);
		return;
	}

new_packet:
	spin_lock_irqsave(&config->agg_lock, flags);
	if (!config->agg_skb) {
		config->agg_skb = skb_copy_expand(skb, 0, size, GFP_ATOMIC);
		if (!config->agg_skb) {
			config->agg_skb = 0;
			config->agg_count = 0;
			spin_unlock_irqrestore(&config->agg_lock, flags);
			dev_queue_xmit(skb);
			return;
		}
		config->agg_count = 1;
		kfree_skb(skb);
		goto schedule;
	}

	if (skb->len > (config->egress_agg_size - config->agg_skb->len)) {
		if (config->agg_count > 1)
			LOGL("Agg count: %d\n", config->agg_count);
		agg_skb = config->agg_skb;
		config->agg_skb = 0;
		config->agg_count = 0;
		spin_unlock_irqrestore(&config->agg_lock, flags);
		dev_queue_xmit(agg_skb);
		goto new_packet;
	}

	dest_buff = skb_put(config->agg_skb, skb->len);
	memcpy(dest_buff, skb->data, skb->len);
	config->agg_count++;
	kfree_skb(skb);

schedule:
	if (config->agg_state != RMNET_MAP_TXFER_SCHEDULED) {
		work = (struct agg_work *)
			kmalloc(sizeof(struct agg_work), GFP_ATOMIC);
		if (!work) {
			LOGE("%s(): Failed to allocate work item for packet %s",
			     "transfer. DATA PATH LIKELY BROKEN!\n", __func__);
			config->agg_state = RMNET_MAP_AGG_IDLE;
			spin_unlock_irqrestore(&config->agg_lock, flags);
			return;
		}
		INIT_DELAYED_WORK((struct delayed_work *)work,
				  rmnet_map_flush_packet_queue);
		work->config = config;
		config->agg_state = RMNET_MAP_TXFER_SCHEDULED;
		schedule_delayed_work((struct delayed_work *)work, 1);
	}
	spin_unlock_irqrestore(&config->agg_lock, flags);
	return;
}

