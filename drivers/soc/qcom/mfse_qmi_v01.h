 /* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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
#ifndef MODEM_FILESYSTEM_EXTERNAL_V01_H
#define MODEM_FILESYSTEM_EXTERNAL_V01_H

#define MFSE_SERVICE_ID_V01 0x2C
#define MFSE_SERVICE_VERS_V01 0x01

#define QMI_MFSE_SET_EFS_SYNC_TIMER_REQ_V01 0x0022
#define QMI_MFSE_GET_SUPPORTED_MSGS_REQ_V01 0x001E
#define QMI_MFSE_GET_EFS_SYNC_TIMER_REQ_V01 0x0023
#define QMI_MFSE_GET_SUPPORTED_FIELDS_RESP_V01 0x001F
#define QMI_MFSE_GET_SUPPORTED_FIELDS_REQ_V01 0x001F
#define QMI_MFSE_SYNC_GET_STATUS_REQ_V01 0x0021
#define QMI_MFSE_SYNC_NO_WAIT_REQ_V01 0x0020
#define QMI_MFSE_SET_EFS_SYNC_TIMER_RESP_V01 0x0022
#define QMI_MFSE_SYNC_NO_WAIT_RESP_V01 0x0020
#define QMI_MFSE_GET_SUPPORTED_MSGS_RESP_V01 0x001E
#define QMI_MFSE_SYNC_GET_STATUS_RESP_V01 0x0021
#define QMI_MFSE_GET_EFS_SYNC_TIMER_RESP_V01 0x0023


enum mfse_errno_type_v01 {
	MFSE_ERRNO_TYPE_MIN_VAL_V01 = INT_MIN,
	MFSE_ENOERR_V01 = 0,
	MFSE_EPERM_V01 = 1,
	MFSE_ENOENT_V01 = 2,
	MFSE_EEXIST_V01 = 3,
	MFSE_EBADF_V01 = 4,
	MFSE_ENOMEM_V01 = 5,
	MFSE_EACCES_V01 = 6,
	MFSE_EBUSY_V01 = 7,
	MFSE_EXDEV_V01 = 8,
	MFSE_ENODEV_V01 = 9,
	MFSE_ENOTDIR_V01 = 10,
	MFSE_EISDIR_V01 = 11,
	MFSE_EINVAL_V01 = 12,
	MFSE_EMFILE_V01 = 13,
	MFSE_ETXTBSY_V01 = 14,
	MFSE_ENOSPC_V01 = 15,
	MFSE_ESPIPE_V01 = 16,
	MFSE_ENAMETOOLONG_V01 = 17,
	MFSE_ENOTEMPTY_V01 = 18,
	MFSE_ELOOP_V01 = 19,
	MFSE_EILSEQ_V01 = 20,
	MFSE_ESTALE_V01 = 21,
	MFSE_EDQUOT_V01 = 22,
	MFSE_ENOCARD_V01 = 23,
	MFSE_EBADFMT_V01 = 24,
	MFSE_ENOTITM_V01 = 25,
	MFSE_EROLLBACK_V01 = 26,
	MFSE_FS_ERANGE_V01 = 27,
	MFSE_EEOF_V01 = 28,
	MFSE_EUNKNOWN_SFAT_V01 = 29,
	MFSE_EUNKNOWN_HFAT_V01 = 30,
	MFSE_ENOTHINGTOSYNC_V01 = 31,
	MFSE_ERRNO_TYPE_MAX_VAL_V01 = INT_MAX,
};

enum mfse_filesystem_id_v01 {
	MFSE_FILESYSTEM_ID_MIN_VAL_V01 = INT_MIN,
	MFSE_EFS2_V01 = 0,
	MFSE_FILESYSTEM_ID_MAX_VAL_V01 = INT_MAX,
};

struct mfse_sync_no_wait_req_msg_v01 {
	enum mfse_filesystem_id_v01 fs_id;
};
#define MFSE_SYNC_NO_WAIT_REQ_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info mfse_sync_no_wait_req_msg_v01_ei[];

struct mfse_sync_no_wait_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t sync_token_valid;
	uint32_t sync_token;
	uint8_t efs_err_num_valid;
	enum mfse_errno_type_v01 efs_err_num;
};
#define MFSE_SYNC_NO_WAIT_RESP_MSG_V01_MAX_MSG_LEN 21
extern struct elem_info mfse_sync_no_wait_resp_msg_v01_ei[];

enum mfse_sync_status_v01 {
	MFSE_SYNC_STATUS_MIN_VAL_V01 = INT_MIN,
	MFSE_SYNC_PENDING_V01 = 0,
	MFSE_SYNC_COMPLETE_V01 = 1,
	MFSE_SYNC_STATUS_MAX_VAL_V01 = INT_MAX,
};

struct mfse_sync_get_status_req_msg_v01 {
	enum mfse_filesystem_id_v01 fs_id;
	uint32_t sync_token;
};
#define MFSE_SYNC_GET_STATUS_REQ_MSG_V01_MAX_MSG_LEN 14
extern struct elem_info mfse_sync_get_status_req_msg_v01_ei[];

struct mfse_sync_get_status_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t sync_status_valid;
	enum mfse_sync_status_v01 sync_status;
	uint8_t efs_err_num_valid;
	enum mfse_errno_type_v01 efs_err_num;
};
#define MFSE_SYNC_GET_STATUS_RESP_MSG_V01_MAX_MSG_LEN 21
extern struct elem_info mfse_sync_get_status_resp_msg_v01_ei[];

struct mfse_set_efs_sync_timer_req_msg_v01 {
	enum mfse_filesystem_id_v01 fs_id;
	uint32_t efs_timer_value;
};
#define MFSE_SET_EFS_SYNC_TIMER_REQ_MSG_V01_MAX_MSG_LEN 14
extern struct elem_info mfse_set_efs_sync_timer_req_msg_v01_ei[];

struct mfse_set_efs_sync_timer_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t efs_err_num_valid;
	enum mfse_errno_type_v01 efs_err_num;
};
#define MFSE_SET_EFS_SYNC_TIMER_RESP_MSG_V01_MAX_MSG_LEN 14
extern struct elem_info mfse_set_efs_sync_timer_resp_msg_v01_ei[];

struct mfse_get_efs_sync_timer_req_msg_v01 {
	enum mfse_filesystem_id_v01 fs_id;
};
#define MFSE_GET_EFS_SYNC_TIMER_REQ_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info mfse_get_efs_sync_timer_req_msg_v01_ei[];

struct mfse_get_efs_sync_timer_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t efs_timer_value_valid;
	uint32_t efs_timer_value;
	uint8_t efs_err_num_valid;
	enum mfse_errno_type_v01 efs_err_num;
};
#define MFSE_GET_EFS_SYNC_TIMER_RESP_MSG_V01_MAX_MSG_LEN 21
extern struct elem_info mfse_get_efs_sync_timer_resp_msg_v01_ei[];

#endif
