 /* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#ifndef QUPM_QMI_INTERFACE_H
#define QUPM_QMI_INTERFACE_H

#include <linux/qmi_encdec.h>
#include <soc/qcom/msm_qmi_interface.h>

#define QUPM_SERVICE_ID_V01 0x1FF
#define QUPM_SERVICE_VERS_V01 0x01

#define QMI_QUPM_READY_REQ_V01 0x0020
#define QMI_QUPM_READY_RESP_V01 0x0020
#define QMI_QUPM_TAKE_OWNERSHIP_REQ_V01 0x0021
#define QMI_QUPM_TAKE_OWNERSHIP_RESP_V01 0x0021
#define QMI_QUPM_GIVE_OWNERSHIP_REQ_V01 0x0022
#define QMI_QUPM_GIVE_OWNERSHIP_RESP_V01 0x0022

/*
 * Request message: This command is used to check
 * if peripheral is ready or not.
 */
struct qupm_ready_req_msg_v01 {
	uint32_t qup_id;
	uint8_t flags_valid;
	uint32_t flags;
};
#define QUPM_READY_REQ_MSG_V01_MAX_MSG_LEN 14

/*
 * Response message: This command is used to check
 * if peripheral is ready or not.
 */
struct qupm_ready_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t status_valid;
	uint32_t status;
};
#define QUPM_READY_RESP_MSG_V01_MAX_MSG_LEN 14

/*
 * Request message: This command is used to take
 * ownership of qup_id by APPS.
 */
struct qupm_take_ownership_req_msg_v01 {
	uint32_t qup_id;
	uint8_t flags_valid;
	uint32_t flags;
};
#define QUPM_TAKE_OWNERSHIP_REQ_MSG_V01_MAX_MSG_LEN 14

/*
 * Respone message: This command is used to take
 * ownership of qup_id by APPS.
 */
struct qupm_take_ownership_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t status_valid;
	uint32_t status;
};
#define QUPM_TAKE_OWNERSHIP_RESP_MSG_V01_MAX_MSG_LEN 14

/*
 * Request message: This command is used to give
 * ownership of qup_id to peripheral.
 */
struct qupm_give_ownership_req_msg_v01 {
	uint32_t qup_id;
	uint8_t flags_valid;
	uint32_t flags;
};
#define QUPM_GIVE_OWNERSHIP_REQ_MSG_V01_MAX_MSG_LEN 14

/*
 * Response message: This command is used to give
 * ownership of qup_id to peripheral.
 */
struct qupm_give_ownership_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t status_valid;
	uint32_t status;
};
#define QUPM_GIVE_OWNERSHIP_RESP_MSG_V01_MAX_MSG_LEN 14

/* QMI Message Encoder/Decoder structures */
struct elem_info qupm_ready_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct qupm_ready_req_msg_v01,
					   qup_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct qupm_ready_req_msg_v01,
					   flags_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct qupm_ready_req_msg_v01,
					   flags),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qupm_ready_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct qupm_ready_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct qupm_ready_resp_msg_v01,
					   status_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct qupm_ready_resp_msg_v01,
					   status),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qupm_take_ownership_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(
			struct qupm_take_ownership_req_msg_v01,
			qup_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_take_ownership_req_msg_v01,
			flags_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_take_ownership_req_msg_v01,
			flags),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qupm_take_ownership_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(
			struct qupm_take_ownership_resp_msg_v01,
			resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_take_ownership_resp_msg_v01,
			status_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_take_ownership_resp_msg_v01,
			status),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qupm_give_ownership_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(
			struct qupm_give_ownership_req_msg_v01,
			qup_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_give_ownership_req_msg_v01,
			flags_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_give_ownership_req_msg_v01,
			flags),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qupm_give_ownership_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(
			struct qupm_give_ownership_resp_msg_v01,
			resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_give_ownership_resp_msg_v01,
			status_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
			struct qupm_give_ownership_resp_msg_v01,
			status),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

#endif /* QUPM_QMI_INTERFACE_H */
