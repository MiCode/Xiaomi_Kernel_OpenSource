/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2015, 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_HW_DEFS_H
#define _IPA_HW_DEFS_H
#include <linux/bitops.h>

/* This header defines various HW related data types */

/* immediate command op-codes */
#define IPA_DECIPH_INIT           (1)
#define IPA_PPP_FRM_INIT          (2)
#define IPA_IP_V4_FILTER_INIT     (3)
#define IPA_IP_V6_FILTER_INIT     (4)
#define IPA_IP_V4_NAT_INIT        (5)
#define IPA_IP_V6_NAT_INIT        (6)
#define IPA_IP_V4_ROUTING_INIT    (7)
#define IPA_IP_V6_ROUTING_INIT    (8)
#define IPA_HDR_INIT_LOCAL        (9)
#define IPA_HDR_INIT_SYSTEM      (10)
#define IPA_DECIPH_SETUP         (11)
#define IPA_REGISTER_WRITE       (12)
#define IPA_NAT_DMA              (14)
#define IPA_IP_PACKET_TAG        (15)
#define IPA_IP_PACKET_INIT       (16)
#define IPA_DMA_SHARED_MEM       (19)
#define IPA_IP_PACKET_TAG_STATUS (20)

/* Processing context TLV type */
#define IPA_PROC_CTX_TLV_TYPE_END 0
#define IPA_PROC_CTX_TLV_TYPE_HDR_ADD 1
#define IPA_PROC_CTX_TLV_TYPE_PROC_CMD 3


/**
 * struct ipa_flt_rule_hw_hdr - HW header of IPA filter rule
 * @word: filtering rule properties
 * @en_rule: enable rule
 * @action: post routing action
 * @rt_tbl_idx: index in routing table
 * @retain_hdr: added to add back to the packet the header removed
 *  as part of header removal. This will be done as part of
 *  header insertion block.
 * @to_uc: direct IPA to sent the packet to uc instead of
 *  the intended destination. This will be performed just after
 *  routing block processing, so routing will have determined
 *  destination end point and uc will receive this information
 *  together with the packet as part of the HW packet TX commands
 * @rsvd: reserved bits
 */
struct ipa_flt_rule_hw_hdr {
	union {
		u32 word;
		struct {
			u32 en_rule:16;
			u32 action:5;
			u32 rt_tbl_idx:5;
			u32 retain_hdr:1;
			u32 to_uc:1;
			u32 rsvd:4;
		} hdr;
	} u;
};

/**
 * struct ipa_rt_rule_hw_hdr - HW header of IPA routing rule
 * @word: filtering rule properties
 * @en_rule: enable rule
 * @pipe_dest_idx: destination pipe index
 * @system: changed from local to system due to HW change
 * @hdr_offset: header offset
 * @proc_ctx: whether hdr_offset points to header table or to
 *	header processing context table
 */
struct ipa_rt_rule_hw_hdr {
	union {
		u32 word;
		struct {
			u32 en_rule:16;
			u32 pipe_dest_idx:5;
			u32 system:1;
			u32 hdr_offset:10;
		} hdr;
		struct {
			u32 en_rule:16;
			u32 pipe_dest_idx:5;
			u32 system:1;
			u32 hdr_offset:9;
			u32 proc_ctx:1;
		} hdr_v2_5;
	} u;
};

/**
 * struct ipa_ip_v4_filter_init - IPA_IP_V4_FILTER_INIT command payload
 * @ipv4_rules_addr: address of ipv4 rules
 * @size_ipv4_rules: size of the above
 * @ipv4_addr: ipv4 address
 * @rsvd: reserved
 */
struct ipa_ip_v4_filter_init {
	u64 ipv4_rules_addr:32;
	u64 size_ipv4_rules:12;
	u64 ipv4_addr:16;
	u64 rsvd:4;
};

/**
 * struct ipa_ip_v6_filter_init - IPA_IP_V6_FILTER_INIT command payload
 * @ipv6_rules_addr: address of ipv6 rules
 * @size_ipv6_rules: size of the above
 * @ipv6_addr: ipv6 address
 */
struct ipa_ip_v6_filter_init {
	u64 ipv6_rules_addr:32;
	u64 size_ipv6_rules:16;
	u64 ipv6_addr:16;
};

/**
 * struct ipa_ip_v4_routing_init - IPA_IP_V4_ROUTING_INIT command payload
 * @ipv4_rules_addr: address of ipv4 rules
 * @size_ipv4_rules: size of the above
 * @ipv4_addr: ipv4 address
 * @rsvd: reserved
 */
struct ipa_ip_v4_routing_init {
	u64 ipv4_rules_addr:32;
	u64 size_ipv4_rules:12;
	u64 ipv4_addr:16;
	u64 rsvd:4;
};

/**
 * struct ipa_ip_v6_routing_init - IPA_IP_V6_ROUTING_INIT command payload
 * @ipv6_rules_addr: address of ipv6 rules
 * @size_ipv6_rules: size of the above
 * @ipv6_addr: ipv6 address
 */
struct ipa_ip_v6_routing_init {
	u64 ipv6_rules_addr:32;
	u64 size_ipv6_rules:16;
	u64 ipv6_addr:16;
};

/**
 * struct ipa_hdr_init_local - IPA_HDR_INIT_LOCAL command payload
 * @hdr_table_src_addr: word address of header table in system memory where the
 *  table starts (use as source for memory copying)
 * @size_hdr_table: size of the above (in bytes)
 * @hdr_table_dst_addr: header address in IPA sram (used as dst for memory copy)
 * @rsvd: reserved
 */
struct ipa_hdr_init_local {
	u64 hdr_table_src_addr:32;
	u64 size_hdr_table:12;
	u64 hdr_table_dst_addr:16;
	u64 rsvd:4;
};

/**
 * struct ipa_hdr_init_system - IPA_HDR_INIT_SYSTEM command payload
 * @hdr_table_addr: word address of header table in system memory where the
 *  table starts (use as source for memory copying)
 * @rsvd: reserved
 */
struct ipa_hdr_init_system {
	u64 hdr_table_addr:32;
	u64 rsvd:32;
};

/**
 * struct ipa_hdr_proc_ctx_tlv -
 * HW structure of IPA processing context header - TLV part
 * @type: 0 - end type
 *        1 - header addition type
 *        3 - processing command type
 * @length: number of bytes after tlv
 *        for type:
 *        0 - needs to be 0
 *        1 - header addition length
 *        3 - number of 32B including type and length.
 * @value: specific value for type
 *        for type:
 *        0 - needs to be 0
 *        1 - header length
 *        3 - command ID (see IPA_HDR_UCP_* definitions)
 */
struct ipa_hdr_proc_ctx_tlv {
	u32 type:8;
	u32 length:8;
	u32 value:16;
};

/**
 * struct ipa_hdr_proc_ctx_hdr_add -
 * HW structure of IPA processing context - add header tlv
 * @tlv: IPA processing context TLV
 * @hdr_addr: processing context header address
 */
struct ipa_hdr_proc_ctx_hdr_add {
	struct ipa_hdr_proc_ctx_tlv tlv;
	u32 hdr_addr;
};

#define IPA_A5_MUX_HDR_EXCP_FLAG_IP		BIT(7)
#define IPA_A5_MUX_HDR_EXCP_FLAG_NAT		BIT(6)
#define IPA_A5_MUX_HDR_EXCP_FLAG_SW_FLT	BIT(5)
#define IPA_A5_MUX_HDR_EXCP_FLAG_TAG		BIT(4)
#define IPA_A5_MUX_HDR_EXCP_FLAG_REPLICATED	BIT(3)
#define IPA_A5_MUX_HDR_EXCP_FLAG_IHL		BIT(2)

/**
 * struct ipa_a5_mux_hdr - A5 MUX header definition
 * @interface_id: interface ID
 * @src_pipe_index: source pipe index
 * @flags: flags
 * @metadata: metadata
 *
 * A5 MUX header is in BE, A5 runs in LE. This struct definition
 * allows A5 SW to correctly parse the header
 */
struct ipa_a5_mux_hdr {
	u16 interface_id;
	u8 src_pipe_index;
	u8 flags;
	u32 metadata;
};

/**
 * struct ipa_register_write - IPA_REGISTER_WRITE command payload
 * @rsvd: reserved
 * @skip_pipeline_clear: 0 to wait until IPA pipeline is clear
 * @offset: offset from IPA base address
 * @value: value to write to register
 * @value_mask: mask specifying which value bits to write to the register
 */
struct ipa_register_write {
	u32 rsvd:15;
	u32 skip_pipeline_clear:1;
	u32 offset:16;
	u32 value:32;
	u32 value_mask:32;
};

/**
 * struct ipa_nat_dma - IPA_NAT_DMA command payload
 * @table_index: NAT table index
 * @rsvd1: reserved
 * @base_addr: base address
 * @rsvd2: reserved
 * @offset: offset
 * @data: metadata
 * @rsvd3: reserved
 */
struct ipa_nat_dma {
	u64 table_index:3;
	u64 rsvd1:1;
	u64 base_addr:2;
	u64 rsvd2:2;
	u64 offset:32;
	u64 data:16;
	u64 rsvd3:8;
};

/**
 * struct ipa_nat_dma - IPA_IP_PACKET_INIT command payload
 * @destination_pipe_index: destination pipe index
 * @rsvd1: reserved
 * @metadata: metadata
 * @rsvd2: reserved
 */
struct ipa_ip_packet_init {
	u64 destination_pipe_index:5;
	u64 rsvd1:3;
	u64 metadata:32;
	u64 rsvd2:24;
};

/**
 * struct ipa_nat_dma - IPA_IP_V4_NAT_INIT command payload
 * @ipv4_rules_addr: ipv4 rules address
 * @ipv4_expansion_rules_addr: ipv4 expansion rules address
 * @index_table_addr: index tables address
 * @index_table_expansion_addr: index expansion table address
 * @table_index: index in table
 * @ipv4_rules_addr_type: ipv4 address type
 * @ipv4_expansion_rules_addr_type: ipv4 expansion address type
 * @index_table_addr_type: index table address type
 * @index_table_expansion_addr_type: index expansion table type
 * @size_base_tables: size of base tables
 * @size_expansion_tables: size of expansion tables
 * @rsvd2: reserved
 * @public_ip_addr: public IP address
 */
struct ipa_ip_v4_nat_init {
	u64 ipv4_rules_addr:32;
	u64 ipv4_expansion_rules_addr:32;
	u64 index_table_addr:32;
	u64 index_table_expansion_addr:32;
	u64 table_index:3;
	u64 rsvd1:1;
	u64 ipv4_rules_addr_type:1;
	u64 ipv4_expansion_rules_addr_type:1;
	u64 index_table_addr_type:1;
	u64 index_table_expansion_addr_type:1;
	u64 size_base_tables:12;
	u64 size_expansion_tables:10;
	u64 rsvd2:2;
	u64 public_ip_addr:32;
};

/**
 * struct ipa_ip_packet_tag - IPA_IP_PACKET_TAG command payload
 * @tag: tag value returned with response
 */
struct ipa_ip_packet_tag {
	u32 tag;
};

/**
 * struct ipa_ip_packet_tag_status - IPA_IP_PACKET_TAG_STATUS command payload
 * @rsvd: reserved
 * @tag_f_1: tag value returned within status
 * @tag_f_2: tag value returned within status
 */
struct ipa_ip_packet_tag_status {
	u32 rsvd:16;
	u32 tag_f_1:16;
	u32 tag_f_2:32;
};

/*! @brief Struct for the IPAv2.0 and IPAv2.5 UL packet status header */
struct ipa_hw_pkt_status {
	u32 status_opcode:8;
	u32 exception:8;
	u32 status_mask:16;
	u32 pkt_len:16;
	u32 endp_src_idx:5;
	u32 reserved_1:3;
	u32 endp_dest_idx:5;
	u32 reserved_2:3;
	u32 metadata:32;
	union {
		struct {
			u32 filt_local:1;
			u32 filt_global:1;
			u32 filt_pipe_idx:5;
			u32 filt_match:1;
			u32 filt_rule_idx:6;
			u32 ret_hdr:1;
			u32 reserved_3:1;
			u32 tag_f_1:16;

		} ipa_hw_v2_0_pkt_status;
		struct {
			u32 filt_local:1;
			u32 filt_global:1;
			u32 filt_pipe_idx:5;
			u32 ret_hdr:1;
			u32 filt_rule_idx:8;
			u32 tag_f_1:16;

		} ipa_hw_v2_5_pkt_status;
	};

	u32 tag_f_2:32;
	u32 time_day_ctr:32;
	u32 nat_hit:1;
	u32 nat_tbl_idx:13;
	u32 nat_type:2;
	u32 route_local:1;
	u32 route_tbl_idx:5;
	u32 route_match:1;
	u32 ucp:1;
	u32 route_rule_idx:8;
	u32 hdr_local:1;
	u32 hdr_offset:10;
	u32 frag_hit:1;
	u32 frag_rule:4;
	u32 reserved_4:16;
};

#define IPA_PKT_STATUS_SIZE 32

/*! @brief Status header opcodes */
enum ipa_hw_status_opcode {
	IPA_HW_STATUS_OPCODE_MIN,
	IPA_HW_STATUS_OPCODE_PACKET = IPA_HW_STATUS_OPCODE_MIN,
	IPA_HW_STATUS_OPCODE_NEW_FRAG_RULE,
	IPA_HW_STATUS_OPCODE_DROPPED_PACKET,
	IPA_HW_STATUS_OPCODE_SUSPENDED_PACKET,
	IPA_HW_STATUS_OPCODE_XLAT_PACKET = 6,
	IPA_HW_STATUS_OPCODE_MAX
};

/*! @brief Possible Masks received in status */
enum ipa_hw_pkt_status_mask {
	IPA_HW_PKT_STATUS_MASK_FRAG_PROCESS      = 0x1,
	IPA_HW_PKT_STATUS_MASK_FILT_PROCESS      = 0x2,
	IPA_HW_PKT_STATUS_MASK_NAT_PROCESS       = 0x4,
	IPA_HW_PKT_STATUS_MASK_ROUTE_PROCESS     = 0x8,
	IPA_HW_PKT_STATUS_MASK_TAG_VALID         = 0x10,
	IPA_HW_PKT_STATUS_MASK_FRAGMENT          = 0x20,
	IPA_HW_PKT_STATUS_MASK_FIRST_FRAGMENT    = 0x40,
	IPA_HW_PKT_STATUS_MASK_V4                = 0x80,
	IPA_HW_PKT_STATUS_MASK_CKSUM_PROCESS     = 0x100,
	IPA_HW_PKT_STATUS_MASK_AGGR_PROCESS      = 0x200,
	IPA_HW_PKT_STATUS_MASK_DEST_EOT          = 0x400,
	IPA_HW_PKT_STATUS_MASK_DEAGGR_PROCESS    = 0x800,
	IPA_HW_PKT_STATUS_MASK_DEAGG_FIRST       = 0x1000,
	IPA_HW_PKT_STATUS_MASK_SRC_EOT           = 0x2000
};

/*! @brief Possible Exceptions received in status */
enum ipa_hw_pkt_status_exception {
	IPA_HW_PKT_STATUS_EXCEPTION_NONE           = 0x0,
	IPA_HW_PKT_STATUS_EXCEPTION_DEAGGR         = 0x1,
	IPA_HW_PKT_STATUS_EXCEPTION_REPL           = 0x2,
	IPA_HW_PKT_STATUS_EXCEPTION_IPTYPE         = 0x4,
	IPA_HW_PKT_STATUS_EXCEPTION_IHL            = 0x8,
	IPA_HW_PKT_STATUS_EXCEPTION_FRAG_RULE_MISS = 0x10,
	IPA_HW_PKT_STATUS_EXCEPTION_SW_FILT        = 0x20,
	IPA_HW_PKT_STATUS_EXCEPTION_NAT            = 0x40,
	IPA_HW_PKT_STATUS_EXCEPTION_ACTUAL_MAX,
	IPA_HW_PKT_STATUS_EXCEPTION_MAX            = 0xFF
};

/*! @brief IPA_HW_IMM_CMD_DMA_SHARED_MEM Immediate Command Parameters */
struct ipa_hw_imm_cmd_dma_shared_mem {
	u32 reserved_1:16;
	u32 size:16;
	u32 system_addr:32;
	u32 local_addr:16;
	u32 direction:1;
	u32 skip_pipeline_clear:1;
	u32 reserved_2:14;
	u32 padding:32;
};

#endif /* _IPA_HW_DEFS_H */
