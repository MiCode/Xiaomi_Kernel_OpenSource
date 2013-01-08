/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#ifndef MSM_QMI_TEST_SERVICE_V01_H
#define MSM_QMI_TEST_SERVICE_V01_H

#include <mach/msm_qmi_interface.h>

#define TEST_MED_DATA_SIZE_V01 8192
#define TEST_MAX_NAME_SIZE_V01 255

#define TEST_PING_REQ_MSG_ID_V01 0x20
#define TEST_DATA_REQ_MSG_ID_V01 0x21

#define TEST_PING_REQ_MAX_MSG_LEN_V01 266
#define TEST_DATA_REQ_MAX_MSG_LEN_V01 8456

struct test_name_type_v01 {
	uint32_t name_len;
	char name[TEST_MAX_NAME_SIZE_V01];
};

struct test_ping_req_msg_v01 {
	char ping[4];

	uint8_t client_name_valid;
	struct test_name_type_v01 client_name;
};

struct test_ping_resp_msg_v01 {
	struct qmi_response_type_v01 resp;

	uint8_t pong_valid;
	char pong[4];

	uint8_t service_name_valid;
	struct test_name_type_v01 service_name;
};

struct test_data_req_msg_v01 {
	uint32_t data_len;
	uint8_t data[TEST_MED_DATA_SIZE_V01];

	uint8_t client_name_valid;
	struct test_name_type_v01 client_name;
};

struct test_data_resp_msg_v01 {
	struct qmi_response_type_v01 resp;

	uint8_t data_valid;
	uint32_t data_len;
	uint8_t data[TEST_MED_DATA_SIZE_V01];

	uint8_t service_name_valid;
	struct test_name_type_v01 service_name;
};

extern struct elem_info test_ping_req_msg_v01_ei[];
extern struct elem_info test_data_req_msg_v01_ei[];
extern struct elem_info test_ping_resp_msg_v01_ei[];
extern struct elem_info test_data_resp_msg_v01_ei[];

#endif
