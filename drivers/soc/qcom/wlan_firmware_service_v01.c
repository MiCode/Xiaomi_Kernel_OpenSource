 /* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/qmi_encdec.h>

#include <soc/qcom/msm_qmi_interface.h>

#include "wlan_firmware_service_v01.h"

static struct elem_info wlfw_ce_tgt_pipe_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_pipedir_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_tgt_pipe_cfg_s_v01,
					   nentries),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_tgt_pipe_cfg_s_v01,
					   nbytes_max),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_tgt_pipe_cfg_s_v01,
					   flags),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info wlfw_ce_svc_pipe_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_svc_pipe_cfg_s_v01,
					   service_id),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_pipedir_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_svc_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_ce_svc_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info wlfw_shadow_reg_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_shadow_reg_cfg_s_v01,
					   id),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_shadow_reg_cfg_s_v01,
					   offset),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info wlfw_memory_region_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint64_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_memory_region_info_s_v01,
					   region_addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_memory_region_info_s_v01,
					   size),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_memory_region_info_s_v01,
					   secure_flag),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info wlfw_rf_chip_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_rf_chip_info_s_v01,
					   chip_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_rf_chip_info_s_v01,
					   chip_family),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info wlfw_rf_board_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_rf_board_info_s_v01,
					   board_id),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info wlfw_soc_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_soc_info_s_v01,
					   soc_id),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

static struct elem_info wlfw_fw_version_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_fw_version_info_s_v01,
					   fw_version),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_TIMESTAMP_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct wlfw_fw_version_info_s_v01,
					   fw_build_timestamp),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_ind_register_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   fw_ready_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   fw_ready_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   msa_ready_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   msa_ready_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   pin_connect_result_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_ind_register_req_msg_v01,
					   pin_connect_result_enable),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_ind_register_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_ind_register_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_fw_ready_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_msa_ready_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_pin_connect_result_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
				   struct wlfw_pin_connect_result_ind_msg_v01,
				   pwr_pin_result_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(
				   struct wlfw_pin_connect_result_ind_msg_v01,
				   pwr_pin_result),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(
				   struct wlfw_pin_connect_result_ind_msg_v01,
				   phy_io_pin_result_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(
				   struct wlfw_pin_connect_result_ind_msg_v01,
				   phy_io_pin_result),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(
				   struct wlfw_pin_connect_result_ind_msg_v01,
				   rf_pin_result_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(
				   struct wlfw_pin_connect_result_ind_msg_v01,
				   rf_pin_result),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_wlan_mode_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_driver_mode_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_wlan_mode_req_msg_v01,
					   mode),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_wlan_mode_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_wlan_mode_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_wlan_cfg_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   host_version_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   host_version),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_CE_V01,
		.elem_size      = sizeof(struct wlfw_ce_tgt_pipe_cfg_s_v01),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   tgt_cfg),
		.ei_array      = wlfw_ce_tgt_pipe_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   svc_cfg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   svc_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_SVC_V01,
		.elem_size      = sizeof(struct wlfw_ce_svc_pipe_cfg_s_v01),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   svc_cfg),
		.ei_array      = wlfw_ce_svc_pipe_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_SHADOW_REG_V01,
		.elem_size      = sizeof(struct wlfw_shadow_reg_cfg_s_v01),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg),
		.ei_array      = wlfw_shadow_reg_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_wlan_cfg_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_wlan_cfg_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cap_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cap_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   chip_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_rf_chip_info_s_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   chip_info),
		.ei_array      = wlfw_rf_chip_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   board_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_rf_board_info_s_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   board_info),
		.ei_array      = wlfw_rf_board_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   soc_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_soc_info_s_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   soc_info),
		.ei_array      = wlfw_soc_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   fw_version_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_fw_version_info_s_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cap_resp_msg_v01,
					   fw_version_info),
		.ei_array      = wlfw_fw_version_info_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_bdf_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   valid),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   file_id_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(uint8_t),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_bdf_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_bdf_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_bdf_download_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cal_report_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_cal_report_req_msg_v01,
					   meta_data_len),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = QMI_WLFW_MAX_NUM_CAL_V01,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_cal_report_req_msg_v01,
					   meta_data),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cal_report_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_cal_report_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_initiate_cal_download_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_initiate_cal_download_ind_msg_v01,
					   cal_id),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cal_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   valid),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   file_id_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(uint8_t),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_cal_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cal_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_cal_download_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_initiate_cal_update_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_initiate_cal_update_ind_msg_v01,
					   cal_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_initiate_cal_update_ind_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cal_update_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_cal_update_req_msg_v01,
					   cal_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_cal_update_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_cal_update_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   file_id_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint16_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(uint8_t),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct wlfw_cal_update_resp_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_msa_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint64_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wlfw_msa_info_req_msg_v01,
					   msa_addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint32_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_msa_info_req_msg_v01,
					   size),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_msa_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_msa_info_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct wlfw_msa_info_resp_msg_v01,
					   mem_region_info_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01,
		.elem_size      = sizeof(struct wlfw_memory_region_info_s_v01),
		.is_array       = VAR_LEN_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct wlfw_msa_info_resp_msg_v01,
					   mem_region_info),
		.ei_array      = wlfw_memory_region_info_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_msa_ready_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_msa_ready_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_msa_ready_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_ini_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_ini_req_msg_v01,
					   enablefwlog_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wlfw_ini_req_msg_v01,
					   enablefwlog),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wlfw_ini_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wlfw_ini_resp_msg_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};
