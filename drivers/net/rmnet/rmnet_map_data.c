/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include <linux/rmnet.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <net/ip.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include "rmnet_config.h"
#include "rmnet_map.h"
#include "rmnet_private.h"
#include "rmnet_stats.h"

RMNET_LOG_MODULE(RMNET_LOGMASK_MAPD);

#define RMNET_MAP_DEAGGR_SPACING  64
#define RMNET_MAP_DEAGGR_HEADROOM (RMNET_MAP_DEAGGR_SPACING / 2)

/* rmnet_map_add_map_header() - Adds MAP header to front of skb->data
 * @skb:        Socket buffer ("packet") to modify
 * @hdrlen:     Number of bytes of header data which should not be included in
 *              MAP length field
 * @pad:        Specify if padding the MAP packet to make it 4 byte aligned is
 *              necessary
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
						    int hdrlen, int pad)
{
	u32 padding, map_datalen;
	u8 *padbytes;
	struct rmnet_map_header_s *map_header;

	if (skb_headroom(skb) < sizeof(struct rmnet_map_header_s))
		return 0;

	map_datalen = skb->len - hdrlen;
	map_header = (struct rmnet_map_header_s *)
			skb_push(skb, sizeof(struct rmnet_map_header_s));
	memset(map_header, 0, sizeof(struct rmnet_map_header_s));

	if (pad == RMNET_MAP_NO_PAD_BYTES) {
		map_header->pkt_len = htons(map_datalen);
		return map_header;
	}

	padding = ALIGN(map_datalen, 4) - map_datalen;

	if (padding == 0)
		goto done;

	if (skb_tailroom(skb) < padding)
		return 0;

	padbytes = (u8 *)skb_put(skb, padding);
	LOGD("pad: %d", padding);
	memset(padbytes, 0, padding);

done:
	map_header->pkt_len = htons(map_datalen + padding);
	map_header->pad_len = padding & 0x3F;

	return map_header;
}

/* rmnet_map_deaggregate() - Deaggregates a single packet
 * @skb:        Source socket buffer containing multiple MAP frames
 * @config:     Physical endpoint configuration of the ingress device
 *
 * A whole new buffer is allocated for each portion of an aggregated frame.
 * Caller should keep calling deaggregate() on the source skb until 0 is
 * returned, indicating that there are no more packets to deaggregate. Caller
 * is responsible for freeing the original skb.
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
	u32 packet_len;

	if (skb->len == 0)
		return 0;

	maph = (struct rmnet_map_header_s *)skb->data;
	packet_len = ntohs(maph->pkt_len) + sizeof(struct rmnet_map_header_s);

	if ((((int)skb->len) - ((int)packet_len)) < 0) {
		LOGM("%s", "Got malformed packet. Dropping");
		return 0;
	}

	skbn = alloc_skb(packet_len + RMNET_MAP_DEAGGR_SPACING, GFP_ATOMIC);
	if (!skbn)
		return 0;

	skbn->dev = skb->dev;
	skb_reserve(skbn, RMNET_MAP_DEAGGR_HEADROOM);
	skb_put(skbn, packet_len);
	memcpy(skbn->data, skb->data, packet_len);
	skb_pull(skb, packet_len);

	/* Some hardware can send us empty frames. Catch them */
	if (ntohs(maph->pkt_len) == 0) {
		LOGD("Dropping empty MAP frame");
		rmnet_kfree_skb(skbn, RMNET_STATS_SKBFREE_DEAGG_DATA_LEN_0);
		return 0;
	}

	return skbn;
}
