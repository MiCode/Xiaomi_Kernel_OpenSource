/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
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

#ifndef _VDEC_IPI_MSG_H_
#define _VDEC_IPI_MSG_H_

/**
 * enum vdec_ipi_msgid - message id between AP and VCU
 * @AP_IPIMSG_XXX	: AP to VCU cmd message id
 * @VCU_IPIMSG_XXX_ACK	: VCU ack AP cmd message id
 */
enum vdec_ipi_msgid {
	AP_IPIMSG_DEC_INIT = 0xA000,
	AP_IPIMSG_DEC_START = 0xA001,
	AP_IPIMSG_DEC_END = 0xA002,
	AP_IPIMSG_DEC_DEINIT = 0xA003,
	AP_IPIMSG_DEC_RESET = 0xA004,
	AP_IPIMSG_DEC_SET_PARAM = 0xA005,

	VCU_IPIMSG_DEC_INIT_ACK = 0xB000,
	VCU_IPIMSG_DEC_START_ACK = 0xB001,
	VCU_IPIMSG_DEC_END_ACK = 0xB002,
	VCU_IPIMSG_DEC_DEINIT_ACK = 0xB003,
	VCU_IPIMSG_DEC_RESET_ACK = 0xB004,
	VCU_IPIMSG_DEC_SET_PARAM_ACK = 0xB005,

	VCU_IPIMSG_DEC_WAITISR = 0xC000,
	VCU_IPIMSG_DEC_GET_FRAME_BUFFER = 0xC001,
	VCU_IPIMSG_DEC_CLOCK_ON = 0xC002,
	VCU_IPIMSG_DEC_CLOCK_OFF = 0xC003
};

/**
 * struct vdec_ap_ipi_cmd - generic AP to VCU ipi command format
 * @msg_id	: vdec_ipi_msgid
 * @vcu_inst_addr	: VCU decoder instance address
 */
struct vdec_ap_ipi_cmd {
	uint32_t msg_id;
	uint32_t vcu_inst_addr;
};

/**
 * struct vdec_vpu_ipi_ack - generic VPU to AP ipi command format
 * @msg_id	: vdec_ipi_msgid
 * @status	: VPU exeuction result
 * @ap_inst_addr	: AP video decoder instance address
 */
struct vdec_vcu_ipi_ack {
	uint32_t msg_id;
	int32_t status;
	uint64_t ap_inst_addr;
};

/**
 * struct vdec_ap_ipi_init - for AP_IPIMSG_DEC_INIT
 * @msg_id	: AP_IPIMSG_DEC_INIT
 * @reserved	: Reserved field
 * @ap_inst_addr	: AP video decoder instance address
 */
struct vdec_ap_ipi_init {
	uint32_t msg_id;
	uint32_t reserved;
	uint64_t ap_inst_addr;
};

/**
 * struct vdec_ap_ipi_dec_start - for AP_IPIMSG_DEC_START
 * @msg_id	: AP_IPIMSG_DEC_START
 * @vcu_inst_addr	: VCU decoder instance address
 * @data	: Header info
 * @reserved	: Reserved field
 */
struct vdec_ap_ipi_dec_start {
	uint32_t msg_id;
	uint32_t vcu_inst_addr;
	uint32_t data[3];
	uint32_t reserved;
};

/**
 * struct vdec_ap_ipi_set_param - for AP_IPIMSG_DEC_SET_PARAM
 * @msg_id        : AP_IPIMSG_DEC_SET_PARAM
 * @vcu_inst_addr : VCU decoder instance address
 * @id            : set param  type
 * @data          : param data
 */
struct vdec_ap_ipi_set_param {
	uint32_t msg_id;
	uint32_t vcu_inst_addr;
	uint32_t id;
	uint32_t data[8];
};

/**
 * struct vdec_vcu_ipi_init_ack - for VCU_IPIMSG_DEC_INIT_ACK
 * @msg_id        : VCU_IPIMSG_DEC_INIT_ACK
 * @status        : VCU exeuction result
 * @ap_inst_addr	: AP vcodec_vcu_inst instance address
 * @vcu_inst_addr : VCU decoder instance address
 */
struct vdec_vcu_ipi_init_ack {
	uint32_t msg_id;
	int32_t status;
	uint64_t ap_inst_addr;
	uint32_t vcu_inst_addr;
};

#endif
