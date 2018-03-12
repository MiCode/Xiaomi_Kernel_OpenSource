/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/rmnet_data.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/net_map.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <net/ip.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <net/rmnet_config.h>
#include "rmnet_data_config.h"
#include "rmnet_map.h"
#include "rmnet_data_private.h"
#include "rmnet_data_stats.h"
#include "rmnet_data_trace.h"

RMNET_LOG_MODULE(RMNET_DATA_LOGMASK_MAPD);

/* Local Definitions */

long agg_time_limit __read_mostly = 1000000L;
module_param(agg_time_limit, long, 0644);
MODULE_PARM_DESC(agg_time_limit, "Maximum time packets sit in the agg buf");

long agg_bypass_time __read_mostly = 10000000L;
module_param(agg_bypass_time, long, 0644);
MODULE_PARM_DESC(agg_bypass_time, "Skip agg when apart spaced more than this");

struct agg_work {
	struct work_struct work;
	struct rmnet_phys_ep_config *config;
};

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
				      struct rmnet_phys_ep_config *config)
{
	struct sk_buff *skbn;
	struct rmnet_map_header_s *maph;
	u32 packet_len;

	if (skb->len == 0)
		return 0;

	maph = (struct rmnet_map_header_s *)skb->data;
	packet_len = ntohs(maph->pkt_len) + sizeof(struct rmnet_map_header_s);

	if ((config->ingress_data_format & RMNET_INGRESS_FORMAT_MAP_CKSUMV3) ||
	    (config->ingress_data_format & RMNET_INGRESS_FORMAT_MAP_CKSUMV4))
		packet_len += sizeof(struct rmnet_map_dl_checksum_trailer_s);

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

static void rmnet_map_flush_packet_work(struct work_struct *work)
{
	struct rmnet_phys_ep_config *config;
	struct agg_work *real_work;
	int rc, agg_count = 0;
	unsigned long flags;
	struct sk_buff *skb;

	real_work = (struct agg_work *)work;
	config = real_work->config;
	skb = NULL;

	LOGD("%s", "Entering flush thread");
	spin_lock_irqsave(&config->agg_lock, flags);
	if (likely(config->agg_state == RMNET_MAP_TXFER_SCHEDULED)) {
		/* Buffer may have already been shipped out */
		if (likely(config->agg_skb)) {
			rmnet_stats_agg_pkts(config->agg_count);
			if (config->agg_count > 1)
				LOGL("Agg count: %d", config->agg_count);
			skb = config->agg_skb;
			agg_count = config->agg_count;
			config->agg_skb = NULL;
			config->agg_count = 0;
			memset(&config->agg_time, 0, sizeof(struct timespec));
		}
		config->agg_state = RMNET_MAP_AGG_IDLE;
	}

	spin_unlock_irqrestore(&config->agg_lock, flags);
	if (skb) {
		trace_rmnet_map_flush_packet_queue(skb, agg_count);
		rc = dev_queue_xmit(skb);
		rmnet_stats_queue_xmit(rc, RMNET_STATS_QUEUE_XMIT_AGG_TIMEOUT);
	}

	kfree(work);
}

/* rmnet_map_flush_packet_queue() - Transmits aggregeted frame on timeout
 *
 * This function is scheduled to run in a specified number of ns after
 * the last frame transmitted by the network stack. When run, the buffer
 * containing aggregated packets is finally transmitted on the underlying link.
 *
 */
enum hrtimer_restart rmnet_map_flush_packet_queue(struct hrtimer *t)
{
	struct rmnet_phys_ep_config *config;
	struct agg_work *work;

	config = container_of(t, struct rmnet_phys_ep_config, hrtimer);

	work = kmalloc(sizeof(*work), GFP_ATOMIC);
	if (!work) {
		config->agg_state = RMNET_MAP_AGG_IDLE;

		return HRTIMER_NORESTART;
	}

	INIT_WORK(&work->work, rmnet_map_flush_packet_work);
	work->config = config;
	schedule_work((struct work_struct *)work);
	return HRTIMER_NORESTART;
}

/* rmnet_map_aggregate() - Software aggregates multiple packets.
 * @skb:        current packet being transmitted
 * @config:     Physical endpoint configuration of the ingress device
 *
 * Aggregates multiple SKBs into a single large SKB for transmission. MAP
 * protocol is used to separate the packets in the buffer. This function
 * consumes the argument SKB and should not be further processed by any other
 * function.
 */
void rmnet_map_aggregate(struct sk_buff *skb,
			 struct rmnet_phys_ep_config *config) {
	u8 *dest_buff;
	unsigned long flags;
	struct sk_buff *agg_skb;
	struct timespec diff, last;
	int size, rc, agg_count = 0;

	if (!skb || !config)
		return;

new_packet:
	spin_lock_irqsave(&config->agg_lock, flags);
	memcpy(&last, &config->agg_last, sizeof(struct timespec));
	getnstimeofday(&config->agg_last);

	if (!config->agg_skb) {
		/* Check to see if we should agg first. If the traffic is very
		 * sparse, don't aggregate. We will need to tune this later
		 */
		diff = timespec_sub(config->agg_last, last);

		if ((diff.tv_sec > 0) || (diff.tv_nsec > agg_bypass_time)) {
			spin_unlock_irqrestore(&config->agg_lock, flags);
			LOGL("delta t: %ld.%09lu\tcount: bypass", diff.tv_sec,
			     diff.tv_nsec);
			rmnet_stats_agg_pkts(1);
			trace_rmnet_map_aggregate(skb, 0);
			rc = dev_queue_xmit(skb);
			rmnet_stats_queue_xmit(rc,
					       RMNET_STATS_QUEUE_XMIT_AGG_SKIP);
			return;
		}

		size = config->egress_agg_size - skb->len;
		config->agg_skb = skb_copy_expand(skb, 0, size, GFP_ATOMIC);
		if (!config->agg_skb) {
			config->agg_skb = 0;
			config->agg_count = 0;
			memset(&config->agg_time, 0, sizeof(struct timespec));
			spin_unlock_irqrestore(&config->agg_lock, flags);
			rmnet_stats_agg_pkts(1);
			trace_rmnet_map_aggregate(skb, 0);
			rc = dev_queue_xmit(skb);
			rmnet_stats_queue_xmit
				(rc,
				 RMNET_STATS_QUEUE_XMIT_AGG_CPY_EXP_FAIL);
			return;
		}
		config->agg_count = 1;
		getnstimeofday(&config->agg_time);
		trace_rmnet_start_aggregation(skb);
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_AGG_CPY_EXPAND);
		goto schedule;
	}
	diff = timespec_sub(config->agg_last, config->agg_time);

	if (skb->len > (config->egress_agg_size - config->agg_skb->len) ||
	    (config->agg_count >= config->egress_agg_count) ||
	    (diff.tv_sec > 0) || (diff.tv_nsec > agg_time_limit)) {
		rmnet_stats_agg_pkts(config->agg_count);
		agg_skb = config->agg_skb;
		agg_count = config->agg_count;
		config->agg_skb = 0;
		config->agg_count = 0;
		memset(&config->agg_time, 0, sizeof(struct timespec));
		config->agg_state = RMNET_MAP_AGG_IDLE;
		spin_unlock_irqrestore(&config->agg_lock, flags);
		hrtimer_cancel(&config->hrtimer);
		LOGL("delta t: %ld.%09lu\tcount: %d", diff.tv_sec,
		     diff.tv_nsec, agg_count);
		trace_rmnet_map_aggregate(skb, agg_count);
		rc = dev_queue_xmit(agg_skb);
		rmnet_stats_queue_xmit(rc,
				       RMNET_STATS_QUEUE_XMIT_AGG_FILL_BUFFER);
		goto new_packet;
	}

	dest_buff = skb_put(config->agg_skb, skb->len);
	memcpy(dest_buff, skb->data, skb->len);
	config->agg_count++;
	rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_AGG_INTO_BUFF);

schedule:
	if (config->agg_state != RMNET_MAP_TXFER_SCHEDULED) {
		config->agg_state = RMNET_MAP_TXFER_SCHEDULED;
		hrtimer_start(&config->hrtimer, ns_to_ktime(3000000),
			      HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&config->agg_lock, flags);
}

/* Checksum Offload */

static inline u16 *rmnet_map_get_checksum_field(unsigned char protocol,
						const void *txporthdr)
{
	u16 *check = 0;

	switch (protocol) {
	case IPPROTO_TCP:
		check = &(((struct tcphdr *)txporthdr)->check);
		break;

	case IPPROTO_UDP:
		check = &(((struct udphdr *)txporthdr)->check);
		break;

	default:
		check = 0;
		break;
	}

	return check;
}

static inline u16 rmnet_map_add_checksums(u16 val1, u16 val2)
{
	int sum = val1 + val2;

	sum = (((sum & 0xFFFF0000) >> 16) + sum) & 0x0000FFFF;
	return (u16)(sum & 0x0000FFFF);
}

static inline u16 rmnet_map_subtract_checksums(u16 val1, u16 val2)
{
	return rmnet_map_add_checksums(val1, ~val2);
}

/* rmnet_map_validate_ipv4_packet_checksum() - Validates TCP/UDP checksum
 *	value for IPv4 packet
 * @map_payload:	Pointer to the beginning of the map payload
 * @cksum_trailer:	Pointer to the checksum trailer
 *
 * Validates the TCP/UDP checksum for the packet using the checksum value
 * from the checksum trailer added to the packet.
 * The validation formula is the following:
 * 1. Performs 1's complement over the checksum value from the trailer
 * 2. Computes 1's complement checksum over IPv4 header and subtracts it from
 *    the value from step 1
 * 3. Computes 1's complement checksum over IPv4 pseudo header and adds it to
 *    the value from step 2
 * 4. Subtracts the checksum value from the TCP/UDP header from the value from
 *    step 3
 * 5. Compares the value from step 4 to the checksum value from the TCP/UDP
 *    header
 *
 * Fragmentation and tunneling are not supported.
 *
 * Return: 0 is validation succeeded.
 */
static int rmnet_map_validate_ipv4_packet_checksum
	(unsigned char *map_payload,
	 struct rmnet_map_dl_checksum_trailer_s *cksum_trailer)
{
	struct iphdr *ip4h;
	u16 *checksum_field;
	void *txporthdr;
	u16 pseudo_checksum;
	u16 ip_hdr_checksum;
	u16 checksum_value;
	u16 ip_payload_checksum;
	u16 ip_pseudo_payload_checksum;
	u16 checksum_value_final;

	ip4h = (struct iphdr *)map_payload;
	if ((ntohs(ip4h->frag_off) & IP_MF) ||
	    ((ntohs(ip4h->frag_off) & IP_OFFSET) > 0))
		return RMNET_MAP_CHECKSUM_FRAGMENTED_PACKET;

	txporthdr = map_payload + ip4h->ihl * 4;

	checksum_field = rmnet_map_get_checksum_field(ip4h->protocol,
						      txporthdr);

	if (unlikely(!checksum_field))
		return RMNET_MAP_CHECKSUM_ERR_UNKNOWN_TRANSPORT;

	/* RFC 768 - Skip IPv4 UDP packets where sender checksum field is 0 */
	if ((*checksum_field == 0) && (ip4h->protocol == IPPROTO_UDP))
		return RMNET_MAP_CHECKSUM_SKIPPED;

	checksum_value = ~ntohs(cksum_trailer->checksum_value);
	ip_hdr_checksum = ~ip_fast_csum(ip4h, (int)ip4h->ihl);
	ip_payload_checksum = rmnet_map_subtract_checksums(checksum_value,
							   ip_hdr_checksum);

	pseudo_checksum = ~ntohs(csum_tcpudp_magic(ip4h->saddr, ip4h->daddr,
		(u16)(ntohs(ip4h->tot_len) - ip4h->ihl * 4),
		(u16)ip4h->protocol, 0));
	ip_pseudo_payload_checksum = rmnet_map_add_checksums(
		ip_payload_checksum, pseudo_checksum);

	checksum_value_final = ~rmnet_map_subtract_checksums(
		ip_pseudo_payload_checksum, ntohs(*checksum_field));

	if (unlikely(checksum_value_final == 0)) {
		switch (ip4h->protocol) {
		case IPPROTO_UDP:
			/* RFC 768 */
			LOGD("DL4 1's complement rule for UDP checksum 0");
			checksum_value_final = ~checksum_value_final;
			break;

		case IPPROTO_TCP:
			if (*checksum_field == 0xFFFF) {
				LOGD(
				"DL4 Non-RFC compliant TCP checksum found");
				checksum_value_final = ~checksum_value_final;
			}
			break;
		}
	}

	LOGD(
	"DL4 cksum: ~HW: %04X, field: %04X, pseudo header: %04X, final: %04X",
	~ntohs(cksum_trailer->checksum_value), ntohs(*checksum_field),
	pseudo_checksum, checksum_value_final);

	if (checksum_value_final == ntohs(*checksum_field))
		return RMNET_MAP_CHECKSUM_OK;
	else
		return RMNET_MAP_CHECKSUM_VALIDATION_FAILED;
}

/* rmnet_map_validate_ipv6_packet_checksum() - Validates TCP/UDP checksum
 *	value for IPv6 packet
 * @map_payload:	Pointer to the beginning of the map payload
 * @cksum_trailer:	Pointer to the checksum trailer
 *
 * Validates the TCP/UDP checksum for the packet using the checksum value
 * from the checksum trailer added to the packet.
 * The validation formula is the following:
 * 1. Performs 1's complement over the checksum value from the trailer
 * 2. Computes 1's complement checksum over IPv6 header and subtracts it from
 *    the value from step 1
 * 3. Computes 1's complement checksum over IPv6 pseudo header and adds it to
 *    the value from step 2
 * 4. Subtracts the checksum value from the TCP/UDP header from the value from
 *    step 3
 * 5. Compares the value from step 4 to the checksum value from the TCP/UDP
 *    header
 *
 * Fragmentation, extension headers and tunneling are not supported.
 *
 * Return: 0 is validation succeeded.
 */
static int rmnet_map_validate_ipv6_packet_checksum
	(unsigned char *map_payload,
	 struct rmnet_map_dl_checksum_trailer_s *cksum_trailer)
{
	struct ipv6hdr *ip6h;
	u16 *checksum_field;
	void *txporthdr;
	u16 pseudo_checksum;
	u16 ip_hdr_checksum;
	u16 checksum_value;
	u16 ip_payload_checksum;
	u16 ip_pseudo_payload_checksum;
	u16 checksum_value_final;
	u32 length;

	ip6h = (struct ipv6hdr *)map_payload;

	txporthdr = map_payload + sizeof(struct ipv6hdr);
	checksum_field = rmnet_map_get_checksum_field(ip6h->nexthdr,
						      txporthdr);

	if (unlikely(!checksum_field))
		return RMNET_MAP_CHECKSUM_ERR_UNKNOWN_TRANSPORT;

	checksum_value = ~ntohs(cksum_trailer->checksum_value);
	ip_hdr_checksum = ~ntohs(ip_compute_csum(ip6h,
				 (int)(txporthdr - (void *)map_payload)));
	ip_payload_checksum = rmnet_map_subtract_checksums
				(checksum_value, ip_hdr_checksum);

	length = (ip6h->nexthdr == IPPROTO_UDP) ?
		ntohs(((struct udphdr *)txporthdr)->len) :
		ntohs(ip6h->payload_len);
	pseudo_checksum = ~ntohs(csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
		length, ip6h->nexthdr, 0));
	ip_pseudo_payload_checksum = rmnet_map_add_checksums(
		ip_payload_checksum, pseudo_checksum);

	checksum_value_final = ~rmnet_map_subtract_checksums(
		ip_pseudo_payload_checksum, ntohs(*checksum_field));

	if (unlikely(checksum_value_final == 0)) {
		switch (ip6h->nexthdr) {
		case IPPROTO_UDP:
			/* RFC 2460 section 8.1 */
			LOGD("DL6 One's complement rule for UDP checksum 0");
			checksum_value_final = ~checksum_value_final;
			break;

		case IPPROTO_TCP:
			if (*checksum_field == 0xFFFF) {
				LOGD(
				"DL6 Non-RFC compliant TCP checksum found");
				checksum_value_final = ~checksum_value_final;
			}
			break;
		}
	}

	LOGD(
	"DL6 cksum: ~HW: %04X, field: %04X, pseudo header: %04X, final: %04X",
	~ntohs(cksum_trailer->checksum_value), ntohs(*checksum_field),
	pseudo_checksum, checksum_value_final);

	if (checksum_value_final == ntohs(*checksum_field))
		return RMNET_MAP_CHECKSUM_OK;
	else
		return RMNET_MAP_CHECKSUM_VALIDATION_FAILED;
	}

/* rmnet_map_checksum_downlink_packet() - Validates checksum on
 * a downlink packet
 * @skb:	Pointer to the packet's skb.
 *
 * Validates packet checksums. Function takes a pointer to
 * the beginning of a buffer which contains the entire MAP
 * frame: MAP header + IP payload + padding + checksum trailer.
 * Currently, only IPv4 and IPv6 are supported along with
 * TCP & UDP. Fragmented or tunneled packets are not supported.
 *
 * Return:
 *   - RMNET_MAP_CHECKSUM_OK: Validation of checksum succeeded.
 *   - RMNET_MAP_CHECKSUM_ERR_BAD_BUFFER: Skb buffer given is corrupted.
 *   - RMNET_MAP_CHECKSUM_VALID_FLAG_NOT_SET: Valid flag is not set in the
 *					      checksum trailer.
 *   - RMNET_MAP_CHECKSUM_FRAGMENTED_PACKET: The packet is a fragment.
 *   - RMNET_MAP_CHECKSUM_ERR_UNKNOWN_TRANSPORT: The transport header is
 *						   not TCP/UDP.
 *   - RMNET_MAP_CHECKSUM_ERR_UNKNOWN_IP_VERSION: Unrecognized IP header.
 *   - RMNET_MAP_CHECKSUM_VALIDATION_FAILED: In case the validation failed.
 */
int rmnet_map_checksum_downlink_packet(struct sk_buff *skb)
{
	struct rmnet_map_dl_checksum_trailer_s *cksum_trailer;
	unsigned int data_len;
	unsigned char *map_payload;
	unsigned char ip_version;

	data_len = RMNET_MAP_GET_LENGTH(skb);

	if (unlikely(skb->len < (sizeof(struct rmnet_map_header_s) + data_len +
	    sizeof(struct rmnet_map_dl_checksum_trailer_s))))
		return RMNET_MAP_CHECKSUM_ERR_BAD_BUFFER;

	cksum_trailer = (struct rmnet_map_dl_checksum_trailer_s *)
			(skb->data + data_len
			+ sizeof(struct rmnet_map_header_s));

	if (unlikely(!ntohs(cksum_trailer->valid)))
		return RMNET_MAP_CHECKSUM_VALID_FLAG_NOT_SET;

	map_payload = (unsigned char *)(skb->data
		+ sizeof(struct rmnet_map_header_s));

	ip_version = (*map_payload & 0xF0) >> 4;
	if (ip_version == 0x04)
		return rmnet_map_validate_ipv4_packet_checksum(map_payload,
			cksum_trailer);
	else if (ip_version == 0x06)
		return rmnet_map_validate_ipv6_packet_checksum(map_payload,
			cksum_trailer);

	return RMNET_MAP_CHECKSUM_ERR_UNKNOWN_IP_VERSION;
}

static void rmnet_map_fill_ipv4_packet_ul_checksum_header
	(void *iphdr, struct rmnet_map_ul_checksum_header_s *ul_header,
	 struct sk_buff *skb)
{
	struct iphdr *ip4h = (struct iphdr *)iphdr;
	unsigned short *hdr = (unsigned short *)ul_header;

	ul_header->checksum_start_offset = htons((unsigned short)
		(skb_transport_header(skb) - (unsigned char *)iphdr));
	ul_header->checksum_insert_offset = skb->csum_offset;
	ul_header->cks_en = 1;
	if (ip4h->protocol == IPPROTO_UDP)
		ul_header->udp_ip4_ind = 1;
	else
		ul_header->udp_ip4_ind = 0;
	/* Changing checksum_insert_offset to network order */
	hdr++;
	*hdr = htons(*hdr);
	skb->ip_summed = CHECKSUM_NONE;
}

static void rmnet_map_fill_ipv6_packet_ul_checksum_header
	(void *iphdr, struct rmnet_map_ul_checksum_header_s *ul_header,
	 struct sk_buff *skb)
{
	unsigned short *hdr = (unsigned short *)ul_header;

	ul_header->checksum_start_offset = htons((unsigned short)
		(skb_transport_header(skb) - (unsigned char *)iphdr));
	ul_header->checksum_insert_offset = skb->csum_offset;
	ul_header->cks_en = 1;
	ul_header->udp_ip4_ind = 0;
	/* Changing checksum_insert_offset to network order */
	hdr++;
	*hdr = htons(*hdr);
	skb->ip_summed = CHECKSUM_NONE;
}

static void rmnet_map_complement_ipv4_txporthdr_csum_field(void *iphdr)
{
	struct iphdr *ip4h = (struct iphdr *)iphdr;
	void *txporthdr;
	u16 *csum;

	txporthdr = iphdr + ip4h->ihl * 4;

	if ((ip4h->protocol == IPPROTO_TCP) ||
	    (ip4h->protocol == IPPROTO_UDP)) {
		csum = (u16 *)rmnet_map_get_checksum_field(ip4h->protocol,
								txporthdr);
		*csum = ~(*csum);
	}
}

static void rmnet_map_complement_ipv6_txporthdr_csum_field(void *ip6hdr)
{
	struct ipv6hdr *ip6h = (struct ipv6hdr *)ip6hdr;
	void *txporthdr;
	u16 *csum;

	txporthdr = ip6hdr + sizeof(struct ipv6hdr);

	if ((ip6h->nexthdr == IPPROTO_TCP) || (ip6h->nexthdr == IPPROTO_UDP)) {
		csum = (u16 *)rmnet_map_get_checksum_field(ip6h->nexthdr,
								txporthdr);
		*csum = ~(*csum);
	}
}

/* rmnet_map_checksum_uplink_packet() - Generates UL checksum
 * meta info header
 * @skb:	Pointer to the packet's skb.
 *
 * Generates UL checksum meta info header for IPv4 and IPv6  over TCP and UDP
 * packets that are supported for UL checksum offload.
 *
 * Return:
 *   - RMNET_MAP_CHECKSUM_OK: Validation of checksum succeeded.
 *   - RMNET_MAP_CHECKSUM_ERR_UNKNOWN_IP_VERSION: Unrecognized IP header.
 *   - RMNET_MAP_CHECKSUM_SW: Unsupported packet for UL checksum offload.
 */
int rmnet_map_checksum_uplink_packet(struct sk_buff *skb,
				     struct net_device *orig_dev,
				     u32 egress_data_format)
{
	unsigned char ip_version;
	struct rmnet_map_ul_checksum_header_s *ul_header;
	void *iphdr;
	int ret;

	ul_header = (struct rmnet_map_ul_checksum_header_s *)
		skb_push(skb, sizeof(struct rmnet_map_ul_checksum_header_s));

	if (unlikely(!(orig_dev->features &
		(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM)))) {
		ret = RMNET_MAP_CHECKSUM_SW;
		goto sw_checksum;
	}

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		iphdr = (char *)ul_header +
			sizeof(struct rmnet_map_ul_checksum_header_s);
		ip_version = (*(char *)iphdr & 0xF0) >> 4;
		if (ip_version == 0x04) {
			rmnet_map_fill_ipv4_packet_ul_checksum_header
				(iphdr, ul_header, skb);
			if (egress_data_format &
			    RMNET_EGRESS_FORMAT_MAP_CKSUMV4)
				rmnet_map_complement_ipv4_txporthdr_csum_field(
					iphdr);
			ret = RMNET_MAP_CHECKSUM_OK;
			goto done;
		} else if (ip_version == 0x06) {
			rmnet_map_fill_ipv6_packet_ul_checksum_header
				(iphdr, ul_header, skb);
			if (egress_data_format &
			    RMNET_EGRESS_FORMAT_MAP_CKSUMV4)
				rmnet_map_complement_ipv6_txporthdr_csum_field(
					iphdr);
			ret =  RMNET_MAP_CHECKSUM_OK;
			goto done;
		} else {
			ret = RMNET_MAP_CHECKSUM_ERR_UNKNOWN_IP_VERSION;
			goto sw_checksum;
		}
	} else {
		ret = RMNET_MAP_CHECKSUM_SW;
		goto sw_checksum;
	}

sw_checksum:
	ul_header->checksum_start_offset = 0;
	ul_header->checksum_insert_offset = 0;
	ul_header->cks_en = 0;
	ul_header->udp_ip4_ind = 0;
done:
	return ret;
}

int rmnet_ul_aggregation_skip(struct sk_buff *skb, int offset)
{
	unsigned char *packet_start = skb->data + offset;
	int is_icmp = 0;

	if ((skb->data[offset]) >> 4 == 0x04) {
		struct iphdr *ip4h = (struct iphdr *)(packet_start);

		if (ip4h->protocol == IPPROTO_ICMP)
			is_icmp = 1;
	} else if ((skb->data[offset]) >> 4 == 0x06) {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)(packet_start);

		if (ip6h->nexthdr == IPPROTO_ICMPV6) {
			is_icmp = 1;
		} else if (ip6h->nexthdr == NEXTHDR_FRAGMENT) {
			struct frag_hdr *frag;

			frag = (struct frag_hdr *)(packet_start
						   + sizeof(struct ipv6hdr));
			if (frag->nexthdr == IPPROTO_ICMPV6)
				is_icmp = 1;
		}
	}

	return is_icmp;
}
