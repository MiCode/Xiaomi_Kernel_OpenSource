 /* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#define QMI_WLFW_BDF_DOWNLOAD_REQ_V01 0x0025
#define QMI_WLFW_FW_MEM_READY_IND_V01 0x0037
#define QMI_WLFW_INITIATE_CAL_UPDATE_IND_V01 0x002A
#define QMI_WLFW_HOST_CAP_REQ_V01 0x0034
#define QMI_WLFW_DYNAMIC_FEATURE_MASK_RESP_V01 0x003B
#define QMI_WLFW_CAP_REQ_V01 0x0024
#define QMI_WLFW_CAL_REPORT_REQ_V01 0x0026
#define QMI_WLFW_CAL_UPDATE_RESP_V01 0x0029
#define QMI_WLFW_CAL_DOWNLOAD_RESP_V01 0x0027
#define QMI_WLFW_INI_RESP_V01 0x002F
#define QMI_WLFW_CAL_REPORT_RESP_V01 0x0026
#define QMI_WLFW_MAC_ADDR_RESP_V01 0x0033
#define QMI_WLFW_INITIATE_CAL_DOWNLOAD_IND_V01 0x0028
#define QMI_WLFW_HOST_CAP_RESP_V01 0x0034
#define QMI_WLFW_MSA_READY_IND_V01 0x002B
#define QMI_WLFW_ATHDIAG_WRITE_RESP_V01 0x0031
#define QMI_WLFW_WLAN_MODE_REQ_V01 0x0022
#define QMI_WLFW_IND_REGISTER_REQ_V01 0x0020
#define QMI_WLFW_WLAN_CFG_RESP_V01 0x0023
#define QMI_WLFW_COLD_BOOT_CAL_DONE_IND_V01 0x0038
#define QMI_WLFW_REQUEST_MEM_IND_V01 0x0035
#define QMI_WLFW_REJUVENATE_IND_V01 0x0039
#define QMI_WLFW_DYNAMIC_FEATURE_MASK_REQ_V01 0x003B
#define QMI_WLFW_ATHDIAG_WRITE_REQ_V01 0x0031
#define QMI_WLFW_WLAN_MODE_RESP_V01 0x0022
#define QMI_WLFW_RESPOND_MEM_REQ_V01 0x0036
#define QMI_WLFW_PIN_CONNECT_RESULT_IND_V01 0x002C
#define QMI_WLFW_FW_READY_IND_V01 0x0021
#define QMI_WLFW_MSA_READY_RESP_V01 0x002E
#define QMI_WLFW_CAL_UPDATE_REQ_V01 0x0029
#define QMI_WLFW_INI_REQ_V01 0x002F
#define QMI_WLFW_BDF_DOWNLOAD_RESP_V01 0x0025
#define QMI_WLFW_REJUVENATE_ACK_RESP_V01 0x003A
#define QMI_WLFW_MSA_INFO_RESP_V01 0x002D
#define QMI_WLFW_MSA_READY_REQ_V01 0x002E
#define QMI_WLFW_CAP_RESP_V01 0x0024
#define QMI_WLFW_REJUVENATE_ACK_REQ_V01 0x003A
#define QMI_WLFW_ATHDIAG_READ_RESP_V01 0x0030
#define QMI_WLFW_VBATT_REQ_V01 0x0032
#define QMI_WLFW_MAC_ADDR_REQ_V01 0x0033
#define QMI_WLFW_RESPOND_MEM_RESP_V01 0x0036
#define QMI_WLFW_VBATT_RESP_V01 0x0032
#define QMI_WLFW_MSA_INFO_REQ_V01 0x002D
#define QMI_WLFW_CAL_DOWNLOAD_REQ_V01 0x0027
#define QMI_WLFW_ATHDIAG_READ_REQ_V01 0x0030
#define QMI_WLFW_WLAN_CFG_REQ_V01 0x0023
#define QMI_WLFW_IND_REGISTER_RESP_V01 0x0020

#define QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01 2
#define QMI_WLFW_MAX_NUM_CAL_V01 5
#define QMI_WLFW_MAX_DATA_SIZE_V01 6144
#define QMI_WLFW_FUNCTION_NAME_LEN_V01 128
#define QMI_WLFW_MAX_NUM_CE_V01 12
#define QMI_WLFW_MAX_TIMESTAMP_LEN_V01 32
#define QMI_WLFW_MAX_BUILD_ID_LEN_V01 128
#define QMI_WLFW_MAX_STR_LEN_V01 16
#define QMI_WLFW_MAX_NUM_SHADOW_REG_V01 24
#define QMI_WLFW_MAC_ADDR_SIZE_V01 6
#define QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01 36
#define QMI_WLFW_MAX_NUM_SVC_V01 24

enum wlfw_driver_mode_enum_v01 {
	WLFW_DRIVER_MODE_ENUM_MIN_VAL_V01 = INT_MIN,
	QMI_WLFW_MISSION_V01 = 0,
	QMI_WLFW_FTM_V01 = 1,
	QMI_WLFW_EPPING_V01 = 2,
	QMI_WLFW_WALTEST_V01 = 3,
	QMI_WLFW_OFF_V01 = 4,
	QMI_WLFW_CCPM_V01 = 5,
	QMI_WLFW_QVIT_V01 = 6,
	QMI_WLFW_CALIBRATION_V01 = 7,
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

#define QMI_WLFW_ALREADY_REGISTERED_V01 ((uint64_t)0x01ULL)
#define QMI_WLFW_FW_READY_V01 ((uint64_t)0x02ULL)
#define QMI_WLFW_MSA_READY_V01 ((uint64_t)0x04ULL)
#define QMI_WLFW_FW_MEM_READY_V01 ((uint64_t)0x08ULL)

#define QMI_WLFW_FW_REJUVENATE_V01 ((uint64_t)0x01ULL)

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

struct wlfw_shadow_reg_v2_cfg_s_v01 {
	uint32_t addr;
};

struct wlfw_memory_region_info_s_v01 {
	uint64_t region_addr;
	uint32_t size;
	uint8_t secure_flag;
};

struct wlfw_rf_chip_info_s_v01 {
	uint32_t chip_id;
	uint32_t chip_family;
};

struct wlfw_rf_board_info_s_v01 {
	uint32_t board_id;
};

struct wlfw_soc_info_s_v01 {
	uint32_t soc_id;
};

struct wlfw_fw_version_info_s_v01 {
	uint32_t fw_version;
	char fw_build_timestamp[QMI_WLFW_MAX_TIMESTAMP_LEN_V01 + 1];
};

struct wlfw_ind_register_req_msg_v01 {
	uint8_t fw_ready_enable_valid;
	uint8_t fw_ready_enable;
	uint8_t initiate_cal_download_enable_valid;
	uint8_t initiate_cal_download_enable;
	uint8_t initiate_cal_update_enable_valid;
	uint8_t initiate_cal_update_enable;
	uint8_t msa_ready_enable_valid;
	uint8_t msa_ready_enable;
	uint8_t pin_connect_result_enable_valid;
	uint8_t pin_connect_result_enable;
	uint8_t client_id_valid;
	uint32_t client_id;
	uint8_t request_mem_enable_valid;
	uint8_t request_mem_enable;
	uint8_t fw_mem_ready_enable_valid;
	uint8_t fw_mem_ready_enable;
	uint8_t cold_boot_cal_done_enable_valid;
	uint8_t cold_boot_cal_done_enable;
	uint8_t rejuvenate_enable_valid;
	uint32_t rejuvenate_enable;
};
#define WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN 46
extern struct elem_info wlfw_ind_register_req_msg_v01_ei[];

struct wlfw_ind_register_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t fw_status_valid;
	uint64_t fw_status;
};
#define WLFW_IND_REGISTER_RESP_MSG_V01_MAX_MSG_LEN 18
extern struct elem_info wlfw_ind_register_resp_msg_v01_ei[];

struct wlfw_fw_ready_ind_msg_v01 {
	char placeholder;
};
#define WLFW_FW_READY_IND_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_fw_ready_ind_msg_v01_ei[];

struct wlfw_msa_ready_ind_msg_v01 {
	char placeholder;
};
#define WLFW_MSA_READY_IND_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_msa_ready_ind_msg_v01_ei[];

struct wlfw_pin_connect_result_ind_msg_v01 {
	uint8_t pwr_pin_result_valid;
	uint32_t pwr_pin_result;
	uint8_t phy_io_pin_result_valid;
	uint32_t phy_io_pin_result;
	uint8_t rf_pin_result_valid;
	uint32_t rf_pin_result;
};
#define WLFW_PIN_CONNECT_RESULT_IND_MSG_V01_MAX_MSG_LEN 21
extern struct elem_info wlfw_pin_connect_result_ind_msg_v01_ei[];

struct wlfw_wlan_mode_req_msg_v01 {
	enum wlfw_driver_mode_enum_v01 mode;
	uint8_t hw_debug_valid;
	uint8_t hw_debug;
};
#define WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN 11
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
	uint8_t shadow_reg_v2_valid;
	uint32_t shadow_reg_v2_len;
	struct wlfw_shadow_reg_v2_cfg_s_v01
	shadow_reg_v2[QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01];
};
#define WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN 803
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
	uint8_t chip_info_valid;
	struct wlfw_rf_chip_info_s_v01 chip_info;
	uint8_t board_info_valid;
	struct wlfw_rf_board_info_s_v01 board_info;
	uint8_t soc_info_valid;
	struct wlfw_soc_info_s_v01 soc_info;
	uint8_t fw_version_info_valid;
	struct wlfw_fw_version_info_s_v01 fw_version_info;
	uint8_t fw_build_id_valid;
	char fw_build_id[QMI_WLFW_MAX_BUILD_ID_LEN_V01 + 1];
};
#define WLFW_CAP_RESP_MSG_V01_MAX_MSG_LEN 203
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

struct wlfw_msa_info_req_msg_v01 {
	uint64_t msa_addr;
	uint32_t size;
};
#define WLFW_MSA_INFO_REQ_MSG_V01_MAX_MSG_LEN 18
extern struct elem_info wlfw_msa_info_req_msg_v01_ei[];

struct wlfw_msa_info_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint32_t mem_region_info_len;
	struct wlfw_memory_region_info_s_v01
	mem_region_info[QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01];
};
#define WLFW_MSA_INFO_RESP_MSG_V01_MAX_MSG_LEN 37
extern struct elem_info wlfw_msa_info_resp_msg_v01_ei[];

struct wlfw_msa_ready_req_msg_v01 {
	char placeholder;
};
#define WLFW_MSA_READY_REQ_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_msa_ready_req_msg_v01_ei[];

struct wlfw_msa_ready_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_MSA_READY_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_msa_ready_resp_msg_v01_ei[];

struct wlfw_ini_req_msg_v01 {
	uint8_t enablefwlog_valid;
	uint8_t enablefwlog;
};
#define WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN 4
extern struct elem_info wlfw_ini_req_msg_v01_ei[];

struct wlfw_ini_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_INI_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_ini_resp_msg_v01_ei[];

struct wlfw_athdiag_read_req_msg_v01 {
	uint32_t offset;
	uint32_t mem_type;
	uint32_t data_len;
};
#define WLFW_ATHDIAG_READ_REQ_MSG_V01_MAX_MSG_LEN 21
extern struct elem_info wlfw_athdiag_read_req_msg_v01_ei[];

struct wlfw_athdiag_read_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t data_valid;
	uint32_t data_len;
	uint8_t data[QMI_WLFW_MAX_DATA_SIZE_V01];
};
#define WLFW_ATHDIAG_READ_RESP_MSG_V01_MAX_MSG_LEN 6156
extern struct elem_info wlfw_athdiag_read_resp_msg_v01_ei[];

struct wlfw_athdiag_write_req_msg_v01 {
	uint32_t offset;
	uint32_t mem_type;
	uint32_t data_len;
	uint8_t data[QMI_WLFW_MAX_DATA_SIZE_V01];
};
#define WLFW_ATHDIAG_WRITE_REQ_MSG_V01_MAX_MSG_LEN 6163
extern struct elem_info wlfw_athdiag_write_req_msg_v01_ei[];

struct wlfw_athdiag_write_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_ATHDIAG_WRITE_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_athdiag_write_resp_msg_v01_ei[];

struct wlfw_vbatt_req_msg_v01 {
	uint64_t voltage_uv;
};
#define WLFW_VBATT_REQ_MSG_V01_MAX_MSG_LEN 11
extern struct elem_info wlfw_vbatt_req_msg_v01_ei[];

struct wlfw_vbatt_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_VBATT_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_vbatt_resp_msg_v01_ei[];

struct wlfw_mac_addr_req_msg_v01 {
	uint8_t mac_addr_valid;
	uint8_t mac_addr[QMI_WLFW_MAC_ADDR_SIZE_V01];
};
#define WLFW_MAC_ADDR_REQ_MSG_V01_MAX_MSG_LEN 9
extern struct elem_info wlfw_mac_addr_req_msg_v01_ei[];

struct wlfw_mac_addr_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_MAC_ADDR_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_mac_addr_resp_msg_v01_ei[];

struct wlfw_host_cap_req_msg_v01 {
	uint8_t daemon_support_valid;
	uint8_t daemon_support;
};
#define WLFW_HOST_CAP_REQ_MSG_V01_MAX_MSG_LEN 4
extern struct elem_info wlfw_host_cap_req_msg_v01_ei[];

struct wlfw_host_cap_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_HOST_CAP_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_host_cap_resp_msg_v01_ei[];

struct wlfw_request_mem_ind_msg_v01 {
	uint32_t size;
};
#define WLFW_REQUEST_MEM_IND_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_request_mem_ind_msg_v01_ei[];

struct wlfw_respond_mem_req_msg_v01 {
	uint64_t addr;
	uint32_t size;
};
#define WLFW_RESPOND_MEM_REQ_MSG_V01_MAX_MSG_LEN 18
extern struct elem_info wlfw_respond_mem_req_msg_v01_ei[];

struct wlfw_respond_mem_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_RESPOND_MEM_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_respond_mem_resp_msg_v01_ei[];

struct wlfw_fw_mem_ready_ind_msg_v01 {
	char placeholder;
};
#define WLFW_FW_MEM_READY_IND_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_fw_mem_ready_ind_msg_v01_ei[];

struct wlfw_cold_boot_cal_done_ind_msg_v01 {
	char placeholder;
};
#define WLFW_COLD_BOOT_CAL_DONE_IND_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_cold_boot_cal_done_ind_msg_v01_ei[];

struct wlfw_rejuvenate_ind_msg_v01 {
	uint8_t cause_for_rejuvenation_valid;
	uint8_t cause_for_rejuvenation;
	uint8_t requesting_sub_system_valid;
	uint8_t requesting_sub_system;
	uint8_t line_number_valid;
	uint16_t line_number;
	uint8_t function_name_valid;
	char function_name[QMI_WLFW_FUNCTION_NAME_LEN_V01 + 1];
};
#define WLFW_REJUVENATE_IND_MSG_V01_MAX_MSG_LEN 144
extern struct elem_info wlfw_rejuvenate_ind_msg_v01_ei[];

struct wlfw_rejuvenate_ack_req_msg_v01 {
	char placeholder;
};
#define WLFW_REJUVENATE_ACK_REQ_MSG_V01_MAX_MSG_LEN 0
extern struct elem_info wlfw_rejuvenate_ack_req_msg_v01_ei[];

struct wlfw_rejuvenate_ack_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};
#define WLFW_REJUVENATE_ACK_RESP_MSG_V01_MAX_MSG_LEN 7
extern struct elem_info wlfw_rejuvenate_ack_resp_msg_v01_ei[];

struct wlfw_dynamic_feature_mask_req_msg_v01 {
	uint8_t mask_valid;
	uint64_t mask;
};
#define WLFW_DYNAMIC_FEATURE_MASK_REQ_MSG_V01_MAX_MSG_LEN 11
extern struct elem_info wlfw_dynamic_feature_mask_req_msg_v01_ei[];

struct wlfw_dynamic_feature_mask_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
	uint8_t prev_mask_valid;
	uint64_t prev_mask;
	uint8_t curr_mask_valid;
	uint64_t curr_mask;
};
#define WLFW_DYNAMIC_FEATURE_MASK_RESP_MSG_V01_MAX_MSG_LEN 29
extern struct elem_info wlfw_dynamic_feature_mask_resp_msg_v01_ei[];

#endif
