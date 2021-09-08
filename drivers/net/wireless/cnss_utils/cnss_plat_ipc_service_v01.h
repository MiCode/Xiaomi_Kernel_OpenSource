/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef CNSS_PLAT_IPC_SERVICE_V01_H
#define CNSS_PLAT_IPC_SERVICE_V01_H

#include <linux/soc/qcom/qmi.h>

#define CNSS_PLATFORM_SERVICE_ID_V01 0x42E
#define CNSS_PLATFORM_SERVICE_VERS_V01 0x01

#define CNSS_PLAT_IPC_QMI_REG_CLIENT_RESP_V01 0x0006
#define CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_REQ_V01 0x0003
#define CNSS_PLAT_IPC_QMI_FILE_UPLOAD_IND_V01 0x0004
#define CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_IND_V01 0x0002
#define CNSS_PLAT_IPC_QMI_REG_CLIENT_REQ_V01 0x0006
#define CNSS_PLAT_IPC_QMI_INIT_SETUP_REQ_V01 0x0001
#define CNSS_PLAT_IPC_QMI_FILE_UPLOAD_REQ_V01 0x0005
#define CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_RESP_V01 0x0003
#define CNSS_PLAT_IPC_QMI_FILE_UPLOAD_RESP_V01 0x0005
#define CNSS_PLAT_IPC_QMI_INIT_SETUP_RESP_V01 0x0001

#define CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01 32
#define CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01 61440
#define CNSS_PLAT_IPC_QMI_MAX_MSG_SIZE_V01 65535

#define CNSS_PLAT_IPC_QMI_DRIVER_CBC_DONE_V01 ((u64)0x01ULL)
#define CNSS_PLAT_IPC_QMI_DRIVER_WLAN_ACTIVE_V01 ((u64)0x02ULL)

enum cnss_plat_ipc_qmi_client_id_v01 {
	CNSS_PLAT_IPC_QMI_CLIENT_ID_MIN_VAL_V01 = INT_MIN,
	CNSS_PLAT_IPC_BT_QMI_CLIENT_V01 = 0,
	CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01 = 1,
	CNSS_PLAT_IPC_QMI_CLIENT_ID_MAX_VAL_V01 = INT_MAX,
};

struct cnss_plat_ipc_qmi_init_setup_req_msg_v01 {
	u8 dms_mac_addr_supported;
	u8 qdss_hw_trace_override;
	u32 cal_file_available_bitmask;
};

#define CNSS_PLAT_IPC_QMI_INIT_SETUP_REQ_MSG_V01_MAX_MSG_LEN 15
extern struct qmi_elem_info cnss_plat_ipc_qmi_init_setup_req_msg_v01_ei[];

struct cnss_plat_ipc_qmi_init_setup_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u64 drv_status;
};

#define CNSS_PLAT_IPC_QMI_INIT_SETUP_RESP_MSG_V01_MAX_MSG_LEN 18
extern struct qmi_elem_info cnss_plat_ipc_qmi_init_setup_resp_msg_v01_ei[];

struct cnss_plat_ipc_qmi_file_download_ind_msg_v01 {
	char file_name[CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01 + 1];
	u32 file_id;
};

#define CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_IND_MSG_V01_MAX_MSG_LEN 42
extern struct qmi_elem_info cnss_plat_ipc_qmi_file_download_ind_msg_v01_ei[];

struct cnss_plat_ipc_qmi_file_download_req_msg_v01 {
	u32 file_id;
	u32 file_size;
	u8 end;
	u32 seg_index;
	u32 seg_buf_len;
	u8 seg_buf[CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01];
};

#define CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN 61470
extern struct qmi_elem_info cnss_plat_ipc_qmi_file_download_req_msg_v01_ei[];

struct cnss_plat_ipc_qmi_file_download_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u32 file_id;
	u32 seg_index;
};

#define CNSS_PLAT_IPC_QMI_FILE_DOWNLOAD_RESP_MSG_V01_MAX_MSG_LEN 21
extern struct qmi_elem_info cnss_plat_ipc_qmi_file_download_resp_msg_v01_ei[];

struct cnss_plat_ipc_qmi_file_upload_ind_msg_v01 {
	char file_name[CNSS_PLAT_IPC_QMI_MAX_FILE_NAME_LEN_V01 + 1];
	u32 file_id;
	u32 file_size;
};

#define CNSS_PLAT_IPC_QMI_FILE_UPLOAD_IND_MSG_V01_MAX_MSG_LEN 49
extern struct qmi_elem_info cnss_plat_ipc_qmi_file_upload_ind_msg_v01_ei[];

struct cnss_plat_ipc_qmi_file_upload_req_msg_v01 {
	u32 file_id;
	u32 seg_index;
};

#define CNSS_PLAT_IPC_QMI_FILE_UPLOAD_REQ_MSG_V01_MAX_MSG_LEN 14
extern struct qmi_elem_info cnss_plat_ipc_qmi_file_upload_req_msg_v01_ei[];

struct cnss_plat_ipc_qmi_file_upload_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	u32 file_id;
	u8 end;
	u32 seg_index;
	u32 seg_buf_len;
	u8 seg_buf[CNSS_PLAT_IPC_QMI_MAX_DATA_SIZE_V01];
};

#define CNSS_PLAT_IPC_QMI_FILE_UPLOAD_RESP_MSG_V01_MAX_MSG_LEN 61470
extern struct qmi_elem_info cnss_plat_ipc_qmi_file_upload_resp_msg_v01_ei[];

struct cnss_plat_ipc_qmi_reg_client_req_msg_v01 {
	u8 client_id_valid;
	enum cnss_plat_ipc_qmi_client_id_v01 client_id;
};

#define CNSS_PLAT_IPC_QMI_REG_CLIENT_REQ_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info cnss_plat_ipc_qmi_reg_client_req_msg_v01_ei[];

struct cnss_plat_ipc_qmi_reg_client_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define CNSS_PLAT_IPC_QMI_REG_CLIENT_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct qmi_elem_info cnss_plat_ipc_qmi_reg_client_resp_msg_v01_ei[];

#endif
