/*
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

#ifndef __QSEECOMI_H_
#define __QSEECOMI_H_

#include <linux/qseecom.h>

#define QSEECOM_KEY_ID_SIZE   32

#define QSEOS_RESULT_FAIL_UNSUPPORTED_CE_PIPE -63
#define QSEOS_RESULT_FAIL_KS_OP               -64
#define QSEOS_RESULT_FAIL_KEY_ID_EXISTS       -65
#define QSEOS_RESULT_FAIL_MAX_KEYS            -66
#define QSEOS_RESULT_FAIL_SAVE_KS             -67
#define QSEOS_RESULT_FAIL_LOAD_KS             -68
#define QSEOS_RESULT_FAIL_KS_ALREADY_DONE     -69
#define QSEOS_RESULT_FAIL_KEY_ID_DNE          -70
#define QSEOS_RESULT_FAIL_INCORRECT_PSWD      -71
#define QSEOS_RESULT_FAIL_MAX_ATTEMPT         -72

enum qseecom_command_scm_resp_type {
	QSEOS_APP_ID = 0xEE01,
	QSEOS_LISTENER_ID
};

enum qseecom_qceos_cmd_id {
	QSEOS_APP_START_COMMAND      = 0x01,
	QSEOS_APP_SHUTDOWN_COMMAND,
	QSEOS_APP_LOOKUP_COMMAND,
	QSEOS_REGISTER_LISTENER,
	QSEOS_DEREGISTER_LISTENER,
	QSEOS_CLIENT_SEND_DATA_COMMAND,
	QSEOS_LISTENER_DATA_RSP_COMMAND,
	QSEOS_LOAD_EXTERNAL_ELF_COMMAND,
	QSEOS_UNLOAD_EXTERNAL_ELF_COMMAND,
	QSEOS_GET_APP_STATE_COMMAND,
	QSEOS_LOAD_SERV_IMAGE_COMMAND,
	QSEOS_UNLOAD_SERV_IMAGE_COMMAND,
	QSEOS_APP_REGION_NOTIFICATION,
	QSEOS_REGISTER_LOG_BUF_COMMAND,
	QSEOS_RPMB_PROVISION_KEY_COMMAND,
	QSEOS_RPMB_ERASE_COMMAND,
	QSEOS_GENERATE_KEY  = 0x11,
	QSEOS_DELETE_KEY,
	QSEOS_MAX_KEY_COUNT,
	QSEOS_SET_KEY,
	QSEOS_UPDATE_KEY_USERINFO,
	QSEOS_TEE_OPEN_SESSION,
	QSEOS_TEE_INVOKE_COMMAND,
	QSEOS_TEE_CLOSE_SESSION,
	QSEOS_FSM_LTE_INIT_DB = 0x100,
	QSEOS_FSM_LTE_STORE_KENB = 0x101,
	QSEOS_FSM_LTE_GEN_KEYS = 0x102,
	QSEOS_FSM_LTE_GET_KEY_OFFSETS = 0x103,
	QSEOS_FSM_LTE_GEN_KENB_STAR = 0x104,
	QSEOS_FSM_LTE_GET_KENB_STAR = 0x105,
	QSEOS_FSM_LTE_STORE_NH = 0x106,
	QSEOS_FSM_LTE_DELETE_NH = 0x107,
	QSEOS_FSM_LTE_DELETE_KEYS = 0x108,
	QSEOS_FSM_IKE_CMD_SIGN = 0x200,
	QSEOS_FSM_IKE_CMD_PROV_KEY = 0x201,
	QSEOS_FSM_IKE_CMD_ENCRYPT_PRIVATE_KEY = 0x202,
	QSEOS_CMD_MAX     = 0xEFFFFFFF
};

enum qseecom_qceos_cmd_status {
	QSEOS_RESULT_SUCCESS = 0,
	QSEOS_RESULT_INCOMPLETE,
	QSEOS_RESULT_FAILURE  = 0xFFFFFFFF
};

enum qseecom_pipe_type {
	QSEOS_PIPE_ENC = 0x1,
	QSEOS_PIPE_ENC_XTS = 0x2,
	QSEOS_PIPE_AUTH = 0x4,
	QSEOS_PIPE_ENUM_FILL = 0x7FFFFFFF
};

__packed  struct qsee_apps_region_info_ireq {
	uint32_t qsee_cmd_id;
	uint32_t addr;
	uint32_t size;
};

__packed struct qseecom_check_app_ireq {
	uint32_t qsee_cmd_id;
	char     app_name[MAX_APP_NAME_SIZE];
};

__packed struct qseecom_load_app_ireq {
	uint32_t qsee_cmd_id;
	uint32_t mdt_len;		/* Length of the mdt file */
	uint32_t img_len;		/* Length of .bxx and .mdt files */
	uint32_t phy_addr;		/* phy addr of the start of image */
	char     app_name[MAX_APP_NAME_SIZE];	/* application name*/
};

__packed struct qseecom_unload_app_ireq {
	uint32_t qsee_cmd_id;
	uint32_t  app_id;
};

__packed struct qseecom_load_lib_image_ireq {
	uint32_t qsee_cmd_id;
	uint32_t mdt_len;
	uint32_t img_len;
	uint32_t phy_addr;
};

__packed struct qseecom_unload_lib_image_ireq {
	uint32_t qsee_cmd_id;
};

__packed struct qseecom_register_listener_ireq {
	uint32_t qsee_cmd_id;
	uint32_t listener_id;
	uint32_t sb_ptr;
	uint32_t sb_len;
};

__packed struct qseecom_unregister_listener_ireq {
	uint32_t qsee_cmd_id;
	uint32_t  listener_id;
};

__packed struct qseecom_client_send_data_ireq {
	uint32_t qsee_cmd_id;
	uint32_t app_id;
	uint32_t req_ptr;
	uint32_t req_len;
	uint32_t rsp_ptr;/* First 4 bytes should be the return status */
	uint32_t rsp_len;
};

__packed struct qseecom_reg_log_buf_ireq {
	uint32_t qsee_cmd_id;
	uint32_t phy_addr;
	uint32_t len;
};

/* send_data resp */
__packed struct qseecom_client_listener_data_irsp {
	uint32_t qsee_cmd_id;
	uint32_t listener_id;
	uint32_t status;
};

/*
 * struct qseecom_command_scm_resp - qseecom response buffer
 * @cmd_status: value from enum tz_sched_cmd_status
 * @sb_in_rsp_addr: points to physical location of response
 *                buffer
 * @sb_in_rsp_len: length of command response
 */
__packed struct qseecom_command_scm_resp {
	uint32_t result;
	enum qseecom_command_scm_resp_type resp_type;
	unsigned int data;
};

struct qseecom_rpmb_provision_key {
	uint32_t key_type;
};

__packed struct qseecom_client_send_service_ireq {
	uint32_t qsee_cmd_id;
	uint32_t key_type; /* in */
	unsigned int req_len; /* in */
	uint32_t rsp_ptr; /* in/out */
	unsigned int rsp_len; /* in/out */
};

__packed struct qseecom_key_generate_ireq {
	uint32_t qsee_command_id;
	uint32_t flags;
	uint8_t key_id[QSEECOM_KEY_ID_SIZE];
	uint8_t hash32[QSEECOM_HASH_SIZE];
};

__packed struct qseecom_key_select_ireq {
	uint32_t qsee_command_id;
	uint32_t ce;
	uint32_t pipe;
	uint32_t pipe_type;
	uint32_t flags;
	uint8_t key_id[QSEECOM_KEY_ID_SIZE];
	uint8_t hash32[QSEECOM_HASH_SIZE];
};

__packed struct qseecom_key_delete_ireq {
	uint32_t qsee_command_id;
	uint32_t flags;
	uint8_t key_id[QSEECOM_KEY_ID_SIZE];
	uint8_t hash32[QSEECOM_HASH_SIZE];

};

__packed struct qseecom_key_userinfo_update_ireq {
	uint32_t qsee_command_id;
	uint32_t flags;
	uint8_t key_id[QSEECOM_KEY_ID_SIZE];
	uint8_t current_hash32[QSEECOM_HASH_SIZE];
	uint8_t new_hash32[QSEECOM_HASH_SIZE];
};

__packed struct qseecom_key_max_count_query_ireq {
	uint32_t flags;
};

__packed struct qseecom_key_max_count_query_irsp {
	uint32_t max_key_count;
};

__packed struct qseecom_qteec_ireq {
	uint32_t    qsee_cmd_id;
	uint32_t    app_id;
	uint32_t    req_ptr;
	uint32_t    req_len;
	uint32_t    resp_ptr;
	uint32_t    resp_len;
};

__packed struct qseecom_client_send_fsm_key_req {
	uint32_t qsee_cmd_id;
	uint32_t req_ptr;
	uint32_t req_len;
	uint32_t rsp_ptr;
	uint32_t rsp_len;
};

#endif /* __QSEECOMI_H_ */
