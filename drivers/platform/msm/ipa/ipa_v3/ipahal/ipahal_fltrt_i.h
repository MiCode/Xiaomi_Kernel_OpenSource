/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#ifndef _IPAHAL_FLTRT_I_H_
#define _IPAHAL_FLTRT_I_H_

/*
 * enum ipa_fltrt_equations - RULE equations
 *  These are names values to the equations that can be used
 *  The HAL layer holds mapping between these names and H/W
 *  presentation.
 */
enum ipa_fltrt_equations {
	IPA_TOS_EQ,
	IPA_PROTOCOL_EQ,
	IPA_TC_EQ,
	IPA_OFFSET_MEQ128_0,
	IPA_OFFSET_MEQ128_1,
	IPA_OFFSET_MEQ32_0,
	IPA_OFFSET_MEQ32_1,
	IPA_IHL_OFFSET_MEQ32_0,
	IPA_IHL_OFFSET_MEQ32_1,
	IPA_METADATA_COMPARE,
	IPA_IHL_OFFSET_RANGE16_0,
	IPA_IHL_OFFSET_RANGE16_1,
	IPA_IHL_OFFSET_EQ_32,
	IPA_IHL_OFFSET_EQ_16,
	IPA_FL_EQ,
	IPA_IS_FRAG,
	IPA_EQ_MAX,
};

/* Width and Alignment values for H/W structures.
 * Specific for IPA version.
 */
#define IPA3_0_HW_TBL_SYSADDR_ALIGNMENT (127)
#define IPA3_0_HW_TBL_LCLADDR_ALIGNMENT (7)
#define IPA3_0_HW_TBL_BLK_SIZE_ALIGNMENT (127)
#define IPA3_0_HW_TBL_WIDTH (8)
#define IPA3_0_HW_TBL_HDR_WIDTH (8)
#define IPA3_0_HW_TBL_ADDR_MASK (127)
#define IPA3_0_HW_RULE_BUF_SIZE (256)
#define IPA3_0_HW_RULE_START_ALIGNMENT (7)


/*
 * Rules Priority.
 * Needed due to rules classification to hashable and non-hashable.
 * Higher priority is lower in number. i.e. 0 is highest priority
 */
#define IPA3_0_RULE_MAX_PRIORITY (0)
#define IPA3_0_RULE_MIN_PRIORITY (1023)

/*
 * RULE ID, bit length (e.g. 10 bits).
 */
#define IPA3_0_RULE_ID_BIT_LEN (10)
#define IPA3_0_LOW_RULE_ID (1)

/**
 * struct ipa3_0_rt_rule_hw_hdr - HW header of IPA routing rule
 * @word: routing rule header properties
 * @en_rule: enable rule - Equation bit fields
 * @pipe_dest_idx: destination pipe index
 * @system: Is referenced header is lcl or sys memory
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
struct ipa3_0_rt_rule_hw_hdr {
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
 * struct ipa3_0_flt_rule_hw_hdr - HW header of IPA filter rule
 * @word: filtering rule properties
 * @en_rule: enable rule
 * @action: post filtering action
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
struct ipa3_0_flt_rule_hw_hdr {
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
 * struct ipa4_0_flt_rule_hw_hdr - HW header of IPA filter rule
 * @word: filtering rule properties
 * @en_rule: enable rule
 * @action: post filtering action
 * @rt_tbl_idx: index in routing table
 * @retain_hdr: added to add back to the packet the header removed
 *  as part of header removal. This will be done as part of
 *  header insertion block.
 * @pdn_idx: in case of go to src nat action possible to input the pdn index to
 *  the NAT block
 * @set_metadata: enable metadata replacement in the NAT block
 * @priority: Rule priority. Added to distinguish rules order
 *  at the integrated table consisting from hashable and
 *  non-hashable parts
 * @rsvd2: reserved bits
 * @rule_id: rule ID that will be returned in the packet status
 * @rsvd3: reserved bits
 */
struct ipa4_0_flt_rule_hw_hdr {
	union {
		u64 word;
		struct {
			u64 en_rule : 16;
			u64 action : 5;
			u64 rt_tbl_idx : 5;
			u64 retain_hdr : 1;
			u64 pdn_idx : 4;
			u64 set_metadata : 1;
			u64 priority : 10;
			u64 rsvd2 : 6;
			u64 rule_id : 10;
			u64 rsvd3 : 6;
		} hdr;
	} u;
};

int ipahal_fltrt_init(enum ipa_hw_type ipa_hw_type);
void ipahal_fltrt_destroy(void);

#endif /* _IPAHAL_FLTRT_I_H_ */
