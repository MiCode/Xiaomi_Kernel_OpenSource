/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include "ipahal.h"
#include "ipahal_hw_stats.h"
#include "ipahal_hw_stats_i.h"
#include "ipahal_i.h"

struct ipahal_hw_stats_obj {
	struct ipahal_stats_init_pyld *(*generate_init_pyld)(void *params,
		bool is_atomic_ctx);
	int (*get_offset)(void *params, struct ipahal_stats_offset *out);
	int (*parse_stats)(void *init_params, void *raw_stats,
		void *parsed_stats);
};

static int _count_ones(u32 number)
{
	int count = 0;

	while (number) {
		count++;
		number = number & (number - 1);
	}

	return count;
}

static struct ipahal_stats_init_pyld *ipahal_generate_init_pyld_quota(
	void *params, bool is_atomic_ctx)
{
	struct ipahal_stats_init_pyld *pyld;
	struct ipahal_stats_init_quota *in =
		(struct ipahal_stats_init_quota *)params;
	int entries = _count_ones(in->enabled_bitmask);

	IPAHAL_DBG_LOW("entries = %d\n", entries);
	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) +
		entries * sizeof(struct ipahal_stats_quota_hw), is_atomic_ctx);
	if (!pyld) {
		IPAHAL_ERR("no mem\n");
		return NULL;
	}

	pyld->len = entries * sizeof(struct ipahal_stats_quota_hw);
	return pyld;
}

static int ipahal_get_offset_quota(void *params,
	struct ipahal_stats_offset *out)
{
	struct ipahal_stats_get_offset_quota *in =
		(struct ipahal_stats_get_offset_quota *)params;
	int entries = _count_ones(in->init.enabled_bitmask);

	IPAHAL_DBG_LOW("\n");
	out->offset = 0;
	out->size = entries * sizeof(struct ipahal_stats_quota_hw);

	return 0;
}

static int ipahal_parse_stats_quota(void *init_params, void *raw_stats,
	void *parsed_stats)
{
	struct ipahal_stats_init_quota *init =
		(struct ipahal_stats_init_quota *)init_params;
	struct ipahal_stats_quota_hw *raw_hw =
		(struct ipahal_stats_quota_hw *)raw_stats;
	struct ipahal_stats_quota_all *out =
		(struct ipahal_stats_quota_all *)parsed_stats;
	int stat_idx = 0;
	int i;

	memset(out, 0, sizeof(*out));
	IPAHAL_DBG_LOW("\n");
	for (i = 0; i < IPAHAL_MAX_PIPES; i++) {
		if (init->enabled_bitmask & (1 << i)) {
			IPAHAL_DBG_LOW("pipe %d stat_idx %d\n", i, stat_idx);
			out->stats[i].num_ipv4_bytes =
				raw_hw[stat_idx].num_ipv4_bytes;
			out->stats[i].num_ipv4_pkts =
				raw_hw[stat_idx].num_ipv4_pkts;
			out->stats[i].num_ipv6_pkts =
				raw_hw[stat_idx].num_ipv6_pkts;
			out->stats[i].num_ipv6_bytes =
				raw_hw[stat_idx].num_ipv6_bytes;
			stat_idx++;
		}
	}

	return 0;
}

static struct ipahal_stats_init_pyld *ipahal_generate_init_pyld_tethering(
	void *params, bool is_atomic_ctx)
{
	struct ipahal_stats_init_pyld *pyld;
	struct ipahal_stats_init_tethering *in =
		(struct ipahal_stats_init_tethering *)params;
	int hdr_entries = _count_ones(in->prod_bitmask);
	int entries = 0;
	int i;
	void *pyld_ptr;
	u32 incremental_offset;

	IPAHAL_DBG_LOW("prod entries = %d\n", hdr_entries);
	for (i = 0; i < sizeof(in->prod_bitmask) * 8; i++) {
		if (in->prod_bitmask & (1 << i)) {
			if (in->cons_bitmask[i] == 0) {
				IPAHAL_ERR("no cons bitmask for prod %d\n", i);
				return NULL;
			}
			entries += _count_ones(in->cons_bitmask[i]);
		}
	}
	IPAHAL_DBG_LOW("sum all entries = %d\n", entries);

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) +
		hdr_entries * sizeof(struct ipahal_stats_tethering_hdr_hw) +
		entries * sizeof(struct ipahal_stats_tethering_hw),
		is_atomic_ctx);
	if (!pyld)
		return NULL;

	pyld->len = hdr_entries * sizeof(struct ipahal_stats_tethering_hdr_hw) +
		entries * sizeof(struct ipahal_stats_tethering_hw);

	pyld_ptr = pyld->data;
	incremental_offset =
		(hdr_entries * sizeof(struct ipahal_stats_tethering_hdr_hw))
			/ 8;
	for (i = 0; i < sizeof(in->prod_bitmask) * 8; i++) {
		if (in->prod_bitmask & (1 << i)) {
			struct ipahal_stats_tethering_hdr_hw *hdr = pyld_ptr;

			hdr->dst_mask = in->cons_bitmask[i];
			hdr->offset = incremental_offset;
			IPAHAL_DBG_LOW("hdr->dst_mask=0x%x\n", hdr->dst_mask);
			IPAHAL_DBG_LOW("hdr->offset=0x%x\n", hdr->offset);
			/* add the stats entry */
			incremental_offset += _count_ones(in->cons_bitmask[i]) *
				sizeof(struct ipahal_stats_tethering_hw) / 8;
			pyld_ptr += sizeof(*hdr);
		}
	}

	return pyld;
}

static int ipahal_get_offset_tethering(void *params,
	struct ipahal_stats_offset *out)
{
	struct ipahal_stats_get_offset_tethering *in =
		(struct ipahal_stats_get_offset_tethering *)params;
	int entries = 0;
	int i;

	for (i = 0; i < sizeof(in->init.prod_bitmask) * 8; i++) {
		if (in->init.prod_bitmask & (1 << i)) {
			if (in->init.cons_bitmask[i] == 0) {
				IPAHAL_ERR("no cons bitmask for prod %d\n", i);
				return -EPERM;
			}
			entries += _count_ones(in->init.cons_bitmask[i]);
		}
	}
	IPAHAL_DBG_LOW("sum all entries = %d\n", entries);

	/* skip the header */
	out->offset = _count_ones(in->init.prod_bitmask) *
		sizeof(struct ipahal_stats_tethering_hdr_hw);
	out->size = entries * sizeof(struct ipahal_stats_tethering_hw);

	return 0;
}

static int ipahal_parse_stats_tethering(void *init_params, void *raw_stats,
	void *parsed_stats)
{
	struct ipahal_stats_init_tethering *init =
		(struct ipahal_stats_init_tethering *)init_params;
	struct ipahal_stats_tethering_hw *raw_hw =
		(struct ipahal_stats_tethering_hw *)raw_stats;
	struct ipahal_stats_tethering_all *out =
		(struct ipahal_stats_tethering_all *)parsed_stats;
	int i, j;
	int stat_idx = 0;

	memset(out, 0, sizeof(*out));
	IPAHAL_DBG_LOW("\n");
	for (i = 0; i < IPAHAL_MAX_PIPES; i++) {
		for (j = 0; j < IPAHAL_MAX_PIPES; j++) {
			if ((init->prod_bitmask & (1 << i)) &&
			    init->cons_bitmask[i] & (1 << j)) {
				IPAHAL_DBG_LOW("prod %d cons %d\n", i, j);
				IPAHAL_DBG_LOW("stat_idx %d\n", stat_idx);
				out->stats[i][j].num_ipv4_bytes =
					raw_hw[stat_idx].num_ipv4_bytes;
				IPAHAL_DBG_LOW("num_ipv4_bytes %lld\n",
					out->stats[i][j].num_ipv4_bytes);
				out->stats[i][j].num_ipv4_pkts =
					raw_hw[stat_idx].num_ipv4_pkts;
				IPAHAL_DBG_LOW("num_ipv4_pkts %lld\n",
					out->stats[i][j].num_ipv4_pkts);
				out->stats[i][j].num_ipv6_pkts =
					raw_hw[stat_idx].num_ipv6_pkts;
				IPAHAL_DBG_LOW("num_ipv6_pkts %lld\n",
					out->stats[i][j].num_ipv6_pkts);
				out->stats[i][j].num_ipv6_bytes =
					raw_hw[stat_idx].num_ipv6_bytes;
				IPAHAL_DBG_LOW("num_ipv6_bytes %lld\n",
					out->stats[i][j].num_ipv6_bytes);
				stat_idx++;
			}
		}
	}

	return 0;
}

static struct ipahal_stats_init_pyld *ipahal_generate_init_pyld_flt_rt_v4_5(
	void *params, bool is_atomic_ctx)
{
	struct ipahal_stats_init_pyld *pyld;
	long int num = (long int)(params);

	if (num > IPA_MAX_FLT_RT_CNT_INDEX ||
		num <= 0) {
		IPAHAL_ERR("num %ld not valid\n", num);
		return NULL;
	}
	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) +
		num *
		sizeof(struct ipahal_stats_flt_rt_v4_5_hw),
		is_atomic_ctx);
	if (!pyld) {
		IPAHAL_ERR("no mem\n");
		return NULL;
	}
	pyld->len = num *
		sizeof(struct ipahal_stats_flt_rt_v4_5_hw);
	return pyld;
}

static int ipahal_get_offset_flt_rt_v4_5(void *params,
	struct ipahal_stats_offset *out)
{
	struct ipahal_stats_get_offset_flt_rt_v4_5 *in =
		(struct ipahal_stats_get_offset_flt_rt_v4_5 *)params;
	int num;

	out->offset = (in->start_id - 1) *
		sizeof(struct ipahal_stats_flt_rt_v4_5);
	num = in->end_id - in->start_id + 1;
	out->size = num * sizeof(struct ipahal_stats_flt_rt_v4_5);

	return 0;
}

static int ipahal_parse_stats_flt_rt_v4_5(void *init_params,
	void *raw_stats, void *parsed_stats)
{
	struct ipahal_stats_flt_rt_v4_5_hw *raw_hw =
		(struct ipahal_stats_flt_rt_v4_5_hw *)raw_stats;
	struct ipa_ioc_flt_rt_query *query =
		(struct ipa_ioc_flt_rt_query *)parsed_stats;
	int num, i;

	num = query->end_id - query->start_id + 1;
	IPAHAL_DBG_LOW("\n");
	for (i = 0; i < num; i++) {
		((struct ipa_flt_rt_stats *)
		query->stats)[i].num_bytes =
			raw_hw[i].num_bytes;
		((struct ipa_flt_rt_stats *)
		query->stats)[i].num_pkts_hash =
			raw_hw[i].num_packets_hash;
		((struct ipa_flt_rt_stats *)
		query->stats)[i].num_pkts =
			raw_hw[i].num_packets;
	}

	return 0;
}


static struct ipahal_stats_init_pyld *ipahal_generate_init_pyld_flt_rt(
	void *params, bool is_atomic_ctx)
{
	struct ipahal_stats_init_pyld *pyld;
	struct ipahal_stats_init_flt_rt *in =
		(struct ipahal_stats_init_flt_rt *)params;
	int hdr_entries;
	int num_rules = 0;
	int i, start_entry;
	void *pyld_ptr;
	u32 incremental_offset;

	for (i = 0; i < IPAHAL_MAX_RULE_ID_32; i++)
		num_rules += _count_ones(in->rule_id_bitmask[i]);

	if (num_rules == 0) {
		IPAHAL_ERR("no rule ids provided\n");
		return NULL;
	}
	IPAHAL_DBG_LOW("num_rules = %d\n", num_rules);

	hdr_entries = IPAHAL_MAX_RULE_ID_32;
	for (i = 0; i < IPAHAL_MAX_RULE_ID_32; i++) {
		if (in->rule_id_bitmask[i] != 0)
			break;
		hdr_entries--;
	}
	start_entry = i;

	for (i = IPAHAL_MAX_RULE_ID_32 - 1; i >= start_entry; i--) {
		if (in->rule_id_bitmask[i] != 0)
			break;
		hdr_entries--;
	}
	IPAHAL_DBG_LOW("hdr_entries = %d\n", hdr_entries);

	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) +
		hdr_entries * sizeof(struct ipahal_stats_flt_rt_hdr_hw) +
		num_rules * sizeof(struct ipahal_stats_flt_rt_hw),
		is_atomic_ctx);
	if (!pyld) {
		IPAHAL_ERR("no mem\n");
		return NULL;
	}

	pyld->len = hdr_entries * sizeof(struct ipahal_stats_flt_rt_hdr_hw) +
		num_rules * sizeof(struct ipahal_stats_flt_rt_hw);

	pyld_ptr = pyld->data;
	incremental_offset =
		(hdr_entries * sizeof(struct ipahal_stats_flt_rt_hdr_hw))
			/ 8;
	for (i = start_entry; i < hdr_entries; i++) {
		struct ipahal_stats_flt_rt_hdr_hw *hdr = pyld_ptr;

		hdr->en_mask = in->rule_id_bitmask[i];
		hdr->cnt_offset = incremental_offset;
		/* add the stats entry */
		incremental_offset += _count_ones(in->rule_id_bitmask[i]) *
			sizeof(struct ipahal_stats_flt_rt_hw) / 8;
		pyld_ptr += sizeof(*hdr);
	}

	return pyld;
}

static int ipahal_get_offset_flt_rt(void *params,
	struct ipahal_stats_offset *out)
{
	struct ipahal_stats_get_offset_flt_rt *in =
		(struct ipahal_stats_get_offset_flt_rt *)params;
	int i;
	int hdr_entries;
	int skip_rules = 0;
	int start_entry;
	int rule_bit = in->rule_id % 32;
	int rule_idx = in->rule_id / 32;

	if (rule_idx >= IPAHAL_MAX_RULE_ID_32) {
		IPAHAL_ERR("invalid rule_id %d\n", in->rule_id);
		return -EPERM;
	}

	hdr_entries = IPAHAL_MAX_RULE_ID_32;
	for (i = 0; i < IPAHAL_MAX_RULE_ID_32; i++) {
		if (in->init.rule_id_bitmask[i] != 0)
			break;
		hdr_entries--;
	}

	if (hdr_entries == 0) {
		IPAHAL_ERR("no rule ids provided\n");
		return -EPERM;
	}
	start_entry = i;

	for (i = IPAHAL_MAX_RULE_ID_32 - 1; i >= 0; i--) {
		if (in->init.rule_id_bitmask[i] != 0)
			break;
		hdr_entries--;
	}
	IPAHAL_DBG_LOW("hdr_entries = %d\n", hdr_entries);

	/* skip the header */
	out->offset = hdr_entries * sizeof(struct ipahal_stats_flt_rt_hdr_hw);

	/* skip the previous rules  */
	for (i = start_entry; i < rule_idx; i++)
		skip_rules += _count_ones(in->init.rule_id_bitmask[i]);

	for (i = 0; i < rule_bit; i++)
		if (in->init.rule_id_bitmask[rule_idx] & (1 << i))
			skip_rules++;

	out->offset += skip_rules * sizeof(struct ipahal_stats_flt_rt_hw);
	out->size = sizeof(struct ipahal_stats_flt_rt_hw);

	return 0;
}

static int ipahal_parse_stats_flt_rt(void *init_params, void *raw_stats,
	void *parsed_stats)
{
	struct ipahal_stats_flt_rt_hw *raw_hw =
		(struct ipahal_stats_flt_rt_hw *)raw_stats;
	struct ipahal_stats_flt_rt *out =
		(struct ipahal_stats_flt_rt *)parsed_stats;

	memset(out, 0, sizeof(*out));
	IPAHAL_DBG_LOW("\n");
	out->num_packets = raw_hw->num_packets;
	out->num_packets_hash = raw_hw->num_packets_hash;

	return 0;
}

static struct ipahal_stats_init_pyld *ipahal_generate_init_pyld_drop(
	void *params, bool is_atomic_ctx)
{
	struct ipahal_stats_init_pyld *pyld;
	struct ipahal_stats_init_drop *in =
		(struct ipahal_stats_init_drop *)params;
	int entries = _count_ones(in->enabled_bitmask);

	IPAHAL_DBG_LOW("entries = %d\n", entries);
	pyld = IPAHAL_MEM_ALLOC(sizeof(*pyld) +
		entries * sizeof(struct ipahal_stats_drop_hw), is_atomic_ctx);
	if (!pyld)
		return NULL;

	pyld->len = entries * sizeof(struct ipahal_stats_drop_hw);

	return pyld;
}

static int ipahal_get_offset_drop(void *params,
	struct ipahal_stats_offset *out)
{
	struct ipahal_stats_get_offset_drop *in =
		(struct ipahal_stats_get_offset_drop *)params;
	int entries = _count_ones(in->init.enabled_bitmask);

	IPAHAL_DBG_LOW("\n");
	out->offset = 0;
	out->size = entries * sizeof(struct ipahal_stats_drop_hw);

	return 0;
}

static int ipahal_parse_stats_drop(void *init_params, void *raw_stats,
	void *parsed_stats)
{
	struct ipahal_stats_init_drop *init =
		(struct ipahal_stats_init_drop *)init_params;
	struct ipahal_stats_drop_hw *raw_hw =
		(struct ipahal_stats_drop_hw *)raw_stats;
	struct ipahal_stats_drop_all *out =
		(struct ipahal_stats_drop_all *)parsed_stats;
	int stat_idx = 0;
	int i;

	memset(out, 0, sizeof(*out));
	IPAHAL_DBG_LOW("\n");
	for (i = 0; i < IPAHAL_MAX_PIPES; i++) {
		if (init->enabled_bitmask & (1 << i)) {
			out->stats[i].drop_byte_cnt =
				raw_hw[stat_idx].drop_byte_cnt;
			out->stats[i].drop_packet_cnt =
				raw_hw[stat_idx].drop_packet_cnt;
			stat_idx++;
		}
	}

	return 0;
}

static struct ipahal_hw_stats_obj
	ipahal_hw_stats_objs[IPA_HW_MAX][IPAHAL_HW_STATS_MAX] = {
	/* IPAv4 */
	[IPA_HW_v4_0][IPAHAL_HW_STATS_QUOTA] = {
		ipahal_generate_init_pyld_quota,
		ipahal_get_offset_quota,
		ipahal_parse_stats_quota
	},
	[IPA_HW_v4_0][IPAHAL_HW_STATS_TETHERING] = {
		ipahal_generate_init_pyld_tethering,
		ipahal_get_offset_tethering,
		ipahal_parse_stats_tethering
	},
	[IPA_HW_v4_0][IPAHAL_HW_STATS_FNR] = {
		ipahal_generate_init_pyld_flt_rt,
		ipahal_get_offset_flt_rt,
		ipahal_parse_stats_flt_rt
	},
	[IPA_HW_v4_0][IPAHAL_HW_STATS_DROP] = {
		ipahal_generate_init_pyld_drop,
		ipahal_get_offset_drop,
		ipahal_parse_stats_drop
	},
	[IPA_HW_v4_5][IPAHAL_HW_STATS_QUOTA] = {
		ipahal_generate_init_pyld_quota,
		ipahal_get_offset_quota,
		ipahal_parse_stats_quota
	},
	[IPA_HW_v4_5][IPAHAL_HW_STATS_FNR] = {
		ipahal_generate_init_pyld_flt_rt_v4_5,
		ipahal_get_offset_flt_rt_v4_5,
		ipahal_parse_stats_flt_rt_v4_5
	},
	[IPA_HW_v4_5][IPAHAL_HW_STATS_TETHERING] = {
		ipahal_generate_init_pyld_tethering,
		ipahal_get_offset_tethering,
		ipahal_parse_stats_tethering
	},
	[IPA_HW_v4_5][IPAHAL_HW_STATS_DROP] = {
		ipahal_generate_init_pyld_drop,
		ipahal_get_offset_drop,
		ipahal_parse_stats_drop
	},
};

int ipahal_hw_stats_init(enum ipa_hw_type ipa_hw_type)
{
	int i;
	int j;
	struct ipahal_hw_stats_obj zero_obj;
	struct ipahal_hw_stats_obj *hw_stat_ptr;

	IPAHAL_DBG_LOW("Entry - HW_TYPE=%d\n", ipa_hw_type);

	if ((ipa_hw_type < 0) || (ipa_hw_type >= IPA_HW_MAX)) {
		IPAHAL_ERR("invalid IPA HW type (%d)\n", ipa_hw_type);
		return -EINVAL;
	}

	memset(&zero_obj, 0, sizeof(zero_obj));
	for (i = IPA_HW_v4_0 ; i < ipa_hw_type ; i++) {
		for (j = 0; j < IPAHAL_HW_STATS_MAX; j++) {
			if (!memcmp(&ipahal_hw_stats_objs[i + 1][j], &zero_obj,
				sizeof(struct ipahal_hw_stats_obj))) {
				memcpy(&ipahal_hw_stats_objs[i + 1][j],
					&ipahal_hw_stats_objs[i][j],
					sizeof(struct ipahal_hw_stats_obj));
			} else {
				/*
				 * explicitly overridden stat.
				 * Check validity
				 */
				hw_stat_ptr = &ipahal_hw_stats_objs[i + 1][j];
				if (!hw_stat_ptr->get_offset) {
					IPAHAL_ERR(
					  "stat=%d get_offset null ver=%d\n",
					  j, i+1);
					WARN_ON(1);
				}
				if (!hw_stat_ptr->parse_stats) {
					IPAHAL_ERR(
					  "stat=%d parse_stats null ver=%d\n",
						j, i + 1);
					WARN_ON(1);
				}
			}
		}
	}

	return 0;
}

int ipahal_stats_get_offset(enum ipahal_hw_stats_type type, void *params,
	struct ipahal_stats_offset *out)
{
	if (type < 0 || type >= IPAHAL_HW_STATS_MAX) {
		IPAHAL_ERR("Invalid type stat=%d\n", type);
		WARN_ON(1);
		return -EFAULT;
	}

	if (!params || !out) {
		IPAHAL_ERR("Null arg\n");
		WARN_ON(1);
		return -EFAULT;
	}

	return ipahal_hw_stats_objs[ipahal_ctx->hw_type][type].get_offset(
		params, out);
}

struct ipahal_stats_init_pyld *ipahal_stats_generate_init_pyld(
	enum ipahal_hw_stats_type type, void *params, bool is_atomic_ctx)
{
	struct ipahal_hw_stats_obj *hw_obj_ptr;

	if (type < 0 || type >= IPAHAL_HW_STATS_MAX) {
		IPAHAL_ERR("Invalid type stat=%d\n", type);
		WARN_ON(1);
		return NULL;
	}

	hw_obj_ptr = &ipahal_hw_stats_objs[ipahal_ctx->hw_type][type];
	return hw_obj_ptr->generate_init_pyld(params, is_atomic_ctx);
}

int ipahal_parse_stats(enum ipahal_hw_stats_type type, void *init_params,
	void *raw_stats, void *parsed_stats)
{
	if (WARN((type < 0 || type >= IPAHAL_HW_STATS_MAX),
		"Invalid type stat = %d\n", type))
		return -EFAULT;

	if (WARN((!raw_stats || !parsed_stats), "Null arg\n"))
		return -EFAULT;

	return ipahal_hw_stats_objs[ipahal_ctx->hw_type][type].parse_stats(
		init_params, raw_stats, parsed_stats);
}
