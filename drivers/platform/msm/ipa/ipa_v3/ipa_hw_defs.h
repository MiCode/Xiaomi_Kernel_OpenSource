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

#endif /* _IPA_HW_DEFS_H */
