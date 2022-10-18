/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_JPEG_DMA_680_HW_INFO_VER_4_2_0_H
#define CAM_JPEG_DMA_680_HW_INFO_VER_4_2_0_H

#define CAM_JPEGDMA_HW_IRQ_STATUS_SESSION_DONE (1 << 0)
#define CAM_JPEGDMA_HW_IRQ_STATUS_RD_BUF_DONE  (1 << 1)
#define CAM_JPEGDMA_HW_IRQ_STATUS_WR_BUF_DONE  (1 << 5)
#define CAM_JPEGDMA_HW_IRQ_STATUS_AXI_HALT     (1 << 9)
#define CAM_JPEGDMA_HW_IRQ_STATUS_RST_DONE     (1 << 10)

#define CAM_JPEG_HW_MASK_SCALE_ENABLE 0x1

#define CAM_JPEGDMA_HW_MASK_COMP_FRAMEDONE \
		CAM_JPEGDMA_HW_IRQ_STATUS_SESSION_DONE
#define CAM_JPEGDMA_HW_MASK_COMP_RESET_ACK \
		CAM_JPEGDMA_HW_IRQ_STATUS_RST_DONE

static struct cam_jpeg_dma_device_hw_info cam_jpeg_dma_680_hw_info = {
	.reg_offset = {
		.hw_version = 0x0,
		.int_clr = 0x14,
		.int_status = 0x10,
		.int_mask = 0x0C,
		.hw_cmd = 0x1C,
		.reset_cmd = 0x08,
		.encode_size = 0x180,
		.core_cfg = 0x18,
		.misr_cfg0 = 0x160,
		.misr_cfg1 = 0x164,
	},
	.reg_val = {
		.int_clr_clearall = 0xFFFFFFFF,
		.int_mask_disable_all = 0x00000000,
		.int_mask_enable_all = 0xFFFFFFFF,
		.hw_cmd_start = 0x00000001,
		.reset_cmd = 0x32083,
		.hw_cmd_stop = 0x00000004,
		.misr_cfg0 = 0x506,
	},
	.int_status = {
		.framedone = CAM_JPEGDMA_HW_MASK_COMP_FRAMEDONE,
		.resetdone = CAM_JPEGDMA_HW_MASK_COMP_RESET_ACK,
		.iserror = 0x0,
		.stopdone = CAM_JPEGDMA_HW_IRQ_STATUS_AXI_HALT,
		.scale_enable = CAM_JPEG_HW_MASK_SCALE_ENABLE,
		.scale_enable_shift = 0x4,
	},
	.camnoc_misr_reg_offset = {
		.main_ctl = 0x8108,
		.id_mask_low = 0x8120,
		.id_value_low = 0x8118,
		.misc_ctl = 0x8110,
		.sigdata0 = 0x8150,
	},
	.camnoc_misr_reg_val = {
		.main_ctl = 0x7,
		.id_mask_low = 0xFC0,
		.id_value_low_rd = 0xD00,
		.id_value_low_wr = 0xD42,
		.misc_ctl_start = 0x1,
		.misc_ctl_stop = 0x2,
	},
	.max_misr = 3,
	.max_misr_rd = 4,
	.max_misr_wr = 4,
	.camnoc_misr_sigdata = 4,
	.master_we_sel = 2,
	.misr_rd_word_sel = 4,
	.camnoc_misr_support = 1,
};

#endif /* CAM_JPEG_DMA_680_HW_INFO_VER_4_2_0_H */
