/* Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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

#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ip6_checksum.h>
#include "rmnet_config.h"
#include "rmnet_map.h"
#include "rmnet_private.h"
#include "rmnet_handlers.h"

#define RMNET_MAP_PKT_COPY_THRESHOLD 64
#define RMNET_MAP_DEAGGR_SPACING  64
#define RMNET_MAP_DEAGGR_HEADROOM (RMNET_MAP_DEAGGR_SPACING / 2)

static __sum16 *rmnet_map_get_csum_field(unsigned char protocol,
					 const void *txporthdr)
{
	__sum16 *check = NULL;

	switch (protocol) {
	case IPPROTO_TCP:
		check = &(((struct tcphdr *)txporthdr)->check);
		break;

	case IPPROTO_UDP:
		check = &(((struct udphdr *)txporthdr)->check);
		break;

	default:
		check = NULL;
		break;
	}

	return check;
}

static int
rmnet_map_ipv4_dl_csum_trailer(struct sk_buff *skb,
			       struct rmnet_map_dl_csum_trailer *csum_trailer,
			       struct rmnet_priv *priv)
{
	__sum16 *csum_field, csum_temp, pseudo_csum, hdr_csum, ip_payload_csum;
	u16 csum_value, csum_value_final;
	struct iphdr *ip4h;
	void *txporthdr;
	__be16 addend;

	ip4h = (struct iphdr *)rmnet_map_data_ptr(skb);
	if ((ntohs(ip4h->frag_off) & IP_MF) ||
	    ((ntohs(ip4h->frag_off) & IP_OFFSET) > 0)) {
		priv->stats.csum_fragmented_pkt++;
		return -EOPNOTSUPP;
	}

	txporthdr = rmnet_map_data_ptr(skb) + ip4h->ihl * 4;

	csum_field = rmnet_map_get_csum_field(ip4h->protocol, txporthdr);

	if (!csum_field) {
		priv->stats.csum_err_invalid_transport++;
		return -EPROTONOSUPPORT;
	}

	/* RFC 768 - Skip IPv4 UDP packets where sender checksum field is 0 */
	if (*csum_field == 0 && ip4h->protocol == IPPROTO_UDP) {
		priv->stats.csum_skipped++;
		return 0;
	}

	csum_value = ~ntohs(csum_trailer->csum_value);
	hdr_csum = ~ip_fast_csum(ip4h, (int)ip4h->ihl);
	ip_payload_csum = csum16_sub((__force __sum16)csum_value,
				     (__force __be16)hdr_csum);

	pseudo_csum = ~csum_tcpudp_magic(ip4h->saddr, ip4h->daddr,
					 ntohs(ip4h->tot_len) - ip4h->ihl * 4,
					 ip4h->protocol, 0);
	addend = (__force __be16)ntohs((__force __be16)pseudo_csum);
	pseudo_csum = csum16_add(ip_payload_csum, addend);

	addend = (__force __be16)ntohs((__force __be16)*csum_field);
	csum_temp = ~csum16_sub(pseudo_csum, addend);
	csum_value_final = (__force u16)csum_temp;

	if (unlikely(csum_value_final == 0)) {
		switch (ip4h->protocol) {
		case IPPROTO_UDP:
			/* RFC 768 - DL4 1's complement rule for UDP csum 0 */
			csum_value_final = ~csum_value_final;
			break;

		case IPPROTO_TCP:
			/* DL4 Non-RFC compliant TCP checksum found */
			if (*csum_field == (__force __sum16)0xFFFF)
				csum_value_final = ~csum_value_final;
			break;
		}
	}

	if (csum_value_final == ntohs((__force __be16)*csum_field)) {
		priv->stats.csum_ok++;
		return 0;
	} else {
		priv->stats.csum_validation_failed++;
		return -EINVAL;
	}
}

#if IS_ENABLED(CONFIG_IPV6)
static int
rmnet_map_ipv6_dl_csum_trailer(struct sk_buff *skb,
			       struct rmnet_map_dl_csum_trailer *csum_trailer,
			       struct rmnet_priv *priv)
{
	__sum16 *csum_field, ip6_payload_csum, pseudo_csum, csum_temp;
	u16 csum_value, csum_value_final;
	__be16 ip6_hdr_csum, addend;
	struct ipv6hdr *ip6h;
	void *txporthdr, *data = rmnet_map_data_ptr(skb);
	u32 length;

	ip6h = data;

	txporthdr = data + sizeof(struct ipv6hdr);
	csum_field = rmnet_map_get_csum_field(ip6h->nexthdr, txporthdr);

	if (!csum_field) {
		priv->stats.csum_err_invalid_transport++;
		return -EPROTONOSUPPORT;
	}

	csum_value = ~ntohs(csum_trailer->csum_value);
	ip6_hdr_csum = (__force __be16)
			~ntohs((__force __be16)ip_compute_csum(ip6h,
			       (int)(txporthdr - data)));
	ip6_payload_csum = csum16_sub((__force __sum16)csum_value,
				      ip6_hdr_csum);

	length = (ip6h->nexthdr == IPPROTO_UDP) ?
		 ntohs(((struct udphdr *)txporthdr)->len) :
		 ntohs(ip6h->payload_len);
	pseudo_csum = ~(csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
			     length, ip6h->nexthdr, 0));
	addend = (__force __be16)ntohs((__force __be16)pseudo_csum);
	pseudo_csum = csum16_add(ip6_payload_csum, addend);

	addend = (__force __be16)ntohs((__force __be16)*csum_field);
	csum_temp = ~csum16_sub(pseudo_csum, addend);
	csum_value_final = (__force u16)csum_temp;

	if (unlikely(csum_value_final == 0)) {
		switch (ip6h->nexthdr) {
		case IPPROTO_UDP:
			/* RFC 2460 section 8.1
			 * DL6 One's complement rule for UDP checksum 0
			 */
			csum_value_final = ~csum_value_final;
			break;

		case IPPROTO_TCP:
			/* DL6 Non-RFC compliant TCP checksum found */
			if (*csum_field == (__force __sum16)0xFFFF)
				csum_value_final = ~csum_value_final;
			break;
		}
	}

	if (csum_value_final == ntohs((__force __be16)*csum_field)) {
		priv->stats.csum_ok++;
		return 0;
	} else {
		priv->stats.csum_validation_failed++;
		return -EINVAL;
	}
}
#endif

static void rmnet_map_complement_ipv4_txporthdr_csum_field(void *iphdr)
{
	struct iphdr *ip4h = (struct iphdr *)iphdr;
	void *txphdr;
	u16 *csum;

	txphdr = iphdr + ip4h->ihl * 4;

	if (ip4h->protocol == IPPROTO_TCP || ip4h->protocol == IPPROTO_UDP) {
		csum = (u16 *)rmnet_map_get_csum_field(ip4h->protocol, txphdr);
		*csum = ~(*csum);
	}
}

static void
rmnet_map_ipv4_ul_csum_header(void *iphdr,
			      struct rmnet_map_ul_csum_header *ul_header,
			      struct sk_buff *skb)
{
	struct iphdr *ip4h = (struct iphdr *)iphdr;
	__be16 *hdr = (__be16 *)ul_header, offset;

	offset = htons((__force u16)(skb_transport_header(skb) -
				     (unsigned char *)iphdr));
	ul_header->csum_start_offset = offset;
	ul_header->csum_insert_offset = skb->csum_offset;
	ul_header->csum_enabled = 1;
	if (ip4h->protocol == IPPROTO_UDP)
		ul_header->udp_ip4_ind = 1;
	else
		ul_header->udp_ip4_ind = 0;

	/* Changing remaining fields to network order */
	hdr++;
	*hdr = htons((__force u16)*hdr);

	skb->ip_summed = CHECKSUM_NONE;

	rmnet_map_complement_ipv4_txporthdr_csum_field(iphdr);
}

#if IS_ENABLED(CONFIG_IPV6)
static void rmnet_map_complement_ipv6_txporthdr_csum_field(void *ip6hdr)
{
	struct ipv6hdr *ip6h = (struct ipv6hdr *)ip6hdr;
	void *txphdr;
	u16 *csum;

	txphdr = ip6hdr + sizeof(struct ipv6hdr);

	if (ip6h->nexthdr == IPPROTO_TCP || ip6h->nexthdr == IPPROTO_UDP) {
		csum = (u16 *)rmnet_map_get_csum_field(ip6h->nexthdr, txphdr);
		*csum = ~(*csum);
	}
}

static void
rmnet_map_ipv6_ul_csum_header(void *ip6hdr,
			      struct rmnet_map_ul_csum_header *ul_header,
			      struct sk_buff *skb)
{
	__be16 *hdr = (__be16 *)ul_header, offset;

	offset = htons((__force u16)(skb_transport_header(skb) -
				     (unsigned char *)ip6hdr));
	ul_header->csum_start_offset = offset;
	ul_header->csum_insert_offset = skb->csum_offset;
	ul_header->csum_enabled = 1;
	ul_header->udp_ip4_ind = 0;

	/* Changing remaining fields to network order */
	hdr++;
	*hdr = htons((__force u16)*hdr);

	skb->ip_summed = CHECKSUM_NONE;

	rmnet_map_complement_ipv6_txporthdr_csum_field(ip6hdr);
}
#endif

/* Adds MAP header to front of skb->data
 * Padding is calculated and set appropriately in MAP header. Mux ID is
 * initialized to 0.
 */
struct rmnet_map_header *rmnet_map_add_map_header(struct sk_buff *skb,
						  int hdrlen, int pad)
{
	struct rmnet_map_header *map_header;
	struct rmnet_port *port = rmnet_get_port(skb->dev);
	u32 padding, map_datalen;
	u8 *padbytes;

	map_datalen = skb->len - hdrlen;
	map_header = (struct rmnet_map_header *)
			skb_push(skb, sizeof(struct rmnet_map_header));
	memset(map_header, 0, sizeof(struct rmnet_map_header));

	/* Set next_hdr bit for csum offload packets */
	if (port->data_format & RMNET_FLAGS_EGRESS_MAP_CKSUMV5)
		map_header->next_hdr = 1;

	if (pad == RMNET_MAP_NO_PAD_BYTES) {
		map_header->pkt_len = htons(map_datalen);
		return map_header;
	}

	padding = ALIGN(map_datalen, 4) - map_datalen;

	if (padding == 0)
		goto done;

	if (skb_tailroom(skb) < padding)
		return NULL;

	padbytes = (u8 *)skb_put(skb, padding);
	memset(padbytes, 0, padding);

done:
	map_header->pkt_len = htons(map_datalen + padding);
	map_header->pad_len = padding & 0x3F;

	return map_header;
}

/* Deaggregates a single packet
 * A whole new buffer is allocated for each portion of an aggregated frame.
 * Caller should keep calling deaggregate() on the source skb until 0 is
 * returned, indicating that there are no more packets to deaggregate. Caller
 * is responsible for freeing the original skb.
 */
struct sk_buff *rmnet_map_deaggregate(struct sk_buff *skb,
				      struct rmnet_port *port)
{
	struct rmnet_map_header *maph;
	struct sk_buff *skbn;
	unsigned char *data = rmnet_map_data_ptr(skb);
	u32 packet_len;

	if (skb->len == 0)
		return NULL;

	maph = (struct rmnet_map_header *)data;
	packet_len = ntohs(maph->pkt_len) + sizeof(struct rmnet_map_header);

	if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4)
		packet_len += sizeof(struct rmnet_map_dl_csum_trailer);
	else if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV5) {
		if (!maph->cd_bit)
			packet_len += sizeof(struct rmnet_map_v5_csum_header);
	}

	if (((int)skb->len - (int)packet_len) < 0)
		return NULL;

	/* Some hardware can send us empty frames. Catch them */
	if (ntohs(maph->pkt_len) == 0)
		return NULL;

	if (skb_is_nonlinear(skb)) {
		skb_frag_t *frag0 = skb_shinfo(skb)->frags;
		struct page *page = skb_frag_page(frag0);

		skbn = alloc_skb(RMNET_MAP_DEAGGR_HEADROOM, GFP_ATOMIC);
		if (!skbn)
			return NULL;

		skb_append_pagefrags(skbn, page, frag0->page_offset,
				     packet_len);
		skbn->data_len += packet_len;
		skbn->len += packet_len;
	} else {
		skbn = alloc_skb(packet_len + RMNET_MAP_DEAGGR_SPACING,
				 GFP_ATOMIC);
		if (!skbn)
			return NULL;

		skb_reserve(skbn, RMNET_MAP_DEAGGR_HEADROOM);
		skb_put(skbn, packet_len);
		memcpy(skbn->data, data, packet_len);
	}

	pskb_pull(skb, packet_len);

	return skbn;
}

/* Validates packet checksums. Function takes a pointer to
 * the beginning of a buffer which contains the IP payload +
 * padding + checksum trailer.
 * Only IPv4 and IPv6 are supported along with TCP & UDP.
 * Fragmented or tunneled packets are not supported.
 */
int rmnet_map_checksum_downlink_packet(struct sk_buff *skb, u16 len)
{
	struct rmnet_priv *priv = netdev_priv(skb->dev);
	struct rmnet_map_dl_csum_trailer *csum_trailer;

	if (unlikely(!(skb->dev->features & NETIF_F_RXCSUM))) {
		priv->stats.csum_sw++;
		return -EOPNOTSUPP;
	}

	csum_trailer = (struct rmnet_map_dl_csum_trailer *)
		       (rmnet_map_data_ptr(skb) + len);

	if (!csum_trailer->valid) {
		priv->stats.csum_valid_unset++;
		return -EINVAL;
	}

	if (skb->protocol == htons(ETH_P_IP)) {
		return rmnet_map_ipv4_dl_csum_trailer(skb, csum_trailer, priv);
	} else if (skb->protocol == htons(ETH_P_IPV6)) {
#if IS_ENABLED(CONFIG_IPV6)
		return rmnet_map_ipv6_dl_csum_trailer(skb, csum_trailer, priv);
#else
		priv->stats.csum_err_invalid_ip_version++;
		return -EPROTONOSUPPORT;
#endif
	} else {
		priv->stats.csum_err_invalid_ip_version++;
		return -EPROTONOSUPPORT;
	}

	return 0;
}
EXPORT_SYMBOL(rmnet_map_checksum_downlink_packet);

void rmnet_map_v4_checksum_uplink_packet(struct sk_buff *skb,
					 struct net_device *orig_dev)
{
	struct rmnet_priv *priv = netdev_priv(orig_dev);
	struct rmnet_map_ul_csum_header *ul_header;
	void *iphdr;

	ul_header = (struct rmnet_map_ul_csum_header *)
		    skb_push(skb, sizeof(struct rmnet_map_ul_csum_header));

	if (unlikely(!(orig_dev->features &
		     (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM))))
		goto sw_csum;

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		iphdr = (char *)ul_header +
			sizeof(struct rmnet_map_ul_csum_header);

		if (skb->protocol == htons(ETH_P_IP)) {
			rmnet_map_ipv4_ul_csum_header(iphdr, ul_header, skb);
			priv->stats.csum_hw++;
			return;
		} else if (skb->protocol == htons(ETH_P_IPV6)) {
#if IS_ENABLED(CONFIG_IPV6)
			rmnet_map_ipv6_ul_csum_header(iphdr, ul_header, skb);
			priv->stats.csum_hw++;
			return;
#else
			priv->stats.csum_err_invalid_ip_version++;
			goto sw_csum;
#endif
		} else {
			priv->stats.csum_err_invalid_ip_version++;
		}
	}

sw_csum:
	ul_header->csum_start_offset = 0;
	ul_header->csum_insert_offset = 0;
	ul_header->csum_enabled = 0;
	ul_header->udp_ip4_ind = 0;

	priv->stats.csum_sw++;
}

void rmnet_map_v5_checksum_uplink_packet(struct sk_buff *skb,
					 struct net_device *orig_dev)
{
	struct rmnet_priv *priv = netdev_priv(orig_dev);
	struct rmnet_map_v5_csum_header *ul_header;

	ul_header = (struct rmnet_map_v5_csum_header *)
		    skb_push(skb, sizeof(*ul_header));
	memset(ul_header, 0, sizeof(*ul_header));
	ul_header->header_type = RMNET_MAP_HEADER_TYPE_CSUM_OFFLOAD;

	if (skb->ip_summed == CHECKSUM_PARTIAL) {
		void *iph = (char *)ul_header + sizeof(*ul_header);
		void *trans;
		__sum16 *check;
		u8 proto;

		if (skb->protocol == htons(ETH_P_IP)) {
			u16 ip_len = ((struct iphdr *)iph)->ihl * 4;

			proto = ((struct iphdr *)iph)->protocol;
			trans = iph + ip_len;
		} else if (skb->protocol == htons(ETH_P_IPV6)) {
			u16 ip_len = sizeof(struct ipv6hdr);

			proto = ((struct ipv6hdr *)iph)->nexthdr;
			trans = iph + ip_len;
		} else {
			priv->stats.csum_err_invalid_ip_version++;
			goto sw_csum;
		}

		check = rmnet_map_get_csum_field(proto, trans);
		if (check) {
			*check = 0;
			skb->ip_summed = CHECKSUM_NONE;
			/* Ask for checksum offloading */
			ul_header->csum_valid_required = 1;
			priv->stats.csum_hw++;
			return;
		}
	}

sw_csum:
	priv->stats.csum_sw++;
}

/* Generates UL checksum meta info header for IPv4 and IPv6 over TCP and UDP
 * packets that are supported for UL checksum offload.
 */
void rmnet_map_checksum_uplink_packet(struct sk_buff *skb,
				      struct net_device *orig_dev,
				      int csum_type)
{
	switch (csum_type) {
	case RMNET_FLAGS_EGRESS_MAP_CKSUMV4:
		rmnet_map_v4_checksum_uplink_packet(skb, orig_dev);
		break;
	case RMNET_FLAGS_EGRESS_MAP_CKSUMV5:
		rmnet_map_v5_checksum_uplink_packet(skb, orig_dev);
		break;
	default:
		break;
	}
}

static void rmnet_map_nonlinear_copy(struct sk_buff *coal_skb,
				     u32 hdr_len,
				     u32 start,
				     u16 pkt_len, u8 pkt_count,
				     struct sk_buff *dest)
{
	unsigned char *data_start = rmnet_map_data_ptr(coal_skb) + hdr_len;
	u32 copy_len = pkt_len * pkt_count;

	if (skb_is_nonlinear(coal_skb)) {
		skb_frag_t *frag0 = skb_shinfo(coal_skb)->frags;
		struct page *page = skb_frag_page(frag0);

		skb_append_pagefrags(dest, page,
				     frag0->page_offset + hdr_len + start,
				     copy_len);
		dest->data_len += copy_len;
		dest->len += copy_len;
	} else {
		skb_put_data(dest, data_start + start, copy_len);
	}
}

/* Fill in GSO metadata to allow the SKB to be segmented by the NW stack
 * if needed (i.e. forwarding, UDP GRO)
 */
static void rmnet_map_gso_stamp(struct sk_buff *skb, u16 gso_size, u8 gso_segs)
{
	struct skb_shared_info *shinfo = skb_shinfo(skb);
	struct iphdr *iph = ip_hdr(skb);
	void *addr;
	__sum16 *check;
	__wsum partial;
	int csum_len;
	u16 pkt_len = gso_size * gso_segs;
	u8 protocol;
	bool ipv4 = iph->version == 4;

	if (ipv4) {
		addr = &iph->saddr;
		csum_len = sizeof(iph->saddr) * 2;
		protocol = iph->protocol;
	} else {
		struct ipv6hdr *ip6h = ipv6_hdr(skb);

		addr = &ip6h->saddr;
		csum_len = sizeof(ip6h->saddr) * 2;
		protocol = ip6h->nexthdr;
	}

	if (protocol == IPPROTO_TCP) {
		struct tcphdr *tp = tcp_hdr(skb);

		pkt_len += tp->doff * 4;
		check = &tp->check;
		shinfo->gso_type = (ipv4) ? SKB_GSO_TCPV4 : SKB_GSO_TCPV6;
		skb->csum_offset = offsetof(struct tcphdr, check);
	} else {
		struct udphdr *up = udp_hdr(skb);

		pkt_len += sizeof(*up);
		check = &up->check;
		shinfo->gso_type = SKB_GSO_UDP_L4;
		skb->csum_offset = offsetof(struct udphdr, check);
	}

	partial = csum_partial(addr, csum_len, 0);
	partial = csum16_add(partial, htons((u16)protocol));
	partial = csum16_add(partial, htons(pkt_len));
	*check = ~csum_fold(partial);

	skb->ip_summed = CHECKSUM_PARTIAL;
	skb->csum_start = skb_transport_header(skb) - skb->head;
	shinfo->gso_size = gso_size;
	shinfo->gso_segs = gso_segs;
}

/* Create a new UDP SKB from the coalesced SKB. Appropriate IP and UDP headers
 * will be added.
 */
static struct sk_buff *rmnet_map_segment_udp_skb(struct sk_buff *coal_skb,
						 u32 start,
						 int start_pkt_num,
						 u16 pkt_len, u8 pkt_count)
{
	struct sk_buff *skbn;
	struct iphdr *iph = (struct iphdr *)rmnet_map_data_ptr(coal_skb);
	struct rmnet_priv *priv = netdev_priv(coal_skb->dev);
	struct udphdr *uh;
	u32 alloc_len;
	u16 ip_len, udp_len = sizeof(*uh);

	if (iph->version == 4) {
		ip_len = iph->ihl * 4;
	} else if (iph->version == 6) {
		ip_len = sizeof(struct ipv6hdr);
	} else {
		priv->stats.coal.coal_ip_invalid++;
		return NULL;
	}

	uh = (struct udphdr *)(rmnet_map_data_ptr(coal_skb) + ip_len);

	/* We can avoid copying the data if the SKB we got from the lower-level
	 * drivers was nonlinear.
	 */
	if (skb_is_nonlinear(coal_skb))
		alloc_len = ip_len + udp_len;
	else
		alloc_len = ip_len + udp_len + pkt_len;

	skbn = alloc_skb(alloc_len, GFP_ATOMIC);
	if (!skbn)
		return NULL;

	skb_reserve(skbn, ip_len + udp_len);
	rmnet_map_nonlinear_copy(coal_skb, ip_len + udp_len,
				 start, pkt_len, pkt_count, skbn);

	/* Push UDP header and update length */
	skb_push(skbn, udp_len);
	memcpy(skbn->data, uh, udp_len);
	skb_reset_transport_header(skbn);
	udp_hdr(skbn)->len = htons(skbn->len);

	/* Push IP header and update necessary fields */
	skb_push(skbn, ip_len);
	memcpy(skbn->data, iph, ip_len);
	skb_reset_network_header(skbn);
	if (iph->version == 4) {
		iph = ip_hdr(skbn);
		iph->id = htons(ntohs(iph->id) + start_pkt_num);
		iph->tot_len = htons(skbn->len);
		iph->check = 0;
		iph->check = ip_fast_csum(iph, iph->ihl);
	} else {
		ipv6_hdr(skbn)->payload_len = htons(skbn->len -
						    ip_len);
	}

	skbn->ip_summed = CHECKSUM_UNNECESSARY;
	skbn->dev = coal_skb->dev;
	priv->stats.coal.coal_reconstruct++;

	/* Stamp GSO information if necessary */
	if (pkt_count > 1)
		rmnet_map_gso_stamp(skbn, pkt_len, pkt_count);

	return skbn;
}

/* Create a new TCP SKB from the coalesced SKB. Appropriate IP and TCP headers
 * will be added.
 */
static struct sk_buff *rmnet_map_segment_tcp_skb(struct sk_buff *coal_skb,
						 u32 start,
						 int start_pkt_num,
						 u16 pkt_len, u8 pkt_count)
{
	struct sk_buff *skbn;
	struct iphdr *iph = (struct iphdr *)rmnet_map_data_ptr(coal_skb);
	struct rmnet_priv *priv = netdev_priv(coal_skb->dev);
	struct tcphdr *th;
	u32 alloc_len;
	u16 ip_len, tcp_len;

	if (iph->version == 4) {
		ip_len = iph->ihl * 4;
	} else if (iph->version == 6) {
		ip_len = sizeof(struct ipv6hdr);
	} else {
		priv->stats.coal.coal_ip_invalid++;
		return NULL;
	}

	th = (struct tcphdr *)(rmnet_map_data_ptr(coal_skb) + ip_len);
	tcp_len = th->doff * 4;

	/* We can avoid copying the data if the SKB we got from the lower-level
	 * drivers was nonlinear.
	 */
	if (skb_is_nonlinear(coal_skb))
		alloc_len = ip_len + tcp_len;
	else
		alloc_len = ip_len + tcp_len + pkt_len;

	skbn = alloc_skb(alloc_len, GFP_ATOMIC);
	if (!skbn)
		return NULL;

	skb_reserve(skbn, ip_len + tcp_len);
	rmnet_map_nonlinear_copy(coal_skb, ip_len + tcp_len,
				 start, pkt_len, pkt_count, skbn);

	/* Push TCP header and update sequence number */
	skb_push(skbn, tcp_len);
	memcpy(skbn->data, th, tcp_len);
	skb_reset_transport_header(skbn);
	th = tcp_hdr(skbn);
	th->seq = htonl(ntohl(th->seq) + start);

	/* Push IP header and update necessary fields */
	skb_push(skbn, ip_len);
	memcpy(skbn->data, iph, ip_len);
	skb_reset_network_header(skbn);
	if (iph->version == 4) {
		iph = ip_hdr(skbn);
		iph->id = htons(ntohs(iph->id) + start_pkt_num);
		iph->tot_len = htons(skbn->len);
		iph->check = 0;
		iph->check = ip_fast_csum(iph, iph->ihl);
	} else {
		ipv6_hdr(skbn)->payload_len = htons(skbn->len - ip_len);
	}

	skbn->ip_summed = CHECKSUM_UNNECESSARY;
	skbn->dev = coal_skb->dev;
	priv->stats.coal.coal_reconstruct++;

	/* Stamp GSO information if necessary */
	if (pkt_count > 1)
		rmnet_map_gso_stamp(skbn, pkt_len, pkt_count);

	return skbn;
}

/* Converts the coalesced SKB into a list of SKBs.
 * NLOs containing csum erros will not be included.
 * The original coalesced SKB should be treated as invalid and
 * must be freed by the caller
 */
static void rmnet_map_segment_coal_data(struct sk_buff *coal_skb,
					u64 nlo_err_mask,
					struct sk_buff_head *list)
{
	struct sk_buff *new_skb;
	struct sk_buff *(*segment)(struct sk_buff *coal_skb,
				   u32 start,
				   int start_pkt_num,
				   u16 pkt_len, u8 pkt_count);
	struct iphdr *iph;
	struct rmnet_priv *priv = netdev_priv(coal_skb->dev);
	struct rmnet_map_v5_coal_header *coal_hdr;
	u32 start = 0;
	u16 pkt_len, ip_len, trans_len;
	u8 protocol, start_pkt_num = 0;
	u8 pkt, total_pkt = 0;
	u8 nlo, gro_count = 0;
	bool gro = coal_skb->dev->features & NETIF_F_GRO_HW;

	/* Pull off the headers we no longer need */
	pskb_pull(coal_skb, sizeof(struct rmnet_map_header));
	coal_hdr = (struct rmnet_map_v5_coal_header *)
		   rmnet_map_data_ptr(coal_skb);
	pskb_pull(coal_skb, sizeof(*coal_hdr));

	iph = (struct iphdr *)rmnet_map_data_ptr(coal_skb);

	if (iph->version == 4) {
		protocol = iph->protocol;
		ip_len = iph->ihl * 4;

		/* Don't allow coalescing of any packets with IP options */
		if (iph->ihl != 5)
			gro = false;
	} else if (iph->version == 6) {
		__be16 frag_off;

		protocol = ((struct ipv6hdr *)iph)->nexthdr;
		ip_len = ipv6_skip_exthdr(coal_skb, sizeof(struct ipv6hdr),
					  &protocol, &frag_off);

		/* If we run into a problem, or this has a fragment header
		 * (which should technically not be possible, if the HW
		 * works as intended...), bail.
		 */
		if (ip_len < 0 || frag_off) {
			priv->stats.coal.coal_ip_invalid++;
			return;
		} else if (ip_len > sizeof(struct ipv6hdr)) {
			/* Don't allow coalescing of any packets with IPv6
			 * extension headers.
			 */
			gro = false;
		}
	} else {
		priv->stats.coal.coal_ip_invalid++;
		return;
	}

	if (protocol == IPPROTO_TCP) {
		struct tcphdr *th = (struct tcphdr *)
				    ((unsigned char *)iph + ip_len);
		trans_len = th->doff * 4;
		segment = rmnet_map_segment_tcp_skb;
	} else if (protocol == IPPROTO_UDP) {
		trans_len = sizeof(struct udphdr);
		segment = rmnet_map_segment_udp_skb;
	} else {
		priv->stats.coal.coal_trans_invalid++;
		return;
	}

	for (nlo = 0; nlo < coal_hdr->num_nlos; nlo++) {
		pkt_len = ntohs(coal_hdr->nl_pairs[nlo].pkt_len);
		pkt_len -= ip_len + trans_len;
		for (pkt = 0; pkt < coal_hdr->nl_pairs[nlo].num_packets;
		     pkt++, total_pkt++) {
			nlo_err_mask <<= 1;
			if (nlo_err_mask & (1ULL << 63)) {
				priv->stats.coal.coal_csum_err++;

				/* Segment out the good data */
				if (gro && gro_count) {
					new_skb = segment(coal_skb, start,
							  start_pkt_num,
							  pkt_len, gro_count);
					if (!new_skb)
						return;

					__skb_queue_tail(list, new_skb);
					start += pkt_len * gro_count;
					gro_count = 0;
				}

				/* skip over bad packet */
				start += pkt_len;
				start_pkt_num = total_pkt + 1;
			} else {
				gro_count++;

				/* Segment the packet if we aren't sending the
				 * larger packet up the stack.
				 */
				if (!gro) {
					new_skb = segment(coal_skb, start,
							  start_pkt_num,
							  pkt_len, 1);
					if (!new_skb)
						return;

					__skb_queue_tail(list, new_skb);

					start += pkt_len;
					start_pkt_num = total_pkt + 1;
					gro_count = 0;
				}
			}
		}

		/* If we're switching NLOs, we need to send out everything from
		 * the previous one, if we haven't done so. NLOs only switch
		 * when the packet length changes.
		 */
		if (gro && gro_count) {
			new_skb = segment(coal_skb, start, start_pkt_num,
					  pkt_len, gro_count);
			if (!new_skb)
				return;

			__skb_queue_tail(list, new_skb);

			start += pkt_len * gro_count;
			start_pkt_num = total_pkt + 1;
			gro_count = 0;
		}
	}
}

/* Record reason for coalescing pipe closure */
static void rmnet_map_data_log_close_stats(struct rmnet_priv *priv, u8 type,
					   u8 code)
{
	struct rmnet_coal_close_stats *stats = &priv->stats.coal.close;

	switch (type) {
	case RMNET_MAP_COAL_CLOSE_NON_COAL:
		stats->non_coal++;
		break;
	case RMNET_MAP_COAL_CLOSE_IP_MISS:
		stats->ip_miss++;
		break;
	case RMNET_MAP_COAL_CLOSE_TRANS_MISS:
		stats->trans_miss++;
		break;
	case RMNET_MAP_COAL_CLOSE_HW:
		switch (code) {
		case RMNET_MAP_COAL_CLOSE_HW_NL:
			stats->hw_nl++;
			break;
		case RMNET_MAP_COAL_CLOSE_HW_PKT:
			stats->hw_pkt++;
			break;
		case RMNET_MAP_COAL_CLOSE_HW_BYTE:
			stats->hw_byte++;
			break;
		case RMNET_MAP_COAL_CLOSE_HW_TIME:
			stats->hw_time++;
			break;
		case RMNET_MAP_COAL_CLOSE_HW_EVICT:
			stats->hw_evict++;
			break;
		default:
			break;
		}
		break;
	case RMNET_MAP_COAL_CLOSE_COAL:
		stats->coal++;
		break;
	default:
		break;
	}
}

/* Check if the coalesced header has any incorrect values, in which case, the
 * entire coalesced skb must be dropped. Then check if there are any
 * checksum issues
 */
static int rmnet_map_data_check_coal_header(struct sk_buff *skb,
					    u64 *nlo_err_mask)
{
	struct rmnet_map_v5_coal_header *coal_hdr;
	unsigned char *data = rmnet_map_data_ptr(skb);
	struct rmnet_priv *priv = netdev_priv(skb->dev);
	u64 mask = 0;
	int i;
	u8 veid, pkts = 0;

	coal_hdr = ((struct rmnet_map_v5_coal_header *)
		    (data + sizeof(struct rmnet_map_header)));
	veid = coal_hdr->virtual_channel_id;

	if (coal_hdr->num_nlos == 0 ||
	    coal_hdr->num_nlos > RMNET_MAP_V5_MAX_NLOS) {
		priv->stats.coal.coal_hdr_nlo_err++;
		return -EINVAL;
	}

	for (i = 0; i < RMNET_MAP_V5_MAX_NLOS; i++) {
		/* If there is a checksum issue, we need to split
		 * up the skb. Rebuild the full csum error field
		 */
		u8 err = coal_hdr->nl_pairs[i].csum_error_bitmap;
		u8 pkt = coal_hdr->nl_pairs[i].num_packets;

		mask |= ((u64)err) << (7 - i) * 8;

		/* Track total packets in frame */
		pkts += pkt;
		if (pkts > RMNET_MAP_V5_MAX_PACKETS) {
			priv->stats.coal.coal_hdr_pkt_err++;
			return -EINVAL;
		}
	}

	/* Track number of packets we get inside of coalesced frames */
	priv->stats.coal.coal_pkts += pkts;

	/* Update ethtool stats */
	rmnet_map_data_log_close_stats(priv,
				       coal_hdr->close_type,
				       coal_hdr->close_value);
	if (veid < RMNET_MAX_VEID)
		priv->stats.coal.coal_veid[veid]++;

	*nlo_err_mask = mask;

	return 0;
}

/* Process a QMAPv5 packet header */
int rmnet_map_process_next_hdr_packet(struct sk_buff *skb,
				      struct sk_buff_head *list)
{
	struct rmnet_priv *priv = netdev_priv(skb->dev);
	u64 nlo_err_mask;
	int rc = 0;

	switch (rmnet_map_get_next_hdr_type(skb)) {
	case RMNET_MAP_HEADER_TYPE_COALESCING:
		priv->stats.coal.coal_rx++;
		rc = rmnet_map_data_check_coal_header(skb, &nlo_err_mask);
		if (rc)
			return rc;

		rmnet_map_segment_coal_data(skb, nlo_err_mask, list);
		consume_skb(skb);
		break;
	case RMNET_MAP_HEADER_TYPE_CSUM_OFFLOAD:
		if (rmnet_map_get_csum_valid(skb)) {
			priv->stats.csum_ok++;
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		} else {
			priv->stats.csum_valid_unset++;
		}

		pskb_pull(skb,
			  (sizeof(struct rmnet_map_header) +
			   sizeof(struct rmnet_map_v5_csum_header)));
		__skb_queue_tail(list, skb);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

long rmnet_agg_time_limit __read_mostly = 1000000L;
long rmnet_agg_bypass_time __read_mostly = 10000000L;

int rmnet_map_tx_agg_skip(struct sk_buff *skb, int offset)
{
	u8 *packet_start = skb->data + offset;
	int is_icmp = 0;

	if (skb->protocol == htons(ETH_P_IP)) {
		struct iphdr *ip4h = (struct iphdr *)(packet_start);

		if (ip4h->protocol == IPPROTO_ICMP)
			is_icmp = 1;
	} else if (skb->protocol == htons(ETH_P_IPV6)) {
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

static void rmnet_map_flush_tx_packet_work(struct work_struct *work)
{
	struct sk_buff *skb = NULL;
	struct rmnet_port *port;
	unsigned long flags;

	port = container_of(work, struct rmnet_port, agg_wq);

	spin_lock_irqsave(&port->agg_lock, flags);
	if (likely(port->agg_state == -EINPROGRESS)) {
		/* Buffer may have already been shipped out */
		if (likely(port->agg_skb)) {
			skb = port->agg_skb;
			port->agg_skb = NULL;
			port->agg_count = 0;
			memset(&port->agg_time, 0, sizeof(struct timespec));
		}
		port->agg_state = 0;
	}

	spin_unlock_irqrestore(&port->agg_lock, flags);
	if (skb)
		dev_queue_xmit(skb);
}

enum hrtimer_restart rmnet_map_flush_tx_packet_queue(struct hrtimer *t)
{
	struct rmnet_port *port;

	port = container_of(t, struct rmnet_port, hrtimer);

	schedule_work(&port->agg_wq);
	return HRTIMER_NORESTART;
}

void rmnet_map_tx_aggregate(struct sk_buff *skb, struct rmnet_port *port)
{
	struct timespec diff, last;
	int size, agg_count = 0;
	struct sk_buff *agg_skb;
	unsigned long flags;
	u8 *dest_buff;

new_packet:
	spin_lock_irqsave(&port->agg_lock, flags);
	memcpy(&last, &port->agg_last, sizeof(struct timespec));
	getnstimeofday(&port->agg_last);

	if (!port->agg_skb) {
		/* Check to see if we should agg first. If the traffic is very
		 * sparse, don't aggregate. We will need to tune this later
		 */
		diff = timespec_sub(port->agg_last, last);
		size = port->egress_agg_size - skb->len;

		if (diff.tv_sec > 0 || diff.tv_nsec > rmnet_agg_bypass_time ||
		    size <= 0) {
			spin_unlock_irqrestore(&port->agg_lock, flags);
			skb->protocol = htons(ETH_P_MAP);
			dev_queue_xmit(skb);
			return;
		}

		port->agg_skb = skb_copy_expand(skb, 0, size, GFP_ATOMIC);
		if (!port->agg_skb) {
			port->agg_skb = 0;
			port->agg_count = 0;
			memset(&port->agg_time, 0, sizeof(struct timespec));
			spin_unlock_irqrestore(&port->agg_lock, flags);
			skb->protocol = htons(ETH_P_MAP);
			dev_queue_xmit(skb);
			return;
		}
		port->agg_skb->protocol = htons(ETH_P_MAP);
		port->agg_count = 1;
		getnstimeofday(&port->agg_time);
		dev_kfree_skb_any(skb);
		goto schedule;
	}
	diff = timespec_sub(port->agg_last, port->agg_time);

	if (skb->len > (port->egress_agg_size - port->agg_skb->len) ||
	    port->agg_count >= port->egress_agg_count ||
	    diff.tv_sec > 0 || diff.tv_nsec > rmnet_agg_time_limit) {
		agg_skb = port->agg_skb;
		agg_count = port->agg_count;
		port->agg_skb = 0;
		port->agg_count = 0;
		memset(&port->agg_time, 0, sizeof(struct timespec));
		port->agg_state = 0;
		spin_unlock_irqrestore(&port->agg_lock, flags);
		hrtimer_cancel(&port->hrtimer);
		dev_queue_xmit(agg_skb);
		goto new_packet;
	}

	dest_buff = skb_put(port->agg_skb, skb->len);
	memcpy(dest_buff, skb->data, skb->len);
	port->agg_count++;
	dev_kfree_skb_any(skb);

schedule:
	if (port->agg_state != -EINPROGRESS) {
		port->agg_state = -EINPROGRESS;
		hrtimer_start(&port->hrtimer, ns_to_ktime(3000000),
			      HRTIMER_MODE_REL);
	}
	spin_unlock_irqrestore(&port->agg_lock, flags);
}

void rmnet_map_tx_aggregate_init(struct rmnet_port *port)
{
	hrtimer_init(&port->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	port->hrtimer.function = rmnet_map_flush_tx_packet_queue;
	port->egress_agg_size = 8192;
	port->egress_agg_count = 20;
	spin_lock_init(&port->agg_lock);

	INIT_WORK(&port->agg_wq, rmnet_map_flush_tx_packet_work);
}

void rmnet_map_tx_aggregate_exit(struct rmnet_port *port)
{
	unsigned long flags;

	hrtimer_cancel(&port->hrtimer);
	cancel_work_sync(&port->agg_wq);

	spin_lock_irqsave(&port->agg_lock, flags);
	if (port->agg_state == -EINPROGRESS) {
		if (port->agg_skb) {
			kfree_skb(port->agg_skb);
			port->agg_skb = NULL;
			port->agg_count = 0;
			memset(&port->agg_time, 0, sizeof(struct timespec));
		}

		port->agg_state = 0;
	}

	spin_unlock_irqrestore(&port->agg_lock, flags);
}

void rmnet_map_tx_qmap_cmd(struct sk_buff *qmap_skb)
{
	struct rmnet_port *port;
	struct sk_buff *agg_skb;
	unsigned long flags;

	port = rmnet_get_port(qmap_skb->dev);

	if (port->data_format & RMNET_EGRESS_FORMAT_AGGREGATION) {
		spin_lock_irqsave(&port->agg_lock, flags);
		if (port->agg_skb) {
			agg_skb = port->agg_skb;
			port->agg_skb = 0;
			port->agg_count = 0;
			memset(&port->agg_time, 0, sizeof(struct timespec));
			port->agg_state = 0;
			spin_unlock_irqrestore(&port->agg_lock, flags);
			hrtimer_cancel(&port->hrtimer);
			dev_queue_xmit(agg_skb);
		} else {
			spin_unlock_irqrestore(&port->agg_lock, flags);
		}
	}

	dev_queue_xmit(qmap_skb);
}
EXPORT_SYMBOL(rmnet_map_tx_qmap_cmd);
