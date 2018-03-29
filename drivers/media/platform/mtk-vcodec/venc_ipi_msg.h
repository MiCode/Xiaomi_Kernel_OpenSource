/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Jungchang Tsao <jungchang.tsao@mediatek.com>
 *         Daniel Hsiao <daniel.hsiao@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VENC_IPI_MSG_H_
#define _VENC_IPI_MSG_H_

#define IPIMSG_H264_ENC_ID 0x100
#define IPIMSG_VP8_ENC_ID 0x200

#define AP_IPIMSG_VENC_BASE 0x20000
#define VPU_IPIMSG_VENC_BASE 0x30000

/**
 * enum venc_ipi_msg_id - message id between AP and VPU
 * (ipi stands for inter-processor interrupt)
 * @AP_IPIMSG_XXX:		AP to VPU cmd message id
 * @VPU_IPIMSG_XXX_DONE:	VPU ack AP cmd message id
 */
enum venc_ipi_msg_id {
	AP_IPIMSG_H264_ENC_INIT = AP_IPIMSG_VENC_BASE +
				  IPIMSG_H264_ENC_ID,
	AP_IPIMSG_H264_ENC_SET_PARAM,
	AP_IPIMSG_H264_ENC_ENCODE,
	AP_IPIMSG_H264_ENC_DEINIT,

	AP_IPIMSG_VP8_ENC_INIT = AP_IPIMSG_VENC_BASE +
				 IPIMSG_VP8_ENC_ID,
	AP_IPIMSG_VP8_ENC_SET_PARAM,
	AP_IPIMSG_VP8_ENC_ENCODE,
	AP_IPIMSG_VP8_ENC_DEINIT,

	VPU_IPIMSG_H264_ENC_INIT_DONE = VPU_IPIMSG_VENC_BASE +
					IPIMSG_H264_ENC_ID,
	VPU_IPIMSG_H264_ENC_SET_PARAM_DONE,
	VPU_IPIMSG_H264_ENC_ENCODE_DONE,
	VPU_IPIMSG_H264_ENC_DEINIT_DONE,

	VPU_IPIMSG_VP8_ENC_INIT_DONE = VPU_IPIMSG_VENC_BASE +
				       IPIMSG_VP8_ENC_ID,
	VPU_IPIMSG_VP8_ENC_SET_PARAM_DONE,
	VPU_IPIMSG_VP8_ENC_ENCODE_DONE,
	VPU_IPIMSG_VP8_ENC_DEINIT_DONE,
};

/**
 * struct venc_ap_ipi_msg_init - AP to VPU init cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_INIT)
 * @venc_inst:	AP encoder instance (struct venc_vp8_handle/venc_h264_handle *)
 */
struct venc_ap_ipi_msg_init {
	uint32_t msg_id;
	uint32_t reserved;
	uint64_t venc_inst;
};

/**
 * struct venc_ap_ipi_msg_set_param - AP to VPU set_param cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_SET_PARAM)
 * @inst_id:	VPU encoder instance id (struct venc_vp8_vpu_drv/venc_h264_vpu_drv *)
 * @param_id:	parameter id (venc_set_param_type)
 * @data_item:	number of items in the data array
 * @data[8]:	data array to store the set parameters
 */
struct venc_ap_ipi_msg_set_param {
	uint32_t msg_id;
	uint32_t inst_id;
	uint32_t param_id;
	uint32_t data_item;
	uint32_t data[8];
};

/**
 * struct venc_ap_ipi_msg_enc - AP to VPU enc cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_ENCODE)
 * @inst_id:	VPU encoder instance id (struct venc_vp8_vpu_drv/venc_h264_vpu_drv *)
 * @bs_mode:	bitstream mode for h264
 *		(H264_BS_MODE_SPS/H264_BS_MODE_PPS/H264_BS_MODE_FRAME)
 * @input_addr:	pointer to input image buffer plane
 * @bs_addr:	pointer to output bit stream buffer
 * @bs_size:	bit stream buffer size
 */
struct venc_ap_ipi_msg_enc {
	uint32_t msg_id;
	uint32_t inst_id;
	uint32_t bs_mode;
	uint32_t input_addr[3];
	uint32_t bs_addr;
	uint32_t bs_size;
};

/**
 * struct venc_ap_ipi_msg_deinit - AP to VPU deinit cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_DEINIT)
 * @inst_id:	VPU encoder instance id (struct venc_vp8_vpu_drv/venc_h264_vpu_drv *)
 */
struct venc_ap_ipi_msg_deinit {
	uint32_t msg_id;
	uint32_t inst_id;
};

/**
 * enum venc_ipi_msg_status - VPU ack AP cmd status
 */
enum venc_ipi_msg_status {
	VENC_IPI_MSG_STATUS_OK,
	VENC_IPI_MSG_STATUS_FAIL,
};

/**
 * struct venc_vpu_ipi_msg_common - VPU ack AP cmd common structure
 * @msg_id:	message id (VPU_IPIMSG_XXX_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_handle/venc_h264_handle *)
 */
struct venc_vpu_ipi_msg_common {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
};

/**
 * struct venc_vpu_ipi_msg_init - VPU ack AP init cmd structure
 * @msg_id:	message id (VPU_IPIMSG_XXX_ENC_SET_PARAM_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_handle/venc_h264_handle *)
 * @inst_id:	VPU encoder instance id (struct venc_vp8_vpu_drv/venc_h264_vpu_drv *)
 */
struct venc_vpu_ipi_msg_init {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
	uint32_t inst_id;
	uint32_t reserved;
};

/**
 * struct venc_vpu_ipi_msg_set_param - VPU ack AP set_param cmd structure
 * @msg_id:	message id (VPU_IPIMSG_XXX_ENC_SET_PARAM_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_handle/venc_h264_handle *)
 * @param_id:	parameter id (venc_set_param_type)
 * @data_item:	number of items in the data array
 * @data[6]:	data array to store the return result
 */
struct venc_vpu_ipi_msg_set_param {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
	uint32_t param_id;
	uint32_t data_item;
	uint32_t data[6];
};

/**
 * enum venc_ipi_msg_enc_state - Type of encode state
 * VEN_IPI_MSG_ENC_STATE_FRAME:	one frame being encoded
 * VEN_IPI_MSG_ENC_STATE_PART:	bit stream buffer full
 * VEN_IPI_MSG_ENC_STATE_SKIP:	encoded skip frame
 * VEN_IPI_MSG_ENC_STATE_ERROR:	encounter error
 */
enum venc_ipi_msg_enc_state {
	VEN_IPI_MSG_ENC_STATE_FRAME,
	VEN_IPI_MSG_ENC_STATE_PART,
	VEN_IPI_MSG_ENC_STATE_SKIP,
	VEN_IPI_MSG_ENC_STATE_ERROR,
};

/**
 * struct venc_vpu_ipi_msg_enc - VPU ack AP enc cmd structure
 * @msg_id:	message id (VPU_IPIMSG_XXX_ENC_ENCODE_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_handle/venc_h264_handle *)
 * @state:	encode state (venc_ipi_msg_enc_state)
 * @key_frame:	whether the encoded frame is key frame
 * @bs_size:	encoded bitstream size
 */
struct venc_vpu_ipi_msg_enc {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
	uint32_t state;
	uint32_t key_frame;
	uint32_t bs_size;
	uint32_t reserved;
};

/**
 * struct venc_vpu_ipi_msg_deinit - VPU ack AP deinit cmd structure
 * @msg_id:   message id (VPU_IPIMSG_XXX_ENC_DEINIT_DONE)
 * @status:   cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_handle/venc_h264_handle *)
 */
struct venc_vpu_ipi_msg_deinit {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
};

#endif /* _VENC_IPI_MSG_H_ */
