/*
 * Copyright (c) 2015 MediaTek Inc.
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
 * enum vdec_ipi_msgid - message id between AP and VPU
 * @AP_IPIMSG_XXX:		AP to VPU cmd message id
 * @VPU_IPIMSG_XXX_DONE:	VPU ack AP cmd message id
 */
enum vdec_ipi_msgid {
	AP_IPIMSG_DEC_INIT			= 0xA000,
	AP_IPIMSG_DEC_START			= 0xA001,
	AP_IPIMSG_DEC_END			= 0xA002,
	AP_IPIMSG_DEC_DEINIT			= 0xA003,
	AP_IPIMSG_DEC_RESET			= 0xA004,

	VPU_IPIMSG_DEC_INIT_ACK			= 0xB000,
	VPU_IPIMSG_DEC_START_ACK		= 0xB001,
	VPU_IPIMSG_DEC_END_ACK			= 0xB002,
	VPU_IPIMSG_DEC_DEINIT_ACK		= 0xB003,
	VPU_IPIMSG_DEC_RESET_ACK		= 0xB004,
};

/**
 * struct vdec_ap_ipi_cmd - generic AP to VPU ipi command format
 * @msg_id      : vdec_ipi_msgid
 * @h_drv       : handle to VPU driver
 */
struct vdec_ap_ipi_cmd {
	uint32_t msg_id;
	uint32_t h_drv;
};

/**
 * struct vdec_vpu_ipi_ack - generic VPU to AP ipi command format
 * @msg_id      : vdec_ipi_msgid
 * @status      : VPU exeuction result
 * @vdec_inst   : AP video decoder instance address
 */
struct vdec_vpu_ipi_ack {
	uint32_t msg_id;
	int32_t status;
	uint64_t vdec_inst;
};

#endif
