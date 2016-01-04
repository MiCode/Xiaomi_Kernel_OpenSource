/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_H_
#define _IPA_H_

#include <linux/msm_ipa.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/msm-sps.h>
#include <linux/if_ether.h>
#include "linux/msm_gsi.h"

#define IPA_APPS_MAX_BW_IN_MBPS 700
/**
 * enum ipa_transport_type
 * transport type: either GSI or SPS
 */
enum ipa_transport_type {
	IPA_TRANSPORT_TYPE_SPS,
	IPA_TRANSPORT_TYPE_GSI
};

/**
 * enum ipa_nat_en_type - NAT setting type in IPA end-point
 */
enum ipa_nat_en_type {
	IPA_BYPASS_NAT,
	IPA_SRC_NAT,
	IPA_DST_NAT,
};

/**
 * enum ipa_mode_type - mode setting type in IPA end-point
 * @BASIC: basic mode
 * @ENABLE_FRAMING_HDLC: not currently supported
 * @ENABLE_DEFRAMING_HDLC: not currently supported
 * @DMA: all data arriving IPA will not go through IPA logic blocks, this
 *  allows IPA to work as DMA for specific pipes.
 */
enum ipa_mode_type {
	IPA_BASIC,
	IPA_ENABLE_FRAMING_HDLC,
	IPA_ENABLE_DEFRAMING_HDLC,
	IPA_DMA,
};

/**
 *  enum ipa_aggr_en_type - aggregation setting type in IPA
 *  end-point
 */
enum ipa_aggr_en_type {
	IPA_BYPASS_AGGR,
	IPA_ENABLE_AGGR,
	IPA_ENABLE_DEAGGR,
};

/**
 *  enum ipa_aggr_type - type of aggregation in IPA end-point
 */
enum ipa_aggr_type {
	IPA_MBIM_16 = 0,
	IPA_HDLC    = 1,
	IPA_TLP     = 2,
	IPA_RNDIS   = 3,
	IPA_GENERIC = 4,
	IPA_QCMAP   = 6,
};

/**
 * enum ipa_aggr_mode - global aggregation mode
 */
enum ipa_aggr_mode {
	IPA_MBIM,
	IPA_QCNCM,
};

/**
 * enum ipa_dp_evt_type - type of event client callback is
 * invoked for on data path
 * @IPA_RECEIVE: data is struct sk_buff
 * @IPA_WRITE_DONE: data is struct sk_buff
 */
enum ipa_dp_evt_type {
	IPA_RECEIVE,
	IPA_WRITE_DONE,
};

/**
 * enum hdr_total_len_or_pad_type - type vof alue held by TOTAL_LEN_OR_PAD
 * field in header configuration register.
 * @IPA_HDR_PAD: field is used as padding length
 * @IPA_HDR_TOTAL_LEN: field is used as total length
 */
enum hdr_total_len_or_pad_type {
	IPA_HDR_PAD = 0,
	IPA_HDR_TOTAL_LEN = 1,
};

/**
 * struct ipa_ep_cfg_nat - NAT configuration in IPA end-point
 * @nat_en:	This defines the default NAT mode for the pipe: in case of
 *		filter miss - the default NAT mode defines the NATing operation
 *		on the packet. Valid for Input Pipes only (IPA consumer)
 */
struct ipa_ep_cfg_nat {
	enum ipa_nat_en_type nat_en;
};

/**
 * struct ipa_ep_cfg_hdr - header configuration in IPA end-point
 *
 * @hdr_len:Header length in bytes to be added/removed. Assuming
 *			header len is constant per endpoint. Valid for
 *			both Input and Output Pipes
 * @hdr_ofst_metadata_valid:	0: Metadata_Ofst  value is invalid, i.e., no
 *			metadata within header.
 *			1: Metadata_Ofst  value is valid, i.e., metadata
 *			within header is in offset Metadata_Ofst Valid
 *			for Input Pipes only (IPA Consumer) (for output
 *			pipes, metadata already set within the header)
 * @hdr_ofst_metadata:	Offset within header in which metadata resides
 *			Size of metadata - 4bytes
 *			Example -  Stream ID/SSID/mux ID.
 *			Valid for  Input Pipes only (IPA Consumer) (for output
 *			pipes, metadata already set within the header)
 * @hdr_additional_const_len:	Defines the constant length that should be added
 *			to the payload length in order for IPA to update
 *			correctly the length field within the header
 *			(valid only in case Hdr_Ofst_Pkt_Size_Valid=1)
 *			Valid for Output Pipes (IPA Producer)
 * @hdr_ofst_pkt_size_valid:	0: Hdr_Ofst_Pkt_Size  value is invalid, i.e., no
 *			length field within the inserted header
 *			1: Hdr_Ofst_Pkt_Size  value is valid, i.e., a
 *			packet length field resides within the header
 *			Valid for Output Pipes (IPA Producer)
 * @hdr_ofst_pkt_size:	Offset within header in which packet size reside. Upon
 *			Header Insertion, IPA will update this field within the
 *			header with the packet length . Assumption is that
 *			header length field size is constant and is 2Bytes
 *			Valid for Output Pipes (IPA Producer)
 * @hdr_a5_mux:	Determines whether A5 Mux header should be added to the packet.
 *			This bit is valid only when Hdr_En=01(Header Insertion)
 *			SW should set this bit for IPA-to-A5 pipes.
 *			0: Do not insert A5 Mux Header
 *			1: Insert A5 Mux Header
 *			Valid for Output Pipes (IPA Producer)
 * @hdr_remove_additional:	bool switch, remove more of the header
 *			based on the aggregation configuration (register
 *			HDR_LEN_INC_DEAGG_HDR)
 * @hdr_metadata_reg_valid:	bool switch, metadata from
 *			register INIT_HDR_METADATA_n is valid.
 *			(relevant only for IPA Consumer pipes)
 */
struct ipa_ep_cfg_hdr {
	u32  hdr_len;
	u32  hdr_ofst_metadata_valid;
	u32  hdr_ofst_metadata;
	u32  hdr_additional_const_len;
	u32  hdr_ofst_pkt_size_valid;
	u32  hdr_ofst_pkt_size;
	u32  hdr_a5_mux;
	u32  hdr_remove_additional;
	u32  hdr_metadata_reg_valid;
};

/**
 * struct ipa_ep_cfg_hdr_ext - extended header configuration in IPA end-point
 * @hdr_pad_to_alignment: Pad packet to specified alignment
 *	(2^pad to alignment value), i.e. value of 3 means pad to 2^3 = 8 bytes
 *	alignment. Alignment is to 0,2 up to 32 bytes (IPAv2 does not support 64
 *	byte alignment). Valid for Output Pipes only (IPA Producer).
 * @hdr_total_len_or_pad_offset: Offset to length field containing either
 *	total length or pad length, per hdr_total_len_or_pad config
 * @hdr_payload_len_inc_padding: 0-IPA_ENDP_INIT_HDR_n's
 *	HDR_OFST_PKT_SIZE does
 *	not includes padding bytes size, payload_len = packet length,
 *	1-IPA_ENDP_INIT_HDR_n's HDR_OFST_PKT_SIZE includes
 *	padding bytes size, payload_len = packet length + padding
 * @hdr_total_len_or_pad: field is used as PAD length ot as Total length
 *	(header + packet + padding)
 * @hdr_total_len_or_pad_valid: 0-Ignore TOTAL_LEN_OR_PAD field, 1-Process
 *	TOTAL_LEN_OR_PAD field
 * @hdr_little_endian: 0-Big Endian, 1-Little Endian
 */
struct ipa_ep_cfg_hdr_ext {
	u32 hdr_pad_to_alignment;
	u32 hdr_total_len_or_pad_offset;
	bool hdr_payload_len_inc_padding;
	enum hdr_total_len_or_pad_type hdr_total_len_or_pad;
	bool hdr_total_len_or_pad_valid;
	bool hdr_little_endian;
};

/**
 * struct ipa_ep_cfg_mode - mode configuration in IPA end-point
 * @mode:	Valid for Input Pipes only (IPA Consumer)
 * @dst:	This parameter specifies the output pipe to which the packets
 *		will be routed to.
 *		This parameter is valid for Mode=DMA and not valid for
 *		Mode=Basic
 *		Valid for Input Pipes only (IPA Consumer)
 */
struct ipa_ep_cfg_mode {
	enum ipa_mode_type mode;
	enum ipa_client_type dst;
};

/**
 * struct ipa_ep_cfg_aggr - aggregation configuration in IPA end-point
 *
 * @aggr_en:	Valid for both Input and Output Pipes
 * @aggr:	aggregation type (Valid for both Input and Output Pipes)
 * @aggr_byte_limit:	Limit of aggregated packet size in KB (<=32KB) When set
 *			to 0, there is no size limitation on the aggregation.
 *			When both, Aggr_Byte_Limit and Aggr_Time_Limit are set
 *			to 0, there is no aggregation, every packet is sent
 *			independently according to the aggregation structure
 *			Valid for Output Pipes only (IPA Producer )
 * @aggr_time_limit:	Timer to close aggregated packet (<=32ms) When set to 0,
 *			there is no time limitation on the aggregation.  When
 *			both, Aggr_Byte_Limit and Aggr_Time_Limit are set to 0,
 *			there is no aggregation, every packet is sent
 *			independently according to the aggregation structure
 *			Valid for Output Pipes only (IPA Producer)
 * @aggr_pkt_limit: Defines if EOF close aggregation or not. if set to false
 *			HW closes aggregation (sends EOT) only based on its
 *			aggregation config (byte/time limit, etc). if set to
 *			true EOF closes aggregation in addition to HW based
 *			aggregation closure. Valid for Output Pipes only (IPA
 *			Producer). EOF affects only Pipes configured for
 *			generic aggregation.
 */
struct ipa_ep_cfg_aggr {
	enum ipa_aggr_en_type aggr_en;
	enum ipa_aggr_type aggr;
	u32 aggr_byte_limit;
	u32 aggr_time_limit;
	u32 aggr_pkt_limit;
};

/**
 * struct ipa_ep_cfg_route - route configuration in IPA end-point
 * @rt_tbl_hdl:	Defines the default routing table index to be used in case there
 *		is no filter rule matching, valid for Input Pipes only (IPA
 *		Consumer). Clients should set this to 0 which will cause default
 *		v4 and v6 routes setup internally by IPA driver to be used for
 *		this end-point
 */
struct ipa_ep_cfg_route {
	u32 rt_tbl_hdl;
};

/**
 * struct ipa_ep_cfg_holb - head of line blocking configuration in IPA end-point
 * @en: enable(1 => ok to drop pkt)/disable(0 => never drop pkt)
 * @tmr_val: duration in units of 128 IPA clk clock cyles [0,511], 1 clk=1.28us
 *	     IPAv2.5 support 32 bit HOLB timeout value, previous versions
 *	     supports 16 bit
 */
struct ipa_ep_cfg_holb {
	u16 en;
	u32 tmr_val;
};

/**
 * struct ipa_ep_cfg_deaggr - deaggregation configuration in IPA end-point
 * @deaggr_hdr_len: Deaggregation Header length in bytes. Valid only for Input
 *	Pipes, which are configured for 'Generic' deaggregation.
 * @packet_offset_valid: - 0: PACKET_OFFSET is not used, 1: PACKET_OFFSET is
 *	used.
 * @packet_offset_location: Location of packet offset field, which specifies
 *	the offset to the packet from the start of the packet offset field.
 * @max_packet_len: DEAGGR Max Packet Length in Bytes. A Packet with higher
 *	size wil be treated as an error. 0 - Packet Length is not Bound,
 *	IPA should not check for a Max Packet Length.
 */
struct ipa_ep_cfg_deaggr {
	u32 deaggr_hdr_len;
	bool packet_offset_valid;
	u32 packet_offset_location;
	u32 max_packet_len;
};

/**
 * enum ipa_cs_offload - checksum offload setting
 */
enum ipa_cs_offload {
	IPA_DISABLE_CS_OFFLOAD,
	IPA_ENABLE_CS_OFFLOAD_UL,
	IPA_ENABLE_CS_OFFLOAD_DL,
	IPA_CS_RSVD
};

/**
 * struct ipa_ep_cfg_cfg - IPA ENDP_INIT Configuration register
 * @frag_offload_en: - 0 - IP packet fragment handling is disabled. IP packet
 *	fragments should be sent to SW. SW is responsible for
 *	configuring filter rules, and IP packet filter exception should be
 *	used to send all fragments to SW. 1 - IP packet fragment
 *	handling is enabled. IPA checks for fragments and uses frag
 *	rules table for processing fragments. Valid only for Input Pipes
 *	(IPA Consumer)
 * @cs_offload_en: Checksum offload enable: 00: Disable checksum offload, 01:
 *	Enable checksum calculation offload (UL) - For output pipe
 *	(IPA producer) specifies that checksum trailer is to be added.
 *	For input pipe (IPA consumer) specifies presence of checksum
 *	header and IPA checksum calculation accordingly. 10: Enable
 *	checksum calculation offload (DL) - For output pipe (IPA
 *	producer) specifies that checksum trailer is to be added. For
 *	input pipe (IPA consumer) specifies IPA checksum calculation.
 *	11: Reserved
 * @cs_metadata_hdr_offset: Offset in Words (4 bytes) within header in which
 *	checksum meta info header (4 bytes) starts (UL). Values are 0-15, which
 *	mean 0 - 60 byte checksum header offset. Valid for input
 *	pipes only (IPA consumer)
 */
struct ipa_ep_cfg_cfg {
	bool frag_offload_en;
	enum ipa_cs_offload cs_offload_en;
	u8 cs_metadata_hdr_offset;
};

/**
 * struct ipa_ep_cfg_metadata_mask - Endpoint initialization hdr metadata mask
 * @metadata_mask: Mask specifying which metadata bits to write to
 *	IPA_ENDP_INIT_HDR_n.s HDR_OFST_METADATA. Only
 *	masked metadata bits (set to 1) will be written. Valid for Output
 *	Pipes only (IPA Producer)
 */
struct ipa_ep_cfg_metadata_mask {
	u32 metadata_mask;
};

/**
 * struct ipa_ep_cfg_metadata - Meta Data configuration in IPA end-point
 * @md:	This defines the meta data from tx data descriptor
 * @qmap_id: qmap id
 */
struct ipa_ep_cfg_metadata {
	u32 qmap_id;
};

/**
 * struct ipa_ep_cfg - configuration of IPA end-point
 * @nat:		NAT parmeters
 * @hdr:		Header parameters
 * @hdr_ext:		Extended header parameters
 * @mode:		Mode parameters
 * @aggr:		Aggregation parameters
 * @deaggr:		Deaggregation params
 * @route:		Routing parameters
 * @cfg:		Configuration register data
 * @metadata_mask:	Hdr metadata mask
 * @meta:		Meta Data
 */
struct ipa_ep_cfg {
	struct ipa_ep_cfg_nat nat;
	struct ipa_ep_cfg_hdr hdr;
	struct ipa_ep_cfg_hdr_ext hdr_ext;
	struct ipa_ep_cfg_mode mode;
	struct ipa_ep_cfg_aggr aggr;
	struct ipa_ep_cfg_deaggr deaggr;
	struct ipa_ep_cfg_route route;
	struct ipa_ep_cfg_cfg cfg;
	struct ipa_ep_cfg_metadata_mask metadata_mask;
	struct ipa_ep_cfg_metadata meta;
};

/**
 * struct ipa_ep_cfg_ctrl - Control configuration in IPA end-point
 * @ipa_ep_suspend: 0 - ENDP is enabled, 1 - ENDP is suspended (disabled).
 *			Valid for PROD Endpoints
 * @ipa_ep_delay:   0 - ENDP is free-running, 1 - ENDP is delayed.
 *			SW controls the data flow of an endpoint usind this bit.
 *			Valid for CONS Endpoints
 */
struct ipa_ep_cfg_ctrl {
	bool ipa_ep_suspend;
	bool ipa_ep_delay;
};

/**
 * x should be in bytes
 */
#define IPA_NUM_OF_FIFO_DESC(x) (x/sizeof(struct sps_iovec))
typedef void (*ipa_notify_cb)(void *priv, enum ipa_dp_evt_type evt,
		       unsigned long data);

/**
 * struct ipa_connect_params - low-level client connect input parameters. Either
 * client allocates the data and desc FIFO and specifies that in data+desc OR
 * specifies sizes and pipe_mem pref and IPA does the allocation.
 *
 * @ipa_ep_cfg:	IPA EP configuration
 * @client:	type of "client"
 * @client_bam_hdl:	 client SPS handle
 * @client_ep_idx:	 client PER EP index
 * @priv:	callback cookie
 * @notify:	callback
 *		priv - callback cookie evt - type of event data - data relevant
 *		to event.  May not be valid. See event_type enum for valid
 *		cases.
 * @desc_fifo_sz:	size of desc FIFO
 * @data_fifo_sz:	size of data FIFO
 * @pipe_mem_preferred:	if true, try to alloc the FIFOs in pipe mem, fallback
 *			to sys mem if pipe mem alloc fails
 * @desc:	desc FIFO meta-data when client has allocated it
 * @data:	data FIFO meta-data when client has allocated it
 * @skip_ep_cfg: boolean field that determines if EP should be configured
 *  by IPA driver
 * @keep_ipa_awake: when true, IPA will not be clock gated
 */
struct ipa_connect_params {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	unsigned long client_bam_hdl;
	u32 client_ep_idx;
	void *priv;
	ipa_notify_cb notify;
	u32 desc_fifo_sz;
	u32 data_fifo_sz;
	bool pipe_mem_preferred;
	struct sps_mem_buffer desc;
	struct sps_mem_buffer data;
	bool skip_ep_cfg;
	bool keep_ipa_awake;
};

/**
 *  struct ipa_sps_params - SPS related output parameters resulting from
 *  low/high level client connect
 *  @ipa_bam_hdl:	IPA SPS handle
 *  @ipa_ep_idx:	IPA PER EP index
 *  @desc:	desc FIFO meta-data
 *  @data:	data FIFO meta-data
 */
struct ipa_sps_params {
	unsigned long ipa_bam_hdl;
	u32 ipa_ep_idx;
	struct sps_mem_buffer desc;
	struct sps_mem_buffer data;
};

/**
 * struct ipa_tx_intf - interface tx properties
 * @num_props:	number of tx properties
 * @prop:	the tx properties array
 */
struct ipa_tx_intf {
	u32 num_props;
	struct ipa_ioc_tx_intf_prop *prop;
};

/**
 * struct ipa_rx_intf - interface rx properties
 * @num_props:	number of rx properties
 * @prop:	the rx properties array
 */
struct ipa_rx_intf {
	u32 num_props;
	struct ipa_ioc_rx_intf_prop *prop;
};

/**
 * struct ipa_ext_intf - interface ext properties
 * @excp_pipe_valid:	is next field valid?
 * @excp_pipe:	exception packets should be routed to this pipe
 * @num_props:	number of ext properties
 * @prop:	the ext properties array
 */
struct ipa_ext_intf {
	bool excp_pipe_valid;
	enum ipa_client_type excp_pipe;
	u32 num_props;
	struct ipa_ioc_ext_intf_prop *prop;
};

/**
 * struct ipa_sys_connect_params - information needed to setup an IPA end-point
 * in system-BAM mode
 * @ipa_ep_cfg:	IPA EP configuration
 * @client:	the type of client who "owns" the EP
 * @desc_fifo_sz:	size of desc FIFO
 * @priv:	callback cookie
 * @notify:	callback
 *		priv - callback cookie
 *		evt - type of event
 *		data - data relevant to event.  May not be valid. See event_type
 *		enum for valid cases.
 * @skip_ep_cfg: boolean field that determines if EP should be configured
 *  by IPA driver
 * @keep_ipa_awake: when true, IPA will not be clock gated
 */
struct ipa_sys_connect_params {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	u32 desc_fifo_sz;
	void *priv;
	ipa_notify_cb notify;
	bool skip_ep_cfg;
	bool keep_ipa_awake;
};

/**
 * struct ipa_tx_meta - meta-data for the TX packet
 * @dma_address: dma mapped address of TX packet
 * @dma_address_valid: is above field valid?
 */
struct ipa_tx_meta {
	u8 pkt_init_dst_ep;
	bool pkt_init_dst_ep_valid;
	bool pkt_init_dst_ep_remote;
	dma_addr_t dma_address;
	bool dma_address_valid;
};

/**
 * typedef ipa_msg_free_fn - callback function
 * @param buff - [in] the message payload to free
 * @param len - [in] size of message payload
 * @param type - [in] the message type
 *
 * Message callback registered by kernel client with IPA driver to
 * free message payload after IPA driver processing is complete
 *
 * No return value
 */
typedef void (*ipa_msg_free_fn)(void *buff, u32 len, u32 type);

/**
 * typedef ipa_msg_pull_fn - callback function
 * @param buff - [in] where to copy message payload
 * @param len - [in] size of buffer to copy payload into
 * @param type - [in] the message type
 *
 * Message callback registered by kernel client with IPA driver for
 * IPA driver to pull messages from the kernel client upon demand from
 * user-space
 *
 * Returns how many bytes were copied into the buffer.
 */
typedef int (*ipa_msg_pull_fn)(void *buff, u32 len, u32 type);

/**
 * enum ipa_voltage_level - IPA Voltage levels
 */
enum ipa_voltage_level {
	IPA_VOLTAGE_UNSPECIFIED,
	IPA_VOLTAGE_SVS = IPA_VOLTAGE_UNSPECIFIED,
	IPA_VOLTAGE_NOMINAL,
	IPA_VOLTAGE_TURBO,
	IPA_VOLTAGE_MAX,
};

/**
 * enum ipa_rm_event - IPA RM events
 *
 * Indicate the resource state change
 */
enum ipa_rm_event {
	IPA_RM_RESOURCE_GRANTED,
	IPA_RM_RESOURCE_RELEASED
};

typedef void (*ipa_rm_notify_cb)(void *user_data,
		enum ipa_rm_event event,
		unsigned long data);
/**
 * struct ipa_rm_register_params - information needed to
 *      register IPA RM client with IPA RM
 *
 * @user_data: IPA RM client provided information
 *		to be passed to notify_cb callback below
 * @notify_cb: callback which is called by resource
 *		to notify the IPA RM client about its state
 *		change IPA RM client is expected to perform non
 *		blocking operations only in notify_cb and
 *		release notification context as soon as
 *		possible.
 */
struct ipa_rm_register_params {
	void *user_data;
	ipa_rm_notify_cb notify_cb;
};

/**
 * struct ipa_rm_create_params - information needed to initialize
 *				the resource
 * @name: resource name
 * @floor_voltage: floor voltage needed for client to operate in maximum
 *		bandwidth.
 * @reg_params: register parameters, contains are ignored
 *		for consumer resource NULL should be provided
 *		for consumer resource
 * @request_resource: function which should be called to request resource,
 *			NULL should be provided for producer resource
 * @release_resource: function which should be called to release resource,
 *			NULL should be provided for producer resource
 *
 * IPA RM client is expected to perform non blocking operations only
 * in request_resource and release_resource functions and
 * release notification context as soon as possible.
 */
struct ipa_rm_create_params {
	enum ipa_rm_resource_name name;
	enum ipa_voltage_level floor_voltage;
	struct ipa_rm_register_params reg_params;
	int (*request_resource)(void);
	int (*release_resource)(void);
};

/**
 * struct ipa_rm_perf_profile - information regarding IPA RM client performance
 * profile
 *
 * @max_bandwidth_mbps: maximum bandwidth need of the client in Mbps
 */
struct ipa_rm_perf_profile {
	u32 max_supported_bandwidth_mbps;
};

#define A2_MUX_HDR_NAME_V4_PREF "dmux_hdr_v4_"
#define A2_MUX_HDR_NAME_V6_PREF "dmux_hdr_v6_"

/**
 * enum teth_tethering_mode - Tethering mode (Rmnet / MBIM)
 */
enum teth_tethering_mode {
	TETH_TETHERING_MODE_RMNET,
	TETH_TETHERING_MODE_MBIM,
	TETH_TETHERING_MODE_MAX,
};

/**
 * teth_bridge_init_params - Parameters used for in/out USB API
 * @usb_notify_cb:	Callback function which should be used by the caller.
 * Output parameter.
 * @private_data:	Data for the callback function. Should be used by the
 * caller. Output parameter.
 * @skip_ep_cfg: boolean field that determines if Apps-processor
 *  should or should not confiugre this end-point.
 */
struct teth_bridge_init_params {
	ipa_notify_cb usb_notify_cb;
	void *private_data;
	enum ipa_client_type client;
	bool skip_ep_cfg;
};

/**
 * struct teth_bridge_connect_params - Parameters used in teth_bridge_connect()
 * @ipa_usb_pipe_hdl:	IPA to USB pipe handle, returned from ipa_connect()
 * @usb_ipa_pipe_hdl:	USB to IPA pipe handle, returned from ipa_connect()
 * @tethering_mode:	Rmnet or MBIM
 * @ipa_client_type:    IPA "client" name (IPA_CLIENT_USB#_PROD)
 */
struct teth_bridge_connect_params {
	u32 ipa_usb_pipe_hdl;
	u32 usb_ipa_pipe_hdl;
	enum teth_tethering_mode tethering_mode;
	enum ipa_client_type client_type;
};

/**
 * struct  ipa_tx_data_desc - information needed
 * to send data packet to HW link: link to data descriptors
 * priv: client specific private data
 * @pyld_buffer: pointer to the data buffer that holds frame
 * @pyld_len: length of the data packet
 */
struct ipa_tx_data_desc {
	struct list_head link;
	void *priv;
	void *pyld_buffer;
	u16  pyld_len;
};

/**
 * struct  ipa_rx_data - information needed
 * to send to wlan driver on receiving data from ipa hw
 * @skb: skb
 * @dma_addr: DMA address of this Rx packet
 */
struct ipa_rx_data {
	struct sk_buff *skb;
	dma_addr_t dma_addr;
};

/**
 * enum ipa_irq_type - IPA Interrupt Type
 * Used to register handlers for IPA interrupts
 *
 * Below enum is a logical mapping and not the actual interrupt bit in HW
 */
enum ipa_irq_type {
	IPA_BAD_SNOC_ACCESS_IRQ,
	IPA_EOT_COAL_IRQ,
	IPA_UC_IRQ_0,
	IPA_UC_IRQ_1,
	IPA_UC_IRQ_2,
	IPA_UC_IRQ_3,
	IPA_UC_IN_Q_NOT_EMPTY_IRQ,
	IPA_UC_RX_CMD_Q_NOT_FULL_IRQ,
	IPA_UC_TX_CMD_Q_NOT_FULL_IRQ,
	IPA_UC_TO_PROC_ACK_Q_NOT_FULL_IRQ,
	IPA_PROC_TO_UC_ACK_Q_NOT_EMPTY_IRQ,
	IPA_RX_ERR_IRQ,
	IPA_DEAGGR_ERR_IRQ,
	IPA_TX_ERR_IRQ,
	IPA_STEP_MODE_IRQ,
	IPA_PROC_ERR_IRQ,
	IPA_TX_SUSPEND_IRQ,
	IPA_TX_HOLB_DROP_IRQ,
	IPA_BAM_IDLE_IRQ,
	IPA_IRQ_MAX
};

/**
 * struct ipa_tx_suspend_irq_data - interrupt data for IPA_TX_SUSPEND_IRQ
 * @endpoints: bitmask of endpoints which case IPA_TX_SUSPEND_IRQ interrupt
 * @dma_addr: DMA address of this Rx packet
 */
struct ipa_tx_suspend_irq_data {
	u32 endpoints;
};


/**
 * typedef ipa_irq_handler_t - irq handler/callback type
 * @param ipa_irq_type - [in] interrupt type
 * @param private_data - [in, out] the client private data
 * @param interrupt_data - [out] interrupt information data
 *
 * callback registered by ipa_add_interrupt_handler function to
 * handle a specific interrupt type
 *
 * No return value
 */
typedef void (*ipa_irq_handler_t)(enum ipa_irq_type interrupt,
				void *private_data,
				void *interrupt_data);

/**
 * struct IpaHwBamStats_t - Strucuture holding the BAM statistics
 *
 * @bamFifoFull : Number of times Bam Fifo got full - For In Ch: Good,
 * For Out Ch: Bad
 * @bamFifoEmpty : Number of times Bam Fifo got empty - For In Ch: Bad,
 * For Out Ch: Good
 * @bamFifoUsageHigh : Number of times Bam fifo usage went above 75% -
 * For In Ch: Good, For Out Ch: Bad
 * @bamFifoUsageLow : Number of times Bam fifo usage went below 25% -
 * For In Ch: Bad, For Out Ch: Good
*/
struct IpaHwBamStats_t {
	u32 bamFifoFull;
	u32 bamFifoEmpty;
	u32 bamFifoUsageHigh;
	u32 bamFifoUsageLow;
	u32 bamUtilCount;
} __packed;

/**
 * struct IpaHwRingStats_t - Strucuture holding the Ring statistics
 *
 * @ringFull : Number of times Transfer Ring got full - For In Ch: Good,
 * For Out Ch: Bad
 * @ringEmpty : Number of times Transfer Ring got empty - For In Ch: Bad,
 * For Out Ch: Good
 * @ringUsageHigh : Number of times Transfer Ring usage went above 75% -
 * For In Ch: Good, For Out Ch: Bad
 * @ringUsageLow : Number of times Transfer Ring usage went below 25% -
 * For In Ch: Bad, For Out Ch: Good
*/
struct IpaHwRingStats_t {
	u32 ringFull;
	u32 ringEmpty;
	u32 ringUsageHigh;
	u32 ringUsageLow;
	u32 RingUtilCount;
} __packed;

/**
 * struct IpaHwStatsWDIRxInfoData_t - Structure holding the WDI Rx channel
 * structures
 *
 * @max_outstanding_pkts : Number of outstanding packets in Rx Ring
 * @num_pkts_processed : Number of packets processed - cumulative
 * @rx_ring_rp_value : Read pointer last advertized to the WLAN FW
 * @rx_ind_ring_stats : Ring info
 * @bam_stats : BAM info
 * @num_bam_int_handled : Number of Bam Interrupts handled by FW
 * @num_db : Number of times the doorbell was rung
 * @num_unexpected_db : Number of unexpected doorbells
 * @num_pkts_in_dis_uninit_state : number of completions we
 *		received in disabled or uninitialized state
 * @num_ic_inj_vdev_change : Number of times the Imm Cmd is
 *		injected due to vdev_id change
 * @num_ic_inj_fw_desc_change : Number of times the Imm Cmd is
 *		injected due to fw_desc change
*/
struct IpaHwStatsWDIRxInfoData_t {
	u32 max_outstanding_pkts;
	u32 num_pkts_processed;
	u32 rx_ring_rp_value;
	struct IpaHwRingStats_t rx_ind_ring_stats;
	struct IpaHwBamStats_t bam_stats;
	u32 num_bam_int_handled;
	u32 num_db;
	u32 num_unexpected_db;
	u32 num_pkts_in_dis_uninit_state;
	u32 num_ic_inj_vdev_change;
	u32 num_ic_inj_fw_desc_change;
	u32 reserved1;
	u32 reserved2;
} __packed;

/**
 * struct IpaHwStatsWDITxInfoData_t  - Structure holding the WDI Tx channel
 * structures
 *
 * @num_pkts_processed : Number of packets processed - cumulative
 * @copy_engine_doorbell_value : latest value of doorbell written to copy engine
 * @num_db_fired : Number of DB from uC FW to Copy engine
 * @tx_comp_ring_stats : ring info
 * @bam_stats : BAM info
 * @num_db : Number of times the doorbell was rung
 * @num_unexpected_db : Number of unexpected doorbells
 * @num_bam_int_handled : Number of Bam Interrupts handled by FW
 * @num_bam_int_in_non_running_state : Number of Bam interrupts while not in
 * Running state
 * @num_qmb_int_handled : Number of QMB interrupts handled
*/
struct IpaHwStatsWDITxInfoData_t {
	u32 num_pkts_processed;
	u32 copy_engine_doorbell_value;
	u32 num_db_fired;
	struct IpaHwRingStats_t tx_comp_ring_stats;
	struct IpaHwBamStats_t bam_stats;
	u32 num_db;
	u32 num_unexpected_db;
	u32 num_bam_int_handled;
	u32 num_bam_int_in_non_runnning_state;
	u32 num_qmb_int_handled;
	u32 num_bam_int_handled_while_wait_for_bam;
} __packed;

/**
 * struct IpaHwStatsWDIInfoData_t - Structure holding the WDI channel structures
 *
 * @rx_ch_stats : RX stats
 * @tx_ch_stats : TX stats
*/
struct IpaHwStatsWDIInfoData_t {
	struct IpaHwStatsWDIRxInfoData_t rx_ch_stats;
	struct IpaHwStatsWDITxInfoData_t tx_ch_stats;
} __packed;


/**
 * struct  ipa_wdi_ul_params - WDI_RX configuration
 * @rdy_ring_base_pa: physical address of the base of the Rx ring (containing
 * Rx buffers)
 * @rdy_ring_size: size of the Rx ring in bytes
 * @rdy_ring_rp_pa: physical address of the location through which IPA uc is
 * expected to communicate about the Read pointer into the Rx Ring
 */
struct ipa_wdi_ul_params {
	phys_addr_t rdy_ring_base_pa;
	u32 rdy_ring_size;
	phys_addr_t rdy_ring_rp_pa;
};

/**
 * struct  ipa_wdi_ul_params_smmu - WDI_RX configuration (with WLAN SMMU)
 * @rdy_ring: SG table describing the Rx ring (containing Rx buffers)
 * @rdy_ring_size: size of the Rx ring in bytes
 * @rdy_ring_rp_pa: physical address of the location through which IPA uc is
 * expected to communicate about the Read pointer into the Rx Ring
 */
struct ipa_wdi_ul_params_smmu {
	struct sg_table rdy_ring;
	u32 rdy_ring_size;
	phys_addr_t rdy_ring_rp_pa;
};

/**
 * struct  ipa_wdi_dl_params - WDI_TX configuration
 * @comp_ring_base_pa: physical address of the base of the Tx completion ring
 * @comp_ring_size: size of the Tx completion ring in bytes
 * @ce_ring_base_pa: physical address of the base of the Copy Engine Source
 * Ring
 * @ce_door_bell_pa: physical address of the doorbell that the IPA uC has to
 * write into to trigger the copy engine
 * @ce_ring_size: Copy Engine Ring size in bytes
 * @num_tx_buffers: Number of pkt buffers allocated
 */
struct ipa_wdi_dl_params {
	phys_addr_t comp_ring_base_pa;
	u32 comp_ring_size;
	phys_addr_t ce_ring_base_pa;
	phys_addr_t ce_door_bell_pa;
	u32 ce_ring_size;
	u32 num_tx_buffers;
};

/**
 * struct  ipa_wdi_dl_params_smmu - WDI_TX configuration (with WLAN SMMU)
 * @comp_ring: SG table describing the Tx completion ring
 * @comp_ring_size: size of the Tx completion ring in bytes
 * @ce_ring: SG table describing the Copy Engine Source Ring
 * @ce_door_bell_pa: physical address of the doorbell that the IPA uC has to
 * write into to trigger the copy engine
 * @ce_ring_size: Copy Engine Ring size in bytes
 * @num_tx_buffers: Number of pkt buffers allocated
 */
struct ipa_wdi_dl_params_smmu {
	struct sg_table comp_ring;
	u32 comp_ring_size;
	struct sg_table ce_ring;
	phys_addr_t ce_door_bell_pa;
	u32 ce_ring_size;
	u32 num_tx_buffers;
};

/**
 * struct  ipa_wdi_in_params - information provided by WDI client
 * @sys: IPA EP configuration info
 * @ul: WDI_RX configuration info
 * @dl: WDI_TX configuration info
 * @ul_smmu: WDI_RX configuration info when WLAN uses SMMU
 * @dl_smmu: WDI_TX configuration info when WLAN uses SMMU
 * @smmu_enabled: true if WLAN uses SMMU
 */
struct ipa_wdi_in_params {
	struct ipa_sys_connect_params sys;
	union {
		struct ipa_wdi_ul_params ul;
		struct ipa_wdi_dl_params dl;
		struct ipa_wdi_ul_params_smmu ul_smmu;
		struct ipa_wdi_dl_params_smmu dl_smmu;
	} u;
	bool smmu_enabled;
};

/**
 * struct  ipa_wdi_out_params - information provided to WDI client
 * @uc_door_bell_pa: physical address of IPA uc doorbell
 * @clnt_hdl: opaque handle assigned to client
 */
struct ipa_wdi_out_params {
	phys_addr_t uc_door_bell_pa;
	u32 clnt_hdl;
};

/**
 * struct ipa_wdi_db_params - information provided to retrieve
 *       physical address of uC doorbell
 * @client:	type of "client" (IPA_CLIENT_WLAN#_PROD/CONS)
 * @uc_door_bell_pa: physical address of IPA uc doorbell
 */
struct ipa_wdi_db_params {
	enum ipa_client_type client;
	phys_addr_t uc_door_bell_pa;
};

/**
 * struct  ipa_wdi_uc_ready_params - uC ready CB parameters
 * @is_uC_ready: uC loaded or not
 * @priv : callback cookie
 * @notify:	callback
 */
typedef void (*ipa_uc_ready_cb)(void *priv);
struct ipa_wdi_uc_ready_params {
	bool is_uC_ready;
	void *priv;
	ipa_uc_ready_cb notify;
};

/**
 * struct  ipa_wdi_buffer_info - address info of a WLAN allocated buffer
 * @pa: physical address of the buffer
 * @iova: IOVA of the buffer as embedded inside the WDI descriptors
 * @size: size in bytes of the buffer
 * @result: result of map or unmap operations (out param)
 *
 * IPA driver will create/release IOMMU mapping in IPA SMMU from iova->pa
 */
struct ipa_wdi_buffer_info {
	phys_addr_t pa;
	unsigned long iova;
	size_t size;
	int result;
};

/**
 * struct odu_bridge_params - parameters for odu bridge initialization API
 *
 * @netdev_name: network interface name
 * @priv: private data that will be supplied to client's callback
 * @tx_dp_notify: callback for handling SKB. the following event are supported:
 *	IPA_WRITE_DONE:	will be called after client called to odu_bridge_tx_dp()
 *			Client is expected to free the skb.
 *	IPA_RECEIVE:	will be called for delivering skb to APPS.
 *			Client is expected to deliver the skb to network stack.
 * @send_dl_skb: callback for sending skb on downlink direction to adapter.
 *		Client is expected to free the skb.
 * @device_ethaddr: device Ethernet address in network order.
 * @ipa_desc_size: IPA Sys Pipe Desc Size
 */
struct odu_bridge_params {
	const char *netdev_name;
	void *priv;
	ipa_notify_cb tx_dp_notify;
	int (*send_dl_skb)(void *priv, struct sk_buff *skb);
	u8 device_ethaddr[ETH_ALEN];
	u32 ipa_desc_size;
};

/**
 * enum ipa_mhi_event_type - event type for mhi callback
 *
 * @IPA_MHI_EVENT_READY: IPA MHI is ready and IPA uC is loaded. After getting
 *	this event MHI client is expected to call to ipa_mhi_start() API
 * @IPA_MHI_EVENT_DATA_AVAILABLE: downlink data available on MHI channel
 */
enum ipa_mhi_event_type {
	IPA_MHI_EVENT_READY,
	IPA_MHI_EVENT_DATA_AVAILABLE,
	IPA_MHI_EVENT_MAX,
};

typedef void (*mhi_client_cb)(void *priv, enum ipa_mhi_event_type event,
	unsigned long data);

/**
 * struct ipa_mhi_msi_info - parameters for MSI (Message Signaled Interrupts)
 * @addr_low: MSI lower base physical address
 * @addr_hi: MSI higher base physical address
 * @data: Data Pattern to use when generating the MSI
 * @mask: Mask indicating number of messages assigned by the host to device
 *
 * msi value is written according to this formula:
 *	((data & ~mask) | (mmio.msiVec & mask))
 */
struct ipa_mhi_msi_info {
	u32 addr_low;
	u32 addr_hi;
	u32 data;
	u32 mask;
};

/**
 * struct ipa_mhi_init_params - parameters for IPA MHI initialization API
 *
 * @msi: MSI (Message Signaled Interrupts) parameters
 * @mmio_addr: MHI MMIO physical address
 * @first_ch_idx: First channel ID for hardware accelerated channels.
 * @first_er_idx: First event ring ID for hardware accelerated channels.
 * @assert_bit40: should assert bit 40 in order to access hots space.
 *	if PCIe iATU is configured then not need to assert bit40
 * @notify: client callback
 * @priv: client private data to be provided in client callback
 * @test_mode: flag to indicate if IPA MHI is in unit test mode
 */
struct ipa_mhi_init_params {
	struct ipa_mhi_msi_info msi;
	u32 mmio_addr;
	u32 first_ch_idx;
	u32 first_er_idx;
	bool assert_bit40;
	mhi_client_cb notify;
	void *priv;
	bool test_mode;
};

/**
 * struct ipa_mhi_start_params - parameters for IPA MHI start API
 *
 * @host_ctrl_addr: Base address of MHI control data structures
 * @host_data_addr: Base address of MHI data buffers
 * @channel_context_addr: channel context array address in host address space
 * @event_context_addr: event context array address in host address space
 */
struct ipa_mhi_start_params {
	u32 host_ctrl_addr;
	u32 host_data_addr;
	u64 channel_context_array_addr;
	u64 event_context_array_addr;
};

/**
 * struct ipa_mhi_connect_params - parameters for IPA MHI channel connect API
 *
 * @sys: IPA EP configuration info
 * @channel_id: MHI channel id
 */
struct ipa_mhi_connect_params {
	struct ipa_sys_connect_params sys;
	u8 channel_id;
};

/* bit #40 in address should be asserted for MHI transfers over pcie */
#define IPA_MHI_HOST_ADDR(addr) ((addr) | BIT_ULL(40))

/**
 * struct ipa_gsi_ep_config - IPA GSI endpoint configurations
 *
 * @ipa_ep_num: IPA EP pipe number
 * @ipa_gsi_chan_num: GSI channel number
 * @ipa_if_tlv: number of IPA_IF TLV
 * @ipa_if_aos: number of IPA_IF AOS
 * @ee: Execution environment
 */
struct ipa_gsi_ep_config {
	int ipa_ep_num;
	int ipa_gsi_chan_num;
	int ipa_if_tlv;
	int ipa_if_aos;
	int ee;
};

enum ipa_usb_teth_prot {
	IPA_USB_RNDIS = 0,
	IPA_USB_ECM = 1,
	IPA_USB_RMNET = 2,
	IPA_USB_MBIM = 3,
	IPA_USB_DIAG = 4,
	IPA_USB_MAX_TETH_PROT_SIZE
};

/**
 * ipa_usb_teth_params - parameters for RDNIS/ECM initialization API
 *
 * @host_ethaddr:        host Ethernet address in network order
 * @device_ethaddr:      device Ethernet address in network order
 */
struct ipa_usb_teth_params {
	u8 host_ethaddr[ETH_ALEN];
	u8 device_ethaddr[ETH_ALEN];
};

enum ipa_usb_notify_event {
	IPA_USB_DEVICE_READY,
	IPA_USB_REMOTE_WAKEUP,
	IPA_USB_SUSPEND_COMPLETED
};

enum ipa_usb_max_usb_packet_size {
	IPA_USB_HIGH_SPEED_512B = 512,
	IPA_USB_SUPER_SPEED_1024B = 1024
};

/**
 * ipa_usb_xdci_chan_scratch - xDCI protocol SW config area of
 * channel scratch
 *
 * @last_trb_addr:       Address (LSB - based on alignment restrictions) of
 *                       last TRB in queue. Used to identify roll over case
 * @const_buffer_size:   TRB buffer size in KB (similar to IPA aggregation
 *                       configuration). Must be aligned to max USB Packet Size.
 *                       Should be 1 <= const_buffer_size <= 31.
 * @depcmd_low_addr:     Used to generate "Update Transfer" command
 * @depcmd_hi_addr:      Used to generate "Update Transfer" command.
 */
struct ipa_usb_xdci_chan_scratch {
	u16 last_trb_addr;
	u8 const_buffer_size;
	u32 depcmd_low_addr;
	u8 depcmd_hi_addr;
};

/**
 * ipa_usb_xdci_chan_params - xDCI channel related properties
 *
 * @client:              type of "client"
 * @ipa_ep_cfg:          IPA EP configuration
 * @keep_ipa_awake:      when true, IPA will not be clock gated
 * @teth_prot:           tethering protocol for which the channel is created
 * @gevntcount_low_addr: GEVNCOUNT low address for event scratch
 * @gevntcount_hi_addr:  GEVNCOUNT high address for event scratch
 * @dir:                 channel direction
 * @xfer_ring_len:       length of transfer ring in bytes (must be integral
 *                       multiple of transfer element size - 16B for xDCI)
 * @xfer_ring_base_addr: physical base address of transfer ring. Address must be
 *                       aligned to xfer_ring_len rounded to power of two
 * @xfer_scratch:        parameters for xDCI channel scratch
 *
 */
struct ipa_usb_xdci_chan_params {
	/* IPA EP params */
	enum ipa_client_type client;
	struct ipa_ep_cfg ipa_ep_cfg;
	bool keep_ipa_awake;
	enum ipa_usb_teth_prot teth_prot;
	/* event ring params */
	u32 gevntcount_low_addr;
	u8 gevntcount_hi_addr;
	/* transfer ring params */
	enum gsi_chan_dir dir;
	u16 xfer_ring_len;
	u64 xfer_ring_base_addr;
	struct ipa_usb_xdci_chan_scratch xfer_scratch;
};

/**
 * ipa_usb_chan_out_params - out parameters for channel request
 *
 * @clnt_hdl:            opaque client handle assigned by IPA to client
 * @db_reg_phs_addr_lsb: Physical address of doorbell register where the 32
 *                       LSBs of the doorbell value should be written
 * @db_reg_phs_addr_msb: Physical address of doorbell register where the 32
 *                       MSBs of the doorbell value should be written
 *
 */
struct ipa_req_chan_out_params {
	u32 clnt_hdl;
	u32 db_reg_phs_addr_lsb;
	u32 db_reg_phs_addr_msb;
};

/**
 * ipa_usb_teth_prot_params - parameters for connecting RNDIS
 *
 * @max_xfer_size_bytes_to_dev:   max size of UL packets in bytes
 * @max_packet_number_to_dev:     max number of UL aggregated packets
 * @max_xfer_size_bytes_to_host:  max size of DL packets in bytes
 *
 */
struct ipa_usb_teth_prot_params {
	u32 max_xfer_size_bytes_to_dev;
	u32 max_packet_number_to_dev;
	u32 max_xfer_size_bytes_to_host;
};

/**
 * ipa_usb_xdci_connect_params - parameters required to start IN, OUT
 * channels, and connect RNDIS/ECM/teth_bridge
 *
 * @max_pkt_size:          high speed or full speed
 * @ipa_to_usb_xferrscidx: Transfer Resource Index (XferRscIdx) for IN channel.
 *                         The hardware-assigned transfer resource index for the
 *                         transfer, which was returned in response to the
 *                         Start Transfer command. This field is used for
 *                         "Update Transfer" command.
 *                         Should be 0 =< ipa_to_usb_xferrscidx <= 127.
 * @ipa_to_usb_xferrscidx_valid: true if xferRscIdx should be updated for IN
 *                         channel
 * @usb_to_ipa_xferrscidx: Transfer Resource Index (XferRscIdx) for OUT channel
 *                         Should be 0 =< usb_to_ipa_xferrscidx <= 127.
 * @usb_to_ipa_xferrscidx_valid: true if xferRscIdx should be updated for OUT
 *                         channel
 * @teth_prot:             tethering protocol
 * @teth_prot_params:      parameters for connecting the tethering protocol.
 * @max_supported_bandwidth_mbps: maximum bandwidth need of the client in Mbps
 */
struct ipa_usb_xdci_connect_params {
	enum ipa_usb_max_usb_packet_size max_pkt_size;
	u8 ipa_to_usb_xferrscidx;
	bool ipa_to_usb_xferrscidx_valid;
	u8 usb_to_ipa_xferrscidx;
	bool usb_to_ipa_xferrscidx_valid;
	enum ipa_usb_teth_prot teth_prot;
	struct ipa_usb_teth_prot_params teth_prot_params;
	u32 max_supported_bandwidth_mbps;
};

#if defined CONFIG_IPA || defined CONFIG_IPA3

/*
 * Connect / Disconnect
 */
int ipa_connect(const struct ipa_connect_params *in, struct ipa_sps_params *sps,
		u32 *clnt_hdl);
int ipa_disconnect(u32 clnt_hdl);

/*
 * Resume / Suspend
 */
int ipa_reset_endpoint(u32 clnt_hdl);

/*
 * Remove ep delay
 */
int ipa_clear_endpoint_delay(u32 clnt_hdl);

/*
 * Configuration
 */
int ipa_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg);

int ipa_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ipa_ep_cfg);

int ipa_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ipa_ep_cfg);

int ipa_cfg_ep_hdr_ext(u32 clnt_hdl,
			const struct ipa_ep_cfg_hdr_ext *ipa_ep_cfg);

int ipa_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ipa_ep_cfg);

int ipa_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ipa_ep_cfg);

int ipa_cfg_ep_deaggr(u32 clnt_hdl,
		      const struct ipa_ep_cfg_deaggr *ipa_ep_cfg);

int ipa_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ipa_ep_cfg);

int ipa_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *ipa_ep_cfg);

int ipa_cfg_ep_metadata_mask(u32 clnt_hdl, const struct ipa_ep_cfg_metadata_mask
		*ipa_ep_cfg);

int ipa_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl);

/*
 * Header removal / addition
 */
int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs);

int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls);

int ipa_commit_hdr(void);

int ipa_reset_hdr(void);

int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup);

int ipa_put_hdr(u32 hdr_hdl);

int ipa_copy_hdr(struct ipa_ioc_copy_hdr *copy);

/*
 * Header Processing Context
 */
int ipa_add_hdr_proc_ctx(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs);

int ipa_del_hdr_proc_ctx(struct ipa_ioc_del_hdr_proc_ctx *hdls);

/*
 * Routing
 */
int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules);

int ipa_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls);

int ipa_commit_rt(enum ipa_ip_type ip);

int ipa_reset_rt(enum ipa_ip_type ip);

int ipa_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup);

int ipa_put_rt_tbl(u32 rt_tbl_hdl);

int ipa_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in);

int ipa_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *rules);

/*
 * Filtering
 */
int ipa_add_flt_rule(struct ipa_ioc_add_flt_rule *rules);

int ipa_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls);

int ipa_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *rules);

int ipa_commit_flt(enum ipa_ip_type ip);

int ipa_reset_flt(enum ipa_ip_type ip);

/*
 * NAT
 */
int allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem);

int ipa_nat_init_cmd(struct ipa_ioc_v4_nat_init *init);

int ipa_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma);

int ipa_nat_del_cmd(struct ipa_ioc_v4_nat_del *del);

/*
 * Messaging
 */
int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback);
int ipa_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback);
int ipa_deregister_pull_msg(struct ipa_msg_meta *meta);

/*
 * Interface
 */
int ipa_register_intf(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx);
int ipa_register_intf_ext(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx,
		       const struct ipa_ext_intf *ext);
int ipa_deregister_intf(const char *name);

/*
 * Aggregation
 */
int ipa_set_aggr_mode(enum ipa_aggr_mode mode);

int ipa_set_qcncm_ndp_sig(char sig[3]);

int ipa_set_single_ndp_per_mbim(bool enable);

/*
 * Data path
 */
int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

/*
 * To transfer multiple data packets
 * While passing the data descriptor list, the anchor node
 * should be of type struct ipa_tx_data_desc not list_head
*/
int ipa_tx_dp_mul(enum ipa_client_type dst,
			struct ipa_tx_data_desc *data_desc);

void ipa_free_skb(struct ipa_rx_data *);

/*
 * System pipes
 */
int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl);

int ipa_teardown_sys_pipe(u32 clnt_hdl);

int ipa_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out);
int ipa_disconnect_wdi_pipe(u32 clnt_hdl);
int ipa_enable_wdi_pipe(u32 clnt_hdl);
int ipa_disable_wdi_pipe(u32 clnt_hdl);
int ipa_resume_wdi_pipe(u32 clnt_hdl);
int ipa_suspend_wdi_pipe(u32 clnt_hdl);
int ipa_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats);
u16 ipa_get_smem_restr_bytes(void);
/*
 * To retrieve doorbell physical address of
 * wlan pipes
 */
int ipa_uc_wdi_get_dbpa(struct ipa_wdi_db_params *out);

/*
 * To register uC ready callback if uC not ready
 * and also check uC readiness
 * if uC not ready only, register callback
 */
int ipa_uc_reg_rdyCB(struct ipa_wdi_uc_ready_params *param);

int ipa_create_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);
int ipa_release_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);

/*
 * Resource manager
 */
int ipa_rm_create_resource(struct ipa_rm_create_params *create_params);

int ipa_rm_delete_resource(enum ipa_rm_resource_name resource_name);

int ipa_rm_register(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params);

int ipa_rm_deregister(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params);

int ipa_rm_set_perf_profile(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_perf_profile *profile);

int ipa_rm_add_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name);

int ipa_rm_add_dependency_sync(enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name);

int ipa_rm_delete_dependency(enum ipa_rm_resource_name resource_name,
			enum ipa_rm_resource_name depends_on_name);

int ipa_rm_request_resource(enum ipa_rm_resource_name resource_name);

int ipa_rm_release_resource(enum ipa_rm_resource_name resource_name);

int ipa_rm_notify_completion(enum ipa_rm_event event,
		enum ipa_rm_resource_name resource_name);

int ipa_rm_inactivity_timer_init(enum ipa_rm_resource_name resource_name,
				 unsigned long msecs);

int ipa_rm_inactivity_timer_destroy(enum ipa_rm_resource_name resource_name);

int ipa_rm_inactivity_timer_request_resource(
				enum ipa_rm_resource_name resource_name);

int ipa_rm_inactivity_timer_release_resource(
				enum ipa_rm_resource_name resource_name);

/*
 * Tethering bridge (Rmnet / MBIM)
 */
int teth_bridge_init(struct teth_bridge_init_params *params);

int teth_bridge_disconnect(enum ipa_client_type client);

int teth_bridge_connect(struct teth_bridge_connect_params *connect_params);

/*
 * Tethering client info
 */
void ipa_set_client(int index, enum ipacm_client_enum client, bool uplink);

enum ipacm_client_enum ipa_get_client(int pipe_idx);

bool ipa_get_client_uplink(int pipe_idx);

/*
 * ODU bridge
 */

int odu_bridge_init(struct odu_bridge_params *params);

int odu_bridge_connect(void);

int odu_bridge_disconnect(void);

int odu_bridge_tx_dp(struct sk_buff *skb, struct ipa_tx_meta *metadata);

int odu_bridge_cleanup(void);

/*
 * IPADMA
 */
int ipa_dma_init(void);

int ipa_dma_enable(void);

int ipa_dma_disable(void);

int ipa_dma_sync_memcpy(u64 dest, u64 src, int len);

int ipa_dma_async_memcpy(u64 dest, u64 src, int len,
			void (*user_cb)(void *user1), void *user_param);

int ipa_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len);

void ipa_dma_destroy(void);

/*
 * MHI
 */
int ipa_mhi_init(struct ipa_mhi_init_params *params);

int ipa_mhi_start(struct ipa_mhi_start_params *params);

int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in, u32 *clnt_hdl);

int ipa_mhi_disconnect_pipe(u32 clnt_hdl);

int ipa_mhi_suspend(bool force);

int ipa_mhi_resume(void);

void ipa_mhi_destroy(void);

/*
 * IPA_USB
 */

/**
 * ipa_usb_init_teth_prot - Peripheral should call this function to initialize
 * RNDIS/ECM/teth_bridge/DPL, prior to calling ipa_usb_xdci_connect()
 *
 * @usb_teth_type: tethering protocol type
 * @teth_params:   pointer to tethering protocol parameters.
 *                 Should be struct ipa_usb_teth_params for RNDIS/ECM,
 *                 or NULL for teth_bridge
 * @ipa_usb_notify_cb: will be called to notify USB driver on certain events
 * @user_data:     cookie used for ipa_usb_notify_cb
 *
 * @Return 0 on success, negative on failure
 */
int ipa_usb_init_teth_prot(enum ipa_usb_teth_prot teth_prot,
			   struct ipa_usb_teth_params *teth_params,
			   int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
			   void *),
			   void *user_data);

/**
 * ipa_usb_xdci_connect - Peripheral should call this function to start IN &
 * OUT xDCI channels, and connect RNDIS/ECM/MBIM/RMNET.
 * For DPL, only starts IN channel.
 *
 * @ul_chan_params: parameters for allocating UL xDCI channel. containing
 *              required info on event and transfer rings, and IPA EP
 *              configuration
 * @ul_out_params: [out] opaque client handle assigned by IPA to client & DB
 *              registers physical address for UL channel
 * @dl_chan_params: parameters for allocating DL xDCI channel. containing
 *              required info on event and transfer rings, and IPA EP
 *              configuration
 * @dl_out_params: [out] opaque client handle assigned by IPA to client & DB
 *              registers physical address for DL channel
 * @connect_params: handles and scratch params of the required channels,
 *              tethering protocol and the tethering protocol parameters.
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_usb_xdci_connect(struct ipa_usb_xdci_chan_params *ul_chan_params,
			 struct ipa_usb_xdci_chan_params *dl_chan_params,
			 struct ipa_req_chan_out_params *ul_out_params,
			 struct ipa_req_chan_out_params *dl_out_params,
			 struct ipa_usb_xdci_connect_params *connect_params);

/**
 * ipa_usb_xdci_disconnect - Peripheral should call this function to stop
 * IN & OUT xDCI channels
 * For DPL, only stops IN channel.
 *
 * @ul_clnt_hdl:    client handle received from ipa_usb_xdci_connect()
 *                  for OUT channel
 * @dl_clnt_hdl:    client handle received from ipa_usb_xdci_connect()
 *                  for IN channel
 * @teth_prot:      tethering protocol
 *
 * Note: Should not be called from atomic context
 *
 * @Return 0 on success, negative on failure
 */
int ipa_usb_xdci_disconnect(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			    enum ipa_usb_teth_prot teth_prot);

/**
 * ipa_usb_deinit_teth_prot - Peripheral should call this function to deinit
 * RNDIS/ECM/MBIM/RMNET
 *
 * @teth_prot: tethering protocol
 *
 * @Return 0 on success, negative on failure
 */
int ipa_usb_deinit_teth_prot(enum ipa_usb_teth_prot teth_prot);

/**
 * ipa_usb_xdci_suspend - Peripheral should call this function to suspend
 * IN & OUT or DPL xDCI channels
 *
 * @ul_clnt_hdl: client handle previously obtained from
 *               ipa_usb_xdci_connect() for OUT channel
 * @dl_clnt_hdl: client handle previously obtained from
 *               ipa_usb_xdci_connect() for IN channel
 * @teth_prot:   tethering protocol
 *
 * Note: Should not be called from atomic context
 * Note: for DPL, the ul will be ignored as irrelevant
 *
 * @Return 0 on success, negative on failure
 */
int ipa_usb_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			 enum ipa_usb_teth_prot teth_prot);

/**
 * ipa_usb_xdci_resume - Peripheral should call this function to resume
 * IN & OUT or DPL xDCI channels
 *
 * @ul_clnt_hdl:   client handle received from ipa_usb_xdci_connect()
 *                 for OUT channel
 * @dl_clnt_hdl:   client handle received from ipa_usb_xdci_connect()
 *                 for IN channel
 * @teth_prot:   tethering protocol
 *
 * Note: Should not be called from atomic context
 * Note: for DPL, the ul will be ignored as irrelevant
 *
 * @Return 0 on success, negative on failure
 */
int ipa_usb_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			enum ipa_usb_teth_prot teth_prot);

/*
 * mux id
 */
int ipa_write_qmap_id(struct ipa_ioc_write_qmapid *param_in);

/*
 * interrupts
 */
int ipa_add_interrupt_handler(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data);

int ipa_remove_interrupt_handler(enum ipa_irq_type interrupt);

int ipa_restore_suspend_handler(void);

/*
 * Miscellaneous
 */
void ipa_bam_reg_dump(void);

int ipa_get_ep_mapping(enum ipa_client_type client);

bool ipa_is_ready(void);

void ipa_proxy_clk_vote(void);
void ipa_proxy_clk_unvote(void);

enum ipa_hw_type ipa_get_hw_type(void);

bool ipa_is_client_handle_valid(u32 clnt_hdl);

enum ipa_client_type ipa_get_client_mapping(int pipe_idx);

enum ipa_rm_resource_name ipa_get_rm_resource_from_ep(int pipe_idx);

bool ipa_get_modem_cfg_emb_pipe_flt(void);

enum ipa_transport_type ipa_get_transport_type(void);

struct device *ipa_get_dma_dev(void);
struct iommu_domain *ipa_get_smmu_domain(void);

int ipa_disable_apps_wan_cons_deaggr(uint32_t agg_size, uint32_t agg_count);

struct ipa_gsi_ep_config *ipa_get_gsi_ep_info(int ipa_ep_idx);

int ipa_stop_gsi_channel(u32 clnt_hdl);

typedef void (*ipa_ready_cb)(void *user_data);

/**
* ipa_register_ipa_ready_cb() - register a callback to be invoked
* when IPA core driver initialization is complete.
*
* @ipa_ready_cb:    CB to be triggered.
* @user_data:       Data to be sent to the originator of the CB.
*
* Note: This function is expected to be utilized when ipa_is_ready
* function returns false.
* An IPA client may also use this function directly rather than
* calling ipa_is_ready beforehand, as if this API returns -EEXIST,
* this means IPA initialization is complete (and no callback will
* be triggered).
* When the callback is triggered, the client MUST perform his
* operations in a different context.
*
* The function will return 0 on success, -ENOMEM on memory issues and
* -EEXIST if IPA initialization is complete already.
*/
int ipa_register_ipa_ready_cb(void (*ipa_ready_cb)(void *user_data),
			      void *user_data);

#else /* (CONFIG_IPA || CONFIG_IPA3) */

/*
 * Connect / Disconnect
 */
static inline int ipa_connect(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps,	u32 *clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_disconnect(u32 clnt_hdl)
{
	return -EPERM;
}

/*
 * Resume / Suspend
 */
static inline int ipa_reset_endpoint(u32 clnt_hdl)
{
	return -EPERM;
}

/*
 * Remove ep delay
 */
static inline int ipa_clear_endpoint_delay(u32 clnt_hdl)
{
	return -EPERM;
}

/*
 * Configuration
 */
static inline int ipa_cfg_ep(u32 clnt_hdl,
		const struct ipa_ep_cfg *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_nat(u32 clnt_hdl,
		const struct ipa_ep_cfg_nat *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_hdr(u32 clnt_hdl,
		const struct ipa_ep_cfg_hdr *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_hdr_ext(u32 clnt_hdl,
		const struct ipa_ep_cfg_hdr_ext *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_mode(u32 clnt_hdl,
		const struct ipa_ep_cfg_mode *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_aggr(u32 clnt_hdl,
		const struct ipa_ep_cfg_aggr *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_deaggr(u32 clnt_hdl,
		const struct ipa_ep_cfg_deaggr *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_route(u32 clnt_hdl,
		const struct ipa_ep_cfg_route *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_holb(u32 clnt_hdl,
		const struct ipa_ep_cfg_holb *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_cfg(u32 clnt_hdl,
		const struct ipa_ep_cfg_cfg *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_metadata_mask(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask *ipa_ep_cfg)
{
	return -EPERM;
}

static inline int ipa_cfg_ep_ctrl(u32 clnt_hdl,
			const struct ipa_ep_cfg_ctrl *ep_ctrl)
{
	return -EPERM;
}

/*
 * Header removal / addition
 */
static inline int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs)
{
	return -EPERM;
}

static inline int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls)
{
	return -EPERM;
}

static inline int ipa_commit_hdr(void)
{
	return -EPERM;
}

static inline int ipa_reset_hdr(void)
{
	return -EPERM;
}

static inline int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup)
{
	return -EPERM;
}

static inline int ipa_put_hdr(u32 hdr_hdl)
{
	return -EPERM;
}

static inline int ipa_copy_hdr(struct ipa_ioc_copy_hdr *copy)
{
	return -EPERM;
}

/*
 * Header Processing Context
 */
static inline int ipa_add_hdr_proc_ctx(
				struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs)
{
	return -EPERM;
}

static inline int ipa_del_hdr_proc_ctx(struct ipa_ioc_del_hdr_proc_ctx *hdls)
{
	return -EPERM;
}
/*
 * Routing
 */
static inline int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules)
{
	return -EPERM;
}

static inline int ipa_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls)
{
	return -EPERM;
}

static inline int ipa_commit_rt(enum ipa_ip_type ip)
{
	return -EPERM;
}

static inline int ipa_reset_rt(enum ipa_ip_type ip)
{
	return -EPERM;
}

static inline int ipa_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup)
{
	return -EPERM;
}

static inline int ipa_put_rt_tbl(u32 rt_tbl_hdl)
{
	return -EPERM;
}

static inline int ipa_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in)
{
	return -EPERM;
}

static inline int ipa_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *rules)
{
	return -EPERM;
}

/*
 * Filtering
 */
static inline int ipa_add_flt_rule(struct ipa_ioc_add_flt_rule *rules)
{
	return -EPERM;
}

static inline int ipa_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls)
{
	return -EPERM;
}

static inline int ipa_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *rules)
{
	return -EPERM;
}

static inline int ipa_commit_flt(enum ipa_ip_type ip)
{
	return -EPERM;
}

static inline int ipa_reset_flt(enum ipa_ip_type ip)
{
	return -EPERM;
}

/*
 * NAT
 */
static inline int allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem)
{
	return -EPERM;
}


static inline int ipa_nat_init_cmd(struct ipa_ioc_v4_nat_init *init)
{
	return -EPERM;
}


static inline int ipa_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma)
{
	return -EPERM;
}


static inline int ipa_nat_del_cmd(struct ipa_ioc_v4_nat_del *del)
{
	return -EPERM;
}

/*
 * Messaging
 */
static inline int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
		ipa_msg_free_fn callback)
{
	return -EPERM;
}

static inline int ipa_register_pull_msg(struct ipa_msg_meta *meta,
		ipa_msg_pull_fn callback)
{
	return -EPERM;
}

static inline int ipa_deregister_pull_msg(struct ipa_msg_meta *meta)
{
	return -EPERM;
}

/*
 * Interface
 */
static inline int ipa_register_intf(const char *name,
				     const struct ipa_tx_intf *tx,
				     const struct ipa_rx_intf *rx)
{
	return -EPERM;
}

static inline int ipa_register_intf_ext(const char *name,
		const struct ipa_tx_intf *tx,
		const struct ipa_rx_intf *rx,
		const struct ipa_ext_intf *ext)
{
	return -EPERM;
}

static inline int ipa_deregister_intf(const char *name)
{
	return -EPERM;
}

/*
 * Aggregation
 */
static inline int ipa_set_aggr_mode(enum ipa_aggr_mode mode)
{
	return -EPERM;
}

static inline int ipa_set_qcncm_ndp_sig(char sig[3])
{
	return -EPERM;
}

static inline int ipa_set_single_ndp_per_mbim(bool enable)
{
	return -EPERM;
}

/*
 * Data path
 */
static inline int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata)
{
	return -EPERM;
}

/*
 * To transfer multiple data packets
 */
static inline int ipa_tx_dp_mul(
	enum ipa_client_type dst,
	struct ipa_tx_data_desc *data_desc)
{
	return -EPERM;
}

static inline void ipa_free_skb(struct ipa_rx_data *rx_in)
{
	return;
}

/*
 * System pipes
 */
static inline u16 ipa_get_smem_restr_bytes(void)
{
	return -EPERM;
}

static inline int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in,
		u32 *clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_teardown_sys_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out)
{
	return -EPERM;
}

static inline int ipa_disconnect_wdi_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_enable_wdi_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_disable_wdi_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_resume_wdi_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_suspend_wdi_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_uc_wdi_get_dbpa(
	struct ipa_wdi_db_params *out)
{
	return -EPERM;
}

static inline int ipa_uc_reg_rdyCB(
	struct ipa_wdi_uc_ready_params *param)
{
	return -EPERM;
}


/*
 * Resource manager
 */
static inline int ipa_rm_create_resource(
		struct ipa_rm_create_params *create_params)
{
	return -EPERM;
}

static inline int ipa_rm_delete_resource(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_register(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	return -EPERM;
}

static inline int ipa_rm_set_perf_profile(
		enum ipa_rm_resource_name resource_name,
		struct ipa_rm_perf_profile *profile)
{
	return -EPERM;
}

static inline int ipa_rm_deregister(enum ipa_rm_resource_name resource_name,
			struct ipa_rm_register_params *reg_params)
{
	return -EPERM;
}

static inline int ipa_rm_add_dependency(
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name)
{
	return -EPERM;
}

static inline int ipa_rm_add_dependency_sync(
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name)
{
	return -EPERM;
}

static inline int ipa_rm_delete_dependency(
		enum ipa_rm_resource_name resource_name,
		enum ipa_rm_resource_name depends_on_name)
{
	return -EPERM;
}

static inline int ipa_rm_request_resource(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_release_resource(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_notify_completion(enum ipa_rm_event event,
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_init(
		enum ipa_rm_resource_name resource_name,
			unsigned long msecs)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_destroy(
		enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_request_resource(
				enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

static inline int ipa_rm_inactivity_timer_release_resource(
				enum ipa_rm_resource_name resource_name)
{
	return -EPERM;
}

/*
 * Tethering bridge (Rmnet / MBIM)
 */
static inline int teth_bridge_init(struct teth_bridge_init_params *params)
{
	return -EPERM;
}

static inline int teth_bridge_disconnect(enum ipa_client_type client)
{
	return -EPERM;
}

static inline int teth_bridge_connect(struct teth_bridge_connect_params
				      *connect_params)
{
	return -EPERM;
}

/*
 * Tethering client info
 */
static inline void ipa_set_client(int index, enum ipacm_client_enum client,
	bool uplink)
{
	return;
}

static inline enum ipacm_client_enum ipa_get_client(int pipe_idx)
{
	return -EPERM;
}

static inline bool ipa_get_client_uplink(int pipe_idx)
{
	return -EPERM;
}


/*
 * ODU bridge
 */
static inline int odu_bridge_init(struct odu_bridge_params *params)
{
	return -EPERM;
}

static inline int odu_bridge_disconnect(void)
{
	return -EPERM;
}

static inline int odu_bridge_connect(void)
{
	return -EPERM;
}

static inline int odu_bridge_tx_dp(struct sk_buff *skb,
						struct ipa_tx_meta *metadata)
{
	return -EPERM;
}

static inline int odu_bridge_cleanup(void)
{
	return -EPERM;
}

/*
 * IPADMA
 */
static inline int ipa_dma_init(void)
{
	return -EPERM;
}

static inline int ipa_dma_enable(void)
{
	return -EPERM;
}

static inline int ipa_dma_disable(void)
{
	return -EPERM;
}

static inline int ipa_dma_sync_memcpy(phys_addr_t dest, phys_addr_t src
			, int len)
{
	return -EPERM;
}

static inline int ipa_dma_async_memcpy(phys_addr_t dest, phys_addr_t src
			, int len, void (*user_cb)(void *user1),
			void *user_param)
{
	return -EPERM;
}

static inline int ipa_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len)
{
	return -EPERM;
}

static inline void ipa_dma_destroy(void)
{
	return;
}

/*
 * MHI
 */
static inline int ipa_mhi_init(struct ipa_mhi_init_params *params)
{
	return -EPERM;
}

static inline int ipa_mhi_start(struct ipa_mhi_start_params *params)
{
	return -EPERM;
}

static inline int ipa_mhi_connect_pipe(struct ipa_mhi_connect_params *in,
	u32 *clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_mhi_disconnect_pipe(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_mhi_suspend(bool force)
{
	return -EPERM;
}

static inline int ipa_mhi_resume(void)
{
	return -EPERM;
}

static inline void ipa_mhi_destroy(void)
{
	return;
}

/*
 * IPA_USB
 */

static inline int ipa_usb_init_teth_prot(enum ipa_usb_teth_prot teth_prot,
			   struct ipa_usb_teth_params *teth_params,
			   int (*ipa_usb_notify_cb)(enum ipa_usb_notify_event,
			   void *),
			   void *user_data)
{
	return -EPERM;
}

static inline int ipa_usb_xdci_connect(
			 struct ipa_usb_xdci_chan_params *ul_chan_params,
			 struct ipa_usb_xdci_chan_params *dl_chan_params,
			 struct ipa_req_chan_out_params *ul_out_params,
			 struct ipa_req_chan_out_params *dl_out_params,
			 struct ipa_usb_xdci_connect_params *connect_params)
{
	return -EPERM;
}

static inline int ipa_usb_xdci_disconnect(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			    enum ipa_usb_teth_prot teth_prot)
{
	return -EPERM;
}

static inline int ipa_usb_deinit_teth_prot(enum ipa_usb_teth_prot teth_prot)
{
	return -EPERM;
}

static inline int ipa_usb_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			 enum ipa_usb_teth_prot teth_prot)
{
	return -EPERM;
}

static inline int ipa_usb_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
			enum ipa_usb_teth_prot teth_prot)
{
	return -EPERM;
}

/*
 * mux id
 */
static inline int ipa_write_qmap_id(struct ipa_ioc_write_qmapid *param_in)
{
	return -EPERM;
}

/*
 * interrupts
 */
static inline int ipa_add_interrupt_handler(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data)
{
	return -EPERM;
}

static inline int ipa_remove_interrupt_handler(enum ipa_irq_type interrupt)
{
	return -EPERM;
}

static inline int ipa_restore_suspend_handler(void)
{
	return -EPERM;
}

/*
 * Miscellaneous
 */
static inline void ipa_bam_reg_dump(void)
{
	return;
}

static inline int ipa_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	return -EPERM;
}

static inline int ipa_get_ep_mapping(enum ipa_client_type client)
{
	return -EPERM;
}

static inline bool ipa_is_ready(void)
{
	return false;
}

static inline void ipa_proxy_clk_vote(void)
{
}

static inline void ipa_proxy_clk_unvote(void)
{
}

static inline enum ipa_hw_type ipa_get_hw_type(void)
{
	return IPA_HW_None;
}

static inline bool ipa_is_client_handle_valid(u32 clnt_hdl)
{
	return -EINVAL;
}

static inline enum ipa_client_type ipa_get_client_mapping(int pipe_idx)
{
	return -EINVAL;
}

static inline enum ipa_rm_resource_name ipa_get_rm_resource_from_ep(
	int pipe_idx)
{
	return -EFAULT;
}

static inline bool ipa_get_modem_cfg_emb_pipe_flt(void)
{
	return -EINVAL;
}

static inline enum ipa_transport_type ipa_get_transport_type(void)
{
	return -EFAULT;
}

static inline struct device *ipa_get_dma_dev(void)
{
	return NULL;
}

static inline struct iommu_domain *ipa_get_smmu_domain(void)
{
	return NULL;
}

static inline int ipa_create_wdi_mapping(u32 num_buffers,
		struct ipa_wdi_buffer_info *info)
{
	return -EINVAL;
}

static inline int ipa_release_wdi_mapping(u32 num_buffers,
		struct ipa_wdi_buffer_info *info)
{
	return -EINVAL;
}

static inline int ipa_disable_apps_wan_cons_deaggr(void)
{
	return -EINVAL;
}

static inline struct ipa_gsi_ep_config *ipa_get_gsi_ep_info(int ipa_ep_idx)
{
	return NULL;
}

static inline int ipa_stop_gsi_channel(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_register_ipa_ready_cb(
	void (*ipa_ready_cb)(void *user_data),
	void *user_data)
{
	return -EPERM;
}

#endif /* (CONFIG_IPA || CONFIG_IPA3) */

#endif /* _IPA_H_ */
