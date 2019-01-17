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
#ifdef CONFIG_QCOM_QMI_HELPERS
#include <soc/qcom/rmnet_qmi.h>
#include <soc/qcom/qmi_rmnet.h>

#endif

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

/* Helper Functions */

static int rmnet_check_skb_can_gro(struct sk_buff *skb)
{
	switch(skb->protocol) {
	case htons(ETH_P_IP):
		if (ip_hdr(skb)->protocol == IPPROTO_TCP)
			return 0;
		break;
	case htons(ETH_P_IPV6):
		if (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
			return 0;
		/* Fall through */
	}

	return -EPROTONOSUPPORT;
}

void rmnet_set_skb_proto(struct sk_buff *skb)
{
	switch (skb->data[0] & 0xF0) {
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

	rmnet_shs_stamp = rcu_dereference(rmnet_shs_skb_entry);
	if (rmnet_shs_stamp) {
		rmnet_shs_stamp(skb, port);
		return;
	}

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
	rmnet_shs_stamp = (!ctx) ? rcu_dereference(rmnet_shs_skb_entry) :
				   rcu_dereference(rmnet_shs_skb_entry_wq);
	if (rmnet_shs_stamp) {
		rmnet_shs_stamp(skb, port);
		return;
	}

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

/* MAP handler */

static void
__rmnet_map_ingress_handler(struct sk_buff *skb,
			    struct rmnet_port *port)
{
	struct rmnet_endpoint *ep;
	u16 len, pad;
	u8 mux_id;

	if (RMNET_MAP_GET_CD_BIT(skb)) {
		qmi_rmnet_set_dl_msg_active(port);
		if (port->data_format & RMNET_INGRESS_FORMAT_DL_MARKER) {
			if (!rmnet_map_flow_command(skb, port, false))
				return;
		}

		if (port->data_format & RMNET_FLAGS_INGRESS_MAP_COMMANDS)
			return rmnet_map_command(skb, port);

		goto free_skb;
	}

	mux_id = RMNET_MAP_GET_MUX_ID(skb);
	pad = RMNET_MAP_GET_PAD(skb);
	len = RMNET_MAP_GET_LENGTH(skb) - pad;

	if (mux_id >= RMNET_MAX_LOGICAL_EP)
		goto free_skb;

	ep = rmnet_get_endpoint(port, mux_id);
	if (!ep)
		goto free_skb;

	skb->dev = ep->egress_dev;

	/* Subtract MAP header */
	skb_pull(skb, sizeof(struct rmnet_map_header));
	rmnet_set_skb_proto(skb);

	if (port->data_format & RMNET_FLAGS_INGRESS_MAP_CKSUMV4) {
		if (!rmnet_map_checksum_downlink_packet(skb, len + pad))
			skb->ip_summed = CHECKSUM_UNNECESSARY;
	}

	if (port->data_format & RMNET_INGRESS_FORMAT_PS)
		qmi_rmnet_work_maybe_restart(port);

	skb_trim(skb, len);
	rmnet_deliver_skb(skb, port);
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

	if (skb->dev->type == ARPHRD_ETHER) {
		if (pskb_expand_head(skb, ETH_HLEN, 0, GFP_KERNEL)) {
			kfree_skb(skb);
			return;
		}

		skb_push(skb, ETH_HLEN);
	}

	if (port->data_format & RMNET_FLAGS_INGRESS_DEAGGREGATION) {
		int (*rmnet_perf_core_deaggregate)(struct sk_buff *skb,
						   struct rmnet_port *port);
		/* Deaggregation and freeing of HW originating
		 * buffers is done within here
		 */
		rmnet_perf_core_deaggregate =
					rcu_dereference(rmnet_perf_deag_entry);
		if (rmnet_perf_core_deaggregate) {
			rmnet_perf_core_deaggregate(skb, port);
		} else {
			struct sk_buff *skbn;

			while (skb) {
				struct sk_buff *skb_frag =
						skb_shinfo(skb)->frag_list;

				skb_shinfo(skb)->frag_list = NULL;
				while ((skbn = rmnet_map_deaggregate(skb, port))
					!= NULL)
					__rmnet_map_ingress_handler(skbn, port);
				consume_skb(skb);
				skb = skb_frag;
			}
		}
	} else {
		__rmnet_map_ingress_handler(skb, port);
	}
}

static int rmnet_map_egress_handler(struct sk_buff *skb,
				    struct rmnet_port *port, u8 mux_id,
				    struct net_device *orig_dev)
{
	int required_headroom, additional_header_len;
	struct rmnet_map_header *map_header;

	additional_header_len = 0;
	required_headroom = sizeof(struct rmnet_map_header);

	if (port->data_format & RMNET_FLAGS_EGRESS_MAP_CKSUMV4) {
		additional_header_len = sizeof(struct rmnet_map_ul_csum_header);
		required_headroom += additional_header_len;
	}

	if (skb_headroom(skb) < required_headroom) {
		if (pskb_expand_head(skb, required_headroom, 0, GFP_ATOMIC))
			return -ENOMEM;
	}

	if (port->data_format & RMNET_FLAGS_EGRESS_MAP_CKSUMV4)
		rmnet_map_checksum_uplink_packet(skb, orig_dev);

	map_header = rmnet_map_add_map_header(skb, additional_header_len, 0);
	if (!map_header)
		return -ENOMEM;

	map_header->mux_id = mux_id;

	if (port->data_format & RMNET_EGRESS_FORMAT_AGGREGATION) {
		int non_linear_skb;

		if (rmnet_map_tx_agg_skip(skb, required_headroom))
			goto done;

		non_linear_skb = (orig_dev->features & NETIF_F_GSO) &&
				 skb_is_nonlinear(skb);

		if (non_linear_skb) {
			if (unlikely(__skb_linearize(skb)))
				goto done;
		}

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
	if (err == -ENOMEM)
		goto drop;
	else if (err == -EINPROGRESS) {
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
