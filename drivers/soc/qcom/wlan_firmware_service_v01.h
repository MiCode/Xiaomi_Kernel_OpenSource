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
#ifndef WLAN_FIRMWARE_SERVICE_V01_H
#define WLAN_FIRMWARE_SERVICE_V01_H

#define WLFW_SERVICE_ID_V01 0x45
#define WLFW_SERVICE_VERS_V01 0x01

#define QMI_WLFW_CAL_REPORT_REQ_V01 0x0026
#define QMI_WLFW_CAL_REPORT_RESP_V01 0x0026
#define QMI_WLFW_CAP_RESP_V01 0x0024
#define QMI_WLFW_BDF_DOWNLOAD_REQ_V01 0x0025
#define QMI_WLFW_CAL_DOWNLOAD_RESP_V01 0x0027
#define QMI_WLFW_WLAN_MODE_RESP_V01 0x0022
#define QMI_WLFW_CAL_UPDATE_RESP_V01 0x0029
#define QMI_WLFW_BDF_DOWNLOAD_RESP_V01 0x0025
#define QMI_WLFW_INITIATE_CAL_UPDATE_IND_V01 0x002A
#define QMI_WLFW_FW_READY_IND_V01 0x0021
#define QMI_WLFW_INITIATE_CAL_DOWNLOAD_IND_V01 0x0028
#define QMI_WLFW_CAP_REQ_V01 0x0024
#define QMI_WLFW_CAL_DOWNLOAD_REQ_V01 0x0027
#define QMI_WLFW_WLAN_MODE_REQ_V01 0x0022
#define QMI_WLFW_CAL_UPDATE_REQ_V01 0x0029
#define QMI_WLFW_IND_REGISTER_REQ_V01 0x0020
#define QMI_WLFW_WLAN_CFG_RESP_V01 0x0023
#define QMI_WLFW_WLAN_CFG_REQ_V01 0x0023
#define QMI_WLFW_IND_REGISTER_RESP_V01 0x0020

#define QMI_WLFW_MAX_NUM_CAL_V01 5
#define QMI_WLFW_MAX_DATA_SIZE_V01 6144
#define QMI_WLFW_MAX_NUM_CE_V01 12
#define QMI_WLFW_MAX_STR_LEN_V01 16
#define QMI_WLFW_MAX_NUM_SHADOW_REG_V01 24
#define QMI_WLFW_MAX_NUM_SVC_V01 24

enum wlfw_driver_mode_enum_v01 {
	WLFW_DRIVER_MODE_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLFW_MISSION_V01 = 0,
	QMI_WLFW_FTM_V01 = 1,
	QMI_WLFW_EPPING_V01 = 2,
	QMI_WLFW_WALTEST_V01 = 3,
	QMI_WLFW_OFF_V01 = 4,
	WLFW_DRIVER_MODE_ENUM_MAX_VAL_V01 = INT_MAX,
};

enum wlfw_cal_temp_id_enum_v01 {
	WLFW_CAL_TEMP_ID_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLFW_CAL_TEMP_IDX_0_V01 = 0,
	QMI_WLFW_CAL_TEMP_IDX_1_V01 = 1,
	QMI_WLFW_CAL_TEMP_IDX_2_V01 = 2,
	QMI_WLFW_CAL_TEMP_IDX_3_V01 = 3,
	QMI_WLFW_CAL_TEMP_IDX_4_V01 = 4,
	WLFW_CAL_TEMP_ID_ENUM_MAX_VAL_V01 = INT_MAX,
};

enum wlfw_pipedir_enum_v01 {
	WLFW_PIPEDIR_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLFW_PIPEDIR_NONE_V01 = 0,
	QMI_WLFW_PIPEDIR_IN_V01 = 1,
	QMI_WLFW_PIPEDIR_OUT_V01 = 2,
	QMI_WLFW_PIPEDIR_INOUT_V01 = 3,
	WLFW_PIPEDIR_ENUM_MAX_VAL_V01 = INT_MAX,
};

#define QMI_WLFW_CE_ATTR_FLAGS_V01 ((uint32_t)0x00)
#define QMI_WLFW_CE_ATTR_NO_SNOOP_V01 ((uint32_t)0x01)
#define QMI_WLFW_CE_ATTR_BYTE_SWAP_DATA_V01 ((uint32_t)0x02)
#define QMI_WLFW_CE_ATTR_SWIZZLE_DESCRIPTORS_V01 ((uint32_t)0x04)
#define QMI_WLFW_CE_ATTR_DISABLE_INTR_V01 ((uint32_t)0x08)
#define QMI_WLFW_CE_ATTR_ENABLE_POLL_V01 ((uint32_t)0x10)

struct wlfw_ce_tgt_pipe_cfg_s_v01 {
	uint32_t pipe_num;
	enum wlfw_pipedir_enum_v01 pipe_dir;
	uint32_t nentries;
	uint32_t nbytes_max;
	uint32_t flags;
};

struct wlfw_ce_svc_pipe_cfg_s_v01 {
	uint32_t service_id;
	enum wlfw_pipedir_enum_v01 pipe_dir;
	uint32_t pipe_num;
};

struct wlfw_shadow_reg_cfg_s_v01 {
	uint16_t id;
	uint16_t offset;
};

struct wlfw_ind_register_req_msg_v01 {
	uint8_t fw_ready_enable_valid;
	uint8_t fw_ready_enable;
	uint8_t initiate_cal_download_enable_valid;
	uint8_t initiate_cal_download_enable;
	uint8_t initiate_cal_update_enable_valid;
	uint8_t initiate_cal_update_enable;
};
#define WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN 12
extern struct elem_info wlfw_ind_register_req_msg_v01_ei[];

struct wlfw_ind_register_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_IND_REGISTER_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_ind_register_resp_msg_v01_ei[];

struct wlfw_fw_ready_ind_msg_v01 {
	char placeholder;
};
#define WLFW_FW_READY_IND_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_fw_ready_ind_msg_v01_ei[];

struct wlfw_wlan_mode_req_msg_v01 {
	enum wlfw_driver_mode_enum_v01 mode;
};
#define WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_wlan_mode_req_msg_v01_ei[];

struct wlfw_wlan_mode_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_WLAN_MODE_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_wlan_mode_resp_msg_v01_ei[];

struct wlfw_wlan_cfg_req_msg_v01 {
	uint8_t host_version_valid;
	char host_version[QMI_WLFW_MAX_STR_LEN_V01 + 1];
	uint8_t tgt_cfg_valid;
	uint32_t tgt_cfg_len;
	struct wlfw_ce_tgt_pipe_cfg_s_v01 tgt_cfg[QMI_WLFW_MAX_NUM_CE_V01];
	uint8_t svc_cfg_valid;
	uint32_t svc_cfg_len;
	struct wlfw_ce_svc_pipe_cfg_s_v01 svc_cfg[QMI_WLFW_MAX_NUM_SVC_V01];
	uint8_t shadow_reg_valid;
	uint32_t shadow_reg_len;
	struct wlfw_shadow_reg_cfg_s_v01 shadow_reg[QMI_WLFW_MAX_NUM_SHADOW_REG_V01];
};
#define WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN 655
extern struct elem_info wlfw_wlan_cfg_req_msg_v01_ei[];

struct wlfw_wlan_cfg_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_WLAN_CFG_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_wlan_cfg_resp_msg_v01_ei[];

struct wlfw_cap_req_msg_v01 {
	char placeholder;
};
#define WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_cap_req_msg_v01_ei[];

struct wlfw_cap_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t board_id_valid;
	uint32_t board_id;
	uint8_t num_peers_valid;
	uint32_t num_peers;
	uint8_t mac_version_valid;
	uint32_t mac_version;
	uint8_t fw_version_valid;
	char fw_version[QMI_WLFW_MAX_STR_LEN_V01 + 1];
};
#define WLFW_CAP_RESP_MSG_V01_MAX_MSG_LEN 47
extern struct elem_info wlfw_cap_resp_msg_v01_ei[];

struct wlfw_bdf_download_req_msg_v01 {
	uint8_t valid;
	uint8_t file_id_valid;
	enum wlfw_cal_temp_id_enum_v01 file_id;
	uint8_t total_size_valid;
	uint32_t total_size;
	uint8_t seg_id_valid;
	uint32_t seg_id;
	uint8_t data_valid;
	uint32_t data_len;
	uint8_t data[QMI_WLFW_MAX_DATA_SIZE_V01];
	uint8_t end_valid;
	uint8_t end;
};
#define WLFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN 6178
extern struct elem_info wlfw_bdf_download_req_msg_v01_ei[];

struct wlfw_bdf_download_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_BDF_DOWNLOAD_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_bdf_download_resp_msg_v01_ei[];

struct wlfw_cal_report_req_msg_v01 {
	uint32_t meta_data_len;
	enum wlfw_cal_temp_id_enum_v01 meta_data[QMI_WLFW_MAX_NUM_CAL_V01];
};
#define WLFW_CAL_REPORT_REQ_MSG_V01_MAX_MSG_LEN 24
extern struct elem_info wlfw_cal_report_req_msg_v01_ei[];

struct wlfw_cal_report_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_CAL_REPORT_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_cal_report_resp_msg_v01_ei[];

struct wlfw_initiate_cal_download_ind_msg_v01 {
	enum wlfw_cal_temp_id_enum_v01 cal_id;
};
#define WLFW_INITIATE_CAL_DOWNLOAD_IND_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_initiate_cal_download_ind_msg_v01_ei[];

struct wlfw_cal_download_req_msg_v01 {
	uint8_t valid;
	uint8_t file_id_valid;
	enum wlfw_cal_temp_id_enum_v01 file_id;
	uint8_t total_size_valid;
	uint32_t total_size;
	uint8_t seg_id_valid;
	uint32_t seg_id;
	uint8_t data_valid;
	uint32_t data_len;
	uint8_t data[QMI_WLFW_MAX_DATA_SIZE_V01];
	uint8_t end_valid;
	uint8_t end;
};
#define WLFW_CAL_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN 6178
extern struct elem_info wlfw_cal_download_req_msg_v01_ei[];

struct wlfw_cal_download_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_CAL_DOWNLOAD_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_cal_download_resp_msg_v01_ei[];

struct wlfw_initiate_cal_update_ind_msg_v01 {
	enum wlfw_cal_temp_id_enum_v01 cal_id;
	uint32_t total_size;
};
#define WLFW_INITIATE_CAL_UPDATE_IND_MSG_V01_MAX_MSG_LEN 14
extern struct elem_info wlfw_initiate_cal_update_ind_msg_v01_ei[];

struct wlfw_cal_update_req_msg_v01 {
	enum wlfw_cal_temp_id_enum_v01 cal_id;
	uint32_t seg_id;
};
#define WLFW_CAL_UPDATE_REQ_MSG_V01_MAX_MSG_LEN 14
extern struct elem_info wlfw_cal_update_req_msg_v01_ei[];

struct wlfw_cal_update_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t file_id_valid;
	enum wlfw_cal_temp_id_enum_v01 file_id;
	uint8_t total_size_valid;
	uint32_t total_size;
	uint8_t seg_id_valid;
	uint32_t seg_id;
	uint8_t data_valid;
	uint32_t data_len;
	uint8_t data[QMI_WLFW_MAX_DATA_SIZE_V01];
	uint8_t end_valid;
	uint8_t end;
};
#define WLFW_CAL_UPDATE_RESP_MSG_V01_MAX_MSG_LEN 6181
extern struct elem_info wlfw_cal_update_resp_msg_v01_ei[];

#endif
