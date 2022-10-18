/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_JPEG_ENC_165_HW_INFO_TITAN170_H
#define CAM_JPEG_ENC_165_HW_INFO_TITAN170_H

#define CAM_JPEG_HW_IRQ_STATUS_FRAMEDONE_MASK 0x00000001
#define CAM_JPEG_HW_IRQ_STATUS_FRAMEDONE_SHIFT 0x00000000

#define CAM_JPEG_HW_IRQ_STATUS_RESET_ACK_MASK 0x10000000
#define CAM_JPEG_HW_IRQ_STATUS_RESET_ACK_SHIFT 0x0000000a

#define CAM_JPEG_HW_IRQ_STATUS_STOP_DONE_MASK 0x8000000
#define CAM_JPEG_HW_IRQ_STATUS_STOP_DONE_SHIFT 0x0000001b

#define CAM_JPEG_HW_IRQ_STATUS_BUS_ERROR_MASK 0x00000800
#define CAM_JPEG_HW_IRQ_STATUS_BUS_ERROR_SHIFT 0x0000000b

#define CAM_JPEG_HW_MASK_SCALE_ENABLE 0x1

#define CAM_JPEG_HW_IRQ_STATUS_DCD_UNESCAPED_FF      (0x1<<19)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_HUFFMAN_ERROR     (0x1<<20)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_COEFFICIENT_ERR   (0x1<<21)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_BIT_STUFF (0x1<<22)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_SCAN_UNDERFLOW    (0x1<<23)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM       (0x1<<24)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM_SEQ   (0x1<<25)
#define CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_RSM       (0x1<<26)
#define CAM_JPEG_HW_IRQ_STATUS_VIOLATION_MASK        (0x1<<29)

#define CAM_JPEG_HW_MASK_COMP_FRAMEDONE \
		CAM_JPEG_HW_IRQ_STATUS_FRAMEDONE_MASK
#define CAM_JPEG_HW_MASK_COMP_RESET_ACK \
		CAM_JPEG_HW_IRQ_STATUS_RESET_ACK_MASK
#define CAM_JPEG_HW_MASK_COMP_ERR \
		(CAM_JPEG_HW_IRQ_STATUS_DCD_UNESCAPED_FF | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_HUFFMAN_ERROR | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_COEFFICIENT_ERR | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_BIT_STUFF | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_SCAN_UNDERFLOW | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_INVALID_RSM_SEQ | \
		CAM_JPEG_HW_IRQ_STATUS_DCD_MISSING_RSM | \
		CAM_JPEG_HW_IRQ_STATUS_VIOLATION_MASK)

static struct cam_jpeg_enc_device_hw_info cam_jpeg_enc_165_hw_info = {
	.reg_offset = {
		.hw_version = 0x0,
		.int_clr = 0x1c,
		.int_status = 0x20,
		.int_mask = 0x18,
		.hw_cmd = 0x10,
		.reset_cmd = 0x8,
		.encode_size = 0x180,
		.core_cfg = 0xc,
		.misr_cfg = 0x2B4,
		.misr_rd0 = 0x2B8,
	},
	.reg_val = {
		.int_clr_clearall = 0xFFFFFFFF,
		.int_mask_disable_all = 0x00000000,
		.int_mask_enable_all = 0xFFFFFFFF,
		.hw_cmd_start = 0x00000001,
		.reset_cmd = 0x200320D3,
		.hw_cmd_stop = 0x00000002,
		.misr_cfg = 0x7,
	},
	.int_status = {
		.framedone = CAM_JPEG_HW_MASK_COMP_FRAMEDONE,
		.resetdone = CAM_JPEG_HW_MASK_COMP_RESET_ACK,
		.iserror = CAM_JPEG_HW_MASK_COMP_ERR,
		.stopdone = CAM_JPEG_HW_IRQ_STATUS_STOP_DONE_MASK,
		.scale_enable = CAM_JPEG_HW_MASK_SCALE_ENABLE,
		.scale_enable_shift = 0x7,
	},
	.reg_dump = {
		.start_offset = 0x0,
		.end_offset = 0x33C,
	},
	.camnoc_misr_reg_offset = {
		.main_ctl = 0x5908,
		.id_mask_low = 0x5920,
		.id_value_low = 0x5918,
		.misc_ctl = 0x5910,
		.sigdata0 = 0x5950,
	},
	.camnoc_misr_reg_val = {
		.main_ctl = 0x7,
		.id_mask_low = 0xFC0,
		.id_value_low_rd = 0xD80,
		.id_value_low_wr = 0xDC2,
		.misc_ctl_start = 0x1,
		.misc_ctl_stop = 0x2,
	},
	.max_misr = 3,
	.max_misr_rd = 4,
	.camnoc_misr_sigdata = 4,
	.camnoc_misr_support = 1,
};

#endif /* CAM_JPEG_ENC_165_HW_INFO_TITAN170_H */
