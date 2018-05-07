/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/qmi_encdec.h>
#include <linux/ipa_qmi_service_v01.h>

#include <soc/qcom/msm_qmi_interface.h>

#include "ipa_qmi_service.h"

/* Type Definitions  */
static struct elem_info ipa3_hdr_tbl_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_hdr_tbl_info_type_v01,
					modem_offset_start),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_hdr_tbl_info_type_v01,
					modem_offset_end),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_route_tbl_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_route_tbl_info_type_v01,
					route_tbl_start_addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_route_tbl_info_type_v01,
					num_indices),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_modem_mem_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_modem_mem_info_type_v01,
					block_start_addr),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_modem_mem_info_type_v01,
					size),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_hdr_proc_ctx_tbl_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_hdr_proc_ctx_tbl_info_type_v01,
			modem_offset_start),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_hdr_proc_ctx_tbl_info_type_v01,
			modem_offset_end),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_zip_tbl_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_zip_tbl_info_type_v01,
					modem_offset_start),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_zip_tbl_info_type_v01,
					modem_offset_end),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_ipfltr_range_eq_16_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_range_eq_16_type_v01,
			offset),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_range_eq_16_type_v01,
			range_low),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_range_eq_16_type_v01,
			range_high),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_ipfltr_mask_eq_32_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
				struct ipa_ipfltr_mask_eq_32_type_v01,
				offset),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
				struct ipa_ipfltr_mask_eq_32_type_v01,
				mask),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_mask_eq_32_type_v01,
			value),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_ipfltr_eq_16_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_eq_16_type_v01,
			offset),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_ipfltr_eq_16_type_v01,
					value),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_ipfltr_eq_32_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_ipfltr_eq_32_type_v01,
					offset),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_ipfltr_eq_32_type_v01,
					value),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_ipfltr_mask_eq_128_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_mask_eq_128_type_v01,
			offset),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 16,
		.elem_size	= sizeof(uint8_t),
		.is_array	= STATIC_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_mask_eq_128_type_v01,
			mask),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 16,
		.elem_size	= sizeof(uint8_t),
		.is_array	= STATIC_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ipfltr_mask_eq_128_type_v01,
			value),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_filter_rule_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_filter_rule_type_v01,
			rule_eq_bitmap),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_filter_rule_type_v01,
			tos_eq_present),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					tos_eq),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					protocol_eq_present),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					protocol_eq),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					num_ihl_offset_range_16),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_IPFLTR_NUM_IHL_RANGE_16_EQNS_V01,
		.elem_size	= sizeof(
			struct ipa_ipfltr_range_eq_16_type_v01),
		.is_array	= STATIC_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					ihl_offset_range_16),
		.ei_array	= ipa3_ipfltr_range_eq_16_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					num_offset_meq_32),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_IPFLTR_NUM_MEQ_32_EQNS_V01,
		.elem_size	= sizeof(struct ipa_ipfltr_mask_eq_32_type_v01),
		.is_array	= STATIC_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					offset_meq_32),
		.ei_array	= ipa3_ipfltr_mask_eq_32_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					tc_eq_present),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					tc_eq),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					flow_eq_present),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					flow_eq),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					ihl_offset_eq_16_present),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_ipfltr_eq_16_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					ihl_offset_eq_16),
		.ei_array	= ipa3_ipfltr_eq_16_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					ihl_offset_eq_32_present),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_ipfltr_eq_32_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					ihl_offset_eq_32),
		.ei_array	= ipa3_ipfltr_eq_32_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					num_ihl_offset_meq_32),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_IPFLTR_NUM_IHL_MEQ_32_EQNS_V01,
		.elem_size	= sizeof(struct ipa_ipfltr_mask_eq_32_type_v01),
		.is_array	= STATIC_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					ihl_offset_meq_32),
		.ei_array	= ipa3_ipfltr_mask_eq_32_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					num_offset_meq_128),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	=
			QMI_IPA_IPFLTR_NUM_MEQ_128_EQNS_V01,
		.elem_size	= sizeof(
			struct ipa_ipfltr_mask_eq_128_type_v01),
		.is_array	= STATIC_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_filter_rule_type_v01,
			offset_meq_128),
		.ei_array	= ipa3_ipfltr_mask_eq_128_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					metadata_meq32_present),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_ipfltr_mask_eq_32_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					metadata_meq32),
		.ei_array	= ipa3_ipfltr_mask_eq_32_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_rule_type_v01,
					ipv4_frag_eq_present),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa_filter_spec_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					filter_spec_identifier),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					ip_type),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_filter_rule_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					filter_rule),
		.ei_array	= ipa3_filter_rule_type_data_v01_ei,
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					filter_action),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					is_routing_table_index_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					route_table_index),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					is_mux_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_type_v01,
					mux_id),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa_filter_spec_ex_type_data_v01_ei[] = {
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					ip_type),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_filter_rule_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					filter_rule),
		.ei_array	= ipa3_filter_rule_type_data_v01_ei,
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					filter_action),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					is_routing_table_index_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					route_table_index),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					is_mux_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					mux_id),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					rule_id),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_filter_spec_ex_type_v01,
					is_rule_hashable),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct
elem_info ipa3_filter_rule_identifier_to_handle_map_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_filter_rule_identifier_to_handle_map_v01,
			filter_spec_identifier),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_filter_rule_identifier_to_handle_map_v01,
			filter_handle),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_filter_handle_to_index_map_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_filter_handle_to_index_map_v01,
			filter_handle),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_filter_handle_to_index_map_v01,
			filter_index),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_init_modem_driver_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			platform_type_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			platform_type),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hdr_tbl_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_hdr_tbl_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hdr_tbl_info),
		.ei_array	= ipa3_hdr_tbl_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_route_tbl_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_route_tbl_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_route_tbl_info),
		.ei_array	= ipa3_route_tbl_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_route_tbl_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_route_tbl_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_route_tbl_info),
		.ei_array	= ipa3_route_tbl_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_filter_tbl_start_addr_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_filter_tbl_start_addr),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_filter_tbl_start_addr_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_filter_tbl_start_addr),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			modem_mem_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_modem_mem_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			modem_mem_info),
		.ei_array	= ipa3_modem_mem_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			ctrl_comm_dest_end_pt_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			ctrl_comm_dest_end_pt),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			is_ssr_bootup_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			is_ssr_bootup),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hdr_proc_ctx_tbl_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(
			struct ipa_hdr_proc_ctx_tbl_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hdr_proc_ctx_tbl_info),
		.ei_array	= ipa3_hdr_proc_ctx_tbl_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			zip_tbl_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_zip_tbl_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			zip_tbl_info),
		.ei_array	= ipa3_zip_tbl_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_hash_route_tbl_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_route_tbl_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_hash_route_tbl_info),
		.ei_array	= ipa3_route_tbl_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_hash_route_tbl_info_valid),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_route_tbl_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_hash_route_tbl_info),
		.ei_array	= ipa3_route_tbl_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1D,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_hash_filter_tbl_start_addr_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1D,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v4_hash_filter_tbl_start_addr),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1E,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_hash_filter_tbl_start_addr_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1E,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			v6_hash_filter_tbl_start_addr),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1F,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_stats_quota_base_addr_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1F,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_stats_quota_base_addr),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x20,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_stats_quota_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x20,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_stats_quota_size),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x21,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_drop_stats_base_addr_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x21,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_drop_stats_base_addr),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x22,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_drop_stats_table_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x22,
		.offset		= offsetof(
			struct ipa_init_modem_driver_req_msg_v01,
			hw_drop_stats_table_size),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},

};

struct elem_info ipa3_init_modem_driver_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_init_modem_driver_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_init_modem_driver_resp_msg_v01,
			ctrl_comm_dest_end_pt_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_init_modem_driver_resp_msg_v01,
			ctrl_comm_dest_end_pt),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_init_modem_driver_resp_msg_v01,
			default_end_pt_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_init_modem_driver_resp_msg_v01,
			default_end_pt),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_init_modem_driver_resp_msg_v01,
			modem_driver_init_pending_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_init_modem_driver_resp_msg_v01,
			modem_driver_init_pending),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_init_modem_driver_cmplt_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_init_modem_driver_cmplt_req_msg_v01,
			status),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_init_modem_driver_cmplt_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_init_modem_driver_cmplt_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_indication_reg_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_indication_reg_req_msg_v01,
			master_driver_init_complete_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_indication_reg_req_msg_v01,
			master_driver_init_complete),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_indication_reg_req_msg_v01,
			data_usage_quota_reached_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_indication_reg_req_msg_v01,
			data_usage_quota_reached),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_indication_reg_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_indication_reg_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_master_driver_init_complt_ind_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(struct
			ipa_master_driver_init_complt_ind_msg_v01,
			master_driver_init_status),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_install_fltr_rule_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			filter_spec_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			filter_spec_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(struct ipa_filter_spec_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			filter_spec_list),
		.ei_array	= ipa_filter_spec_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			source_pipe_index_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			source_pipe_index),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			num_ipv4_filters_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			num_ipv4_filters),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			num_ipv6_filters_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			num_ipv6_filters),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			xlat_filter_indices_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			xlat_filter_indices_list_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(uint32_t),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			xlat_filter_indices_list),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			filter_spec_ex_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			filter_spec_ex_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(struct ipa_filter_spec_ex_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_msg_v01,
			filter_spec_ex_list),
		.ei_array	= ipa_filter_spec_ex_type_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_install_fltr_rule_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_msg_v01,
			resp),
		.ei_array       = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_msg_v01,
			filter_handle_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_msg_v01,
			filter_handle_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(
			struct ipa_filter_rule_identifier_to_handle_map_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_msg_v01,
			filter_handle_list),
		.ei_array	=
			ipa3_filter_rule_identifier_to_handle_map_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_msg_v01,
			rule_id_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_msg_v01,
			rule_id_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(uint32_t),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_msg_v01,
			rule_id),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_fltr_installed_notif_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			source_pipe_index),
	},
	{
		.data_type	= QMI_UNSIGNED_2_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint16_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			install_status),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x03,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			filter_index_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(
			struct ipa_filter_handle_to_index_map_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x03,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			filter_index_list),
		.ei_array	= ipa3_filter_handle_to_index_map_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			embedded_pipe_index_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			embedded_pipe_index),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			retain_header_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			retain_header),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			embedded_call_mux_id_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			embedded_call_mux_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			num_ipv4_filters_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			num_ipv4_filters),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			num_ipv6_filters_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			num_ipv6_filters),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			start_ipv4_filter_idx_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			start_ipv4_filter_idx),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			start_ipv6_filter_idx_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			start_ipv6_filter_idx),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			rule_id_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			rule_id_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(uint32_t),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			rule_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			dst_pipe_id_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			dst_pipe_id_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_IPA_MAX_CLIENT_DST_PIPES_V01,
		.elem_size	= sizeof(uint32_t),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_req_msg_v01,
			dst_pipe_id),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_fltr_installed_notif_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_fltr_installed_notif_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_enable_force_clear_datapath_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_enable_force_clear_datapath_req_msg_v01,
			source_pipe_bitmask),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_enable_force_clear_datapath_req_msg_v01,
			request_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_enable_force_clear_datapath_req_msg_v01,
			throttle_source_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_enable_force_clear_datapath_req_msg_v01,
			throttle_source),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_enable_force_clear_datapath_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_enable_force_clear_datapath_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_disable_force_clear_datapath_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_disable_force_clear_datapath_req_msg_v01,
			request_id),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_disable_force_clear_datapath_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_disable_force_clear_datapath_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_config_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			peripheral_type_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			peripheral_type),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			hw_deaggr_supported_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			hw_deaggr_supported),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			max_aggr_frame_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
					max_aggr_frame_size),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ipa_ingress_pipe_mode_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ipa_ingress_pipe_mode),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			peripheral_speed_info_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			peripheral_speed_info),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_accumulation_time_limit_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x15,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_accumulation_time_limit),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_accumulation_pkt_limit_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x16,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_accumulation_pkt_limit),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_accumulation_byte_limit_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x17,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_accumulation_byte_limit),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ul_accumulation_time_limit_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x18,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ul_accumulation_time_limit),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			hw_control_flags_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x19,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			hw_control_flags),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ul_msi_event_threshold_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1A,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ul_msi_event_threshold),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_msi_event_threshold_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1B,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_msi_event_threshold),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ul_fifo_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1C,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			ul_fifo_size),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1D,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_fifo_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1D,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_fifo_size),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1E,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_buf_size_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1E,
		.offset		= offsetof(
			struct ipa_config_req_msg_v01,
			dl_buf_size),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_config_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_config_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_get_data_stats_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_get_data_stats_req_msg_v01,
			ipa_stats_type),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_data_stats_req_msg_v01,
			reset_stats_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_data_stats_req_msg_v01,
			reset_stats),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_pipe_stats_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_pipe_stats_info_type_v01,
					pipe_index),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_pipe_stats_info_type_v01,
					num_ipv4_packets),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_pipe_stats_info_type_v01,
					num_ipv4_bytes),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_pipe_stats_info_type_v01,
					num_ipv6_packets),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_pipe_stats_info_type_v01,
					num_ipv6_bytes),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_stats_type_filter_rule_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_stats_type_filter_rule_v01,
					filter_rule_index),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_stats_type_filter_rule_v01,
					num_packets),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_get_data_stats_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			ipa_stats_type_valid),
	},
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			ipa_stats_type),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			ul_src_pipe_stats_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			ul_src_pipe_stats_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_PIPES_V01,
		.elem_size	= sizeof(struct ipa_pipe_stats_info_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			ul_src_pipe_stats_list),
		.ei_array	= ipa3_pipe_stats_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			dl_dst_pipe_stats_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			dl_dst_pipe_stats_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_PIPES_V01,
		.elem_size	= sizeof(struct ipa_pipe_stats_info_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			dl_dst_pipe_stats_list),
		.ei_array	= ipa3_pipe_stats_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			dl_filter_rule_stats_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			dl_filter_rule_stats_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_FILTERS_V01,
		.elem_size	= sizeof(struct ipa_pipe_stats_info_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_get_data_stats_resp_msg_v01,
			dl_filter_rule_stats_list),
		.ei_array	= ipa3_stats_type_filter_rule_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_apn_data_stats_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_apn_data_stats_info_type_v01,
					mux_id),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_apn_data_stats_info_type_v01,
					num_ul_packets),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_apn_data_stats_info_type_v01,
					num_ul_bytes),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_apn_data_stats_info_type_v01,
					num_dl_packets),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_apn_data_stats_info_type_v01,
					num_dl_bytes),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_get_apn_data_stats_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_apn_data_stats_req_msg_v01,
			mux_id_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_apn_data_stats_req_msg_v01,
			mux_id_list_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_IPA_MAX_APN_V01,
		.elem_size	= sizeof(uint32_t),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_apn_data_stats_req_msg_v01,
			mux_id_list),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_get_apn_data_stats_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_get_apn_data_stats_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_apn_data_stats_resp_msg_v01,
			apn_data_stats_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_apn_data_stats_resp_msg_v01,
			apn_data_stats_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_APN_V01,
		.elem_size	= sizeof(struct
					ipa_apn_data_stats_info_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_apn_data_stats_resp_msg_v01,
			apn_data_stats_list),
		.ei_array	= ipa3_apn_data_stats_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info ipa3_data_usage_quota_info_type_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_data_usage_quota_info_type_v01,
					mux_id),
	},
	{
		.data_type	= QMI_UNSIGNED_8_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint64_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct
					ipa_data_usage_quota_info_type_v01,
					num_Mbytes),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_set_data_usage_quota_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_set_data_usage_quota_req_msg_v01,
			apn_quota_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_set_data_usage_quota_req_msg_v01,
			apn_quota_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_APN_V01,
		.elem_size	= sizeof(struct
					ipa_data_usage_quota_info_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_set_data_usage_quota_req_msg_v01,
			apn_quota_list),
		.ei_array	= ipa3_data_usage_quota_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_set_data_usage_quota_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_set_data_usage_quota_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_data_usage_quota_reached_ind_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct
					ipa_data_usage_quota_info_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_data_usage_quota_reached_ind_msg_v01,
			apn),
		.ei_array	= ipa3_data_usage_quota_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_stop_data_usage_quota_req_msg_data_v01_ei[] = {
	/* ipa_stop_data_usage_quota_req_msg is empty */
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_stop_data_usage_quota_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_stop_data_usage_quota_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_install_fltr_rule_req_ex_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			filter_spec_ex_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			filter_spec_ex_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_FILTERS_EX_V01,
		.elem_size	= sizeof(struct
					ipa_filter_spec_ex_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			filter_spec_ex_list),
		.ei_array	= ipa_filter_spec_ex_type_data_v01_ei,
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			source_pipe_index_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			source_pipe_index),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			num_ipv4_filters_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x12,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			num_ipv4_filters),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			num_ipv6_filters_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x13,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			num_ipv6_filters),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			xlat_filter_indices_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			xlat_filter_indices_list_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_IPA_MAX_FILTERS_EX_V01,
		.elem_size	= sizeof(uint32_t),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x14,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_req_ex_msg_v01,
			xlat_filter_indices_list),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_install_fltr_rule_resp_ex_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_ex_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_ex_msg_v01,
			rule_id_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_ex_msg_v01,
			rule_id_len),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= QMI_IPA_MAX_FILTERS_EX_V01,
		.elem_size	= sizeof(uint32_t),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_install_fltr_rule_resp_ex_msg_v01,
			rule_id),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_per_client_stats_info_type_data_v01_ei[] = {
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				client_id),
	},
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				src_pipe_id),
	},
	{
			.data_type	= QMI_UNSIGNED_8_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint64_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_ul_ipv4_bytes),

	},
	{
			.data_type	= QMI_UNSIGNED_8_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint64_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_ul_ipv6_bytes),

	},
	{
			.data_type	= QMI_UNSIGNED_8_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint64_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_dl_ipv4_bytes),

	},
	{
			.data_type	= QMI_UNSIGNED_8_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint64_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_dl_ipv6_bytes),

	},
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_ul_ipv4_pkts),

	},
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_ul_ipv6_pkts),

	},
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_dl_ipv4_pkts),

	},
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_per_client_stats_info_type_v01,
				num_dl_ipv6_pkts),

	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_ul_firewall_rule_type_data_v01_ei[] = {
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_ul_firewall_rule_type_v01,
				ip_type),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct ipa_filter_rule_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct ipa_ul_firewall_rule_type_v01,
					filter_rule),
		.ei_array	= ipa3_filter_rule_type_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_ul_firewall_config_result_type_data_v01_ei[] = {
	{
			.data_type	= QMI_UNSIGNED_4_BYTE,
			.elem_len	= 1,
			.elem_size	= sizeof(uint32_t),
			.is_array	= NO_ARRAY,
			.tlv_type	= QMI_COMMON_TLV_TYPE,
			.offset		= offsetof(
				struct ipa_ul_firewall_config_result_type_v01,
				is_success),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(
			struct ipa_ul_firewall_config_result_type_v01,
			mux_id),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_enable_per_client_stats_req_msg_data_v01_ei[] = {
	{
				.data_type	= QMI_UNSIGNED_1_BYTE,
				.elem_len	= 1,
				.elem_size	= sizeof(uint8_t),
				.is_array	= NO_ARRAY,
				.tlv_type	= 0x01,
				.offset		= offsetof(struct
				ipa_enable_per_client_stats_req_msg_v01,
				enable_per_client_stats),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_enable_per_client_stats_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_enable_per_client_stats_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_get_stats_per_client_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_req_msg_v01,
			client_id),
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_req_msg_v01,
			src_pipe_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_req_msg_v01,
			reset_stats_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_req_msg_v01,
			reset_stats),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_get_stats_per_client_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_resp_msg_v01,
			per_client_stats_list_valid),
	},
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_resp_msg_v01,
			per_client_stats_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_PER_CLIENTS_V01,
		.elem_size	=
			sizeof(struct ipa_per_client_stats_info_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_get_stats_per_client_resp_msg_v01,
			per_client_stats_list),
		.ei_array	=
			ipa3_per_client_stats_info_type_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_configure_ul_firewall_rules_req_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x1,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_req_msg_v01,
			firewall_rules_list_len),
	},
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= QMI_IPA_MAX_UL_FIREWALL_RULES_V01,
		.elem_size	= sizeof(struct ipa_ul_firewall_rule_type_v01),
		.is_array	= VAR_LEN_ARRAY,
		.tlv_type	= 0x1,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_req_msg_v01,
			firewall_rules_list),
		.ei_array	=
			ipa3_ul_firewall_rule_type_data_v01_ei,
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint32_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x2,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_req_msg_v01,
			mux_id),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_req_msg_v01,
			disable_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x10,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_req_msg_v01,
			disable),
	},
	{
		.data_type	= QMI_OPT_FLAG,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_req_msg_v01,
			are_blacklist_filters_valid),
	},
	{
		.data_type	= QMI_UNSIGNED_1_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x11,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_req_msg_v01,
			are_blacklist_filters),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_configure_ul_firewall_rules_resp_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(struct qmi_response_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x02,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_resp_msg_v01,
			resp),
		.ei_array	= get_qmi_response_type_v01_ei(),
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info ipa3_configure_ul_firewall_rules_ind_msg_data_v01_ei[] = {
	{
		.data_type	= QMI_STRUCT,
		.elem_len	= 1,
		.elem_size	= sizeof(
			struct ipa_ul_firewall_config_result_type_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= 0x01,
		.offset		= offsetof(
			struct ipa_configure_ul_firewall_rules_ind_msg_v01,
			result),
		.ei_array	=
		ipa3_ul_firewall_config_result_type_data_v01_ei,
	},
	{
		.data_type	= QMI_EOTI,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
	},
};
