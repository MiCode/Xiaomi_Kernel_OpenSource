/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef SYSTEM_HEALTH_MONITOR_V01_H
#define SYSTEM_HEALTH_MONITOR_V01_H

#define HMON_SERVICE_ID_V01 0x3C
#define HMON_SERVICE_VERS_V01 0x01

#define QMI_HEALTH_MON_HEALTH_CHECK_COMPLETE_REQ_V01 0x0022
#define QMI_HEALTH_MON_HEALTH_CHECK_COMPLETE_RESP_V01 0x0022
#define QMI_HEALTH_MON_HEALTH_CHECK_IND_V01 0x0021
#define QMI_HEALTH_MON_REG_RESP_V01 0x0020
#define QMI_HEALTH_MON_REG_REQ_V01 0x0020

struct hmon_register_req_msg_v01 {
	uint8_t name_valid;
	char name[256];
	uint8_t timeout_valid;
	uint32_t timeout;
};
#define HMON_REGISTER_REQ_MSG_V01_MAX_MSG_LEN 265
extern struct elem_info hmon_register_req_msg_v01_ei[];

struct hmon_register_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define HMON_REGISTER_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info hmon_register_resp_msg_v01_ei[];

struct hmon_health_check_ind_msg_v01 {
	char placeholder;
};
#define HMON_HEALTH_CHECK_IND_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info hmon_health_check_ind_msg_v01_ei[];

enum hmon_check_result_v01 {
	HMON_CHECK_RESULT_MIN_VAL_V01 = INT_MIN,
	HEALTH_MONITOR_CHECK_SUCCESS_V01 = 0,
	HEALTH_MONITOR_CHECK_FAILURE_V01 = 1,
	HMON_CHECK_RESULT_MAX_VAL_V01 = INT_MAX,
};

struct hmon_health_check_complete_req_msg_v01 {
	uint8_t result_valid;
	enum hmon_check_result_v01 result;
};
#define HMON_HEALTH_CHECK_COMPLETE_REQ_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info hmon_health_check_complete_req_msg_v01_ei[];

struct hmon_health_check_complete_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define HMON_HEALTH_CHECK_COMPLETE_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info hmon_health_check_complete_resp_msg_v01_ei[];

#endif /* SYSTEM_HEALTH_MONITOR_V01_H */
