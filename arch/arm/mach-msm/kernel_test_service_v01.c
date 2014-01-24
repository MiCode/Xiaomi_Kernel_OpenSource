/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/msm_qmi_interface.h>

#include "kernel_test_service_v01.h"

#define PING_REQ1_TLV_TYPE 0x1
#define PING_RESP1_TLV_TYPE 0x2
#define PING_OPT1_TLV_TYPE 0x10
#define PING_OPT2_TLV_TYPE 0x11

#define DATA_REQ1_TLV_TYPE 0x1
#define DATA_RESP1_TLV_TYPE 0x2
#define DATA_OPT1_TLV_TYPE 0x10
#define DATA_OPT2_TLV_TYPE 0x11

static struct elem_info test_name_type_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len	= 1,
		.elem_size      = sizeof(uint8_t),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct test_name_type_v01,
					   name_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = TEST_MAX_NAME_SIZE_V01,
		.elem_size      = sizeof(char),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
		.offset         = offsetof(struct test_name_type_v01,
					   name),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info test_ping_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 4,
		.elem_size      = sizeof(char),
		.is_array       = STATIC_ARRAY,
		.tlv_type       = PING_REQ1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_req_msg_v01,
					   ping),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_req_msg_v01,
					   client_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_req_msg_v01,
					   client_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info test_ping_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_RESP1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   resp),
		.ei_array       = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   pong_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 4,
		.elem_size      = sizeof(char),
		.is_array       = STATIC_ARRAY,
		.tlv_type       = PING_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   pong),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   service_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = PING_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_ping_resp_msg_v01,
					   service_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info test_data_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_REQ1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = TEST_MED_DATA_SIZE_V01,
		.elem_size      = sizeof(uint8_t),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = DATA_REQ1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   client_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_req_msg_v01,
					   client_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info test_data_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_RESP1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   resp),
		.ei_array       = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = TEST_MED_DATA_SIZE_V01,
		.elem_size      = sizeof(uint8_t),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = DATA_OPT1_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   service_name_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct test_name_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = DATA_OPT2_TLV_TYPE,
		.offset         = offsetof(struct test_data_resp_msg_v01,
					   service_name),
		.ei_array       = test_name_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
