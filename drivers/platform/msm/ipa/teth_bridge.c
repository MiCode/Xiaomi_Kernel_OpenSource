/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/completion.h>
#include <linux/debugfs.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/if_ether.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/msm_ipa.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <mach/bam_dmux.h>
#include <mach/ipa.h>
#include <mach/sps.h>
#include "ipa_i.h"

#define TETH_BRIDGE_DRV_NAME "ipa_tethering_bridge"

#define TETH_DBG(fmt, args...) \
	pr_debug(TETH_BRIDGE_DRV_NAME " %s:%d " fmt, \
		 __func__, __LINE__, ## args)
#define TETH_DBG_FUNC_ENTRY() \
	pr_debug(TETH_BRIDGE_DRV_NAME " %s:%d ENTRY\n", __func__, __LINE__)
#define TETH_DBG_FUNC_EXIT() \
	pr_debug(TETH_BRIDGE_DRV_NAME " %s:%d EXIT\n", __func__, __LINE__)
#define TETH_ERR(fmt, args...) \
	pr_err(TETH_BRIDGE_DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define USB_ETH_HDR_NAME_IPV4 "usb_bridge_ipv4"
#define USB_ETH_HDR_NAME_IPV6 "usb_bridge_ipv6"
#define A2_ETH_HDR_NAME_IPV4  "a2_bridge_ipv4"
#define A2_ETH_HDR_NAME_IPV6  "a2_bridge_ipv6"

#define USB_TO_A2_RT_TBL_NAME_IPV4 "usb_a2_rt_tbl_ipv4"
#define A2_TO_USB_RT_TBL_NAME_IPV4 "a2_usb_rt_tbl_ipv4"
#define USB_TO_A2_RT_TBL_NAME_IPV6 "usb_a2_rt_tbl_ipv6"
#define A2_TO_USB_RT_TBL_NAME_IPV6 "a2_usb_rt_tbl_ipv6"

#define MBIM_HEADER_NAME "mbim_header"
#define TETH_DEFAULT_AGGR_TIME_LIMIT 1

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_IPV6 0x86DD

#define TETH_AGGR_MAX_DATAGRAMS_DEFAULT 16
#define TETH_AGGR_MAX_AGGR_PACKET_SIZE_DEFAULT (8*1024)

#define TETH_MTU_BYTE 1500

#define TETH_INACTIVITY_TIME_MSEC (1000)

#define TETH_WORKQUEUE_NAME "tethering_bridge_wq"

#define TETH_TOTAL_HDR_ENTRIES 6
#define TETH_TOTAL_RT_ENTRIES_IP 3
#define TETH_TOTAL_FLT_ENTRIES_IP 2
#define TETH_IP_FAMILIES 2

/**
 * struct mac_addresses_type - store host PC and device MAC addresses
 * @host_pc_mac_addr: MAC address of the host PC
 * @host_pc_mac_addr_known: is the MAC address of the host PC known ?
 * @device_mac_addr: MAC address of the device
 * @device_mac_addr_known: is the MAC address of the device known ?
 */
struct mac_addresses_type {
	u8 host_pc_mac_addr[ETH_ALEN];
	bool host_pc_mac_addr_known;
	u8 device_mac_addr[ETH_ALEN];
	bool device_mac_addr_known;
};

/**
 * struct stats - driver statistics, viewable using debugfs
 * @a2_to_usb_num_sw_tx_packets: number of packets bridged from A2 to USB using
 * the SW bridge
 * @usb_to_a2_num_sw_tx_packets: number of packets bridged from USB to A2 using
 * the SW bridge
 * @num_sw_tx_packets_during_resource_wakeup: number of packets bridged during a
 * resource wakeup period, there is a special treatment for these kind of
 * packets
 */
struct stats {
	u64 a2_to_usb_num_sw_tx_packets;
	u64 usb_to_a2_num_sw_tx_packets;
	u64 num_sw_tx_packets_during_resource_wakeup;
};

/**
 * struct teth_bridge_ctx - Tethering bridge driver context information
 * @class: kernel class pointer
 * @dev_num: kernel device number
 * @dev: kernel device struct pointer
 * @cdev: kernel character device struct
 * @usb_ipa_pipe_hdl: USB to IPA pipe handle
 * @ipa_usb_pipe_hdl: IPA to USB pipe handle
 * @a2_ipa_pipe_hdl: A2 to IPA pipe handle
 * @ipa_a2_pipe_hdl: IPA to A2 pipe handle
 * @is_connected: is the tethered bridge connected ?
 * @link_protocol: IP / Ethernet
 * @mac_addresses: Struct which holds host pc and device MAC addresses, relevant
 * in ethernet mode only
 * @is_hw_bridge_complete: is HW bridge setup ?
 * @aggr_params: aggregation parmeters
 * @aggr_params_known: are the aggregation parameters known ?
 * @tethering_mode: Rmnet / MBIM
 * @is_bridge_prod_up: completion object signaled when the bridge producer
 * finished its resource request procedure
 * @is_bridge_prod_down: completion object signaled when the bridge producer
 * finished its resource release procedure
 * @comp_hw_bridge_work: used for setting up the HW bridge using a workqueue
 * @comp_hw_bridge_in_progress: true when the HW bridge setup is in progress
 * @aggr_caps: aggregation capabilities
 * @stats: statistics, how many packets were transmitted using the SW bridge
 * @teth_wq: dedicated workqueue, used for setting up the HW bridge and for
 * sending packets using the SW bridge when the system is waking up from power
 * collapse
 * @a2_ipa_hdr_len: A2 to IPA header length, used for configuring the A2
 * endpoint for header removal
 * @ipa_a2_hdr_len: IPA to A2 header length, used for configuring the A2
 * endpoint for header removal
 * @hdr_del: array to store the headers handles in order to delete them later
 * @routing_del: array of routing rules handles, one array for IPv4 and one for
 * IPv6
 * @filtering_del: array of routing rules handles, one array for IPv4 and one
 * for IPv6
 */
struct teth_bridge_ctx {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	u32 usb_ipa_pipe_hdl;
	u32 ipa_usb_pipe_hdl;
	u32 a2_ipa_pipe_hdl;
	u32 ipa_a2_pipe_hdl;
	bool is_connected;
	enum teth_link_protocol_type link_protocol;
	struct mac_addresses_type mac_addresses;
	bool is_hw_bridge_complete;
	struct teth_aggr_params aggr_params;
	bool aggr_params_known;
	enum teth_tethering_mode tethering_mode;
	struct completion is_bridge_prod_up;
	struct completion is_bridge_prod_down;
	struct work_struct comp_hw_bridge_work;
	bool comp_hw_bridge_in_progress;
	struct teth_aggr_capabilities *aggr_caps;
	struct stats stats;
	struct workqueue_struct *teth_wq;
	u16 a2_ipa_hdr_len;
	u16 ipa_a2_hdr_len;
	struct ipa_ioc_del_hdr *hdr_del;
	struct ipa_ioc_del_rt_rule *routing_del[TETH_IP_FAMILIES];
	struct ipa_ioc_del_flt_rule *filtering_del[TETH_IP_FAMILIES];
};
static struct teth_bridge_ctx *teth_ctx;

enum teth_packet_direction {
	TETH_USB_TO_A2,
	TETH_A2_TO_USB,
};

/**
 * struct teth_work - wrapper for an skb which is sent using a workqueue
 * @work: used by the workqueue
 * @skb: pointer to the skb to be sent
 * @dir: direction of send, A2 to USB or USB to A2
 */
struct teth_work {
	struct work_struct work;
	struct sk_buff *skb;
	enum teth_packet_direction dir;
};

#ifdef CONFIG_DEBUG_FS
#define TETH_MAX_MSG_LEN 512
static char dbg_buff[TETH_MAX_MSG_LEN];
#endif

/**
 * add_eth_hdrs() - add Ethernet headers to IPA
 * @hdr_name_ipv4: header name for IPv4
 * @hdr_name_ipv6: header name for IPv6
 * @src_mac_addr: source MAC address
 * @dst_mac_addr: destination MAC address
 *
 * This function is called only when link protocol is Ethernet
 */
static int add_eth_hdrs(char *hdr_name_ipv4, char *hdr_name_ipv6,
			u8 *src_mac_addr, u8 *dst_mac_addr)
{
	int res;
	struct ipa_ioc_add_hdr *hdrs;
	struct ethhdr hdr_ipv4;
	struct ethhdr hdr_ipv6;
	int idx1;

	TETH_DBG_FUNC_ENTRY();
	memcpy(hdr_ipv4.h_source, src_mac_addr, ETH_ALEN);
	memcpy(hdr_ipv4.h_dest, dst_mac_addr, ETH_ALEN);
	hdr_ipv4.h_proto = htons(ETHERTYPE_IPV4);

	memcpy(hdr_ipv6.h_source, src_mac_addr, ETH_ALEN);
	memcpy(hdr_ipv6.h_dest, dst_mac_addr, ETH_ALEN);
	hdr_ipv6.h_proto = htons(ETHERTYPE_IPV6);

	/* Add headers to the header insertion tables */
	hdrs = kzalloc(sizeof(struct ipa_ioc_add_hdr) +
		       2 * sizeof(struct ipa_hdr_add), GFP_KERNEL);
	if (hdrs == NULL) {
		TETH_ERR("Failed allocating memory for headers !\n");
		return -ENOMEM;
	}

	hdrs->commit = 0;
	hdrs->num_hdrs = 2;

	/* Ethernet IPv4 header */
	strlcpy(hdrs->hdr[0].name, hdr_name_ipv4, IPA_RESOURCE_NAME_MAX);
	hdrs->hdr[0].hdr_len = ETH_HLEN;
	memcpy(hdrs->hdr[0].hdr, &hdr_ipv4, ETH_HLEN);

	/* Ethernet IPv6 header */
	strlcpy(hdrs->hdr[1].name, hdr_name_ipv6, IPA_RESOURCE_NAME_MAX);
	hdrs->hdr[1].hdr_len = ETH_HLEN;
	memcpy(hdrs->hdr[1].hdr, &hdr_ipv6, ETH_HLEN);

	res = ipa_add_hdr(hdrs);
	if (res || hdrs->hdr[0].status || hdrs->hdr[1].status)
		TETH_ERR("Header insertion failed\n");

	/* Save the headers handles in order to delete them later */
	for (idx1 = 0; idx1 < hdrs->num_hdrs; idx1++) {
		int idx2 = teth_ctx->hdr_del->num_hdls++;
		teth_ctx->hdr_del->hdl[idx2].hdl = hdrs->hdr[idx1].hdr_hdl;
	}

	kfree(hdrs);
	TETH_DBG_FUNC_EXIT();

	return res;
}

static int configure_ipa_header_block_internal(u32 usb_ipa_hdr_len,
					       u32 a2_ipa_hdr_len,
					       u32 ipa_usb_hdr_len,
					       u32 ipa_a2_hdr_len)
{
	struct ipa_ep_cfg_hdr hdr_cfg;
	int res;

	TETH_DBG_FUNC_ENTRY();
	/* Configure header removal for the USB->IPA pipe and A2->IPA pipe */
	memset(&hdr_cfg, 0, sizeof(hdr_cfg));
	hdr_cfg.hdr_len = usb_ipa_hdr_len;
	res = ipa_cfg_ep_hdr(teth_ctx->usb_ipa_pipe_hdl, &hdr_cfg);
	if (res) {
		TETH_ERR("Header removal config for USB->IPA pipe failed\n");
		goto bail;
	}

	hdr_cfg.hdr_len = a2_ipa_hdr_len;
	teth_ctx->a2_ipa_hdr_len = a2_ipa_hdr_len;
	res = ipa_cfg_ep_hdr(teth_ctx->a2_ipa_pipe_hdl, &hdr_cfg);
	if (res) {
		TETH_ERR("Header removal config for A2->IPA pipe failed\n");
		goto bail;
	}

	/* Configure header insertion for the IPA->USB pipe and IPA->A2 pipe */
	hdr_cfg.hdr_len = ipa_usb_hdr_len;
	res = ipa_cfg_ep_hdr(teth_ctx->ipa_usb_pipe_hdl, &hdr_cfg);
	if (res) {
		TETH_ERR("Header insertion config for IPA->USB pipe failed\n");
		goto bail;
	}

	hdr_cfg.hdr_len = ipa_a2_hdr_len;
	teth_ctx->ipa_a2_hdr_len = ipa_a2_hdr_len;
	res = ipa_cfg_ep_hdr(teth_ctx->ipa_a2_pipe_hdl, &hdr_cfg);
	if (res) {
		TETH_ERR("Header insertion config for IPA->A2 pipe failed\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

static int add_mbim_hdr(void)
{
	int res;
	struct ipa_ioc_add_hdr *mbim_hdr;
	u8 mbim_stream_id = 0;
	int idx;

	TETH_DBG_FUNC_ENTRY();
	mbim_hdr = kzalloc(sizeof(struct ipa_ioc_add_hdr) +
			   sizeof(struct ipa_hdr_add),
			   GFP_KERNEL);
	if (!mbim_hdr) {
		TETH_ERR("Failed allocating memory for MBIM header\n");
		return -ENOMEM;
	}

	mbim_hdr->commit = 0;
	mbim_hdr->num_hdrs = 1;
	strlcpy(mbim_hdr->hdr[0].name, MBIM_HEADER_NAME, IPA_RESOURCE_NAME_MAX);
	memcpy(mbim_hdr->hdr[0].hdr, &mbim_stream_id, sizeof(u8));
	mbim_hdr->hdr[0].hdr_len = sizeof(u8);
	mbim_hdr->hdr[0].is_partial = false;
	res = ipa_add_hdr(mbim_hdr);
	if (res || mbim_hdr->hdr[0].status) {
		TETH_ERR("Failed adding MBIM header\n");
		res = -EFAULT;
	} else {
		TETH_DBG("Added MBIM stream ID header\n");
	}

	/* Save the header handle in order to delete it later */
	idx = teth_ctx->hdr_del->num_hdls++;
	teth_ctx->hdr_del->hdl[idx].hdl = mbim_hdr->hdr[0].hdr_hdl;

	kfree(mbim_hdr);
	TETH_DBG_FUNC_EXIT();

	return res;
}

/**
 * configure_ipa_header_block() - adds headers and configures endpoint registers
 *
 * - For IP link protocol and MBIM aggregation, configure MBIM header
 * - For Ethernet link protocol, configure Ethernet headers
 */
static int configure_ipa_header_block(void)
{
	int res;
	u32 hdr_len = 0;
	u32 ipa_usb_hdr_len = 0;

	TETH_DBG_FUNC_ENTRY();
	if (teth_ctx->link_protocol == TETH_LINK_PROTOCOL_IP) {
		/*
		 * Create a new header for MBIM stream ID and associate it with
		 * the IPA->USB routing table
		 */
		if (teth_ctx->aggr_params.dl.aggr_prot ==
					TETH_AGGR_PROTOCOL_MBIM) {
			ipa_usb_hdr_len = 1;
			res = add_mbim_hdr();
			if (res) {
				TETH_ERR("Failed adding MBIM header\n");
				goto bail;
			}
		}
	} else if (teth_ctx->link_protocol == TETH_LINK_PROTOCOL_ETHERNET) {
		/* Add a header entry for USB */
		res = add_eth_hdrs(USB_ETH_HDR_NAME_IPV4,
				   USB_ETH_HDR_NAME_IPV6,
				   teth_ctx->mac_addresses.device_mac_addr,
				   teth_ctx->mac_addresses.host_pc_mac_addr);
		if (res) {
			TETH_ERR("Failed adding USB Ethernet header\n");
			goto bail;
		}
		TETH_DBG("Added USB Ethernet headers (IPv4 / IPv6)\n");

		/* Add a header entry for A2 */
		res = add_eth_hdrs(A2_ETH_HDR_NAME_IPV4,
				   A2_ETH_HDR_NAME_IPV6,
				   teth_ctx->mac_addresses.host_pc_mac_addr,
				   teth_ctx->mac_addresses.device_mac_addr);
		if (res) {
			TETH_ERR("Failed adding A2 Ethernet header\n");
			goto bail;
		}
		TETH_DBG("Added A2 Ethernet headers (IPv4 / IPv6\n");

		hdr_len = ETH_HLEN;
		ipa_usb_hdr_len = ETH_HLEN;
	}

	res = configure_ipa_header_block_internal(hdr_len,
						  hdr_len,
						  ipa_usb_hdr_len,
						  hdr_len);
	if (res) {
		TETH_ERR("Configuration of header removal/insertion failed\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

static int configure_routing_by_ip(char *hdr_name,
			    char *rt_tbl_name,
			    enum ipa_client_type dst,
			    enum ipa_ip_type ip_address_family)
{

	struct ipa_ioc_add_rt_rule *rt_rule;
	struct ipa_ioc_get_hdr hdr_info;
	int res;
	int idx;

	TETH_DBG_FUNC_ENTRY();
	/* Get the header handle */
	memset(&hdr_info, 0, sizeof(hdr_info));
	strlcpy(hdr_info.name, hdr_name, IPA_RESOURCE_NAME_MAX);
	ipa_get_hdr(&hdr_info);

	rt_rule = kzalloc(sizeof(struct ipa_ioc_add_rt_rule) +
			  1 * sizeof(struct ipa_rt_rule_add),
			  GFP_KERNEL);
	if (!rt_rule) {
		TETH_ERR("Memory allocation failure");
		return -ENOMEM;
	}

	/* Match all, do not commit to HW*/
	rt_rule->commit = 0;
	rt_rule->num_rules = 1;
	rt_rule->ip = ip_address_family;
	strlcpy(rt_rule->rt_tbl_name, rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	rt_rule->rules[0].rule.dst = dst;
	rt_rule->rules[0].rule.hdr_hdl = hdr_info.hdl;
	rt_rule->rules[0].rule.attrib.attrib_mask = 0; /* Match all */
	res = ipa_add_rt_rule(rt_rule);
	if (res || rt_rule->rules[0].status)
		TETH_ERR("Failed adding routing rule\n");

	/* Save the routing rule handle in order to delete it later */
	idx = teth_ctx->routing_del[ip_address_family]->num_hdls++;
	teth_ctx->routing_del[ip_address_family]->hdl[idx].hdl =
		rt_rule->rules[0].rt_rule_hdl;

	kfree(rt_rule);
	TETH_DBG_FUNC_EXIT();

	return res;
}

static int configure_routing(char *hdr_name_ipv4,
			     char *rt_tbl_name_ipv4,
			     char *hdr_name_ipv6,
			     char *rt_tbl_name_ipv6,
			     enum ipa_client_type dst)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	/* Configure IPv4 routing table */
	res = configure_routing_by_ip(hdr_name_ipv4,
				      rt_tbl_name_ipv4,
				      dst,
				      IPA_IP_v4);
	if (res) {
		TETH_ERR("Failed adding IPv4 routing table\n");
		goto bail;
	}

	/* Configure IPv6 routing table */
	res = configure_routing_by_ip(hdr_name_ipv6,
				      rt_tbl_name_ipv6,
				      dst,
				      IPA_IP_v6);
	if (res) {
		TETH_ERR("Failed adding IPv6 routing table\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

/**
 * configure_ipa_routing_block() - Configure the IPA routing block
 *
 * This function configures IPA for:
 * - Route all packets from USB to A2
 * - Route all packets from A2 to USB
 * - Use the correct headers in Ethernet or MBIM cases
 */
static int configure_ipa_routing_block(void)
{
	int res;
	char hdr_name_ipv4[IPA_RESOURCE_NAME_MAX];
	char hdr_name_ipv6[IPA_RESOURCE_NAME_MAX];

	TETH_DBG_FUNC_ENTRY();
	hdr_name_ipv4[0] = '\0';
	hdr_name_ipv6[0] = '\0';

	/* Configure USB -> A2 routing table */
	if (teth_ctx->link_protocol == TETH_LINK_PROTOCOL_ETHERNET) {
		strlcpy(hdr_name_ipv4,
			A2_ETH_HDR_NAME_IPV4,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(hdr_name_ipv6,
			A2_ETH_HDR_NAME_IPV6,
			IPA_RESOURCE_NAME_MAX);
	}
	res = configure_routing(hdr_name_ipv4,
				USB_TO_A2_RT_TBL_NAME_IPV4,
				hdr_name_ipv6,
				USB_TO_A2_RT_TBL_NAME_IPV6,
				IPA_CLIENT_A2_TETHERED_CONS);
	if (res) {
		TETH_ERR("USB to A2 routing block configuration failed\n");
		goto bail;
	}

	/* Configure A2 -> USB routing table */
	if (teth_ctx->link_protocol == TETH_LINK_PROTOCOL_ETHERNET) {
		strlcpy(hdr_name_ipv4,
			USB_ETH_HDR_NAME_IPV4,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(hdr_name_ipv6,
			USB_ETH_HDR_NAME_IPV6,
			IPA_RESOURCE_NAME_MAX);
	} else if (teth_ctx->aggr_params.dl.aggr_prot ==
						TETH_AGGR_PROTOCOL_MBIM) {
		strlcpy(hdr_name_ipv4,
			MBIM_HEADER_NAME,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(hdr_name_ipv6,
			MBIM_HEADER_NAME,
			IPA_RESOURCE_NAME_MAX);
	}
	res = configure_routing(hdr_name_ipv4,
				A2_TO_USB_RT_TBL_NAME_IPV4,
				hdr_name_ipv6,
				A2_TO_USB_RT_TBL_NAME_IPV6,
				IPA_CLIENT_USB_CONS);
	if (res) {
		TETH_ERR("A2 to USB routing block configuration failed\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

static int configure_filtering_by_ip(char *rt_tbl_name,
			      enum ipa_client_type src,
			      enum ipa_ip_type ip_address_family)
{
	struct ipa_ioc_add_flt_rule *flt_tbl;
	struct ipa_ioc_get_rt_tbl rt_tbl_info;
	int res;
	int idx;

	TETH_DBG_FUNC_ENTRY();
	/* Get the needed routing table handle */
	rt_tbl_info.ip = ip_address_family;
	strlcpy(rt_tbl_info.name, rt_tbl_name, IPA_RESOURCE_NAME_MAX);
	res = ipa_get_rt_tbl(&rt_tbl_info);
	if (res) {
		TETH_ERR("Failed getting routing table handle\n");
		goto bail;
	}

	flt_tbl = kzalloc(sizeof(struct ipa_ioc_add_flt_rule) +
			  1 * sizeof(struct ipa_flt_rule_add), GFP_KERNEL);
	if (!flt_tbl) {
		TETH_ERR("Filtering table memory allocation failure\n");
		return -ENOMEM;
	}

	flt_tbl->commit = 0;
	flt_tbl->ep = src;
	flt_tbl->global = 0;
	flt_tbl->ip = ip_address_family;
	flt_tbl->num_rules = 1;
	flt_tbl->rules[0].rule.action = IPA_PASS_TO_ROUTING;
	flt_tbl->rules[0].rule.rt_tbl_hdl = rt_tbl_info.hdl;
	flt_tbl->rules[0].rule.attrib.attrib_mask = 0; /* Match all */

	res = ipa_add_flt_rule(flt_tbl);
	if (res || flt_tbl->rules[0].status)
		TETH_ERR("Failed adding filtering table\n");

	/* Save the filtering rule handle in order to delete it later */
	idx = teth_ctx->filtering_del[ip_address_family]->num_hdls++;
	teth_ctx->filtering_del[ip_address_family]->hdl[idx].hdl =
		flt_tbl->rules[0].flt_rule_hdl;

	kfree(flt_tbl);
	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

static int configure_filtering(char *rt_tbl_name_ipv4,
			char *rt_tbl_name_ipv6,
			enum ipa_client_type src)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	res = configure_filtering_by_ip(rt_tbl_name_ipv4, src, IPA_IP_v4);
	if (res) {
		TETH_ERR("Failed adding IPv4 filtering table\n");
		goto bail;
	}

	res = configure_filtering_by_ip(rt_tbl_name_ipv6, src, IPA_IP_v6);
	if (res) {
		TETH_ERR("Failed adding IPv4 filtering table\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

/**
 * configure_ipa_filtering_block() - Configures IPA filtering block
 *
 * This function configures IPA for:
 * - Filter all traffic coming from USB to A2 pointing routing table
 * - Filter all traffic coming from A2 to USB pointing routing table
 */
static int configure_ipa_filtering_block(void)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	/* Filter all traffic coming from USB to A2 */
	res = configure_filtering(USB_TO_A2_RT_TBL_NAME_IPV4,
				  USB_TO_A2_RT_TBL_NAME_IPV6,
				  IPA_CLIENT_USB_PROD);
	if (res) {
		TETH_ERR("USB_PROD ep filtering configuration failed\n");
		goto bail;
	}

	/* Filter all traffic coming from A2 to USB */
	res = configure_filtering(A2_TO_USB_RT_TBL_NAME_IPV4,
				  A2_TO_USB_RT_TBL_NAME_IPV6,
				  IPA_CLIENT_A2_TETHERED_PROD);
	if (res) {
		TETH_ERR("A2_PROD filtering configuration failed\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

static int prepare_ipa_aggr_struct(
	const struct teth_aggr_params_link *teth_aggr_params,
	struct ipa_ep_cfg_aggr *ipa_aggr_params,
	bool client_is_prod)
{
	TETH_DBG_FUNC_ENTRY();
	memset(ipa_aggr_params, 0, sizeof(*ipa_aggr_params));

	switch (teth_aggr_params->aggr_prot) {
	case TETH_AGGR_PROTOCOL_NONE:
		ipa_aggr_params->aggr_en = IPA_BYPASS_AGGR;
		break;
	case TETH_AGGR_PROTOCOL_MBIM:
		 ipa_aggr_params->aggr = IPA_MBIM_16;
		 ipa_aggr_params->aggr_en = (client_is_prod) ?
			 IPA_ENABLE_DEAGGR : IPA_ENABLE_AGGR;
		 break;
	case TETH_AGGR_PROTOCOL_TLP:
		ipa_aggr_params->aggr = IPA_TLP;
		ipa_aggr_params->aggr_en = (client_is_prod) ?
			IPA_ENABLE_DEAGGR : IPA_ENABLE_AGGR;
		break;
	default:
		TETH_ERR("Unsupported aggregation protocol\n");
		return -EFAULT;
	}

	/*
	 * Due to a HW 'feature', the maximal aggregated packet size may be the
	 * requested aggr_byte_limit plus the MTU. Therefore, the MTU is
	 * subtracted from the requested aggr_byte_limit so that the requested
	 * byte limit is honored .
	 */
	ipa_aggr_params->aggr_byte_limit =
		(teth_aggr_params->max_transfer_size_byte - TETH_MTU_BYTE) /
		1024;
	ipa_aggr_params->aggr_time_limit = TETH_DEFAULT_AGGR_TIME_LIMIT;
	TETH_DBG_FUNC_EXIT();

	return 0;
}

static int teth_set_aggr_per_ep(
	const struct teth_aggr_params_link *teth_aggr_params,
	bool client_is_prod,
	u32 pipe_hdl)
{
	struct ipa_ep_cfg_aggr agg_params;
	int res;

	TETH_DBG_FUNC_ENTRY();
	res = prepare_ipa_aggr_struct(teth_aggr_params,
				      &agg_params,
				      client_is_prod);
	if (res) {
		TETH_ERR("prepare_ipa_aggregation_struct() failed\n");
		goto bail;
	}

	res = ipa_cfg_ep_aggr(pipe_hdl, &agg_params);
	if (res) {
		TETH_ERR("ipa_cfg_ep_aggr() failed\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

static void aggr_prot_to_str(enum teth_aggr_protocol_type aggr_prot,
			     char *buff,
			     uint buff_size)
{
	switch (aggr_prot) {
	case TETH_AGGR_PROTOCOL_NONE:
		strlcpy(buff, "NONE", buff_size);
		break;
	case TETH_AGGR_PROTOCOL_MBIM:
		strlcpy(buff, "MBIM", buff_size);
		break;
	case TETH_AGGR_PROTOCOL_TLP:
		strlcpy(buff, "TLP", buff_size);
		break;
	default:
		strlcpy(buff, "ERROR", buff_size);
		break;
	}
}

/**
 * teth_set_aggregation() - set aggregation parameters to IPA
 *
 * The parameters to this function are passed in the context variable ipa_ctx.
 */
static int teth_set_aggregation(void)
{
	int res;
	char aggr_prot_str[20];

	TETH_DBG_FUNC_ENTRY();
	if (!teth_ctx->aggr_params_known) {
		TETH_ERR("Aggregation parameters unknown.\n");
		return -EINVAL;
	}

	if ((teth_ctx->usb_ipa_pipe_hdl == 0) ||
	    (teth_ctx->ipa_usb_pipe_hdl == 0))
		return 0;
		/*
		 * Returning 0 in case pipe handles are 0 becuase aggregation
		 * params will be set later
		 */

	if (teth_ctx->aggr_params.ul.aggr_prot == TETH_AGGR_PROTOCOL_MBIM ||
	    teth_ctx->aggr_params.dl.aggr_prot == TETH_AGGR_PROTOCOL_MBIM) {
		res = ipa_set_aggr_mode(IPA_MBIM);
		if (res) {
			TETH_ERR("ipa_set_aggr_mode() failed\n");
			goto bail;
		}
		res = ipa_set_single_ndp_per_mbim(false);
		if (res) {
			TETH_ERR("ipa_set_single_ndp_per_mbim() failed\n");
			goto bail;
		}
	}

	aggr_prot_to_str(teth_ctx->aggr_params.ul.aggr_prot,
			 aggr_prot_str,
			 sizeof(aggr_prot_str)-1);
	TETH_DBG("Setting %s aggregation on UL\n", aggr_prot_str);
	aggr_prot_to_str(teth_ctx->aggr_params.dl.aggr_prot,
			 aggr_prot_str,
			 sizeof(aggr_prot_str)-1);
	TETH_DBG("Setting %s aggregation on DL\n", aggr_prot_str);

	/* Configure aggregation on UL producer (USB->IPA) */
	res = teth_set_aggr_per_ep(&teth_ctx->aggr_params.ul,
				   true,
				   teth_ctx->usb_ipa_pipe_hdl);
	if (res) {
		TETH_ERR("teth_set_aggregation_per_ep() failed\n");
		goto bail;
	}

	/* Configure aggregation on DL consumer (IPA->USB) */
	res = teth_set_aggr_per_ep(&teth_ctx->aggr_params.dl,
				   false,
				   teth_ctx->ipa_usb_pipe_hdl);
	if (res) {
		TETH_ERR("teth_set_aggregation_per_ep() failed\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

/**
 * teth_request_resource() - wrapper function to
 * ipa_rm_inactivity_timer_request_resource()
 *
 * - initialize the is_bridge_prod_up completion object
 * - request the resource
 * - error handling
 */
static int teth_request_resource(void)
{
	int res;

	INIT_COMPLETION(teth_ctx->is_bridge_prod_up);
	res = ipa_rm_inactivity_timer_request_resource(
		IPA_RM_RESOURCE_BRIDGE_PROD);
	if (res < 0) {
		if (res == -EINPROGRESS)
			wait_for_completion(&teth_ctx->is_bridge_prod_up);
		else
			return res;
	}

	return 0;
}

/**
 * complete_hw_bridge() - setup the HW bridge from USB to A2 and back through
 * IPA
 */
static void complete_hw_bridge(struct work_struct *work)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	TETH_DBG("Completing HW bridge in %s mode\n",
		 (teth_ctx->link_protocol == TETH_LINK_PROTOCOL_ETHERNET) ?
		 "ETHERNET" :
		 "IP");

	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		goto bail;
	}

	res = teth_set_aggregation();
	if (res) {
		TETH_ERR("Failed setting aggregation params\n");
		goto bail;
	}

	res = configure_ipa_header_block();
	if (res) {
		TETH_ERR("Configuration of IPA header block Failed\n");
		goto bail;
	}

	res = configure_ipa_routing_block();
	if (res) {
		TETH_ERR("Configuration of IPA routing block Failed\n");
		goto bail;
	}

	res = configure_ipa_filtering_block();
	if (res) {
		TETH_ERR("Configuration of IPA filtering block Failed\n");
		goto bail;
	}

	/*
	 * Commit all the data to HW, including header, routing and filtering
	 * blocks, IPv4 and IPv6
	 */
	res = ipa_commit_hdr();
	if (res) {
		TETH_ERR("Failed committing headers / routing / filtering.\n");
		goto bail;
	}

	teth_ctx->is_hw_bridge_complete = true;
bail:
	teth_ctx->comp_hw_bridge_in_progress = false;
	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);
	TETH_DBG_FUNC_EXIT();

	return;
}

static void mac_addr_to_str(u8 mac_addr[ETH_ALEN],
		     char *buff,
		     uint buff_size)
{
	scnprintf(buff, buff_size, "%02x-%02x-%02x-%02x-%02x-%02x",
		  mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3],
		  mac_addr[4], mac_addr[5]);
}

/**
 * check_to_complete_hw_bridge() - can HW bridge be set up ?
 * @param skb: pointer to socket buffer
 * @param my_mac_addr: pointer to write 'my' extracted MAC address to
 * @param my_mac_addr_known: pointer to update whether 'my' extracted MAC
 * address is known
 * @param peer_mac_addr_known: pointer to update whether the 'peer' extracted
 * MAC address is known
 *
 * This function is used by both A2 and USB callback functions, therefore the
 * meaning of 'my' and 'peer' changes according to the context.
 * Extracts MAC address from the packet in Ethernet link protocol,
 * Sets up the HW bridge in case all conditions are met.
 */
static void check_to_complete_hw_bridge(struct sk_buff *skb,
					u8 *my_mac_addr,
					bool *my_mac_addr_known,
					bool *peer_mac_addr_known)
{
	bool both_mac_addresses_known;
	char mac_addr_str[20];

	if ((teth_ctx->link_protocol == TETH_LINK_PROTOCOL_ETHERNET) &&
	    (!(*my_mac_addr_known))) {
		memcpy(my_mac_addr, &skb->data[ETH_ALEN], ETH_ALEN);
		mac_addr_to_str(my_mac_addr,
				mac_addr_str,
				sizeof(mac_addr_str)-1);
		TETH_DBG("Extracted MAC addr: %s\n", mac_addr_str);
		*my_mac_addr_known = true;
	}

	both_mac_addresses_known = *my_mac_addr_known && *peer_mac_addr_known;
	if ((both_mac_addresses_known ||
	    (teth_ctx->link_protocol == TETH_LINK_PROTOCOL_IP)) &&
	    (!teth_ctx->comp_hw_bridge_in_progress) &&
	    (teth_ctx->aggr_params_known)) {
		INIT_WORK(&teth_ctx->comp_hw_bridge_work, complete_hw_bridge);
		teth_ctx->comp_hw_bridge_in_progress = true;
		queue_work(teth_ctx->teth_wq, &teth_ctx->comp_hw_bridge_work);
	}
}

/**
 * teth_send_skb_work() - workqueue function for sending a packet
 */
static void teth_send_skb_work(struct work_struct *work)
{
	struct teth_work *work_data =
		container_of(work, struct teth_work, work);
	int res;

	res = teth_request_resource();
	if (res) {
		TETH_ERR("Packet send failure, dropping packet !\n");
		goto bail;
	}

	switch (work_data->dir) {
	case TETH_USB_TO_A2:
		res = a2_mux_write(A2_MUX_TETHERED_0, work_data->skb);
		if (res) {
			TETH_ERR("Packet send failure, dropping packet !\n");
			goto bail;
		}
		teth_ctx->stats.usb_to_a2_num_sw_tx_packets++;
		break;

	case TETH_A2_TO_USB:
		res = ipa_tx_dp(IPA_CLIENT_USB_CONS, work_data->skb, NULL);
		if (res) {
			TETH_ERR("Packet send failure, dropping packet !\n");
			goto bail;
		}
		teth_ctx->stats.a2_to_usb_num_sw_tx_packets++;
		break;

	default:
		TETH_ERR("Unsupported direction to send !\n");
		WARN_ON(1);
	}
	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);
	kfree(work_data);
	teth_ctx->stats.num_sw_tx_packets_during_resource_wakeup++;

	return;
bail:
	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);
	dev_kfree_skb(work_data->skb);
	kfree(work_data);
}

/**
 * defer_skb_send() - defer sending an skb using the SW bridge to a workqueue
 * @param skb: pointer to the socket buffer
 * @param dir: direction of send
 *
 * In case where during a packet send, the A2 or USB needs to wake up from power
 * collapse, defer the send and return the context to IPA driver. This is
 * important since IPA driver has a single threaded Rx path.
 */
static void defer_skb_send(struct sk_buff *skb, enum teth_packet_direction dir)
{
	struct teth_work *work = kmalloc(sizeof(struct teth_work), GFP_KERNEL);

	if (!work) {
		TETH_ERR("No mem, dropping packet\n");
		dev_kfree_skb(skb);
		ipa_rm_inactivity_timer_release_resource
			(IPA_RM_RESOURCE_BRIDGE_PROD);
		return;
	}

	/*
	 * Since IPA uses a single Rx thread, we don't
	 * want to wait for completion here
	 */
	INIT_WORK(&work->work, teth_send_skb_work);
	work->dir = dir;
	work->skb = skb;
	queue_work(teth_ctx->teth_wq, &work->work);
}

/**
 * usb_notify_cb() - callback function for sending packets from USB to A2
 * @param priv: private data
 * @param evt: event - RECEIVE or WRITE_DONE
 * @param data: pointer to skb to be sent
 *
 * This callback function is installed by the IPA driver, it is invoked in 2
 * cases:
 * 1. When a packet comes from the USB pipe and is routed to A5 (SW bridging)
 * 2. After a packet has been bridged from USB to A2 and its skb should be freed
 *
 * Invocation: sps driver --> IPA driver --> bridge driver
 *
 * In the event of IPA_RECEIVE:
 * - Checks whether the HW bridge can be set up..
 * - Requests the BRIDGE_PROD resource so that A2 and USB are not in power
 * collapse. In case where the resource is waking up, defer the send operation
 * to a workqueue in order to not block the IPA driver single threaded Rx path.
 * - Sends the packets to A2 using a2_service driver API.
 * - Releases the BRIDGE_PROD resource.
 *
 * In the event of IPA_WRITE_DONE:
 * - Frees the skb memory
 */
static void usb_notify_cb(void *priv,
			  enum ipa_dp_evt_type evt,
			  unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	int res;

	switch (evt) {
	case IPA_RECEIVE:
		if (!teth_ctx->is_hw_bridge_complete)
			check_to_complete_hw_bridge(
				skb,
				teth_ctx->mac_addresses.host_pc_mac_addr,
				&teth_ctx->mac_addresses.host_pc_mac_addr_known,
				&teth_ctx->mac_addresses.device_mac_addr_known);

		/*
		 * Request the BRIDGE_PROD resource, send the packet and release
		 * the resource
		 */
		res = ipa_rm_inactivity_timer_request_resource(
			IPA_RM_RESOURCE_BRIDGE_PROD);
		if (res < 0) {
			if (res == -EINPROGRESS) {
				/* The resource is waking up */
				defer_skb_send(skb, TETH_USB_TO_A2);
			} else {
				TETH_ERR(
					"Packet send failure, dropping packet !\n");
				dev_kfree_skb(skb);
			}
			ipa_rm_inactivity_timer_release_resource(
				IPA_RM_RESOURCE_BRIDGE_PROD);
			return;
		}
		res = a2_mux_write(A2_MUX_TETHERED_0, skb);
		if (res) {
			TETH_ERR("Packet send failure, dropping packet !\n");
			dev_kfree_skb(skb);
			ipa_rm_inactivity_timer_release_resource(
				IPA_RM_RESOURCE_BRIDGE_PROD);
			return;
		}
		teth_ctx->stats.usb_to_a2_num_sw_tx_packets++;
		ipa_rm_inactivity_timer_release_resource(
			IPA_RM_RESOURCE_BRIDGE_PROD);
		break;

	case IPA_WRITE_DONE:
		dev_kfree_skb(skb);
		break;

	default:
		TETH_ERR("Unsupported IPA event !\n");
		WARN_ON(1);
	}

	return;
}

/**
 * a2_notify_cb() - callback function for sending packets from A2 to USB
 * @param user_data: private data
 * @param event: event - RECEIVE or WRITE_DONE
 * @param data: pointer to skb to be sent
 *
 * This callback function is installed by the IPA driver, it is invoked in 2
 * cases:
 * 1. When a packet comes from the A2 pipe and is routed to A5 (SW bridging)
 * 2. After a packet has been bridged from A2 to USB and its skb should be freed
 *
 * Invocation: sps driver --> IPA driver --> a2_service driver --> bridge driver
 *
 * In the event of A2_MUX_RECEIVE:
 * - Checks whether the HW bridge can be set up..
 * - Requests the BRIDGE_PROD resource so that A2 and USB are not in power
 * collapse. In case where the resource is waking up, defer the send operation
 * to a workqueue in order to not block the IPA driver single threaded Rx path.
 * - Sends the packets to USB using IPA drivers ipa_tx_dp() API.
 * - Releases the BRIDGE_PROD resource.
 *
 * In the event of A2_MUX_WRITE_DONE:
 * - Frees the skb memory
 */
static void a2_notify_cb(void *user_data,
			 enum a2_mux_event_type event,
			 unsigned long data)
{
	struct sk_buff *skb = (struct sk_buff *)data;
	int res;

	switch (event) {
	case A2_MUX_RECEIVE:
		if (!teth_ctx->is_hw_bridge_complete)
			check_to_complete_hw_bridge(
				skb,
				teth_ctx->mac_addresses.device_mac_addr,
				&teth_ctx->mac_addresses.device_mac_addr_known,
				&teth_ctx->
				mac_addresses.host_pc_mac_addr_known);

		/*
		 * Request the BRIDGE_PROD resource, send the packet and release
		 * the resource
		 */
		res = ipa_rm_inactivity_timer_request_resource(
			IPA_RM_RESOURCE_BRIDGE_PROD);
		if (res < 0) {
			if (res == -EINPROGRESS) {
				/* The resource is waking up */
				defer_skb_send(skb, TETH_A2_TO_USB);
			} else {
				TETH_ERR(
					"Packet send failure, dropping packet !\n");
				dev_kfree_skb(skb);
			}
			ipa_rm_inactivity_timer_release_resource(
				IPA_RM_RESOURCE_BRIDGE_PROD);
			return;
		}

		res = ipa_tx_dp(IPA_CLIENT_USB_CONS, skb, NULL);
		if (res) {
			TETH_ERR("Packet send failure, dropping packet !\n");
			dev_kfree_skb(skb);
			ipa_rm_inactivity_timer_release_resource(
				IPA_RM_RESOURCE_BRIDGE_PROD);
			return;
		}
		teth_ctx->stats.a2_to_usb_num_sw_tx_packets++;
		ipa_rm_inactivity_timer_release_resource(
			IPA_RM_RESOURCE_BRIDGE_PROD);
		break;

	case A2_MUX_WRITE_DONE:
		dev_kfree_skb(skb);
		break;

	default:
		TETH_ERR("Unsupported IPA event !\n");
		WARN_ON(1);
	}

	return;
}

/**
 * bridge_prod_notify_cb() - IPA Resource Manager callback function
 * @param notify_cb_data: private data
 * @param event: RESOURCE_GRANTED / RESOURCE_RELEASED
 * @param data: not used in this case
 *
 * This callback function is called by IPA resource manager to notify the
 * BRIDGE_PROD entity of events like RESOURCE_GRANTED and RESOURCE_RELEASED.
 */
static void bridge_prod_notify_cb(void *notify_cb_data,
				  enum ipa_rm_event event,
				  unsigned long data)
{
	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		complete(&teth_ctx->is_bridge_prod_up);
		break;

	case IPA_RM_RESOURCE_RELEASED:
		complete(&teth_ctx->is_bridge_prod_down);
		break;

	default:
		TETH_ERR("Unsupported notification!\n");
		WARN_ON(1);
		break;
	}

	return;
}

static void a2_prod_notify_cb(void *notify_cb_data,
			      enum ipa_rm_event event,
			      unsigned long data)
{
	int res;
	struct ipa_ep_cfg ipa_ep_cfg;

	switch (event) {
	case IPA_RM_RESOURCE_GRANTED:
		res = a2_mux_get_tethered_client_handles(
			A2_MUX_TETHERED_0,
			&teth_ctx->ipa_a2_pipe_hdl,
			&teth_ctx->a2_ipa_pipe_hdl);
		if (res) {
			TETH_ERR(
				"a2_mux_get_tethered_client_handles() failed, res = %d\n",
				res);
			return;
		}

		/* Reset the various endpoints configuration */
		memset(&ipa_ep_cfg, 0, sizeof(ipa_ep_cfg));
		ipa_ep_cfg.hdr.hdr_len = teth_ctx->ipa_a2_hdr_len;
		ipa_cfg_ep(teth_ctx->ipa_a2_pipe_hdl, &ipa_ep_cfg);

		memset(&ipa_ep_cfg, 0, sizeof(ipa_ep_cfg));
		ipa_ep_cfg.hdr.hdr_len = teth_ctx->a2_ipa_hdr_len;
		ipa_cfg_ep(teth_ctx->a2_ipa_pipe_hdl, &ipa_ep_cfg);
		break;

	case IPA_RM_RESOURCE_RELEASED:
		break;

	default:
		TETH_ERR("Unsupported notification!\n");
		WARN_ON(1);
		break;
	}

	return;
}

/**
* teth_bridge_init() - Initialize the Tethering bridge driver
* @usb_notify_cb_ptr:	Callback function which should be used by the caller.
* Output parameter.
* @private_data_ptr:	Data for the callback function. Should be used by the
* caller. Output parameter.
*
* USB driver gets a pointer to a callback function (usb_notify_cb) and an
* associated data. USB driver installs this callback function in the call to
* ipa_connect().
*
* Builds IPA resource manager dependency graph.
*
* Return codes: 0: success,
*		-EINVAL - Bad parameter
*		Other negative value - Failure
*/
int teth_bridge_init(ipa_notify_cb *usb_notify_cb_ptr, void **private_data_ptr)
{
	int res = 0;
	struct ipa_rm_register_params a2_prod_reg_params;

	TETH_DBG_FUNC_ENTRY();
	if (usb_notify_cb_ptr == NULL) {
		TETH_ERR("Bad parameter\n");
		res = -EINVAL;
		goto bail;
	}

	*usb_notify_cb_ptr = usb_notify_cb;
	*private_data_ptr = NULL;

	/* Build IPA Resource manager dependency graph */
	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				    IPA_RM_RESOURCE_USB_CONS);
	if (res && res != -EINPROGRESS) {
		TETH_ERR("ipa_rm_add_dependency() failed\n");
		goto bail;
	}

	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				    IPA_RM_RESOURCE_A2_CONS);
	if (res && res != -EINPROGRESS) {
		TETH_ERR("ipa_rm_add_dependency() failed\n");
		goto fail_add_dependency_1;
	}

	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_USB_PROD,
				    IPA_RM_RESOURCE_A2_CONS);
	if (res && res != -EINPROGRESS) {
		TETH_ERR("ipa_rm_add_dependency() failed\n");
		goto fail_add_dependency_2;
	}

	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_A2_PROD,
				    IPA_RM_RESOURCE_USB_CONS);
	if (res && res != -EINPROGRESS) {
		TETH_ERR("ipa_rm_add_dependency() failed\n");
		goto fail_add_dependency_3;
	}

	/* Register for A2_PROD resource notifications */
	a2_prod_reg_params.user_data = NULL;
	a2_prod_reg_params.notify_cb = a2_prod_notify_cb;
	res = ipa_rm_register(IPA_RM_RESOURCE_A2_PROD, &a2_prod_reg_params);
	if (res) {
		TETH_ERR("ipa_rm_register() failed\n");
		goto fail_add_dependency_4;
	}

	/* Return 0 as EINPROGRESS is a valid return value at this point */
	res = 0;
	goto bail;

fail_add_dependency_4:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_A2_PROD,
				 IPA_RM_RESOURCE_USB_CONS);
fail_add_dependency_3:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_PROD,
				 IPA_RM_RESOURCE_A2_CONS);
fail_add_dependency_2:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				 IPA_RM_RESOURCE_A2_CONS);
fail_add_dependency_1:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				 IPA_RM_RESOURCE_USB_CONS);
bail:
	TETH_DBG_FUNC_EXIT();
	return res;
}
EXPORT_SYMBOL(teth_bridge_init);

/**
 * initialize_context() - Initialize the ipa_ctx struct
 */
static void initialize_context(void)
{
	TETH_DBG_FUNC_ENTRY();
	/* Initialize context variables */
	teth_ctx->usb_ipa_pipe_hdl = 0;
	teth_ctx->ipa_a2_pipe_hdl = 0;
	teth_ctx->a2_ipa_pipe_hdl = 0;
	teth_ctx->ipa_usb_pipe_hdl = 0;
	teth_ctx->is_connected = false;

	/* The default link protocol is Ethernet */
	teth_ctx->link_protocol = TETH_LINK_PROTOCOL_ETHERNET;

	memset(&teth_ctx->mac_addresses, 0, sizeof(teth_ctx->mac_addresses));
	teth_ctx->is_hw_bridge_complete = false;
	memset(&teth_ctx->aggr_params, 0, sizeof(teth_ctx->aggr_params));
	teth_ctx->aggr_params_known = false;
	teth_ctx->tethering_mode = 0;
	INIT_COMPLETION(teth_ctx->is_bridge_prod_up);
	INIT_COMPLETION(teth_ctx->is_bridge_prod_down);
	teth_ctx->comp_hw_bridge_in_progress = false;
	memset(&teth_ctx->stats, 0, sizeof(teth_ctx->stats));
	teth_ctx->a2_ipa_hdr_len = 0;
	teth_ctx->ipa_a2_hdr_len = 0;
	memset(teth_ctx->hdr_del,
	       0,
	       sizeof(struct ipa_ioc_del_hdr) + TETH_TOTAL_HDR_ENTRIES *
	       sizeof(struct ipa_hdr_del));
	memset(teth_ctx->routing_del[IPA_IP_v4],
	       0,
	       sizeof(struct ipa_ioc_del_rt_rule) +
	       TETH_TOTAL_RT_ENTRIES_IP * sizeof(struct ipa_rt_rule_del));
	teth_ctx->routing_del[IPA_IP_v4]->ip = IPA_IP_v4;
	memset(teth_ctx->routing_del[IPA_IP_v6],
	       0,
	       sizeof(struct ipa_ioc_del_rt_rule) +
	       TETH_TOTAL_RT_ENTRIES_IP * sizeof(struct ipa_rt_rule_del));
	teth_ctx->routing_del[IPA_IP_v6]->ip = IPA_IP_v6;
	memset(teth_ctx->filtering_del[IPA_IP_v4],
	       0,
	       sizeof(struct ipa_ioc_del_flt_rule) +
	       TETH_TOTAL_FLT_ENTRIES_IP * sizeof(struct ipa_flt_rule_del));
	teth_ctx->filtering_del[IPA_IP_v4]->ip = IPA_IP_v4;
	memset(teth_ctx->filtering_del[IPA_IP_v6],
	       0,
	       sizeof(struct ipa_ioc_del_flt_rule) +
	       TETH_TOTAL_FLT_ENTRIES_IP * sizeof(struct ipa_flt_rule_del));
	teth_ctx->filtering_del[IPA_IP_v6]->ip = IPA_IP_v6;

	TETH_DBG_FUNC_EXIT();
}

/**
* teth_bridge_disconnect() - Disconnect tethering bridge module
*/
int teth_bridge_disconnect(void)
{
	int res;
	struct ipa_rm_register_params a2_prod_reg_params;

	TETH_DBG_FUNC_ENTRY();
	if (!teth_ctx->is_connected) {
		TETH_ERR(
			"Trying to disconnect an already disconnected bridge\n");
		goto bail;
	}

	/*
	 * Delete part of IPA resource manager dependency graph. Only the
	 * BRIDGE_PROD <-> A2 dependency remains intact
	 */
	res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				       IPA_RM_RESOURCE_USB_CONS);
	if ((res != 0) && (res != -EINPROGRESS))
		TETH_ERR(
			"Failed deleting ipa_rm dependency BRIDGE_PROD <-> USB_CONS\n");
	res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_PROD,
				       IPA_RM_RESOURCE_A2_CONS);
	if ((res != 0) && (res != -EINPROGRESS))
		TETH_ERR(
			"Failed deleting ipa_rm dependency USB_PROD <-> A2_CONS\n");
	res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_A2_PROD,
				       IPA_RM_RESOURCE_USB_CONS);
	if ((res != 0) && (res != -EINPROGRESS))
		TETH_ERR(
			"Failed deleting ipa_rm dependency A2_PROD <-> USB_CONS\n");

	/* Request the BRIDGE_PROD resource, A2 and IPA should power up */
	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		goto bail;
	}

	/* Close the channel to A2 */
	if (a2_mux_close_channel(A2_MUX_TETHERED_0))
		TETH_ERR("a2_mux_close_channel() failed\n");

	/* Teardown the IPA HW bridge */
	if (teth_ctx->is_hw_bridge_complete) {
		/* Delete header entries */
		if (ipa_del_hdr(teth_ctx->hdr_del))
			TETH_ERR("ipa_del_hdr() failed\n");

		/* Delete installed routing rules */
		if (ipa_del_rt_rule(teth_ctx->routing_del[IPA_IP_v4]))
			TETH_ERR("ipa_del_rt_rule() failed\n");
		if (ipa_del_rt_rule(teth_ctx->routing_del[IPA_IP_v6]))
			TETH_ERR("ipa_del_rt_rule() failed\n");

		/* Delete installed filtering rules */
		if (ipa_del_flt_rule(teth_ctx->filtering_del[IPA_IP_v4]))
			TETH_ERR("ipa_del_flt_rule() failed\n");
		if (ipa_del_flt_rule(teth_ctx->filtering_del[IPA_IP_v6]))
			TETH_ERR("ipa_del_flt_rule() failed\n");

		/*
		 * Commit all the data to HW, including header, routing and
		 * filtering blocks, IPv4 and IPv6
		 */
		if (ipa_commit_hdr())
			TETH_ERR("Failed committing headers\n");
	}

	initialize_context();

	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);

	/* Delete the last ipa_rm dependency - BRIDGE_PROD <-> A2 */
	res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				       IPA_RM_RESOURCE_A2_CONS);
	if ((res != 0) && (res != -EINPROGRESS))
		TETH_ERR(
			"Failed deleting ipa_rm dependency BRIDGE_PROD <-> A2_CONS\n");

	/* Deregister from A2_PROD notifications */
	a2_prod_reg_params.user_data = NULL;
	a2_prod_reg_params.notify_cb = a2_prod_notify_cb;
	res = ipa_rm_deregister(IPA_RM_RESOURCE_A2_PROD, &a2_prod_reg_params);
	if (res)
		TETH_ERR("Failed deregistering from A2_prod notifications.\n");

	teth_ctx->is_connected = false;
bail:
	TETH_DBG_FUNC_EXIT();

	return 0;
}
EXPORT_SYMBOL(teth_bridge_disconnect);

/**
* teth_bridge_connect() - Connect bridge for a tethered Rmnet / MBIM call
* @connect_params:	Connection info
*
* Return codes: 0: success
*		-EINVAL: invalid parameters
*		-EPERM: Operation not permitted as the bridge is already
*		connected
*/
int teth_bridge_connect(struct teth_bridge_connect_params *connect_params)
{
	int res;
	struct ipa_ep_cfg ipa_ep_cfg;

	TETH_DBG_FUNC_ENTRY();
	if (teth_ctx->is_connected) {
		TETH_ERR("Trying to connect an already connected bridge !\n");
		return -EPERM;
	}
	if (connect_params == NULL ||
	    connect_params->ipa_usb_pipe_hdl <= 0 ||
	    connect_params->usb_ipa_pipe_hdl <= 0 ||
	    connect_params->tethering_mode >= TETH_TETHERING_MODE_MAX ||
	    connect_params->tethering_mode < 0)
		return -EINVAL;

	teth_ctx->ipa_usb_pipe_hdl = connect_params->ipa_usb_pipe_hdl;
	teth_ctx->usb_ipa_pipe_hdl = connect_params->usb_ipa_pipe_hdl;
	teth_ctx->tethering_mode = connect_params->tethering_mode;

	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		goto bail;
	}

	res = a2_mux_open_channel(A2_MUX_TETHERED_0,
				  NULL,
				  a2_notify_cb);
	if (res) {
		TETH_ERR("a2_mux_open_channel() failed\n");
		goto bail;
	}

	res = a2_mux_get_tethered_client_handles(A2_MUX_TETHERED_0,
						 &teth_ctx->ipa_a2_pipe_hdl,
						 &teth_ctx->a2_ipa_pipe_hdl);
	if (res) {
		TETH_ERR(
		"a2_mux_get_tethered_client_handles() failed, res = %d\n", res);
		goto bail;
	}

	/* Reset the various endpoints configuration */
	memset(&ipa_ep_cfg, 0, sizeof(ipa_ep_cfg));
	ipa_cfg_ep(teth_ctx->ipa_usb_pipe_hdl, &ipa_ep_cfg);
	ipa_cfg_ep(teth_ctx->usb_ipa_pipe_hdl, &ipa_ep_cfg);
	ipa_cfg_ep(teth_ctx->ipa_a2_pipe_hdl, &ipa_ep_cfg);
	ipa_cfg_ep(teth_ctx->a2_ipa_pipe_hdl, &ipa_ep_cfg);

	teth_ctx->is_connected = true;

	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
		teth_ctx->link_protocol = TETH_LINK_PROTOCOL_IP;

	if (teth_ctx->aggr_params_known) {
		res = teth_set_aggregation();
		if (res) {
			TETH_ERR("Failed setting aggregation params\n");
			goto bail;
		}
	}

	/* In case of IP link protocol, complete HW bridge */
	if ((teth_ctx->link_protocol == TETH_LINK_PROTOCOL_IP) &&
	    (!teth_ctx->comp_hw_bridge_in_progress) &&
	    (teth_ctx->aggr_params_known) &&
	    (!teth_ctx->is_hw_bridge_complete)) {
		INIT_WORK(&teth_ctx->comp_hw_bridge_work, complete_hw_bridge);
		teth_ctx->comp_hw_bridge_in_progress = true;
		queue_work(teth_ctx->teth_wq, &teth_ctx->comp_hw_bridge_work);
	}
bail:
	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);
	TETH_DBG_FUNC_EXIT();

	return res;
}
EXPORT_SYMBOL(teth_bridge_connect);

static void set_aggr_default_params(struct teth_aggr_params_link *params)
{
	if (params->max_datagrams == 0)
		params->max_datagrams =
		   TETH_AGGR_MAX_DATAGRAMS_DEFAULT;
	if (params->max_transfer_size_byte == 0)
		params->max_transfer_size_byte =
		   TETH_AGGR_MAX_AGGR_PACKET_SIZE_DEFAULT;
}

/**
 * teth_set_bridge_mode() - set the link protocol (IP / Ethernet)
 */
static void teth_set_bridge_mode(enum teth_link_protocol_type link_protocol)
{
	teth_ctx->link_protocol = link_protocol;
	teth_ctx->is_hw_bridge_complete = false;
	memset(&teth_ctx->mac_addresses, 0, sizeof(teth_ctx->mac_addresses));
}

/**
 * teth_bridge_set_aggr_params() - kernel API to set aggregation parameters
 * @param aggr_params: aggregation parmeters for uplink and downlink
 *
 * Besides setting the aggregation parameters, the function enforces max tranfer
 * size which is less then 8K and also forbids Ethernet link protocol with MBIM
 * aggregation which is not supported by HW.
 */
int teth_bridge_set_aggr_params(struct teth_aggr_params *aggr_params)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	if (!aggr_params) {
		TETH_ERR("Invalid parameter\n");
		return -EINVAL;
	}

	/*
	 * In case the requested max transfer size is larger than 8K, set it to
	 * to the default 8K
	 */
	if (aggr_params->dl.max_transfer_size_byte >
	    TETH_AGGR_MAX_AGGR_PACKET_SIZE_DEFAULT)
		aggr_params->dl.max_transfer_size_byte =
			TETH_AGGR_MAX_AGGR_PACKET_SIZE_DEFAULT;
	if (aggr_params->ul.max_transfer_size_byte >
	    TETH_AGGR_MAX_AGGR_PACKET_SIZE_DEFAULT)
		aggr_params->ul.max_transfer_size_byte =
			TETH_AGGR_MAX_AGGR_PACKET_SIZE_DEFAULT;

	/* Ethernet link protocol and MBIM aggregation is not supported */
	if (teth_ctx->link_protocol == TETH_LINK_PROTOCOL_ETHERNET &&
	    (aggr_params->dl.aggr_prot == TETH_AGGR_PROTOCOL_MBIM ||
	     aggr_params->ul.aggr_prot == TETH_AGGR_PROTOCOL_MBIM)) {
		TETH_ERR("Ethernet with MBIM is not supported.\n");
		return -EINVAL;
	}

	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		return res;
	}

	memcpy(&teth_ctx->aggr_params,
	       aggr_params,
	       sizeof(struct teth_aggr_params));
	set_aggr_default_params(&teth_ctx->aggr_params.dl);
	set_aggr_default_params(&teth_ctx->aggr_params.ul);

	teth_ctx->aggr_params_known = true;
	res = teth_set_aggregation();
	if (res)
		TETH_ERR("Failed setting aggregation params\n");

	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);
	TETH_DBG_FUNC_EXIT();

	return res;
}
EXPORT_SYMBOL(teth_bridge_set_aggr_params);

static long teth_bridge_ioctl(struct file *filp,
			      unsigned int cmd,
			      unsigned long arg)
{
	int res = 0;
	struct teth_aggr_params aggr_params;

	TETH_DBG("cmd=%x nr=%d\n", cmd, _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != TETH_BRIDGE_IOC_MAGIC) ||
	    (_IOC_NR(cmd) >= TETH_BRIDGE_IOCTL_MAX)) {
		TETH_ERR("Invalid ioctl\n");
		return -ENOIOCTLCMD;
	}

	switch (cmd) {
	case TETH_BRIDGE_IOC_SET_BRIDGE_MODE:
		TETH_DBG("TETH_BRIDGE_IOC_SET_BRIDGE_MODE ioctl called\n");
		if (teth_ctx->link_protocol != arg)
			teth_set_bridge_mode(arg);
		break;

	case TETH_BRIDGE_IOC_SET_AGGR_PARAMS:
		TETH_DBG("TETH_BRIDGE_IOC_SET_AGGR_PARAMS ioctl called\n");
		res = copy_from_user(&aggr_params,
				   (struct teth_aggr_params *)arg,
				   sizeof(struct teth_aggr_params));
		if (res) {
			TETH_ERR("Error, res = %d\n", res);
			res = -EFAULT;
			break;
		}

		res = teth_bridge_set_aggr_params(&aggr_params);
		if (res)
			break;

		/* In case of IP link protocol, complete HW bridge */
		if ((teth_ctx->link_protocol == TETH_LINK_PROTOCOL_IP) &&
		    (!teth_ctx->comp_hw_bridge_in_progress) &&
		    (!teth_ctx->is_hw_bridge_complete)) {
			INIT_WORK(&teth_ctx->comp_hw_bridge_work,
				  complete_hw_bridge);
			teth_ctx->comp_hw_bridge_in_progress = true;
			queue_work(teth_ctx->teth_wq,
				   &teth_ctx->comp_hw_bridge_work);
		}
		break;

	case TETH_BRIDGE_IOC_GET_AGGR_PARAMS:
		TETH_DBG("TETH_BRIDGE_IOC_GET_AGGR_PARAMS ioctl called\n");
		if (copy_to_user((u8 *)arg, (u8 *)&teth_ctx->aggr_params,
				   sizeof(struct teth_aggr_params))) {
			res = -EFAULT;
			break;
		}
		break;

	case TETH_BRIDGE_IOC_GET_AGGR_CAPABILITIES:
	{
		u16 sz;
		u16 pyld_sz;
		struct teth_aggr_capabilities caps;

		TETH_DBG("GET_AGGR_CAPABILITIES ioctl called\n");
		sz = sizeof(struct teth_aggr_capabilities);
		if (copy_from_user(&caps,
				   (struct teth_aggr_capabilities *)arg,
				   sz)) {
			res = -EFAULT;
			break;
		}

		if (caps.num_protocols < teth_ctx->aggr_caps->num_protocols) {
			caps.num_protocols = teth_ctx->aggr_caps->num_protocols;
			if (copy_to_user((struct teth_aggr_capabilities *)arg,
					 &caps,
					 sz)) {
				res = -EFAULT;
				break;
			}
			TETH_DBG("Not enough space allocated.\n");
			res = -EAGAIN;
			break;
		}

		pyld_sz = sz + caps.num_protocols *
			sizeof(struct teth_aggr_params_link);

		if (copy_to_user((u8 *)arg,
				 (u8 *)(teth_ctx->aggr_caps),
				 pyld_sz)) {
			res = -EFAULT;
			break;
		}
	}
	break;
	}

	return res;
}

/**
 * set_aggr_capabilities() - allocates and fills the aggregation capabilities
 * struct
 */
static int set_aggr_capabilities(void)
{
	u16 NUM_PROTOCOLS = 2;

	teth_ctx->aggr_caps = kzalloc(sizeof(struct teth_aggr_capabilities) +
				      NUM_PROTOCOLS *
				      sizeof(struct teth_aggr_params_link),
				      GFP_KERNEL);
	if (!teth_ctx->aggr_caps) {
		TETH_ERR("Memory alloc failed for aggregation capabilities.\n");
		return -ENOMEM;
	}

	teth_ctx->aggr_caps->num_protocols = NUM_PROTOCOLS;

	teth_ctx->aggr_caps->prot_caps[0].aggr_prot = TETH_AGGR_PROTOCOL_MBIM;
	set_aggr_default_params(&teth_ctx->aggr_caps->prot_caps[0]);

	teth_ctx->aggr_caps->prot_caps[1].aggr_prot = TETH_AGGR_PROTOCOL_TLP;
	set_aggr_default_params(&teth_ctx->aggr_caps->prot_caps[1]);

	return 0;
}

/**
* teth_bridge_get_client_handles() - Get USB <--> IPA pipe handles
* @producer_handle:	USB --> IPA pipe handle
* @consumer_handle:	IPA --> USB pipe handle
*/
void teth_bridge_get_client_handles(u32 *producer_handle,
		u32 *consumer_handle)
{
	if (producer_handle == NULL || consumer_handle == NULL)
		return;

	*producer_handle = teth_ctx->usb_ipa_pipe_hdl;
	*consumer_handle = teth_ctx->ipa_usb_pipe_hdl;
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static struct dentry *dfile_link_protocol;
static struct dentry *dfile_get_aggr_params;
static struct dentry *dfile_set_aggr_protocol;
static struct dentry *dfile_stats;
static struct dentry *dfile_is_hw_bridge_complete;

static ssize_t teth_debugfs_read_link_protocol(struct file *file,
					       char __user *ubuf,
					       size_t count,
					       loff_t *ppos)
{
	int nbytes;

	nbytes = scnprintf(dbg_buff, TETH_MAX_MSG_LEN, "Link protocol = %s\n",
			   (teth_ctx->link_protocol ==
				TETH_LINK_PROTOCOL_ETHERNET) ?
			   "ETHERNET" :
			   "IP");

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t teth_debugfs_write_link_protocol(struct file *file,
					const char __user *ubuf,
					size_t count,
					loff_t *ppos)
{
	unsigned long missing;
	enum teth_link_protocol_type link_protocol;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	if (count > 0)
		dbg_buff[count-1] = '\0';

	if (strcmp(dbg_buff, "ETHERNET") == 0) {
		link_protocol = TETH_LINK_PROTOCOL_ETHERNET;
	} else if (strcmp(dbg_buff, "IP") == 0) {
		link_protocol = TETH_LINK_PROTOCOL_IP;
	} else {
		TETH_ERR("Bad link protocol, got %s,\n"
			 "Use <ETHERNET> or <IP>.\n", dbg_buff);
		return count;
	}

	teth_set_bridge_mode(link_protocol);

	return count;
}

static ssize_t teth_debugfs_read_aggr_params(struct file *file,
					     char __user *ubuf,
					     size_t count,
					     loff_t *ppos)
{
	int nbytes = 0;
	char aggr_str[20];

	aggr_prot_to_str(teth_ctx->aggr_params.ul.aggr_prot,
			 aggr_str,
			 sizeof(aggr_str)-1);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			   "Aggregation parameters for uplink:\n");
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Aggregation protocol: %s\n",
			    aggr_str);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Max transfer size [byte]: %d\n",
			    teth_ctx->aggr_params.ul.max_transfer_size_byte);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Max datagrams: %d\n",
			    teth_ctx->aggr_params.ul.max_datagrams);

	aggr_prot_to_str(teth_ctx->aggr_params.dl.aggr_prot,
			 aggr_str,
			 sizeof(aggr_str)-1);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN,
			   "Aggregation parameters for downlink:\n");
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Aggregation protocol: %s\n",
			    aggr_str);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Max transfer size [byte]: %d\n",
			    teth_ctx->aggr_params.dl.max_transfer_size_byte);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Max datagrams: %d\n",
			    teth_ctx->aggr_params.dl.max_datagrams);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t teth_debugfs_set_aggr_protocol(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	unsigned long missing;
	enum teth_aggr_protocol_type aggr_prot;
	int res;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	if (count > 0)
		dbg_buff[count-1] = '\0';

	set_aggr_default_params(&teth_ctx->aggr_params.dl);
	set_aggr_default_params(&teth_ctx->aggr_params.ul);

	if (strcmp(dbg_buff, "NONE") == 0) {
		aggr_prot = TETH_AGGR_PROTOCOL_NONE;
	} else if (strcmp(dbg_buff, "MBIM") == 0) {
		aggr_prot = TETH_AGGR_PROTOCOL_MBIM;
	} else if (strcmp(dbg_buff, "TLP") == 0) {
		aggr_prot = TETH_AGGR_PROTOCOL_TLP;
	} else {
		TETH_ERR("Bad aggregation protocol, got %s,\n"
			 "Use <NONE>, <MBIM> or <TLP>.\n", dbg_buff);
		return count;
	}

	teth_ctx->aggr_params.dl.aggr_prot = aggr_prot;
	teth_ctx->aggr_params.ul.aggr_prot = aggr_prot;
	teth_ctx->aggr_params_known = true;

	res = teth_set_aggregation();
	if (res)
		TETH_ERR("Failed setting aggregation params\n");

	return count;
}

static ssize_t teth_debugfs_stats(struct file *file,
				  char __user *ubuf,
				  size_t count,
				  loff_t *ppos)
{
	int nbytes = 0;

	nbytes += scnprintf(&dbg_buff[nbytes],
			    TETH_MAX_MSG_LEN - nbytes,
			   "USB to A2 SW Tx packets: %lld\n",
			    teth_ctx->stats.usb_to_a2_num_sw_tx_packets);
	nbytes += scnprintf(&dbg_buff[nbytes],
			    TETH_MAX_MSG_LEN - nbytes,
			   "A2 to USB SW Tx packets: %lld\n",
			    teth_ctx->stats.a2_to_usb_num_sw_tx_packets);
	nbytes += scnprintf(
		&dbg_buff[nbytes],
		TETH_MAX_MSG_LEN - nbytes,
		"SW Tx packets sent during resource wakeup: %lld\n",
		teth_ctx->stats.num_sw_tx_packets_during_resource_wakeup);
	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t teth_debugfs_hw_bridge_status(struct file *file,
					     char __user *ubuf,
					     size_t count,
					     loff_t *ppos)
{
	int nbytes = 0;

	if (teth_ctx->is_hw_bridge_complete)
		nbytes += scnprintf(&dbg_buff[nbytes],
				    TETH_MAX_MSG_LEN - nbytes,
				   "HW bridge is in use.\n");
	else
		nbytes += scnprintf(&dbg_buff[nbytes],
				    TETH_MAX_MSG_LEN - nbytes,
				   "SW bridge is in use. HW bridge not complete yet.\n");

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

const struct file_operations teth_link_protocol_ops = {
	.read = teth_debugfs_read_link_protocol,
	.write = teth_debugfs_write_link_protocol,
};

const struct file_operations teth_get_aggr_params_ops = {
	.read = teth_debugfs_read_aggr_params,
};

const struct file_operations teth_set_aggr_protocol_ops = {
	.write = teth_debugfs_set_aggr_protocol,
};

const struct file_operations teth_stats_ops = {
	.read = teth_debugfs_stats,
};

const struct file_operations teth_hw_bridge_status_ops = {
	.read = teth_debugfs_hw_bridge_status,
};

void teth_debugfs_init(void)
{
	const mode_t read_only_mode = S_IRUSR | S_IRGRP | S_IROTH;
	const mode_t read_write_mode = S_IRUSR | S_IRGRP | S_IROTH |
			S_IWUSR | S_IWGRP | S_IWOTH;

	dent = debugfs_create_dir("ipa_teth", 0);
	if (IS_ERR(dent)) {
		IPAERR("fail to create folder ipa_teth debug_fs.\n");
		return;
	}

	dfile_link_protocol =
		debugfs_create_file("link_protocol", read_write_mode, dent, 0,
				    &teth_link_protocol_ops);
	if (!dfile_link_protocol || IS_ERR(dfile_link_protocol)) {
		IPAERR("fail to create file link_protocol\n");
		goto fail;
	}

	dfile_get_aggr_params =
		debugfs_create_file("get_aggr_params", read_only_mode, dent, 0,
				    &teth_get_aggr_params_ops);
	if (!dfile_get_aggr_params || IS_ERR(dfile_get_aggr_params)) {
		IPAERR("fail to create file get_aggr_params\n");
		goto fail;
	}

	dfile_set_aggr_protocol =
		debugfs_create_file("set_aggr_protocol", read_only_mode, dent,
				    0, &teth_set_aggr_protocol_ops);
	if (!dfile_set_aggr_protocol || IS_ERR(dfile_set_aggr_protocol)) {
		IPAERR("fail to create file set_aggr_protocol\n");
		goto fail;
	}

	dfile_stats =
		debugfs_create_file("stats", read_only_mode, dent,
				    0, &teth_stats_ops);
	if (!dfile_stats || IS_ERR(dfile_stats)) {
		IPAERR("fail to create file stats\n");
		goto fail;
	}

	dfile_is_hw_bridge_complete =
		debugfs_create_file("is_hw_bridge_complete", read_only_mode,
				    dent, 0, &teth_hw_bridge_status_ops);
	if (!dfile_is_hw_bridge_complete ||
	    IS_ERR(dfile_is_hw_bridge_complete)) {
		IPAERR("fail to create file is_hw_bridge_complete\n");
		goto fail;
	}

	return;
fail:
	debugfs_remove_recursive(dent);
}
#else
void teth_debugfs_init(void) {}
#endif /* CONFIG_DEBUG_FS */


static const struct file_operations teth_bridge_drv_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = teth_bridge_ioctl,
};

/**
* teth_bridge_driver_init() - Initialize tethering bridge driver
*
*/
int teth_bridge_driver_init(void)
{
	int res;
	struct ipa_rm_create_params bridge_prod_params;

	TETH_DBG("Tethering bridge driver init\n");
	teth_ctx = kzalloc(sizeof(*teth_ctx), GFP_KERNEL);
	if (!teth_ctx) {
		TETH_ERR("kzalloc err.\n");
		return -ENOMEM;
	}

	res = set_aggr_capabilities();
	if (res) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_aggr_caps;
	}

	res = -ENOMEM;
	teth_ctx->hdr_del = kzalloc(sizeof(struct ipa_ioc_del_hdr) +
				    TETH_TOTAL_HDR_ENTRIES *
				    sizeof(struct ipa_hdr_del),
				    GFP_KERNEL);
	if (!teth_ctx->hdr_del) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_hdr_del;
	}

	teth_ctx->routing_del[IPA_IP_v4] =
		kzalloc(sizeof(struct ipa_ioc_del_rt_rule) +
			TETH_TOTAL_RT_ENTRIES_IP *
			sizeof(struct ipa_rt_rule_del),
			GFP_KERNEL);
	if (!teth_ctx->routing_del[IPA_IP_v4]) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_routing_del_ipv4;
	}
	teth_ctx->routing_del[IPA_IP_v6] =
		kzalloc(sizeof(struct ipa_ioc_del_rt_rule) +
			TETH_TOTAL_RT_ENTRIES_IP *
			sizeof(struct ipa_rt_rule_del),
			GFP_KERNEL);
	if (!teth_ctx->routing_del[IPA_IP_v6]) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_routing_del_ipv6;
	}

	teth_ctx->filtering_del[IPA_IP_v4] =
		kzalloc(sizeof(struct ipa_ioc_del_flt_rule) +
			TETH_TOTAL_FLT_ENTRIES_IP *
			sizeof(struct ipa_flt_rule_del),
			GFP_KERNEL);
	if (!teth_ctx->filtering_del[IPA_IP_v4]) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_filtering_del_ipv4;
	}
	teth_ctx->filtering_del[IPA_IP_v6] =
		kzalloc(sizeof(struct ipa_ioc_del_flt_rule) +
			TETH_TOTAL_FLT_ENTRIES_IP *
			sizeof(struct ipa_flt_rule_del),
			GFP_KERNEL);
	if (!teth_ctx->filtering_del[IPA_IP_v6]) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_filtering_del_ipv6;
	}

	teth_ctx->class = class_create(THIS_MODULE, TETH_BRIDGE_DRV_NAME);

	res = alloc_chrdev_region(&teth_ctx->dev_num, 0, 1,
				  TETH_BRIDGE_DRV_NAME);
	if (res) {
		TETH_ERR("alloc_chrdev_region err.\n");
		res = -ENODEV;
		goto fail_alloc_chrdev_region;
	}

	teth_ctx->dev = device_create(teth_ctx->class, NULL, teth_ctx->dev_num,
				      teth_ctx, TETH_BRIDGE_DRV_NAME);
	if (IS_ERR(teth_ctx->dev)) {
		TETH_ERR(":device_create err.\n");
		res = -ENODEV;
		goto fail_device_create;
	}

	cdev_init(&teth_ctx->cdev, &teth_bridge_drv_fops);
	teth_ctx->cdev.owner = THIS_MODULE;
	teth_ctx->cdev.ops = &teth_bridge_drv_fops;

	res = cdev_add(&teth_ctx->cdev, teth_ctx->dev_num, 1);
	if (res) {
		TETH_ERR(":cdev_add err=%d\n", -res);
		res = -ENODEV;
		goto fail_cdev_add;
	}

	teth_debugfs_init();

	/* Create BRIDGE_PROD entity in IPA Resource Manager */
	bridge_prod_params.name = IPA_RM_RESOURCE_BRIDGE_PROD;
	bridge_prod_params.reg_params.user_data = NULL;
	bridge_prod_params.reg_params.notify_cb = bridge_prod_notify_cb;
	res = ipa_rm_create_resource(&bridge_prod_params);
	if (res) {
		TETH_ERR("ipa_rm_create_resource() failed\n");
		goto fail_cdev_add;
	}
	init_completion(&teth_ctx->is_bridge_prod_up);
	init_completion(&teth_ctx->is_bridge_prod_down);

	res = ipa_rm_inactivity_timer_init(IPA_RM_RESOURCE_BRIDGE_PROD,
					   TETH_INACTIVITY_TIME_MSEC);
	if (res) {
		TETH_ERR("ipa_rm_inactivity_timer_init() failed, res=%d\n",
			 res);
		goto fail_cdev_add;
	}

	teth_ctx->teth_wq = create_workqueue(TETH_WORKQUEUE_NAME);
	if (!teth_ctx->teth_wq) {
		TETH_ERR("workqueue creation failed\n");
		goto fail_cdev_add;
	}

	initialize_context();
	TETH_DBG("Tethering bridge driver init OK\n");

	return 0;
fail_cdev_add:
	device_destroy(teth_ctx->class, teth_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(teth_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	kfree(teth_ctx->filtering_del[IPA_IP_v6]);
fail_alloc_filtering_del_ipv6:
	kfree(teth_ctx->filtering_del[IPA_IP_v4]);
fail_alloc_filtering_del_ipv4:
	kfree(teth_ctx->routing_del[IPA_IP_v6]);
fail_alloc_routing_del_ipv6:
	kfree(teth_ctx->routing_del[IPA_IP_v4]);
fail_alloc_routing_del_ipv4:
	kfree(teth_ctx->hdr_del);
fail_alloc_hdr_del:
	kfree(teth_ctx->aggr_caps);
fail_alloc_aggr_caps:
	kfree(teth_ctx);
	teth_ctx = NULL;

	return res;
}
EXPORT_SYMBOL(teth_bridge_driver_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tethering bridge driver");
