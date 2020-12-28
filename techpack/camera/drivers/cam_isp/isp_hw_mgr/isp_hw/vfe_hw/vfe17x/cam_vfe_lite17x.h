/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_LITE17X_H_
#define _CAM_VFE_LITE17X_H_

#include "cam_vfe_bus_ver2.h"
#include "cam_irq_controller.h"
#include "cam_vfe_top_ver2.h"
#include "cam_vfe_core.h"

static struct cam_irq_register_set vfe17x_top_irq_reg_set[2] = {
	{
		.mask_reg_offset   = 0x0000005C,
		.clear_reg_offset  = 0x00000064,
		.status_reg_offset = 0x0000006C,
	},
	{
		.mask_reg_offset   = 0x00000060,
		.clear_reg_offset  = 0x00000068,
		.status_reg_offset = 0x00000070,
	},
};

static struct cam_irq_controller_reg_info vfe17x_top_irq_reg_info = {
	.num_registers = 2,
	.irq_reg_set = vfe17x_top_irq_reg_set,
	.global_clear_offset  = 0x00000058,
	.global_clear_bitmask = 0x00000001,
};

static struct cam_vfe_top_ver2_reg_offset_common vfe17x_top_common_reg = {
	.hw_version               = 0x00000000,
	.hw_capability            = 0x00000004,
	.lens_feature             = 0x00000008,
	.stats_feature            = 0x0000000C,
	.color_feature            = 0x00000010,
	.zoom_feature             = 0x00000014,
	.global_reset_cmd         = 0x00000018,
	.module_ctrl              = {
		NULL,
		NULL,
		NULL,
		NULL,
	},
	.bus_cgc_ovd              = 0x0000003C,
	.core_cfg                 = 0x00000000,
	.three_D_cfg              = 0x00000000,
	.violation_status         = 0x0000007C,
	.reg_update_cmd           = 0x000004AC,
};

static struct cam_vfe_rdi_ver2_reg vfe17x_rdi_reg = {
	.reg_update_cmd           = 0x000004AC,
};

static struct cam_vfe_rdi_common_reg_data vfe17x_rdi_reg_data = {
	.subscribe_irq_mask0      = 0x780001E0,
	.subscribe_irq_mask1      = 0x0,
	.error_irq_mask0          = 0x0,
	.error_irq_mask1          = 0x3C,
};

static struct cam_vfe_rdi_reg_data  vfe17x_rdi_0_data = {
	.reg_update_cmd_data      = 0x2,
	.sof_irq_mask             = 0x8000000,
	.reg_update_irq_mask      = 0x20,
};

static struct cam_vfe_rdi_reg_data  vfe17x_rdi_1_data = {
	.reg_update_cmd_data      = 0x4,
	.sof_irq_mask             = 0x10000000,
	.reg_update_irq_mask      = 0x40,
};

static struct cam_vfe_rdi_reg_data  vfe17x_rdi_2_data = {
	.reg_update_cmd_data      = 0x8,
	.sof_irq_mask             = 0x20000000,
	.reg_update_irq_mask      = 0x80,
};

static struct cam_vfe_rdi_reg_data  vfe17x_rdi_3_data = {
	.reg_update_cmd_data      = 0x10,
	.sof_irq_mask             = 0x40000000,
	.reg_update_irq_mask      = 0x100,
};

static struct cam_vfe_rdi_overflow_status vfe17x_rdi_irq_status = {
	.rdi0_overflow_mask = 0x8,
	.rdi1_overflow_mask = 0x10,
	.rdi2_overflow_mask = 0x18,
	.rdi3_overflow_mask = 0x20,
	.rdi_overflow_mask  = 0x3c,
};

static struct cam_vfe_top_ver2_hw_info vfe17x_top_hw_info = {
	.common_reg = &vfe17x_top_common_reg,
	.camif_hw_info = {
		.common_reg = NULL,
		.camif_reg  = NULL,
		.reg_data   = NULL,
		},
	.rdi_hw_info = {
		.common_reg = &vfe17x_top_common_reg,
		.rdi_reg    = &vfe17x_rdi_reg,
		.common_reg_data = &vfe17x_rdi_reg_data,
		.reg_data = {
			&vfe17x_rdi_0_data,
			&vfe17x_rdi_1_data,
			&vfe17x_rdi_2_data,
			&vfe17x_rdi_3_data,
			},
		.rdi_irq_status  = &vfe17x_rdi_irq_status,
		},
	.num_mux = 4,
	.mux_type = {
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
	},
};

static struct cam_irq_register_set vfe17x_bus_irq_reg[3] = {
		{
			.mask_reg_offset   = 0x00002044,
			.clear_reg_offset  = 0x00002050,
			.status_reg_offset = 0x0000205C,
		},
		{
			.mask_reg_offset   = 0x00002048,
			.clear_reg_offset  = 0x00002054,
			.status_reg_offset = 0x00002060,
		},
		{
			.mask_reg_offset   = 0x0000204C,
			.clear_reg_offset  = 0x00002058,
			.status_reg_offset = 0x00002064,
		},
};

static struct cam_vfe_bus_ver2_hw_info vfe17x_bus_hw_info = {
	.common_reg = {
		.hw_version                   = 0x00002000,
		.hw_capability                = 0x00002004,
		.sw_reset                     = 0x00002008,
		.cgc_ovd                      = 0x0000200C,
		.pwr_iso_cfg                  = 0x000020CC,
		.dual_master_comp_cfg         = 0x00002028,
		.irq_reg_info = {
			.num_registers        = 3,
			.irq_reg_set          = vfe17x_bus_irq_reg,
			.global_clear_offset  = 0x00002068,
			.global_clear_bitmask = 0x00000001,
		},
		.comp_error_status            = 0x0000206C,
		.comp_ovrwr_status            = 0x00002070,
		.dual_comp_error_status       = 0x00002074,
		.dual_comp_ovrwr_status       = 0x00002078,
		.addr_sync_cfg                = 0x0000207C,
		.addr_sync_frame_hdr          = 0x00002080,
		.addr_sync_no_sync            = 0x00002084,
		.top_irq_mask_0               = 0x0000005C,
	},
	.num_client = 4,
	.bus_client_reg = {
		/* BUS Client 0 */
		{
			.status0                  = 0x00002200,
			.status1                  = 0x00002204,
			.cfg                      = 0x00002208,
			.header_addr              = 0x0000220C,
			.header_cfg               = 0x00002210,
			.image_addr               = 0x00002214,
			.image_addr_offset        = 0x00002218,
			.buffer_width_cfg         = 0x0000221C,
			.buffer_height_cfg        = 0x00002220,
			.packer_cfg               = 0x00002224,
			.stride                   = 0x00002228,
			.irq_subsample_period     = 0x00002248,
			.irq_subsample_pattern    = 0x0000224C,
			.framedrop_period         = 0x00002250,
			.framedrop_pattern        = 0x00002254,
			.frame_inc                = 0x00002258,
			.burst_limit              = 0x0000225C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 1 */
		{
			.status0                  = 0x00002300,
			.status1                  = 0x00002304,
			.cfg                      = 0x00002308,
			.header_addr              = 0x0000230C,
			.header_cfg               = 0x00002310,
			.image_addr               = 0x00002314,
			.image_addr_offset        = 0x00002318,
			.buffer_width_cfg         = 0x0000231C,
			.buffer_height_cfg        = 0x00002320,
			.packer_cfg               = 0x00002324,
			.stride                   = 0x00002328,
			.irq_subsample_period     = 0x00002348,
			.irq_subsample_pattern    = 0x0000234C,
			.framedrop_period         = 0x00002350,
			.framedrop_pattern        = 0x00002354,
			.frame_inc                = 0x00002358,
			.burst_limit              = 0x0000235C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 2 */
		{
			.status0                  = 0x00002400,
			.status1                  = 0x00002404,
			.cfg                      = 0x00002408,
			.header_addr              = 0x0000240C,
			.header_cfg               = 0x00002410,
			.image_addr               = 0x00002414,
			.image_addr_offset        = 0x00002418,
			.buffer_width_cfg         = 0x0000241C,
			.buffer_height_cfg        = 0x00002420,
			.packer_cfg               = 0x00002424,
			.stride                   = 0x00002428,
			.irq_subsample_period     = 0x00002448,
			.irq_subsample_pattern    = 0x0000244C,
			.framedrop_period         = 0x00002450,
			.framedrop_pattern        = 0x00002454,
			.frame_inc                = 0x00002458,
			.burst_limit              = 0x0000245C,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 3 */
		{
			.status0                  = 0x00002500,
			.status1                  = 0x00002504,
			.cfg                      = 0x00002508,
			.header_addr              = 0x0000250C,
			.header_cfg               = 0x00002510,
			.image_addr               = 0x00002514,
			.image_addr_offset        = 0x00002518,
			.buffer_width_cfg         = 0x0000251C,
			.buffer_height_cfg        = 0x00002520,
			.packer_cfg               = 0x00002524,
			.stride                   = 0x00002528,
			.irq_subsample_period     = 0x00002548,
			.irq_subsample_pattern    = 0x0000254C,
			.framedrop_period         = 0x00002550,
			.framedrop_pattern        = 0x00002554,
			.frame_inc                = 0x00002558,
			.burst_limit              = 0x0000255C,
			.ubwc_regs                = NULL,
		},
	},
	.comp_grp_reg = {
		/* CAM_VFE_BUS_VER2_COMP_GRP_0 */
		{
			.comp_mask                    = 0x00002010,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_1 */
		{
			.comp_mask                    = 0x00002014,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_2 */
		{
			.comp_mask                    = 0x00002018,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_3 */
		{
			.comp_mask                    = 0x0000201C,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_4 */
		{
			.comp_mask                    = 0x00002020,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_5 */
		{
			.comp_mask                    = 0x00002024,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0 */
		{
			.comp_mask                    = 0x0000202C,
			.addr_sync_mask               = 0x00002088,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_1 */
		{
			.comp_mask                    = 0x00002030,
			.addr_sync_mask               = 0x0000208C,

		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_2 */
		{
			.comp_mask                    = 0x00002034,
			.addr_sync_mask               = 0x00002090,

		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_3 */
		{
			.comp_mask                    = 0x00002038,
			.addr_sync_mask               = 0x00002094,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_4 */
		{
			.comp_mask                    = 0x0000203C,
			.addr_sync_mask               = 0x00002098,
		},
		/* CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5 */
		{
			.comp_mask                    = 0x00002040,
			.addr_sync_mask               = 0x0000209C,
		},
	},
	.num_out = 4,
	.vfe_out_hw_info = {
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RDI0,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RDI1,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RDI2,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER2_VFE_OUT_RDI3,
			.max_width     = -1,
			.max_height    = -1,
		},
	},
	.top_irq_shift = 9,
	.support_consumed_addr = false,
};

static struct cam_vfe_hw_info cam_vfe_lite17x_hw_info = {
	.irq_reg_info                  = &vfe17x_top_irq_reg_info,

	.bus_version                   = CAM_VFE_BUS_VER_2_0,
	.bus_hw_info                   = &vfe17x_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_2_0,
	.top_hw_info                   = &vfe17x_top_hw_info,

};

#endif /* _CAM_VFE_LITE17X_H_ */
