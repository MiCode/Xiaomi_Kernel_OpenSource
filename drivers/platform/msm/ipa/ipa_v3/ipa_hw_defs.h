/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_HW_DEFS_H
#define _IPA_HW_DEFS_H
#include <linux/bitops.h>

/* This header defines various HW related data types */

/* Processing context TLV type */
#define IPA_PROC_CTX_TLV_TYPE_END 0
#define IPA_PROC_CTX_TLV_TYPE_HDR_ADD 1
#define IPA_PROC_CTX_TLV_TYPE_PROC_CMD 3

#define IPA_RULE_ID_INVALID 0x3FF

/**
 * struct ipa3_flt_rule_hw_hdr - HW header of IPA filter rule
 * @word: filtering rule properties
 * @en_rule: enable rule
 * @action: post routing action
 * @rt_tbl_idx: index in routing table
 * @retain_hdr: added to add back to the packet the header removed
 *  as part of header removal. This will be done as part of
 *  header insertion block.
 * @rsvd1: reserved bits
 * @priority: Rule priority. Added to distinguish rules order
 *  at the integrated table consisting from hashable and
 *  non-hashable parts
 * @rsvd2: reserved bits
 * @rule_id: rule ID that will be returned in the packet status
 * @rsvd3: reserved bits
 */
struct ipa3_flt_rule_hw_hdr {
	union {
		u64 word;
		struct {
			u64 en_rule:16;
			u64 action:5;
			u64 rt_tbl_idx:5;
			u64 retain_hdr:1;
			u64 rsvd1:5;
			u64 priority:10;
			u64 rsvd2:6;
			u64 rule_id:10;
			u64 rsvd3:6;
		} hdr;
	} u;
};

/**
 * struct ipa3_rt_rule_hw_hdr - HW header of IPA routing rule
 * @word: routing rule properties
 * @en_rule: enable rule
 * @pipe_dest_idx: destination pipe index
 * @system: changed from local to system due to HW change
 * @hdr_offset: header offset
 * @proc_ctx: whether hdr_offset points to header table or to
 *	header processing context table
 * @priority: Rule priority. Added to distinguish rules order
 *  at the integrated table consisting from hashable and
 *  non-hashable parts
 * @rsvd1: reserved bits
 * @retain_hdr: added to add back to the packet the header removed
 *  as part of header removal. This will be done as part of
 *  header insertion block.
 * @rule_id: rule ID that will be returned in the packet status
 * @rsvd2: reserved bits
 */
struct ipa3_rt_rule_hw_hdr {
	union {
		u64 word;
		struct {
			u64 en_rule:16;
			u64 pipe_dest_idx:5;
			u64 system:1;
			u64 hdr_offset:9;
			u64 proc_ctx:1;
			u64 priority:10;
			u64 rsvd1:5;
			u64 retain_hdr:1;
			u64 rule_id:10;
			u64 rsvd2:6;
		} hdr;
	} u;
};

/**
 * struct ipa3_hdr_proc_ctx_tlv -
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
struct ipa3_hdr_proc_ctx_tlv {
	u32 type:8;
	u32 length:8;
	u32 value:16;
};

/**
 * struct ipa3_hdr_proc_ctx_hdr_add -
 * HW structure of IPA processing context - add header tlv
 * @tlv: IPA processing context TLV
 * @hdr_addr: processing context header address
 */
struct ipa3_hdr_proc_ctx_hdr_add {
	struct ipa3_hdr_proc_ctx_tlv tlv;
	u32 hdr_addr;
};

#define IPA_A5_MUX_HDR_EXCP_FLAG_IP		BIT(7)
#define IPA_A5_MUX_HDR_EXCP_FLAG_NAT		BIT(6)
#define IPA_A5_MUX_HDR_EXCP_FLAG_SW_FLT	BIT(5)
#define IPA_A5_MUX_HDR_EXCP_FLAG_TAG		BIT(4)
#define IPA_A5_MUX_HDR_EXCP_FLAG_REPLICATED	BIT(3)
#define IPA_A5_MUX_HDR_EXCP_FLAG_IHL		BIT(2)

/**
 * struct ipa3_a5_mux_hdr - A5 MUX header definition
 * @interface_id: interface ID
 * @src_pipe_index: source pipe index
 * @flags: flags
 * @metadata: metadata
 *
 * A5 MUX header is in BE, A5 runs in LE. This struct definition
 * allows A5 SW to correctly parse the header
 */
struct ipa3_a5_mux_hdr {
	u16 interface_id;
	u8 src_pipe_index;
	u8 flags;
	u32 metadata;
};

/*! @brief Struct for the IPAv3.0 UL packet status header */
struct ipa3_hw_pkt_status {
	u64 status_opcode:8;
	u64 exception:8;
	u64 status_mask:16;
	u64 pkt_len:16;
	u64 endp_src_idx:5;
	u64 reserved_1:3;
	u64 endp_dest_idx:5;
	u64 reserved_2:3;
	u64 metadata:32;
	u64 filt_local:1;
	u64 filt_hash:1;
	u64 filt_global:1;
	u64 ret_hdr:1;
	u64 filt_rule_id:10;
	u64 route_local:1;
	u64 route_hash:1;
	u64 ucp:1;
	u64 route_tbl_idx:5;
	u64 route_rule_id:10;
	u64 nat_hit:1;
	u64 nat_tbl_idx:13;
	u64 nat_type:2;
	u64 tag:48;
	u64 seq_num:8;
	u64 time_day_ctr:24;
	u64 hdr_local:1;
	u64 hdr_offset:10;
	u64 frag_hit:1;
	u64 frag_rule:4;
	u64 reserved_4:16;
};

#define IPA_PKT_STATUS_SIZE 32

/*! @brief Status header opcodes */
enum ipa3_hw_status_opcode {
	IPA_HW_STATUS_OPCODE_PACKET             = 0x1,
	IPA_HW_STATUS_OPCODE_NEW_FRAG_RULE      = 0x2,
	IPA_HW_STATUS_OPCODE_DROPPED_PACKET     = 0x4,
	IPA_HW_STATUS_OPCODE_SUSPENDED_PACKET   = 0x8,
	IPA_HW_STATUS_OPCODE_LOG                = 0x10,
	IPA_HW_STATUS_OPCODE_DCMP               = 0x20,
	IPA_HW_STATUS_OPCODE_PACKET_2ND_PASS    = 0x40,

};

/*! @brief Possible Masks received in status */
enum ipa3_hw_pkt_status_mask {
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
enum ipa3_hw_pkt_status_exception {
	IPA_HW_PKT_STATUS_EXCEPTION_NONE             = 0x0,
	IPA_HW_PKT_STATUS_EXCEPTION_DEAGGR           = 0x1,
	IPA_HW_PKT_STATUS_EXCEPTION_IPTYPE           = 0x4,
	IPA_HW_PKT_STATUS_EXCEPTION_PACKET_LENGTH    = 0x8,
	IPA_HW_PKT_STATUS_EXCEPTION_PACKET_THRESHOLD = 0x9,
	IPA_HW_PKT_STATUS_EXCEPTION_FRAG_RULE_MISS   = 0x10,
	IPA_HW_PKT_STATUS_EXCEPTION_SW_FILT          = 0x20,
	IPA_HW_PKT_STATUS_EXCEPTION_NAT              = 0x40,
	IPA_HW_PKT_STATUS_EXCEPTION_ACTUAL_MAX,
	IPA_HW_PKT_STATUS_EXCEPTION_MAX              = 0xFF
};

#endif /* _IPA_HW_DEFS_H */
