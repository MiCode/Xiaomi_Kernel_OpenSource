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

#include <linux/ipc_logging.h>
#include <linux/debugfs.h>
#include <linux/ipa.h>
#include "ipahal.h"
#include "ipahal_fltrt.h"
#include "ipahal_fltrt_i.h"
#include "ipahal_i.h"
#include "../../ipa_common_i.h"

/*
 * struct ipahal_fltrt_obj - Flt/Rt H/W information for specific IPA version
 * @support_hash: Is hashable tables supported
 * @tbl_width: Width of table in bytes
 * @sysaddr_alignment: System table address alignment
 * @lcladdr_alignment: Local table offset alignment
 * @blk_sz_alignment: Rules block size alignment
 * @rule_start_alignment: Rule start address alignment
 * @tbl_hdr_width: Width of the header structure in bytes
 * @tbl_addr_mask: Masking for Table address
 * @rule_max_prio: Max possible priority of a rule
 * @rule_min_prio: Min possible priority of a rule
 * @low_rule_id: Low value of Rule ID that can be used
 * @rule_id_bit_len: Rule is high (MSB) bit len
 * @rule_buf_size: Max size rule may utilize.
 * @write_val_to_hdr: Write address or offset to header entry
 * @create_flt_bitmap: Create bitmap in H/W format using given bitmap
 * @create_tbl_addr: Given raw table address, create H/W formated one
 * @parse_tbl_addr: Parse the given H/W address (hdr format)
 * @rt_generate_hw_rule: Generate RT rule in H/W format
 * @flt_generate_hw_rule: Generate FLT rule in H/W format
 * @flt_generate_eq: Generate flt equation attributes from rule attributes
 * @rt_parse_hw_rule: Parse rt rule read from H/W
 * @flt_parse_hw_rule: Parse flt rule read from H/W
 * @eq_bitfield: Array of the bit fields of the support equations
 */
struct ipahal_fltrt_obj {
	bool support_hash;
	u32 tbl_width;
	u32 sysaddr_alignment;
	u32 lcladdr_alignment;
	u32 blk_sz_alignment;
	u32 rule_start_alignment;
	u32 tbl_hdr_width;
	u32 tbl_addr_mask;
	int rule_max_prio;
	int rule_min_prio;
	u32 low_rule_id;
	u32 rule_id_bit_len;
	u32 rule_buf_size;
	u8* (*write_val_to_hdr)(u64 val, u8 *hdr);
	u64 (*create_flt_bitmap)(u64 ep_bitmap);
	u64 (*create_tbl_addr)(bool is_sys, u64 addr);
	void (*parse_tbl_addr)(u64 hwaddr, u64 *addr, bool *is_sys);
	int (*rt_generate_hw_rule)(struct ipahal_rt_rule_gen_params *params,
		u32 *hw_len, u8 *buf);
	int (*flt_generate_hw_rule)(struct ipahal_flt_rule_gen_params *params,
		u32 *hw_len, u8 *buf);
	int (*flt_generate_eq)(enum ipa_ip_type ipt,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb);
	int (*rt_parse_hw_rule)(u8 *addr, struct ipahal_rt_rule_entry *rule);
	int (*flt_parse_hw_rule)(u8 *addr, struct ipahal_flt_rule_entry *rule);
	u8 eq_bitfield[IPA_EQ_MAX];
};


static u64 ipa_fltrt_create_flt_bitmap(u64 ep_bitmap)
{
	/* At IPA3, there global configuration is possible but not used */
	return (ep_bitmap << 1) & ~0x1;
}

static u64 ipa_fltrt_create_tbl_addr(bool is_sys, u64 addr)
{
	if (is_sys) {
		if (addr & IPA3_0_HW_TBL_SYSADDR_ALIGNMENT) {
			IPAHAL_ERR(
				"sys addr is not aligned accordingly addr=0x%pad\n",
				&addr);
			ipa_assert();
			return 0;
		}
	} else {
		if (addr & IPA3_0_HW_TBL_LCLADDR_ALIGNMENT) {
			IPAHAL_ERR("addr/ofst isn't lcl addr aligned %llu\n",
				addr);
			ipa_assert();
			return 0;
		}
		/*
		 * for local tables (at sram) offsets is used as tables
		 * addresses. offset need to be in 8B units
		 * (local address aligned) and left shifted to its place.
		 * Local bit need to be enabled.
		 */
		addr /= IPA3_0_HW_TBL_LCLADDR_ALIGNMENT + 1;
		addr *= IPA3_0_HW_TBL_ADDR_MASK + 1;
		addr += 1;
	}

	return addr;
}

static void ipa_fltrt_parse_tbl_addr(u64 hwaddr, u64 *addr, bool *is_sys)
{
	IPAHAL_DBG_LOW("Parsing hwaddr 0x%llx\n", hwaddr);

	*is_sys = !(hwaddr & 0x1);
	hwaddr &= (~0ULL - 1);
	if (hwaddr & IPA3_0_HW_TBL_SYSADDR_ALIGNMENT) {
		IPAHAL_ERR(
			"sys addr is not aligned accordingly addr=0x%pad\n",
			&hwaddr);
		ipa_assert();
		return;
	}

	if (!*is_sys) {
		hwaddr /= IPA3_0_HW_TBL_ADDR_MASK + 1;
		hwaddr *= IPA3_0_HW_TBL_LCLADDR_ALIGNMENT + 1;
	}

	*addr = hwaddr;
}

/* Update these tables of the number of equations changes */
static const int ipa3_0_ofst_meq32[] = { IPA_OFFSET_MEQ32_0,
					IPA_OFFSET_MEQ32_1};
static const int ipa3_0_ofst_meq128[] = { IPA_OFFSET_MEQ128_0,
					IPA_OFFSET_MEQ128_1};
static const int ipa3_0_ihl_ofst_rng16[] = { IPA_IHL_OFFSET_RANGE16_0,
					IPA_IHL_OFFSET_RANGE16_1};
static const int ipa3_0_ihl_ofst_meq32[] = { IPA_IHL_OFFSET_MEQ32_0,
					IPA_IHL_OFFSET_MEQ32_1};

static int ipa_fltrt_generate_hw_rule_bdy(enum ipa_ip_type ipt,
	const struct ipa_rule_attrib *attrib, u8 **buf, u16 *en_rule);
static int ipa_fltrt_generate_hw_rule_bdy_from_eq(
		const struct ipa_ipfltri_rule_eq *attrib, u8 **buf);
static int ipa_flt_generate_eq_ip4(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb);
static int ipa_flt_generate_eq_ip6(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb);
static int ipa_flt_generate_eq(enum ipa_ip_type ipt,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb);
static int ipa_rt_parse_hw_rule(u8 *addr,
		struct ipahal_rt_rule_entry *rule);
static int ipa_flt_parse_hw_rule(u8 *addr,
		struct ipahal_flt_rule_entry *rule);

#define IPA_IS_RAN_OUT_OF_EQ(__eq_array, __eq_index) \
	(ARRAY_SIZE(__eq_array) <= (__eq_index))

#define IPA_GET_RULE_EQ_BIT_PTRN(__eq) \
	(BIT(ipahal_fltrt_objs[ipahal_ctx->hw_type].eq_bitfield[(__eq)]))

/*
 * ipa_fltrt_rule_generation_err_check() - check basic validity on the rule
 *  attribs before starting building it
 *  checks if not not using ipv4 attribs on ipv6 and vice-versa
 * @ip: IP address type
 * @attrib: IPA rule attribute
 *
 * Return: 0 on success, -EPERM on failure
 */
static int ipa_fltrt_rule_generation_err_check(
	enum ipa_ip_type ipt, const struct ipa_rule_attrib *attrib)
{
	if (ipt == IPA_IP_v4) {
		if (attrib->attrib_mask & IPA_FLT_NEXT_HDR ||
		    attrib->attrib_mask & IPA_FLT_TC ||
		    attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
			IPAHAL_ERR("v6 attrib's specified for v4 rule\n");
			return -EPERM;
		}
	} else if (ipt == IPA_IP_v6) {
		if (attrib->attrib_mask & IPA_FLT_TOS ||
		    attrib->attrib_mask & IPA_FLT_PROTOCOL) {
			IPAHAL_ERR("v4 attrib's specified for v6 rule\n");
			return -EPERM;
		}
	} else {
		IPAHAL_ERR("unsupported ip %d\n", ipt);
		return -EPERM;
	}

	return 0;
}

static int ipa_rt_gen_hw_rule(struct ipahal_rt_rule_gen_params *params,
	u32 *hw_len, u8 *buf)
{
	struct ipa3_0_rt_rule_hw_hdr *rule_hdr;
	u8 *start;
	u16 en_rule = 0;

	start = buf;
	rule_hdr = (struct ipa3_0_rt_rule_hw_hdr *)buf;

	ipa_assert_on(params->dst_pipe_idx & ~0x1F);
	rule_hdr->u.hdr.pipe_dest_idx = params->dst_pipe_idx;
	switch (params->hdr_type) {
	case IPAHAL_RT_RULE_HDR_PROC_CTX:
		rule_hdr->u.hdr.system = !params->hdr_lcl;
		rule_hdr->u.hdr.proc_ctx = 1;
		ipa_assert_on(params->hdr_ofst & 31);
		rule_hdr->u.hdr.hdr_offset = (params->hdr_ofst) >> 5;
		break;
	case IPAHAL_RT_RULE_HDR_RAW:
		rule_hdr->u.hdr.system = !params->hdr_lcl;
		rule_hdr->u.hdr.proc_ctx = 0;
		ipa_assert_on(params->hdr_ofst & 3);
		rule_hdr->u.hdr.hdr_offset = (params->hdr_ofst) >> 2;
		break;
	case IPAHAL_RT_RULE_HDR_NONE:
		rule_hdr->u.hdr.system = !params->hdr_lcl;
		rule_hdr->u.hdr.proc_ctx = 0;
		rule_hdr->u.hdr.hdr_offset = 0;
		break;
	default:
		IPAHAL_ERR("Invalid HDR type %d\n", params->hdr_type);
		WARN_ON(1);
		return -EINVAL;
	};

	ipa_assert_on(params->priority & ~0x3FF);
	rule_hdr->u.hdr.priority = params->priority;
	rule_hdr->u.hdr.retain_hdr = params->rule->retain_hdr ? 0x1 : 0x0;
	ipa_assert_on(params->id & ~((1 << IPA3_0_RULE_ID_BIT_LEN) - 1));
	ipa_assert_on(params->id == ((1 << IPA3_0_RULE_ID_BIT_LEN) - 1));
	rule_hdr->u.hdr.rule_id = params->id;

	buf += sizeof(struct ipa3_0_rt_rule_hw_hdr);

	if (ipa_fltrt_generate_hw_rule_bdy(params->ipt, &params->rule->attrib,
		&buf, &en_rule)) {
		IPAHAL_ERR("fail to generate hw rule\n");
		return -EPERM;
	}
	rule_hdr->u.hdr.en_rule = en_rule;

	IPAHAL_DBG_LOW("en_rule 0x%x\n", en_rule);
	ipa_write_64(rule_hdr->u.word, (u8 *)rule_hdr);

	if (*hw_len == 0) {
		*hw_len = buf - start;
	} else if (*hw_len != (buf - start)) {
		IPAHAL_ERR("hw_len differs b/w passed=0x%x calc=%td\n",
			*hw_len, (buf - start));
		return -EPERM;
	}

	return 0;
}

static int ipa_flt_gen_hw_rule(struct ipahal_flt_rule_gen_params *params,
	u32 *hw_len, u8 *buf)
{
	struct ipa3_0_flt_rule_hw_hdr *rule_hdr;
	u8 *start;
	u16 en_rule = 0;

	start = buf;
	rule_hdr = (struct ipa3_0_flt_rule_hw_hdr *)buf;

	switch (params->rule->action) {
	case IPA_PASS_TO_ROUTING:
		rule_hdr->u.hdr.action = 0x0;
		break;
	case IPA_PASS_TO_SRC_NAT:
		rule_hdr->u.hdr.action = 0x1;
		break;
	case IPA_PASS_TO_DST_NAT:
		rule_hdr->u.hdr.action = 0x2;
		break;
	case IPA_PASS_TO_EXCEPTION:
		rule_hdr->u.hdr.action = 0x3;
		break;
	default:
		IPAHAL_ERR("Invalid Rule Action %d\n", params->rule->action);
		WARN_ON(1);
		return -EINVAL;
	}
	ipa_assert_on(params->rt_tbl_idx & ~0x1F);
	rule_hdr->u.hdr.rt_tbl_idx = params->rt_tbl_idx;
	rule_hdr->u.hdr.retain_hdr = params->rule->retain_hdr ? 0x1 : 0x0;
	rule_hdr->u.hdr.rsvd1 = 0;
	rule_hdr->u.hdr.rsvd2 = 0;
	rule_hdr->u.hdr.rsvd3 = 0;

	ipa_assert_on(params->priority & ~0x3FF);
	rule_hdr->u.hdr.priority = params->priority;
	ipa_assert_on(params->id & ~((1 << IPA3_0_RULE_ID_BIT_LEN) - 1));
	ipa_assert_on(params->id == ((1 << IPA3_0_RULE_ID_BIT_LEN) - 1));
	rule_hdr->u.hdr.rule_id = params->id;

	buf += sizeof(struct ipa3_0_flt_rule_hw_hdr);

	if (params->rule->eq_attrib_type) {
		if (ipa_fltrt_generate_hw_rule_bdy_from_eq(
			&params->rule->eq_attrib, &buf)) {
			IPAHAL_ERR("fail to generate hw rule from eq\n");
			return -EPERM;
		}
		en_rule = params->rule->eq_attrib.rule_eq_bitmap;
	} else {
		if (ipa_fltrt_generate_hw_rule_bdy(params->ipt,
			&params->rule->attrib, &buf, &en_rule)) {
			IPAHAL_ERR("fail to generate hw rule\n");
			return -EPERM;
		}
	}
	rule_hdr->u.hdr.en_rule = en_rule;

	IPAHAL_DBG_LOW("en_rule=0x%x, action=%d, rt_idx=%d, retain_hdr=%d\n",
		en_rule,
		rule_hdr->u.hdr.action,
		rule_hdr->u.hdr.rt_tbl_idx,
		rule_hdr->u.hdr.retain_hdr);
	IPAHAL_DBG_LOW("priority=%d, rule_id=%d\n",
		rule_hdr->u.hdr.priority,
		rule_hdr->u.hdr.rule_id);

	ipa_write_64(rule_hdr->u.word, (u8 *)rule_hdr);

	if (*hw_len == 0) {
		*hw_len = buf - start;
	} else if (*hw_len != (buf - start)) {
		IPAHAL_ERR("hw_len differs b/w passed=0x%x calc=%td\n",
			*hw_len, (buf - start));
		return -EPERM;
	}

	return 0;
}

/*
 * This array contains the FLT/RT info for IPAv3 and later.
 * All the information on IPAv3 are statically defined below.
 * If information is missing regarding on some IPA version,
 *  the init function will fill it with the information from the previous
 *  IPA version.
 * Information is considered missing if all of the fields are 0.
 */
static struct ipahal_fltrt_obj ipahal_fltrt_objs[IPA_HW_MAX] = {
	/* IPAv3 */
	[IPA_HW_v3_0] = {
		true,
		IPA3_0_HW_TBL_WIDTH,
		IPA3_0_HW_TBL_SYSADDR_ALIGNMENT,
		IPA3_0_HW_TBL_LCLADDR_ALIGNMENT,
		IPA3_0_HW_TBL_BLK_SIZE_ALIGNMENT,
		IPA3_0_HW_RULE_START_ALIGNMENT,
		IPA3_0_HW_TBL_HDR_WIDTH,
		IPA3_0_HW_TBL_ADDR_MASK,
		IPA3_0_RULE_MAX_PRIORITY,
		IPA3_0_RULE_MIN_PRIORITY,
		IPA3_0_LOW_RULE_ID,
		IPA3_0_RULE_ID_BIT_LEN,
		IPA3_0_HW_RULE_BUF_SIZE,
		ipa_write_64,
		ipa_fltrt_create_flt_bitmap,
		ipa_fltrt_create_tbl_addr,
		ipa_fltrt_parse_tbl_addr,
		ipa_rt_gen_hw_rule,
		ipa_flt_gen_hw_rule,
		ipa_flt_generate_eq,
		ipa_rt_parse_hw_rule,
		ipa_flt_parse_hw_rule,
		{
			[IPA_TOS_EQ]			= 0,
			[IPA_PROTOCOL_EQ]		= 1,
			[IPA_TC_EQ]			= 2,
			[IPA_OFFSET_MEQ128_0]		= 3,
			[IPA_OFFSET_MEQ128_1]		= 4,
			[IPA_OFFSET_MEQ32_0]		= 5,
			[IPA_OFFSET_MEQ32_1]		= 6,
			[IPA_IHL_OFFSET_MEQ32_0]	= 7,
			[IPA_IHL_OFFSET_MEQ32_1]	= 8,
			[IPA_METADATA_COMPARE]		= 9,
			[IPA_IHL_OFFSET_RANGE16_0]	= 10,
			[IPA_IHL_OFFSET_RANGE16_1]	= 11,
			[IPA_IHL_OFFSET_EQ_32]		= 12,
			[IPA_IHL_OFFSET_EQ_16]		= 13,
			[IPA_FL_EQ]			= 14,
			[IPA_IS_FRAG]			= 15,
		},
	},
};

static int ipa_flt_generate_eq(enum ipa_ip_type ipt,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	if (ipa_fltrt_rule_generation_err_check(ipt, attrib))
		return -EPERM;

	if (ipt == IPA_IP_v4) {
		if (ipa_flt_generate_eq_ip4(ipt, attrib, eq_atrb)) {
			IPAHAL_ERR("failed to build ipv4 flt eq rule\n");
			return -EPERM;
		}
	} else if (ipt == IPA_IP_v6) {
		if (ipa_flt_generate_eq_ip6(ipt, attrib, eq_atrb)) {
			IPAHAL_ERR("failed to build ipv6 flt eq rule\n");
			return -EPERM;
		}
	} else {
		IPAHAL_ERR("unsupported ip %d\n", ipt);
		return  -EPERM;
	}

	/*
	 * default "rule" means no attributes set -> map to
	 * OFFSET_MEQ32_0 with mask of 0 and val of 0 and offset 0
	 */
	if (attrib->attrib_mask == 0) {
		eq_atrb->rule_eq_bitmap = 0;
		eq_atrb->rule_eq_bitmap |= IPA_GET_RULE_EQ_BIT_PTRN(
			IPA_OFFSET_MEQ32_0);
		eq_atrb->offset_meq_32[0].offset = 0;
		eq_atrb->offset_meq_32[0].mask = 0;
		eq_atrb->offset_meq_32[0].value = 0;
	}

	return 0;
}

static void ipa_fltrt_generate_mac_addr_hw_rule(u8 **extra, u8 **rest,
	u8 hdr_mac_addr_offset,
	const uint8_t mac_addr_mask[ETH_ALEN],
	const uint8_t mac_addr[ETH_ALEN])
{
	int i;

	*extra = ipa_write_8(hdr_mac_addr_offset, *extra);

	/* LSB MASK and ADDR */
	*rest = ipa_write_64(0, *rest);
	*rest = ipa_write_64(0, *rest);

	/* MSB MASK and ADDR */
	*rest = ipa_write_16(0, *rest);
	for (i = 5; i >= 0; i--)
		*rest = ipa_write_8(mac_addr_mask[i], *rest);
	*rest = ipa_write_16(0, *rest);
	for (i = 5; i >= 0; i--)
		*rest = ipa_write_8(mac_addr[i], *rest);
}

static int ipa_fltrt_generate_hw_rule_bdy_ip4(u16 *en_rule,
	const struct ipa_rule_attrib *attrib,
	u8 **extra_wrds, u8 **rest_wrds)
{
	u8 *extra = *extra_wrds;
	u8 *rest = *rest_wrds;
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	int rc = 0;

	if (attrib->attrib_mask & IPA_FLT_TOS) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_TOS_EQ);
		extra = ipa_write_8(attrib->u.v4.tos, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_PROTOCOL_EQ);
		extra = ipa_write_8(attrib->u.v4.protocol, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-14,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-8,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-22,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-16,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		/* 0 => offset of TOS in v4 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_32((attrib->tos_mask << 16), rest);
		rest = ipa_write_32((attrib->tos_value << 16), rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		/* 12 => offset of src ip in v4 header */
		extra = ipa_write_8(12, extra);
		rest = ipa_write_32(attrib->u.v4.src_addr_mask, rest);
		rest = ipa_write_32(attrib->u.v4.src_addr, rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		/* 16 => offset of dst ip in v4 header */
		extra = ipa_write_8(16, extra);
		rest = ipa_write_32(attrib->u.v4.dst_addr_mask, rest);
		rest = ipa_write_32(attrib->u.v4.dst_addr, rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		/* -2 => offset of ether type in L2 hdr */
		extra = ipa_write_8((u8)-2, extra);
		rest = ipa_write_16(0, rest);
		rest = ipa_write_16(htons(attrib->ether_type), rest);
		rest = ipa_write_16(0, rest);
		rest = ipa_write_16(htons(attrib->ether_type), rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		/* 0  => offset of type after v4 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_32(0xFF, rest);
		rest = ipa_write_32(attrib->type, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		/* 1  => offset of code after v4 header */
		extra = ipa_write_8(1, extra);
		rest = ipa_write_32(0xFF, rest);
		rest = ipa_write_32(attrib->code, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		/* 0  => offset of SPI after v4 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_32(0xFFFFFFFF, rest);
		rest = ipa_write_32(attrib->spi, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_METADATA_COMPARE);
		rest = ipa_write_32(attrib->meta_data_mask, rest);
		rest = ipa_write_32(attrib->meta_data, rest);
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAHAL_ERR("bad src port range param\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 0  => offset of src port after v4 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_16(attrib->src_port_hi, rest);
		rest = ipa_write_16(attrib->src_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAHAL_ERR("bad dst port range param\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 2  => offset of dst port after v4 header */
		extra = ipa_write_8(2, extra);
		rest = ipa_write_16(attrib->dst_port_hi, rest);
		rest = ipa_write_16(attrib->dst_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 0  => offset of src port after v4 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_16(attrib->src_port, rest);
		rest = ipa_write_16(attrib->src_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 2  => offset of dst port after v4 header */
		extra = ipa_write_8(2, extra);
		rest = ipa_write_16(attrib->dst_port, rest);
		rest = ipa_write_16(attrib->dst_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT)
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_IS_FRAG);

	goto done;

err:
	rc = -EPERM;
done:
	*extra_wrds = extra;
	*rest_wrds = rest;
	return rc;
}

static int ipa_fltrt_generate_hw_rule_bdy_ip6(u16 *en_rule,
	const struct ipa_rule_attrib *attrib,
	u8 **extra_wrds, u8 **rest_wrds)
{
	u8 *extra = *extra_wrds;
	u8 *rest = *rest_wrds;
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	int rc = 0;

	/* v6 code below assumes no extension headers TODO: fix this */

	if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_PROTOCOL_EQ);
		extra = ipa_write_8(attrib->u.v6.next_hdr, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_TC) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_TC_EQ);
		extra = ipa_write_8(attrib->u.v6.tc, extra);
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);
		/* 8 => offset of src ip in v6 header */
		extra = ipa_write_8(8, extra);
		rest = ipa_write_32(attrib->u.v6.src_addr_mask[3], rest);
		rest = ipa_write_32(attrib->u.v6.src_addr_mask[2], rest);
		rest = ipa_write_32(attrib->u.v6.src_addr[3], rest);
		rest = ipa_write_32(attrib->u.v6.src_addr[2], rest);
		rest = ipa_write_32(attrib->u.v6.src_addr_mask[1], rest);
		rest = ipa_write_32(attrib->u.v6.src_addr_mask[0], rest);
		rest = ipa_write_32(attrib->u.v6.src_addr[1], rest);
		rest = ipa_write_32(attrib->u.v6.src_addr[0], rest);
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);
		/* 24 => offset of dst ip in v6 header */
		extra = ipa_write_8(24, extra);
		rest = ipa_write_32(attrib->u.v6.dst_addr_mask[3], rest);
		rest = ipa_write_32(attrib->u.v6.dst_addr_mask[2], rest);
		rest = ipa_write_32(attrib->u.v6.dst_addr[3], rest);
		rest = ipa_write_32(attrib->u.v6.dst_addr[2], rest);
		rest = ipa_write_32(attrib->u.v6.dst_addr_mask[1], rest);
		rest = ipa_write_32(attrib->u.v6.dst_addr_mask[0], rest);
		rest = ipa_write_32(attrib->u.v6.dst_addr[1], rest);
		rest = ipa_write_32(attrib->u.v6.dst_addr[0], rest);
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);
		/* 0 => offset of TOS in v6 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_64(0, rest);
		rest = ipa_write_64(0, rest);
		rest = ipa_write_32(0, rest);
		rest = ipa_write_32((attrib->tos_mask << 20), rest);
		rest = ipa_write_32(0, rest);
		rest = ipa_write_32((attrib->tos_value << 20), rest);
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-14,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-8,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-22,
			attrib->dst_mac_addr_mask,
			attrib->dst_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa_fltrt_generate_mac_addr_hw_rule(
			&extra,
			&rest,
			-16,
			attrib->src_mac_addr_mask,
			attrib->src_mac_addr);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		/* -2 => offset of ether type in L2 hdr */
		extra = ipa_write_8((u8)-2, extra);
		rest = ipa_write_16(0, rest);
		rest = ipa_write_16(htons(attrib->ether_type), rest);
		rest = ipa_write_16(0, rest);
		rest = ipa_write_16(htons(attrib->ether_type), rest);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		/* 0  => offset of type after v6 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_32(0xFF, rest);
		rest = ipa_write_32(attrib->type, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		/* 1  => offset of code after v6 header */
		extra = ipa_write_8(1, extra);
		rest = ipa_write_32(0xFF, rest);
		rest = ipa_write_32(attrib->code, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		/* 0  => offset of SPI after v6 header FIXME */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_32(0xFFFFFFFF, rest);
		rest = ipa_write_32(attrib->spi, rest);
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_METADATA_COMPARE);
		rest = ipa_write_32(attrib->meta_data_mask, rest);
		rest = ipa_write_32(attrib->meta_data, rest);
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 0  => offset of src port after v6 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_16(attrib->src_port, rest);
		rest = ipa_write_16(attrib->src_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 2  => offset of dst port after v6 header */
		extra = ipa_write_8(2, extra);
		rest = ipa_write_16(attrib->dst_port, rest);
		rest = ipa_write_16(attrib->dst_port, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAHAL_ERR("bad src port range param\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 0  => offset of src port after v6 header */
		extra = ipa_write_8(0, extra);
		rest = ipa_write_16(attrib->src_port_hi, rest);
		rest = ipa_write_16(attrib->src_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			goto err;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAHAL_ERR("bad dst port range param\n");
			goto err;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		/* 2  => offset of dst port after v6 header */
		extra = ipa_write_8(2, extra);
		rest = ipa_write_16(attrib->dst_port_hi, rest);
		rest = ipa_write_16(attrib->dst_port_lo, rest);
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_FL_EQ);
		rest = ipa_write_32(attrib->u.v6.flow_label & 0xFFFFF,
			rest);
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT)
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_IS_FRAG);

	goto done;

err:
	rc = -EPERM;
done:
	*extra_wrds = extra;
	*rest_wrds = rest;
	return rc;
}

static u8 *ipa_fltrt_copy_mem(u8 *src, u8 *dst, int cnt)
{
	while (cnt--)
		*dst++ = *src++;

	return dst;
}

/*
 * ipa_fltrt_generate_hw_rule_bdy() - generate HW rule body (w/o header)
 * @ip: IP address type
 * @attrib: IPA rule attribute
 * @buf: output buffer. Advance it after building the rule
 * @en_rule: enable rule
 *
 * Return codes:
 * 0: success
 * -EPERM: wrong input
 */
static int ipa_fltrt_generate_hw_rule_bdy(enum ipa_ip_type ipt,
	const struct ipa_rule_attrib *attrib, u8 **buf, u16 *en_rule)
{
	int sz;
	int rc = 0;
	u8 *extra_wrd_buf;
	u8 *rest_wrd_buf;
	u8 *extra_wrd_start;
	u8 *rest_wrd_start;
	u8 *extra_wrd_i;
	u8 *rest_wrd_i;

	sz = IPA3_0_HW_TBL_WIDTH * 2 + IPA3_0_HW_RULE_START_ALIGNMENT;
	extra_wrd_buf = kzalloc(sz, GFP_KERNEL);
	if (!extra_wrd_buf) {
		IPAHAL_ERR("failed to allocate %d bytes\n", sz);
		rc = -ENOMEM;
		goto fail_extra_alloc;
	}

	sz = IPA3_0_HW_RULE_BUF_SIZE + IPA3_0_HW_RULE_START_ALIGNMENT;
	rest_wrd_buf = kzalloc(sz, GFP_KERNEL);
	if (!rest_wrd_buf) {
		IPAHAL_ERR("failed to allocate %d bytes\n", sz);
		rc = -ENOMEM;
		goto fail_rest_alloc;
	}

	extra_wrd_start = extra_wrd_buf + IPA3_0_HW_RULE_START_ALIGNMENT;
	extra_wrd_start = (u8 *)((long)extra_wrd_start &
		~IPA3_0_HW_RULE_START_ALIGNMENT);

	rest_wrd_start = rest_wrd_buf + IPA3_0_HW_RULE_START_ALIGNMENT;
	rest_wrd_start = (u8 *)((long)rest_wrd_start &
		~IPA3_0_HW_RULE_START_ALIGNMENT);

	extra_wrd_i = extra_wrd_start;
	rest_wrd_i = rest_wrd_start;

	rc = ipa_fltrt_rule_generation_err_check(ipt, attrib);
	if (rc) {
		IPAHAL_ERR("rule generation err check failed\n");
		goto fail_err_check;
	}

	if (ipt == IPA_IP_v4) {
		if (ipa_fltrt_generate_hw_rule_bdy_ip4(en_rule, attrib,
			&extra_wrd_i, &rest_wrd_i)) {
			IPAHAL_ERR("failed to build ipv4 hw rule\n");
			rc = -EPERM;
			goto fail_err_check;
		}

	} else if (ipt == IPA_IP_v6) {
		if (ipa_fltrt_generate_hw_rule_bdy_ip6(en_rule, attrib,
			&extra_wrd_i, &rest_wrd_i)) {
			IPAHAL_ERR("failed to build ipv6 hw rule\n");
			rc = -EPERM;
			goto fail_err_check;
		}
	} else {
		IPAHAL_ERR("unsupported ip %d\n", ipt);
		goto fail_err_check;
	}

	/*
	 * default "rule" means no attributes set -> map to
	 * OFFSET_MEQ32_0 with mask of 0 and val of 0 and offset 0
	 */
	if (attrib->attrib_mask == 0) {
		IPAHAL_DBG_LOW("building default rule\n");
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(ipa3_0_ofst_meq32[0]);
		extra_wrd_i = ipa_write_8(0, extra_wrd_i);  /* offset */
		rest_wrd_i = ipa_write_32(0, rest_wrd_i);   /* mask */
		rest_wrd_i = ipa_write_32(0, rest_wrd_i);   /* val */
	}

	IPAHAL_DBG_LOW("extra_word_1 0x%llx\n", *(u64 *)extra_wrd_start);
	IPAHAL_DBG_LOW("extra_word_2 0x%llx\n",
		*(u64 *)(extra_wrd_start + IPA3_0_HW_TBL_WIDTH));

	extra_wrd_i = ipa_pad_to_64(extra_wrd_i);
	sz = extra_wrd_i - extra_wrd_start;
	IPAHAL_DBG_LOW("extra words params sz %d\n", sz);
	*buf = ipa_fltrt_copy_mem(extra_wrd_start, *buf, sz);

	rest_wrd_i = ipa_pad_to_64(rest_wrd_i);
	sz = rest_wrd_i - rest_wrd_start;
	IPAHAL_DBG_LOW("non extra words params sz %d\n", sz);
	*buf = ipa_fltrt_copy_mem(rest_wrd_start, *buf, sz);

fail_err_check:
	kfree(rest_wrd_buf);
fail_rest_alloc:
	kfree(extra_wrd_buf);
fail_extra_alloc:
	return rc;
}


/**
 * ipa_fltrt_calc_extra_wrd_bytes()- Calculate the number of extra words for eq
 * @attrib: equation attribute
 *
 * Return value: 0 on success, negative otherwise
 */
static int ipa_fltrt_calc_extra_wrd_bytes(
	const struct ipa_ipfltri_rule_eq *attrib)
{
	int num = 0;

	if (attrib->tos_eq_present)
		num++;
	if (attrib->protocol_eq_present)
		num++;
	if (attrib->tc_eq_present)
		num++;
	num += attrib->num_offset_meq_128;
	num += attrib->num_offset_meq_32;
	num += attrib->num_ihl_offset_meq_32;
	num += attrib->num_ihl_offset_range_16;
	if (attrib->ihl_offset_eq_32_present)
		num++;
	if (attrib->ihl_offset_eq_16_present)
		num++;

	IPAHAL_DBG_LOW("extra bytes number %d\n", num);

	return num;
}

static int ipa_fltrt_generate_hw_rule_bdy_from_eq(
		const struct ipa_ipfltri_rule_eq *attrib, u8 **buf)
{
	int num_offset_meq_32 = attrib->num_offset_meq_32;
	int num_ihl_offset_range_16 = attrib->num_ihl_offset_range_16;
	int num_ihl_offset_meq_32 = attrib->num_ihl_offset_meq_32;
	int num_offset_meq_128 = attrib->num_offset_meq_128;
	int i;
	int extra_bytes;
	u8 *extra;
	u8 *rest;

	extra_bytes = ipa_fltrt_calc_extra_wrd_bytes(attrib);
	/* only 3 eq does not have extra word param, 13 out of 16 is the number
	 * of equations that needs extra word param
	 */
	if (extra_bytes > 13) {
		IPAHAL_ERR("too much extra bytes\n");
		return -EPERM;
	} else if (extra_bytes > IPA3_0_HW_TBL_HDR_WIDTH) {
		/* two extra words */
		extra = *buf;
		rest = *buf + IPA3_0_HW_TBL_HDR_WIDTH * 2;
	} else if (extra_bytes > 0) {
		/* single exra word */
		extra = *buf;
		rest = *buf + IPA3_0_HW_TBL_HDR_WIDTH;
	} else {
		/* no extra words */
		extra = NULL;
		rest = *buf;
	}

	if (attrib->tos_eq_present)
		extra = ipa_write_8(attrib->tos_eq, extra);

	if (attrib->protocol_eq_present)
		extra = ipa_write_8(attrib->protocol_eq, extra);

	if (attrib->tc_eq_present)
		extra = ipa_write_8(attrib->tc_eq, extra);

	if (num_offset_meq_128) {
		extra = ipa_write_8(attrib->offset_meq_128[0].offset, extra);
		for (i = 0; i < 8; i++)
			rest = ipa_write_8(attrib->offset_meq_128[0].mask[i],
				rest);
		for (i = 0; i < 8; i++)
			rest = ipa_write_8(attrib->offset_meq_128[0].value[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa_write_8(attrib->offset_meq_128[0].mask[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa_write_8(attrib->offset_meq_128[0].value[i],
				rest);
		num_offset_meq_128--;
	}

	if (num_offset_meq_128) {
		extra = ipa_write_8(attrib->offset_meq_128[1].offset, extra);
		for (i = 0; i < 8; i++)
			rest = ipa_write_8(attrib->offset_meq_128[1].mask[i],
				rest);
		for (i = 0; i < 8; i++)
			rest = ipa_write_8(attrib->offset_meq_128[1].value[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa_write_8(attrib->offset_meq_128[1].mask[i],
				rest);
		for (i = 8; i < 16; i++)
			rest = ipa_write_8(attrib->offset_meq_128[1].value[i],
				rest);
		num_offset_meq_128--;
	}

	if (num_offset_meq_32) {
		extra = ipa_write_8(attrib->offset_meq_32[0].offset, extra);
		rest = ipa_write_32(attrib->offset_meq_32[0].mask, rest);
		rest = ipa_write_32(attrib->offset_meq_32[0].value, rest);
		num_offset_meq_32--;
	}

	if (num_offset_meq_32) {
		extra = ipa_write_8(attrib->offset_meq_32[1].offset, extra);
		rest = ipa_write_32(attrib->offset_meq_32[1].mask, rest);
		rest = ipa_write_32(attrib->offset_meq_32[1].value, rest);
		num_offset_meq_32--;
	}

	if (num_ihl_offset_meq_32) {
		extra = ipa_write_8(attrib->ihl_offset_meq_32[0].offset,
		extra);

		rest = ipa_write_32(attrib->ihl_offset_meq_32[0].mask, rest);
		rest = ipa_write_32(attrib->ihl_offset_meq_32[0].value, rest);
		num_ihl_offset_meq_32--;
	}

	if (num_ihl_offset_meq_32) {
		extra = ipa_write_8(attrib->ihl_offset_meq_32[1].offset,
		extra);

		rest = ipa_write_32(attrib->ihl_offset_meq_32[1].mask, rest);
		rest = ipa_write_32(attrib->ihl_offset_meq_32[1].value, rest);
		num_ihl_offset_meq_32--;
	}

	if (attrib->metadata_meq32_present) {
		rest = ipa_write_32(attrib->metadata_meq32.mask, rest);
		rest = ipa_write_32(attrib->metadata_meq32.value, rest);
	}

	if (num_ihl_offset_range_16) {
		extra = ipa_write_8(attrib->ihl_offset_range_16[0].offset,
		extra);

		rest = ipa_write_16(attrib->ihl_offset_range_16[0].range_high,
				rest);
		rest = ipa_write_16(attrib->ihl_offset_range_16[0].range_low,
				rest);
		num_ihl_offset_range_16--;
	}

	if (num_ihl_offset_range_16) {
		extra = ipa_write_8(attrib->ihl_offset_range_16[1].offset,
		extra);

		rest = ipa_write_16(attrib->ihl_offset_range_16[1].range_high,
				rest);
		rest = ipa_write_16(attrib->ihl_offset_range_16[1].range_low,
				rest);
		num_ihl_offset_range_16--;
	}

	if (attrib->ihl_offset_eq_32_present) {
		extra = ipa_write_8(attrib->ihl_offset_eq_32.offset, extra);
		rest = ipa_write_32(attrib->ihl_offset_eq_32.value, rest);
	}

	if (attrib->ihl_offset_eq_16_present) {
		extra = ipa_write_8(attrib->ihl_offset_eq_16.offset, extra);
		rest = ipa_write_16(attrib->ihl_offset_eq_16.value, rest);
		rest = ipa_write_16(0, rest);
	}

	if (attrib->fl_eq_present)
		rest = ipa_write_32(attrib->fl_eq & 0xFFFFF, rest);

	extra = ipa_pad_to_64(extra);
	rest = ipa_pad_to_64(rest);
	*buf = rest;

	return 0;
}

static void ipa_flt_generate_mac_addr_eq(struct ipa_ipfltri_rule_eq *eq_atrb,
	u8 hdr_mac_addr_offset,	const uint8_t mac_addr_mask[ETH_ALEN],
	const uint8_t mac_addr[ETH_ALEN], u8 ofst_meq128)
{
	int i;

	eq_atrb->offset_meq_128[ofst_meq128].offset = hdr_mac_addr_offset;

	/* LSB MASK and ADDR */
	memset(eq_atrb->offset_meq_128[ofst_meq128].mask, 0, 8);
	memset(eq_atrb->offset_meq_128[ofst_meq128].value, 0, 8);

	/* MSB MASK and ADDR */
	memset(eq_atrb->offset_meq_128[ofst_meq128].mask + 8, 0, 2);
	for (i = 0; i <= 5; i++)
		eq_atrb->offset_meq_128[ofst_meq128].mask[15 - i] =
			mac_addr_mask[i];

	memset(eq_atrb->offset_meq_128[ofst_meq128].value + 8, 0, 2);
	for (i = 0; i <= 5; i++)
		eq_atrb->offset_meq_128[ofst_meq128].value[15 - i] =
			mac_addr[i];
}

static int ipa_flt_generate_eq_ip4(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	u16 eq_bitmap = 0;
	u16 *en_rule = &eq_bitmap;

	if (attrib->attrib_mask & IPA_FLT_TOS) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_TOS_EQ);
		eq_atrb->tos_eq_present = 1;
		eq_atrb->tos_eq = attrib->u.v4.tos;
	}

	if (attrib->attrib_mask & IPA_FLT_PROTOCOL) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_PROTOCOL_EQ);
		eq_atrb->protocol_eq_present = 1;
		eq_atrb->protocol_eq = attrib->u.v4.protocol;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -14,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -8,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -22,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -16,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		eq_atrb->offset_meq_32[ofst_meq32].offset = 0;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			attrib->tos_mask << 16;
		eq_atrb->offset_meq_32[ofst_meq32].value =
			attrib->tos_value << 16;
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		eq_atrb->offset_meq_32[ofst_meq32].offset = 12;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			attrib->u.v4.src_addr_mask;
		eq_atrb->offset_meq_32[ofst_meq32].value =
			attrib->u.v4.src_addr;
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		eq_atrb->offset_meq_32[ofst_meq32].offset = 16;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			attrib->u.v4.dst_addr_mask;
		eq_atrb->offset_meq_32[ofst_meq32].value =
			attrib->u.v4.dst_addr;
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		eq_atrb->offset_meq_32[ofst_meq32].offset = -2;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			htons(attrib->ether_type);
		eq_atrb->offset_meq_32[ofst_meq32].value =
			htons(attrib->ether_type);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->type;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 1;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->code;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask =
			0xFFFFFFFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->spi;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			IPA_METADATA_COMPARE);
		eq_atrb->metadata_meq32_present = 1;
		eq_atrb->metadata_meq32.offset = 0;
		eq_atrb->metadata_meq32.mask = attrib->meta_data_mask;
		eq_atrb->metadata_meq32.value = attrib->meta_data;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAHAL_ERR("bad src port range param\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAHAL_ERR("bad dst port range param\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_IS_FRAG);
		eq_atrb->ipv4_frag_eq_present = 1;
	}

	eq_atrb->rule_eq_bitmap = *en_rule;
	eq_atrb->num_offset_meq_32 = ofst_meq32;
	eq_atrb->num_ihl_offset_range_16 = ihl_ofst_rng16;
	eq_atrb->num_ihl_offset_meq_32 = ihl_ofst_meq32;
	eq_atrb->num_offset_meq_128 = ofst_meq128;

	return 0;
}

static int ipa_flt_generate_eq_ip6(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	u8 ofst_meq32 = 0;
	u8 ihl_ofst_rng16 = 0;
	u8 ihl_ofst_meq32 = 0;
	u8 ofst_meq128 = 0;
	u16 eq_bitmap = 0;
	u16 *en_rule = &eq_bitmap;

	if (attrib->attrib_mask & IPA_FLT_NEXT_HDR) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			IPA_PROTOCOL_EQ);
		eq_atrb->protocol_eq_present = 1;
		eq_atrb->protocol_eq = attrib->u.v6.next_hdr;
	}

	if (attrib->attrib_mask & IPA_FLT_TC) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			IPA_TC_EQ);
		eq_atrb->tc_eq_present = 1;
		eq_atrb->tc_eq = attrib->u.v6.tc;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);
		/* use the same word order as in ipa v2 */
		eq_atrb->offset_meq_128[ofst_meq128].offset = 8;
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 0)
			= attrib->u.v6.src_addr_mask[0];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 4)
			= attrib->u.v6.src_addr_mask[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 8)
			= attrib->u.v6.src_addr_mask[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
			= attrib->u.v6.src_addr_mask[3];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 0)
			= attrib->u.v6.src_addr[0];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 4)
			= attrib->u.v6.src_addr[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 8)
			= attrib->u.v6.src_addr[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
				12) = attrib->u.v6.src_addr[3];
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_ADDR) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);
		eq_atrb->offset_meq_128[ofst_meq128].offset = 24;
		/* use the same word order as in ipa v2 */
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 0)
			= attrib->u.v6.dst_addr_mask[0];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 4)
			= attrib->u.v6.dst_addr_mask[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 8)
			= attrib->u.v6.dst_addr_mask[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
			= attrib->u.v6.dst_addr_mask[3];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 0)
			= attrib->u.v6.dst_addr[0];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 4)
			= attrib->u.v6.dst_addr[1];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value + 8)
			= attrib->u.v6.dst_addr[2];
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
				12) = attrib->u.v6.dst_addr[3];
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_TOS_MASKED) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);
		eq_atrb->offset_meq_128[ofst_meq128].offset = 0;
		memset(eq_atrb->offset_meq_128[ofst_meq128].mask, 0, 12);
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].mask + 12)
			= attrib->tos_mask << 20;
		memset(eq_atrb->offset_meq_128[ofst_meq128].value, 0, 12);
		*(u32 *)(eq_atrb->offset_meq_128[ofst_meq128].value +
				12) = attrib->tos_value << 20;
		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -14 => offset of dst mac addr in Ethernet II hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -14,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_ETHER_II) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -8 => offset of src mac addr in Ethernet II hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -8,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_DST_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -22 => offset of dst mac addr in 802.3 hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -22,
			attrib->dst_mac_addr_mask, attrib->dst_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_SRC_ADDR_802_3) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq128, ofst_meq128)) {
			IPAHAL_ERR("ran out of meq128 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq128[ofst_meq128]);

		/* -16 => offset of src mac addr in 802.3 hdr */
		ipa_flt_generate_mac_addr_eq(eq_atrb, -16,
			attrib->src_mac_addr_mask, attrib->src_mac_addr,
			ofst_meq128);

		ofst_meq128++;
	}

	if (attrib->attrib_mask & IPA_FLT_MAC_ETHER_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ofst_meq32, ofst_meq32)) {
			IPAHAL_ERR("ran out of meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ofst_meq32[ofst_meq32]);
		eq_atrb->offset_meq_32[ofst_meq32].offset = -2;
		eq_atrb->offset_meq_32[ofst_meq32].mask =
			htons(attrib->ether_type);
		eq_atrb->offset_meq_32[ofst_meq32].value =
			htons(attrib->ether_type);
		ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_TYPE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->type;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_CODE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 1;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask = 0xFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->code;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_SPI) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_meq32,
			ihl_ofst_meq32)) {
			IPAHAL_ERR("ran out of ihl_meq32 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_meq32[ihl_ofst_meq32]);
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].offset = 0;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].mask =
			0xFFFFFFFF;
		eq_atrb->ihl_offset_meq_32[ihl_ofst_meq32].value =
			attrib->spi;
		ihl_ofst_meq32++;
	}

	if (attrib->attrib_mask & IPA_FLT_META_DATA) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			IPA_METADATA_COMPARE);
		eq_atrb->metadata_meq32_present = 1;
		eq_atrb->metadata_meq32.offset = 0;
		eq_atrb->metadata_meq32.mask = attrib->meta_data_mask;
		eq_atrb->metadata_meq32.value = attrib->meta_data;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_SRC_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->src_port_hi < attrib->src_port_lo) {
			IPAHAL_ERR("bad src port range param\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 0;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->src_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->src_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_DST_PORT_RANGE) {
		if (IPA_IS_RAN_OUT_OF_EQ(ipa3_0_ihl_ofst_rng16,
				ihl_ofst_rng16)) {
			IPAHAL_ERR("ran out of ihl_rng16 eq\n");
			return -EPERM;
		}
		if (attrib->dst_port_hi < attrib->dst_port_lo) {
			IPAHAL_ERR("bad dst port range param\n");
			return -EPERM;
		}
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			ipa3_0_ihl_ofst_rng16[ihl_ofst_rng16]);
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].offset = 2;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_low
			= attrib->dst_port_lo;
		eq_atrb->ihl_offset_range_16[ihl_ofst_rng16].range_high
			= attrib->dst_port_hi;
		ihl_ofst_rng16++;
	}

	if (attrib->attrib_mask & IPA_FLT_FLOW_LABEL) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(IPA_FL_EQ);
		eq_atrb->fl_eq_present = 1;
		eq_atrb->fl_eq = attrib->u.v6.flow_label;
	}

	if (attrib->attrib_mask & IPA_FLT_FRAGMENT) {
		*en_rule |= IPA_GET_RULE_EQ_BIT_PTRN(
			IPA_IS_FRAG);
		eq_atrb->ipv4_frag_eq_present = 1;
	}

	eq_atrb->rule_eq_bitmap = *en_rule;
	eq_atrb->num_offset_meq_32 = ofst_meq32;
	eq_atrb->num_ihl_offset_range_16 = ihl_ofst_rng16;
	eq_atrb->num_ihl_offset_meq_32 = ihl_ofst_meq32;
	eq_atrb->num_offset_meq_128 = ofst_meq128;

	return 0;
}

static int ipa_fltrt_parse_hw_rule_eq(u8 *addr, u32 hdr_sz,
	struct ipa_ipfltri_rule_eq *atrb, u32 *rule_size)
{
	u16 eq_bitmap;
	int extra_bytes;
	u8 *extra;
	u8 *rest;
	int i;
	u8 dummy_extra_wrd;

	if (!addr || !atrb || !rule_size) {
		IPAHAL_ERR("Input error: addr=%p atrb=%p rule_size=%p\n",
			addr, atrb, rule_size);
		return -EINVAL;
	}

	eq_bitmap = atrb->rule_eq_bitmap;

	IPAHAL_DBG_LOW("eq_bitmap=0x%x\n", eq_bitmap);

	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_TOS_EQ))
		atrb->tos_eq_present = true;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_PROTOCOL_EQ))
		atrb->protocol_eq_present = true;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_TC_EQ))
		atrb->tc_eq_present = true;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_OFFSET_MEQ128_0))
		atrb->num_offset_meq_128++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_OFFSET_MEQ128_1))
		atrb->num_offset_meq_128++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_OFFSET_MEQ32_0))
		atrb->num_offset_meq_32++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_OFFSET_MEQ32_1))
		atrb->num_offset_meq_32++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_IHL_OFFSET_MEQ32_0))
		atrb->num_ihl_offset_meq_32++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_IHL_OFFSET_MEQ32_1))
		atrb->num_ihl_offset_meq_32++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_METADATA_COMPARE))
		atrb->metadata_meq32_present = true;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_IHL_OFFSET_RANGE16_0))
		atrb->num_ihl_offset_range_16++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_IHL_OFFSET_RANGE16_1))
		atrb->num_ihl_offset_range_16++;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_IHL_OFFSET_EQ_32))
		atrb->ihl_offset_eq_32_present = true;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_IHL_OFFSET_EQ_16))
		atrb->ihl_offset_eq_16_present = true;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_FL_EQ))
		atrb->fl_eq_present = true;
	if (eq_bitmap & IPA_GET_RULE_EQ_BIT_PTRN(IPA_IS_FRAG))
		atrb->ipv4_frag_eq_present = true;

	extra_bytes = ipa_fltrt_calc_extra_wrd_bytes(atrb);
	/* only 3 eq does not have extra word param, 13 out of 16 is the number
	 * of equations that needs extra word param
	 */
	if (extra_bytes > 13) {
		IPAHAL_ERR("too much extra bytes\n");
		return -EPERM;
	} else if (extra_bytes > IPA3_0_HW_TBL_HDR_WIDTH) {
		/* two extra words */
		extra = addr + hdr_sz;
		rest = extra + IPA3_0_HW_TBL_HDR_WIDTH * 2;
	} else if (extra_bytes > 0) {
		/* single extra word */
		extra = addr + hdr_sz;
		rest = extra + IPA3_0_HW_TBL_HDR_WIDTH;
	} else {
		/* no extra words */
		dummy_extra_wrd = 0;
		extra = &dummy_extra_wrd;
		rest = addr + hdr_sz;
	}
	IPAHAL_DBG_LOW("addr=0x%p extra=0x%p rest=0x%p\n", addr, extra, rest);

	if (atrb->tos_eq_present)
		atrb->tos_eq = *extra++;
	if (atrb->protocol_eq_present)
		atrb->protocol_eq = *extra++;
	if (atrb->tc_eq_present)
		atrb->tc_eq = *extra++;

	if (atrb->num_offset_meq_128 > 0) {
		atrb->offset_meq_128[0].offset = *extra++;
		for (i = 0; i < 8; i++)
			atrb->offset_meq_128[0].mask[i] = *rest++;
		for (i = 0; i < 8; i++)
			atrb->offset_meq_128[0].value[i] = *rest++;
		for (i = 8; i < 16; i++)
			atrb->offset_meq_128[0].mask[i] = *rest++;
		for (i = 8; i < 16; i++)
			atrb->offset_meq_128[0].value[i] = *rest++;
	}
	if (atrb->num_offset_meq_128 > 1) {
		atrb->offset_meq_128[1].offset = *extra++;
		for (i = 0; i < 8; i++)
			atrb->offset_meq_128[1].mask[i] = *rest++;
		for (i = 0; i < 8; i++)
			atrb->offset_meq_128[1].value[i] = *rest++;
		for (i = 8; i < 16; i++)
			atrb->offset_meq_128[1].mask[i] = *rest++;
		for (i = 8; i < 16; i++)
			atrb->offset_meq_128[1].value[i] = *rest++;
	}

	if (atrb->num_offset_meq_32 > 0) {
		atrb->offset_meq_32[0].offset = *extra++;
		atrb->offset_meq_32[0].mask = *((u32 *)rest);
		rest += 4;
		atrb->offset_meq_32[0].value = *((u32 *)rest);
		rest += 4;
	}
	if (atrb->num_offset_meq_32 > 1) {
		atrb->offset_meq_32[1].offset = *extra++;
		atrb->offset_meq_32[1].mask = *((u32 *)rest);
		rest += 4;
		atrb->offset_meq_32[1].value = *((u32 *)rest);
		rest += 4;
	}

	if (atrb->num_ihl_offset_meq_32 > 0) {
		atrb->ihl_offset_meq_32[0].offset = *extra++;
		atrb->ihl_offset_meq_32[0].mask = *((u32 *)rest);
		rest += 4;
		atrb->ihl_offset_meq_32[0].value = *((u32 *)rest);
		rest += 4;
	}
	if (atrb->num_ihl_offset_meq_32 > 1) {
		atrb->ihl_offset_meq_32[1].offset = *extra++;
		atrb->ihl_offset_meq_32[1].mask = *((u32 *)rest);
		rest += 4;
		atrb->ihl_offset_meq_32[1].value = *((u32 *)rest);
		rest += 4;
	}

	if (atrb->metadata_meq32_present) {
		atrb->metadata_meq32.mask = *((u32 *)rest);
		rest += 4;
		atrb->metadata_meq32.value = *((u32 *)rest);
		rest += 4;
	}

	if (atrb->num_ihl_offset_range_16 > 0) {
		atrb->ihl_offset_range_16[0].offset = *extra++;
		atrb->ihl_offset_range_16[0].range_high = *((u16 *)rest);
		rest += 2;
		atrb->ihl_offset_range_16[0].range_low = *((u16 *)rest);
		rest += 2;
	}
	if (atrb->num_ihl_offset_range_16 > 1) {
		atrb->ihl_offset_range_16[1].offset = *extra++;
		atrb->ihl_offset_range_16[1].range_high = *((u16 *)rest);
		rest += 2;
		atrb->ihl_offset_range_16[1].range_low = *((u16 *)rest);
		rest += 2;
	}

	if (atrb->ihl_offset_eq_32_present) {
		atrb->ihl_offset_eq_32.offset = *extra++;
		atrb->ihl_offset_eq_32.value = *((u32 *)rest);
		rest += 4;
	}

	if (atrb->ihl_offset_eq_16_present) {
		atrb->ihl_offset_eq_16.offset = *extra++;
		atrb->ihl_offset_eq_16.value = *((u16 *)rest);
		rest += 4;
	}

	if (atrb->fl_eq_present) {
		atrb->fl_eq = *((u32 *)rest);
		atrb->fl_eq &= 0xfffff;
		rest += 4;
	}

	IPAHAL_DBG_LOW("before rule alignment rest=0x%p\n", rest);
	rest = (u8 *)(((unsigned long)rest + IPA3_0_HW_RULE_START_ALIGNMENT) &
		~IPA3_0_HW_RULE_START_ALIGNMENT);
	IPAHAL_DBG_LOW("after rule alignment  rest=0x%p\n", rest);

	*rule_size = rest - addr;
	IPAHAL_DBG_LOW("rule_size=0x%x\n", *rule_size);

	return 0;
}

static int ipa_rt_parse_hw_rule(u8 *addr, struct ipahal_rt_rule_entry *rule)
{
	struct ipa3_0_rt_rule_hw_hdr *rule_hdr;
	struct ipa_ipfltri_rule_eq *atrb;

	IPAHAL_DBG_LOW("Entry\n");

	rule_hdr = (struct ipa3_0_rt_rule_hw_hdr *)addr;
	atrb = &rule->eq_attrib;

	IPAHAL_DBG_LOW("read hdr 0x%llx\n", rule_hdr->u.word);

	if (rule_hdr->u.word == 0) {
		/* table termintator - empty table */
		rule->rule_size = 0;
		return 0;
	}

	rule->dst_pipe_idx = rule_hdr->u.hdr.pipe_dest_idx;
	if (rule_hdr->u.hdr.proc_ctx) {
		rule->hdr_type = IPAHAL_RT_RULE_HDR_PROC_CTX;
		rule->hdr_ofst = (rule_hdr->u.hdr.hdr_offset) << 5;
	} else {
		rule->hdr_type = IPAHAL_RT_RULE_HDR_RAW;
		rule->hdr_ofst = (rule_hdr->u.hdr.hdr_offset) << 2;
	}
	rule->hdr_lcl = !rule_hdr->u.hdr.system;

	rule->priority = rule_hdr->u.hdr.priority;
	rule->retain_hdr = rule_hdr->u.hdr.retain_hdr;
	rule->id = rule_hdr->u.hdr.rule_id;

	atrb->rule_eq_bitmap = rule_hdr->u.hdr.en_rule;
	return ipa_fltrt_parse_hw_rule_eq(addr, sizeof(*rule_hdr),
		atrb, &rule->rule_size);
}

static int ipa_flt_parse_hw_rule(u8 *addr, struct ipahal_flt_rule_entry *rule)
{
	struct ipa3_0_flt_rule_hw_hdr *rule_hdr;
	struct ipa_ipfltri_rule_eq *atrb;

	IPAHAL_DBG_LOW("Entry\n");

	rule_hdr = (struct ipa3_0_flt_rule_hw_hdr *)addr;
	atrb = &rule->rule.eq_attrib;

	if (rule_hdr->u.word == 0) {
		/* table termintator - empty table */
		rule->rule_size = 0;
		return 0;
	}

	switch (rule_hdr->u.hdr.action) {
	case 0x0:
		rule->rule.action = IPA_PASS_TO_ROUTING;
		break;
	case 0x1:
		rule->rule.action = IPA_PASS_TO_SRC_NAT;
		break;
	case 0x2:
		rule->rule.action = IPA_PASS_TO_DST_NAT;
		break;
	case 0x3:
		rule->rule.action = IPA_PASS_TO_EXCEPTION;
		break;
	default:
		IPAHAL_ERR("Invalid Rule Action %d\n", rule_hdr->u.hdr.action);
		WARN_ON(1);
		rule->rule.action = rule_hdr->u.hdr.action;
	}

	rule->rule.rt_tbl_idx = rule_hdr->u.hdr.rt_tbl_idx;
	rule->rule.retain_hdr = rule_hdr->u.hdr.retain_hdr;
	rule->priority = rule_hdr->u.hdr.priority;
	rule->id = rule_hdr->u.hdr.rule_id;

	atrb->rule_eq_bitmap = rule_hdr->u.hdr.en_rule;
	rule->rule.eq_attrib_type = 1;
	return ipa_fltrt_parse_hw_rule_eq(addr, sizeof(*rule_hdr),
		atrb, &rule->rule_size);
}

/*
 * ipahal_fltrt_init() - Build the FLT/RT information table
 *  See ipahal_fltrt_objs[] comments
 *
 * Note: As global variables are initialized with zero, any un-overridden
 *  register entry will be zero. By this we recognize them.
 */
int ipahal_fltrt_init(enum ipa_hw_type ipa_hw_type)
{
	struct ipahal_fltrt_obj zero_obj;
	int i;
	struct ipa_mem_buffer *mem;
	int rc = -EFAULT;

	IPAHAL_DBG("Entry - HW_TYPE=%d\n", ipa_hw_type);

	if (ipa_hw_type >= IPA_HW_MAX) {
		IPAHAL_ERR("Invalid H/W type\n");
		return -EFAULT;
	}

	memset(&zero_obj, 0, sizeof(zero_obj));
	for (i = IPA_HW_v3_0 ; i < ipa_hw_type ; i++) {
		if (!memcmp(&ipahal_fltrt_objs[i+1], &zero_obj,
			sizeof(struct ipahal_fltrt_obj))) {
			memcpy(&ipahal_fltrt_objs[i+1],
				&ipahal_fltrt_objs[i],
				sizeof(struct ipahal_fltrt_obj));
		} else {
			/*
			 * explicitly overridden FLT RT info
			 * Check validity
			 */
			if (!ipahal_fltrt_objs[i+1].tbl_width) {
				IPAHAL_ERR(
				 "Zero tbl width ipaver=%d\n",
				 i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].sysaddr_alignment) {
				IPAHAL_ERR(
				  "No tbl sysaddr alignment ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].lcladdr_alignment) {
				IPAHAL_ERR(
				  "No tbl lcladdr alignment ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].blk_sz_alignment) {
				IPAHAL_ERR(
				  "No blk sz alignment ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].rule_start_alignment) {
				IPAHAL_ERR(
				  "No rule start alignment ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].tbl_hdr_width) {
				IPAHAL_ERR(
				 "Zero tbl hdr width ipaver=%d\n",
				 i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].tbl_addr_mask) {
				IPAHAL_ERR(
				 "Zero tbl hdr width ipaver=%d\n",
				 i+1);
				WARN_ON(1);
			}
			if (ipahal_fltrt_objs[i+1].rule_id_bit_len < 2) {
				IPAHAL_ERR(
				 "Too little bits for rule_id ipaver=%d\n",
				 i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].rule_buf_size) {
				IPAHAL_ERR(
				 "zero rule buf size ipaver=%d\n",
				 i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].write_val_to_hdr) {
				IPAHAL_ERR(
				  "No write_val_to_hdr CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].create_flt_bitmap) {
				IPAHAL_ERR(
				  "No create_flt_bitmap CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].create_tbl_addr) {
				IPAHAL_ERR(
				  "No create_tbl_addr CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].parse_tbl_addr) {
				IPAHAL_ERR(
				  "No parse_tbl_addr CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].rt_generate_hw_rule) {
				IPAHAL_ERR(
				  "No rt_generate_hw_rule CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].flt_generate_hw_rule) {
				IPAHAL_ERR(
				  "No flt_generate_hw_rule CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].flt_generate_eq) {
				IPAHAL_ERR(
				  "No flt_generate_eq CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].rt_parse_hw_rule) {
				IPAHAL_ERR(
				  "No rt_parse_hw_rule CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
			if (!ipahal_fltrt_objs[i+1].flt_parse_hw_rule) {
				IPAHAL_ERR(
				  "No flt_parse_hw_rule CB ipaver=%d\n",
				  i+1);
				WARN_ON(1);
			}
		}
	}

	mem = &ipahal_ctx->empty_fltrt_tbl;

	/* setup an empty  table in system memory; This will
	 * be used, for example, to delete a rt tbl safely
	 */
	mem->size = ipahal_fltrt_objs[ipa_hw_type].tbl_width;
	mem->base = dma_alloc_coherent(ipahal_ctx->ipa_pdev, mem->size,
		&mem->phys_base, GFP_KERNEL);
	if (!mem->base) {
		IPAHAL_ERR("DMA buff alloc fail %d bytes for empty tbl\n",
			mem->size);
		return -ENOMEM;
	}

	if (mem->phys_base &
		ipahal_fltrt_objs[ipa_hw_type].sysaddr_alignment) {
		IPAHAL_ERR("Empty table buf is not address aligned 0x%pad\n",
			&mem->phys_base);
		rc = -EFAULT;
		goto clear_empty_tbl;
	}

	memset(mem->base, 0, mem->size);
	IPAHAL_DBG("empty table allocated in system memory");

	return 0;

clear_empty_tbl:
	dma_free_coherent(ipahal_ctx->ipa_pdev, mem->size, mem->base,
		mem->phys_base);
	return rc;
}

void ipahal_fltrt_destroy(void)
{
	IPAHAL_DBG("Entry\n");

	if (ipahal_ctx && ipahal_ctx->empty_fltrt_tbl.base)
		dma_free_coherent(ipahal_ctx->ipa_pdev,
			ipahal_ctx->empty_fltrt_tbl.size,
			ipahal_ctx->empty_fltrt_tbl.base,
			ipahal_ctx->empty_fltrt_tbl.phys_base);
}

/* Get the H/W table (flt/rt) header width */
u32 ipahal_get_hw_tbl_hdr_width(void)
{
	return ipahal_fltrt_objs[ipahal_ctx->hw_type].tbl_hdr_width;
}

/* Get the H/W local table (SRAM) address alignment
 * Tables headers references to local tables via offsets in SRAM
 * This function return the alignment of the offset that IPA expects
 */
u32 ipahal_get_lcl_tbl_addr_alignment(void)
{
	return ipahal_fltrt_objs[ipahal_ctx->hw_type].lcladdr_alignment;
}

/*
 * Rule priority is used to distinguish rules order
 * at the integrated table consisting from hashable and
 * non-hashable tables. Max priority are rules that once are
 * scanned by IPA, IPA will not look for further rules and use it.
 */
int ipahal_get_rule_max_priority(void)
{
	return ipahal_fltrt_objs[ipahal_ctx->hw_type].rule_max_prio;
}

/* Given a priority, calc and return the next lower one if it is in
 * legal range.
 */
int ipahal_rule_decrease_priority(int *prio)
{
	struct ipahal_fltrt_obj *obj;

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (!prio) {
		IPAHAL_ERR("Invalid Input\n");
		return -EINVAL;
	}

	/* Priority logic is reverse. 0 priority considred max priority */
	if (*prio > obj->rule_min_prio || *prio < obj->rule_max_prio) {
		IPAHAL_ERR("Invalid given priority %d\n", *prio);
		return -EINVAL;
	}

	*prio += 1;

	if (*prio > obj->rule_min_prio) {
		IPAHAL_ERR("Cannot decrease priority. Already on min\n");
		*prio -= 1;
		return -EFAULT;
	}

	return 0;
}

/* Does the given ID represents rule miss?
 * Rule miss ID, is always the max ID possible in the bit-pattern
 */
bool ipahal_is_rule_miss_id(u32 id)
{
	return (id ==
		((1U << ipahal_fltrt_objs[ipahal_ctx->hw_type].rule_id_bit_len)
		-1));
}

/* Get rule ID with high bit only asserted
 * Used e.g. to create groups of IDs according to this bit
 */
u32 ipahal_get_rule_id_hi_bit(void)
{
	return BIT(ipahal_fltrt_objs[ipahal_ctx->hw_type].rule_id_bit_len - 1);
}

/* Get the low value possible to be used for rule-id */
u32 ipahal_get_low_rule_id(void)
{
	return  ipahal_fltrt_objs[ipahal_ctx->hw_type].low_rule_id;
}

/*
 * ipahal_rt_generate_empty_img() - Generate empty route image
 *  Creates routing header buffer for the given tables number.
 *  For each table, make it point to the empty table on DDR.
 * @tbls_num: Number of tables. For each will have an entry in the header
 * @hash_hdr_size: SRAM buf size of the hash tbls hdr. Used for space check
 * @nhash_hdr_size: SRAM buf size of the nhash tbls hdr. Used for space check
 * @mem: mem object that points to DMA mem representing the hdr structure
 */
int ipahal_rt_generate_empty_img(u32 tbls_num, u32 hash_hdr_size,
	u32 nhash_hdr_size, struct ipa_mem_buffer *mem)
{
	int i;
	u64 addr;
	struct ipahal_fltrt_obj *obj;

	IPAHAL_DBG("Entry\n");

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (!tbls_num || !nhash_hdr_size || !mem) {
		IPAHAL_ERR("Input Error: tbls_num=%d nhash_hdr_sz=%d mem=%p\n",
			tbls_num, nhash_hdr_size, mem);
		return -EINVAL;
	}
	if (obj->support_hash && !hash_hdr_size) {
		IPAHAL_ERR("Input Error: hash_hdr_sz=%d\n", hash_hdr_size);
		return -EINVAL;
	}

	if (nhash_hdr_size < (tbls_num * obj->tbl_hdr_width)) {
		IPAHAL_ERR("No enough spc at non-hash hdr blk for all tbls\n");
		WARN_ON(1);
		return -EINVAL;
	}
	if (obj->support_hash &&
		(hash_hdr_size < (tbls_num * obj->tbl_hdr_width))) {
		IPAHAL_ERR("No enough spc at hash hdr blk for all tbls\n");
		WARN_ON(1);
		return -EINVAL;
	}

	mem->size = tbls_num * obj->tbl_hdr_width;
	mem->base = dma_alloc_coherent(ipahal_ctx->ipa_pdev, mem->size,
		&mem->phys_base, GFP_KERNEL);
	if (!mem->base) {
		IPAHAL_ERR("fail to alloc DMA buff of size %d\n", mem->size);
		return -ENOMEM;
	}

	addr = obj->create_tbl_addr(true,
		ipahal_ctx->empty_fltrt_tbl.phys_base);
	for (i = 0; i < tbls_num; i++)
		obj->write_val_to_hdr(addr,
			mem->base + i * obj->tbl_hdr_width);

	return 0;
}

/*
 * ipahal_flt_generate_empty_img() - Generate empty filter image
 *  Creates filter header buffer for the given tables number.
 *  For each table, make it point to the empty table on DDR.
 * @tbls_num: Number of tables. For each will have an entry in the header
 * @hash_hdr_size: SRAM buf size of the hash tbls hdr. Used for space check
 * @nhash_hdr_size: SRAM buf size of the nhash tbls hdr. Used for space check
 * @ep_bitmap: Bitmap representing the EP that has flt tables. The format
 *  should be: bit0->EP0, bit1->EP1
 *  If bitmap is zero -> create tbl without bitmap entry
 * @mem: mem object that points to DMA mem representing the hdr structure
 */
int ipahal_flt_generate_empty_img(u32 tbls_num, u32 hash_hdr_size,
	u32 nhash_hdr_size, u64 ep_bitmap, struct ipa_mem_buffer *mem)
{
	int flt_spc;
	u64 flt_bitmap;
	int i;
	u64 addr;
	struct ipahal_fltrt_obj *obj;

	IPAHAL_DBG("Entry - ep_bitmap 0x%llx\n", ep_bitmap);

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (!tbls_num || !nhash_hdr_size || !mem) {
		IPAHAL_ERR("Input Error: tbls_num=%d nhash_hdr_sz=%d mem=%p\n",
			tbls_num, nhash_hdr_size, mem);
		return -EINVAL;
	}
	if (obj->support_hash && !hash_hdr_size) {
		IPAHAL_ERR("Input Error: hash_hdr_sz=%d\n", hash_hdr_size);
		return -EINVAL;
	}

	if (obj->support_hash) {
		flt_spc = hash_hdr_size;
		/* bitmap word */
		if (ep_bitmap)
			flt_spc -= obj->tbl_hdr_width;
		flt_spc /= obj->tbl_hdr_width;
		if (tbls_num > flt_spc)  {
			IPAHAL_ERR("space for hash flt hdr is too small\n");
			WARN_ON(1);
			return -EPERM;
		}
	}

	flt_spc = nhash_hdr_size;
	/* bitmap word */
	if (ep_bitmap)
		flt_spc -= obj->tbl_hdr_width;
	flt_spc /= obj->tbl_hdr_width;
	if (tbls_num > flt_spc)  {
		IPAHAL_ERR("space for non-hash flt hdr is too small\n");
		WARN_ON(1);
		return -EPERM;
	}

	mem->size = tbls_num * obj->tbl_hdr_width;
	if (ep_bitmap)
		mem->size += obj->tbl_hdr_width;
	mem->base = dma_alloc_coherent(ipahal_ctx->ipa_pdev, mem->size,
		&mem->phys_base, GFP_KERNEL);
	if (!mem->base) {
		IPAHAL_ERR("fail to alloc DMA buff of size %d\n", mem->size);
		return -ENOMEM;
	}

	if (ep_bitmap) {
		flt_bitmap = obj->create_flt_bitmap(ep_bitmap);
		IPAHAL_DBG("flt bitmap 0x%llx\n", flt_bitmap);
		obj->write_val_to_hdr(flt_bitmap, mem->base);
	}

	addr = obj->create_tbl_addr(true,
		ipahal_ctx->empty_fltrt_tbl.phys_base);

	if (ep_bitmap) {
		for (i = 1; i <= tbls_num; i++)
			obj->write_val_to_hdr(addr,
				mem->base + i * obj->tbl_hdr_width);
	} else {
		for (i = 0; i < tbls_num; i++)
			obj->write_val_to_hdr(addr,
				mem->base + i * obj->tbl_hdr_width);
	}

	return 0;
}

/*
 * ipa_fltrt_alloc_init_tbl_hdr() - allocate and initialize buffers for
 *  flt/rt tables headers to be filled into sram. Init each table to point
 *  to empty system table
 * @params: Allocate IN and OUT params
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_fltrt_alloc_init_tbl_hdr(
	struct ipahal_fltrt_alloc_imgs_params *params)
{
	u64 addr;
	int i;
	struct ipahal_fltrt_obj *obj;

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (!params) {
		IPAHAL_ERR("Input error: params=%p\n", params);
		return -EINVAL;
	}

	params->nhash_hdr.size = params->tbls_num * obj->tbl_hdr_width;
	params->nhash_hdr.base = dma_alloc_coherent(ipahal_ctx->ipa_pdev,
		params->nhash_hdr.size,
		&params->nhash_hdr.phys_base, GFP_KERNEL);
	if (!params->nhash_hdr.base) {
		IPAHAL_ERR("fail to alloc DMA buff of size %d\n",
			params->nhash_hdr.size);
		goto nhash_alloc_fail;
	}

	if (obj->support_hash) {
		params->hash_hdr.size = params->tbls_num * obj->tbl_hdr_width;
		params->hash_hdr.base = dma_alloc_coherent(ipahal_ctx->ipa_pdev,
			params->hash_hdr.size, &params->hash_hdr.phys_base,
			GFP_KERNEL);
		if (!params->hash_hdr.base) {
			IPAHAL_ERR("fail to alloc DMA buff of size %d\n",
				params->hash_hdr.size);
			goto hash_alloc_fail;
		}
	}

	addr = obj->create_tbl_addr(true,
		ipahal_ctx->empty_fltrt_tbl.phys_base);
	for (i = 0; i < params->tbls_num; i++) {
		obj->write_val_to_hdr(addr,
			params->nhash_hdr.base + i * obj->tbl_hdr_width);
		if (obj->support_hash)
			obj->write_val_to_hdr(addr,
				params->hash_hdr.base +
				i * obj->tbl_hdr_width);
	}

	return 0;

hash_alloc_fail:
	ipahal_free_dma_mem(&params->nhash_hdr);
nhash_alloc_fail:
	return -ENOMEM;
}

/*
 * ipa_fltrt_alloc_lcl_bdy() - allocate and initialize buffers for
 *  local flt/rt tables bodies to be filled into sram
 * @params: Allocate IN and OUT params
 *
 * Return: 0 on success, negative on failure
 */
static int ipa_fltrt_alloc_lcl_bdy(
	struct ipahal_fltrt_alloc_imgs_params *params)
{
	struct ipahal_fltrt_obj *obj;

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	/* The HAL allocates larger sizes than the given effective ones
	 * for alignments and border indications
	 */
	IPAHAL_DBG_LOW("lcl tbl bdy total effective sizes: hash=%u nhash=%u\n",
		params->total_sz_lcl_hash_tbls,
		params->total_sz_lcl_nhash_tbls);

	IPAHAL_DBG_LOW("lcl tbl bdy count: hash=%u nhash=%u\n",
		params->num_lcl_hash_tbls,
		params->num_lcl_nhash_tbls);

	/* Align the sizes to coop with termination word
	 *  and H/W local table start offset alignment
	 */
	if (params->nhash_bdy.size) {
		params->nhash_bdy.size = params->total_sz_lcl_nhash_tbls;
		/* for table terminator */
		params->nhash_bdy.size += obj->tbl_width *
			params->num_lcl_nhash_tbls;
		/* align the start of local rule-set */
		params->nhash_bdy.size += obj->lcladdr_alignment *
			params->num_lcl_nhash_tbls;
		/* SRAM block size alignment */
		params->nhash_bdy.size += obj->blk_sz_alignment;
		params->nhash_bdy.size &= ~(obj->blk_sz_alignment);

		IPAHAL_DBG_LOW("nhash lcl tbl bdy total h/w size = %u\n",
			params->nhash_bdy.size);

		params->nhash_bdy.base = dma_alloc_coherent(
			ipahal_ctx->ipa_pdev, params->nhash_bdy.size,
			&params->nhash_bdy.phys_base, GFP_KERNEL);
		if (!params->nhash_bdy.base) {
			IPAHAL_ERR("fail to alloc DMA buff of size %d\n",
				params->nhash_bdy.size);
			return -ENOMEM;
		}
		memset(params->nhash_bdy.base, 0, params->nhash_bdy.size);
	}

	if (!obj->support_hash && params->hash_bdy.size) {
		IPAHAL_ERR("No HAL Hash tbls support - Will be ignored\n");
		WARN_ON(1);
	}

	if (obj->support_hash && params->hash_bdy.size) {
		params->hash_bdy.size = params->total_sz_lcl_hash_tbls;
		/* for table terminator */
		params->hash_bdy.size += obj->tbl_width *
			params->num_lcl_hash_tbls;
		/* align the start of local rule-set */
		params->hash_bdy.size += obj->lcladdr_alignment *
			params->num_lcl_hash_tbls;
		/* SRAM block size alignment */
		params->hash_bdy.size += obj->blk_sz_alignment;
		params->hash_bdy.size &= ~(obj->blk_sz_alignment);

		IPAHAL_DBG_LOW("hash lcl tbl bdy total h/w size = %u\n",
			params->hash_bdy.size);

		params->hash_bdy.base = dma_alloc_coherent(
			ipahal_ctx->ipa_pdev, params->hash_bdy.size,
			&params->hash_bdy.phys_base, GFP_KERNEL);
		if (!params->hash_bdy.base) {
			IPAHAL_ERR("fail to alloc DMA buff of size %d\n",
				params->hash_bdy.size);
			goto hash_bdy_fail;
		}
		memset(params->hash_bdy.base, 0, params->hash_bdy.size);
	}

	return 0;

hash_bdy_fail:
	if (params->nhash_bdy.size)
		ipahal_free_dma_mem(&params->nhash_bdy);

	return -ENOMEM;
}

/*
 * ipahal_fltrt_allocate_hw_tbl_imgs() - Allocate tbl images DMA structures
 *  Used usually during commit.
 *  Allocates header structures and init them to point to empty DDR table
 *  Allocate body strucutres for local bodies tables
 * @params: Parameters for IN and OUT regard the allocation.
 */
int ipahal_fltrt_allocate_hw_tbl_imgs(
	struct ipahal_fltrt_alloc_imgs_params *params)
{
	IPAHAL_DBG_LOW("Entry\n");

	/* Input validation */
	if (!params) {
		IPAHAL_ERR("Input err: no params\n");
		return -EINVAL;
	}
	if (params->ipt >= IPA_IP_MAX) {
		IPAHAL_ERR("Input err: Invalid ip type %d\n", params->ipt);
		return -EINVAL;
	}

	if (ipa_fltrt_alloc_init_tbl_hdr(params)) {
		IPAHAL_ERR("fail to alloc and init tbl hdr\n");
		return -ENOMEM;
	}

	if (ipa_fltrt_alloc_lcl_bdy(params)) {
		IPAHAL_ERR("fail to alloc tbl bodies\n");
		goto bdy_alloc_fail;
	}

	return 0;

bdy_alloc_fail:
	ipahal_free_dma_mem(&params->nhash_hdr);
	if (params->hash_hdr.size)
		ipahal_free_dma_mem(&params->hash_hdr);
	return -ENOMEM;
}

/*
 * ipahal_fltrt_allocate_hw_sys_tbl() - Allocate DMA mem for H/W flt/rt sys tbl
 * @tbl_mem: IN/OUT param. size for effective table size. Pointer, for the
 *  allocated memory.
 *
 * The size is adapted for needed alignments/borders.
 */
int ipahal_fltrt_allocate_hw_sys_tbl(struct ipa_mem_buffer *tbl_mem)
{
	struct ipahal_fltrt_obj *obj;

	IPAHAL_DBG_LOW("Entry\n");

	if (!tbl_mem) {
		IPAHAL_ERR("Input err\n");
		return -EINVAL;
	}

	if (!tbl_mem->size) {
		IPAHAL_ERR("Input err: zero table size\n");
		return -EINVAL;
	}

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	/* add word for rule-set terminator */
	tbl_mem->size += obj->tbl_width;

	tbl_mem->base = dma_alloc_coherent(ipahal_ctx->ipa_pdev, tbl_mem->size,
		&tbl_mem->phys_base, GFP_KERNEL);
	if (!tbl_mem->base) {
		IPAHAL_ERR("fail to alloc DMA buf of size %d\n",
			tbl_mem->size);
		return -ENOMEM;
	}
	if (tbl_mem->phys_base & obj->sysaddr_alignment) {
		IPAHAL_ERR("sys rt tbl address is not aligned\n");
		goto align_err;
	}

	memset(tbl_mem->base, 0, tbl_mem->size);

	return 0;

align_err:
	ipahal_free_dma_mem(tbl_mem);
	return -EPERM;
}

/*
 * ipahal_fltrt_write_addr_to_hdr() - Fill table header with table address
 *  Given table addr/offset, adapt it to IPA H/W format and write it
 *  to given header index.
 * @addr: Address or offset to be used
 * @hdr_base: base address of header structure to write the address
 * @hdr_idx: index of the address in the header structure
 * @is_sys: Is it system address or local offset
 */
int ipahal_fltrt_write_addr_to_hdr(u64 addr, void *hdr_base, u32 hdr_idx,
	bool is_sys)
{
	struct ipahal_fltrt_obj *obj;
	u64 hwaddr;
	u8 *hdr;

	IPAHAL_DBG_LOW("Entry\n");

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (!addr || !hdr_base) {
		IPAHAL_ERR("Input err: addr=0x%llx hdr_base=%p\n",
			addr, hdr_base);
		return -EINVAL;
	}

	hdr = (u8 *)hdr_base;
	hdr += hdr_idx * obj->tbl_hdr_width;
	hwaddr = obj->create_tbl_addr(is_sys, addr);
	obj->write_val_to_hdr(hwaddr, hdr);

	return 0;
}

/*
 * ipahal_fltrt_read_addr_from_hdr() - Given sram address, read it's
 *  content (physical address or offset) and parse it.
 * @hdr_base: base sram address of the header structure.
 * @hdr_idx: index of the header entry line in the header structure.
 * @addr: The parsed address - Out parameter
 * @is_sys: Is this system or local address - Out parameter
 */
int ipahal_fltrt_read_addr_from_hdr(void *hdr_base, u32 hdr_idx, u64 *addr,
	bool *is_sys)
{
	struct ipahal_fltrt_obj *obj;
	u64 hwaddr;
	u8 *hdr;

	IPAHAL_DBG_LOW("Entry\n");

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (!addr || !hdr_base || !is_sys) {
		IPAHAL_ERR("Input err: addr=%p hdr_base=%p is_sys=%p\n",
			addr, hdr_base, is_sys);
		return -EINVAL;
	}

	hdr = (u8 *)hdr_base;
	hdr += hdr_idx * obj->tbl_hdr_width;
	hwaddr = *((u64 *)hdr);
	obj->parse_tbl_addr(hwaddr, addr, is_sys);
	return 0;
}

/*
 * ipahal_rt_generate_hw_rule() - generates the routing hardware rule
 * @params: Params for the rule creation.
 * @hw_len: Size of the H/W rule to be returned
 * @buf: Buffer to build the rule in. If buf is NULL, then the rule will
 *  be built in internal temp buf. This is used e.g. to get the rule size
 *  only.
 */
int ipahal_rt_generate_hw_rule(struct ipahal_rt_rule_gen_params *params,
	u32 *hw_len, u8 *buf)
{
	struct ipahal_fltrt_obj *obj;
	u8 *tmp = NULL;
	int rc;

	IPAHAL_DBG_LOW("Entry\n");

	if (!params || !hw_len) {
		IPAHAL_ERR("Input err: params=%p hw_len=%p\n", params, hw_len);
		return -EINVAL;
	}
	if (!params->rule) {
		IPAHAL_ERR("Input err: invalid rule\n");
		return -EINVAL;
	}
	if (params->ipt >= IPA_IP_MAX) {
		IPAHAL_ERR("Input err: Invalid ip type %d\n", params->ipt);
		return -EINVAL;
	}

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (buf == NULL) {
		tmp = kzalloc(obj->rule_buf_size, GFP_KERNEL);
		if (!tmp) {
			IPAHAL_ERR("failed to alloc %u bytes\n",
				obj->rule_buf_size);
			return -ENOMEM;
		}
		buf = tmp;
	} else
		if ((long)buf & obj->rule_start_alignment) {
			IPAHAL_ERR("buff is not rule rule start aligned\n");
			return -EPERM;
		}

	rc = ipahal_fltrt_objs[ipahal_ctx->hw_type].rt_generate_hw_rule(
		params, hw_len, buf);
	if (!tmp && !rc) {
		/* write the rule-set terminator */
		memset(buf + *hw_len, 0, obj->tbl_width);
	}

	kfree(tmp);

	return rc;
}

/*
 * ipahal_flt_generate_hw_rule() - generates the filtering hardware rule.
 * @params: Params for the rule creation.
 * @hw_len: Size of the H/W rule to be returned
 * @buf: Buffer to build the rule in. If buf is NULL, then the rule will
 *  be built in internal temp buf. This is used e.g. to get the rule size
 *  only.
 */
int ipahal_flt_generate_hw_rule(struct ipahal_flt_rule_gen_params *params,
	u32 *hw_len, u8 *buf)
{
	struct ipahal_fltrt_obj *obj;
	u8 *tmp = NULL;
	int rc;

	IPAHAL_DBG_LOW("Entry\n");

	if (!params || !hw_len) {
		IPAHAL_ERR("Input err: params=%p hw_len=%p\n", params, hw_len);
		return -EINVAL;
	}
	if (!params->rule) {
		IPAHAL_ERR("Input err: invalid rule\n");
		return -EINVAL;
	}
	if (params->ipt >= IPA_IP_MAX) {
		IPAHAL_ERR("Input err: Invalid ip type %d\n", params->ipt);
		return -EINVAL;
	}

	obj = &ipahal_fltrt_objs[ipahal_ctx->hw_type];

	if (buf == NULL) {
		tmp = kzalloc(obj->rule_buf_size, GFP_KERNEL);
		if (!tmp) {
			IPAHAL_ERR("failed to alloc %u bytes\n",
				obj->rule_buf_size);
			return -ENOMEM;
		}
		buf = tmp;
	} else
		if ((long)buf & obj->rule_start_alignment) {
			IPAHAL_ERR("buff is not rule rule start aligned\n");
			return -EPERM;
		}

	rc = ipahal_fltrt_objs[ipahal_ctx->hw_type].flt_generate_hw_rule(
		params, hw_len, buf);
	if (!tmp && !rc) {
		/* write the rule-set terminator */
		memset(buf + *hw_len, 0, obj->tbl_width);
	}

	kfree(tmp);

	return rc;

}

/*
 * ipahal_flt_generate_equation() - generate flt rule in equation form
 *  Will build equation form flt rule from given info.
 * @ipt: IP family
 * @attrib: Rule attribute to be generated
 * @eq_atrb: Equation form generated rule
 * Note: Usage example: Pass the generated form to other sub-systems
 *  for inter-subsystems rules exchange.
 */
int ipahal_flt_generate_equation(enum ipa_ip_type ipt,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_atrb)
{
	IPAHAL_DBG_LOW("Entry\n");

	if (ipt >= IPA_IP_MAX) {
		IPAHAL_ERR("Input err: Invalid ip type %d\n", ipt);
		return -EINVAL;
	}

	if (!attrib || !eq_atrb) {
		IPAHAL_ERR("Input err: attrib=%p eq_atrb=%p\n",
			attrib, eq_atrb);
		return -EINVAL;
	}

	return ipahal_fltrt_objs[ipahal_ctx->hw_type].flt_generate_eq(ipt,
		attrib, eq_atrb);

}

/*
 * ipahal_rt_parse_hw_rule() - Parse H/W formated rt rule
 *  Given the rule address, read the rule info from H/W and parse it.
 * @rule_addr: Rule address (virtual memory)
 * @rule: Out parameter for parsed rule info
 */
int ipahal_rt_parse_hw_rule(u8 *rule_addr,
	struct ipahal_rt_rule_entry *rule)
{
	IPAHAL_DBG_LOW("Entry\n");

	if (!rule_addr || !rule) {
		IPAHAL_ERR("Input err: rule_addr=%p rule=%p\n",
			rule_addr, rule);
		return -EINVAL;
	}

	return ipahal_fltrt_objs[ipahal_ctx->hw_type].rt_parse_hw_rule(
		rule_addr, rule);
}

/*
 * ipahal_flt_parse_hw_rule() - Parse H/W formated flt rule
 *  Given the rule address, read the rule info from H/W and parse it.
 * @rule_addr: Rule address (virtual memory)
 * @rule: Out parameter for parsed rule info
 */
int ipahal_flt_parse_hw_rule(u8 *rule_addr,
	struct ipahal_flt_rule_entry *rule)
{
	IPAHAL_DBG_LOW("Entry\n");

	if (!rule_addr || !rule) {
		IPAHAL_ERR("Input err: rule_addr=%p rule=%p\n",
			rule_addr, rule);
		return -EINVAL;
	}

	return ipahal_fltrt_objs[ipahal_ctx->hw_type].flt_parse_hw_rule(
		rule_addr, rule);
}

