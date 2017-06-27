/*
 * Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/rmnet_data.h>
#include <linux/net_map.h>
#include <linux/netdev_features.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/rmnet_config.h>
#include "rmnet_data_private.h"
#include "rmnet_data_config.h"
#include "rmnet_data_vnd.h"
#include "rmnet_map.h"
#include "rmnet_data_stats.h"
#include "rmnet_data_trace.h"

RMNET_LOG_MODULE(RMNET_DATA_LOGMASK_HANDLER);


void rmnet_egress_handler(struct sk_buff *skb,
			  struct rmnet_logical_ep_conf_s *ep);

#ifdef CONFIG_RMNET_DATA_DEBUG_PKT
unsigned int dump_pkt_rx;
module_param(dump_pkt_rx, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dump_pkt_rx, "Dump packets entering ingress handler");

unsigned int dump_pkt_tx;
module_param(dump_pkt_tx, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dump_pkt_tx, "Dump packets exiting egress handler");
#endif /* CONFIG_RMNET_DATA_DEBUG_PKT */

/* Time in nano seconds. This number must be less that a second. */
long gro_flush_time __read_mostly = 10000L;
module_param(gro_flush_time, long, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(gro_flush_time, "Flush GRO when spaced more than this");

unsigned int gro_min_byte_thresh __read_mostly = 7500;
module_param(gro_min_byte_thresh, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(gro_min_byte_thresh, "Min byte thresh to change flush time");

unsigned int dynamic_gro_on __read_mostly = 1;
module_param(dynamic_gro_on, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(dynamic_gro_on, "Toggle to turn on dynamic gro logic");

unsigned int upper_flush_time __read_mostly = 15000;
module_param(upper_flush_time, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(upper_flush_time, "Upper limit on flush time");

unsigned int upper_byte_limit __read_mostly = 10500;
module_param(upper_byte_limit, uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(upper_byte_limit, "Upper byte limit");

#define RMNET_DATA_IP_VERSION_4 0x40
#define RMNET_DATA_IP_VERSION_6 0x60

#define RMNET_DATA_GRO_RCV_FAIL 0
#define RMNET_DATA_GRO_RCV_PASS 1

/* ***************** Helper Functions *************************************** */

/**
 * __rmnet_data_set_skb_proto() - Set skb->protocol field
 * @skb:      packet being modified
 *
 * Peek at the first byte of the packet and set the protocol. There is not
 * good way to determine if a packet has a MAP header. As of writing this,
 * the reserved bit in the MAP frame will prevent it from overlapping with
 * IPv4/IPv6 frames. This could change in the future!
 */
static inline void __rmnet_data_set_skb_proto(struct sk_buff *skb)
{
	switch (skb->data[0] & 0xF0) {
	case RMNET_DATA_IP_VERSION_4:
		skb->protocol = htons(ETH_P_IP);
		break;
	case RMNET_DATA_IP_VERSION_6:
		skb->protocol = htons(ETH_P_IPV6);
		break;
	default:
		skb->protocol = htons(ETH_P_MAP);
		break;
	}
}

#ifdef CONFIG_RMNET_DATA_DEBUG_PKT
/**
 * rmnet_print_packet() - Print packet / diagnostics
 * @skb:      Packet to print
 * @printlen: Number of bytes to print
 * @dev:      Name of interface
 * @dir:      Character representing direction (e.g.. 'r' for receive)
 *
 * This function prints out raw bytes in an SKB. Use of this will have major
 * performance impacts and may even trigger watchdog resets if too much is being
 * printed. Hence, this should always be compiled out unless absolutely needed.
 */
void rmnet_print_packet(const struct sk_buff *skb, const char *dev, char dir)
{
	char buffer[200];
	unsigned int len, printlen;
	int i, buffloc = 0;

	switch (dir) {
	case 'r':
		printlen = dump_pkt_rx;
		break;

	case 't':
		printlen = dump_pkt_tx;
		break;

	default:
		printlen = 0;
		break;
	}

	if (!printlen)
		return;

	pr_err("[%s][%c] - PKT skb->len=%d skb->head=%pK skb->data=%pK\n",
	       dev, dir, skb->len, (void *)skb->head, (void *)skb->data);
	pr_err("[%s][%c] - PKT skb->tail=%pK skb->end=%pK\n",
	       dev, dir, skb_tail_pointer(skb), skb_end_pointer(skb));

	if (skb->len > 0)
		len = skb->len;
	else
		len = ((unsigned int)(uintptr_t)skb->end) -
		      ((unsigned int)(uintptr_t)skb->data);

	pr_err("[%s][%c] - PKT len: %d, printing first %d bytes\n",
		dev, dir, len, printlen);

	memset(buffer, 0, sizeof(buffer));
	for (i = 0; (i < printlen) && (i < len); i++) {
		if ((i%16) == 0) {
			pr_err("[%s][%c] - PKT%s\n", dev, dir, buffer);
			memset(buffer, 0, sizeof(buffer));
			buffloc = 0;
			buffloc += snprintf(&buffer[buffloc],
					sizeof(buffer)-buffloc, "%04X:",
					i);
		}

		buffloc += snprintf(&buffer[buffloc], sizeof(buffer)-buffloc,
					" %02x", skb->data[i]);

	}
	pr_err("[%s][%c] - PKT%s\n", dev, dir, buffer);
}
#else
void rmnet_print_packet(const struct sk_buff *skb, const char *dev, char dir)
{
	return;
}
#endif /* CONFIG_RMNET_DATA_DEBUG_PKT */

/* ***************** Generic handler **************************************** */

/**
 * rmnet_bridge_handler() - Bridge related functionality
 *
 * Return:
 *      - RX_HANDLER_CONSUMED in all cases
 */
static rx_handler_result_t rmnet_bridge_handler(struct sk_buff *skb,
					struct rmnet_logical_ep_conf_s *ep)
{
	if (!ep->egress_dev) {
		LOGD("Missing egress device for packet arriving on %s",
		     skb->dev->name);
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_BRDG_NO_EGRESS);
	} else {
		rmnet_egress_handler(skb, ep);
	}

	return RX_HANDLER_CONSUMED;
}

#ifdef NET_SKBUFF_DATA_USES_OFFSET
static void rmnet_reset_mac_header(struct sk_buff *skb)
{
	skb->mac_header = 0;
	skb->mac_len = 0;
}
#else
static void rmnet_reset_mac_header(struct sk_buff *skb)
{
	skb->mac_header = skb->network_header;
	skb->mac_len = 0;
}
#endif /*NET_SKBUFF_DATA_USES_OFFSET*/

/**
 * rmnet_check_skb_can_gro() - Check is skb can be passed through GRO handler
 *
 * Determines whether to pass the skb to the GRO handler napi_gro_receive() or
 * handle normally by passing to netif_receive_skb().
 *
 * Warning:
 * This assumes that only TCP packets can be coalesced by the GRO handler which
 * is not true in general. We lose the ability to use GRO for cases like UDP
 * encapsulation protocols.
 *
 * Return:
 *      - RMNET_DATA_GRO_RCV_FAIL if packet is sent to netif_receive_skb()
 *      - RMNET_DATA_GRO_RCV_PASS if packet is sent to napi_gro_receive()
 */
static int rmnet_check_skb_can_gro(struct sk_buff *skb)
{
	switch (skb->data[0] & 0xF0) {
	case RMNET_DATA_IP_VERSION_4:
		if (ip_hdr(skb)->protocol == IPPROTO_TCP)
			return RMNET_DATA_GRO_RCV_PASS;
		break;
	case RMNET_DATA_IP_VERSION_6:
		if (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
			return RMNET_DATA_GRO_RCV_PASS;
		/* Fall through */
	}

	return RMNET_DATA_GRO_RCV_FAIL;
}

/**
 * rmnet_optional_gro_flush() - Check if GRO handler needs to flush now
 *
 * Determines whether GRO handler needs to flush packets which it has
 * coalesced so far.
 *
 * Tuning this parameter will trade TCP slow start performance for GRO coalesce
 * ratio.
 */
static void rmnet_optional_gro_flush(struct napi_struct *napi,
				     struct rmnet_logical_ep_conf_s *ep,
					 unsigned int skb_size)
{
	struct timespec curr_time, diff;

	if (!gro_flush_time)
		return;

	if (unlikely(ep->flush_time.tv_sec == 0)) {
		getnstimeofday(&ep->flush_time);
		ep->flush_byte_count = 0;
	} else {
		getnstimeofday(&(curr_time));
		diff = timespec_sub(curr_time, ep->flush_time);
		ep->flush_byte_count += skb_size;

		if (dynamic_gro_on) {
			if ((!(diff.tv_sec > 0) || diff.tv_nsec <=
					gro_flush_time) &&
					ep->flush_byte_count >=
					gro_min_byte_thresh) {
				/* Processed many bytes in a small time window.
				 * No longer need to flush so often and we can
				 * increase our byte limit
				 */
				gro_flush_time = upper_flush_time;
				gro_min_byte_thresh = upper_byte_limit;
			} else if ((diff.tv_sec > 0 ||
					diff.tv_nsec > gro_flush_time) &&
					ep->flush_byte_count <
					gro_min_byte_thresh) {
				/* We have not hit our time limit and we are not
				 * receive many bytes. Demote ourselves to the
				 * lowest limits and flush
				 */
				napi_gro_flush(napi, false);
				getnstimeofday(&ep->flush_time);
				ep->flush_byte_count = 0;
				gro_flush_time = 10000L;
				gro_min_byte_thresh = 7500L;
			} else if ((diff.tv_sec > 0 ||
					diff.tv_nsec > gro_flush_time) &&
					ep->flush_byte_count >=
					gro_min_byte_thresh) {
				/* Above byte and time limt, therefore we can
				 * move/maintain our limits to be the max
				 * and flush
				 */
				napi_gro_flush(napi, false);
				getnstimeofday(&ep->flush_time);
				ep->flush_byte_count = 0;
				gro_flush_time = upper_flush_time;
				gro_min_byte_thresh = upper_byte_limit;
			}
			/* else, below time limit and below
			 * byte thresh, so change nothing
			 */
		} else if (diff.tv_sec > 0 ||
				diff.tv_nsec >= gro_flush_time) {
			napi_gro_flush(napi, false);
			getnstimeofday(&ep->flush_time);
			ep->flush_byte_count = 0;
		}
	}
}

/**
 * __rmnet_deliver_skb() - Deliver skb
 *
 * Determines where to deliver skb. Options are: consume by network stack,
 * pass to bridge handler, or pass to virtual network device
 *
 * Return:
 *      - RX_HANDLER_CONSUMED if packet forwarded or dropped
 *      - RX_HANDLER_PASS if packet is to be consumed by network stack as-is
 */
static rx_handler_result_t __rmnet_deliver_skb(struct sk_buff *skb,
					 struct rmnet_logical_ep_conf_s *ep)
{
	struct napi_struct *napi = NULL;
	gro_result_t gro_res;
	unsigned int skb_size;

	trace___rmnet_deliver_skb(skb);
	switch (ep->rmnet_mode) {
	case RMNET_EPMODE_NONE:
		return RX_HANDLER_PASS;

	case RMNET_EPMODE_BRIDGE:
		return rmnet_bridge_handler(skb, ep);

	case RMNET_EPMODE_VND:
		skb_reset_transport_header(skb);
		skb_reset_network_header(skb);
		switch (rmnet_vnd_rx_fixup(skb, skb->dev)) {
		case RX_HANDLER_CONSUMED:
			return RX_HANDLER_CONSUMED;

		case RX_HANDLER_PASS:
			skb->pkt_type = PACKET_HOST;
			rmnet_reset_mac_header(skb);
			if (rmnet_check_skb_can_gro(skb) &&
			    (skb->dev->features & NETIF_F_GRO)) {
				napi = get_current_napi_context();
				if (napi != NULL) {
					skb_size = skb->len;
					gro_res = napi_gro_receive(napi, skb);
					trace_rmnet_gro_downlink(gro_res);
					rmnet_optional_gro_flush(
								napi, ep,
								skb_size);
				} else {
					WARN_ONCE(1, "current napi is NULL\n");
					netif_receive_skb(skb);
				}
			} else {
				netif_receive_skb(skb);
			}
			return RX_HANDLER_CONSUMED;
		}
		return RX_HANDLER_PASS;

	default:
		LOGD("Unkown ep mode %d", ep->rmnet_mode);
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_DELIVER_NO_EP);
		return RX_HANDLER_CONSUMED;
	}
}

/**
 * rmnet_ingress_deliver_packet() - Ingress handler for raw IP and bridged
 *                                  MAP packets.
 * @skb:     Packet needing a destination.
 * @config:  Physical end point configuration that the packet arrived on.
 *
 * Return:
 *      - RX_HANDLER_CONSUMED if packet forwarded/dropped
 *      - RX_HANDLER_PASS if packet should be passed up the stack by caller
 */
static rx_handler_result_t rmnet_ingress_deliver_packet(struct sk_buff *skb,
					   struct rmnet_phys_ep_config *config)
{
	if (!config) {
		LOGD("%s", "NULL physical EP provided");
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

	if (!(config->local_ep.refcount)) {
		LOGD("Packet on %s has no local endpoint configuration",
			skb->dev->name);
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_IPINGRESS_NO_EP);
		return RX_HANDLER_CONSUMED;
	}

	skb->dev = config->local_ep.egress_dev;

	return __rmnet_deliver_skb(skb, &(config->local_ep));
}

/* ***************** MAP handler ******************************************** */

/**
 * _rmnet_map_ingress_handler() - Actual MAP ingress handler
 * @skb:        Packet being received
 * @config:     Physical endpoint configuration for the ingress device
 *
 * Most MAP ingress functions are processed here. Packets are processed
 * individually; aggregated packets should use rmnet_map_ingress_handler()
 *
 * Return:
 *      - RX_HANDLER_CONSUMED if packet is dropped
 *      - result of __rmnet_deliver_skb() for all other cases
 */
static rx_handler_result_t _rmnet_map_ingress_handler(struct sk_buff *skb,
					   struct rmnet_phys_ep_config *config)
{
	struct rmnet_logical_ep_conf_s *ep;
	uint8_t mux_id;
	uint16_t len;
	int ckresult;

	if (RMNET_MAP_GET_CD_BIT(skb)) {
		if (config->ingress_data_format
		    & RMNET_INGRESS_FORMAT_MAP_COMMANDS)
			return rmnet_map_command(skb, config);

		LOGM("MAP command packet on %s; %s", skb->dev->name,
		     "Not configured for MAP commands");
		rmnet_kfree_skb(skb,
				RMNET_STATS_SKBFREE_INGRESS_NOT_EXPECT_MAPC);
		return RX_HANDLER_CONSUMED;
	}

	mux_id = RMNET_MAP_GET_MUX_ID(skb);
	len = RMNET_MAP_GET_LENGTH(skb)
			- RMNET_MAP_GET_PAD(skb)
			- config->tail_spacing;

	if (mux_id >= RMNET_DATA_MAX_LOGICAL_EP) {
		LOGD("Got packet on %s with bad mux id %d",
			skb->dev->name, mux_id);
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_MAPINGRESS_BAD_MUX);
			return RX_HANDLER_CONSUMED;
	}

	ep = &(config->muxed_ep[mux_id]);

	if (!ep->refcount) {
		LOGD("Packet on %s:%d; has no logical endpoint config",
		     skb->dev->name, mux_id);

		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_MAPINGRESS_MUX_NO_EP);
		return RX_HANDLER_CONSUMED;
	}

	if (config->ingress_data_format & RMNET_INGRESS_FORMAT_DEMUXING)
		skb->dev = ep->egress_dev;

	if ((config->ingress_data_format & RMNET_INGRESS_FORMAT_MAP_CKSUMV3) ||
	    (config->ingress_data_format & RMNET_INGRESS_FORMAT_MAP_CKSUMV4)) {
		ckresult = rmnet_map_checksum_downlink_packet(skb);
		trace_rmnet_map_checksum_downlink_packet(skb, ckresult);
		rmnet_stats_dl_checksum(ckresult);
		if (likely((ckresult == RMNET_MAP_CHECKSUM_OK)
			    || (ckresult == RMNET_MAP_CHECKSUM_SKIPPED)))
			skb->ip_summed |= CHECKSUM_UNNECESSARY;
		else if (ckresult !=
				    RMNET_MAP_CHECKSUM_ERR_UNKNOWN_IP_VERSION &&
			 ckresult != RMNET_MAP_CHECKSUM_VALIDATION_FAILED &&
			 ckresult != RMNET_MAP_CHECKSUM_ERR_UNKNOWN_TRANSPORT &&
			 ckresult != RMNET_MAP_CHECKSUM_VALID_FLAG_NOT_SET &&
			 ckresult != RMNET_MAP_CHECKSUM_FRAGMENTED_PACKET) {
			rmnet_kfree_skb(skb,
				RMNET_STATS_SKBFREE_INGRESS_BAD_MAP_CKSUM);
			return RX_HANDLER_CONSUMED;
		}
	}

	/* Subtract MAP header */
	skb_pull(skb, sizeof(struct rmnet_map_header_s));
	skb_trim(skb, len);
	__rmnet_data_set_skb_proto(skb);
	return __rmnet_deliver_skb(skb, ep);
}

/**
 * rmnet_map_ingress_handler() - MAP ingress handler
 * @skb:        Packet being received
 * @config:     Physical endpoint configuration for the ingress device
 *
 * Called if and only if MAP is configured in the ingress device's ingress data
 * format. Deaggregation is done here, actual MAP processing is done in
 * _rmnet_map_ingress_handler().
 *
 * Return:
 *      - RX_HANDLER_CONSUMED for aggregated packets
 *      - RX_HANDLER_CONSUMED for dropped packets
 *      - result of _rmnet_map_ingress_handler() for all other cases
 */
static rx_handler_result_t rmnet_map_ingress_handler(struct sk_buff *skb,
					   struct rmnet_phys_ep_config *config)
{
	struct sk_buff *skbn;
	int rc, co = 0;

	if (config->ingress_data_format & RMNET_INGRESS_FORMAT_DEAGGREGATION) {
		trace_rmnet_start_deaggregation(skb);
		while ((skbn = rmnet_map_deaggregate(skb, config)) != 0) {
			_rmnet_map_ingress_handler(skbn, config);
			co++;
		}
		trace_rmnet_end_deaggregation(skb, co);
		LOGD("De-aggregated %d packets", co);
		rmnet_stats_deagg_pkts(co);
		rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_MAPINGRESS_AGGBUF);
		rc = RX_HANDLER_CONSUMED;
	} else {
		rc = _rmnet_map_ingress_handler(skb, config);
	}

	return rc;
}

/**
 * rmnet_map_egress_handler() - MAP egress handler
 * @skb:        Packet being sent
 * @config:     Physical endpoint configuration for the egress device
 * @ep:         logical endpoint configuration of the packet originator
 *              (e.g.. RmNet virtual network device)
 * @orig_dev:   The originator vnd device
 *
 * Called if and only if MAP is configured in the egress device's egress data
 * format. Will expand skb if there is insufficient headroom for MAP protocol.
 * Note: headroomexpansion will incur a performance penalty.
 *
 * Return:
 *      - 0 on success
 *      - 1 on failure
 */
static int rmnet_map_egress_handler(struct sk_buff *skb,
				    struct rmnet_phys_ep_config *config,
				    struct rmnet_logical_ep_conf_s *ep,
				    struct net_device *orig_dev)
{
	int required_headroom, additional_header_length, ckresult;
	struct rmnet_map_header_s *map_header;

	additional_header_length = 0;

	required_headroom = sizeof(struct rmnet_map_header_s);
	if ((config->egress_data_format & RMNET_EGRESS_FORMAT_MAP_CKSUMV3) ||
	    (config->egress_data_format & RMNET_EGRESS_FORMAT_MAP_CKSUMV4)) {
		required_headroom +=
			sizeof(struct rmnet_map_ul_checksum_header_s);
		additional_header_length +=
			sizeof(struct rmnet_map_ul_checksum_header_s);
	}

	LOGD("headroom of %d bytes", required_headroom);

	if (skb_headroom(skb) < required_headroom) {
		LOGE("Not enough headroom for %d bytes", required_headroom);
		kfree_skb(skb);
		return 1;
	}

	if ((config->egress_data_format & RMNET_EGRESS_FORMAT_MAP_CKSUMV3) ||
	    (config->egress_data_format & RMNET_EGRESS_FORMAT_MAP_CKSUMV4)) {
		ckresult = rmnet_map_checksum_uplink_packet
				(skb, orig_dev, config->egress_data_format);
		trace_rmnet_map_checksum_uplink_packet(orig_dev, ckresult);
		rmnet_stats_ul_checksum(ckresult);
	}

	if ((!(config->egress_data_format &
	    RMNET_EGRESS_FORMAT_AGGREGATION)) ||
	    ((orig_dev->features & NETIF_F_GSO) && skb_is_nonlinear(skb)))
		map_header = rmnet_map_add_map_header
		(skb, additional_header_length, RMNET_MAP_NO_PAD_BYTES);
	else
		map_header = rmnet_map_add_map_header
		(skb, additional_header_length, RMNET_MAP_ADD_PAD_BYTES);

	if (!map_header) {
		LOGD("%s", "Failed to add MAP header to egress packet");
		kfree_skb(skb);
		return 1;
	}

	if (config->egress_data_format & RMNET_EGRESS_FORMAT_MUXING) {
		if (ep->mux_id == 0xff)
			map_header->mux_id = 0;
		else
			map_header->mux_id = ep->mux_id;
	}

	skb->protocol = htons(ETH_P_MAP);

	if (config->egress_data_format & RMNET_EGRESS_FORMAT_AGGREGATION) {
		rmnet_map_aggregate(skb, config);
		return RMNET_MAP_CONSUMED;
	}

	return RMNET_MAP_SUCCESS;
}
/* ***************** Ingress / Egress Entry Points ************************** */

/**
 * rmnet_ingress_handler() - Ingress handler entry point
 * @skb: Packet being received
 *
 * Processes packet as per ingress data format for receiving device. Logical
 * endpoint is determined from packet inspection. Packet is then sent to the
 * egress device listed in the logical endpoint configuration.
 *
 * Return:
 *      - RX_HANDLER_PASS if packet is not processed by handler (caller must
 *        deal with the packet)
 *      - RX_HANDLER_CONSUMED if packet is forwarded or processed by MAP
 */
rx_handler_result_t rmnet_ingress_handler(struct sk_buff *skb)
{
	struct rmnet_phys_ep_config *config;
	struct net_device *dev;
	int rc;

	if (!skb)
		BUG();

	dev = skb->dev;
	trace_rmnet_ingress_handler(skb);
	rmnet_print_packet(skb, dev->name, 'r');

	config = _rmnet_get_phys_ep_config(skb->dev);

	if (!config) {
		LOGD("%s is not associated with rmnet_data", skb->dev->name);
		kfree_skb(skb);
		return RX_HANDLER_CONSUMED;
	}

	/* Sometimes devices operate in ethernet mode even thouth there is no
	 * ethernet header. This causes the skb->protocol to contain a bogus
	 * value and the skb->data pointer to be off by 14 bytes. Fix it if
	 * configured to do so
	 */
	if (config->ingress_data_format & RMNET_INGRESS_FIX_ETHERNET) {
		skb_push(skb, RMNET_ETHERNET_HEADER_LENGTH);
		__rmnet_data_set_skb_proto(skb);
	}

	if (config->ingress_data_format & RMNET_INGRESS_FORMAT_MAP) {
			rc = rmnet_map_ingress_handler(skb, config);
	} else {
		switch (ntohs(skb->protocol)) {
		case ETH_P_MAP:
			if (config->local_ep.rmnet_mode ==
				RMNET_EPMODE_BRIDGE) {
				rc = rmnet_ingress_deliver_packet(skb, config);
			} else {
				LOGD("MAP packet on %s; MAP not set",
					dev->name);
				rmnet_kfree_skb(skb,
				   RMNET_STATS_SKBFREE_INGRESS_NOT_EXPECT_MAPD);
				rc = RX_HANDLER_CONSUMED;
			}
			break;

		case ETH_P_ARP:
		case ETH_P_IP:
		case ETH_P_IPV6:
			rc = rmnet_ingress_deliver_packet(skb, config);
			break;

		default:
			LOGD("Unknown skb->proto 0x%04X\n",
				ntohs(skb->protocol) & 0xFFFF);
			rc = RX_HANDLER_PASS;
		}
	}

	return rc;
}

/**
 * rmnet_rx_handler() - Rx handler callback registered with kernel
 * @pskb: Packet to be processed by rx handler
 *
 * Standard kernel-expected footprint for rx handlers. Calls
 * rmnet_ingress_handler with correctly formatted arguments
 *
 * Return:
 *      - Whatever rmnet_ingress_handler() returns
 */
rx_handler_result_t rmnet_rx_handler(struct sk_buff **pskb)
{
	return rmnet_ingress_handler(*pskb);
}

/**
 * rmnet_egress_handler() - Egress handler entry point
 * @skb:        packet to transmit
 * @ep:         logical endpoint configuration of the packet originator
 *              (e.g.. RmNet virtual network device)
 *
 * Modifies packet as per logical endpoint configuration and egress data format
 * for egress device configured in logical endpoint. Packet is then transmitted
 * on the egress device.
 */
void rmnet_egress_handler(struct sk_buff *skb,
			  struct rmnet_logical_ep_conf_s *ep)
{
	struct rmnet_phys_ep_config *config;
	struct net_device *orig_dev;
	int rc;
	orig_dev = skb->dev;
	skb->dev = ep->egress_dev;

	config = _rmnet_get_phys_ep_config(skb->dev);

	if (!config) {
		LOGD("%s is not associated with rmnet_data", skb->dev->name);
		kfree_skb(skb);
		return;
	}

	LOGD("Packet going out on %s with egress format 0x%08X",
	     skb->dev->name, config->egress_data_format);

	if (config->egress_data_format & RMNET_EGRESS_FORMAT_MAP) {
		switch (rmnet_map_egress_handler(skb, config, ep, orig_dev)) {
		case RMNET_MAP_CONSUMED:
			LOGD("%s", "MAP process consumed packet");
			return;

		case RMNET_MAP_SUCCESS:
			break;

		default:
			LOGD("MAP egress failed on packet on %s",
			     skb->dev->name);
			rmnet_kfree_skb(skb, RMNET_STATS_SKBFREE_EGR_MAPFAIL);
			return;
		}
	}

	if (ep->rmnet_mode == RMNET_EPMODE_VND)
		rmnet_vnd_tx_fixup(skb, orig_dev);

	rmnet_print_packet(skb, skb->dev->name, 't');
	trace_rmnet_egress_handler(skb);
	rc = dev_queue_xmit(skb);
	if (rc != 0) {
		LOGD("Failed to queue packet for transmission on [%s]",
		      skb->dev->name);
	}
	rmnet_stats_queue_xmit(rc, RMNET_STATS_QUEUE_XMIT_EGRESS);
}
