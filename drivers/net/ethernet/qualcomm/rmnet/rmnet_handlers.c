// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 *
 * RMNET Data ingress/egress handler
 *
 */

#include <linux/netdevice.h>
#include <linux/netdev_features.h>
#include <linux/if_arp.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/sock.h>
#include <linux/tracepoint.h>
#include "rmnet_private.h"
#include "rmnet_config.h"
#include "rmnet_vnd.h"
#include "rmnet_map.h"
#include "rmnet_handlers.h"
#include "rmnet_descriptor.h"

#include <soc/qcom/rmnet_qmi.h>
#include <soc/qcom/qmi_rmnet.h>

#define RMNET_IP_VERSION_4 0x40
#define RMNET_IP_VERSION_6 0x60
#define CREATE_TRACE_POINTS
#include "rmnet_trace.h"

EXPORT_TRACEPOINT_SYMBOL(rmnet_shs_low);
EXPORT_TRACEPOINT_SYMBOL(rmnet_shs_high);
EXPORT_TRACEPOINT_SYMBOL(rmnet_shs_err);
EXPORT_TRACEPOINT_SYMBOL(rmnet_shs_wq_low);
EXPORT_TRACEPOINT_SYMBOL(rmnet_shs_wq_high);
EXPORT_TRACEPOINT_SYMBOL(rmnet_shs_wq_err);
EXPORT_TRACEPOINT_SYMBOL(rmnet_perf_low);
EXPORT_TRACEPOINT_SYMBOL(rmnet_perf_high);
EXPORT_TRACEPOINT_SYMBOL(rmnet_perf_err);
EXPORT_TRACEPOINT_SYMBOL(rmnet_low);
EXPORT_TRACEPOINT_SYMBOL(rmnet_high);
EXPORT_TRACEPOINT_SYMBOL(rmnet_err);
EXPORT_TRACEPOINT_SYMBOL(rmnet_freq_update);
EXPORT_TRACEPOINT_SYMBOL(rmnet_freq_reset);
EXPORT_TRACEPOINT_SYMBOL(rmnet_freq_boost);

/* Helper Functions */

static int rmnet_check_skb_can_gro(struct sk_buff *skb)
{
	unsigned char *data = rmnet_map_data_ptr(skb);

	switch (skb->protocol) {
	case htons(ETH_P_IP):
		if (((struct iphdr *)data)->protocol == IPPROTO_TCP)
			return 0;
		break;
	case htons(ETH_P_IPV6):
		if (((struct ipv6hdr *)data)->nexthdr == IPPROTO_TCP)
			return 0;
		/* Fall through */
	}

	return -EPROTONOSUPPORT;
}

void rmnet_set_skb_proto(struct sk_buff *skb)
{
	switch (rmnet_map_data_ptr(skb)[0] & 0xF0) {
	case RMNET_IP_VERSION_4:
		skb->protocol = htons(ETH_P_IP);
		break;
	case RMNET_IP_VERSION_6:
		skb->protocol = htons(ETH_P_IPV6);
		break;
	default:
		skb->protocol = htons(ETH_P_MAP);
		break;
	}
}
EXPORT_SYMBOL(rmnet_set_skb_proto);

bool (*rmnet_shs_slow_start_detect)(u32 hash_key) __rcu __read_mostly;
EXPORT_SYMBOL(rmnet_shs_slow_start_detect);

bool rmnet_slow_start_on(u32 hash_key)
{
	bool (*rmnet_shs_slow_start_on)(u32 hash_key);

	rmnet_shs_slow_start_on = rcu_dereference(rmnet_shs_slow_start_detect);
	if (rmnet_shs_slow_start_on)
		return rmnet_shs_slow_start_on(hash_key);
	return false;
}
EXPORT_SYMBOL(rmnet_slow_start_on);

/* Shs hook handler */

int (*rmnet_shs_skb_entry)(struct sk_buff *skb,
			   struct rmnet_port *port) __rcu __read_mostly;
EXPORT_SYMBOL(rmnet_shs_skb_entry);

/* Shs hook handler for work queue*/
int (*rmnet_shs_skb_entry_wq)(struct sk_buff *skb,
			      struct rmnet_port *port) __rcu __read_mostly;
EXPORT_SYMBOL(rmnet_shs_skb_entry_wq);

/* Generic handler */

void
rmnet_deliver_skb(struct sk_buff *skb, struct rmnet_port *port)
{
	int (*rmnet_shs_stamp)(struct sk_buff *skb, struct rmnet_port *port);
	struct rmnet_priv *priv = netdev_priv(skb->dev);

	trace_rmnet_low(RMNET_MODULE, RMNET_DLVR_SKB, 0xDEF, 0xDEF,
			0xDEF, 0xDEF, (void *)skb, NULL);
	skb_reset_transport_header(skb);
	skb_reset_network_header(skb);
	rmnet_vnd_rx_fixup(skb->dev, skb->len);

	skb->pkt_type = PACKET_HOST;
	skb_set_mac_header(skb, 0);

	rcu_read_lock();
	rmnet_shs_stamp = rcu_dereference(rmnet_shs_skb_entry);
	if (rmnet_shs_stamp) {
		rmnet_shs_stamp(skb, port);
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	if (port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER) {
		if (!rmnet_check_skb_can_gro(skb) &&
		    port->dl_marker_flush >= 0) {
			struct napi_struct *napi = get_current_napi_context();

			napi_gro_receive(napi, skb);
			port->dl_marker_flush++;
		} else {
			netif_receive_skb(skb);
		}
	} else {
		if (!rmnet_check_skb_can_gro(skb))
			gro_cells_receive(&priv->gro_cells, skb);
		else
			netif_receive_skb(skb);
	}
}
EXPORT_SYMBOL(rmnet_deliver_skb);

/* Important to note, port cannot be used here if it has gone stale */
void
rmnet_deliver_skb_wq(struct sk_buff *skb, struct rmnet_port *port,
		     enum rmnet_packet_context ctx)
{
	int (*rmnet_shs_stamp)(struct sk_buff *skb, struct rmnet_port *port);
	struct rmnet_priv *priv = netdev_priv(skb->dev);

	trace_rmnet_low(RMNET_MODULE, RMNET_DLVR_SKB, 0xDEF, 0xDEF,
			0xDEF, 0xDEF, (void *)skb, NULL);
	skb_reset_transport_header(skb);
	skb_reset_network_header(skb);
	rmnet_vnd_rx_fixup(skb->dev, skb->len);

	skb->pkt_type = PACKET_HOST;
	skb_set_mac_header(skb, 0);

	/* packets coming from work queue context due to packet flush timer
	 * must go through the special workqueue path in SHS driver
	 */
	rcu_read_lock();
	rmnet_shs_stamp = (!ctx) ? rcu_dereference(rmnet_shs_skb_entry) :
				   rcu_dereference(rmnet_shs_skb_entry_wq);
	if (rmnet_shs_stamp) {
		rmnet_shs_stamp(skb, port);
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	if (ctx == RMNET_NET_RX_CTX) {
		if (port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER) {
			if (!rmnet_check_skb_can_gro(skb) &&
			    port->dl_marker_flush >= 0) {
				struct napi_struct *napi =
					get_current_napi_context();
				napi_gro_receive(napi, skb);
				port->dl_marker_flush++;
			} else {
				netif_receive_skb(skb);
			}
		} else {
			if (!rmnet_check_skb_can_gro(skb))
				gro_cells_receive(&priv->gro_cells, skb);
			else
				netif_receive_skb(skb);
		}
	} else {
		if ((port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER) &&
		    port->dl_marker_flush >= 0)
			port->dl_marker_flush++;
		gro_cells_receive(&priv->gro_cells, skb);
	}
}
EXPORT_SYMBOL(rmnet_deliver_skb_wq);

/* Deliver a list of skbs after undoing coalescing */
static void rmnet_deliver_skb_list(struct sk_buff_head *head,
				   struct rmnet_port *port)
{
	struct sk_buff *skb;

	while ((skb = __skb_dequeue(head))) {
		rmnet_set_skb_proto(skb);
		rmnet_deliver_skb(skb, port);
	}
}

/* MAP handler */

static void
__rmnet_map_ingress_handler(struct sk_buff *skb,
			    struct rmnet_port *port)
{
	struct rmnet_map_header *qmap;
	struct rmnet_endpoint *ep;
	struct sk_buff_head list;
	u16 len, pad;
	u8 mux_id;

	/* We don't need the spinlock since only we touch this */
	__skb_queue_head_init(&list);

	qmap = (struct rmnet_map_header *)rmnet_map_data_ptr(skb);
	if (qmap->cd_bit) {
		qmi_rmnet_set_dl_msg_active(port);
		if (port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER) {
			if (!rmnet_map_flow_command(skb, port, false))
				return;
		}

		if (port->data_format & RMNET_FLAGS_INGRESS_MAP_COMMANDS)
			return rmnet_map_command(skb, port);

		goto free_skb;
	}

	mux_id = qmap->mux_id;
	pad = qmap->pad_len;
	len = ntohs(qmap->pkt_len) - pad;

	if (mux_id >= RMNET_MAX_LOGICAL_EP)
		goto free_skb;

	ep = rmnet_get_endpoint(port, mux_id);
	if (!ep)
		goto free_skb;

	skb->dev = ep->egress_dev;

	/* Handle QMAPv5 packet */
	if (qmap->next_hdr &&
	    (port->data_format & (RMNET_FLAGS_INGRESS_COALESCE |
				  RMNET_FLAGS_INGRESS_MAP_CKSUMV5))) {
		if (rmnet_map_process_next_hdr_packet(skb, &list, len))
			goto free_skb;
	} else {
		/* We only have the main QMAP header to worry about */
		pskb_pull(skb, sizeof(*qmap));

		rmnet_set_skb_proto(skb);

		if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4) {
			if (!rmnet_map_checksum_downlink_packet(skb, len + pad))
				skb->ip_summed = CHECKSUM_UNNECESSARY;
		}

		pskb_trim(skb, len);

		/* Push the single packet onto the list */
		__skb_queue_tail(&list, skb);
	}

#ifdef CONFIG_QCOM_QMI_HELPERS
	if (port->data_format & RMNET_INGRESS_FORMAT_PS)
		qmi_rmnet_work_maybe_restart(port);
#endif

	rmnet_deliver_skb_list(&list, port);
	return;

free_skb:
	kfree_skb(skb);
}

int (*rmnet_perf_deag_entry)(struct sk_buff *skb,
			     struct rmnet_port *port) __rcu __read_mostly;
EXPORT_SYMBOL(rmnet_perf_deag_entry);

static void
rmnet_map_ingress_handler(struct sk_buff *skb,
			  struct rmnet_port *port)
{
	struct sk_buff *skbn;
	int (*rmnet_perf_core_deaggregate)(struct sk_buff *skb,
					   struct rmnet_port *port);

	if (skb->dev->type == ARPHRD_ETHER) {
		if (pskb_expand_head(skb, ETH_HLEN, 0, GFP_ATOMIC)) {
			kfree_skb(skb);
			return;
		}

		skb_push(skb, ETH_HLEN);
	}

	if (port->data_format & (RMNET_FLAGS_INGRESS_COALESCE |
				 RMNET_FLAGS_INGRESS_MAP_CKSUMV5)) {
		if (skb_is_nonlinear(skb)) {
			rmnet_frag_ingress_handler(skb, port);
			return;
		}
	}

	/* No aggregation. Pass the frame on as is */
	if (!(port->data_format & RMNET_FLAGS_INGRESS_DEAGGREGATION)) {
		__rmnet_map_ingress_handler(skb, port);
		return;
	}

	/* Pass off handling to rmnet_perf module, if present */
	rcu_read_lock();
	rmnet_perf_core_deaggregate = rcu_dereference(rmnet_perf_deag_entry);
	if (rmnet_perf_core_deaggregate) {
		rmnet_perf_core_deaggregate(skb, port);
		rcu_read_unlock();
		return;
	}
	rcu_read_unlock();

	/* Deaggregation and freeing of HW originating
	 * buffers is done within here
	 */
	while (skb) {
		struct sk_buff *skb_frag = skb_shinfo(skb)->frag_list;

		skb_shinfo(skb)->frag_list = NULL;
		while ((skbn = rmnet_map_deaggregate(skb, port)) != NULL) {
			__rmnet_map_ingress_handler(skbn, port);

			if (skbn == skb)
				goto next_skb;
		}

		consume_skb(skb);
next_skb:
		skb = skb_frag;
	}
}

static int rmnet_map_egress_handler(struct sk_buff *skb,
				    struct rmnet_port *port, u8 mux_id,
				    struct net_device *orig_dev)
{
	int required_headroom, additional_header_len, csum_type;
	struct rmnet_map_header *map_header;

	additional_header_len = 0;
	required_headroom = sizeof(struct rmnet_map_header);
	csum_type = 0;

	if (port->data_format & RMNET_FLAGS_EGRESS_MAP_CKSUMV3) {
		additional_header_len = sizeof(struct rmnet_map_ul_csum_header);
		csum_type = RMNET_FLAGS_EGRESS_MAP_CKSUMV3;
	} else if (port->data_format & RMNET_FLAGS_EGRESS_MAP_CKSUMV4) {
		additional_header_len = sizeof(struct rmnet_map_ul_csum_header);
		csum_type = RMNET_FLAGS_EGRESS_MAP_CKSUMV4;
	} else if ((port->data_format & RMNET_FLAGS_EGRESS_MAP_CKSUMV5) ||
		   (port->data_format & RMNET_EGRESS_FORMAT_PRIORITY)) {
		additional_header_len = sizeof(struct rmnet_map_v5_csum_header);
		csum_type = RMNET_FLAGS_EGRESS_MAP_CKSUMV5;
	}

	required_headroom += additional_header_len;

	if (skb_headroom(skb) < required_headroom) {
		if (pskb_expand_head(skb, required_headroom, 0, GFP_ATOMIC))
			return -ENOMEM;
	}

#ifdef CONFIG_QCOM_QMI_HELPERS
	if (port->data_format & RMNET_INGRESS_FORMAT_PS)
		qmi_rmnet_work_maybe_restart(port);
#endif

	if (csum_type)
		rmnet_map_checksum_uplink_packet(skb, port, orig_dev,
						 csum_type);

	map_header = rmnet_map_add_map_header(skb, additional_header_len, 0,
					      port);
	if (!map_header)
		return -ENOMEM;

	map_header->mux_id = mux_id;

	if (port->data_format & RMNET_EGRESS_FORMAT_AGGREGATION) {
		if (rmnet_map_tx_agg_skip(skb, required_headroom))
			goto done;

		rmnet_map_tx_aggregate(skb, port);
		return -EINPROGRESS;
	}

done:
	skb->protocol = htons(ETH_P_MAP);
	return 0;
}

static void
rmnet_bridge_handler(struct sk_buff *skb, struct net_device *bridge_dev)
{
	if (bridge_dev) {
		skb->dev = bridge_dev;
		dev_queue_xmit(skb);
	}
}

/* Ingress / Egress Entry Points */

/* Processes packet as per ingress data format for receiving device. Logical
 * endpoint is determined from packet inspection. Packet is then sent to the
 * egress device listed in the logical endpoint configuration.
 */
rx_handler_result_t rmnet_rx_handler(struct sk_buff **pskb)
{
	struct sk_buff *skb = *pskb;
	struct rmnet_port *port;
	struct net_device *dev;

	if (!skb)
		goto done;

	if (skb->pkt_type == PACKET_LOOPBACK)
		return RX_HANDLER_PASS;

	trace_rmnet_low(RMNET_MODULE, RMNET_RCV_FROM_PND, 0xDEF,
			0xDEF, 0xDEF, 0xDEF, NULL, NULL);
	dev = skb->dev;
	port = rmnet_get_port(dev);

	switch (port->rmnet_mode) {
	case RMNET_EPMODE_VND:
		rmnet_map_ingress_handler(skb, port);
		break;
	case RMNET_EPMODE_BRIDGE:
		rmnet_bridge_handler(skb, port->bridge_ep);
		break;
	}

done:
	return RX_HANDLER_CONSUMED;
}
EXPORT_SYMBOL(rmnet_rx_handler);

/* Modifies packet as per logical endpoint configuration and egress data format
 * for egress device configured in logical endpoint. Packet is then transmitted
 * on the egress device.
 */
void rmnet_egress_handler(struct sk_buff *skb)
{
	struct net_device *orig_dev;
	struct rmnet_port *port;
	struct rmnet_priv *priv;
	u8 mux_id;
	int err;
	u32 skb_len;

	trace_rmnet_low(RMNET_MODULE, RMNET_TX_UL_PKT, 0xDEF, 0xDEF, 0xDEF,
			0xDEF, (void *)skb, NULL);
	sk_pacing_shift_update(skb->sk, 8);

	orig_dev = skb->dev;
	priv = netdev_priv(orig_dev);
	skb->dev = priv->real_dev;
	mux_id = priv->mux_id;

	port = rmnet_get_port(skb->dev);
	if (!port)
		goto drop;

	skb_len = skb->len;
	err = rmnet_map_egress_handler(skb, port, mux_id, orig_dev);
	if (err == -ENOMEM) {
		goto drop;
	} else if (err == -EINPROGRESS) {
		rmnet_vnd_tx_fixup(orig_dev, skb_len);
		return;
	}

	rmnet_vnd_tx_fixup(orig_dev, skb_len);

	dev_queue_xmit(skb);
	return;

drop:
	this_cpu_inc(priv->pcpu_stats->stats.tx_drops);
	kfree_skb(skb);
}
