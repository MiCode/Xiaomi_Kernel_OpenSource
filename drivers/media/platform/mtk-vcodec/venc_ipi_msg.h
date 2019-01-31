/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Jungchang Tsao <jungchang.tsao@mediatek.com>
 *	   Daniel Hsiao <daniel.hsiao@mediatek.com>
 *	   Tiffany Lin <tiffany.lin@mediatek.com>
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

#define AP_IPIMSG_VENC_BASE 0xC000
#define VCU_IPIMSG_VENC_BASE 0xD000

/**
 * enum venc_ipi_msg_id - message id between AP and VCU
 * (ipi stands for inter-processor interrupt)
 * @AP_IPIMSG_ENC_XXX:		AP to VCU cmd message id
 * @VCU_IPIMSG_ENC_XXX_DONE:	VCU ack AP cmd message id
 */
enum venc_ipi_msg_id {
	AP_IPIMSG_ENC_INIT = AP_IPIMSG_VENC_BASE,
	AP_IPIMSG_ENC_SET_PARAM,
	AP_IPIMSG_ENC_ENCODE,
	AP_IPIMSG_ENC_DEINIT,

	VCU_IPIMSG_ENC_INIT_DONE = VCU_IPIMSG_VENC_BASE,
	VCU_IPIMSG_ENC_SET_PARAM_DONE,
	VCU_IPIMSG_ENC_ENCODE_DONE,
	VCU_IPIMSG_ENC_DEINIT_DONE,
};

/**
 * struct venc_ap_ipi_msg_init - AP to VCU init cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_INIT)
 * @reserved:	reserved for future use. vcu is running in 32bit. Without
 *		this reserved field, if kernel run in 64bit. this struct size
 *		will be different between kernel and vcu
 * @venc_inst:	AP encoder instance
 *		(struct venc_vp8_inst/venc_h264_inst *)
 */
struct venc_ap_ipi_msg_init {
	uint32_t msg_id;
	uint32_t reserved;
	uint64_t venc_inst;
};

/**
 * struct venc_ap_ipi_msg_set_param - AP to VCU set_param cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_SET_PARAM)
 * @vcu_inst_addr:	VCU encoder instance addr
 *			(struct venc_vp8_vsi/venc_h264_vsi *)
 * @param_id:	parameter id (venc_set_param_type)
 * @data_item:	number of items in the data array
 * @data[8]:	data array to store the set parameters
 */
struct venc_ap_ipi_msg_set_param {
	uint32_t msg_id;
	uint32_t vcu_inst_addr;
	uint32_t param_id;
	uint32_t data_item;
	uint32_t data[8];
};

/**
 * struct venc_ap_ipi_msg_enc - AP to VCU enc cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_ENCODE)
 * @vcu_inst_addr:	VCU encoder instance addr
 *			(struct venc_vp8_vsi/venc_h264_vsi *)
 * @bs_mode:	bitstream mode for h264
 *		(H264_BS_MODE_SPS/H264_BS_MODE_PPS/H264_BS_MODE_FRAME)
 * @input_addr:	pointer to input image buffer plane
 * @bs_addr:	pointer to output bit stream buffer
 * @bs_size:	bit stream buffer size
 */
struct venc_ap_ipi_msg_enc {
	uint32_t msg_id;
	uint32_t vcu_inst_addr;
	uint32_t bs_mode;
	uint32_t input_addr[3];
	uint32_t bs_addr;
	uint32_t bs_size;
};

/**
 * struct venc_ap_ipi_msg_deinit - AP to VCU deinit cmd structure
 * @msg_id:	message id (AP_IPIMSG_XXX_ENC_DEINIT)
 * @vcu_inst_addr:	VCU encoder instance addr
 *			(struct venc_vp8_vsi/venc_h264_vsi *)
 */
struct venc_ap_ipi_msg_deinit {
	uint32_t msg_id;
	uint32_t vcu_inst_addr;
};

/**
 * enum venc_ipi_msg_status - VCU ack AP cmd status
 */
enum venc_ipi_msg_status {
	VENC_IPI_MSG_STATUS_OK,
	VENC_IPI_MSG_STATUS_FAIL,
};

/**
 * struct venc_vcu_ipi_msg_common - VCU ack AP cmd common structure
 * @msg_id:	message id (VCU_IPIMSG_XXX_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 */
struct venc_vcu_ipi_msg_common {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
};

/**
 * struct venc_vcu_ipi_msg_init - VCU ack AP init cmd structure
 * @msg_id:	message id (VCU_IPIMSG_XXX_ENC_SET_PARAM_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 * @vcu_inst_addr:	VCU encoder instance addr
 *			(struct venc_vp8_vsi/venc_h264_vsi *)
 * @reserved:	reserved for future use. vcu is running in 32bit. Without
 *		this reserved field, if kernel run in 64bit. this struct size
 *		will be different between kernel and vcu
 */
struct venc_vcu_ipi_msg_init {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
	uint32_t vcu_inst_addr;
	uint32_t reserved;
};

/**
 * struct venc_vcu_ipi_msg_set_param - VCU ack AP set_param cmd structure
 * @msg_id:	message id (VCU_IPIMSG_XXX_ENC_SET_PARAM_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 * @param_id:	parameter id (venc_set_param_type)
 * @data_item:	number of items in the data array
 * @data[6]:	data array to store the return result
 */
struct venc_vcu_ipi_msg_set_param {
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
 * struct venc_vcu_ipi_msg_enc - VCU ack AP enc cmd structure
 * @msg_id:	message id (VCU_IPIMSG_XXX_ENC_ENCODE_DONE)
 * @status:	cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 * @state:	encode state (venc_ipi_msg_enc_state)
 * @is_key_frm:	whether the encoded frame is key frame
 * @bs_size:	encoded bitstream size
 * @reserved:	reserved for future use. vcu is running in 32bit. Without
 *		this reserved field, if kernel run in 64bit. this struct size
 *		will be different between kernel and vcu
 */
struct venc_vcu_ipi_msg_enc {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
	uint32_t state;
	uint32_t is_key_frm;
	uint32_t bs_size;
	uint32_t reserved;
};

/**
 * struct venc_vcu_ipi_msg_deinit - VCU ack AP deinit cmd structure
 * @msg_id:   message id (VCU_IPIMSG_XXX_ENC_DEINIT_DONE)
 * @status:   cmd status (venc_ipi_msg_status)
 * @venc_inst:	AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 */
struct venc_vcu_ipi_msg_deinit {
	uint32_t msg_id;
	uint32_t status;
	uint64_t venc_inst;
};

#endif /* _VENC_IPI_MSG_H_ */
