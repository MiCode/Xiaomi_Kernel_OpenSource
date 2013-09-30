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

#define USB_TO_A2_RT_TBL_NAME_IPV4 "usb_a2_rt_ipv4"
#define A2_TO_USB_RT_TBL_NAME_IPV4 "a2_usb_rt_ipv4"
#define USB_TO_A2_RT_TBL_NAME_IPV6 "usb_a2_rt_ipv6"
#define A2_TO_USB_RT_TBL_NAME_IPV6 "a2_usb_rt_ipv6"

#define MBIM_HEADER_NAME "mbim_header"
#define TETH_DEFAULT_AGGR_TIME_LIMIT 1

#define ETHERTYPE_IPV4 0x0800
#define ETHERTYPE_IPV6 0x86DD

#define TETH_AGGR_MAX_DATAGRAMS_DEFAULT 16
#define TETH_AGGR_MAX_AGGR_PACKET_SIZE_DEFAULT (8*1024)

#define TETH_MTU_BYTE 1500

#define TETH_INACTIVITY_TIME_MSEC (1000)

#define TETH_WORKQUEUE_NAME "tethering_bridge_wq"

#define TETH_TOTAL_HDR_ENTRIES 8
#define TETH_TOTAL_RT_ENTRIES_IP 3
#define TETH_TOTAL_FLT_ENTRIES_IP 2
#define TETH_IP_FAMILIES 2

#define METADATA_SHFT 16
#define METADATA_MASK 0x00FF0000

#define TETH_NUM_CHANNELS 12

#define TETH_METADATA_LEN 4

#define MAX_MBIM_STREAMS 8

/**
 * enum teth_init_status - bridge initialization state
 *			(NOT_INITIALIZED / INITIALIZED/ ERROR)
 */
enum teth_init_status {
	TETH_NOT_INITIALIZED,
	TETH_INITIALIZED,
	TETH_INITIALIZATION_ERROR,
};

/**
 * enum teth_ch_type - channel type (Embedded or Tethered)
 */
enum teth_ch_type {
	TETH_EMBEDDED_CH,
	TETH_TETHERED_CH,
};

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
 * struct hw_bridge_work_wrap - wrapper for the channel number which is sent
 * when using a workqueue
 * @work: used by the workqueue
 * @lcid: logic channel number
 */
struct hw_bridge_work_wrap {
	struct work_struct comp_hw_bridge_work;
	u16 lcid;
};

/**
 * struct teth_bridge_ctx - Tethering bridge driver context information
 * @usb_ipa_pipe_hdl: USB to IPA pipe handle
 * @ipa_usb_pipe_hdl: IPA to USB pipe handle
 * @is_connected: is the tethered bridge connected ?
 * @link_protocol: IP / Ethernet
 * @is_hw_bridge_complete: is HW bridge setup ?
 * @aggr_params: aggregation parmeters
 * @aggr_params_known: are the aggregation parameters known ?
 * @hw_bridge_work_wrap: used for setting up the HW bridge using a workqueue
 * @comp_hw_bridge_in_progress: true when the HW bridge setup is in progress
 * @ch_type: Is this channel tethered or embedded ?
 * @routing_del: array of routing rules handles, one array for IPv4 and one for
 * IPv6
 * @filtering_del: array of routing rules handles, one array for IPv4 and one
 * for IPv6
 */
struct logic_ch_info {
	u32 usb_ipa_pipe_hdl;
	u32 ipa_usb_pipe_hdl;
	bool is_connected;
	enum teth_link_protocol_type link_protocol;
	bool is_hw_bridge_complete;
	struct teth_aggr_params aggr_params;
	bool aggr_params_known;
	struct hw_bridge_work_wrap hw_bridge_work;
	bool comp_hw_bridge_in_progress;
	enum teth_ch_type ch_type;
	struct ipa_ioc_del_rt_rule *routing_del[TETH_IP_FAMILIES];
	struct ipa_ioc_del_flt_rule *filtering_del[TETH_IP_FAMILIES];
};

/**
 * struct teth_bridge_ctx - Tethering bridge driver context information
 * @class: kernel class pointer
 * @dev_num: kernel device number
 * @dev: kernel device struct pointer
 * @cdev: kernel character device struct
 * @a2_ipa_pipe_hdl: A2 to IPA pipe handle
 * @ipa_a2_pipe_hdl: IPA to A2 pipe handle
 * @mac_addresses: Struct which holds host pc and device MAC addresses, relevant
 * in ethernet mode only
 * @tethering_mode: Rmnet / MBIM
 * @is_bridge_prod_up: completion object signaled when the bridge producer
 * finished its resource request procedure
 * @is_bridge_prod_down: completion object signaled when the bridge producer
 * finished its resource release procedure
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
 * @ch_info: array of logic_ch_info, used to hold channel information
 * @logic_ch_num: the total logical channels number
 * @ch_init_cnt: count the initialized channels
 * @init_status: bridge initialization state
 * @init_mutex: for the initialization, connect and disconnect synchronization
 * @request_resource_mutex: for the teth_request_resource synchronization
 * @debugfs_lcid: logical channel number for debugfs entries
 */
struct teth_bridge_ctx {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	u32 a2_ipa_pipe_hdl;
	u32 ipa_a2_pipe_hdl;
	struct mac_addresses_type mac_addresses;
	enum teth_tethering_mode tethering_mode;
	u16 mbim_stream_id_to_channel_id[IPA_MBIM_MAX_STREAM_NUM];
	struct completion is_bridge_prod_up;
	struct completion is_bridge_prod_down;
	struct teth_aggr_capabilities *aggr_caps;
	struct stats stats;
	struct workqueue_struct *teth_wq;
	u16 a2_ipa_hdr_len;
	u16 ipa_a2_hdr_len;
	struct ipa_ioc_del_hdr *hdr_del;
	struct logic_ch_info *ch_info;
	u16 ch_init_cnt;
	enum teth_init_status init_status;
	struct mutex init_mutex;
	struct mutex request_resource_mutex;
	u16 debugfs_lcid;
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
 * @lcid: logical channel number
 */
struct teth_work {
	struct work_struct work;
	struct sk_buff *skb;
	enum teth_packet_direction dir;
	enum a2_mux_logical_channel_id lcid;
	struct ipa_tx_meta metadata;
};

#ifdef CONFIG_DEBUG_FS
#define TETH_MAX_MSG_LEN 512
static char dbg_buff[TETH_MAX_MSG_LEN];
#endif

static u16 get_channel_id_from_client_prod(enum ipa_client_type client)
{
	TETH_DBG("client_id=%d\n", client);
	if (client == IPA_CLIENT_USB_PROD)
		return A2_MUX_TETHERED_0;
	if (client > IPA_CLIENT_USB_PROD || client <= IPA_CLIENT_PROD) {
		TETH_ERR("%s: Invalid client type %d\n", __func__, client);
		return A2_MUX_TETHERED_0;
	}

	return client - IPA_CLIENT_USB2_PROD + A2_MUX_MULTI_RMNET_10;
}

static u16 get_cons_client(enum a2_mux_logical_channel_id lcid)
{
	TETH_DBG("lcid=%d\n", lcid);

	if (lcid < A2_MUX_TETHERED_0 || lcid >= A2_MUX_NUM_CHANNELS ||
			lcid == A2_MUX_RESERVED_9) {
		TETH_ERR("%s: Invalid lcid %d\n", __func__, lcid);
		return IPA_CLIENT_USB_CONS;
	}
	if (lcid == A2_MUX_TETHERED_0)
		return IPA_CLIENT_USB_CONS;

	return lcid - A2_MUX_MULTI_RMNET_10 + IPA_CLIENT_USB2_CONS;
}

static u16 get_prod_client(enum a2_mux_logical_channel_id lcid)
{
	TETH_DBG("lcid=%d\n", lcid);
	if (lcid < A2_MUX_TETHERED_0 || lcid >= A2_MUX_NUM_CHANNELS ||
				lcid == A2_MUX_RESERVED_9) {
			TETH_ERR("%s: Invalid lcid %d\n", __func__, lcid);
			return IPA_CLIENT_USB_PROD;
	}
	if (lcid == A2_MUX_TETHERED_0)
		return IPA_CLIENT_USB_PROD;

	return lcid - A2_MUX_MULTI_RMNET_10 + IPA_CLIENT_USB2_PROD;
}

static u16 get_ch_info_idx(enum a2_mux_logical_channel_id lcid)
{
	if (lcid < A2_MUX_TETHERED_0 || lcid >= A2_MUX_NUM_CHANNELS ||
			lcid == A2_MUX_RESERVED_9) {
			TETH_ERR("%s: Invalid lcid %d\n", __func__, lcid);
			return 0;
	}
	if (lcid == A2_MUX_TETHERED_0)
		return 0;

	return lcid - A2_MUX_RESERVED_9;
}

static int get_completed_ch_num(void)
{
	int idx;
	int cnt = 0;

	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++) {
		if (teth_ctx->ch_info[idx].is_hw_bridge_complete)
			cnt++;
	}
	TETH_DBG("completed_ch_num=%d\n", cnt);

	return cnt;
}

static int get_connected_ch_num(void)
{
	int idx;
	int cnt = 0;

	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++) {
		if (teth_ctx->ch_info[idx].is_connected)
			cnt++;
	}
	TETH_DBG("connected_ch_num=%d\n", cnt);

	return cnt;
}

/**
 * add_eth_hdrs_internal() - add Ethernet headers to IPA
 * @hdr_name_ipv4: header name for IPv4
 * @hdr_name_ipv6: header name for IPv6
 * @src_mac_addr: source MAC address
 * @dst_mac_addr: destination MAC address
 *
 * This function is called only when link protocol is Ethernet
 */
static int add_eth_hdrs_internal(char *hdr_name_ipv4, char *hdr_name_ipv6,
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

/**
 * add_eth_hdrs() - add Ethernet headers to IPA
 * This function is called only when link protocol is Ethernet
 */
static int add_eth_hdrs(void)
{
	int res;

	/* Add a header entry for USB */
	res = add_eth_hdrs_internal(USB_ETH_HDR_NAME_IPV4,
			   USB_ETH_HDR_NAME_IPV6,
			   teth_ctx->mac_addresses.device_mac_addr,
			   teth_ctx->mac_addresses.host_pc_mac_addr);
	if (res) {
		TETH_ERR("Failed adding USB Ethernet header\n");
		goto bail;
	}
	TETH_DBG("Added USB Ethernet headers (IPv4 / IPv6)\n");

	/* Add a header entry for A2 */
	res = add_eth_hdrs_internal(A2_ETH_HDR_NAME_IPV4,
			   A2_ETH_HDR_NAME_IPV6,
			   teth_ctx->mac_addresses.host_pc_mac_addr,
			   teth_ctx->mac_addresses.device_mac_addr);
	if (res) {
		TETH_ERR("Failed adding A2 Ethernet header\n");
		goto bail;
	}
	TETH_DBG("Added A2 Ethernet headers (IPv4 / IPv6\n");
bail:
	return res;
}

/**
 * configure_ipa_header_block_a2_internal() - configures IPA end-point registers
 * (header removal/insertion for IPA<->A2 pipes)
 * @a2_ipa_hdr_len: Header length in bytes to be added/removed.
 * @ipa_a2_hdr_len: Header length in bytes to be added/removed.
 */
static int configure_ipa_header_block_a2_internal(u32 a2_ipa_hdr_len,
					       u32 ipa_a2_hdr_len)
{
	struct ipa_ep_cfg_hdr hdr_cfg;
	int res;

	TETH_DBG_FUNC_ENTRY();
	/* Configure header removal for the A2->IPA pipe */
	memset(&hdr_cfg, 0, sizeof(hdr_cfg));

	hdr_cfg.hdr_len = a2_ipa_hdr_len;
	teth_ctx->a2_ipa_hdr_len = a2_ipa_hdr_len;
	res = ipa_cfg_ep_hdr(teth_ctx->a2_ipa_pipe_hdl, &hdr_cfg);
	if (res) {
		TETH_ERR("Header removal config for A2->IPA pipe failed\n");
		goto bail;
	}

	/* Configure header insertion for the IPA->A2 pipe */
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

/**
 * configure_ipa_header_block_usb_internal() - configures IPA end-point
 * registers (header removal/insertion for IPA<->USB pipes)
 * @usb_ipa_hdr_len: Header length in bytes to be added/removed.
 * @ipa_usb_hdr_len: Header length in bytes to be added/removed.
 * @lcid: logical channel number
 */
static int configure_ipa_header_block_usb_internal(u32 usb_ipa_hdr_len,
					       u32 ipa_usb_hdr_len,
					       u16 lcid)
{
	struct ipa_ep_cfg_hdr hdr_cfg;
	int res;
	u16 idx;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);

	TETH_DBG(
		"Configure header removal for the USB->IPA pipe(lcid=%d). hdr_len=%d, usb_ipa_pipe_hdl=%d\n ",
		lcid,
		usb_ipa_hdr_len,
		teth_ctx->ch_info[idx].usb_ipa_pipe_hdl);

	/* Configure header removal for the USB->IPA pipe */
	memset(&hdr_cfg, 0, sizeof(hdr_cfg));
	hdr_cfg.hdr_len = usb_ipa_hdr_len;
	res = ipa_cfg_ep_hdr(teth_ctx->ch_info[idx].usb_ipa_pipe_hdl,
			&hdr_cfg);
	if (res) {
		TETH_ERR("Header removal config for USB->IPA pipe failed\n");
		goto bail;
	}

	TETH_DBG(
		"Configure header insertion for the IPA->USB pipe(lcid=%d). hdr_len=%d, ipa_usb_pipe_hdl=%d\n ",
		lcid,
		ipa_usb_hdr_len,
		teth_ctx->ch_info[idx].ipa_usb_pipe_hdl);

	/* Configure header insertion for the IPA->USB pipe */
	hdr_cfg.hdr_len = ipa_usb_hdr_len;
	res = ipa_cfg_ep_hdr(teth_ctx->ch_info[idx].ipa_usb_pipe_hdl,
			&hdr_cfg);
	if (res) {
		TETH_ERR("Header insertion config for IPA->USB pipe failed\n");
		goto bail;
	}

	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

/**
 * add_mbim_hdrl() - Adding a single MBIM hdr according to his stream_id
 * @mbim_stream_id: The MBIM stream id
 */

static int add_mbim_hdr(u16 mbim_stream_id)
{
	int res;
	struct ipa_ioc_add_hdr *mbim_hdr;
	int idx;
	char mbim_header_name[IPA_RESOURCE_NAME_MAX] = { '\0' };

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
	snprintf(mbim_header_name,
			  IPA_RESOURCE_NAME_MAX,
			  "%s_%d", MBIM_HEADER_NAME,
			mbim_stream_id);
	strlcpy(mbim_hdr->hdr[0].name, mbim_header_name, IPA_RESOURCE_NAME_MAX);
	memcpy(mbim_hdr->hdr[0].hdr, &mbim_stream_id, sizeof(u8));
	mbim_hdr->hdr[0].hdr_len = sizeof(u8);
	mbim_hdr->hdr[0].is_partial = false;
	res = ipa_add_hdr(mbim_hdr);
	if (res || mbim_hdr->hdr[0].status) {
		TETH_ERR("Failed adding MBIM header %d\n", mbim_stream_id);
		res = -EFAULT;
		goto bail;
	} else {
		TETH_DBG("Added MBIM header stream ID %d\n", mbim_stream_id);
	}

	/* Save the header handle in order to delete it later */
	idx = teth_ctx->hdr_del->num_hdls++;
	teth_ctx->hdr_del->hdl[idx].hdl = mbim_hdr->hdr[0].hdr_hdl;

	kfree(mbim_hdr);
	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

/**
 * configure_ipa_header_block_ip() - configures endpoint registers for IP
 * link protocol. If MBIM aggregation add MBIM header.
 * @lcid: logical channel number
 */
static int configure_ipa_header_block_ip(u16 lcid)
{
	int res;
	u32 usb_ipa_hdr_len = 0;
	u32 ipa_usb_hdr_len = 0;
	u32 ipa_a2_hdr_len = 0;
	u16 idx;
	u16 stream_id;
	u16 num_of_iterations = 1;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);

	if (teth_ctx->ch_info[idx].aggr_params.dl.aggr_prot ==
			TETH_AGGR_PROTOCOL_MBIM)
		ipa_usb_hdr_len = 1;

	if (get_completed_ch_num() == 0) {
		/*
		 * Create a new header for MBIM stream ID and associate
		 * it with the IPA->USB routing table
		 */
		if (teth_ctx->ch_info[idx].aggr_params.dl.aggr_prot ==
					TETH_AGGR_PROTOCOL_MBIM) {
			if (teth_ctx->tethering_mode
					== TETH_TETHERING_MODE_MBIM)
				num_of_iterations = IPA_MBIM_MAX_STREAM_NUM;
			for (stream_id = 0; stream_id < num_of_iterations;
					stream_id++) {
				res = add_mbim_hdr(stream_id);
				if (res) {
					TETH_ERR("adding MBIM header %d fail\n"
							, stream_id);
					goto bail;
				}
			}
		}
	}
	/*
	 * Configure only the tethered pipe, don't need to configure the
	 * embedded pipes, the a2_service does it (in the connect_to_bam)
	 */
	if (teth_ctx->ch_info[idx].ch_type == TETH_TETHERED_CH) {
		res = configure_ipa_header_block_a2_internal(ipa_a2_hdr_len,
							ipa_a2_hdr_len);
		if (res) {
			TETH_ERR(
				"Configuration of header removal/insertion for A2<->IPA failed\n");
			goto bail;
		}
	}

	res = configure_ipa_header_block_usb_internal(usb_ipa_hdr_len,
							  ipa_usb_hdr_len,
							  lcid);
	if (res) {
		TETH_ERR(
			"Configuration of header removal/insertion for USB<->IPA failed\n");
		goto bail;
	}

	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

/**
 * configure_ipa_header_block_ethernet() - add Ethernet headers and configures
 * endpoint registers for Ethernet link protocol.
 * @lcid: logical channel number
 */
static int configure_ipa_header_block_ethernet(u16 lcid)
{
	int res;
	u32 ipa_usb_hdr_len = ETH_HLEN;
	u32 ipa_a2_hdr_len = ETH_HLEN;
	int idx;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);

	res = add_eth_hdrs();
	if (res) {
		TETH_ERR("Failed adding Ethernet header\n");
		goto bail;
	}

	res = configure_ipa_header_block_a2_internal(ipa_a2_hdr_len,
						     ipa_a2_hdr_len);
	if (res) {
		TETH_ERR(
			"Configuration of header removal/insertion for A2<->IPA failed\n");
		goto bail;
	}

	res = configure_ipa_header_block_usb_internal(ipa_usb_hdr_len,
						      ipa_usb_hdr_len,
						      lcid);
	if (res) {
		TETH_ERR(
			"Configuration of header removal/insertion for USB<->IPA failed\n");
		goto bail;
	}

	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

/**
 * configure_ipa_header_block() - adds headers and configures endpoint registers
 * @lcid: logical channel number
 * - For IP link protocol and MBIM aggregation, configure MBIM header
 * - For Ethernet link protocol, configure Ethernet headers
 */
static int configure_ipa_header_block(u16 lcid)
{
	u16 idx;
	int res = -EINVAL;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);

	if (teth_ctx->ch_info[idx].link_protocol ==
					TETH_LINK_PROTOCOL_IP)
		res = configure_ipa_header_block_ip(lcid);
	else if (teth_ctx->ch_info[idx].link_protocol ==
			TETH_LINK_PROTOCOL_ETHERNET)
		res = configure_ipa_header_block_ethernet(lcid);
	TETH_DBG_FUNC_EXIT();

	return res;
}

static int configure_routing_by_ip(char *hdr_name,
			    char *rt_tbl_name,
			    enum ipa_client_type dst,
			    enum ipa_ip_type ip_address_family,
			    u16  lcid)
{

	struct ipa_ioc_add_rt_rule *rt_rule;
	struct ipa_ioc_get_hdr hdr_info;
	int res;
	int idx;
	int i;

	TETH_DBG_FUNC_ENTRY();
	i = get_ch_info_idx(lcid);
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
	idx = teth_ctx->ch_info[i].routing_del[ip_address_family]->num_hdls++;
	teth_ctx->ch_info[i].routing_del[ip_address_family]->hdl[idx].hdl =
		rt_rule->rules[0].rt_rule_hdl;

	kfree(rt_rule);
	TETH_DBG_FUNC_EXIT();

	return res;
}

static int configure_routing(char *hdr_name_ipv4,
			     char *rt_tbl_name_ipv4,
			     char *hdr_name_ipv6,
			     char *rt_tbl_name_ipv6,
			     enum ipa_client_type dst,
			     u16 lcid)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	/* Configure IPv4 routing table */
	res = configure_routing_by_ip(hdr_name_ipv4,
				      rt_tbl_name_ipv4,
				      dst,
				      IPA_IP_v4,
				      lcid);
	if (res) {
		TETH_ERR("Failed adding IPv4 routing table\n");
		goto bail;
	}

	/* Configure IPv6 routing table */
	res = configure_routing_by_ip(hdr_name_ipv6,
				      rt_tbl_name_ipv6,
				      dst,
				      IPA_IP_v6,
				      lcid);
	if (res) {
		TETH_ERR("Failed adding IPv6 routing table\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

/**
 * configure_ul_header_routing() - Configure the IPA routing block:
 * route all packets from pipe #n(taken from lcid) to A2 (USB->A2)
 * @lcid: logical channel number
 * @rt_tbl_name_ipv4: IPv4 routing table name
 * @rt_tbl_name_ipv6: IPv6 routing table name
 */
static int configure_ul_header_routing(u16 lcid, char *rt_tbl_name_ipv4,
		     char *rt_tbl_name_ipv6)
{
	char hdr_name_ipv4[IPA_RESOURCE_NAME_MAX] = {'\0'};
	char hdr_name_ipv6[IPA_RESOURCE_NAME_MAX] = {'\0'};
	int res;
	u16 idx;
	enum ipa_client_type dst;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);

	if (teth_ctx->ch_info[idx].ch_type == TETH_EMBEDDED_CH) {
		dst = IPA_CLIENT_A2_EMBEDDED_CONS;
		if (teth_ctx->ch_info[idx].link_protocol ==
		   TETH_LINK_PROTOCOL_IP) {
			snprintf(hdr_name_ipv4, IPA_RESOURCE_NAME_MAX, "%s%d",
					A2_MUX_HDR_NAME_V4_PREF, lcid);
			snprintf(hdr_name_ipv6, IPA_RESOURCE_NAME_MAX, "%s%d",
					A2_MUX_HDR_NAME_V6_PREF, lcid);
		}
	} else {
		dst = IPA_CLIENT_A2_TETHERED_CONS;
		if (teth_ctx->ch_info[idx].link_protocol ==
		    TETH_LINK_PROTOCOL_ETHERNET) {
			strlcpy(hdr_name_ipv4,
				A2_ETH_HDR_NAME_IPV4,
				IPA_RESOURCE_NAME_MAX);
			strlcpy(hdr_name_ipv6,
				A2_ETH_HDR_NAME_IPV6,
				IPA_RESOURCE_NAME_MAX);
		}
	}

	res = configure_routing(hdr_name_ipv4,
				rt_tbl_name_ipv4,
				hdr_name_ipv6,
				rt_tbl_name_ipv6,
				dst,
				lcid);
	if (res) {
		TETH_ERR("USB to A2 routing block configuration failed\n");
		goto bail;
	}
bail:
	TETH_DBG_FUNC_EXIT();
	return res;

}

/**
 * find_mbim_stream_id() - mapping between the lcid and the stream_id
 * @lcid: The logical channel ID
 */

static s16 find_mbim_stream_id(u16 lcid)
{
	int i;

	for (i = 0; i < IPA_MBIM_MAX_STREAM_NUM; i++) {
		if (lcid == teth_ctx->mbim_stream_id_to_channel_id[i])
			return i;
	}

	return -EINVAL;
}

/**
 * configure_dl_header_routing() - Configure the IPA routing block:
 * route all incoming packets to the corresponding output pipe (A2->USB)
 * @lcid: logical channel number
 * @rt_tbl_name_ipv4: IPv4 routing table name
 * @rt_tbl_name_ipv6: IPv6 routing table name
 */
static int configure_dl_header_routing(u16 lcid,  char *rt_tbl_name_ipv4,
		     char *rt_tbl_name_ipv6)
{
	char hdr_name_ipv4[IPA_RESOURCE_NAME_MAX] = {'\0'};
	char hdr_name_ipv6[IPA_RESOURCE_NAME_MAX] = {'\0'};
	int res;
	u16 idx;
	u16 cons_client;
	TETH_DBG_FUNC_ENTRY();

	idx = get_ch_info_idx(lcid);

	if (teth_ctx->ch_info[idx].link_protocol ==
				TETH_LINK_PROTOCOL_ETHERNET) {
		strlcpy(hdr_name_ipv4,
			USB_ETH_HDR_NAME_IPV4,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(hdr_name_ipv6,
			USB_ETH_HDR_NAME_IPV6,
			IPA_RESOURCE_NAME_MAX);
	} else if (teth_ctx->ch_info[idx].aggr_params.dl.aggr_prot ==
					TETH_AGGR_PROTOCOL_MBIM) {
		s16 stream_id = 0;
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM) {
			stream_id = find_mbim_stream_id(lcid);
			if (lcid < 0) {
				res = -EFAULT;
				TETH_ERR("Bad LCID %d for multi MBIM\n", lcid);
				goto bail;
			}
		}
		snprintf(hdr_name_ipv4, IPA_RESOURCE_NAME_MAX, "%s_%d",
			MBIM_HEADER_NAME,
			stream_id);
		snprintf(hdr_name_ipv6, IPA_RESOURCE_NAME_MAX, "%s_%d",
			MBIM_HEADER_NAME,
			stream_id);
	}

	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
		cons_client = IPA_CLIENT_USB_CONS;
	else
		cons_client = get_cons_client(lcid);
	res = configure_routing(hdr_name_ipv4,
					rt_tbl_name_ipv4,
					hdr_name_ipv6,
					rt_tbl_name_ipv6,
					cons_client,
					lcid);

	if (res) {
		TETH_ERR("USB to A2 routing block configuration failed\n");
		goto bail;
	}
bail:
	return res;
}

/**
 * configure_ipa_routing_block() - Configure the IPA routing block
 * @lcid: logical channel number
 * This function configures IPA for:
 * - Route all packets from USB to A2
 * - Route all packets from A2 to USB
 * - Use the correct headers in Ethernet or MBIM cases
 */
static int configure_ipa_routing_block(u16 lcid)
{
	char rt_tbl_name_ipv4[IPA_RESOURCE_NAME_MAX] = {'\0'};
	char rt_tbl_name_ipv6[IPA_RESOURCE_NAME_MAX] = {'\0'};
	int res;
	u16 idx;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);
	/* Configure USB -> A2 routing table */
	if (teth_ctx->ch_info[idx].ch_type == TETH_EMBEDDED_CH) {
		snprintf(rt_tbl_name_ipv4, IPA_RESOURCE_NAME_MAX, "%s_%d",
					USB_TO_A2_RT_TBL_NAME_IPV4, lcid);
		snprintf(rt_tbl_name_ipv6, IPA_RESOURCE_NAME_MAX, "%s_%d",
				USB_TO_A2_RT_TBL_NAME_IPV6, lcid);
	} else {
		strlcpy(rt_tbl_name_ipv4,
			USB_TO_A2_RT_TBL_NAME_IPV4,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(rt_tbl_name_ipv6,
			USB_TO_A2_RT_TBL_NAME_IPV6,
			IPA_RESOURCE_NAME_MAX);
	}

	res = configure_ul_header_routing(lcid, rt_tbl_name_ipv4,
				rt_tbl_name_ipv6);

	if (res) {
		TETH_ERR("USB to A2 routing block configuration failed\n");
		goto bail;
	}

	/* Configure A2 -> USB routing table */
	if (teth_ctx->ch_info[idx].ch_type == TETH_EMBEDDED_CH) {
		snprintf(rt_tbl_name_ipv4, IPA_RESOURCE_NAME_MAX, "%s_%d",
				A2_TO_USB_RT_TBL_NAME_IPV4, lcid);
		snprintf(rt_tbl_name_ipv6, IPA_RESOURCE_NAME_MAX, "%s_%d",
				A2_TO_USB_RT_TBL_NAME_IPV6, lcid);
	} else {
		strlcpy(rt_tbl_name_ipv4,
			A2_TO_USB_RT_TBL_NAME_IPV4,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(rt_tbl_name_ipv6,
			A2_TO_USB_RT_TBL_NAME_IPV6,
			IPA_RESOURCE_NAME_MAX);
	}
	res = configure_dl_header_routing(lcid,
			rt_tbl_name_ipv4,
			rt_tbl_name_ipv6);
	if (res) {
		TETH_ERR("A2 to USB routing block configuration failed\n");
		goto bail;
	}
	TETH_DBG_FUNC_EXIT();
bail:
	return res;
}

/**
 * configure_filtering_by_ip() - Configures IPA filtering block for
 * address family: IPv4 or IPv6
 * @rt_tbl_name: routing table name
 * @src: which "clients" pipe does this rule apply to
 * @ip_address_family: address family: IPv4 or IPv6
 * @compare_lcid: whether to use metadata equation to compare the lcid field
 * @lcid: logical channel number
 */
static int configure_filtering_by_ip(char *rt_tbl_name,
			      enum ipa_client_type src,
			      enum ipa_ip_type ip_address_family,
			      enum a2_mux_logical_channel_id lcid,
			      enum teth_packet_direction dir)
{
	struct ipa_ioc_add_flt_rule *flt_tbl;
	struct ipa_ioc_get_rt_tbl rt_tbl_info;
	int res;
	int idx;
	int i;

	TETH_DBG_FUNC_ENTRY();
	i = get_ch_info_idx(lcid);
	TETH_DBG("configure filter: routing table: %s src ep(client type):%d\n",
			rt_tbl_name, src);
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
	if (teth_ctx->ch_info[i].ch_type == TETH_EMBEDDED_CH) {
		if (dir == TETH_A2_TO_USB) {
			flt_tbl->rules[0].rule.attrib.attrib_mask =
					IPA_FLT_META_DATA;
			flt_tbl->rules[0].rule.attrib.meta_data =
					lcid << METADATA_SHFT;
			flt_tbl->rules[0].rule.attrib.meta_data_mask =
					METADATA_MASK;
		} else if (teth_ctx->tethering_mode ==
				TETH_TETHERING_MODE_MBIM) {
			s16 stream_id = find_mbim_stream_id(lcid);
			if (stream_id < 0) {
				TETH_ERR("logical channel %d error\n",
								lcid);
				goto bail;
			}
			flt_tbl->rules[0].rule.attrib.attrib_mask =
					IPA_FLT_META_DATA;
			flt_tbl->rules[0].rule.attrib.meta_data = stream_id;
			flt_tbl->rules[0].rule.attrib.meta_data_mask = 0xFF;
		}
	}


	res = ipa_add_flt_rule(flt_tbl);
	if (res || flt_tbl->rules[0].status)
		TETH_ERR("Failed adding filtering table\n");

	/* Save the filtering rule handle in order to delete it later */
	idx = teth_ctx->ch_info[i].filtering_del[ip_address_family]->num_hdls++;
	teth_ctx->ch_info[i].filtering_del[ip_address_family]->hdl[idx].hdl =
		flt_tbl->rules[0].flt_rule_hdl;

	kfree(flt_tbl);
	TETH_DBG_FUNC_EXIT();

bail:
	return res;
}

/**
 * configure_filtering() - Configures IPA filtering block
 * @rt_tbl_name_ipv4: IPv4 routing table name
 * @rt_tbl_name_ipv6: IPv6 routing table name
 * @src: which "clients" pipe does this rule apply to
 * @compare_lcid: whether to use metadata equation to compare the lcid field
 * @lcid: logical channel number
 */
static int configure_filtering(char *rt_tbl_name_ipv4,
			char *rt_tbl_name_ipv6,
			enum ipa_client_type src,
			enum a2_mux_logical_channel_id lcid,
			enum teth_packet_direction dir)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	res = configure_filtering_by_ip(rt_tbl_name_ipv4,
					src,
					IPA_IP_v4,
					lcid,
					dir);
	if (res) {
		TETH_ERR("Failed adding IPv4 filtering table\n");
		goto bail;
	}

	res = configure_filtering_by_ip(rt_tbl_name_ipv6,
					src,
					IPA_IP_v6,
					lcid,
					dir);
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
 * @lcid: logical channel number
 * This function configures IPA for:
 * - Filter all traffic coming from USB to A2 pointing routing table
 * - Filter all traffic coming from A2 to USB pointing routing table
 */
static int configure_ipa_filtering_block(u16 lcid)
{
	char rt_tbl_name_ipv4[IPA_RESOURCE_NAME_MAX] = {'\0'};
	char rt_tbl_name_ipv6[IPA_RESOURCE_NAME_MAX] = {'\0'};
	enum ipa_client_type src;
	int res;
	int idx;
	u16 prod_client;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);
	if (teth_ctx->ch_info[idx].ch_type == TETH_EMBEDDED_CH) {
		snprintf(rt_tbl_name_ipv4, IPA_RESOURCE_NAME_MAX, "%s_%d",
					USB_TO_A2_RT_TBL_NAME_IPV4, lcid);
		snprintf(rt_tbl_name_ipv6, IPA_RESOURCE_NAME_MAX, "%s_%d",
				USB_TO_A2_RT_TBL_NAME_IPV6, lcid);
	} else {
		strlcpy(rt_tbl_name_ipv4,
			USB_TO_A2_RT_TBL_NAME_IPV4,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(rt_tbl_name_ipv6,
			USB_TO_A2_RT_TBL_NAME_IPV6,
			IPA_RESOURCE_NAME_MAX);
	}

	/* Filter all traffic coming from USB to A2 */
	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
		prod_client = IPA_CLIENT_USB_PROD;
	else
		prod_client = get_prod_client(lcid);
	res = configure_filtering(rt_tbl_name_ipv4, rt_tbl_name_ipv6,
			prod_client, lcid, TETH_USB_TO_A2);
	if (res) {
		TETH_ERR("USB_PROD ep filtering configuration failed\n");
		goto bail;
	}

	/* Filter all traffic coming from A2 to USB */
	if (teth_ctx->ch_info[idx].ch_type == TETH_EMBEDDED_CH) {
		src = IPA_CLIENT_A2_EMBEDDED_PROD;
		snprintf(rt_tbl_name_ipv4, IPA_RESOURCE_NAME_MAX, "%s_%d",
				A2_TO_USB_RT_TBL_NAME_IPV4, lcid);
		snprintf(rt_tbl_name_ipv6, IPA_RESOURCE_NAME_MAX, "%s_%d",
				A2_TO_USB_RT_TBL_NAME_IPV6, lcid);

	} else {
		src = IPA_CLIENT_A2_TETHERED_PROD;
		strlcpy(rt_tbl_name_ipv4,
			A2_TO_USB_RT_TBL_NAME_IPV4,
			IPA_RESOURCE_NAME_MAX);
		strlcpy(rt_tbl_name_ipv6,
			A2_TO_USB_RT_TBL_NAME_IPV6,
			IPA_RESOURCE_NAME_MAX);
	}
	res = configure_filtering(rt_tbl_name_ipv4,
				  rt_tbl_name_ipv6,
				  src,
				  lcid,
				  TETH_A2_TO_USB);
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
 * @param lcid: logical channel number
 * The parameters to this function are passed in the context variable ipa_ctx.
 */
static int teth_set_aggregation(u16 lcid)
{
	int res;
	char aggr_prot_str[20];
	u16 idx;

	TETH_DBG_FUNC_ENTRY();
	idx = get_ch_info_idx(lcid);
	if (!teth_ctx->ch_info[idx].aggr_params_known) {
		TETH_ERR("Aggregation parameters unknown.\n");
		return -EINVAL;
	}

	if ((teth_ctx->ch_info[idx].usb_ipa_pipe_hdl == 0) ||
	    (teth_ctx->ch_info[idx].ipa_usb_pipe_hdl == 0))
		return 0;
		/*
		 * Returning 0 in case pipe handles are 0 becuase aggregation
		 * params will be set later
		 */
	if (get_completed_ch_num() == 0) {
		if (teth_ctx->ch_info[idx].aggr_params.ul.aggr_prot ==
					TETH_AGGR_PROTOCOL_MBIM ||
		    teth_ctx->ch_info[idx].aggr_params.dl.aggr_prot ==
				TETH_AGGR_PROTOCOL_MBIM) {
			res = ipa_set_aggr_mode(IPA_MBIM);
			if (res) {
				TETH_ERR("ipa_set_aggr_mode() failed\n");
				goto bail;
			}
			res = ipa_set_single_ndp_per_mbim(false);
			if (res) {
				TETH_ERR(
					"ipa_set_single_ndp_per_mbim() failed\n");
				goto bail;
			}
		}
	}


	aggr_prot_to_str(teth_ctx->ch_info[idx].aggr_params.ul.aggr_prot,
			 aggr_prot_str,
			 sizeof(aggr_prot_str)-1);
	TETH_DBG("Setting %s aggregation on UL\n", aggr_prot_str);
	aggr_prot_to_str(teth_ctx->ch_info[idx].aggr_params.dl.aggr_prot,
			 aggr_prot_str,
			 sizeof(aggr_prot_str)-1);
	TETH_DBG("Setting %s aggregation on DL\n", aggr_prot_str);

	/* Configure aggregation on UL producer (USB->IPA) */
	res = teth_set_aggr_per_ep(&teth_ctx->ch_info[idx].aggr_params.ul,
				   true,
				   teth_ctx->ch_info[idx].usb_ipa_pipe_hdl);
	if (res) {
		TETH_ERR("teth_set_aggregation_per_ep() failed\n");
		goto bail;
	}

	/* Configure aggregation on DL consumer (IPA->USB) */
	res = teth_set_aggr_per_ep(&teth_ctx->ch_info[idx].aggr_params.dl,
				   false,
				   teth_ctx->ch_info[idx].ipa_usb_pipe_hdl);
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

	mutex_lock(&teth_ctx->request_resource_mutex);
	INIT_COMPLETION(teth_ctx->is_bridge_prod_up);
	res = ipa_rm_inactivity_timer_request_resource(
		IPA_RM_RESOURCE_BRIDGE_PROD);
	if (res < 0) {
		if (res == -EINPROGRESS) {
			wait_for_completion(&teth_ctx->is_bridge_prod_up);
			res = 0;
		}
	} else {
		res = 0;
	}
	mutex_unlock(&teth_ctx->request_resource_mutex);
	return res;
}

/**
 * complete_hw_bridge() - setup the HW bridge from USB to A2 and back through
 * IPA
 */
static void complete_hw_bridge(struct work_struct *work)
{
	int res, i;
	struct hw_bridge_work_wrap *work_data =
				container_of(work,
				struct hw_bridge_work_wrap,
				comp_hw_bridge_work);
	u16 ch_info_idx, lcid;
	int num_of_iterations = 1;

	TETH_DBG_FUNC_ENTRY();
	ch_info_idx = get_ch_info_idx(work_data->lcid);

	TETH_DBG("Completing HW bridge in %s mode. lcid # %d\n",
		 (teth_ctx->ch_info[ch_info_idx].link_protocol ==
				 TETH_LINK_PROTOCOL_ETHERNET) ?
		 "ETHERNET" :
		 "IP", work_data->lcid);

	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		goto bail;
	}


	res = teth_set_aggregation(work_data->lcid);
	if (res) {
		TETH_ERR("Failed setting aggregation params\n");
		goto bail;
	}

	res = configure_ipa_header_block(work_data->lcid);
	if (res) {
		TETH_ERR("Configuration of IPA header block Failed\n");
		goto bail;
	}

	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
		num_of_iterations = 8;
	for (i = 0; i < num_of_iterations; i++) {
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
			lcid = teth_ctx->mbim_stream_id_to_channel_id[i];
		else
			lcid = work_data->lcid;
		res = configure_ipa_routing_block(lcid);
		if (res) {
			TETH_ERR("Configuration of IPA routing block Failed\n");
			goto bail;
		}

		res = configure_ipa_filtering_block(lcid);
		if (res) {
			TETH_ERR("IPA filtering configuration block Failed\n");
			goto bail;
		}
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

	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM) {
		lcid = A2_MUX_MULTI_MBIM_13;
		for (i = 0; i < num_of_iterations; i++, lcid++) {
			ch_info_idx = get_ch_info_idx(lcid);
			teth_ctx->ch_info[ch_info_idx].is_hw_bridge_complete
				= true;
		}
	} else {
		teth_ctx->ch_info[ch_info_idx].is_hw_bridge_complete = true;
	}
bail:
	teth_ctx->ch_info[ch_info_idx].comp_hw_bridge_in_progress = false;
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
 * @param lcid: logical channel id
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
static void check_to_complete_hw_bridge(u16 lcid,
					struct sk_buff *skb,
					u8 *my_mac_addr,
					bool *my_mac_addr_known,
					bool *peer_mac_addr_known)
{
	bool both_mac_addresses_known;
	char mac_addr_str[20];
	u16 idx;
	struct hw_bridge_work_wrap *work_data;

	idx = get_ch_info_idx(lcid);

	if ((teth_ctx->ch_info[idx].link_protocol ==
		TETH_LINK_PROTOCOL_ETHERNET) && (!(*my_mac_addr_known))) {
		memcpy(my_mac_addr, &skb->data[ETH_ALEN], ETH_ALEN);
		mac_addr_to_str(my_mac_addr,
				mac_addr_str,
				sizeof(mac_addr_str)-1);
		TETH_DBG("Extracted MAC addr: %s\n", mac_addr_str);
		*my_mac_addr_known = true;
	}

	both_mac_addresses_known = *my_mac_addr_known && *peer_mac_addr_known;
	if ((both_mac_addresses_known ||
	    (teth_ctx->ch_info[idx].link_protocol ==
					TETH_LINK_PROTOCOL_IP)) &&
	    (!teth_ctx->ch_info[idx].comp_hw_bridge_in_progress) &&
	    (teth_ctx->ch_info[idx].aggr_params_known)) {
		work_data = &teth_ctx->ch_info[idx].hw_bridge_work;
		INIT_WORK(&work_data->comp_hw_bridge_work, complete_hw_bridge);
		work_data->lcid = lcid;
		teth_ctx->ch_info[idx].comp_hw_bridge_in_progress = true;
		queue_work(teth_ctx->teth_wq, &work_data->comp_hw_bridge_work);
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
	u16 client;

	res = teth_request_resource();
	if (res) {
		TETH_ERR("Packet send failure, dropping packet !\n");
		goto bail;
	}

	switch (work_data->dir) {
	case TETH_USB_TO_A2:
		res = a2_mux_write(work_data->lcid, work_data->skb);
		if (res) {
			TETH_ERR("Packet send failure, dropping packet !\n");
			goto bail;
		}
		teth_ctx->stats.usb_to_a2_num_sw_tx_packets++;
		break;

	case TETH_A2_TO_USB:
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
			client = IPA_CLIENT_USB_CONS;
		else
			client = get_cons_client(work_data->lcid);
		res = ipa_tx_dp(client, work_data->skb, &work_data->metadata);
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
static void defer_skb_send(struct sk_buff *skb, enum teth_packet_direction dir,
			enum a2_mux_logical_channel_id lcid)
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
	work->lcid = lcid;
	work->metadata.mbim_stream_id_valid = true;
	work->metadata.mbim_stream_id = find_mbim_stream_id(lcid);
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
	u16 lcid, stream_id = 0;
	u16 idx;

	TETH_DBG("in usb_notify_cb\n");

	switch (evt) {
	case IPA_RECEIVE:
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM) {
			/* extract the stream id from the skb */
			skb_push(skb, TETH_METADATA_LEN);
			stream_id = ntohl(*((u32 *)skb->data));
			TETH_DBG("stream_id %d\n", stream_id);
			skb_pull(skb, TETH_METADATA_LEN);
			lcid = teth_ctx->
					mbim_stream_id_to_channel_id[stream_id];
		} else
			lcid = (u16)(u32)priv;
		TETH_DBG("usb_notify_cb: got lcid=%d from private data\n",
				lcid);
		idx = get_ch_info_idx(lcid);
		if (!teth_ctx->ch_info[idx].is_hw_bridge_complete)
			check_to_complete_hw_bridge(
				lcid,
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
				defer_skb_send(skb, TETH_USB_TO_A2, lcid);
			} else {
				TETH_ERR(
					"Packet send failure, dropping packet !\n");
				dev_kfree_skb(skb);
			}
			ipa_rm_inactivity_timer_release_resource(
				IPA_RM_RESOURCE_BRIDGE_PROD);
			return;
		}
		res = a2_mux_write(lcid, skb);
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
	u16 idx;
	u16 lcid;
	u16 client;
	struct ipa_tx_meta metadata;
	TETH_DBG("in a2_notify_cb: event:%d\n", event);

	switch (event) {
	case A2_MUX_RECEIVE:
		memset(&metadata, 0, sizeof(metadata));
		lcid = (u16)(u32)user_data;
		TETH_DBG("a2_notify_cb: got lcid=%d from private data\n",
			lcid);
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM) {
			s16 stream_id = find_mbim_stream_id(lcid);
			if (stream_id < 0) {
				TETH_ERR("No stream id for lcid %d\n", lcid);
				return;
			}
			client = IPA_CLIENT_USB_CONS;
			metadata.mbim_stream_id_valid = true;
			metadata.mbim_stream_id = stream_id;
		} else {
			client = get_cons_client(lcid);
		}
		idx = get_ch_info_idx(lcid);
		if (!teth_ctx->ch_info[idx].is_hw_bridge_complete)
			check_to_complete_hw_bridge(
				lcid,
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
				defer_skb_send(skb, TETH_A2_TO_USB, lcid);
			} else {
				TETH_ERR(
					"Packet send failure, dropping packet !\n");
				dev_kfree_skb(skb);
			}
			ipa_rm_inactivity_timer_release_resource(
				IPA_RM_RESOURCE_BRIDGE_PROD);
			return;
		}

		res = ipa_tx_dp(client, skb, &metadata);
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
		res = a2_mux_get_client_handles(
					A2_MUX_TETHERED_0,
					&teth_ctx->ipa_a2_pipe_hdl,
					&teth_ctx->a2_ipa_pipe_hdl);
		if (res) {
			TETH_ERR(
				"a2_mux_get_client_handles() failed, res = %d\n",
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
int teth_bridge_init(ipa_notify_cb *usb_notify_cb_ptr, void **private_data_ptr,
		enum ipa_client_type client)
{
	int res = 0;
	u32 lcid;
	int idx;

	TETH_DBG_FUNC_ENTRY();

	if (usb_notify_cb_ptr == NULL || private_data_ptr == NULL) {
		TETH_ERR("Bad parameter\n");
		TETH_DBG_FUNC_EXIT();
		return -EINVAL;
	}

	*usb_notify_cb_ptr = usb_notify_cb;
	lcid = get_channel_id_from_client_prod(client);
	*private_data_ptr = (void *)lcid;
	idx = get_ch_info_idx(lcid);

	mutex_lock(&teth_ctx->init_mutex);
	if (teth_ctx->init_status == TETH_INITIALIZATION_ERROR) {
		res = -EPERM;
		goto bail;
	}

	TETH_DBG("init private data with lcid=%d\n", lcid);

	if (teth_ctx->init_status == TETH_INITIALIZED) {
		teth_ctx->ch_init_cnt++;
		res = 0;
		goto bail;
	}

	TETH_DBG("first call to init: build dependency graph\n");
	/* Build IPA Resource manager dependency graph */
	res = ipa_rm_add_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
					IPA_RM_RESOURCE_USB_CONS);
	if (res && res != -EINPROGRESS) {
		TETH_ERR("ipa_rm_add_dependency() failed\n");
		goto fail;
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

	/* Return 0 as EINPROGRESS is a valid return value at this point */
	teth_ctx->init_status = TETH_INITIALIZED;
	teth_ctx->ch_init_cnt++;
	res = 0;
	goto bail;

fail_add_dependency_3:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_USB_PROD,
				 IPA_RM_RESOURCE_A2_CONS);
fail_add_dependency_2:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				 IPA_RM_RESOURCE_A2_CONS);
fail_add_dependency_1:
	ipa_rm_delete_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
				 IPA_RM_RESOURCE_USB_CONS);
fail:
	teth_ctx->init_status = TETH_INITIALIZATION_ERROR;
bail:
	mutex_unlock(&teth_ctx->init_mutex);
	TETH_DBG_FUNC_EXIT();
	return res;
}
EXPORT_SYMBOL(teth_bridge_init);

static void initialize_ch_info(int idx)
{
	teth_ctx->ch_info[idx].usb_ipa_pipe_hdl = 0;
	teth_ctx->ch_info[idx].ipa_usb_pipe_hdl = 0;
	teth_ctx->ch_info[idx].is_connected = false;
	/*
	 * The first channel(#8) is tethered channel it's and default link
	 * protocol is Ethernet the other channels are embedded channels -
	 * only IP is supported
	 */
	if (idx == 0) {
		teth_ctx->ch_info[idx].ch_type = TETH_TETHERED_CH;
		teth_ctx->ch_info[idx].link_protocol =
				TETH_LINK_PROTOCOL_ETHERNET;
	} else {
		teth_ctx->ch_info[idx].ch_type = TETH_EMBEDDED_CH;
		teth_ctx->ch_info[idx].link_protocol = TETH_LINK_PROTOCOL_IP;
	}

	teth_ctx->ch_info[idx].is_hw_bridge_complete = false;
	memset(&teth_ctx->ch_info[idx].aggr_params, 0,
			sizeof(teth_ctx->ch_info[idx].aggr_params));
	teth_ctx->ch_info[idx].aggr_params_known = false;
	teth_ctx->ch_info[idx].comp_hw_bridge_in_progress = false;
	teth_ctx->ch_info[idx].hw_bridge_work.lcid = A2_MUX_TETHERED_0;

	memset(teth_ctx->ch_info[idx].routing_del[IPA_IP_v4],
	       0,
	       sizeof(struct ipa_ioc_del_rt_rule) +
	       TETH_TOTAL_RT_ENTRIES_IP * sizeof(struct ipa_rt_rule_del));
	teth_ctx->ch_info[idx].routing_del[IPA_IP_v4]->ip = IPA_IP_v4;
	memset(teth_ctx->ch_info[idx].routing_del[IPA_IP_v6],
	       0,
	       sizeof(struct ipa_ioc_del_rt_rule) +
	       TETH_TOTAL_RT_ENTRIES_IP * sizeof(struct ipa_rt_rule_del));
	teth_ctx->ch_info[idx].routing_del[IPA_IP_v6]->ip = IPA_IP_v6;

	memset(teth_ctx->ch_info[idx].filtering_del[IPA_IP_v4],
	       0,
	       sizeof(struct ipa_ioc_del_flt_rule) +
	       TETH_TOTAL_FLT_ENTRIES_IP * sizeof(struct ipa_flt_rule_del));
	teth_ctx->ch_info[idx].filtering_del[IPA_IP_v4]->ip = IPA_IP_v4;
	memset(teth_ctx->ch_info[idx].filtering_del[IPA_IP_v6],
	       0,
	       sizeof(struct ipa_ioc_del_flt_rule) +
	       TETH_TOTAL_FLT_ENTRIES_IP * sizeof(struct ipa_flt_rule_del));
	teth_ctx->ch_info[idx].filtering_del[IPA_IP_v6]->ip = IPA_IP_v6;
}

/**
 * initialize_ch_info_arr() - Initialize the ch_info array
 */
static void initialize_ch_info_arr(void)
{
	int idx;
	TETH_DBG_FUNC_ENTRY();
	/* Initialize channel info array*/
	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++)
		initialize_ch_info(idx);

	TETH_DBG_FUNC_EXIT();
}

/**
 * init_stream_id_to_channel_id_array() -
		Initialize the stream_id to lcid array
 */
void init_stream_id_to_channel_id_array(void)
{
	u16 stream, lcid = A2_MUX_MULTI_MBIM_13;

	for (stream = 0; stream < MAX_MBIM_STREAMS; stream++, lcid++)
		teth_ctx->mbim_stream_id_to_channel_id[stream] = lcid;
}

/**
 * initialize_context() - Initialize the ipa_ctx struct
 */
static void initialize_context(void)
{
	TETH_DBG_FUNC_ENTRY();
	/* Initialize context variables */
	teth_ctx->ipa_a2_pipe_hdl = 0;
	teth_ctx->a2_ipa_pipe_hdl = 0;

	memset(&teth_ctx->mac_addresses, 0, sizeof(teth_ctx->mac_addresses));
	teth_ctx->tethering_mode = 0;
	INIT_COMPLETION(teth_ctx->is_bridge_prod_up);
	INIT_COMPLETION(teth_ctx->is_bridge_prod_down);
	memset(&teth_ctx->stats, 0, sizeof(teth_ctx->stats));
	teth_ctx->a2_ipa_hdr_len = 0;
	teth_ctx->ipa_a2_hdr_len = 0;

	memset(teth_ctx->hdr_del,
	       0,
	       sizeof(struct ipa_ioc_del_hdr) + TETH_TOTAL_HDR_ENTRIES *
	       sizeof(struct ipa_hdr_del));

	teth_ctx->ch_init_cnt = 0;
	teth_ctx->init_status = TETH_NOT_INITIALIZED;
	teth_ctx->debugfs_lcid = A2_MUX_TETHERED_0;

	init_stream_id_to_channel_id_array();

	TETH_DBG_FUNC_EXIT();
}

static int delete_usb_dependencies(void)
{
	int res;
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
	return res;
}

static void teardown_hw_bridge(int idx)
{
	if (get_completed_ch_num() <= 1) {
		/* Delete header entries */
		if (ipa_del_hdr(teth_ctx->hdr_del))
			TETH_ERR("ipa_del_hdr() failed\n");
	}

	/* Delete installed routing rules */
	if (ipa_del_rt_rule(teth_ctx->ch_info[idx].routing_del[IPA_IP_v4]))
		TETH_ERR("ipa_del_rt_rule() failed\n");
	if (ipa_del_rt_rule(teth_ctx->ch_info[idx].routing_del[IPA_IP_v6]))
		TETH_ERR("ipa_del_rt_rule() failed\n");

	/* Delete installed filtering rules */
	if (ipa_del_flt_rule(teth_ctx->ch_info[idx].filtering_del[IPA_IP_v4]))
		TETH_ERR("ipa_del_flt_rule() failed\n");
	if (ipa_del_flt_rule(teth_ctx->ch_info[idx].filtering_del[IPA_IP_v6]))
		TETH_ERR("ipa_del_flt_rule() failed\n");

	/*
	 * Commit all the data to HW, including header, routing and
	 * filtering blocks, IPv4 and IPv6
	 */
	if (ipa_commit_hdr())
		TETH_ERR("Failed committing delete rules\n");
}

/**
* disconnect_first_ch() - any channel disconnect. if last channel disconnects
* delete  bridge prod dependency from the dependency graph
*/
static int disconnect_ch(u16 lcid)
{
	int res;
	struct ipa_rm_register_params a2_prod_reg_params;
	u16 idx = get_ch_info_idx(lcid);

	TETH_DBG_FUNC_ENTRY();
	/* Request the BRIDGE_PROD resource, A2 and IPA should power up */
	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		goto bail;
	}

	/* Close the channel to A2 */
	if (a2_mux_close_channel(lcid))
		TETH_ERR("a2_mux_close_channel(%d) failed\n", lcid);
	/* Tear down the IPA HW bridge */
	if (teth_ctx->ch_info[idx].is_hw_bridge_complete)
		teardown_hw_bridge(idx);

	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);

	/* Deregister from A2_PROD notifications */
	if (teth_ctx->ch_info[idx].ch_type == TETH_TETHERED_CH) {
		a2_prod_reg_params.user_data = NULL;
		a2_prod_reg_params.notify_cb = a2_prod_notify_cb;
		res = ipa_rm_deregister(IPA_RM_RESOURCE_A2_PROD,
				&a2_prod_reg_params);
		if (res)
			TETH_ERR(
				"Failed deregistering from A2_prod notifications.\n");
	}

	if (get_connected_ch_num() <= 1) {
		initialize_context();
		/* Delete the last ipa_rm dependency - BRIDGE_PROD <-> A2 */
		res = ipa_rm_delete_dependency(IPA_RM_RESOURCE_BRIDGE_PROD,
					       IPA_RM_RESOURCE_A2_CONS);
		if ((res != 0) && (res != -EINPROGRESS))
			TETH_ERR(
				"Failed deleting ipa_rm dependency BRIDGE_PROD <-> A2_CONS\n");
	}
	initialize_ch_info(idx);

bail:
	TETH_DBG_FUNC_EXIT();
	return 0;
}

/**
* disconnect_first_ch() - First channel disconnect
*			delete all USB dependencies from the dependency graph
*/
static int disconnect_first_ch(u16 lcid)
{
	int res;

	TETH_DBG_FUNC_ENTRY();
	res = delete_usb_dependencies();

	disconnect_ch(lcid);

	teth_ctx->init_status = TETH_NOT_INITIALIZED;

	TETH_DBG_FUNC_EXIT();
	return 0;
}

/**
* teth_bridge_disconnect() - Disconnect tethering bridge module
*/
int teth_bridge_disconnect(enum ipa_client_type client)
{
	u16 lcid;
	u16 idx;
	u16 num_of_iteration = 1, i;

	TETH_DBG_FUNC_ENTRY();
	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM) {
		num_of_iteration = 8;
		lcid = A2_MUX_MULTI_MBIM_13;
	} else {
		lcid = get_channel_id_from_client_prod(client);
	}

	for (i = 0; i < num_of_iteration; i++, lcid++) {
		idx = get_ch_info_idx(lcid);
		if (!teth_ctx->ch_info[idx].is_connected) {
			TETH_ERR(
				"Trying to disconnect an already disconnected bridge\n");
			goto bail;
		}
		mutex_lock(&teth_ctx->init_mutex);
		if (teth_ctx->init_status == TETH_INITIALIZED)
			disconnect_first_ch(lcid);
		else
			disconnect_ch(lcid);
		teth_ctx->ch_info[idx].is_connected = false;
		mutex_unlock(&teth_ctx->init_mutex);
		TETH_DBG("ch #%d is disconnected.\n", lcid);
	}

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
	int res, num_of_iterations = 1, i;

	struct ipa_ep_cfg ipa_ep_cfg;
	u16 lcid;
	u16 idx;
	struct hw_bridge_work_wrap *work_data;
	enum teth_tethering_mode mode;
	struct ipa_rm_register_params a2_prod_reg_params;

	TETH_DBG_FUNC_ENTRY();

	if (connect_params == NULL ||
	    connect_params->ipa_usb_pipe_hdl <= 0 ||
	    connect_params->usb_ipa_pipe_hdl <= 0 ||
	    connect_params->tethering_mode >= TETH_TETHERING_MODE_MAX ||
	    connect_params->tethering_mode < 0 ||
	    connect_params->client_type > IPA_CLIENT_USB_PROD ||
	    connect_params->client_type < IPA_CLIENT_USB2_PROD) {
		TETH_DBG("Received invalid connect_params.\n");
		return -EINVAL;
	}

	teth_ctx->tethering_mode = connect_params->tethering_mode;
	mode = connect_params->tethering_mode;

	if (mode == TETH_TETHERING_MODE_MBIM) {
		num_of_iterations = MAX_MBIM_STREAMS;
		lcid = teth_ctx->mbim_stream_id_to_channel_id[0];
	} else {
		lcid = get_channel_id_from_client_prod(
				connect_params->client_type);
	}
	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		goto bail;
	}
	for (i = 0; i < num_of_iterations; i++) {
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
			lcid = teth_ctx->mbim_stream_id_to_channel_id[i];
		idx = get_ch_info_idx(lcid);
		if (teth_ctx->ch_info[idx].is_connected) {
			TETH_ERR(
				"Trying to connect an already connected bridge\n");
			return -EPERM;
		}

		teth_ctx->ch_info[idx].ipa_usb_pipe_hdl =
			connect_params->ipa_usb_pipe_hdl;
		teth_ctx->ch_info[idx].usb_ipa_pipe_hdl =
			connect_params->usb_ipa_pipe_hdl;
		teth_ctx->tethering_mode = connect_params->tethering_mode;

		res = a2_mux_open_channel(lcid,
					  (void *)(u32)lcid,
					  a2_notify_cb);
		if (res) {
			TETH_ERR("a2_mux_open_channel(%d) failed\n", lcid);
			goto bail;
		}
	}

	/* Reset the various endpoints configuration */
	memset(&ipa_ep_cfg, 0, sizeof(ipa_ep_cfg));
	ipa_cfg_ep(teth_ctx->ch_info[idx].ipa_usb_pipe_hdl, &ipa_ep_cfg);
	ipa_cfg_ep(teth_ctx->ch_info[idx].usb_ipa_pipe_hdl, &ipa_ep_cfg);

	mutex_lock(&teth_ctx->init_mutex);
	if (get_connected_ch_num() == 0) {
		res = a2_mux_get_client_handles(lcid,
						&teth_ctx->ipa_a2_pipe_hdl,
						&teth_ctx->a2_ipa_pipe_hdl);
		if (res) {
			TETH_ERR(
			"a2_mux_get_client_handles() failed, res = %d\n", res);
			mutex_unlock(&teth_ctx->init_mutex);
			goto bail;
		}
		TETH_DBG("ipa_a2_pipe_hdl=0x%x, a2_ipa_pipe_hdl=0x%x\n",
			teth_ctx->ipa_a2_pipe_hdl,
			teth_ctx->a2_ipa_pipe_hdl);
		if (teth_ctx->ch_info[idx].ch_type == TETH_TETHERED_CH) {
			ipa_cfg_ep(teth_ctx->ipa_a2_pipe_hdl, &ipa_ep_cfg);
			ipa_cfg_ep(teth_ctx->a2_ipa_pipe_hdl, &ipa_ep_cfg);

			/*
			* Register for A2_PROD resource notifications
			* (only for single rmnet)
			* In multi rmnet,
			* the a2_service is responsible for configure the
			* ipa<->a2 pipes
			*/
			a2_prod_reg_params.user_data = NULL;
			a2_prod_reg_params.notify_cb = a2_prod_notify_cb;
			res = ipa_rm_register(IPA_RM_RESOURCE_A2_PROD,
				&a2_prod_reg_params);
			if (res) {
				TETH_ERR("ipa_rm_register() failed\n");
				goto bail;
			}
		}
	}

	for (i = 0; i < num_of_iterations; i++) {
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
			lcid = teth_ctx->mbim_stream_id_to_channel_id[i];
		idx = get_ch_info_idx(lcid);
		teth_ctx->ch_info[idx].is_connected = true;
		TETH_DBG("lcid #%d is connected.\n", lcid);
	}
	mutex_unlock(&teth_ctx->init_mutex);

	for (i = 0; i < num_of_iterations; i++) {
		if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
			lcid = teth_ctx->mbim_stream_id_to_channel_id[i];
		idx = get_ch_info_idx(lcid);
		if (mode == TETH_TETHERING_MODE_MBIM) {
			TETH_DBG(
			"TETH_TETHERING_MODE_MBIM: setting link protocol to IP.\n");
			teth_ctx->ch_info[idx].link_protocol
					= TETH_LINK_PROTOCOL_IP;
		}


		if (teth_ctx->ch_info[idx].aggr_params_known) {
			res = teth_set_aggregation(lcid);
			if (res) {
				TETH_ERR("Failed setting aggregation params\n");
				goto bail;
			}
		}
	}

	/* In case of IP link protocol, complete HW bridge */
	if ((teth_ctx->ch_info[idx].link_protocol ==
			TETH_LINK_PROTOCOL_IP) &&
	    (!teth_ctx->ch_info[idx].comp_hw_bridge_in_progress) &&
	    (teth_ctx->ch_info[idx].aggr_params_known) &&
	    (!teth_ctx->ch_info[idx].is_hw_bridge_complete)) {
		work_data = &teth_ctx->ch_info[idx].hw_bridge_work;
		INIT_WORK(&work_data->comp_hw_bridge_work, complete_hw_bridge);
		work_data->lcid = lcid;
		teth_ctx->ch_info[idx].comp_hw_bridge_in_progress = true;
		queue_work(teth_ctx->teth_wq, &work_data->comp_hw_bridge_work);
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
 * @param lcid: logical channel number
 */
static void teth_set_bridge_mode(u16 lcid,
				 enum teth_link_protocol_type link_protocol)
{
	u16 idx = get_ch_info_idx(lcid);
	teth_ctx->ch_info[idx].link_protocol = link_protocol;
	teth_ctx->ch_info[idx].is_hw_bridge_complete = false;
	memset(&teth_ctx->mac_addresses, 0, sizeof(teth_ctx->mac_addresses));
}



/**
 * teth_bridge_set_aggr_params() - set aggregation parameters
 * @param client: name of the IPA "client"
 * @param aggr_params: aggregation parmeters for uplink and downlink
 *
 * Besides setting the aggregation parameters, the function enforces max
 * transfer size which is less then 8K and also forbids Ethernet link protocol
 * with MBIM aggregation which is not supported by HW.
 */
static int teth_bridge_set_aggr_params(struct teth_aggr_params *aggr_params,
		enum ipa_client_type client)
{
	int res;
	u16 idx;
	u16 lcid, i;
	int num_of_iteration = 1;

	TETH_DBG_FUNC_ENTRY();
	if (!aggr_params) {
		TETH_ERR("Invalid parameter\n");
		return -EINVAL;
	}

	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM) {
		num_of_iteration = 8;
		lcid = A2_MUX_MULTI_MBIM_13;
	} else {
		lcid = get_channel_id_from_client_prod(client);
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
	for (i = 0; i < num_of_iteration; i++, lcid++) {
		idx = get_ch_info_idx(lcid);
		if (teth_ctx->ch_info[idx].link_protocol ==
				TETH_LINK_PROTOCOL_ETHERNET
				&& (aggr_params->dl.aggr_prot ==
						TETH_AGGR_PROTOCOL_MBIM
						|| aggr_params->ul.aggr_prot ==
						TETH_AGGR_PROTOCOL_MBIM)) {
			TETH_ERR("Ethernet with MBIM is not supported.\n");
			return -EINVAL;
		}
	}

	res = teth_request_resource();
	if (res) {
		TETH_ERR("request_resource() failed.\n");
		return res;
	}
	if (teth_ctx->tethering_mode == TETH_TETHERING_MODE_MBIM)
		lcid = A2_MUX_MULTI_MBIM_13;
	else
		lcid = get_channel_id_from_client_prod(client);
	for (i = 0; i < num_of_iteration; ++i, ++lcid) {
		idx = get_ch_info_idx(lcid);
		memcpy(&teth_ctx->ch_info[idx].aggr_params, aggr_params,
		       sizeof(struct teth_aggr_params));
		set_aggr_default_params(&teth_ctx->ch_info[idx].aggr_params.dl);
		set_aggr_default_params(&teth_ctx->ch_info[idx].aggr_params.ul);

		teth_ctx->ch_info[idx].aggr_params_known = true;
		res = teth_set_aggregation(lcid);
		if (res)
			TETH_ERR(
			 "Setting aggregation params fail, lcid %d\n", lcid);
	}
	ipa_rm_inactivity_timer_release_resource(IPA_RM_RESOURCE_BRIDGE_PROD);
	TETH_DBG_FUNC_EXIT();

	return res;
}

/**
 * teth_bridge_set_mbim_aggr_params() - Kernel API to set aggregation parameters
 * for MBIM
 * @param client: name of the IPA "client"
 * @param aggr_params: aggregation parmeters for uplink and downlink
 * Besides setting the aggregation parameters, the function enforces max
 * transfer size which is less then 8K and also forbids Ethernet link protocol
 * with MBIM aggregation which is not supported by HW.
 */
int teth_bridge_set_mbim_aggr_params(struct teth_aggr_params *aggr_params,
	enum ipa_client_type client)
{
	teth_ctx->tethering_mode = TETH_TETHERING_MODE_MBIM;
	return teth_bridge_set_aggr_params(aggr_params, client);
}
EXPORT_SYMBOL(teth_bridge_set_mbim_aggr_params);


static long teth_bridge_ioctl(struct file *filp,
			      unsigned int cmd,
			      unsigned long arg)
{
	int res = 0;
	struct teth_ioc_aggr_params i_aggr_params;
	struct teth_ioc_set_bridge_mode bridge_mode_params;
	struct hw_bridge_work_wrap *work_data;
	u16 i = 0;

	TETH_DBG("cmd=%x nr=%d\n", cmd, _IOC_NR(cmd));

	if ((_IOC_TYPE(cmd) != TETH_BRIDGE_IOC_MAGIC) ||
	    (_IOC_NR(cmd) >= TETH_BRIDGE_IOCTL_MAX)) {
		TETH_ERR("Invalid ioctl\n");
		return -ENOIOCTLCMD;
	}

	switch (cmd) {
	case TETH_BRIDGE_IOC_SET_BRIDGE_MODE:
		TETH_DBG("TETH_BRIDGE_IOC_SET_BRIDGE_MODE ioctl called\n");
		res = copy_from_user(&bridge_mode_params,
				   (struct teth_ioc_set_bridge_mode *)arg,
				   sizeof(struct teth_ioc_set_bridge_mode));
		if (res) {
			TETH_ERR("Error, res = %d\n", res);
			res = -EFAULT;
			break;
		}
		if (bridge_mode_params.lcid >= A2_MUX_NUM_CHANNELS ||
				bridge_mode_params.lcid < A2_MUX_WWAN_0) {
			TETH_ERR("Invalid lcid = %d\n",
					bridge_mode_params.lcid);
			res = -EINVAL;
			break;
		}
		i = get_ch_info_idx(bridge_mode_params.lcid);
		if (teth_ctx->ch_info[i].ch_type == TETH_EMBEDDED_CH &&
		   bridge_mode_params.link_protocol ==
		   TETH_LINK_PROTOCOL_ETHERNET) {
			TETH_ERR(
				"Invalid link protocol. Ethernet is not supported for multi rmnet\n");
			res = -EINVAL;
			break;
		}
		if (teth_ctx->ch_info[i].link_protocol !=
				bridge_mode_params.link_protocol)
			teth_set_bridge_mode(bridge_mode_params.lcid,
					bridge_mode_params.link_protocol);
		break;

	case TETH_BRIDGE_IOC_SET_AGGR_PARAMS:
		TETH_DBG("TETH_BRIDGE_IOC_SET_AGGR_PARAMS ioctl called\n");
		res = copy_from_user(&i_aggr_params,
				   (struct teth_ioc_aggr_params *)arg,
				   sizeof(struct teth_ioc_aggr_params));
		if (res) {
			TETH_ERR("Error, res = %d\n", res);
			res = -EFAULT;
			break;
		}
		if (i_aggr_params.lcid >= A2_MUX_NUM_CHANNELS ||
				i_aggr_params.lcid < A2_MUX_WWAN_0) {
			TETH_ERR("Invalid lcid = %d\n", i_aggr_params.lcid);
			res = -EINVAL;
			break;
		}
		i = get_ch_info_idx(i_aggr_params.lcid);
		res = teth_bridge_set_aggr_params(&i_aggr_params.aggr_params,
				get_prod_client(i_aggr_params.lcid));
		if (res)
			break;

		/* In case of IP link protocol, complete HW bridge */
		if ((teth_ctx->ch_info[i].link_protocol ==
				TETH_LINK_PROTOCOL_IP) &&
		    (!teth_ctx->ch_info[i].comp_hw_bridge_in_progress) &&
		    (!teth_ctx->ch_info[i].is_hw_bridge_complete)) {
			work_data = &teth_ctx->ch_info[i].hw_bridge_work;
			INIT_WORK(&work_data->comp_hw_bridge_work,
				  complete_hw_bridge);
			work_data->lcid = i_aggr_params.lcid;
			teth_ctx->ch_info[i].comp_hw_bridge_in_progress = true;
			queue_work(teth_ctx->teth_wq,
					&work_data->comp_hw_bridge_work);
		}
		break;

	case TETH_BRIDGE_IOC_GET_AGGR_PARAMS:
		TETH_DBG("TETH_BRIDGE_IOC_GET_AGGR_PARAMS ioctl called\n");
		/*get the channel number from the ioctl params*/
		res = copy_from_user(&i_aggr_params,
				   (struct teth_ioc_aggr_params *)arg,
				   sizeof(struct teth_ioc_aggr_params));
		if (res) {
			TETH_ERR("Error, res = %d\n", res);
			res = -EFAULT;
			break;
		}
		TETH_DBG("get_aggr_params ioctl for lcid #%d\n",
				i_aggr_params.lcid);
		if (i_aggr_params.lcid >= A2_MUX_NUM_CHANNELS ||
				i_aggr_params.lcid < A2_MUX_WWAN_0) {
			TETH_ERR("Invalid lcid = %d\n", i_aggr_params.lcid);
			res = -EINVAL;
			break;
		}
		i = get_ch_info_idx(i_aggr_params.lcid);
		i_aggr_params.aggr_params = teth_ctx->ch_info[i].aggr_params;
		if (copy_to_user((u8 *)arg,
				(u8 *)&i_aggr_params,
				sizeof(struct teth_ioc_aggr_params))) {
			res = -EFAULT;
			break;
		}
		TETH_DBG("TETH_BRIDGE_IOC_GET_AGGR_PARAMS ioctl end\n");
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
	*producer_handle = teth_ctx->ch_info[0].usb_ipa_pipe_hdl;
	*consumer_handle = teth_ctx->ch_info[0].ipa_usb_pipe_hdl;
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *dent;
static struct dentry *dfile_lcid;
static struct dentry *dfile_link_protocol;
static struct dentry *dfile_get_aggr_params;
static struct dentry *dfile_set_aggr_protocol;
static struct dentry *dfile_stats;
static struct dentry *dfile_is_hw_bridge_complete;

static ssize_t teth_debugfs_read_lcid(struct file *file,
					       char __user *ubuf,
					       size_t count,
					       loff_t *ppos)
{
	int nbytes;

	nbytes = scnprintf(dbg_buff, TETH_MAX_MSG_LEN, "lcid = %d\n",
			   teth_ctx->debugfs_lcid);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t teth_debugfs_write_lcid(struct file *file,
					const char __user *ubuf,
					size_t count,
					loff_t *ppos)
{
	unsigned long missing;

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	if (count > 0)
		dbg_buff[count-1] = '\0';

	if (strcmp(dbg_buff, "8") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_TETHERED_0;
	} else if (strcmp(dbg_buff, "10") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_RMNET_10;
	} else if (strcmp(dbg_buff, "11") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_RMNET_11;
	} else if (strcmp(dbg_buff, "12") == 0) {
			teth_ctx->debugfs_lcid = A2_MUX_MULTI_RMNET_12;
	} else if (strcmp(dbg_buff, "13") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_13;
	} else if (strcmp(dbg_buff, "14") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_14;
	} else if (strcmp(dbg_buff, "15") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_15;
	} else if (strcmp(dbg_buff, "16") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_16;
	} else if (strcmp(dbg_buff, "17") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_17;
	} else if (strcmp(dbg_buff, "18") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_18;
	} else if (strcmp(dbg_buff, "19") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_19;
	} else if (strcmp(dbg_buff, "20") == 0) {
		teth_ctx->debugfs_lcid = A2_MUX_MULTI_MBIM_20;
	} else {
		teth_ctx->debugfs_lcid = A2_MUX_TETHERED_0;
		TETH_ERR("Bad lcid, got %s,\n"
			 "Use <8, 10-20>.\n", dbg_buff);
	}

	return count;
}

static ssize_t teth_debugfs_read_link_protocol(struct file *file,
					       char __user *ubuf,
					       size_t count,
					       loff_t *ppos)
{
	int nbytes;
	u16 ch_info_idx;
	ch_info_idx = get_ch_info_idx(teth_ctx->debugfs_lcid);

	nbytes = scnprintf(dbg_buff, TETH_MAX_MSG_LEN, "Link protocol = %s\n",
			   (teth_ctx->ch_info[ch_info_idx].link_protocol ==
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

	teth_set_bridge_mode(teth_ctx->debugfs_lcid , link_protocol);

	return count;
}

static ssize_t teth_debugfs_read_aggr_params(struct file *file,
					     char __user *ubuf,
					     size_t count,
					     loff_t *ppos)
{
	int nbytes = 0;
	char aggr_str[20];
	u16 idx;
	idx = get_ch_info_idx(teth_ctx->debugfs_lcid);

	aggr_prot_to_str(teth_ctx->ch_info[idx].aggr_params.ul.aggr_prot,
			 aggr_str,
			 sizeof(aggr_str)-1);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			   "Aggregation parameters for uplink:\n");
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Aggregation protocol: %s\n",
			    aggr_str);
	nbytes += scnprintf(
		&dbg_buff[nbytes],
		TETH_MAX_MSG_LEN - nbytes,
		"  Max transfer size [byte]: %d\n",
		teth_ctx->ch_info[idx].aggr_params.ul.max_transfer_size_byte);
	nbytes += scnprintf(
			&dbg_buff[nbytes],
			TETH_MAX_MSG_LEN - nbytes,
			"  Max datagrams: %d\n",
			teth_ctx->ch_info[idx].aggr_params.ul.max_datagrams);

	aggr_prot_to_str(teth_ctx->ch_info[idx].aggr_params.dl.aggr_prot,
			 aggr_str,
			 sizeof(aggr_str)-1);
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN,
			   "Aggregation parameters for downlink:\n");
	nbytes += scnprintf(&dbg_buff[nbytes], TETH_MAX_MSG_LEN - nbytes,
			    "  Aggregation protocol: %s\n",
			    aggr_str);
	nbytes += scnprintf(
		&dbg_buff[nbytes],
		TETH_MAX_MSG_LEN - nbytes,
		"  Max transfer size [byte]: %d\n",
		teth_ctx->ch_info[idx].aggr_params.dl.max_transfer_size_byte);
	nbytes += scnprintf(
			&dbg_buff[nbytes],
			TETH_MAX_MSG_LEN - nbytes,
			"  Max datagrams: %d\n",
			teth_ctx->ch_info[idx].aggr_params.dl.max_datagrams);

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

static ssize_t teth_debugfs_set_aggr_protocol(struct file *file,
				      const char __user *ubuf,
				      size_t count, loff_t *ppos)
{
	unsigned long missing;
	enum teth_aggr_protocol_type aggr_prot;
	int res;
	u16 idx;
	idx = get_ch_info_idx(teth_ctx->debugfs_lcid);

	if (sizeof(dbg_buff) < count + 1)
		return -EFAULT;

	missing = copy_from_user(dbg_buff, ubuf, count);
	if (missing)
		return -EFAULT;

	if (count > 0)
		dbg_buff[count-1] = '\0';

	set_aggr_default_params(&teth_ctx->ch_info[idx].aggr_params.dl);
	set_aggr_default_params(&teth_ctx->ch_info[idx].aggr_params.ul);

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

	teth_ctx->ch_info[idx].aggr_params.dl.aggr_prot = aggr_prot;
	teth_ctx->ch_info[idx].aggr_params.ul.aggr_prot = aggr_prot;
	teth_ctx->ch_info[idx].aggr_params_known = true;

	res = teth_set_aggregation(teth_ctx->debugfs_lcid);
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
	u16 ch_info_idx;
	ch_info_idx = get_ch_info_idx(teth_ctx->debugfs_lcid);

	if (teth_ctx->ch_info[ch_info_idx].is_hw_bridge_complete)
		nbytes += scnprintf(&dbg_buff[nbytes],
				    TETH_MAX_MSG_LEN - nbytes,
				   "HW bridge is in use.\n");
	else
		nbytes += scnprintf(&dbg_buff[nbytes],
				    TETH_MAX_MSG_LEN - nbytes,
				   "SW bridge is in use. HW bridge not complete yet.\n");

	return simple_read_from_buffer(ubuf, count, ppos, dbg_buff, nbytes);
}

const struct file_operations teth_lcid_ops = {
	.read = teth_debugfs_read_lcid,
	.write = teth_debugfs_write_lcid,
};

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
	dfile_lcid = debugfs_create_file("lcid", read_write_mode, dent, 0,
					&teth_lcid_ops);
	if (!dfile_lcid || IS_ERR(dfile_lcid)) {
		IPAERR("fail to create file lcid\n");
		goto fail;
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

static int alloc_del_hnds(void)
{
	int idx;

	teth_ctx->hdr_del = kzalloc(sizeof(struct ipa_ioc_del_hdr) +
					TETH_TOTAL_HDR_ENTRIES *
					sizeof(struct ipa_hdr_del),
					GFP_KERNEL);
	if (!teth_ctx->hdr_del) {
		TETH_ERR("kzalloc err for hdr_del.\n");
		return -ENOMEM;
	}

	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++) {
		teth_ctx->ch_info[idx].routing_del[IPA_IP_v4] =
			kzalloc(sizeof(struct ipa_ioc_del_rt_rule) +
				TETH_TOTAL_RT_ENTRIES_IP *
				sizeof(struct ipa_rt_rule_del),
				GFP_KERNEL);
		if (!teth_ctx->ch_info[idx].routing_del[IPA_IP_v4]) {
			TETH_ERR("kzalloc err for routing_del[IPA_IP_v4].\n");
			goto fail_alloc_routing_del_ipv4;
		}
	}

	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++) {
		teth_ctx->ch_info[idx].routing_del[IPA_IP_v6] =
			kzalloc(sizeof(struct ipa_ioc_del_rt_rule) +
				TETH_TOTAL_RT_ENTRIES_IP *
				sizeof(struct ipa_rt_rule_del),
				GFP_KERNEL);
		if (!teth_ctx->ch_info[idx].routing_del[IPA_IP_v6]) {
			TETH_ERR("kzalloc err for routing_del[IPA_IP_v6].\n");
			goto fail_alloc_routing_del_ipv6;
		}
	}

	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++) {
		teth_ctx->ch_info[idx].filtering_del[IPA_IP_v4] =
			kzalloc(sizeof(struct ipa_ioc_del_flt_rule) +
				TETH_TOTAL_FLT_ENTRIES_IP *
				sizeof(struct ipa_flt_rule_del),
				GFP_KERNEL);
		if (!teth_ctx->ch_info[idx].filtering_del[IPA_IP_v4]) {
			TETH_ERR("kzalloc err.\n");
			goto fail_alloc_filtering_del_ipv4;
		}
	}

	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++) {
		teth_ctx->ch_info[idx].filtering_del[IPA_IP_v6] =
			kzalloc(sizeof(struct ipa_ioc_del_flt_rule) +
				TETH_TOTAL_FLT_ENTRIES_IP *
				sizeof(struct ipa_flt_rule_del),
				GFP_KERNEL);
		if (!teth_ctx->ch_info[idx].filtering_del[IPA_IP_v6]) {
			TETH_ERR("kzalloc err.\n");
			goto fail_alloc_filtering_del_ipv6;
		}
	}

	return 0;

fail_alloc_filtering_del_ipv6:
	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++)
		kfree(teth_ctx->ch_info[idx].filtering_del[IPA_IP_v6]);

fail_alloc_filtering_del_ipv4:
	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++)
		kfree(teth_ctx->ch_info[idx].filtering_del[IPA_IP_v4]);

fail_alloc_routing_del_ipv6:
	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++)
		kfree(teth_ctx->ch_info[idx].routing_del[IPA_IP_v6]);

fail_alloc_routing_del_ipv4:
	for (idx = 0; idx < TETH_NUM_CHANNELS; idx++)
		kfree(teth_ctx->ch_info[idx].routing_del[IPA_IP_v4]);

	kfree(teth_ctx->hdr_del);

	return -ENOMEM;
}

/**
* teth_bridge_driver_init() - Initialize tethering bridge driver
*
*/
int teth_bridge_driver_init(void)
{
	int res;
	struct ipa_rm_create_params bridge_prod_params;
	res = -ENOMEM;

	TETH_DBG("Tethering bridge driver init\n");
	teth_ctx = kzalloc(sizeof(*teth_ctx), GFP_KERNEL);
	if (!teth_ctx) {
		TETH_ERR("kzalloc err.\n");
		return -ENOMEM;
	}

	teth_ctx->ch_info =
		kzalloc(sizeof(struct logic_ch_info)*TETH_NUM_CHANNELS,
		   GFP_KERNEL);
	if (!teth_ctx->ch_info) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_channel_info;
	}

	res = set_aggr_capabilities();
	if (res) {
		TETH_ERR("kzalloc err.\n");
		goto fail_alloc_aggr_caps;
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

	res = alloc_del_hnds();
	if (res) {
		TETH_ERR("kzalloc err.\n");
		goto fail_cdev_add;
	}

	initialize_context();
	initialize_ch_info_arr();
	mutex_init(&teth_ctx->request_resource_mutex);
	mutex_init(&teth_ctx->init_mutex);
	TETH_DBG("Tethering bridge driver init OK\n");

	return 0;
fail_cdev_add:
	device_destroy(teth_ctx->class, teth_ctx->dev_num);
fail_device_create:
	unregister_chrdev_region(teth_ctx->dev_num, 1);
fail_alloc_chrdev_region:
	kfree(teth_ctx->aggr_caps);
fail_alloc_aggr_caps:
	kfree(teth_ctx->ch_info);
fail_alloc_channel_info:
	kfree(teth_ctx);
	teth_ctx = NULL;

	return res;
}
EXPORT_SYMBOL(teth_bridge_driver_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Tethering bridge driver");
