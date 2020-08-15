// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 *
 * RMNET Packet Descriptor Framework
 *
 */

#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/ip6_checksum.h>
#include "rmnet_config.h"
#include "rmnet_descriptor.h"
#include "rmnet_handlers.h"
#include "rmnet_private.h"
#include "rmnet_vnd.h"
#include <soc/qcom/rmnet_qmi.h>
#include <soc/qcom/qmi_rmnet.h>

#define RMNET_FRAG_DESCRIPTOR_POOL_SIZE 64
#define RMNET_DL_IND_HDR_SIZE (sizeof(struct rmnet_map_dl_ind_hdr) + \
			       sizeof(struct rmnet_map_header) + \
			       sizeof(struct rmnet_map_control_command_header))
#define RMNET_DL_IND_TRL_SIZE (sizeof(struct rmnet_map_dl_ind_trl) + \
			       sizeof(struct rmnet_map_header) + \
			       sizeof(struct rmnet_map_control_command_header))

typedef void (*rmnet_perf_desc_hook_t)(struct rmnet_frag_descriptor *frag_desc,
				       struct rmnet_port *port);
typedef void (*rmnet_perf_chain_hook_t)(void);

struct rmnet_frag_descriptor *
rmnet_get_frag_descriptor(struct rmnet_port *port)
{
	struct rmnet_frag_descriptor_pool *pool = port->frag_desc_pool;
	struct rmnet_frag_descriptor *frag_desc;
	unsigned long flags;

	spin_lock_irqsave(&port->desc_pool_lock, flags);
	if (!list_empty(&pool->free_list)) {
		frag_desc = list_first_entry(&pool->free_list,
					     struct rmnet_frag_descriptor,
					     list);
		list_del_init(&frag_desc->list);
	} else {
		frag_desc = kzalloc(sizeof(*frag_desc), GFP_ATOMIC);
		if (!frag_desc)
			goto out;

		INIT_LIST_HEAD(&frag_desc->list);
		INIT_LIST_HEAD(&frag_desc->sub_frags);
		pool->pool_size++;
	}

out:
	spin_unlock_irqrestore(&port->desc_pool_lock, flags);
	return frag_desc;
}
EXPORT_SYMBOL(rmnet_get_frag_descriptor);

void rmnet_recycle_frag_descriptor(struct rmnet_frag_descriptor *frag_desc,
				   struct rmnet_port *port)
{
	struct rmnet_frag_descriptor_pool *pool = port->frag_desc_pool;
	struct page *page = skb_frag_page(&frag_desc->frag);
	unsigned long flags;

	list_del(&frag_desc->list);
	if (page)
		put_page(page);

	memset(frag_desc, 0, sizeof(*frag_desc));
	INIT_LIST_HEAD(&frag_desc->list);
	INIT_LIST_HEAD(&frag_desc->sub_frags);
	spin_lock_irqsave(&port->desc_pool_lock, flags);
	list_add_tail(&frag_desc->list, &pool->free_list);
	spin_unlock_irqrestore(&port->desc_pool_lock, flags);
}
EXPORT_SYMBOL(rmnet_recycle_frag_descriptor);

void rmnet_descriptor_add_frag(struct rmnet_port *port, struct list_head *list,
			       struct page *p, u32 page_offset, u32 len)
{
	struct rmnet_frag_descriptor *frag_desc;

	frag_desc = rmnet_get_frag_descriptor(port);
	if (!frag_desc)
		return;

	rmnet_frag_fill(frag_desc, p, page_offset, len);
	list_add_tail(&frag_desc->list, list);
}
EXPORT_SYMBOL(rmnet_descriptor_add_frag);

int rmnet_frag_ipv6_skip_exthdr(struct rmnet_frag_descriptor *frag_desc,
				int start, u8 *nexthdrp, __be16 *fragp)
{
	u32 frag_size = skb_frag_size(&frag_desc->frag);
	u8 nexthdr = *nexthdrp;

	*fragp = 0;

	while (ipv6_ext_hdr(nexthdr)) {
		struct ipv6_opt_hdr *hp;
		int hdrlen;

		if (nexthdr == NEXTHDR_NONE)
			return -EINVAL;

		if (start >= frag_size)
			return -EINVAL;

		hp = rmnet_frag_data_ptr(frag_desc) + start;
		if (nexthdr == NEXTHDR_FRAGMENT) {
			__be16 *fp;

			if (start + offsetof(struct frag_hdr, frag_off) >=
			    frag_size)
				return -EINVAL;

			fp = rmnet_frag_data_ptr(frag_desc) + start +
			     offsetof(struct frag_hdr, frag_off);
			*fragp = *fp;
			if (ntohs(*fragp) & ~0x7)
				break;
			hdrlen = 8;
		} else if (nexthdr == NEXTHDR_AUTH) {
			hdrlen = (hp->hdrlen + 2) << 2;
		} else {
			hdrlen = ipv6_optlen(hp);
		}

		nexthdr = hp->nexthdr;
		start += hdrlen;
	}

	*nexthdrp = nexthdr;
	return start;
}
EXPORT_SYMBOL(rmnet_frag_ipv6_skip_exthdr);

static u8 rmnet_frag_do_flow_control(struct rmnet_map_header *qmap,
				     struct rmnet_port *port,
				     int enable)
{
	struct rmnet_map_control_command *cmd;
	struct rmnet_endpoint *ep;
	struct net_device *vnd;
	u16 ip_family;
	u16 fc_seq;
	u32 qos_id;
	u8 mux_id;
	int r;

	mux_id = qmap->mux_id;
	cmd = (struct rmnet_map_control_command *)
	      ((char *)qmap + sizeof(*qmap));

	if (mux_id >= RMNET_MAX_LOGICAL_EP)
		return RX_HANDLER_CONSUMED;

	ep = rmnet_get_endpoint(port, mux_id);
	if (!ep)
		return RX_HANDLER_CONSUMED;

	vnd = ep->egress_dev;

	ip_family = cmd->flow_control.ip_family;
	fc_seq = ntohs(cmd->flow_control.flow_control_seq_num);
	qos_id = ntohl(cmd->flow_control.qos_id);

	/* Ignore the ip family and pass the sequence number for both v4 and v6
	 * sequence. User space does not support creating dedicated flows for
	 * the 2 protocols
	 */
	r = rmnet_vnd_do_flow_control(vnd, enable);
	if (r)
		return RMNET_MAP_COMMAND_UNSUPPORTED;
	else
		return RMNET_MAP_COMMAND_ACK;
}

static void rmnet_frag_send_ack(struct rmnet_map_header *qmap,
				unsigned char type,
				struct rmnet_port *port)
{
	struct rmnet_map_control_command *cmd;
	struct net_device *dev = port->dev;
	struct sk_buff *skb;
	u16 alloc_len = ntohs(qmap->pkt_len) + sizeof(*qmap);

	skb = alloc_skb(alloc_len, GFP_ATOMIC);
	if (!skb)
		return;

	skb->protocol = htons(ETH_P_MAP);
	skb->dev = dev;

	cmd = rmnet_map_get_cmd_start(skb);
	cmd->cmd_type = type & 0x03;

	netif_tx_lock(dev);
	dev->netdev_ops->ndo_start_xmit(skb, dev);
	netif_tx_unlock(dev);
}

static void
rmnet_frag_process_flow_start(struct rmnet_map_control_command_header *cmd,
			      struct rmnet_port *port,
			      u16 cmd_len)
{
	struct rmnet_map_dl_ind_hdr *dlhdr;
	u32 data_format;
	bool is_dl_mark_v2;

	if (cmd_len + sizeof(struct rmnet_map_header) < RMNET_DL_IND_HDR_SIZE)
		return;

	data_format = port->data_format;
	is_dl_mark_v2 = data_format & RMNET_INGRESS_FORMAT_DL_MARKER_V2;
	dlhdr = (struct rmnet_map_dl_ind_hdr *)((char *)cmd + sizeof(*cmd));

	port->stats.dl_hdr_last_ep_id = cmd->source_id;
	port->stats.dl_hdr_last_qmap_vers = cmd->reserved;
	port->stats.dl_hdr_last_trans_id = cmd->transaction_id;
	port->stats.dl_hdr_last_seq = dlhdr->le.seq;
	port->stats.dl_hdr_last_bytes = dlhdr->le.bytes;
	port->stats.dl_hdr_last_pkts = dlhdr->le.pkts;
	port->stats.dl_hdr_last_flows = dlhdr->le.flows;
	port->stats.dl_hdr_total_bytes += port->stats.dl_hdr_last_bytes;
	port->stats.dl_hdr_total_pkts += port->stats.dl_hdr_last_pkts;
	port->stats.dl_hdr_count++;

	/* If a target is taking frag path, we can assume DL marker v2 is in
	 * play
	 */
	if (is_dl_mark_v2)
		rmnet_map_dl_hdr_notify_v2(port, dlhdr, cmd);
	else
		rmnet_map_dl_hdr_notify(port, dlhdr);
}

static void
rmnet_frag_process_flow_end(struct rmnet_map_control_command_header *cmd,
			    struct rmnet_port *port, u16 cmd_len)
{
	struct rmnet_map_dl_ind_trl *dltrl;
	u32 data_format;
	bool is_dl_mark_v2;


	if (cmd_len + sizeof(struct rmnet_map_header) < RMNET_DL_IND_TRL_SIZE)
		return;

	data_format = port->data_format;
	is_dl_mark_v2 = data_format & RMNET_INGRESS_FORMAT_DL_MARKER_V2;
	dltrl = (struct rmnet_map_dl_ind_trl *)((char *)cmd + sizeof(*cmd));

	port->stats.dl_trl_last_seq = dltrl->seq_le;
	port->stats.dl_trl_count++;

	/* If a target is taking frag path, we can assume DL marker v2 is in
	 * play
	 */
	if (is_dl_mark_v2)
		rmnet_map_dl_trl_notify_v2(port, dltrl, cmd);
	else
		rmnet_map_dl_trl_notify(port, dltrl);
}

/* Process MAP command frame and send N/ACK message as appropriate. Message cmd
 * name is decoded here and appropriate handler is called.
 */
void rmnet_frag_command(struct rmnet_map_header *qmap, struct rmnet_port *port)
{
	struct rmnet_map_control_command *cmd;
	unsigned char command_name;
	unsigned char rc = 0;

	cmd = (struct rmnet_map_control_command *)
	      ((char *)qmap + sizeof(*qmap));
	command_name = cmd->command_name;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_ENABLE:
		rc = rmnet_frag_do_flow_control(qmap, port, 1);
		break;

	case RMNET_MAP_COMMAND_FLOW_DISABLE:
		rc = rmnet_frag_do_flow_control(qmap, port, 0);
		break;

	default:
		rc = RMNET_MAP_COMMAND_UNSUPPORTED;
		break;
	}
	if (rc == RMNET_MAP_COMMAND_ACK)
		rmnet_frag_send_ack(qmap, rc, port);
}

int rmnet_frag_flow_command(struct rmnet_map_header *qmap,
			    struct rmnet_port *port, u16 pkt_len)
{
	struct rmnet_map_control_command_header *cmd;
	unsigned char command_name;

	cmd = (struct rmnet_map_control_command_header *)
	      ((char *)qmap + sizeof(*qmap));
	command_name = cmd->command_name;

	switch (command_name) {
	case RMNET_MAP_COMMAND_FLOW_START:
		rmnet_frag_process_flow_start(cmd, port, pkt_len);
		break;

	case RMNET_MAP_COMMAND_FLOW_END:
		rmnet_frag_process_flow_end(cmd, port, pkt_len);
		break;

	default:
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(rmnet_frag_flow_command);

void rmnet_frag_deaggregate(skb_frag_t *frag, struct rmnet_port *port,
			    struct list_head *list)
{
	struct rmnet_map_header *maph;
	u8 *data = skb_frag_address(frag);
	u32 offset = 0;
	u32 packet_len;

	while (offset < skb_frag_size(frag)) {
		maph = (struct rmnet_map_header *)data;
		packet_len = ntohs(maph->pkt_len);

		/* Some hardware can send us empty frames. Catch them */
		if (packet_len == 0)
			return;

		packet_len += sizeof(*maph);

		if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4) {
			packet_len += sizeof(struct rmnet_map_dl_csum_trailer);
		} else if (port->data_format &
			   (RMNET_FLAGS_INGRESS_MAP_CKSUMV5 |
			    RMNET_FLAGS_INGRESS_COALESCE) && !maph->cd_bit) {
			u32 hsize = 0;
			u8 type;

			type = ((struct rmnet_map_v5_coal_header *)
				(data + sizeof(*maph)))->header_type;
			switch (type) {
			case RMNET_MAP_HEADER_TYPE_COALESCING:
				hsize = sizeof(struct rmnet_map_v5_coal_header);
				break;
			case RMNET_MAP_HEADER_TYPE_CSUM_OFFLOAD:
				hsize = sizeof(struct rmnet_map_v5_csum_header);
				break;
			}

			packet_len += hsize;
		}

		if ((int)skb_frag_size(frag) - (int)packet_len < 0)
			return;

		rmnet_descriptor_add_frag(port, list, skb_frag_page(frag),
					  frag->page_offset + offset,
					  packet_len);

		offset += packet_len;
		data += packet_len;
	}
}

/* Fill in GSO metadata to allow the SKB to be segmented by the NW stack
 * if needed (i.e. forwarding, UDP GRO)
 */
static void rmnet_frag_gso_stamp(struct sk_buff *skb,
				 struct rmnet_frag_descriptor *frag_desc)
{
	struct skb_shared_info *shinfo = skb_shinfo(skb);

	if (frag_desc->trans_proto == IPPROTO_TCP)
		shinfo->gso_type = (frag_desc->ip_proto == 4) ?
				   SKB_GSO_TCPV4 : SKB_GSO_TCPV6;
	else
		shinfo->gso_type = SKB_GSO_UDP_L4;

	shinfo->gso_size = frag_desc->gso_size;
	shinfo->gso_segs = frag_desc->gso_segs;
}

/* Set the partial checksum information. Sets the transport checksum tot he
 * pseudoheader checksum and sets the offload metadata.
 */
static void rmnet_frag_partial_csum(struct sk_buff *skb,
				    struct rmnet_frag_descriptor *frag_desc)
{
	struct iphdr *iph = (struct iphdr *)skb->data;
	__sum16 pseudo;
	u16 pkt_len = skb->len - frag_desc->ip_len;

	if (frag_desc->ip_proto == 4) {
		iph->tot_len = htons(skb->len);
		iph->check = 0;
		iph->check = ip_fast_csum(iph, iph->ihl);
		pseudo = ~csum_tcpudp_magic(iph->saddr, iph->daddr,
					    pkt_len, frag_desc->trans_proto,
					    0);
	} else {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)iph;

		/* Payload length includes any extension headers */
		ip6h->payload_len = htons(skb->len - sizeof(*ip6h));
		pseudo = ~csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
					  pkt_len, frag_desc->trans_proto, 0);
	}

	if (frag_desc->trans_proto == IPPROTO_TCP) {
		struct tcphdr *tp = (struct tcphdr *)
				    ((u8 *)iph + frag_desc->ip_len);

		tp->check = pseudo;
		skb->csum_offset = offsetof(struct tcphdr, check);
	} else {
		struct udphdr *up = (struct udphdr *)
				    ((u8 *)iph + frag_desc->ip_len);

		up->len = htons(pkt_len);
		up->check = pseudo;
		skb->csum_offset = offsetof(struct udphdr, check);
	}

	skb->ip_summed = CHECKSUM_PARTIAL;
	skb->csum_start = (u8 *)iph + frag_desc->ip_len - skb->head;
}

/* Allocate and populate an skb to contain the packet represented by the
 * frag descriptor.
 */
static struct sk_buff *rmnet_alloc_skb(struct rmnet_frag_descriptor *frag_desc,
				       struct rmnet_port *port)
{
	struct sk_buff *head_skb, *current_skb, *skb;
	struct skb_shared_info *shinfo;
	struct rmnet_frag_descriptor *sub_frag, *tmp;

	/* Use the exact sizes if we know them (i.e. RSB/RSC, rmnet_perf) */
	if (frag_desc->hdrs_valid) {
		u16 hdr_len = frag_desc->ip_len + frag_desc->trans_len;

		head_skb = alloc_skb(hdr_len + RMNET_MAP_DEAGGR_HEADROOM,
				     GFP_ATOMIC);
		if (!head_skb)
			return NULL;

		skb_reserve(head_skb, RMNET_MAP_DEAGGR_HEADROOM);
		skb_put_data(head_skb, frag_desc->hdr_ptr, hdr_len);
		skb_reset_network_header(head_skb);

		if (frag_desc->trans_len)
			skb_set_transport_header(head_skb, frag_desc->ip_len);

		/* If the headers we added are the start of the page,
		 * we don't want to add them twice
		 */
		if (frag_desc->hdr_ptr == rmnet_frag_data_ptr(frag_desc)) {
			/* "Header only" packets can be fast-forwarded */
			if (hdr_len == skb_frag_size(&frag_desc->frag))
				goto skip_frags;

			if (!rmnet_frag_pull(frag_desc, port, hdr_len)) {
				kfree_skb(head_skb);
				return NULL;
			}
		}
	} else {
		/* Allocate enough space to avoid penalties in the stack
		 * from __pskb_pull_tail()
		 */
		head_skb = alloc_skb(256 + RMNET_MAP_DEAGGR_HEADROOM,
				     GFP_ATOMIC);
		if (!head_skb)
			return NULL;

		skb_reserve(head_skb, RMNET_MAP_DEAGGR_HEADROOM);
	}

	/* Add main fragment */
	get_page(skb_frag_page(&frag_desc->frag));
	skb_add_rx_frag(head_skb, 0, skb_frag_page(&frag_desc->frag),
			frag_desc->frag.page_offset,
			skb_frag_size(&frag_desc->frag),
			skb_frag_size(&frag_desc->frag));

	shinfo = skb_shinfo(head_skb);
	current_skb = head_skb;

	/* Add in any frags from rmnet_perf */
	list_for_each_entry_safe(sub_frag, tmp, &frag_desc->sub_frags, list) {
		skb_frag_t *frag;
		u32 frag_size;

		frag = &sub_frag->frag;
		frag_size = skb_frag_size(frag);

add_frag:
		if (shinfo->nr_frags < MAX_SKB_FRAGS) {
			get_page(skb_frag_page(frag));
			skb_add_rx_frag(current_skb, shinfo->nr_frags,
					skb_frag_page(frag), frag->page_offset,
					frag_size, frag_size);
			if (current_skb != head_skb) {
				head_skb->len += frag_size;
				head_skb->data_len += frag_size;
			}
		} else {
			/* Alloc a new skb and try again */
			skb = alloc_skb(0, GFP_ATOMIC);
			if (!skb)
				break;

			if (current_skb == head_skb)
				shinfo->frag_list = skb;
			else
				current_skb->next = skb;

			current_skb = skb;
			shinfo = skb_shinfo(current_skb);
			goto add_frag;
		}

		rmnet_recycle_frag_descriptor(sub_frag, port);
	}

skip_frags:
	head_skb->dev = frag_desc->dev;
	rmnet_set_skb_proto(head_skb);

	/* Handle any header metadata that needs to be updated after RSB/RSC
	 * segmentation
	 */
	if (frag_desc->ip_id_set) {
		struct iphdr *iph;

		iph = (struct iphdr *)rmnet_map_data_ptr(head_skb);
		csum_replace2(&iph->check, iph->id, frag_desc->ip_id);
		iph->id = frag_desc->ip_id;
	}

	if (frag_desc->tcp_seq_set) {
		struct tcphdr *th;

		th = (struct tcphdr *)
		     (rmnet_map_data_ptr(head_skb) + frag_desc->ip_len);
		th->seq = frag_desc->tcp_seq;
	}

	/* Handle csum offloading */
	if (frag_desc->csum_valid && frag_desc->hdrs_valid) {
		/* Set the partial checksum information */
		rmnet_frag_partial_csum(head_skb, frag_desc);
	} else if (frag_desc->csum_valid) {
		/* Non-RSB/RSC/perf packet. The current checksum is fine */
		head_skb->ip_summed = CHECKSUM_UNNECESSARY;
	} else if (frag_desc->hdrs_valid &&
		   (frag_desc->trans_proto == IPPROTO_TCP ||
		    frag_desc->trans_proto == IPPROTO_UDP)) {
		/* Unfortunately, we have to fake a bad checksum here, since
		 * the original bad value is lost by the hardware. The only
		 * reliable way to do it is to calculate the actual checksum
		 * and corrupt it.
		 */
		__sum16 *check;
		__wsum csum;
		unsigned int offset = skb_transport_offset(head_skb);
		__sum16 pseudo;

		/* Calculate pseudo header and update header fields */
		if (frag_desc->ip_proto == 4) {
			struct iphdr *iph = ip_hdr(head_skb);
			__be16 tot_len = htons(head_skb->len);

			csum_replace2(&iph->check, iph->tot_len, tot_len);
			iph->tot_len = tot_len;
			pseudo = ~csum_tcpudp_magic(iph->saddr, iph->daddr,
						    head_skb->len -
						    frag_desc->ip_len,
						    frag_desc->trans_proto, 0);
		} else {
			struct ipv6hdr *ip6h = ipv6_hdr(head_skb);

			ip6h->payload_len = htons(head_skb->len -
						  sizeof(*ip6h));
			pseudo = ~csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
						  head_skb->len -
						  frag_desc->ip_len,
						  frag_desc->trans_proto, 0);
		}

		if (frag_desc->trans_proto == IPPROTO_TCP) {
			check = &tcp_hdr(head_skb)->check;
		} else {
			udp_hdr(head_skb)->len = htons(head_skb->len -
						       frag_desc->ip_len);
			check = &udp_hdr(head_skb)->check;
		}

		*check = pseudo;
		csum = skb_checksum(head_skb, offset, head_skb->len - offset,
				    0);
		/* Add 1 to corrupt. This cannot produce a final value of 0
		 * since csum_fold() can't return a value of 0xFFFF
		 */
		*check = csum16_add(csum_fold(csum), htons(1));
		head_skb->ip_summed = CHECKSUM_NONE;
	}

	/* Handle any rmnet_perf metadata */
	if (frag_desc->hash) {
		head_skb->hash = frag_desc->hash;
		head_skb->sw_hash = 1;
	}

	if (frag_desc->flush_shs)
		head_skb->cb[0] = 1;

	/* Handle coalesced packets */
	if (frag_desc->gso_segs > 1)
		rmnet_frag_gso_stamp(head_skb, frag_desc);

	return head_skb;
}

/* Deliver the packets contained within a frag descriptor */
void rmnet_frag_deliver(struct rmnet_frag_descriptor *frag_desc,
			struct rmnet_port *port)
{
	struct sk_buff *skb;

	skb = rmnet_alloc_skb(frag_desc, port);
	if (skb)
		rmnet_deliver_skb(skb, port);
	rmnet_recycle_frag_descriptor(frag_desc, port);
}
EXPORT_SYMBOL(rmnet_frag_deliver);

static void __rmnet_frag_segment_data(struct rmnet_frag_descriptor *coal_desc,
				      struct rmnet_port *port,
				      struct list_head *list, u8 pkt_id,
				      bool csum_valid)
{
	struct rmnet_priv *priv = netdev_priv(coal_desc->dev);
	struct rmnet_frag_descriptor *new_frag;
	u8 *hdr_start = rmnet_frag_data_ptr(coal_desc);
	u32 offset;

	new_frag = rmnet_get_frag_descriptor(port);
	if (!new_frag)
		return;

	/* Account for header lengths to access the data start */
	offset = coal_desc->frag.page_offset + coal_desc->ip_len +
		 coal_desc->trans_len + coal_desc->data_offset;

	/* Header information and most metadata is the same as the original */
	memcpy(new_frag, coal_desc, sizeof(*coal_desc));
	INIT_LIST_HEAD(&new_frag->list);
	INIT_LIST_HEAD(&new_frag->sub_frags);
	rmnet_frag_fill(new_frag, skb_frag_page(&coal_desc->frag), offset,
			coal_desc->gso_size * coal_desc->gso_segs);

	if (coal_desc->trans_proto == IPPROTO_TCP) {
		struct tcphdr *th;

		th = (struct tcphdr *)(hdr_start + coal_desc->ip_len);
		new_frag->tcp_seq_set = 1;
		new_frag->tcp_seq = htonl(ntohl(th->seq) +
					  coal_desc->data_offset);
	} else if (coal_desc->trans_proto == IPPROTO_UDP) {
		struct udphdr *uh;

		uh = (struct udphdr *)(hdr_start + coal_desc->ip_len);
		if (coal_desc->ip_proto == 4 && !uh->check)
			csum_valid = true;
	}

	if (coal_desc->ip_proto == 4) {
		struct iphdr *iph;

		iph = (struct iphdr *)hdr_start;
		new_frag->ip_id_set = 1;
		new_frag->ip_id = htons(ntohs(iph->id) + coal_desc->pkt_id);
	}

	new_frag->hdr_ptr = hdr_start;
	new_frag->csum_valid = csum_valid;
	priv->stats.coal.coal_reconstruct++;

	/* Update meta information to move past the data we just segmented */
	coal_desc->data_offset += coal_desc->gso_size * coal_desc->gso_segs;
	coal_desc->pkt_id = pkt_id + 1;
	coal_desc->gso_segs = 0;

	list_add_tail(&new_frag->list, list);
}

static bool rmnet_frag_validate_csum(struct rmnet_frag_descriptor *frag_desc)
{
	u8 *data = rmnet_frag_data_ptr(frag_desc);
	unsigned int datagram_len;
	__wsum csum;
	__sum16 pseudo;

	datagram_len = skb_frag_size(&frag_desc->frag) - frag_desc->ip_len;
	if (frag_desc->ip_proto == 4) {
		struct iphdr *iph = (struct iphdr *)data;

		pseudo = ~csum_tcpudp_magic(iph->saddr, iph->daddr,
					    datagram_len,
					    frag_desc->trans_proto, 0);
	} else {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)data;

		pseudo = ~csum_ipv6_magic(&ip6h->saddr, &ip6h->daddr,
					  datagram_len, frag_desc->trans_proto,
					  0);
	}

	csum = csum_partial(data + frag_desc->ip_len, datagram_len,
			    csum_unfold(pseudo));
	return !csum_fold(csum);
}

/* Converts the coalesced frame into a list of descriptors.
 * NLOs containing csum erros will not be included.
 */
static void
rmnet_frag_segment_coal_data(struct rmnet_frag_descriptor *coal_desc,
			     u64 nlo_err_mask, struct rmnet_port *port,
			     struct list_head *list)
{
	struct iphdr *iph;
	struct rmnet_priv *priv = netdev_priv(coal_desc->dev);
	struct rmnet_map_v5_coal_header *coal_hdr;
	u16 pkt_len;
	u8 pkt, total_pkt = 0;
	u8 nlo;
	bool gro = coal_desc->dev->features & NETIF_F_GRO_HW;
	bool zero_csum = false;

	/* Pull off the headers we no longer need */
	if (!rmnet_frag_pull(coal_desc, port, sizeof(struct rmnet_map_header)))
		return;

	coal_hdr = (struct rmnet_map_v5_coal_header *)
		   rmnet_frag_data_ptr(coal_desc);
	if (!rmnet_frag_pull(coal_desc, port, sizeof(*coal_hdr)))
		return;

	iph = (struct iphdr *)rmnet_frag_data_ptr(coal_desc);

	if (iph->version == 4) {
		coal_desc->ip_proto = 4;
		coal_desc->ip_len = iph->ihl * 4;
		coal_desc->trans_proto = iph->protocol;

		/* Don't allow coalescing of any packets with IP options */
		if (iph->ihl != 5)
			gro = false;
	} else if (iph->version == 6) {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)iph;
		int ip_len;
		__be16 frag_off;
		u8 protocol = ip6h->nexthdr;

		coal_desc->ip_proto = 6;
		ip_len = rmnet_frag_ipv6_skip_exthdr(coal_desc,
						     sizeof(*ip6h),
						     &protocol,
						     &frag_off);
		coal_desc->trans_proto = protocol;

		/* If we run into a problem, or this has a fragment header
		 * (which should technically not be possible, if the HW
		 * works as intended...), bail.
		 */
		if (ip_len < 0 || frag_off) {
			priv->stats.coal.coal_ip_invalid++;
			return;
		}

		coal_desc->ip_len = (u16)ip_len;
		if (coal_desc->ip_len > sizeof(*ip6h)) {
			/* Don't allow coalescing of any packets with IPv6
			 * extension headers.
			 */
			gro = false;
		}
	} else {
		priv->stats.coal.coal_ip_invalid++;
		return;
	}

	if (coal_desc->trans_proto == IPPROTO_TCP) {
		struct tcphdr *th;

		th = (struct tcphdr *)((u8 *)iph + coal_desc->ip_len);
		coal_desc->trans_len = th->doff * 4;
		priv->stats.coal.coal_tcp++;
		priv->stats.coal.coal_tcp_bytes +=
			skb_frag_size(&coal_desc->frag);
	} else if (coal_desc->trans_proto == IPPROTO_UDP) {
		struct udphdr *uh;

		uh = (struct udphdr *)((u8 *)iph + coal_desc->ip_len);
		coal_desc->trans_len = sizeof(*uh);
		priv->stats.coal.coal_udp++;
		priv->stats.coal.coal_udp_bytes +=
			skb_frag_size(&coal_desc->frag);
		if (coal_desc->ip_proto == 4 && !uh->check)
			zero_csum = true;
	} else {
		priv->stats.coal.coal_trans_invalid++;
		return;
	}

	coal_desc->hdrs_valid = 1;

	if (rmnet_map_v5_csum_buggy(coal_hdr) && !zero_csum) {
		/* Mark the checksum as valid if it checks out */
		if (rmnet_frag_validate_csum(coal_desc))
			coal_desc->csum_valid = true;

		coal_desc->hdr_ptr = rmnet_frag_data_ptr(coal_desc);
		coal_desc->gso_size = ntohs(coal_hdr->nl_pairs[0].pkt_len);
		coal_desc->gso_size -= coal_desc->ip_len + coal_desc->trans_len;
		coal_desc->gso_segs = coal_hdr->nl_pairs[0].num_packets;
		list_add_tail(&coal_desc->list, list);
		return;
	}

	/* Fast-forward the case where we have 1 NLO (i.e. 1 packet length),
	 * no checksum errors, and are allowing GRO. We can just reuse this
	 * descriptor unchanged.
	 */
	if (gro && coal_hdr->num_nlos == 1 && coal_hdr->csum_valid) {
		coal_desc->csum_valid = true;
		coal_desc->hdr_ptr = rmnet_frag_data_ptr(coal_desc);
		coal_desc->gso_size = ntohs(coal_hdr->nl_pairs[0].pkt_len);
		coal_desc->gso_size -= coal_desc->ip_len + coal_desc->trans_len;
		coal_desc->gso_segs = coal_hdr->nl_pairs[0].num_packets;
		list_add_tail(&coal_desc->list, list);
		return;
	}

	/* Segment the coalesced descriptor into new packets */
	for (nlo = 0; nlo < coal_hdr->num_nlos; nlo++) {
		pkt_len = ntohs(coal_hdr->nl_pairs[nlo].pkt_len);
		pkt_len -= coal_desc->ip_len + coal_desc->trans_len;
		coal_desc->gso_size = pkt_len;
		for (pkt = 0; pkt < coal_hdr->nl_pairs[nlo].num_packets;
		     pkt++, total_pkt++, nlo_err_mask >>= 1) {
			bool csum_err = nlo_err_mask & 1;

			/* Segment the packet if we're not sending the larger
			 * packet up the stack.
			 */
			if (!gro) {
				coal_desc->gso_segs = 1;
				if (csum_err)
					priv->stats.coal.coal_csum_err++;

				__rmnet_frag_segment_data(coal_desc, port,
							  list, total_pkt,
							  !csum_err);
				continue;
			}

			if (csum_err) {
				priv->stats.coal.coal_csum_err++;

				/* Segment out the good data */
				if (coal_desc->gso_segs)
					__rmnet_frag_segment_data(coal_desc,
								  port,
								  list,
								  total_pkt,
								  true);

				/* Segment out the bad checksum */
				coal_desc->gso_segs = 1;
				__rmnet_frag_segment_data(coal_desc, port,
							  list, total_pkt,
							  false);
			} else {
				coal_desc->gso_segs++;

			}
		}

		/* If we're switching NLOs, we need to send out everything from
		 * the previous one, if we haven't done so. NLOs only switch
		 * when the packet length changes.
		 */
		if (coal_desc->gso_segs)
			__rmnet_frag_segment_data(coal_desc, port, list,
						  total_pkt, true);
	}
}

/* Record reason for coalescing pipe closure */
static void rmnet_frag_data_log_close_stats(struct rmnet_priv *priv, u8 type,
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
 * entire coalesced frame must be dropped. Then check if there are any
 * checksum issues
 */
static int
rmnet_frag_data_check_coal_header(struct rmnet_frag_descriptor *frag_desc,
				  u64 *nlo_err_mask)
{
	struct rmnet_map_v5_coal_header *coal_hdr;
	unsigned char *data = rmnet_frag_data_ptr(frag_desc);
	struct rmnet_priv *priv = netdev_priv(frag_desc->dev);
	u64 mask = 0;
	int i;
	u8 veid, pkts = 0;

	coal_hdr = (struct rmnet_map_v5_coal_header *)
		   (data + sizeof(struct rmnet_map_header));
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

		mask |= ((u64)err) << (8 * i);

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
	rmnet_frag_data_log_close_stats(priv,
					coal_hdr->close_type,
					coal_hdr->close_value);
	if (veid < RMNET_MAX_VEID)
		priv->stats.coal.coal_veid[veid]++;

	*nlo_err_mask = mask;

	return 0;
}

/* Process a QMAPv5 packet header */
int rmnet_frag_process_next_hdr_packet(struct rmnet_frag_descriptor *frag_desc,
				       struct rmnet_port *port,
				       struct list_head *list,
				       u16 len)
{
	struct rmnet_priv *priv = netdev_priv(frag_desc->dev);
	u64 nlo_err_mask;
	int rc = 0;

	switch (rmnet_frag_get_next_hdr_type(frag_desc)) {
	case RMNET_MAP_HEADER_TYPE_COALESCING:
		priv->stats.coal.coal_rx++;
		rc = rmnet_frag_data_check_coal_header(frag_desc,
						       &nlo_err_mask);
		if (rc)
			return rc;

		rmnet_frag_segment_coal_data(frag_desc, nlo_err_mask, port,
					     list);
		if (list_first_entry(list, struct rmnet_frag_descriptor,
				     list) != frag_desc)
			rmnet_recycle_frag_descriptor(frag_desc, port);
		break;
	case RMNET_MAP_HEADER_TYPE_CSUM_OFFLOAD:
		if (rmnet_frag_get_csum_valid(frag_desc)) {
			priv->stats.csum_ok++;
			frag_desc->csum_valid = true;
		} else {
			priv->stats.csum_valid_unset++;
		}

		if (!rmnet_frag_pull(frag_desc, port,
				     sizeof(struct rmnet_map_header) +
				     sizeof(struct rmnet_map_v5_csum_header))) {
			rc = -EINVAL;
			break;
		}

		frag_desc->hdr_ptr = rmnet_frag_data_ptr(frag_desc);

		/* Remove padding only for csum offload packets.
		 * Coalesced packets should never have padding.
		 */
		if (!rmnet_frag_trim(frag_desc, port, len)) {
			rc = -EINVAL;
			break;
		}

		list_del_init(&frag_desc->list);
		list_add_tail(&frag_desc->list, list);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

/* Perf hook handler */
rmnet_perf_desc_hook_t rmnet_perf_desc_entry __rcu __read_mostly;
EXPORT_SYMBOL(rmnet_perf_desc_entry);

static void
__rmnet_frag_ingress_handler(struct rmnet_frag_descriptor *frag_desc,
			     struct rmnet_port *port)
{
	rmnet_perf_desc_hook_t rmnet_perf_ingress;
	struct rmnet_map_header *qmap;
	struct rmnet_endpoint *ep;
	struct rmnet_frag_descriptor *frag, *tmp;
	LIST_HEAD(segs);
	u16 len, pad;
	u8 mux_id;

	qmap = (struct rmnet_map_header *)skb_frag_address(&frag_desc->frag);
	mux_id = qmap->mux_id;
	pad = qmap->pad_len;
	len = ntohs(qmap->pkt_len) - pad;

	if (qmap->cd_bit) {
		qmi_rmnet_set_dl_msg_active(port);
		if (port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER) {
			rmnet_frag_flow_command(qmap, port, len);
			goto recycle;
		}

		if (port->data_format & RMNET_FLAGS_INGRESS_MAP_COMMANDS)
			rmnet_frag_command(qmap, port);

		goto recycle;
	}

	if (mux_id >= RMNET_MAX_LOGICAL_EP)
		goto recycle;

	ep = rmnet_get_endpoint(port, mux_id);
	if (!ep)
		goto recycle;

	frag_desc->dev = ep->egress_dev;

	/* Handle QMAPv5 packet */
	if (qmap->next_hdr &&
	    (port->data_format & (RMNET_FLAGS_INGRESS_COALESCE |
				  RMNET_FLAGS_INGRESS_MAP_CKSUMV5))) {
		if (rmnet_frag_process_next_hdr_packet(frag_desc, port, &segs,
						       len))
			goto recycle;
	} else {
		/* We only have the main QMAP header to worry about */
		if (!rmnet_frag_pull(frag_desc, port, sizeof(*qmap)))
			return;

		frag_desc->hdr_ptr = rmnet_frag_data_ptr(frag_desc);

		if (!rmnet_frag_trim(frag_desc, port, len))
			return;

		list_add_tail(&frag_desc->list, &segs);
	}

	if (port->data_format & RMNET_INGRESS_FORMAT_PS)
		qmi_rmnet_work_maybe_restart(port);

	rcu_read_lock();
	rmnet_perf_ingress = rcu_dereference(rmnet_perf_desc_entry);
	if (rmnet_perf_ingress) {
		list_for_each_entry_safe(frag, tmp, &segs, list) {
			list_del_init(&frag->list);
			rmnet_perf_ingress(frag, port);
		}
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	list_for_each_entry_safe(frag, tmp, &segs, list) {
		list_del_init(&frag->list);
		rmnet_frag_deliver(frag, port);
	}
	return;

recycle:
	rmnet_recycle_frag_descriptor(frag_desc, port);
}

/* Notify perf at the end of SKB chain */
rmnet_perf_chain_hook_t rmnet_perf_chain_end __rcu __read_mostly;
EXPORT_SYMBOL(rmnet_perf_chain_end);

void rmnet_frag_ingress_handler(struct sk_buff *skb,
				struct rmnet_port *port)
{
	rmnet_perf_chain_hook_t rmnet_perf_opt_chain_end;
	LIST_HEAD(desc_list);

	/* Deaggregation and freeing of HW originating
	 * buffers is done within here
	 */
	while (skb) {
		struct sk_buff *skb_frag;

		rmnet_frag_deaggregate(skb_shinfo(skb)->frags, port,
				       &desc_list);
		if (!list_empty(&desc_list)) {
			struct rmnet_frag_descriptor *frag_desc, *tmp;

			list_for_each_entry_safe(frag_desc, tmp, &desc_list,
						 list) {
				list_del_init(&frag_desc->list);
				__rmnet_frag_ingress_handler(frag_desc, port);
			}
		}

		skb_frag = skb_shinfo(skb)->frag_list;
		skb_shinfo(skb)->frag_list = NULL;
		consume_skb(skb);
		skb = skb_frag;
	}

	rcu_read_lock();
	rmnet_perf_opt_chain_end = rcu_dereference(rmnet_perf_chain_end);
	if (rmnet_perf_opt_chain_end)
		rmnet_perf_opt_chain_end();
	rcu_read_unlock();
}

void rmnet_descriptor_deinit(struct rmnet_port *port)
{
	struct rmnet_frag_descriptor_pool *pool;
	struct rmnet_frag_descriptor *frag_desc, *tmp;

	pool = port->frag_desc_pool;

	list_for_each_entry_safe(frag_desc, tmp, &pool->free_list, list) {
		kfree(frag_desc);
		pool->pool_size--;
	}

	kfree(pool);
}

int rmnet_descriptor_init(struct rmnet_port *port)
{
	struct rmnet_frag_descriptor_pool *pool;
	int i;

	spin_lock_init(&port->desc_pool_lock);
	pool = kzalloc(sizeof(*pool), GFP_ATOMIC);
	if (!pool)
		return -ENOMEM;

	INIT_LIST_HEAD(&pool->free_list);
	port->frag_desc_pool = pool;

	for (i = 0; i < RMNET_FRAG_DESCRIPTOR_POOL_SIZE; i++) {
		struct rmnet_frag_descriptor *frag_desc;

		frag_desc = kzalloc(sizeof(*frag_desc), GFP_ATOMIC);
		if (!frag_desc)
			return -ENOMEM;

		INIT_LIST_HEAD(&frag_desc->list);
		INIT_LIST_HEAD(&frag_desc->sub_frags);
		list_add_tail(&frag_desc->list, &pool->free_list);
		pool->pool_size++;
	}

	return 0;
}
