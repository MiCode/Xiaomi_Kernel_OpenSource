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
 */

#ifndef __SSM_H_
#define __SSM_H_

#define MAX_APP_NAME_SIZE	32
#define MODE_INFO_MAX_SIZE	4
#define ENC_MODE_MAX_SIZE	(100 + MODE_INFO_MAX_SIZE)

/* tzapp response.*/
enum tz_response {
	RESULT_SUCCESS = 0,
	RESULT_FAILURE  = 0xFFFFFFFF,
};

/* tzapp command list.*/
enum tz_commands {
	ENC_MODE,
	GET_ENC_MODE,
	KEY_EXCHANGE = 11,
};

/* Command list for QSEOS.*/
enum qceos_cmd_id {
	APP_START_COMMAND = 0x01,
	APP_SHUTDOWN_COMMAND,
	APP_LOOKUP_COMMAND,
	CLIENT_SEND_DATA_COMMAND = 0x6,
	QSEOS_CMD_MAX = 0xEFFFFFFF,
};

/* MODEM/SSM command list.*/
enum ssm_ipc_req {
	SSM_MTOA_KEY_EXCHANGE = 0x0000AAAA,
	SSM_ATOM_KEY_STATUS,
	SSM_ATOM_MODE_UPDATE,
	SSM_MTOA_MODE_UPDATE_STATUS,
	SSM_MTOA_PREV_INVALID,
	SSM_ATOM_PREV_INVALID,
	SSM_ATOM_SET_DEFAULT_MODE,
	SSM_INVALID_REQ,
};

/* OEM reuest commands list.*/
enum oem_req {
	SSM_READY,
	SSM_GET_APP_ID,
	SSM_MODE_INFO_READY,
	SSM_SET_MODE,
	SSM_GET_MODE_STATUS,
	SSM_SET_DEFAULT_MODE,
	SSM_INVALID,
};

/* Modem mode update status.*/
enum modem_mode_status {
	SUCCESS,
	RETRY,
	FAILED = -1,
};

__packed struct load_app {
	uint32_t cmd_id;
	uint32_t mdt_len;
	uint32_t img_len;
	uint32_t phy_addr;
	char     app_name[MAX_APP_NAME_SIZE];
};

/* Stop tzapp reuest.*/
__packed struct scm_shutdown_req {
	uint32_t cmd_id;
	uint32_t app_id;
};

/* Common tzos response.*/
__packed struct scm_resp {
	uint32_t result;
	enum tz_response resp_type;
	unsigned int data;
};

/* tzos request.*/
__packed struct check_app_req {
	uint32_t cmd_id;
	char     app_name[MAX_APP_NAME_SIZE];
};

/* tzapp encode mode reuest.*/
__packed struct tzapp_mode_enc_req {
	uint32_t tzapp_ssm_cmd;
	uint8_t  mode_info[4];
};

/* tzapp encode mode response.*/
__packed struct tzapp_mode_enc_rsp {
	uint32_t tzapp_ssm_cmd;
	uint8_t enc_mode_info[ENC_MODE_MAX_SIZE];
	uint32_t enc_mode_len;
	long status;
};

/* tzapp get mode request.*/
__packed struct tzapp_get_mode_info_req {
	uint32_t tzapp_ssm_cmd;
};

/* tzapp get mode response.*/
__packed struct tzapp_get_mode_info_rsp {
	uint32_t tzapp_ssm_cmd;
	uint8_t  enc_mode_info[ENC_MODE_MAX_SIZE];
	uint32_t enc_mode_len;
	long status;
};

/* tzos key exchange request.*/
__packed struct ssm_keyexchg_req {
	uint32_t ssid;
	void *address;
	uint32_t length;
	uint32_t *status;
};

/* tzos common request.*/
__packed struct common_req {
	uint32_t cmd_id;
	uint32_t app_id;
	void *req_ptr;
	uint32_t req_len;
	void *resp_ptr;
	uint32_t resp_len;
};

/* tzos common response.*/
__packed struct common_resp {
	uint32_t result;
	uint32_t type;
	uint32_t data;
};

/* Modem/SSM packet format.*/
struct ssm_common_msg {
	unsigned long pktlen;
	unsigned long replaynum;
	enum ssm_ipc_req ipc_req;
	unsigned long msg_len;
	char *msg;
};

#endif
