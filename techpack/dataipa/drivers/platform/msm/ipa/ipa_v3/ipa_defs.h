/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_DEFS_H_
#define _IPA_DEFS_H_
#include <linux/ipa.h>

/**
 * struct ipa_rt_rule_i - attributes of a routing rule
 * @dst: dst "client"
 * @hdr_hdl: handle to the dynamic header
	it is not an index or an offset
 * @hdr_proc_ctx_hdl: handle to header processing context. if it is provided
	hdr_hdl shall be 0
 * @attrib: attributes of the rule
 * @max_prio: bool switch. is this rule with Max priority? meaning on rule hit,
 *  IPA will use the rule and will not look for other rules that may have
 *  higher priority
 * @hashable: bool switch. is this rule hashable or not?
 *  ipa uses hashable rules to cache their hit results to be used in
 *  consecutive packets
 * @retain_hdr: bool switch to instruct IPA core to add back to the packet
 *  the header removed as part of header removal
 * @coalesce: bool to decide whether packets should be coalesced or not
 * @enable_stats: is true when we want to enable stats for this
 * rt rule.
 * @cnt_idx: if enable_stats is 1 and cnt_idx is 0, then cnt_idx
 * will be assigned by ipa driver.
 */
struct ipa_rt_rule_i {
	enum ipa_client_type dst;
	u32 hdr_hdl;
	u32 hdr_proc_ctx_hdl;
	struct ipa_rule_attrib attrib;
	u8 max_prio;
	u8 hashable;
	u8 retain_hdr;
	u8 coalesce;
	u8 enable_stats;
	u8 cnt_idx;
};

/**
 * struct ipa_flt_rule_i - attributes of a filtering rule
 * @retain_hdr: bool switch to instruct IPA core to add back to the packet
 *  the header removed as part of header removal
 * @to_uc: bool switch to pass packet to micro-controller
 * @action: action field
 * @rt_tbl_hdl: handle of table from "get"
 * @attrib: attributes of the rule
 * @eq_attrib: attributes of the rule in equation form (valid when
 * eq_attrib_type is true)
 * @rt_tbl_idx: index of RT table referred to by filter rule (valid when
 * eq_attrib_type is true and non-exception action)
 * @eq_attrib_type: true if equation level form used to specify attributes
 * @max_prio: bool switch. is this rule with Max priority? meaning on rule hit,
 *  IPA will use the rule and will not look for other rules that may have
 *  higher priority
 * @hashable: bool switch. is this rule hashable or not?
 *  ipa uses hashable rules to cache their hit results to be used in
 *  consecutive packets
 * @rule_id: rule_id to be assigned to the filter rule. In case client specifies
 *  rule_id as 0 the driver will assign a new rule_id
 * @set_metadata: bool switch. should metadata replacement at the NAT block
 *  take place?
 * @pdn_idx: if action is "pass to source\destination NAT" then a comparison
 * against the PDN index in the matching PDN entry will take place as an
 * additional condition for NAT hit.
 * @enable_stats: is true when we want to enable stats for this
 * flt rule.
 * @cnt_idx: if 0 means disable, otherwise use for index.
 * will be assigned by ipa driver.
 */
struct ipa_flt_rule_i {
	u8 retain_hdr;
	u8 to_uc;
	enum ipa_flt_action action;
	u32 rt_tbl_hdl;
	struct ipa_rule_attrib attrib;
	struct ipa_ipfltri_rule_eq eq_attrib;
	u32 rt_tbl_idx;
	u8 eq_attrib_type;
	u8 max_prio;
	u8 hashable;
	u16 rule_id;
	u8 set_metadata;
	u8 pdn_idx;
	u8 enable_stats;
	u8 cnt_idx;
};

#endif /* _IPA_DEFS_H_ */
