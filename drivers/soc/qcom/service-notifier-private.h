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
#ifndef SERVICE_REGISTRY_NOTIFIER_H
#define SERVICE_REGISTRY_NOTIFIER_H

#include <linux/qmi_encdec.h>

#include <soc/qcom/msm_qmi_interface.h>

#define SERVREG_NOTIF_SERVICE_ID_V01 0x42
#define SERVREG_NOTIF_SERVICE_VERS_V01 0x01

#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_REQ_V01 0x0020
#define QMI_SERVREG_NOTIF_QUERY_STATE_REQ_V01 0x0021
#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_RESP_V01 0x0020
#define QMI_SERVREG_NOTIF_QUERY_STATE_RESP_V01 0x0021
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_V01 0x0022
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_ACK_RESP_V01 0x0023
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_ACK_REQ_V01 0x0023

#define QMI_SERVREG_NOTIF_NAME_LENGTH_V01 64

struct qmi_servreg_notif_register_listener_req_msg_v01 {
	uint8_t enable;
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
};
#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_REQ_MSG_V01_MAX_MSG_LEN 71
struct elem_info qmi_servreg_notif_register_listener_req_msg_v01_ei[];

struct qmi_servreg_notif_register_listener_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t curr_state_valid;
	enum qmi_servreg_notif_service_state_enum_type_v01 curr_state;
};
#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_RESP_MSG_V01_MAX_MSG_LEN 14
struct elem_info qmi_servreg_notif_register_listener_resp_msg_v01_ei[];

struct qmi_servreg_notif_query_state_req_msg_v01 {
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
};
#define QMI_SERVREG_NOTIF_QUERY_STATE_REQ_MSG_V01_MAX_MSG_LEN 67
struct elem_info qmi_servreg_notif_query_state_req_msg_v01_ei[];

struct qmi_servreg_notif_query_state_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t curr_state_valid;
	enum qmi_servreg_notif_service_state_enum_type_v01 curr_state;
};
#define QMI_SERVREG_NOTIF_QUERY_STATE_RESP_MSG_V01_MAX_MSG_LEN 14
struct elem_info qmi_servreg_notif_query_state_resp_msg_v01_ei[];

struct qmi_servreg_notif_state_updated_ind_msg_v01 {
	enum qmi_servreg_notif_service_state_enum_type_v01 curr_state;
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
	uint16_t transaction_id;
};
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_MSG_V01_MAX_MSG_LEN 79
struct elem_info qmi_servreg_notif_state_updated_ind_msg_v01_ei[];

struct qmi_servreg_notif_set_ack_req_msg_v01 {
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
	uint16_t transaction_id;
};
#define QMI_SERVREG_NOTIF_SET_ACK_REQ_MSG_V01_MAX_MSG_LEN 72
struct elem_info qmi_servreg_notif_set_ack_req_msg_v01_ei[];

struct qmi_servreg_notif_set_ack_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define QMI_SERVREG_NOTIF_SET_ACK_RESP_MSG_V01_MAX_MSG_LEN 7
struct elem_info qmi_servreg_notif_set_ack_resp_msg_v01_ei[];

struct elem_info qmi_servreg_notif_register_listener_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_register_listener_req_msg_v01,
					   enable),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_register_listener_req_msg_v01,
					   service_name),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qmi_servreg_notif_register_listener_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
			qmi_servreg_notif_register_listener_resp_msg_v01,
									resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
			qmi_servreg_notif_register_listener_resp_msg_v01,
							curr_state_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(
			enum qmi_servreg_notif_service_state_enum_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
			qmi_servreg_notif_register_listener_resp_msg_v01,
								curr_state),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qmi_servreg_notif_query_state_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_query_state_req_msg_v01,
								service_name),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qmi_servreg_notif_query_state_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_query_state_resp_msg_v01,
									resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_notif_query_state_resp_msg_v01,
							curr_state_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum
				qmi_servreg_notif_service_state_enum_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_notif_query_state_resp_msg_v01,
								curr_state),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qmi_servreg_notif_state_updated_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum
				qmi_servreg_notif_service_state_enum_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_state_updated_ind_msg_v01,
								curr_state),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_state_updated_ind_msg_v01,
								service_name),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
				qmi_servreg_notif_state_updated_ind_msg_v01,
								transaction_id),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qmi_servreg_notif_set_ack_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_set_ack_req_msg_v01,
								service_name),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_set_ack_req_msg_v01,
								transaction_id),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info qmi_servreg_notif_set_ack_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_set_ack_resp_msg_v01,
									resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

#endif
