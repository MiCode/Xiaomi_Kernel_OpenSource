/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_H_
#define _IPA_H_

#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/msm_ipa.h>
#include <linux/skbuff.h>
#include <linux/types.h>
#include <linux/ipa_qmi_service_v01.h>
#include <linux/msm_gsi.h>

#define IPA_APPS_MAX_BW_IN_MBPS 700
#define IPA_BW_THRESHOLD_MAX 3

#define IPA_MAX_CH_STATS_SUPPORTED 5

/**
 * the attributes of the socksv5 options
 */
#define IPA_SOCKSv5_ENTRY_VALID	(1ul << 0)
#define IPA_SOCKSv5_IPV4	(1ul << 1)
#define IPA_SOCKSv5_IPV6	(1ul << 2)
#define IPA_SOCKSv5_OPT_TS	(1ul << 3)
#define IPA_SOCKSv5_OPT_SACK	(1ul << 4)
#define IPA_SOCKSv5_OPT_WS_STC	(1ul << 5)
#define IPA_SOCKSv5_OPT_WS_DMC	(1ul << 6)

#define IPA_SOCKsv5_ADD_COM_ID		15
#define IPA_SOCKsv5_ADD_V6_V4_COM_PM	1
#define IPA_SOCKsv5_ADD_V4_V6_COM_PM	2
#define IPA_SOCKsv5_ADD_V6_V6_COM_PM	3

/* Notifiers for rmnet driver */
#define BUFF_ABOVE_HIGH_THRESHOLD_FOR_DEFAULT_PIPE        1
#define BUFF_ABOVE_HIGH_THRESHOLD_FOR_COAL_PIPE           2
#define BUFF_BELOW_LOW_THRESHOLD_FOR_DEFAULT_PIPE         3
#define BUFF_BELOW_LOW_THRESHOLD_FOR_COAL_PIPE            4

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
 * enum ipa_ipv6ct_en_type - IPv6CT setting type in IPA end-point
 */
enum ipa_ipv6ct_en_type {
	IPA_BYPASS_IPV6CT,
	IPA_ENABLE_IPV6CT,
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
	IPA_COALESCE = 5,
	IPA_QCMAP   = 6,
};

/**
 * enum ipa_aggr_mode - global aggregation mode
 */
enum ipa_aggr_mode {
	IPA_MBIM_AGGR,
	IPA_QCNCM_AGGR,
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
 * enum hdr_total_len_or_pad_type - type of value held by TOTAL_LEN_OR_PAD
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
 * struct ipa_ep_cfg_conn_track - IPv6 Connection tracking configuration in
 *	IPA end-point
 * @conn_track_en: Defines speculative conn_track action, means if specific
 *		   pipe needs to have UL/DL IPv6 Connection Tracking or Bypass
 *		   IPv6 Connection Tracking. 0: Bypass IPv6 Connection Tracking
 *					     1: IPv6 UL/DL Connection Tracking.
 *		  Valid for Input Pipes only (IPA consumer)
 */
struct ipa_ep_cfg_conn_track {
	enum ipa_ipv6ct_en_type conn_track_en;
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
 *			Starting IPA4.5, this field in H/W requires more bits
 *			to support larger range, but no spare bits to use.
 *			So the MSB part is done thourgh the EXT register.
 *			When accessing this register, need to access the EXT
 *			register as well.
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
 *			Starting IPA4.5, this field in H/W requires more bits
 *			to support larger range, but no spare bits to use.
 *			So the MSB part is done thourgh the EXT register.
 *			When accessing this register, need to access the EXT
 *			register as well.
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
 *			Starting IPA4.5, this parameter is irrelevant and H/W
 *			assumes it is always valid.
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
 * @hdr: The header structure. Used starting IPA4.5 where part of the info
 *	at the header structure is implemented via the EXT register at the H/W
 * @hdr_bytes_to_remove_valid: 0-Ignore hdr_bytes_to_remove field, 1-Process
 *	hdr_bytes_to_remove field
 * @hdr_bytes_to_remove: desired bytes to remove from top of the packet for
 *	partial L2 header retention
 */
struct ipa_ep_cfg_hdr_ext {
	u32 hdr_pad_to_alignment;
	u32 hdr_total_len_or_pad_offset;
	bool hdr_payload_len_inc_padding;
	enum hdr_total_len_or_pad_type hdr_total_len_or_pad;
	bool hdr_total_len_or_pad_valid;
	bool hdr_little_endian;
	struct ipa_ep_cfg_hdr *hdr;
	bool hdr_bytes_to_remove_valid;
	u32 hdr_bytes_to_remove;
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
 * @aggr_time_limit:	Timer to close aggregated packet When set to 0,
 *			there is no time limitation on the aggregation.  When
 *			both, Aggr_Byte_Limit and Aggr_Time_Limit are set to 0,
 *			there is no aggregation, every packet is sent
 *			independently according to the aggregation structure
 *			Valid for Output Pipes only (IPA Producer).
 *			Time unit is -->> usec <<--
 * @aggr_pkt_limit: Defines if EOF close aggregation or not. if set to false
 *			HW closes aggregation (sends EOT) only based on its
 *			aggregation config (byte/time limit, etc). if set to
 *			true EOF closes aggregation in addition to HW based
 *			aggregation closure. Valid for Output Pipes only (IPA
 *			Producer). EOF affects only Pipes configured for
 *			generic aggregation.
 * @aggr_hard_byte_limit_en: If set to 1, byte-limit aggregation for this
 *			pipe will apply a hard-limit behavior which will not
 *			allow frames to be closed with more than byte-limit
 *			bytes. If set to 0, previous byte-limit behavior
 *			will apply - frames close once a packet causes the
 *			accumulated byte-count to cross the byte-limit
 *			threshold (closed frame will contain that packet).
 * @aggr_sw_eof_active: 0: EOF does not close aggregation. HW closes aggregation
 *			(sends EOT) only based on its aggregation config
 *			(byte/time limit, etc).
 *			1: EOF closes aggregation in addition to HW based
 *			aggregation closure. Valid for Output Pipes only (IPA
 *			Producer). EOF affects only Pipes configured for generic
 *			aggregation.
 * @pulse_generator:	Pulse generator number to be used.
 *			For internal use.
 *			Supported starting IPA4.5.
 * @scaled_time:	Time limit in accordance to the pulse generator
 *			granularity.
 *			For internal use
 *			Supported starting IPA4.5
 */
struct ipa_ep_cfg_aggr {
	enum ipa_aggr_en_type aggr_en;
	enum ipa_aggr_type aggr;
	u32 aggr_byte_limit;
	u32 aggr_time_limit;
	u32 aggr_pkt_limit;
	u32 aggr_hard_byte_limit_en;
	bool aggr_sw_eof_active;
	u8 pulse_generator;
	u8 scaled_time;
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
 *  IPAv4.2: splitting timer value into 2 fields. Timer value is:
 *   BASE_VALUE * (2^SCALE)
 *  IPA4.5: tmr_val is in -->>msec<<--. Range is dynamic based
 *   on H/W configuration. (IPA4.5 absolute maximum is 0.65535*31 -> ~20sec).
 * @base_val : IPA4.2 only field. base value of the timer.
 * @scale : IPA4.2 only field. scale value for timer.
 * @pulse_generator: Pulse generator number to be used.
 *  For internal use.
 *  Supported starting IPA4.5.
 * @scaled_time: Time limit in accordance to the pulse generator granularity
 *  For internal use
 *  Supported starting IPA4.5
 */
struct ipa_ep_cfg_holb {
	u32 tmr_val;
	u32 base_val;
	u32 scale;
	u16 en;
	u8 pulse_generator;
	u8 scaled_time;
};

/**
 * struct ipa_ep_cfg_deaggr - deaggregation configuration in IPA end-point
 * @deaggr_hdr_len: Deaggregation Header length in bytes. Valid only for Input
 *	Pipes, which are configured for 'Generic' deaggregation.
 * @syspipe_err_detection - If set to 1, enables error detection for
 *	de-aggregration. Valid only for Input Pipes, which are configured
 *	for 'Generic' deaggregation.
 *	Note: if this bit is set, de-aggregated frames must be contiguous
 *	in memory.
 * @packet_offset_valid: - 0: PACKET_OFFSET is not used, 1: PACKET_OFFSET is
 *	used.
 * @packet_offset_location: Location of packet offset field, which specifies
 *	the offset to the packet from the start of the packet offset field.
 * @ignore_min_pkt_err - Ignore packets smaller than header. This is intended
 *	for use in RNDIS de-aggregated pipes, to silently ignore a redundant
 *	1-byte trailer in MSFT implementation.
 * @max_packet_len: DEAGGR Max Packet Length in Bytes. A Packet with higher
 *	size wil be treated as an error. 0 - Packet Length is not Bound,
 *	IPA should not check for a Max Packet Length.
 */
struct ipa_ep_cfg_deaggr {
	u32 deaggr_hdr_len;
	bool syspipe_err_detection;
	bool packet_offset_valid;
	u32 packet_offset_location;
	bool ignore_min_pkt_err;
	u32 max_packet_len;
};

/**
 * enum ipa_cs_offload - checksum offload setting
 */
enum ipa_cs_offload {
	IPA_DISABLE_CS_OFFLOAD,
	/*
	 * For enum value = 1, we check the csum required/valid bit which is the
	 * same bit used for both DL and UL but have different meanings.
	 * For UL pipe, HW checks if it needs to perform Csum caluclation.
	 * For DL pipe, HW checks if the csum is valid or invalid
	 */
	IPA_ENABLE_CS_OFFLOAD_UL,
	IPA_ENABLE_CS_DL_QMAP = IPA_ENABLE_CS_OFFLOAD_UL,
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
 * @gen_qmb_master_sel: Select bit for ENDP GEN-QMB master. This is used to
 *	separate DDR & PCIe transactions in-order to limit them as
 *	a group (using MAX_WRITES/READS limiation). Valid for input and
 *	output pipes (IPA consumer+producer)
 */
struct ipa_ep_cfg_cfg {
	bool frag_offload_en;
	enum ipa_cs_offload cs_offload_en;
	u8 cs_metadata_hdr_offset;
	u8 gen_qmb_master_sel;
	u8 tx_instance;
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
 * struct ipa_ep_cfg_seq - HPS/DPS sequencer type configuration in IPA end-point
 * @set_dynamic:  0 - HPS/DPS seq type is configured statically,
 *		   1 - HPS/DPS seq type is set to seq_type
 * @seq_type: HPS/DPS sequencer type configuration
 */
struct ipa_ep_cfg_seq {
	bool set_dynamic;
	int seq_type;
};

/**
 * struct ipa_ep_cfg_ulso - ULSO configurations
 * @ipid_min_max_idx: A value in the range [0, 2]. Determines the registers
 *		pair from which to read the minimum and maximum of IPv4 packets ID. It
 *		is set to 0 as this range is platform specific and there is no need for
 *		more than one pair values for this range. The minimum and maximum values
 *		are taken from the device tree in pre_init and are stored in dedicated
 *		registers.
 * @is_ulso_pipe: Indicates whether the pipe is in ulso operation mode.
 */
struct ipa_ep_cfg_ulso {
	int ipid_min_max_idx;
	bool is_ulso_pipe;
};

/**
 * struct ipa_ep_cfg - configuration of IPA end-point
 * @nat:		NAT parameters
 * @conn_track:		IPv6CT parameters
 * @hdr:		Header parameters
 * @hdr_ext:		Extended header parameters
 * @mode:		Mode parameters
 * @aggr:		Aggregation parameters
 * @deaggr:		Deaggregation params
 * @route:		Routing parameters
 * @cfg:		Configuration register data
 * @metadata_mask:	Hdr metadata mask
 * @meta:		Meta Data
 * @seq:		HPS/DPS sequencers configuration
 * @ulso:		ULSO configuration
 */
struct ipa_ep_cfg {
	struct ipa_ep_cfg_nat nat;
	struct ipa_ep_cfg_conn_track conn_track;
	struct ipa_ep_cfg_hdr hdr;
	struct ipa_ep_cfg_hdr_ext hdr_ext;
	struct ipa_ep_cfg_mode mode;
	struct ipa_ep_cfg_aggr aggr;
	struct ipa_ep_cfg_deaggr deaggr;
	struct ipa_ep_cfg_route route;
	struct ipa_ep_cfg_cfg cfg;
	struct ipa_ep_cfg_metadata_mask metadata_mask;
	struct ipa_ep_cfg_metadata meta;
	struct ipa_ep_cfg_seq seq;
	struct ipa_ep_cfg_ulso ulso;
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
 * enum ipa_wdi_meter_evt_type - type of event client callback is
 * for AP+STA mode metering
 * @IPA_GET_WDI_SAP_STATS: get IPA_stats betwen SAP and STA -
 *			use ipa_get_wdi_sap_stats structure
 * @IPA_SET_WIFI_QUOTA: set quota limit on STA -
 *			use ipa_set_wifi_quota structure
 * @IPA_SET_WLAN_BW: set wlan BW -
 *			use ipa_set_wlan_bw structure
 */
enum ipa_wdi_meter_evt_type {
	IPA_GET_WDI_SAP_STATS,
	IPA_SET_WIFI_QUOTA,
	IPA_INFORM_WLAN_BW,
};

struct ipa_get_wdi_sap_stats {
	/* indicate to reset stats after query */
	uint8_t reset_stats;
	/* indicate valid stats from wlan-fw */
	uint8_t stats_valid;
	/* Tx: SAP->STA */
	uint64_t ipv4_tx_packets;
	uint64_t ipv4_tx_bytes;
	/* Rx: STA->SAP */
	uint64_t ipv4_rx_packets;
	uint64_t ipv4_rx_bytes;
	uint64_t ipv6_tx_packets;
	uint64_t ipv6_tx_bytes;
	uint64_t ipv6_rx_packets;
	uint64_t ipv6_rx_bytes;
};

/**
 * struct ipa_set_wifi_quota - structure used for
 *                                   IPA_SET_WIFI_QUOTA.
 *
 * @quota_bytes:    Quota (in bytes) for the STA interface.
 * @set_quota:       Indicate whether to set the quota (use 1) or
 *                   unset the quota.
 *
 */
struct ipa_set_wifi_quota {
	uint64_t quota_bytes;
	uint8_t  set_quota;
	/* indicate valid quota set from wlan-fw */
	uint8_t set_valid;
};

/**
 * struct ipa_inform_wlan_bw - structure used for
 *                                   IPA_INFORM_WLAN_BW.
 *
 * @index:       Indicate which bw-index hit
 * @throughput:  throughput usage
 *
 */
struct ipa_inform_wlan_bw {
	uint8_t  index;
	uint64_t throughput;
};

typedef void (*ipa_wdi_meter_notifier_cb)(enum ipa_wdi_meter_evt_type evt,
		       void *data);


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
 * @desc_fifo_sz: size of desc FIFO. This number is used to allocate the desc
 *		fifo for BAM. For GSI, this size is used by IPA driver as a
 *		baseline to calculate the GSI ring size in the following way:
 *		For PROD pipes, GSI ring is 4 * desc_fifo_sz.
		For PROD pipes, GSI ring is 2 * desc_fifo_sz.
 * @priv:	callback cookie
 * @notify:	callback
 *		priv - callback cookie
 *		evt - type of event
 *		data - data relevant to event.  May not be valid. See event_type
 *		enum for valid cases.
 * @skip_ep_cfg: boolean field that determines if EP should be configured
 *  by IPA driver
 * @keep_ipa_awake: when true, IPA will not be clock gated
 * @napi_enabled: when true, IPA call client callback to start polling
 * @bypass_agg: when true, IPA bypasses the aggregation
 * @int_modt: GSI event ring interrupt moderation time
 *		cycles base interrupt moderation (32KHz clock)
 * @int_modc: GSI event ring interrupt moderation packet counter
 * @buff_size: Actual buff size of rx_pkt
 * @ext_ioctl_v2: Flag to determine whether ioctl_v2 received
 */
struct ipa_sys_connect_params {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	u32 desc_fifo_sz;
	void *priv;
	ipa_notify_cb notify;
	bool skip_ep_cfg;
	bool keep_ipa_awake;
	struct napi_struct *napi_obj;
	bool recycle_enabled;
	bool bypass_agg;
	u32 int_modt;
	u32 int_modc;
	u32 buff_size;
	bool ext_ioctl_v2;
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
	IPA_VOLTAGE_SVS2 = IPA_VOLTAGE_UNSPECIFIED,
	IPA_VOLTAGE_SVS,
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
	IPA_UC_IRQ_0,
	IPA_UC_IRQ_1,
	IPA_UC_IRQ_2,
	IPA_UC_IRQ_3,
	IPA_UC_IN_Q_NOT_EMPTY_IRQ,
	IPA_UC_RX_CMD_Q_NOT_FULL_IRQ,
	IPA_PROC_TO_UC_ACK_Q_NOT_EMPTY_IRQ,
	IPA_RX_ERR_IRQ,
	IPA_DEAGGR_ERR_IRQ,
	IPA_TX_ERR_IRQ,
	IPA_STEP_MODE_IRQ,
	IPA_PROC_ERR_IRQ,
	IPA_TX_SUSPEND_IRQ,
	IPA_TX_HOLB_DROP_IRQ,
	IPA_BAM_GSI_IDLE_IRQ,
	IPA_PIPE_YELLOW_MARKER_BELOW_IRQ,
	IPA_PIPE_RED_MARKER_BELOW_IRQ,
	IPA_PIPE_YELLOW_MARKER_ABOVE_IRQ,
	IPA_PIPE_RED_MARKER_ABOVE_IRQ,
	IPA_UCP_IRQ,
	IPA_DCMP_IRQ,
	IPA_GSI_EE_IRQ,
	IPA_GSI_IPA_IF_TLV_RCVD_IRQ,
	IPA_GSI_UC_IRQ,
	IPA_TLV_LEN_MIN_DSM_IRQ,
	IPA_DRBIP_PKT_EXCEED_MAX_SIZE_IRQ,
	IPA_DRBIP_DATA_SCTR_CFG_ERROR_IRQ,
	IPA_DRBIP_IMM_CMD_NO_FLSH_HZRD_IRQ,
	IPA_IRQ_MAX
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
 * struct IpaHwBamStats_t - Structure holding the BAM statistics
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
 * struct IpaHwRingStats_t - Structure holding the Ring statistics
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
 * struct ipa_uc_dbg_rtk_ring_stats - uC dbg stats info for RTK
 * offloading protocol
 * @commStats: common stats
 * @trCount: transfer ring count
 * @erCount: event ring count
 * @totalAosCount: total AoS completion count
 * @busyTime: total busy time
 */
struct ipa_uc_dbg_rtk_ring_stats {
	struct IpaHwRingStats_t commStats;
	u32 trCount;
	u32 erCount;
	u32 totalAosCount;
	u64 busyTime;
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
 * @num_qmb_int_handled : Number of QMB interrupts handled
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
	u32 num_qmb_int_handled;
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
	u32 num_bam_int_in_non_running_state;
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
 * reading (WDI-1.0)
 * @rdy_comp_ring_base_pa: physical address of the base of the Rx completion
 * ring (WDI-2.0)
 * @rdy_comp_ring_wp_pa: physical address of the location through which IPA
 * uc is writing (WDI-2.0)
 * @rdy_comp_ring_size: size of the Rx_completion ring in bytes
 * expected to communicate about the Read pointer into the Rx Ring
 * @is_txr_rn_db_pcie_addr: tx ring PCIE doorbell address
 * @is_evt_rn_db_pcie_addr: event ring PCIE doorbell address
 */
struct ipa_wdi_ul_params {
	phys_addr_t rdy_ring_base_pa;
	u32 rdy_ring_size;
	phys_addr_t rdy_ring_rp_pa;
	phys_addr_t rdy_comp_ring_base_pa;
	phys_addr_t rdy_comp_ring_wp_pa;
	u32 rdy_comp_ring_size;
	u32 *rdy_ring_rp_va;
	u32 *rdy_comp_ring_wp_va;
	bool is_txr_rn_db_pcie_addr;
	bool is_evt_rn_db_pcie_addr;
};

/**
 * struct  ipa_wdi_ul_params_smmu - WDI_RX configuration (with WLAN SMMU)
 * @rdy_ring: SG table describing the Rx ring (containing Rx buffers)
 * @rdy_ring_size: size of the Rx ring in bytes
 * @rdy_ring_rp_pa: physical address of the location through which IPA uc is
 * expected to communicate about the Read pointer into the Rx Ring
 * @is_txr_rn_db_pcie_addr: tx ring PCIE doorbell address
 * @is_evt_rn_db_pcie_addr: event ring PCIE doorbell address
 */
struct ipa_wdi_ul_params_smmu {
	struct sg_table rdy_ring;
	u32 rdy_ring_size;
	phys_addr_t rdy_ring_rp_pa;
	struct sg_table rdy_comp_ring;
	phys_addr_t rdy_comp_ring_wp_pa;
	u32 rdy_comp_ring_size;
	u32 *rdy_ring_rp_va;
	u32 *rdy_comp_ring_wp_va;
	bool is_txr_rn_db_pcie_addr;
	bool is_evt_rn_db_pcie_addr;
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
 * @is_txr_rn_db_pcie_addr: tx ring PCIE doorbell address
 * @is_evt_rn_db_pcie_addr: event ring PCIE doorbell address
 */
struct ipa_wdi_dl_params {
	phys_addr_t comp_ring_base_pa;
	u32 comp_ring_size;
	phys_addr_t ce_ring_base_pa;
	phys_addr_t ce_door_bell_pa;
	u32 ce_ring_size;
	u32 num_tx_buffers;
	bool is_txr_rn_db_pcie_addr;
	bool is_evt_rn_db_pcie_addr;
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
 * @is_txr_rn_db_pcie_addr: tx ring PCIE doorbell address
 * @is_evt_rn_db_pcie_addr: event ring PCIE doorbell address
 */
struct ipa_wdi_dl_params_smmu {
	struct sg_table comp_ring;
	u32 comp_ring_size;
	struct sg_table ce_ring;
	phys_addr_t ce_door_bell_pa;
	u32 ce_ring_size;
	u32 num_tx_buffers;
	bool is_txr_rn_db_pcie_addr;
	bool is_evt_rn_db_pcie_addr;
};

/**
 * struct  ipa_wdi_in_params - information provided by WDI client
 * @sys: IPA EP configuration info
 * @ul: WDI_RX configuration info
 * @dl: WDI_TX configuration info
 * @ul_smmu: WDI_RX configuration info when WLAN uses SMMU
 * @dl_smmu: WDI_TX configuration info when WLAN uses SMMU
 * @smmu_enabled: true if WLAN uses SMMU
 * @ipa_wdi_meter_notifier_cb: Get WDI stats and quato info
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
#ifdef IPA_WAN_MSG_IPv6_ADDR_GW_LEN
	ipa_wdi_meter_notifier_cb wdi_notify;
#endif
};

enum ipa_upstream_type {
	IPA_UPSTEAM_MODEM = 1,
	IPA_UPSTEAM_WLAN,
	IPA_UPSTEAM_MAX
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
 * struct  ipa_wdi_bw_info - address info of a WLAN allocated buffer
 * @threshold: throughput wants to be monitored
 * @num: number of threshold entries
 * @stop: true to stop monitoring
 *
 * IPA driver will create/release IOMMU mapping in IPA SMMU from iova->pa
 */
struct ipa_wdi_bw_info {
	uint64_t threshold[IPA_BW_THRESHOLD_MAX];
	int num;
	bool stop;
};

/**
 * struct  ipa_wdi_tx_info - sw tx info from WLAN
 * @sta_tx: sw tx stats on sta interface
 * @ap_tx: sw tx stats on ap interface
 *
 * IPA driver will create/release IOMMU mapping in IPA SMMU from iova->pa
 */
struct ipa_wdi_tx_info {
	uint64_t sta_tx;
	uint64_t ap_tx;
};

/**
 * struct ipa_gsi_ep_config - IPA GSI endpoint configurations
 *
 * @ipa_ep_num: IPA EP pipe number
 * @ipa_gsi_chan_num: GSI channel number
 * @ipa_if_tlv: number of IPA_IF TLV
 * @ipa_if_aos: number of IPA_IF AOS
 * @ee: Execution environment
 * @prefetch_mode: Prefetch mode to be used
 * @prefetch_threshold: Prefetch empty level threshold.
 *  relevant for smart and free prefetch modes
 */
struct ipa_gsi_ep_config {
	int ipa_ep_num;
	int ipa_gsi_chan_num;
	int ipa_if_tlv;
	int ipa_if_aos;
	int ee;
	enum gsi_prefetch_mode prefetch_mode;
	uint8_t prefetch_threshold;
};

/**
 * struct  ipa_smmu_in_params - information provided from client
 * @ipa_smmu_client_type: clinet requesting for the smmu info.
 */

enum ipa_smmu_client_type {
	IPA_SMMU_WLAN_CLIENT,
	IPA_SMMU_AP_CLIENT,
	IPA_SMMU_WIGIG_CLIENT,
	IPA_SMMU_WLAN1_CLIENT,
	IPA_SMMU_ETH_CLIENT,
	IPA_SMMU_ETH1_CLIENT,
	IPA_SMMU_CLIENT_MAX
};

struct ipa_smmu_in_params {
	enum ipa_smmu_client_type smmu_client;
};

/**
 * struct  ipa_smmu_out_params - information provided to IPA client
 * @smmu_enable: IPA S1 SMMU enable/disable status
 * @shared_cb: is client CB shared (mappings should be done by client only)
 */
struct ipa_smmu_out_params {
	bool smmu_enable;
	bool shared_cb;
};

struct iphdr_rsv {
	struct iphdr ipv4_temp;  /* 20 bytes */
	uint32_t rsv1;
	uint32_t rsv2;
	uint32_t rsv3;
	uint32_t rsv4;
	uint32_t rsv5;
} __packed;

union ip_hdr_temp {
	struct iphdr_rsv ipv4_rsv;	/* 40 bytes */
	struct ipv6hdr ipv6_temp;	/* 40 bytes */
} __packed;

struct ipa_socksv5_uc_tmpl {
	uint16_t cmd_id;
	uint16_t rsv;
	uint32_t cmd_param;
	uint16_t pkt_count;
	uint16_t rsv2;
	uint32_t byte_count;
	union ip_hdr_temp ip_hdr;
	/* 2B src/dst port */
	uint16_t src_port;
	uint16_t dst_port;

	/* attribute mask */
	uint32_t ipa_sockv5_mask;

	/* reqquired update 4B/4B Seq/Ack/SACK */
	uint32_t out_irs;
	uint32_t out_iss;
	uint32_t in_irs;
	uint32_t in_iss;

	/* option 10B: time-stamp */
	uint32_t out_ircv_tsval;
	uint32_t in_ircv_tsecr;
	uint32_t out_ircv_tsecr;
	uint32_t in_ircv_tsval;

	/* option 2B: window-scaling/dynamic */
	uint16_t in_isnd_wscale:4;
	uint16_t out_isnd_wscale:4;
	uint16_t in_ircv_wscale:4;
	uint16_t out_ircv_wscale:4;
	uint16_t MAX_WINDOW_SIZE;
	/* 11*4 + 40 bytes = 84 bytes */
	uint32_t rsv3;
	uint32_t rsv4;
	uint32_t rsv5;
	uint32_t rsv6;
	uint32_t rsv7;
	uint32_t rsv8;
	uint32_t rsv9;
} __packed;
/*reserve 16 bytes : 16 bytes+ 40 bytes + 44 bytes = 100 bytes (28 bytes left)*/

struct ipa_socksv5_info {
	/* ipa-uc info */
	struct ipa_socksv5_uc_tmpl ul_out;
	struct ipa_socksv5_uc_tmpl dl_out;

	/* ipacm info */
	struct ipacm_socksv5_info ul_in;
	struct ipacm_socksv5_info dl_in;

	/* output: handle (index) */
	uint16_t handle;
};

struct ipa_ipv6_nat_uc_tmpl {
	uint16_t cmd_id;
	uint16_t rsv;
	uint32_t cmd_param;
	uint16_t pkt_count;
	uint16_t rsv2;
	uint32_t byte_count;
	uint64_t private_address_lsb;
	uint64_t private_address_msb;
	uint64_t public_address_lsb;
	uint64_t public_address_msb;
	uint16_t private_port;
	uint16_t public_port;
	uint32_t rsv3;
	uint64_t rsv4;
	uint64_t rsv5;
	uint64_t rsv6;
	uint64_t rsv7;
	uint64_t rsv8;
	uint64_t rsv9;
	uint64_t rsv10;
	uint64_t rsv11;
	uint64_t rsv12;
} __packed;

#if IS_ENABLED(CONFIG_IPA3)
/*
 * Configuration
 */

/**
 * ipa_cfg_ep_ctrl() -  IPA end-point Control configuration
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 * @ipa_ep_cfg_ctrl:	[in] IPA end-point configuration params
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl);

/*
 * Routing
 */

/**
 * ipa_add_rt_rule() - Add the specified routing rules to SW and optionally
 * commit to IPA HW
 * @rules:	[inout] set of routing rules to add
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_add_rt_rule(struct ipa_ioc_add_rt_rule *rules);

/**
 * ipa_put_rt_tbl() - Release the specified routing table handle
 * @rt_tbl_hdl:	[in] the routing table handle to release
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_put_rt_tbl(u32 rt_tbl_hdl);

/*
 * Interface
 */
int ipa_register_intf(const char *name,
	const struct ipa_tx_intf *tx,
	const struct ipa_rx_intf *rx);
int ipa_deregister_intf(const char *name);

/*
 * Aggregation
 */

/**
 * ipa_set_aggr_mode() - Set the aggregation mode which is a global setting
 * @mode:	[in] the desired aggregation mode for e.g. straight MBIM, QCNCM,
 * etc
 *
 * Returns:	0 on success
 */

int ipa_set_aggr_mode(enum ipa_aggr_mode mode);

/**
 * ipa_set_qcncm_ndp_sig() - Set the NDP signature used for QCNCM aggregation
 * mode
 * @sig:	[in] the first 3 bytes of QCNCM NDP signature (expected to be
 * "QND")
 *
 * Set the NDP signature used for QCNCM aggregation mode. The fourth byte
 * (expected to be 'P') needs to be set using the header addition mechanism
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_set_qcncm_ndp_sig(char sig[3]);

/**
 * ipa_set_single_ndp_per_mbim() - Enable/disable single NDP per MBIM frame
 * configuration
 * @enable:	[in] true for single NDP/MBIM; false otherwise
 *
 * Returns:	0 on success
 */
int ipa_set_single_ndp_per_mbim(bool enable);

/*
 * interrupts
 */

/**
 * ipa_add_interrupt_handler() - Adds handler to an interrupt type
 * @interrupt:		Interrupt type
 * @handler:		The handler to be added
 * @deferred_flag:	whether the handler processing should be deferred in
 *			a workqueue
 * @private_data:	the client's private data
 *
 * Adds handler to an interrupt type and enable the specific bit
 * in IRQ_EN register, associated interrupt in IRQ_STTS register will be enabled
 */

int ipa_add_interrupt_handler(enum ipa_irq_type interrupt,
	ipa_irq_handler_t handler,
	bool deferred_flag,
	void *private_data);

/**
 * ipa_restore_suspend_handler() - restores the original suspend IRQ handler
 * as it was registered in the IPA init sequence.
 * Return codes:
 * 0: success
 * -EPERM: failed to remove current handler or failed to add original handler
 */
int ipa_restore_suspend_handler(void);

/*
 * Messaging
 */

/**
 * ipa_send_msg() - Send "message" from kernel client to IPA driver
 * @meta: [in] message meta-data
 * @buff: [in] the payload for message
 * @callback: [in] free callback
 *
 * Client supplies the message meta-data and payload which IPA driver buffers
 * till read by user-space. After read from user space IPA driver invokes the
 * callback supplied to free the message payload. Client must not touch/free
 * the message payload after calling this API.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback);

/*
 * Data path
 */

/**
 * ipa_tx_dp() - Data-path tx handler
 * @dst:	[in] which IPA destination to route tx packets to
 * @skb:	[in] the packet to send
 * @metadata:	[in] TX packet meta-data
 *
 * Data-path tx handler, this is used for both SW data-path which by-passes most
 * IPA HW blocks AND the regular HW data-path for WLAN AMPDU traffic only. If
 * dst is a "valid" CONS type, then SW data-path is used. If dst is the
 * WLAN_AMPDU PROD type, then HW data-path for WLAN AMPDU is used. Anything else
 * is an error. For errors, client needs to free the skb as needed. For success,
 * IPA driver will later invoke client callback if one was supplied. That
 * callback should free the skb. If no callback supplied, IPA driver will free
 * the skb internally
 *
 * The function will use two descriptors for this send command
 * (for A5_WLAN_AMPDU_PROD only one desciprtor will be sent),
 * the first descriptor will be used to inform the IPA hardware that
 * apps need to push data into the IPA (IP_PACKET_INIT immediate command).
 * Once this send was done from SPS point-of-view the IPA driver will
 * get notified by the supplied callback - ipa_sps_irq_tx_comp()
 *
 * ipa_sps_irq_tx_comp will call to the user supplied
 * callback (from ipa_connect)
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

/*
 * ipa_rmnet_ctl_xmit - QMAP Flow control TX
 *
 * @skb - tx QMAP control packet
 *
 * Note: This need to be called after client receive rmnet_ctl_
 * ready_cb and want to send TX flow control message.
 *
 * This funciton will return 0 on success, -EAGAIN if pipe if full.
 */
int ipa_rmnet_ctl_xmit(struct sk_buff *skb);

/*
 * ipa_rmnet_ll_xmit - Low lat data Tx
 *
 * @skb - tx low lat data packet
 *
 * Note: This need to be called after client receive rmnet_ll_
 * ready_cb and want to send TX ll data message.
 *
 * This funciton will return 0 on success, -EAGAIN if pipe if full.
 */
int ipa_rmnet_ll_xmit(struct sk_buff *skb);

/*
 * ipa_register_notifier - Register for IPA atomic notifier
 *
 * @fn_ptr - Function pointer to get the notification
 *
 * This funciton will return 0 on success, -EAGAIN if reg fails.
 */
int ipa_register_notifier(void *fn_ptr);

/*
 * ipa_unregister_notifier - Unregister for IPA atomic notifier
 *
 * @fn_ptr - Function pointer to get the notification
 *
 * This funciton will return 0 on success, -EAGAIN if reg fails.
 */
int ipa_unregister_notifier(void *fn_ptr);

void ipa_free_skb(struct ipa_rx_data *data);

/*
 * System pipes
 */

/**
 * ipa_setup_sys_pipe() - Setup an IPA end-point in system-BAM mode and perform
 * IPA EP configuration
 * @sys_in:	[in] input needed to setup BAM pipe and configure EP
 * @clnt_hdl:	[out] client handle
 *
 *  - configure the end-point registers with the supplied
 *    parameters from the user.
 *  - call SPS APIs to create a system-to-bam connection with IPA.
 *  - allocate descriptor FIFO
 *  - register callback function(ipa_sps_irq_rx_notify or
 *    ipa_sps_irq_tx_notify - depends on client type) in case the driver is
 *    not configured to pulling mode
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl);

/**
 * ipa_teardown_sys_pipe() - Teardown the system-BAM pipe and cleanup IPA EP
 * @clnt_hdl:	[in] the handle obtained from ipa_setup_sys_pipe
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_teardown_sys_pipe(u32 clnt_hdl);

int ipa_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out);
int ipa_disconnect_wdi_pipe(u32 clnt_hdl);
int ipa_enable_wdi_pipe(u32 clnt_hdl);
int ipa_disable_wdi_pipe(u32 clnt_hdl);
int ipa_resume_wdi_pipe(u32 clnt_hdl);
int ipa_suspend_wdi_pipe(u32 clnt_hdl);
int ipa_reg_uc_rdyCB(struct ipa_wdi_uc_ready_params *param);
int ipa_dereg_uc_rdyCB(void);
int ipa_add_hdr(struct ipa_ioc_add_hdr *hdrs);
int ipa_del_hdr(struct ipa_ioc_del_hdr *hdls);
int ipa_get_hdr(struct ipa_ioc_get_hdr *lookup);
/**
 * ipa_get_wdi_stats() - Query WDI statistics from uc
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats);
int ipa_uc_bw_monitor(struct ipa_wdi_bw_info *info);

/**
 * ipa_broadcast_wdi_quota_reach_ind() - quota reach
 * @uint32_t fid: [in] input netdev ID
 * @uint64_t num_bytes: [in] used bytes
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_broadcast_wdi_quota_reach_ind(uint32_t fid,
		uint64_t num_bytes);

/*
 * To retrieve doorbell physical address of
 * wlan pipes
 */
int ipa_uc_wdi_get_dbpa(struct ipa_wdi_db_params *out);

/*
 * IPADMA
 */
 /**
  * ipa_dma_init() -Initialize IPADMA.
  *
  * This function initialize all IPADMA internal data and connect in dma:
  *	MEMCPY_DMA_SYNC_PROD ->MEMCPY_DMA_SYNC_CONS
  *	MEMCPY_DMA_ASYNC_PROD->MEMCPY_DMA_SYNC_CONS
  *
  * Return codes: 0: success
  *		-EFAULT: IPADMA is already initialized
  *		-ENOMEM: allocating memory error
  *		-EPERM: pipe connection failed
  */
int ipa_dma_init(void);

/**
 * ipa_dma_enable() -Vote for IPA clocks.
 *
 *Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 *		-EPERM: Operation not permitted as ipa_dma is already
 *		 enabled
 */
int ipa_dma_enable(void);


/**
 * ipa_dma_disable()- Unvote for IPA clocks.
 *
 * enter to power save mode.
 *
 * Return codes: 0: success
 *		-EINVAL: IPADMA is not initialized
 *		-EPERM: Operation not permitted as ipa_dma is already
 *			diabled
 *		-EFAULT: can not disable ipa_dma as there are pending
 *			memcopy works
 */
int ipa_dma_disable(void);

/**
 * ipa_dma_sync_memcpy()- Perform synchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-SPS_ERROR: on sps faliures
 *		-EFAULT: other
 */
int ipa_dma_sync_memcpy(u64 dest, u64 src, int len);

/**
 * ipa_dma_async_memcpy()- Perform asynchronous memcpy using IPA.
 *
 * @dest: physical address to store the copied data.
 * @src: physical address of the source data to copy.
 * @len: number of bytes to copy.
 * @user_cb: callback function to notify the client when the copy was done.
 * @user_param: cookie for user_cb.
 *
 * Return codes: 0: success
 *		-EINVAL: invalid params
 *		-EPERM: operation not permitted as ipa_dma isn't enable or
 *			initialized
 *		-SPS_ERROR: on sps faliures
 *		-EFAULT: descr fifo is full.
 */
int ipa_dma_async_memcpy(u64 dest, u64 src, int len,
			void (*user_cb)(void *user1), void *user_param);


/**
 * ipa_dma_destroy() -teardown IPADMA pipes and release ipadma.
 *
 * this is a blocking function, returns just after destroying IPADMA.
 */
void ipa_dma_destroy(void);

/*
 * Miscellaneous
 */

int ipa_get_ep_mapping(enum ipa_client_type client);

bool ipa_is_ready(void);

enum ipa_hw_type ipa_get_hw_type(void);

const struct ipa_gsi_ep_config *ipa_get_gsi_ep_info(
	enum ipa_client_type client);

int ipa_stop_gsi_channel(u32 clnt_hdl);

typedef void (*ipa_ready_cb)(void *user_data);

typedef void (*ipa_rmnet_ctl_ready_cb)(void *user_data);

typedef void (*ipa_rmnet_ctl_stop_cb)(void *user_data);

typedef void (*ipa_rmnet_ctl_rx_notify_cb)(void *user_data, void *rx_data);

int ipa_get_default_aggr_time_limit(enum ipa_client_type client,
	u32 *default_aggr_time_limit);

typedef void (*ipa_rmnet_ll_ready_cb)(void *user_data);

typedef void (*ipa_rmnet_ll_stop_cb)(void *user_data);

typedef void (*ipa_rmnet_ll_rx_notify_cb)(void *user_data, void *rx_data);

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

/**
 * ipa_register_rmnet_ctl_cb() - register callbacks to be invoked
 * to rmnet_ctl for qmap flow control pipes setup/teardown/rx_notify.
 *
 * @ipa_rmnet_ctl_ready_cb:  CB to be called when pipes setup.
 * @user_data1: user_data for ipa_rmnet_ctl_ready_cb.
 * @ipa_rmnet_ctl_stop_cb: CB to be called when pipes teardown.
 * @user_data2: user_data for ipa_rmnet_ctl_stop_cb.
 * @ipa_rmnet_ctl_rx_notify_cb: CB to be called when receive rx pkts.
 * @user_data3: user_data for ipa_rmnet_ctl_rx_notify_cb.
 * @rx_data: RX data buffer.
 *
 * Note: This function is expected to be utilized for rmnet_ctl
 * module when new qmap flow control is enabled.
 *
 * The function will return 0 on success, -EAGAIN if IPA not ready,
 * -ENXIO is feature is not enabled, -EEXIST if already called.
 */
int ipa_register_rmnet_ctl_cb(
	void (*ipa_rmnet_ctl_ready_cb)(void *user_data1),
	void *user_data1,
	void (*ipa_rmnet_ctl_stop_cb)(void *user_data2),
	void *user_data2,
	void (*ipa_rmnet_ctl_rx_notify_cb)(void *user_data3, void *rx_data),
	void *user_data3);

/**
 * ipa_unregister_rmnet_ctl_cb() - unregister callbacks to be
 * invoked to rmnet_ctl for qmap flow control pipes
 * setup/teardown/rx_notify.
 *
 * Note: This function is expected to be utilized for rmnet_ctl
 * module when new qmap flow control is enabled.
 *
 * The function will return 0 on success, -EAGAIN if IPA not ready,
 * -ENXIO is feature is not enabled.
 */
int ipa_unregister_rmnet_ctl_cb(void);

/**
 * ipa_register_rmnet_ll_cb() - register callbacks to be invoked
 * to rmnet_ll for low latency data pipes setup/teardown/rx_notify.
 *
 * @ipa_rmnet_ll_ready_cb:  CB to be called when pipes setup.
 * @user_data1: user_data for ipa_rmnet_ctl_ready_cb.
 * @ipa_rmnet_ll_stop_cb: CB to be called when pipes teardown.
 * @user_data2: user_data for ipa_rmnet_ctl_stop_cb.
 * @ipa_rmnet_ll_rx_notify_cb: CB to be called when receive rx pkts.
 * @user_data3: user_data for ipa_rmnet_ctl_rx_notify_cb.
 * @rx_data: RX data buffer.
 *
 * Note: This function is expected to be utilized for rmnet_ll
 * module.
 *
 * The function will return 0 on success, -EAGAIN if IPA not ready,
 * -ENXIO is feature is not enabled, -EEXIST if already called.
 */
int ipa_register_rmnet_ll_cb(
	void (*ipa_rmnet_ll_ready_cb)(void *user_data1),
	void *user_data1,
	void (*ipa_rmnet_ll_stop_cb)(void *user_data2),
	void *user_data2,
	void (*ipa_rmnet_ll_rx_notify_cb)(void *user_data3, void *rx_data),
	void *user_data3);

/**
 * ipa_unregister_rmnet_ll_cb() - unregister callbacks to be
 * invoked to rmnet_ll for low lat data pipes
 * setup/teardown/rx_notify.
 *
 * Note: This function is expected to be utilized for rmnet_ll
 * module.
 *
 * The function will return 0 on success, -EAGAIN if IPA not ready,
 * -ENXIO is feature is not enabled.
 */
int ipa_unregister_rmnet_ll_cb(void);

int ipa_get_smmu_params(struct ipa_smmu_in_params *in,
	struct ipa_smmu_out_params *out);
/**
 * ipa_is_vlan_mode - check if a LAN driver should load in VLAN mode
 * @iface - type of vlan capable device
 * @res - query result: true for vlan mode, false for non vlan mode
 *
 * API must be called after ipa_is_ready() returns true, otherwise it will fail
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_is_vlan_mode(enum ipa_vlan_ifaces iface, bool *res);

/**
 * ipa_get_lan_rx_napi - returns true if NAPI is enabled in the LAN RX dp
 */
bool ipa_get_lan_rx_napi(void);
/*
 * ipa_add_socksv5_conn - add socksv5 info to ipa driver
 */
int ipa_add_socksv5_conn(struct ipa_socksv5_info *info);

/*
 * ipa_del_socksv5_conn - del socksv5 info to ipa driver
 */
int ipa_del_socksv5_conn(uint32_t handle);

int ipa_mhi_handle_ipa_config_req(struct ipa_config_req_msg_v01 *config_req);
int ipa_wigig_save_regs(void);

/*
 * this function needs to be removed, but wlan driver is checking return value
 * to see if IPA is present, so we can't return -EPERM
 */
static inline int ipa_uc_reg_rdyCB(
	struct ipa_wdi_uc_ready_params *inout)
{
	return -EFAULT;
}

#else /* IS_ENABLED(CONFIG_IPA3) */

/*
 * Configuration
 */
static inline int ipa_cfg_ep_ctrl(u32 clnt_hdl,
	const struct ipa_ep_cfg_ctrl *ep_ctrl)
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

static inline int ipa_put_rt_tbl(u32 rt_tbl_hdl)
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
 * interrupts
 */
static inline int ipa_add_interrupt_handler(enum ipa_irq_type interrupt,
	ipa_irq_handler_t handler,
	bool deferred_flag,
	void *private_data)
{
	return -EPERM;
}

static inline int ipa_restore_suspend_handler(void)
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

/*
 * Data path
 */
static inline int ipa_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata)
{
	return -EPERM;
}

/*
 * QMAP Flow control TX
 */
static inline int ipa_rmnet_ctl_xmit(struct sk_buff *skb)
{
	return -EPERM;
}

/*
 * Low Latency data Tx
 */
static inline int ipa_rmnet_ll_xmit(struct sk_buff *skb)
{
	return -EPERM;
}

/*
 * Rmnet Notifier register
 */
static inline int ipa_register_notifier(struct sk_buff *skb)
{
	return -EPERM;
}

/*
 * Rmnet Notifier unregister
 */
static inline int ipa_unregister_notifier(struct sk_buff *skb)
{
	return -EPERM;
}

static inline void ipa_free_skb(struct ipa_rx_data *rx_in)
{
}

/*
 * System pipes
 */

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

static inline int ipa_broadcast_wdi_quota_reach_ind(uint32_t fid,
		uint64_t num_bytes)
{
	return -EPERM;
}

static inline int ipa_uc_wdi_get_dbpa(
	struct ipa_wdi_db_params *out)
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

static inline void ipa_dma_destroy(void)
{
}

/*
 * Miscellaneous
 */
static inline int ipa_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats)
{
	return -EPERM;
}

static inline int ipa_uc_bw_monitor(struct ipa_wdi_bw_info *info)
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

static inline enum ipa_hw_type ipa_get_hw_type(void)
{
	return IPA_HW_None;
}

static inline int ipa_register_ipa_ready_cb(
	void (*ipa_ready_cb)(void *user_data),
	void *user_data)
{
	return -EPERM;
}

static inline int ipa_get_smmu_params(struct ipa_smmu_in_params *in,
	struct ipa_smmu_out_params *out)
{
	return -EPERM;
}

static inline int ipa_is_vlan_mode(enum ipa_vlan_ifaces iface, bool *res)
{
	return -EPERM;
}

static inline bool ipa_get_lan_rx_napi(void)
{
	return false;
}

static inline int ipa_add_socksv5_conn(struct ipa_socksv5_info *info)
{
	return -EPERM;
}

static inline int ipa_del_socksv5_conn(uint32_t handle)
{
	return -EPERM;
}

static inline const struct ipa_gsi_ep_config *ipa_get_gsi_ep_info(
	enum ipa_client_type client)
{
	return NULL;
}

static inline int ipa_stop_gsi_channel(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_register_rmnet_ctl_cb(
	void (*ipa_rmnet_ctl_ready_cb)(void *user_data1),
	void *user_data1,
	void (*ipa_rmnet_ctl_stop_cb)(void *user_data2),
	void *user_data2,
	void (*ipa_rmnet_ctl_rx_notify_cb)(void *user_data3, void *rx_data),
	void *user_data3)
{
	return -EPERM;
}

static inline int ipa_unregister_rmnet_ctl_cb(void)
{
	return -EPERM;
}

static inline int ipa_uc_reg_rdyCB(
	struct ipa_wdi_uc_ready_params *inout)
{
	return -EPERM;
}

static inline int ipa_get_default_aggr_time_limit(enum ipa_client_type client,
	u32 *default_aggr_time_limit)
{
	return -EPERM;
}

static inline int ipa_register_rmnet_ll_cb(
	void (*ipa_rmnet_ll_ready_cb)(void *user_data1),
	void *user_data1,
	void (*ipa_rmnet_ll_stop_cb)(void *user_data2),
	void *user_data2,
	void (*ipa_rmnet_ll_rx_notify_cb)(void *user_data3, void *rx_data),
	void *user_data3)
{
	return -EPERM;
}

static inline int ipa_unregister_rmnet_ll_cb(void)
{
	return -EPERM;
}

#endif /* IS_ENABLED(CONFIG_IPA3) */

/* stubs - to be removed once dependent drivers remove references */
static inline int ipa_reset_endpoint(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_clear_endpoint_delay(u32 clnt_hdl)
{
	return -EPERM;
}

static inline int ipa_commit_hdr(void)
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

static inline int ipa_register_pull_msg(struct ipa_msg_meta *meta,
	ipa_msg_pull_fn callback)
{
	return -EPERM;
}

static inline int ipa_deregister_pull_msg(struct ipa_msg_meta *meta)
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

static inline int ipa_tx_dp_mul(enum ipa_client_type src,
	struct ipa_tx_data_desc *data_desc)
{
	return -EPERM;
}

static inline u16 ipa_get_smem_restr_bytes(void)
{
	return -EPERM;
}

static inline int ipa_create_wdi_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

static inline int ipa_release_wdi_mapping(u32 num_buffers,
	struct ipa_wdi_buffer_info *info)
{
	return -EPERM;
}

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

static inline int ipa_rm_deregister(
	enum ipa_rm_resource_name resource_name,
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

static inline int ipa_rm_inactivity_timer_init(
	enum ipa_rm_resource_name resource_name,
	unsigned long msecs)
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

static inline enum ipa_rm_resource_name ipa_get_rm_resource_from_ep(
	int pipe_idx)
{
	return -EPERM;
}

static inline void ipa_bam_reg_dump(void)
{
}

static inline void ipa_proxy_clk_vote(void)
{
}

static inline void ipa_proxy_clk_unvote(void)
{
}

static inline bool ipa_is_client_handle_valid(u32 clnt_hdl)
{
	return false;
}

static inline enum ipa_client_type ipa_get_client_mapping(int pipe_idx)
{
	return -EPERM;
}

static inline bool ipa_get_modem_cfg_emb_pipe_flt(void)
{
	return false;
}

static inline enum ipa_transport_type ipa_get_transport_type(void)
{
	return IPA_TRANSPORT_TYPE_GSI;
}

static inline struct device *ipa_get_dma_dev(void)
{
	return NULL;
}

static inline struct iommu_domain *ipa_get_smmu_domain(void)
{
	return NULL;
}

static inline int ipa_disable_apps_wan_cons_deaggr(
	uint32_t agg_size, uint32_t agg_count)
{
	return -EPERM;
}

static inline int ipa_uc_dereg_rdyCB(void)
{
	return -EPERM;
}

#endif /* _IPA_H_ */

