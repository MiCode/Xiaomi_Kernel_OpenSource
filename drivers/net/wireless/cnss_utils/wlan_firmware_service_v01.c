// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "wlan_firmware_service_v01.h"
#include <linux/module.h>
#include <linux/of.h>

static struct qmi_elem_info wlfw_ce_tgt_pipe_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_pipedir_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_tgt_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_tgt_pipe_cfg_s_v01,
					   nentries),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_tgt_pipe_cfg_s_v01,
					   nbytes_max),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_tgt_pipe_cfg_s_v01,
					   flags),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_ce_svc_pipe_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_svc_pipe_cfg_s_v01,
					   service_id),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_pipedir_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_svc_pipe_cfg_s_v01,
					   pipe_dir),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_ce_svc_pipe_cfg_s_v01,
					   pipe_num),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_shadow_reg_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_shadow_reg_cfg_s_v01,
					   id),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_shadow_reg_cfg_s_v01,
					   offset),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_shadow_reg_v2_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_shadow_reg_v2_cfg_s_v01,
					   addr),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_rri_over_ddr_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_rri_over_ddr_cfg_s_v01,
					   base_addr_low),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_rri_over_ddr_cfg_s_v01,
					   base_addr_high),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_msi_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_msi_cfg_s_v01,
					   ce_id),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_msi_cfg_s_v01,
					   msi_vector),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_memory_region_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_memory_region_info_s_v01,
					   region_addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_memory_region_info_s_v01,
					   size),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_memory_region_info_s_v01,
					   secure_flag),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_mem_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_cfg_s_v01,
					   offset),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_cfg_s_v01,
					   size),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_cfg_s_v01,
					   secure_flag),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_mem_seg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_s_v01,
					   size),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_mem_type_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_s_v01,
					   type),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_s_v01,
					   mem_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEM_CFG_V01,
		.elem_size      = sizeof(struct wlfw_mem_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_s_v01,
					   mem_cfg),
		.ei_array      = wlfw_mem_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_mem_seg_resp_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_resp_s_v01,
					   addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_resp_s_v01,
					   size),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_mem_type_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_resp_s_v01,
					   type),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_mem_seg_resp_s_v01,
					   restore),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_rf_chip_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_rf_chip_info_s_v01,
					   chip_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_rf_chip_info_s_v01,
					   chip_family),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_rf_board_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_rf_board_info_s_v01,
					   board_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_soc_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_soc_info_s_v01,
					   soc_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_fw_version_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_fw_version_info_s_v01,
					   fw_version),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_TIMESTAMP_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_fw_version_info_s_v01,
					   fw_build_timestamp),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_host_ddr_range_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_host_ddr_range_s_v01,
					   start),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_host_ddr_range_s_v01,
					   size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_m3_segment_info_s_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_m3_segment_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_m3_segment_info_s_v01,
					   type),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_m3_segment_info_s_v01,
					   addr),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_m3_segment_info_s_v01,
					   size),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_m3_segment_info_s_v01,
					   name),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_dev_mem_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_dev_mem_info_s_v01,
					   start),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_dev_mem_info_s_v01,
					   size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_host_mlo_chip_info_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_host_mlo_chip_info_s_v01,
					   chip_id),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_host_mlo_chip_info_s_v01,
					   num_local_links),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_host_mlo_chip_info_s_v01,
					   hw_link_id),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_LINKS_PER_CHIP_V01,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_host_mlo_chip_info_s_v01,
					   valid_mlo_link_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

static struct qmi_elem_info wlfw_shadow_reg_v3_cfg_s_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0,
		.offset         = offsetof(struct
					   wlfw_shadow_reg_v3_cfg_s_v01,
					   addr),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct qmi_elem_info wlfw_ind_register_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   fw_ready_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   fw_ready_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   initiate_cal_download_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   initiate_cal_update_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   msa_ready_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   msa_ready_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   pin_connect_result_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   pin_connect_result_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   client_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   client_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   request_mem_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   request_mem_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   fw_mem_ready_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   fw_mem_ready_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   fw_init_done_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   fw_init_done_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   rejuvenate_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   rejuvenate_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   xo_cal_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   xo_cal_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   cal_done_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   cal_done_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_trace_req_mem_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_trace_req_mem_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_trace_save_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_trace_save_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1E,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_trace_free_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1E,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_trace_free_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1F,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   respond_get_info_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1F,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   respond_get_info_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   m3_dump_upload_req_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   m3_dump_upload_req_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x21,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   wfc_call_twt_config_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x21,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   wfc_call_twt_config_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x22,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_mem_ready_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x22,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   qdss_mem_ready_enable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x23,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   m3_dump_upload_segments_req_enable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x23,
		.offset         = offsetof(struct
					   wlfw_ind_register_req_msg_v01,
					   m3_dump_upload_segments_req_enable),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_ind_register_req_msg_v01_ei);

struct qmi_elem_info wlfw_ind_register_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_ind_register_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ind_register_resp_msg_v01,
					   fw_status_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ind_register_resp_msg_v01,
					   fw_status),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_ind_register_resp_msg_v01_ei);

struct qmi_elem_info wlfw_fw_ready_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_fw_ready_ind_msg_v01_ei);

struct qmi_elem_info wlfw_msa_ready_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_msa_ready_ind_msg_v01,
					   hang_data_addr_offset_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_msa_ready_ind_msg_v01,
					   hang_data_addr_offset),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_msa_ready_ind_msg_v01,
					   hang_data_length_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_msa_ready_ind_msg_v01,
					   hang_data_length),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_msa_ready_ind_msg_v01_ei);

struct qmi_elem_info wlfw_pin_connect_result_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_pin_connect_result_ind_msg_v01,
					   pwr_pin_result_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_pin_connect_result_ind_msg_v01,
					   pwr_pin_result),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_pin_connect_result_ind_msg_v01,
					   phy_io_pin_result_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_pin_connect_result_ind_msg_v01,
					   phy_io_pin_result),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_pin_connect_result_ind_msg_v01,
					   rf_pin_result_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_pin_connect_result_ind_msg_v01,
					   rf_pin_result),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_pin_connect_result_ind_msg_v01_ei);

struct qmi_elem_info wlfw_wlan_mode_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_driver_mode_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_wlan_mode_req_msg_v01,
					   mode),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wlan_mode_req_msg_v01,
					   hw_debug_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wlan_mode_req_msg_v01,
					   hw_debug),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wlan_mode_req_msg_v01,
					   xo_cal_data_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wlan_mode_req_msg_v01,
					   xo_cal_data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_wlan_mode_req_msg_v01_ei);

struct qmi_elem_info wlfw_wlan_mode_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_wlan_mode_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_wlan_mode_resp_msg_v01_ei);

struct qmi_elem_info wlfw_wlan_cfg_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   host_version_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   host_version),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   tgt_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_CE_V01,
		.elem_size      = sizeof(struct wlfw_ce_tgt_pipe_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   tgt_cfg),
		.ei_array      = wlfw_ce_tgt_pipe_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   svc_cfg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   svc_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_SVC_V01,
		.elem_size      = sizeof(struct wlfw_ce_svc_pipe_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   svc_cfg),
		.ei_array      = wlfw_ce_svc_pipe_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_SHADOW_REG_V01,
		.elem_size      = sizeof(struct wlfw_shadow_reg_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg),
		.ei_array      = wlfw_shadow_reg_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v2_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v2_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01,
		.elem_size      = sizeof(struct wlfw_shadow_reg_v2_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v2),
		.ei_array      = wlfw_shadow_reg_v2_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   rri_over_ddr_cfg_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_rri_over_ddr_cfg_s_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   rri_over_ddr_cfg),
		.ei_array      = wlfw_rri_over_ddr_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   msi_cfg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   msi_cfg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_CE_V01,
		.elem_size      = sizeof(struct wlfw_msi_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   msi_cfg),
		.ei_array      = wlfw_msi_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v3_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v3_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_SHADOW_REG_V3_V01,
		.elem_size      = sizeof(struct wlfw_shadow_reg_v3_cfg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_req_msg_v01,
					   shadow_reg_v3),
		.ei_array      = wlfw_shadow_reg_v3_cfg_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_wlan_cfg_req_msg_v01_ei);

struct qmi_elem_info wlfw_wlan_cfg_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_wlan_cfg_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_wlan_cfg_resp_msg_v01_ei);

struct qmi_elem_info wlfw_cap_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cap_req_msg_v01_ei);

struct qmi_elem_info wlfw_cap_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   chip_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_rf_chip_info_s_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   chip_info),
		.ei_array      = wlfw_rf_chip_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   board_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_rf_board_info_s_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   board_info),
		.ei_array      = wlfw_rf_board_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   soc_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_soc_info_s_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   soc_info),
		.ei_array      = wlfw_soc_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   fw_version_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct wlfw_fw_version_info_s_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   fw_version_info),
		.ei_array      = wlfw_fw_version_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   fw_build_id_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_BUILD_ID_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   fw_build_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   num_macs_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   num_macs),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   voltage_mv_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   voltage_mv),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   time_freq_hz_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   time_freq_hz),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   otp_version_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   otp_version),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   eeprom_caldata_read_timeout_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   eeprom_caldata_read_timeout),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   fw_caps_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   fw_caps),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   rd_card_chain_cap_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_rd_card_chain_cap_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   rd_card_chain_cap),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   dev_mem_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_DEV_MEM_NUM_V01,
		.elem_size      = sizeof(struct wlfw_dev_mem_info_s_v01),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   dev_mem_info),
		.ei_array      = wlfw_dev_mem_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   foundry_name_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   foundry_name),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1E,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   hang_data_addr_offset_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1E,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   hang_data_addr_offset),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1F,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   hang_data_length_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1F,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   hang_data_length),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   bdf_dnld_method_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_bdf_dnld_method_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   bdf_dnld_method),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x21,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   hwid_bitmap_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x21,
		.offset         = offsetof(struct
					   wlfw_cap_resp_msg_v01,
					   hwid_bitmap),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cap_resp_msg_v01_ei);

struct qmi_elem_info wlfw_bdf_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   valid),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   file_id_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   bdf_type_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_bdf_download_req_msg_v01,
					   bdf_type),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_bdf_download_req_msg_v01_ei);

struct qmi_elem_info wlfw_bdf_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_bdf_download_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_bdf_download_resp_msg_v01,
					   host_bdf_data_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_bdf_download_resp_msg_v01,
					   host_bdf_data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_bdf_download_resp_msg_v01_ei);

struct qmi_elem_info wlfw_cal_report_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   meta_data_len),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = QMI_WLFW_MAX_NUM_CAL_V01,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   meta_data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   xo_cal_data_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   xo_cal_data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   cal_remove_supported_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   cal_remove_supported),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   cal_file_download_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cal_report_req_msg_v01,
					   cal_file_download_size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cal_report_req_msg_v01_ei);

struct qmi_elem_info wlfw_cal_report_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_cal_report_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cal_report_resp_msg_v01_ei);

struct qmi_elem_info wlfw_initiate_cal_download_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_download_ind_msg_v01,
					   cal_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_download_ind_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_download_ind_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_download_ind_msg_v01,
					   cal_data_location_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_download_ind_msg_v01,
					   cal_data_location),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_initiate_cal_download_ind_msg_v01_ei);

struct qmi_elem_info wlfw_cal_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   valid),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   file_id_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   cal_data_location_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_cal_download_req_msg_v01,
					   cal_data_location),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cal_download_req_msg_v01_ei);

struct qmi_elem_info wlfw_cal_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_cal_download_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cal_download_resp_msg_v01_ei);

struct qmi_elem_info wlfw_initiate_cal_update_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_update_ind_msg_v01,
					   cal_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_update_ind_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_update_ind_msg_v01,
					   cal_data_location_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_initiate_cal_update_ind_msg_v01,
					   cal_data_location),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_initiate_cal_update_ind_msg_v01_ei);

struct qmi_elem_info wlfw_cal_update_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_cal_update_req_msg_v01,
					   cal_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_cal_update_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cal_update_req_msg_v01_ei);

struct qmi_elem_info wlfw_cal_update_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   file_id_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_cal_temp_id_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   file_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   cal_data_location_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_cal_update_resp_msg_v01,
					   cal_data_location),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cal_update_resp_msg_v01_ei);

struct qmi_elem_info wlfw_msa_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_msa_info_req_msg_v01,
					   msa_addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_msa_info_req_msg_v01,
					   size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_msa_info_req_msg_v01_ei);

struct qmi_elem_info wlfw_msa_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_msa_info_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wlfw_msa_info_resp_msg_v01,
					   mem_region_info_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEMORY_REGIONS_V01,
		.elem_size      = sizeof(struct wlfw_memory_region_info_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wlfw_msa_info_resp_msg_v01,
					   mem_region_info),
		.ei_array      = wlfw_memory_region_info_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_msa_info_resp_msg_v01_ei);

struct qmi_elem_info wlfw_msa_ready_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_msa_ready_req_msg_v01_ei);

struct qmi_elem_info wlfw_msa_ready_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_msa_ready_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_msa_ready_resp_msg_v01_ei);

struct qmi_elem_info wlfw_ini_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ini_req_msg_v01,
					   enablefwlog_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ini_req_msg_v01,
					   enablefwlog),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_ini_req_msg_v01_ei);

struct qmi_elem_info wlfw_ini_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_ini_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_ini_resp_msg_v01_ei);

struct qmi_elem_info wlfw_athdiag_read_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_athdiag_read_req_msg_v01,
					   offset),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_athdiag_read_req_msg_v01,
					   mem_type),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wlfw_athdiag_read_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_athdiag_read_req_msg_v01_ei);

struct qmi_elem_info wlfw_athdiag_read_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_athdiag_read_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_athdiag_read_resp_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_athdiag_read_resp_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_ATHDIAG_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_athdiag_read_resp_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_athdiag_read_resp_msg_v01_ei);

struct qmi_elem_info wlfw_athdiag_write_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_athdiag_write_req_msg_v01,
					   offset),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_athdiag_write_req_msg_v01,
					   mem_type),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wlfw_athdiag_write_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_ATHDIAG_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wlfw_athdiag_write_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_athdiag_write_req_msg_v01_ei);

struct qmi_elem_info wlfw_athdiag_write_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_athdiag_write_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_athdiag_write_resp_msg_v01_ei);

struct qmi_elem_info wlfw_vbatt_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_vbatt_req_msg_v01,
					   voltage_uv),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_vbatt_req_msg_v01_ei);

struct qmi_elem_info wlfw_vbatt_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_vbatt_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_vbatt_resp_msg_v01_ei);

struct qmi_elem_info wlfw_mac_addr_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_mac_addr_req_msg_v01,
					   mac_addr_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAC_ADDR_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_mac_addr_req_msg_v01,
					   mac_addr),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_mac_addr_req_msg_v01_ei);

struct qmi_elem_info wlfw_mac_addr_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_mac_addr_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_mac_addr_resp_msg_v01_ei);

struct qmi_elem_info wlfw_host_cap_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   num_clients_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   num_clients),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   wake_msi_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   wake_msi),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   gpios_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   gpios_len),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = QMI_WLFW_MAX_NUM_GPIO_V01,
		.elem_size      = sizeof(u32),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   gpios),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   nm_modem_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   nm_modem),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   bdf_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   bdf_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   bdf_cache_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   bdf_cache_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   m3_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x16,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   m3_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   m3_cache_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x17,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   m3_cache_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_filesys_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x18,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_filesys_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_cache_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x19,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_cache_support),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_done_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1A,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_done),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mem_bucket_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1B,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mem_bucket),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mem_cfg_mode_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1C,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mem_cfg_mode),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_duration_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1D,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   cal_duration),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1E,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   platform_name_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_PLATFORM_NAME_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1E,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   platform_name),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x1F,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   ddr_range_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_HOST_DDR_RANGE_SIZE_V01,
		.elem_size      = sizeof(struct wlfw_host_ddr_range_s_v01),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x1F,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   ddr_range),
		.ei_array      = wlfw_host_ddr_range_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   host_build_type_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_host_build_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x20,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   host_build_type),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x21,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_capable_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x21,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_capable),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x22,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_chip_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x22,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_chip_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x23,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_group_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x23,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_group_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x24,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   max_mlo_peer_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x24,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   max_mlo_peer),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x25,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_num_chips_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x25,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_num_chips),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x26,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_chip_info_valid),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MLO_CHIPS_V01,
		.elem_size      = sizeof(struct wlfw_host_mlo_chip_info_s_v01),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x26,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   mlo_chip_info),
		.ei_array      = wlfw_host_mlo_chip_info_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x27,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   feature_list_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x27,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   feature_list),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x28,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   num_wlan_clients_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x28,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   num_wlan_clients),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x29,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   num_wlan_vaps_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x29,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   num_wlan_vaps),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2A,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   wake_msi_addr_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2A,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   wake_msi_addr),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2B,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   wlan_enable_delay_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2B,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   wlan_enable_delay),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2C,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   ddr_type_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2C,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   ddr_type),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2D,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   gpio_info_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2D,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   gpio_info_len),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = QMI_WLFW_MAX_NUM_GPIO_INFO_V01,
		.elem_size      = sizeof(u32),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x2D,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   gpio_info),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2E,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   fw_ini_cfg_support_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x2E,
		.offset         = offsetof(struct
					   wlfw_host_cap_req_msg_v01,
					   fw_ini_cfg_support),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_host_cap_req_msg_v01_ei);

struct qmi_elem_info wlfw_host_cap_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_host_cap_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_host_cap_resp_msg_v01_ei);

struct qmi_elem_info wlfw_request_mem_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_request_mem_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEM_SEG_V01,
		.elem_size      = sizeof(struct wlfw_mem_seg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_request_mem_ind_msg_v01,
					   mem_seg),
		.ei_array      = wlfw_mem_seg_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_request_mem_ind_msg_v01_ei);

struct qmi_elem_info wlfw_respond_mem_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_respond_mem_req_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEM_SEG_V01,
		.elem_size      = sizeof(struct wlfw_mem_seg_resp_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_respond_mem_req_msg_v01,
					   mem_seg),
		.ei_array      = wlfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_respond_mem_req_msg_v01_ei);

struct qmi_elem_info wlfw_respond_mem_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_respond_mem_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_respond_mem_resp_msg_v01_ei);

struct qmi_elem_info wlfw_fw_mem_ready_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_fw_mem_ready_ind_msg_v01_ei);

struct qmi_elem_info wlfw_fw_init_done_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_fw_init_done_ind_msg_v01,
					   hang_data_addr_offset_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_fw_init_done_ind_msg_v01,
					   hang_data_addr_offset),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_fw_init_done_ind_msg_v01,
					   hang_data_length_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_fw_init_done_ind_msg_v01,
					   hang_data_length),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_fw_init_done_ind_msg_v01_ei);

struct qmi_elem_info wlfw_rejuvenate_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   cause_for_rejuvenation_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   cause_for_rejuvenation),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   requesting_sub_system_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   requesting_sub_system),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   line_number_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   line_number),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   function_name_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_FUNCTION_NAME_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ind_msg_v01,
					   function_name),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_rejuvenate_ind_msg_v01_ei);

struct qmi_elem_info wlfw_rejuvenate_ack_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_rejuvenate_ack_req_msg_v01_ei);

struct qmi_elem_info wlfw_rejuvenate_ack_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_rejuvenate_ack_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_rejuvenate_ack_resp_msg_v01_ei);

struct qmi_elem_info wlfw_dynamic_feature_mask_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_dynamic_feature_mask_req_msg_v01,
					   mask_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_dynamic_feature_mask_req_msg_v01,
					   mask),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_dynamic_feature_mask_req_msg_v01_ei);

struct qmi_elem_info wlfw_dynamic_feature_mask_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_dynamic_feature_mask_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_dynamic_feature_mask_resp_msg_v01,
					   prev_mask_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_dynamic_feature_mask_resp_msg_v01,
					   prev_mask),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_dynamic_feature_mask_resp_msg_v01,
					   curr_mask_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_dynamic_feature_mask_resp_msg_v01,
					   curr_mask),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_dynamic_feature_mask_resp_msg_v01_ei);

struct qmi_elem_info wlfw_m3_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_m3_info_req_msg_v01,
					   addr),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_m3_info_req_msg_v01,
					   size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_m3_info_req_msg_v01_ei);

struct qmi_elem_info wlfw_m3_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_m3_info_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_m3_info_resp_msg_v01_ei);

struct qmi_elem_info wlfw_xo_cal_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_xo_cal_ind_msg_v01,
					   xo_cal_data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_xo_cal_ind_msg_v01_ei);

struct qmi_elem_info wlfw_cal_done_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_done_ind_msg_v01,
					   cal_file_upload_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_cal_done_ind_msg_v01,
					   cal_file_upload_size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_cal_done_ind_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_req_mem_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_req_mem_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEM_SEG_V01,
		.elem_size      = sizeof(struct wlfw_mem_seg_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_req_mem_ind_msg_v01,
					   mem_seg),
		.ei_array      = wlfw_mem_seg_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_req_mem_ind_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_mem_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mem_info_req_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEM_SEG_V01,
		.elem_size      = sizeof(struct wlfw_mem_seg_resp_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mem_info_req_msg_v01,
					   mem_seg),
		.ei_array      = wlfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_mem_info_req_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_mem_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mem_info_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_mem_info_resp_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_save_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_save_ind_msg_v01,
					   source),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_save_ind_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_save_ind_msg_v01,
					   mem_seg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_save_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEM_SEG_V01,
		.elem_size      = sizeof(struct wlfw_mem_seg_resp_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_save_ind_msg_v01,
					   mem_seg),
		.ei_array      = wlfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_save_ind_msg_v01,
					   file_name_valid),
	},
	{
		.data_type      = QMI_STRING,
		.elem_len       = QMI_WLFW_MAX_STR_LEN_V01 + 1,
		.elem_size      = sizeof(char),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_save_ind_msg_v01,
					   file_name),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_save_ind_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_data_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_data_req_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_data_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_data_resp_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_data_resp_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_config_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_config_download_req_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_config_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_config_download_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_config_download_resp_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_mode_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mode_req_msg_v01,
					   mode_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_qdss_trace_mode_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mode_req_msg_v01,
					   mode),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mode_req_msg_v01,
					   option_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mode_req_msg_v01,
					   option),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mode_req_msg_v01,
					   hw_trc_disable_override_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_qmi_param_value_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mode_req_msg_v01,
					   hw_trc_disable_override),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_mode_req_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_mode_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_mode_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_mode_resp_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_trace_free_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_free_ind_msg_v01,
					   mem_seg_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_free_ind_msg_v01,
					   mem_seg_len),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_NUM_MEM_SEG_V01,
		.elem_size      = sizeof(struct wlfw_mem_seg_resp_s_v01),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_qdss_trace_free_ind_msg_v01,
					   mem_seg),
		.ei_array      = wlfw_mem_seg_resp_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_trace_free_ind_msg_v01_ei);

struct qmi_elem_info wlfw_shutdown_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_shutdown_req_msg_v01,
					   shutdown_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_shutdown_req_msg_v01,
					   shutdown),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_shutdown_req_msg_v01_ei);

struct qmi_elem_info wlfw_shutdown_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_shutdown_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_shutdown_resp_msg_v01_ei);

struct qmi_elem_info wlfw_antenna_switch_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_antenna_switch_req_msg_v01_ei);

struct qmi_elem_info wlfw_antenna_switch_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_antenna_switch_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_antenna_switch_resp_msg_v01,
					   antenna_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_antenna_switch_resp_msg_v01,
					   antenna),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_antenna_switch_resp_msg_v01_ei);

struct qmi_elem_info wlfw_antenna_grant_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_antenna_grant_req_msg_v01,
					   grant_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_antenna_grant_req_msg_v01,
					   grant),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_antenna_grant_req_msg_v01_ei);

struct qmi_elem_info wlfw_antenna_grant_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_antenna_grant_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_antenna_grant_resp_msg_v01_ei);

struct qmi_elem_info wlfw_wfc_call_status_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   wfc_call_status_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_WFC_CALL_STATUS_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   wfc_call_status),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   wfc_call_active_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   wfc_call_active),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   all_wfc_calls_held_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   all_wfc_calls_held),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   is_wfc_emergency_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   is_wfc_emergency),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   twt_ims_start_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   twt_ims_start),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   twt_ims_int_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   twt_ims_int),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   media_quality_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_wfc_media_quality_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_req_msg_v01,
					   media_quality),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_wfc_call_status_req_msg_v01_ei);

struct qmi_elem_info wlfw_wfc_call_status_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_wfc_call_status_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_wfc_call_status_resp_msg_v01_ei);

struct qmi_elem_info wlfw_get_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_get_info_req_msg_v01,
					   type),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_get_info_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_get_info_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_get_info_req_msg_v01_ei);

struct qmi_elem_info wlfw_get_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_get_info_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_get_info_resp_msg_v01_ei);

struct qmi_elem_info wlfw_respond_get_info_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   type_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   type),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   is_last_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   is_last),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   seq_no_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_respond_get_info_ind_msg_v01,
					   seq_no),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_respond_get_info_ind_msg_v01_ei);

struct qmi_elem_info wlfw_device_info_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_device_info_req_msg_v01_ei);

struct qmi_elem_info wlfw_device_info_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   bar_addr_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   bar_addr),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   bar_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   bar_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   mhi_state_info_addr_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   mhi_state_info_addr),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   mhi_state_info_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_device_info_resp_msg_v01,
					   mhi_state_info_size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_device_info_resp_msg_v01_ei);

struct qmi_elem_info wlfw_m3_dump_upload_req_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_req_ind_msg_v01,
					   pdev_id),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_req_ind_msg_v01,
					   addr),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_req_ind_msg_v01,
					   size),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_m3_dump_upload_req_ind_msg_v01_ei);

struct qmi_elem_info wlfw_m3_dump_upload_done_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_done_req_msg_v01,
					   pdev_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_done_req_msg_v01,
					   status),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_m3_dump_upload_done_req_msg_v01_ei);

struct qmi_elem_info wlfw_m3_dump_upload_done_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_done_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_m3_dump_upload_done_resp_msg_v01_ei);

struct qmi_elem_info wlfw_soc_wake_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_soc_wake_req_msg_v01,
					   wake_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_soc_wake_enum_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_soc_wake_req_msg_v01,
					   wake),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_soc_wake_req_msg_v01_ei);

struct qmi_elem_info wlfw_soc_wake_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_soc_wake_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_soc_wake_resp_msg_v01_ei);

struct qmi_elem_info wlfw_power_save_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_power_save_req_msg_v01,
					   power_save_mode_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_power_save_mode_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_power_save_req_msg_v01,
					   power_save_mode),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_power_save_req_msg_v01_ei);

struct qmi_elem_info wlfw_power_save_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_power_save_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_power_save_resp_msg_v01_ei);

struct qmi_elem_info wlfw_wfc_call_twt_config_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_start_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_start),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_int_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_int),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_upo_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_upo),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_sp_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_sp),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_dl_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_2_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_dl),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_config_changed_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x15,
		.offset         = offsetof(struct
					   wlfw_wfc_call_twt_config_ind_msg_v01,
					   twt_sta_config_changed),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_wfc_call_twt_config_ind_msg_v01_ei);

struct qmi_elem_info wlfw_qdss_mem_ready_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_qdss_mem_ready_ind_msg_v01_ei);

struct qmi_elem_info wlfw_pcie_gen_switch_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_pcie_gen_speed_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_pcie_gen_switch_req_msg_v01,
					   pcie_speed),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_pcie_gen_switch_req_msg_v01_ei);

struct qmi_elem_info wlfw_pcie_gen_switch_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_pcie_gen_switch_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_pcie_gen_switch_resp_msg_v01_ei);

struct qmi_elem_info wlfw_m3_dump_upload_segments_req_ind_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_segments_req_ind_msg_v01,
					   pdev_id),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_segments_req_ind_msg_v01,
					   no_of_valid_segments),
	},
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = QMI_WLFW_MAX_M3_SEGMENTS_SIZE_V01,
		.elem_size      = sizeof(struct wlfw_m3_segment_info_s_v01),
		.array_type       = STATIC_ARRAY,
		.tlv_type       = 0x03,
		.offset         = offsetof(struct
					   wlfw_m3_dump_upload_segments_req_ind_msg_v01,
					   m3_segment),
		.ei_array      = wlfw_m3_segment_info_s_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_m3_dump_upload_segments_req_ind_msg_v01_ei);

struct qmi_elem_info wlfw_subsys_restart_level_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_subsys_restart_level_req_msg_v01,
					   restart_level_type_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_subsys_restart_level_req_msg_v01,
					   restart_level_type),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_subsys_restart_level_req_msg_v01_ei);

struct qmi_elem_info wlfw_subsys_restart_level_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_subsys_restart_level_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_subsys_restart_level_resp_msg_v01_ei);

struct qmi_elem_info wlfw_ini_file_download_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   file_type_valid),
	},
	{
		.data_type      = QMI_SIGNED_4_BYTE_ENUM,
		.elem_len       = 1,
		.elem_size      = sizeof(enum wlfw_ini_file_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   file_type),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   total_size_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x11,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   total_size),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   seg_id_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u32),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x12,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   seg_id),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   data_valid),
	},
	{
		.data_type      = QMI_DATA_LEN,
		.elem_len       = 1,
		.elem_size      = sizeof(u16),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   data_len),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = QMI_WLFW_MAX_DATA_SIZE_V01,
		.elem_size      = sizeof(u8),
		.array_type       = VAR_LEN_ARRAY,
		.tlv_type       = 0x13,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   data),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   end_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x14,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_req_msg_v01,
					   end),
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_ini_file_download_req_msg_v01_ei);

struct qmi_elem_info wlfw_ini_file_download_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.array_type       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct
					   wlfw_ini_file_download_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.array_type       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
EXPORT_SYMBOL(wlfw_ini_file_download_resp_msg_v01_ei);

/**
 * wlfw_is_valid_dt_node_found - Check if valid device tree node present
 *
 * Valid device tree node means a node with "qcom,wlan" property present and
 * "status" property not disabled.
 *
 * Return: true if valid device tree node found, false if not found
 */
static bool wlfw_is_valid_dt_node_found(void)
{
	struct device_node *dn = NULL;

	for_each_node_with_property(dn, "qcom,wlan") {
		if (of_device_is_available(dn))
			break;
	}

	if (dn)
		return true;

	return false;
}

static int __init wlfw_init(void)
{
	if (!wlfw_is_valid_dt_node_found())
		return -ENODEV;

	return 0;
}

module_init(wlfw_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("WLAN FW QMI service");
