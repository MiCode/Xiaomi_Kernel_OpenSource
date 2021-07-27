// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2021, The Linux Foundation. All rights reserved. */

#include "cnss_plat_ipc_service_v01.h"
#include <linux/module.h>

struct qmi_elem_info cnss_plat_ipc_qmi_init_setup_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_init_setup_req_msg_v01,
					   dms_mac_addr_supported),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_init_setup_req_msg_v01,
					   qdss_hw_trace_override),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_init_setup_req_msg_v01,
					   cal_file_available_bitmask),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_init_setup_req_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_init_setup_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_init_setup_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_init_setup_resp_msg_v01,
					   drv_status),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_init_setup_resp_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_file_download_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_ind_msg_v01,
					   file_name),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_ind_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_download_ind_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_file_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_req_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_req_msg_v01,
					   file_size),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x04,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_req_msg_v01,
					   seg_index),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x05,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_req_msg_v01,
					   seg_buf_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x05,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_req_msg_v01,
					   seg_buf),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_download_req_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_file_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_resp_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x04,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_download_resp_msg_v01,
					   seg_index),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_download_resp_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_file_upload_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_ind_msg_v01,
					   file_name),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_ind_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_ind_msg_v01,
					   file_size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_upload_ind_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_file_upload_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_req_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_req_msg_v01,
					   seg_index),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_upload_req_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_file_upload_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_resp_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x04,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_resp_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x05,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_resp_msg_v01,
					   seg_index),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x06,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_resp_msg_v01,
					   seg_buf_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x06,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_file_upload_resp_msg_v01,
					   seg_buf),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_file_upload_resp_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_reg_client_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_reg_client_req_msg_v01,
					   client_id_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum cnss_plat_ipc_qmi_client_id_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_reg_client_req_msg_v01,
					   client_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_reg_client_req_msg_v01_ei);

struct qmi_elem_info cnss_plat_ipc_qmi_reg_client_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   cnss_plat_ipc_qmi_reg_client_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(cnss_plat_ipc_qmi_reg_client_resp_msg_v01_ei);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("WLAN FW QMI service");
