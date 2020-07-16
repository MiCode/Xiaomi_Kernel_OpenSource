// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved. */

#include "ip_multimedia_subsystem_private_service_v01.h"

static struct qmi_elem_info ims_private_service_header_value_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = IMS_PRIVATE_SERVICE_HEADER_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         =
		offsetof(struct ims_private_service_header_value_v01, header),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = IMS_PRIVATE_SERVICE_HEADER_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         =
		offsetof(struct ims_private_service_header_value_v01, value),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info
ims_private_service_subscribe_for_indications_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
		ims_private_service_subscribe_for_indications_req_msg_v01,
				mt_invite_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
		ims_private_service_subscribe_for_indications_req_msg_v01,
				mt_invite),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
		ims_private_service_subscribe_for_indications_req_msg_v01,
				wfc_call_status_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
		ims_private_service_subscribe_for_indications_req_msg_v01,
				wfc_call_status),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info
ims_private_service_subscribe_for_indications_rsp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
		ims_private_service_subscribe_for_indications_rsp_msg_v01,
				resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info ims_private_service_mt_invite_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum ims_subscription_type_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         =
		offsetof(struct ims_private_service_mt_invite_ind_msg_v01,
			 subscription_type),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         =
		offsetof(struct ims_private_service_mt_invite_ind_msg_v01,
			 iccid_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = IMS_PRIVATE_SERVICE_MAX_ICCID_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         =
		offsetof(struct ims_private_service_mt_invite_ind_msg_v01,
			 iccid),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         =
		offsetof(struct ims_private_service_mt_invite_ind_msg_v01,
			 header_value_list_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         =
		offsetof(struct ims_private_service_mt_invite_ind_msg_v01,
			 header_value_list_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = IMS_PRIVATE_SERVICE_MAX_MT_INVITE_HEADERS_V01,
		.elem_size      =
		sizeof(struct ims_private_service_header_value_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x11,
		.offset         =
		offsetof(struct ims_private_service_mt_invite_ind_msg_v01,
			 header_value_list),
		.ei_array      = ims_private_service_header_value_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info ims_private_service_wfc_call_status_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 wfc_call_active),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 all_wfc_calls_held_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 all_wfc_calls_held),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 is_wfc_emergency_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 is_wfc_emergency),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 twt_ims_start_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 twt_ims_start),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 twt_ims_int_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 twt_ims_int),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 media_quality_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wfc_media_quality_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         =
		offsetof(struct ims_private_service_wfc_call_status_ind_msg_v01,
			 media_quality),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info
ims_private_service_wfc_call_twt_config_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_start_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_start),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_int_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_int),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_upo_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_upo),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_sp_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_sp),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_dl_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_dl),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_config_changed_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_req_msg_v01,
			 twt_sta_config_changed),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info
ims_private_service_wfc_call_twt_config_rsp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         =
		offsetof(struct
			 ims_private_service_wfc_call_twt_config_rsp_msg_v01,
			 resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

