/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 */
#ifndef SERVICE_REGISTRY_NOTIFIER_H
#define SERVICE_REGISTRY_NOTIFIER_H

#include <linux/soc/qcom/qmi.h>

#define SERVREG_NOTIF_SERVICE_ID_V01 0x42
#define SERVREG_NOTIF_SERVICE_VERS_V01 0x01

#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_REQ_V01 0x0020
#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_RESP_V01 0x0020
#define QMI_SERVREG_NOTIF_QUERY_STATE_REQ_V01 0x0021
#define QMI_SERVREG_NOTIF_QUERY_STATE_RESP_V01 0x0021
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_V01 0x0022
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_ACK_REQ_V01 0x0023
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_ACK_RESP_V01 0x0023
#define QMI_SERVREG_NOTIF_RESTART_PD_REQ_V01 0x0024
#define QMI_SERVREG_NOTIF_RESTART_PD_RESP_V01 0x0024

#define QMI_SERVREG_NOTIF_NAME_LENGTH_V01 64

struct qmi_servreg_notif_register_listener_req_msg_v01 {
	uint8_t enable;
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
};
#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_REQ_MSG_V01_MAX_MSG_LEN 71
struct qmi_elem_info qmi_servreg_notif_register_listener_req_msg_v01_ei[];

struct qmi_servreg_notif_register_listener_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t curr_state_valid;
	enum qmi_servreg_notif_service_state_enum_type_v01 curr_state;
};
#define QMI_SERVREG_NOTIF_REGISTER_LISTENER_RESP_MSG_V01_MAX_MSG_LEN 14
struct qmi_elem_info qmi_servreg_notif_register_listener_resp_msg_v01_ei[];

struct qmi_servreg_notif_query_state_req_msg_v01 {
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
};
#define QMI_SERVREG_NOTIF_QUERY_STATE_REQ_MSG_V01_MAX_MSG_LEN 67
struct qmi_elem_info qmi_servreg_notif_query_state_req_msg_v01_ei[];

struct qmi_servreg_notif_query_state_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t curr_state_valid;
	enum qmi_servreg_notif_service_state_enum_type_v01 curr_state;
};
#define QMI_SERVREG_NOTIF_QUERY_STATE_RESP_MSG_V01_MAX_MSG_LEN 14
struct qmi_elem_info qmi_servreg_notif_query_state_resp_msg_v01_ei[];

struct qmi_servreg_notif_state_updated_ind_msg_v01 {
	enum qmi_servreg_notif_service_state_enum_type_v01 curr_state;
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
	uint16_t transaction_id;
};
#define QMI_SERVREG_NOTIF_STATE_UPDATED_IND_MSG_V01_MAX_MSG_LEN 79
struct qmi_elem_info qmi_servreg_notif_state_updated_ind_msg_v01_ei[];

struct qmi_servreg_notif_set_ack_req_msg_v01 {
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
	uint16_t transaction_id;
};
#define QMI_SERVREG_NOTIF_SET_ACK_REQ_MSG_V01_MAX_MSG_LEN 72
struct qmi_elem_info qmi_servreg_notif_set_ack_req_msg_v01_ei[];

struct qmi_servreg_notif_set_ack_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define QMI_SERVREG_NOTIF_SET_ACK_RESP_MSG_V01_MAX_MSG_LEN 7
struct qmi_elem_info qmi_servreg_notif_set_ack_resp_msg_v01_ei[];

struct qmi_servreg_notif_restart_pd_req_msg_v01 {
	char service_name[QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1];
};
#define QMI_SERVREG_NOTIF_RESTART_PD_REQ_MSG_V01_MAX_MSG_LEN 67
extern struct qmi_elem_info qmi_servreg_notif_restart_pd_req_msg_v01_ei[];

struct qmi_servreg_notif_restart_pd_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define QMI_SERVREG_NOTIF_RESTART_PD_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info qmi_servreg_notif_restart_pd_resp_msg_v01_ei[];

struct qmi_elem_info qmi_servreg_notif_register_listener_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_register_listener_req_msg_v01,
					   enable),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_register_listener_req_msg_v01,
					   service_name),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_register_listener_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
			qmi_servreg_notif_register_listener_resp_msg_v01,
									resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
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
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
			qmi_servreg_notif_register_listener_resp_msg_v01,
								curr_state),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_query_state_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_query_state_req_msg_v01,
								service_name),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_query_state_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_query_state_resp_msg_v01,
									resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
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
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_notif_query_state_resp_msg_v01,
								curr_state),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_state_updated_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum
				qmi_servreg_notif_service_state_enum_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_state_updated_ind_msg_v01,
								curr_state),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_state_updated_ind_msg_v01,
								service_name),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
				qmi_servreg_notif_state_updated_ind_msg_v01,
								transaction_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_set_ack_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_set_ack_req_msg_v01,
								service_name),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_set_ack_req_msg_v01,
								transaction_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_set_ack_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_set_ack_resp_msg_v01,
									resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_restart_pd_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_NOTIF_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_notif_restart_pd_req_msg_v01,
								service_name),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info qmi_servreg_notif_restart_pd_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_notif_restart_pd_resp_msg_v01,
								   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.array_type       = QMI_COMMON_TLV_TYPE,
	},
};
#endif
