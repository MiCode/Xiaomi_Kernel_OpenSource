/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_HFI_SESSION_DEFS_H
#define _CAM_HFI_SESSION_DEFS_H

#include <linux/types.h>

#define HFI_IPEBPS_CMD_OPCODE_BPS_CONFIG_IO             0x1
#define HFI_IPEBPS_CMD_OPCODE_BPS_FRAME_PROCESS         0x2
#define HFI_IPEBPS_CMD_OPCODE_BPS_ABORT                 0x3
#define HFI_IPEBPS_CMD_OPCODE_BPS_DESTROY               0x4

#define HFI_IPEBPS_CMD_OPCODE_IPE_CONFIG_IO             0x5
#define HFI_IPEBPS_CMD_OPCODE_IPE_FRAME_PROCESS         0x6
#define HFI_IPEBPS_CMD_OPCODE_IPE_ABORT                 0x7
#define HFI_IPEBPS_CMD_OPCODE_IPE_DESTROY               0x8

#define HFI_IPEBPS_CMD_OPCODE_BPS_WAIT_FOR_IPE          0x9
#define HFI_IPEBPS_CMD_OPCODE_BPS_WAIT_FOR_BPS          0xa
#define HFI_IPEBPS_CMD_OPCODE_IPE_WAIT_FOR_BPS          0xb
#define HFI_IPEBPS_CMD_OPCODE_IPE_WAIT_FOR_IPE          0xc

#define HFI_IPEBPS_HANDLE_TYPE_BPS                      0x1
#define HFI_IPEBPS_HANDLE_TYPE_IPE_RT                   0x2
#define HFI_IPEBPS_HANDLE_TYPE_IPE_NON_RT               0x3

/**
 * struct abort_data
 * @num_req_ids: Number of req ids
 * @num_req_id: point to specific req id
 *
 * create abort data
 */
struct abort_data {
	uint32_t num_req_ids;
	uint32_t num_req_id[1];
};

/**
 * struct hfi_cmd_data
 * @abort: abort data
 * @user data: user supplied argument
 *
 * create session abort data
 */
struct hfi_cmd_abort {
	struct abort_data abort;
	uint64_t user_data;
} __packed;

/**
 * struct hfi_cmd_abort_destroy
 * @user_data: user supplied data
 *
 * IPE/BPS destroy/abort command
 * @HFI_IPEBPS_CMD_OPCODE_IPE_ABORT
 * @HFI_IPEBPS_CMD_OPCODE_BPS_ABORT
 * @HFI_IPEBPS_CMD_OPCODE_IPE_DESTROY
 * @HFI_IPEBPS_CMD_OPCODE_BPS_DESTROY
 */
struct hfi_cmd_abort_destroy {
	uint64_t user_data;
} __packed;

/**
 * struct hfi_cmd_chaining_ops
 * @wait_hdl: current session handle waits on wait_hdl to complete operation
 * @user_data: user supplied argument
 *
 * this structure for chaining opcodes
 * BPS_WAITS_FOR_IPE
 * BPS_WAITS_FOR_BPS
 * IPE_WAITS_FOR_BPS
 * IPE_WAITS_FOR_IPE
 */
struct hfi_cmd_chaining_ops {
	uint32_t wait_hdl;
	uint64_t user_data;
} __packed;

/**
 * struct hfi_cmd_create_handle
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @handle_type: IPE/BPS firmware session handle type
 * @user_data1: caller provided data1
 * @user_data2: caller provided data2
 *
 * create firmware session handle
 */
struct hfi_cmd_create_handle {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t handle_type;
	uint64_t user_data1;
	uint64_t user_data2;
} __packed;

/**
 * struct hfi_cmd_ipebps_async
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @opcode: opcode for IPE/BPS async operation
 *          CONFIG_IO: configures I/O for IPE/BPS handle
 *          FRAME_PROCESS: image frame to be processed by IPE/BPS
 *          ABORT: abort all processing frames of IPE/BPS handle
 *          DESTROY: destroy earlier created IPE/BPS handle
 *          BPS_WAITS_FOR_IPE: sync for BPS to wait for IPE
 *          BPS_WAITS_FOR_BPS: sync for BPS to wait for BPS
 *          IPE_WAITS_FOR_IPE: sync for IPE to wait for IPE
 *          IPE_WAITS_FOR_BPS: sync for IPE to wait for BPS
 * @num_fw_handles: number of IPE/BPS firmware handles in fw_handles array
 * @fw_handles: IPE/BPS handles array
 * @payload: command payload for IPE/BPS opcodes
 * @direct: points to actual payload
 * @indirect: points to address of payload
 *
 * sends async command to the earlier created IPE or BPS handle
 * for asynchronous operation.
 */
struct hfi_cmd_ipebps_async {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t opcode;
	uint64_t user_data1;
	uint64_t user_data2;
	uint32_t num_fw_handles;
	uint32_t fw_handles[1];
	union {
		uint32_t direct[1];
		uint32_t indirect;
	} payload;
} __packed;

/**
 * struct hfi_msg_create_handle_ack
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @err_type: error code
 * @fw_handle: output param for IPE/BPS handle
 * @user_data1: user provided data1
 * @user_data2: user provided data2
 *
 * ack for create handle command of IPE/BPS
 * @HFI_MSG_IPEBPS_CREATE_HANDLE_ACK
 */
struct hfi_msg_create_handle_ack {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t err_type;
	uint32_t fw_handle;
	uint64_t user_data1;
	uint64_t user_data2;
} __packed;

/**
 * struct hfi_msg_ipebps_async
 * @size: packet size in bytes
 * @pkt_type: opcode of a packet
 * @opcode: opcode of IPE/BPS async operation
 * @user_data1: user provided data1
 * @user_data2: user provided data2
 * @err_type: error code
 * @msg_data: IPE/BPS async done message data
 *
 * result of IPE/BPS async command
 * @HFI_MSG_IPEBPS_ASYNC_COMMAND_ACK
 */
struct hfi_msg_ipebps_async_ack {
	uint32_t size;
	uint32_t pkt_type;
	uint32_t opcode;
	uint64_t user_data1;
	uint64_t user_data2;
	uint32_t err_type;
	uint32_t msg_data[1];
} __packed;

/**
 * struct hfi_msg_frame_process_done
 * @result: result of frame process command
 * @scratch_buffer_address: address of scratch buffer
 */
struct hfi_msg_frame_process_done {
	uint32_t result;
	uint32_t scratch_buffer_address;
};

/**
 * struct hfi_msg_chaining_op
 * @status: return status
 * @user_data: user data provided as part of chaining ops
 *
 * IPE/BPS wait response
 */
struct hfi_msg_chaining_op {
	uint32_t status;
	uint64_t user_data;
} __packed;

/**
 * struct hfi_msg_abort_destroy
 * @status: return status
 * @user_data: user data provided as part of abort/destroy ops
 *
 * IPE/BPS abort/destroy response
 */
struct hfi_msg_abort_destroy {
	uint32_t status;
	uint64_t user_data;
} __packed;

#define MAX_NUM_OF_IMAGE_PLANES	2
#define MAX_HFR_GROUP          16

enum hfi_ipe_io_images {
	IPE_INPUT_IMAGE_FULL,
	IPE_INPUT_IMAGE_DS4,
	IPE_INPUT_IMAGE_DS16,
	IPE_INPUT_IMAGE_DS64,
	IPE_INPUT_IMAGE_FULL_REF,
	IPE_INPUT_IMAGE_DS4_REF,
	IPE_INPUT_IMAGE_DS16_REF,
	IPE_INPUT_IMAGE_DS64_REF,
	IPE_OUTPUT_IMAGE_DISPLAY,
	IPE_OUTPUT_IMAGE_VIDEO,
	IPE_OUTPUT_IMAGE_FULL_REF,
	IPE_OUTPUT_IMAGE_DS4_REF,
	IPE_OUTPUT_IMAGE_DS16_REF,
	IPE_OUTPUT_IMAGE_DS64_REF,
	IPE_INPUT_IMAGE_FIRST = IPE_INPUT_IMAGE_FULL,
	IPE_INPUT_IMAGE_LAST = IPE_INPUT_IMAGE_DS64_REF,
	IPE_OUTPUT_IMAGE_FIRST = IPE_OUTPUT_IMAGE_DISPLAY,
	IPE_OUTPUT_IMAGE_LAST = IPE_OUTPUT_IMAGE_DS64_REF,
	IPE_IO_IMAGES_MAX
};

enum bps_io_images {
	BPS_INPUT_IMAGE,
	BPS_OUTPUT_IMAGE_FULL,
	BPS_OUTPUT_IMAGE_DS4,
	BPS_OUTPUT_IMAGE_DS16,
	BPS_OUTPUT_IMAGE_DS64,
	BPS_OUTPUT_IMAGE_STATS_BG,
	BPS_OUTPUT_IMAGE_STATS_BHIST,
	BPS_OUTPUT_IMAGE_REG1,
	BPS_OUTPUT_IMAGE_REG2,
	BPS_OUTPUT_IMAGE_FIRST = BPS_OUTPUT_IMAGE_FULL,
	BPS_OUTPUT_IMAGE_LAST = BPS_OUTPUT_IMAGE_REG2,
	BPS_IO_IMAGES_MAX
};

struct frame_buffer {
	uint32_t buffer_ptr[MAX_NUM_OF_IMAGE_PLANES];
	uint32_t meta_buffer_ptr[MAX_NUM_OF_IMAGE_PLANES];
} __packed;

struct bps_frame_process_data {
	struct frame_buffer buffers[BPS_IO_IMAGES_MAX];
	uint32_t max_num_cores;
	uint32_t target_time;
	uint32_t ubwc_stats_buffer_addr;
	uint32_t ubwc_stats_buffer_size;
	uint32_t cdm_buffer_addr;
	uint32_t cdm_buffer_size;
	uint32_t iq_settings_addr;
	uint32_t strip_lib_out_addr;
	uint32_t cdm_prog_addr;
	uint32_t request_id;
};

enum hfi_ipe_image_format {
	IMAGE_FORMAT_INVALID,
	IMAGE_FORMAT_MIPI_8,
	IMAGE_FORMAT_MIPI_10,
	IMAGE_FORMAT_MIPI_12,
	IMAGE_FORMAT_MIPI_14,
	IMAGE_FORMAT_BAYER_8,
	IMAGE_FORMAT_BAYER_10,
	IMAGE_FORMAT_BAYER_12,
	IMAGE_FORMAT_BAYER_14,
	IMAGE_FORMAT_PDI_10,
	IMAGE_FORMAT_PD_10,
	IMAGE_FORMAT_PD_8,
	IMAGE_FORMAT_INDICATIONS,
	IMAGE_FORMAT_REFINEMENT,
	IMAGE_FORMAT_UBWC_TP_10,
	IMAGE_FORMAT_UBWC_NV_12,
	IMAGE_FORMAT_UBWC_NV12_4R,
	IMAGE_FORMAT_UBWC_P010,
	IMAGE_FORMAT_LINEAR_TP_10,
	IMAGE_FORMAT_LINEAR_P010,
	IMAGE_FORMAT_LINEAR_NV12,
	IMAGE_FORMAT_LINEAR_PLAIN_16,
	IMAGE_FORMAT_YUV422_8,
	IMAGE_FORMAT_YUV422_10,
	IMAGE_FORMAT_STATISTICS_BAYER_GRID,
	IMAGE_FORMAT_STATISTICS_BAYER_HISTOGRAM,
	IMAGE_FORMAT_MAX
};

enum hfi_ipe_plane_format {
	PLANE_FORMAT_INVALID = 0,
	PLANE_FORMAT_MIPI_8,
	PLANE_FORMAT_MIPI_10,
	PLANE_FORMAT_MIPI_12,
	PLANE_FORMAT_MIPI_14,
	PLANE_FORMAT_BAYER_8,
	PLANE_FORMAT_BAYER_10,
	PLANE_FORMAT_BAYER_12,
	PLANE_FORMAT_BAYER_14,
	PLANE_FORMAT_PDI_10,
	PLANE_FORMAT_PD_10,
	PLANE_FORMAT_PD_8,
	PLANE_FORMAT_INDICATIONS,
	PLANE_FORMAT_REFINEMENT,
	PLANE_FORMAT_UBWC_TP_10_Y,
	PLANE_FORMAT_UBWC_TP_10_C,
	PLANE_FORMAT_UBWC_NV_12_Y,
	PLANE_FORMAT_UBWC_NV_12_C,
	PLANE_FORMAT_UBWC_NV_12_4R_Y,
	PLANE_FORMAT_UBWC_NV_12_4R_C,
	PLANE_FORMAT_UBWC_P010_Y,
	PLANE_FORMAT_UBWC_P010_C,
	PLANE_FORMAT_LINEAR_TP_10_Y,
	PLANE_FORMAT_LINEAR_TP_10_C,
	PLANE_FORMAT_LINEAR_P010_Y,
	PLANE_FORMAT_LINEAR_P010_C,
	PLANE_FORMAT_LINEAR_NV12_Y,
	PLANE_FORMAT_LINEAR_NV12_C,
	PLANE_FORMAT_LINEAR_PLAIN_16_Y,
	PLANE_FORMAT_LINEAR_PLAIN_16_C,
	PLANE_FORMAT_YUV422_8,
	PLANE_FORMAT_YUV422_10,
	PLANE_FORMAT_STATISTICS_BAYER_GRID,
	PLANE_FORMAT_STATISTICS_BAYER_HISTOGRAM,
	PLANE_FORMAT_MAX
};

enum hfi_ipe_bayer_pixel_order {
	FIRST_PIXEL_R,
	FIRST_PIXEL_GR,
	FIRST_PIXEL_B,
	FIRST_PIXEL_GB,
	FIRST_PIXEL_MAX
};

enum hfi_ipe_pixel_pack_alignment {
	PIXEL_LSB_ALIGNED,
	PIXEL_MSB_ALIGNED,
};

enum hfi_ipe_yuv_422_order {
	PIXEL_ORDER_Y_U_Y_V,
	PIXEL_ORDER_Y_V_Y_U,
	PIXEL_ORDER_U_Y_V_Y,
	PIXEL_ORDER_V_Y_U_Y,
	PIXEL_ORDER_YUV422_MAX
};

enum ubwc_write_client {
	IPE_WR_CLIENT_0 = 0,
	IPE_WR_CLIENT_1,
	IPE_WR_CLIENT_5,
	IPE_WR_CLIENT_6,
	IPE_WR_CLIENT_7,
	IPE_WR_CLIENT_8,
	IPE_WR_CLIENT_MAX
};

/**
 * struct image_info
 * @format: image format
 * @img_width: image width
 * @img_height: image height
 * @bayer_order: pixel order
 * @pix_align: alignment
 * @yuv422_order: YUV order
 * @byte_swap: byte swap
 */
struct image_info {
	enum hfi_ipe_image_format format;
	uint32_t img_width;
	uint32_t img_height;
	enum hfi_ipe_bayer_pixel_order bayer_order;
	enum hfi_ipe_pixel_pack_alignment pix_align;
	enum hfi_ipe_yuv_422_order yuv422_order;
	uint32_t byte_swap;
} __packed;

/**
 * struct buffer_layout
 * @buf_stride: buffer stride
 * @buf_height: buffer height
 */
struct buffer_layout {
	uint32_t buf_stride;
	uint32_t buf_height;
} __packed;

/**
 * struct image_desc
 * @info: image info
 * @buf_layout: buffer layout
 * @meta_buf_layout: meta buffer layout
 */
struct image_desc {
	struct image_info info;
	struct buffer_layout buf_layout[MAX_NUM_OF_IMAGE_PLANES];
	struct buffer_layout meta_buf_layout[MAX_NUM_OF_IMAGE_PLANES];
} __packed;

struct ica_stab_coeff {
	uint32_t coeffs[8];
} __packed;

struct ica_stab_params {
	uint32_t mode;
	struct ica_stab_coeff transforms[3];
} __packed;

struct frame_set {
	struct frame_buffer buffers[IPE_IO_IMAGES_MAX];
	struct ica_stab_params ica_params;
	uint32_t cdm_ica1_addr;
	uint32_t cdm_ica2_addr;
} __packed;

struct ipe_frame_process_data {
	uint32_t strip_lib_out_addr;
	uint32_t iq_settings_addr;
	uint32_t scratch_buffer_addr;
	uint32_t scratch_buffer_size;
	uint32_t ubwc_stats_buffer_addr;
	uint32_t ubwc_stats_buffer_size;
	uint32_t cdm_buffer_addr;
	uint32_t cdm_buffer_size;
	uint32_t max_num_cores;
	uint32_t target_time;
	uint32_t cdm_prog_base;
	uint32_t cdm_pre_ltm;
	uint32_t cdm_post_ltm;
	uint32_t cdm_anr_full_pass;
	uint32_t cdm_anr_ds4;
	uint32_t cdm_anr_ds16;
	uint32_t cdm_anr_ds64;
	uint32_t cdm_tf_full_pass;
	uint32_t cdm_tf_ds4;
	uint32_t cdm_tf_ds16;
	uint32_t cdm_tf_ds64;
	uint32_t request_id;
	uint32_t frames_in_batch;
	struct frame_set framesets[MAX_HFR_GROUP];
} __packed;

/**
 * struct hfi_cmd_ipe_config
 * @images: images descreptions
 * @user_data: user supplied data
 *
 * payload for IPE async command
 */
struct hfi_cmd_ipe_config {
	struct image_desc images[IPE_IO_IMAGES_MAX];
	uint64_t user_data;
} __packed;

/**
 * struct frame_buffers
 * @buf_ptr: buffer pointers for all planes
 * @meta_buf_ptr: meta buffer pointers for all planes
 */
struct frame_buffers {
	uint32_t buf_ptr[MAX_NUM_OF_IMAGE_PLANES];
	uint32_t meta_buf_ptr[MAX_NUM_OF_IMAGE_PLANES];
} __packed;

/**
 * struct hfi_msg_ipe_config
 * @rc: result of ipe config command
 * @scratch_mem_size: scratch mem size for a config
 * @user_data: user data
 */
struct hfi_msg_ipe_config {
	uint32_t rc;
	uint32_t scratch_mem_size;
	uint64_t user_data;
} __packed;

/**
 * struct hfi_msg_bps_common
 * @rc: result of ipe config command
 * @user_data: user data
 */
struct hfi_msg_bps_common {
	uint32_t rc;
	uint64_t user_data;
} __packed;

/**
 * struct ipe_bps_destroy
 * @user_data: user data
 */
struct ipe_bps_destroy {
	uint64_t userdata;
};

/**
 * struct hfi_msg_ipe_frame_process
 * @status: result of ipe frame process command
 * @scratch_buf_addr: address of scratch buffer
 * @user_data: user data
 */
struct hfi_msg_ipe_frame_process {
	uint32_t status;
	uint32_t scratch_buf_addr;
	uint64_t user_data;
} __packed;

#endif /* _CAM_HFI_SESSION_DEFS_H */
