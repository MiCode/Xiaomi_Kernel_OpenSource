/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */


#ifndef _CAM_VFE570_H_
#define _CAM_VFE570_H_
#include "cam_vfe480.h"
#include "cam_vfe_top_ver3.h"
#include "cam_vfe_core.h"

static struct cam_vfe_camif_ver3_reg_data vfe_570_camif_reg_data = {
	.pp_extern_reg_update_shift      = 4,
	.dual_pd_extern_reg_update_shift = 17,
	.extern_reg_update_mask          = 1,
	.dual_ife_pix_en_shift           = 3,
	.dual_ife_sync_sel_shift         = 18,
	.operating_mode_shift            = 11,
	.input_mux_sel_shift             = 5,
	.pixel_pattern_shift             = 24,
	.pixel_pattern_mask              = 0x7,
	.dsp_mode_shift                  = 24,
	.dsp_mode_mask                   = 0x1,
	.dsp_en_shift                    = 23,
	.dsp_en_mask                     = 0x1,
	.reg_update_cmd_data             = 0x41,
	.epoch_line_cfg                  = 0x00000014,
	.sof_irq_mask                    = 0x00000001,
	.epoch0_irq_mask                 = 0x00000004,
	.epoch1_irq_mask                 = 0x00000008,
	.eof_irq_mask                    = 0x00000002,
	.error_irq_mask0                 = 0x82000200,
	.error_irq_mask2                 = 0x30301F80,
	.subscribe_irq_mask1             = 0x00000007,
	.enable_diagnostic_hw            = 0x1,
	.pp_camif_cfg_en_shift           = 0,
	.pp_camif_cfg_ife_out_en_shift   = 8,
	.top_debug_cfg_en                = 1,
	.dual_vfe_sync_mask              = 0x3,
	.input_bayer_fmt                 = 0,
	.input_yuv_fmt                   = 1,

};

static struct cam_vfe_camif_lite_ver3_reg_data vfe570_camif_rdi1_reg_data = {
	.extern_reg_update_shift         = 0,
	.input_mux_sel_shift             = 7,
	.reg_update_cmd_data             = 0x4,
	.epoch_line_cfg                  = 0x0,
	.sof_irq_mask                    = 0x100,
	.epoch0_irq_mask                 = 0x400,
	.epoch1_irq_mask                 = 0x800,
	.eof_irq_mask                    = 0x200,
	.error_irq_mask0                 = 0x10000000,
	.error_irq_mask2                 = 0x40000,
	.subscribe_irq_mask1             = 0x300,
	.enable_diagnostic_hw            = 0x1,
};

struct cam_vfe_camif_lite_ver3_hw_info
	vfe570_rdi_hw_info_arr[CAM_VFE_RDI_VER2_MAX] = {
	{
		.common_reg     = &vfe480_top_common_reg,
		.camif_lite_reg = &vfe480_camif_rdi[0],
		.reg_data       = &vfe480_camif_rdi_reg_data[0],
	},
	{
		.common_reg     = &vfe480_top_common_reg,
		.camif_lite_reg = &vfe480_camif_rdi[1],
		.reg_data       = &vfe570_camif_rdi1_reg_data,
	},
	{
		.common_reg     = &vfe480_top_common_reg,
		.camif_lite_reg = &vfe480_camif_rdi[2],
		.reg_data       = &vfe480_camif_rdi_reg_data[2],
	},
};

static struct cam_vfe_top_ver3_hw_info vfe570_top_hw_info = {
	.common_reg = &vfe480_top_common_reg,
	.camif_hw_info = {
		.common_reg     = &vfe480_top_common_reg,
		.camif_reg      = &vfe480_camif_reg,
		.reg_data       = &vfe_570_camif_reg_data,
		},
	.pdlib_hw_info = {
		.common_reg     = &vfe480_top_common_reg,
		.camif_lite_reg = &vfe480_camif_pd,
		.reg_data       = &vfe480_camif_pd_reg_data,
		},
	.rdi_hw_info[0] = &vfe570_rdi_hw_info_arr[0],
	.rdi_hw_info[1] = &vfe570_rdi_hw_info_arr[1],
	.rdi_hw_info[2] = &vfe570_rdi_hw_info_arr[2],
	.lcr_hw_info = {
		.common_reg     = &vfe480_top_common_reg,
		.camif_lite_reg = &vfe480_camif_lcr,
		.reg_data       = &vfe480_camif_lcr_reg_data,
		},
	.num_mux = 6,
	.mux_type = {
		CAM_VFE_CAMIF_VER_3_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_PDLIB_VER_1_0,
		CAM_VFE_LCR_VER_1_0,
	},
	.num_path_port_map = 2,
	.path_port_map = {
		{CAM_ISP_HW_VFE_IN_LCR, CAM_ISP_IFE_OUT_RES_LCR},
		{CAM_ISP_HW_VFE_IN_PDLIB, CAM_ISP_IFE_OUT_RES_2PD},
	},
};

static struct cam_vfe_hw_info cam_vfe570_hw_info = {
	.irq_hw_info                   = &vfe480_irq_hw_info,

	.bus_version                   = CAM_VFE_BUS_VER_3_0,
	.bus_hw_info                   = &vfe480_bus_hw_info,

	.bus_rd_version                = CAM_VFE_BUS_RD_VER_1_0,
	.bus_rd_hw_info                = &vfe480_bus_rd_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_3_0,
	.top_hw_info                   = &vfe570_top_hw_info,
};

#endif /* _CAM_VFE570_H_ */
