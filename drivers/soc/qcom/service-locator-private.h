/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015,2017-2018, The Linux Foundation. All rights reserved.
 */
#ifndef SERVICE_LOCATOR_V01_H
#define SERVICE_LOCATOR_V01_H

#include <linux/soc/qcom/qmi.h>
#include <soc/qcom/service-locator.h>

#define SERVREG_LOC_SERVICE_ID_V01 0x40
#define SERVREG_LOC_SERVICE_VERS_V01 0x01

#define QMI_SERVREG_LOC_INDICATION_REGISTER_RESP_V01 0x0020
#define QMI_SERVREG_LOC_REGISTER_SERVICE_LIST_REQ_V01 0x0022
#define QMI_SERVREG_LOC_GET_DOMAIN_LIST_REQ_V01 0x0021
#define QMI_SERVREG_LOC_GET_DOMAIN_LIST_RESP_V01 0x0021
#define QMI_SERVREG_LOC_DATABASE_UPDATED_IND_V01 0x0023
#define QMI_SERVREG_LOC_INDICATION_REGISTER_REQ_V01 0x0020
#define QMI_SERVREG_LOC_REGISTER_SERVICE_LIST_RESP_V01 0x0022

#define QMI_SERVREG_LOC_NAME_LENGTH_V01 64
#define QMI_SERVREG_LOC_LIST_LENGTH_V01 32

enum qmi_servreg_loc_service_instance_enum_type_v01 {
	QMI_SERVREG_LOC_SERVICE_INSTANCE_ENUM_TYPE_MIN_VAL_V01 = INT_MIN,
	QMI_SERVREG_LOC_SERVICE_INSTANCE_APSS_V01 = 0x1,
	QMI_SERVREG_LOC_SERVICE_INSTANCE_ENUM_TYPE_MAX_VAL_V01 = INT_MAX,
};

struct qmi_servreg_loc_indication_register_req_msg_v01 {
	uint8_t enable_database_updated_indication_valid;
	uint8_t enable_database_updated_indication;
};
#define QMI_SERVREG_LOC_INDICATION_REGISTER_REQ_MSG_V01_MAX_MSG_LEN 4
struct qmi_elem_info qmi_servreg_loc_indication_register_req_msg_v01_ei[];

struct qmi_servreg_loc_indication_register_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define QMI_SERVREG_LOC_INDICATION_REGISTER_RESP_MSG_V01_MAX_MSG_LEN 7
struct qmi_elem_info qmi_servreg_loc_indication_register_resp_msg_v01_ei[];

struct qmi_servreg_loc_get_domain_list_req_msg_v01 {
	char service_name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
	uint8_t domain_offset_valid;
	uint32_t domain_offset;
};
#define QMI_SERVREG_LOC_GET_DOMAIN_LIST_REQ_MSG_V01_MAX_MSG_LEN 74
struct qmi_elem_info qmi_servreg_loc_get_domain_list_req_msg_v01_ei[];

struct qmi_servreg_loc_get_domain_list_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t total_domains_valid;
	uint16_t total_domains;
	uint8_t db_rev_count_valid;
	uint16_t db_rev_count;
	uint8_t domain_list_valid;
	uint32_t domain_list_len;
	struct servreg_loc_entry_v01
				domain_list[QMI_SERVREG_LOC_LIST_LENGTH_V01];
};
#define QMI_SERVREG_LOC_GET_DOMAIN_LIST_RESP_MSG_V01_MAX_MSG_LEN 2389
struct qmi_elem_info qmi_servreg_loc_get_domain_list_resp_msg_v01_ei[];

struct qmi_servreg_loc_register_service_list_req_msg_v01 {
	char domain_name[QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1];
	uint32_t service_list_len;
	struct servreg_loc_entry_v01
				service_list[QMI_SERVREG_LOC_LIST_LENGTH_V01];
};
#define QMI_SERVREG_LOC_REGISTER_SERVICE_LIST_REQ_MSG_V01_MAX_MSG_LEN 2439
struct qmi_elem_info qmi_servreg_loc_register_service_list_req_msg_v01_ei[];

struct qmi_servreg_loc_register_service_list_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define QMI_SERVREG_LOC_REGISTER_SERVICE_LIST_RESP_MSG_V01_MAX_MSG_LEN 7
struct qmi_elem_info qmi_servreg_loc_register_service_list_resp_msg_v01_ei[];

struct qmi_servreg_loc_database_updated_ind_msg_v01 {
	char placeholder;
};
#define QMI_SERVREG_LOC_DATABASE_UPDATED_IND_MSG_V01_MAX_MSG_LEN 0
struct qmi_elem_info qmi_servreg_loc_database_updated_ind_msg_v01_ei[];

#define QMI_EOTI_DATA_TYPE	\
{				\
	.data_type = QMI_EOTI,	\
	.elem_len  = 0,		\
	.elem_size = 0,		\
	.array_type  = NO_ARRAY,	\
	.tlv_type  = 0x00,	\
	.offset    = 0,		\
	.ei_array  = NULL,	\
},

static struct qmi_elem_info servreg_loc_entry_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct servreg_loc_entry_v01,
					   name),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct servreg_loc_entry_v01,
					   instance_id),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct servreg_loc_entry_v01,
					   service_data_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct servreg_loc_entry_v01,
					   service_data),
	},
	QMI_EOTI_DATA_TYPE
};

struct qmi_elem_info qmi_servreg_loc_indication_register_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_loc_indication_register_req_msg_v01,
				enable_database_updated_indication_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_loc_indication_register_req_msg_v01,
				enable_database_updated_indication),
	},
	QMI_EOTI_DATA_TYPE
};

struct qmi_elem_info qmi_servreg_loc_indication_register_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
			qmi_servreg_loc_indication_register_resp_msg_v01,
			resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	QMI_EOTI_DATA_TYPE
};

struct qmi_elem_info qmi_servreg_loc_get_domain_list_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_req_msg_v01,
				service_name),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_req_msg_v01,
				domain_offset_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_req_msg_v01,
				domain_offset),
	},
	QMI_EOTI_DATA_TYPE
};

struct qmi_elem_info qmi_servreg_loc_get_domain_list_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
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
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
				total_domains_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
				total_domains),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
				db_rev_count_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
				db_rev_count),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
				domain_list_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
				domain_list_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_SERVREG_LOC_LIST_LENGTH_V01,
		.elem_size      = sizeof(struct servreg_loc_entry_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
				qmi_servreg_loc_get_domain_list_resp_msg_v01,
				domain_list),
		.ei_array      = servreg_loc_entry_v01_ei,
	},
	QMI_EOTI_DATA_TYPE
};

struct qmi_elem_info qmi_servreg_loc_register_service_list_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
			qmi_servreg_loc_register_service_list_req_msg_v01,
			domain_name),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
			qmi_servreg_loc_register_service_list_req_msg_v01,
			service_list_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_SERVREG_LOC_LIST_LENGTH_V01,
		.elem_size      = sizeof(struct servreg_loc_entry_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
			qmi_servreg_loc_register_service_list_req_msg_v01,
			service_list),
		.ei_array      = servreg_loc_entry_v01_ei,
	},
	QMI_EOTI_DATA_TYPE
};

struct qmi_elem_info qmi_servreg_loc_register_service_list_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
			qmi_servreg_loc_register_service_list_resp_msg_v01,
			resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	QMI_EOTI_DATA_TYPE
};

struct qmi_elem_info qmi_servreg_loc_database_updated_ind_msg_v01_ei[] = {
	QMI_EOTI_DATA_TYPE
};

#endif
