// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#include <linux/qmi_encdec.h>
#include <soc/qcom/msm_qmi_interface.h>
#include "wcd_spi_ctl_v01.h"

struct elem_info wcd_spi_req_access_msg_v01_ei[] = {
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wcd_spi_req_access_msg_v01,
					   reason_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_8_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u64),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wcd_spi_req_access_msg_v01,
					   reason),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wcd_spi_req_access_resp_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wcd_spi_req_access_resp_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wcd_spi_rel_access_msg_v01_ei[] = {
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wcd_spi_rel_access_resp_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wcd_spi_rel_access_resp_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wcd_spi_buff_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = WCD_SPI_BUFF_CHANNELS_MAX_V01,
		.elem_size      = sizeof(u32),
		.is_array       = STATIC_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct wcd_spi_buff_msg_v01,
					   buff_addr_1),
	},
	{
		.data_type      = QMI_OPT_FLAG,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wcd_spi_buff_msg_v01,
					   buff_addr_2_valid),
	},
	{
		.data_type      = QMI_UNSIGNED_4_BYTE,
		.elem_len       = WCD_SPI_BUFF_CHANNELS_MAX_V01,
		.elem_size      = sizeof(u32),
		.is_array       = STATIC_ARRAY,
		.tlv_type       = 0x10,
		.offset         = offsetof(struct wcd_spi_buff_msg_v01,
					   buff_addr_2),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct elem_info wcd_spi_buff_resp_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct wcd_spi_buff_resp_v01,
					   resp),
		.ei_array      = get_qmi_response_type_v01_ei(),
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};
