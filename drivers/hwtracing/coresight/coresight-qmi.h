/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
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

#ifndef _CORESIGHT_QMI_H
#define _CORESIGHT_QMI_H

#include <linux/soc/qcom/qmi.h>

#define CORESIGHT_QMI_SVC_ID			(0x33)
#define CORESIGHT_QMI_VERSION			(1)

#define CORESIGHT_QMI_GET_ETM_REQ_V01		(0x002B)
#define CORESIGHT_QMI_GET_ETM_RESP_V01		(0x002B)
#define CORESIGHT_QMI_SET_ETM_REQ_V01		(0x002C)
#define CORESIGHT_QMI_SET_ETM_RESP_V01		(0x002C)

#define CORESIGHT_QMI_GET_ETM_REQ_MAX_LEN	(0)
#define CORESIGHT_QMI_GET_ETM_RESP_MAX_LEN	(14)
#define CORESIGHT_QMI_SET_ETM_REQ_MAX_LEN	(7)
#define CORESIGHT_QMI_SET_ETM_RESP_MAX_LEN	(7)

#define TIMEOUT_MS				(5000)

enum coresight_etm_state_enum_type_v01 {
	/* To force a 32 bit signed enum. Do not change or use */
	CORESIGHT_ETM_STATE_ENUM_TYPE_MIN_ENUM_VAL_V01 = INT_MIN,
	CORESIGHT_ETM_STATE_DISABLED_V01 = 0,
	CORESIGHT_ETM_STATE_ENABLED_V01 = 1,
	CORESIGHT_ETM_STATE_ENUM_TYPE_MAX_ENUM_VAL_01 = INT_MAX,
};

struct coresight_get_etm_req_msg_v01 {
	/*
	 * This element is a placeholder to prevent declaration of
	 * empty struct. Do not change.
	 */
	char __placeholder;
};

struct coresight_get_etm_resp_msg_v01 {
	/* Mandatory */
	/* QMI result Code */
	struct qmi_response_type_v01 resp;

	/* Optional */
	/* ETM output state, must be set to true if state is being passed */
	uint8_t state_valid;
	/* Present when result code is QMI_RESULT_SUCCESS */
	enum coresight_etm_state_enum_type_v01 state;
};

struct coresight_set_etm_req_msg_v01 {
	/* Mandatory */
	/* ETM output state */
	enum coresight_etm_state_enum_type_v01 state;
};

struct coresight_set_etm_resp_msg_v01 {
	/* Mandatory */
	struct qmi_response_type_v01 resp;
};

static struct qmi_elem_info coresight_set_etm_req_msg_v01_ei[] = {
	{
		.data_type = QMI_UNSIGNED_4_BYTE,
		.elem_len  = 1,
		.elem_size = sizeof(enum coresight_etm_state_enum_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x01,
		.offset    = offsetof(struct coresight_set_etm_req_msg_v01,
				      state),
		.ei_array  = NULL,
	},
	{
		.data_type = QMI_EOTI,
		.elem_len  = 0,
		.elem_size = 0,
		.is_array  = NO_ARRAY,
		.tlv_type  = 0,
		.offset    = 0,
		.ei_array  = NULL,
	},
};

static struct qmi_elem_info coresight_set_etm_resp_msg_v01_ei[] = {
	{
		.data_type = QMI_STRUCT,
		.elem_len  = 1,
		.elem_size = sizeof(struct qmi_response_type_v01),
		.is_array  = NO_ARRAY,
		.tlv_type  = 0x02,
		.offset    = offsetof(struct coresight_set_etm_resp_msg_v01,
				      resp),
		.ei_array  = qmi_response_type_v01_ei,
	},
	{
		.data_type = QMI_EOTI,
		.elem_len  = 0,
		.elem_size = 0,
		.is_array  = NO_ARRAY,
		.tlv_type  = 0,
		.offset    = 0,
		.ei_array  = NULL,
	},
};

#endif
