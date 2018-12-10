/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_FD_HW_V501_H_
#define _CAM_FD_HW_V501_H_

static struct cam_fd_hw_static_info cam_fd_wrapper200_core501_info = {
	.core_version = {
		.major  = 5,
		.minor  = 0,
		.incr   = 1,
	},
	.wrapper_version = {
		.major  = 2,
		.minor  = 0,
		.incr   = 0,
	},
	.core_regs = {
		.version               = 0x38,
		.control               = 0x0,
		.result_cnt            = 0x4,
		.result_addr           = 0x20,
		.image_addr            = 0x24,
		.work_addr             = 0x28,
		.ro_mode               = 0x34,
		.results_reg_base      = 0x400,
		.raw_results_reg_base  = 0x800,
	},
	.wrapper_regs = {
		.wrapper_version       = 0x0,
		.cgc_disable           = 0x4,
		.hw_stop               = 0x8,
		.sw_reset              = 0x10,
		.vbif_req_priority     = 0x20,
		.vbif_priority_level   = 0x24,
		.vbif_done_status      = 0x34,
		.irq_mask              = 0x50,
		.irq_status            = 0x54,
		.irq_clear             = 0x58,
	},
	.results = {
		.max_faces             = 35,
		.per_face_entries      = 4,
		.raw_results_available = true,
		.raw_results_entries   = 512,
	},
	.enable_errata_wa = {
		.single_irq_only         = true,
		.ro_mode_enable_always   = true,
		.ro_mode_results_invalid = true,
	},
	.irq_mask = CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_FRAME_DONE) |
		CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_HALT_DONE) |
		CAM_FD_IRQ_TO_MASK(CAM_FD_IRQ_RESET_DONE),
	.qos_priority       = 4,
	.qos_priority_level = 4,
	.supported_modes    = CAM_FD_MODE_FACEDETECTION | CAM_FD_MODE_PYRAMID,
	.ro_mode_supported  = true,
};

#endif /* _CAM_FD_HW_V501_H_ */
