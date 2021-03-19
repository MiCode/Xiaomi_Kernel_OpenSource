/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_LITE48x_H_
#define _CAM_VFE_LITE48x_H_

#include "cam_vfe_bus_ver3.h"
#include "cam_irq_controller.h"
#include "cam_vfe_top_ver3.h"
#include "cam_vfe_core.h"

static struct cam_irq_register_set vfe48x_top_irq_reg_set[3] = {
	{
		.mask_reg_offset   = 0x00000028,
		.clear_reg_offset  = 0x00000034,
		.status_reg_offset = 0x00000040,
	},
	{
		.mask_reg_offset   = 0x0000002C,
		.clear_reg_offset  = 0x00000038,
		.status_reg_offset = 0x00000044,
	},
	{
		.mask_reg_offset   = 0x00000030,
		.clear_reg_offset  = 0x0000003C,
		.status_reg_offset = 0x00000048,
	},
};

static struct cam_irq_controller_reg_info vfe48x_top_irq_reg_info = {
	.num_registers = 3,
	.irq_reg_set = vfe48x_top_irq_reg_set,
	.global_clear_offset  = 0x00000024,
	.global_clear_bitmask = 0x00000001,
};

static struct cam_vfe_top_ver3_reg_offset_common vfe48x_top_common_reg = {
	.hw_version               = 0x00000000,
	.titan_version            = 0x00000004,
	.hw_capability            = 0x00000008,
	.global_reset_cmd         = 0x0000000C,
	.core_cgc_ovd_0           = 0x00000010,
	.ahb_cgc_ovd              = 0x00000014,
	.noc_cgc_ovd              = 0x00000018,
	.reg_update_cmd           = 0x00000020,
	.diag_config              = 0x00000050,
	.diag_sensor_status_0     = 0x00000054,
	.bus_overflow_status      = 0x00001A68,
	.top_debug_cfg            = 0x00000074,
	.top_debug_0              = 0x0000005C,
	.top_debug_1              = 0x00000068,
	.top_debug_2              = 0x0000006C,
	.top_debug_3              = 0x00000070,
};

static struct cam_vfe_camif_lite_ver3_reg vfe48x_camif_rdi[4] = {
	{
		.lite_hw_version            = 0x1200,
		.lite_hw_status             = 0x1204,
		.lite_module_config         = 0x1260,
		.lite_skip_period           = 0x1268,
		.lite_irq_subsample_pattern = 0x126C,
		.lite_epoch_irq             = 0x1270,
		.lite_debug_1               = 0x13F0,
		.lite_debug_0               = 0x13F4,
		.lite_test_bus_ctrl         = 0x13F8,
		.camif_lite_spare           = 0x13FC,
		.reg_update_cmd             = 0x0020,
	},
	{
		.lite_hw_version            = 0x1400,
		.lite_hw_status             = 0x1404,
		.lite_module_config         = 0x1460,
		.lite_skip_period           = 0x1468,
		.lite_irq_subsample_pattern = 0x146C,
		.lite_epoch_irq             = 0x1470,
		.lite_debug_1               = 0x15F0,
		.lite_debug_0               = 0x15F4,
		.lite_test_bus_ctrl         = 0x15F8,
		.camif_lite_spare           = 0x15FC,
		.reg_update_cmd             = 0x0020,
	},
	{
		.lite_hw_version            = 0x1600,
		.lite_hw_status             = 0x1604,
		.lite_module_config         = 0x1660,
		.lite_skip_period           = 0x1668,
		.lite_irq_subsample_pattern = 0x166C,
		.lite_epoch_irq             = 0x1670,
		.lite_debug_1               = 0x17F0,
		.lite_debug_0               = 0x17F4,
		.lite_test_bus_ctrl         = 0x17F8,
		.camif_lite_spare           = 0x17FC,
		.reg_update_cmd             = 0x0020,
	},
	{
		.lite_hw_version            = 0x1800,
		.lite_hw_status             = 0x1804,
		.lite_module_config         = 0x1860,
		.lite_skip_period           = 0x1868,
		.lite_irq_subsample_pattern = 0x186C,
		.lite_epoch_irq             = 0x1870,
		.lite_debug_1               = 0x19F0,
		.lite_debug_0               = 0x19F4,
		.lite_test_bus_ctrl         = 0x19F8,
		.camif_lite_spare           = 0x19FC,
		.reg_update_cmd             = 0x0020,
	},
};

static struct cam_vfe_camif_lite_ver3_reg_data vfe48x_camif_rdi_reg_data[4] = {
	{
		.extern_reg_update_shift         = 0,
		.reg_update_cmd_data             = 0x1,
		.epoch_line_cfg                  = 0x0,
		.sof_irq_mask                    = 0x1,
		.epoch0_irq_mask                 = 0x4,
		.epoch1_irq_mask                 = 0x8,
		.eof_irq_mask                    = 0x02,
		.error_irq_mask0                 = 0x1,
		.error_irq_mask2                 = 0x100,
		.subscribe_irq_mask1             = 0x3,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x1,
	},
	{
		.extern_reg_update_shift         = 0,
		.reg_update_cmd_data             = 0x2,
		.epoch_line_cfg                  = 0x0,
		.sof_irq_mask                    = 0x10,
		.epoch0_irq_mask                 = 0x40,
		.epoch1_irq_mask                 = 0x80,
		.eof_irq_mask                    = 0x20,
		.error_irq_mask0                 = 0x2,
		.error_irq_mask2                 = 0x200,
		.subscribe_irq_mask1             = 0x30,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x1,
	},
	{
		.extern_reg_update_shift         = 0,
		.reg_update_cmd_data             = 0x4,
		.epoch_line_cfg                  = 0x0,
		.sof_irq_mask                    = 0x100,
		.epoch0_irq_mask                 = 0x400,
		.epoch1_irq_mask                 = 0x800,
		.eof_irq_mask                    = 0x200,
		.error_irq_mask0                 = 0x4,
		.error_irq_mask2                 = 0x400,
		.subscribe_irq_mask1             = 0x300,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x1,
	},
	{
		.extern_reg_update_shift         = 0,
		.reg_update_cmd_data             = 0x8,
		.epoch_line_cfg                  = 0x0,
		.sof_irq_mask                    = 0x1000,
		.epoch0_irq_mask                 = 0x4000,
		.epoch1_irq_mask                 = 0x8000,
		.eof_irq_mask                    = 0x2000,
		.error_irq_mask0                 = 0x8,
		.error_irq_mask2                 = 0x800,
		.subscribe_irq_mask1             = 0x3000,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x1,
	},
};

static struct cam_vfe_camif_lite_ver3_hw_info
	vfe48x_rdi_hw_info[CAM_VFE_RDI_VER2_MAX] = {
	{
		.common_reg     = &vfe48x_top_common_reg,
		.camif_lite_reg = &vfe48x_camif_rdi[0],
		.reg_data       = &vfe48x_camif_rdi_reg_data[0],
	},
	{
		.common_reg     = &vfe48x_top_common_reg,
		.camif_lite_reg = &vfe48x_camif_rdi[1],
		.reg_data       = &vfe48x_camif_rdi_reg_data[1],
	},
	{
		.common_reg     = &vfe48x_top_common_reg,
		.camif_lite_reg = &vfe48x_camif_rdi[2],
		.reg_data       = &vfe48x_camif_rdi_reg_data[2],
	},
	{
		.common_reg     = &vfe48x_top_common_reg,
		.camif_lite_reg = &vfe48x_camif_rdi[3],
		.reg_data       = &vfe48x_camif_rdi_reg_data[3],
	},
};

static struct cam_vfe_top_ver3_hw_info vfe48x_top_hw_info = {
	.common_reg = &vfe48x_top_common_reg,
	.rdi_hw_info[0] = &vfe48x_rdi_hw_info[0],
	.rdi_hw_info[1] = &vfe48x_rdi_hw_info[1],
	.rdi_hw_info[2] = &vfe48x_rdi_hw_info[2],
	.rdi_hw_info[3] = &vfe48x_rdi_hw_info[3],
	.num_mux = 4,
	.mux_type = {
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
	},
};

static struct cam_irq_register_set vfe48x_bus_irq_reg[2] = {
	{
		.mask_reg_offset   = 0x00001A18,
		.clear_reg_offset  = 0x00001A20,
		.status_reg_offset = 0x00001A28,
	},
	{
		.mask_reg_offset   = 0x00001A1C,
		.clear_reg_offset  = 0x00001A24,
		.status_reg_offset = 0x00001A2C,
	},
};

static struct cam_vfe_bus_ver3_hw_info vfe48x_bus_hw_info = {
	.common_reg = {
		.hw_version                       = 0x00001A00,
		.cgc_ovd                          = 0x00001A08,
		.if_frameheader_cfg               = {
			0x00001A34,
			0x00001A38,
			0x00001A3C,
			0x00001A40,
		},
		.pwr_iso_cfg                      = 0x00001A5C,
		.overflow_status_clear            = 0x00001A60,
		.ccif_violation_status            = 0x00001A64,
		.overflow_status                  = 0x00001A68,
		.image_size_violation_status      = 0x00001A70,
		.debug_status_top_cfg             = 0x00001AD4,
		.debug_status_top                 = 0x00001AD8,
		.test_bus_ctrl                    = 0x00001ADC,
		.irq_reg_info = {
			.num_registers            = 2,
			.irq_reg_set              = vfe48x_bus_irq_reg,
			.global_clear_offset      = 0x00001A30,
			.global_clear_bitmask     = 0x00000001,
		},
	},
	.num_client = 4,
	.bus_client_reg = {
		/* RDI 0 */
		{
			.cfg                      = 0x00001C00,
			.image_addr               = 0x00001C04,
			.frame_incr               = 0x00001C08,
			.image_cfg_0              = 0x00001C0C,
			.image_cfg_1              = 0x00001C10,
			.image_cfg_2              = 0x00001C14,
			.packer_cfg               = 0x00001C18,
			.frame_header_addr        = 0x00001C20,
			.frame_header_incr        = 0x00001C24,
			.frame_header_cfg         = 0x00001C28,
			.line_done_cfg            = 0x00001C2C,
			.irq_subsample_period     = 0x00001C30,
			.irq_subsample_pattern    = 0x00001C34,
			.framedrop_period         = 0x00001C38,
			.framedrop_pattern        = 0x00001C3C,
			.system_cache_cfg         = 0x00001C60,
			.burst_limit              = 0x00001C64,
			.addr_status_0            = 0x00001C68,
			.addr_status_1            = 0x00001C6C,
			.addr_status_2            = 0x00001C70,
			.addr_status_3            = 0x00001C74,
			.debug_status_cfg         = 0x00001C78,
			.debug_status_0           = 0x00001C7C,
			.debug_status_1           = 0x00001C80,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = NULL,
		},
		/* RDI 1 */
		{
			.cfg                      = 0x00001D00,
			.image_addr               = 0x00001D04,
			.frame_incr               = 0x00001D08,
			.image_cfg_0              = 0x00001D0C,
			.image_cfg_1              = 0x00001D10,
			.image_cfg_2              = 0x00001D14,
			.packer_cfg               = 0x00001D18,
			.frame_header_addr        = 0x00001D20,
			.frame_header_incr        = 0x00001D24,
			.frame_header_cfg         = 0x00001D28,
			.line_done_cfg            = 0x00001D2C,
			.irq_subsample_period     = 0x00001D30,
			.irq_subsample_pattern    = 0x00001D34,
			.framedrop_period         = 0x00001D38,
			.framedrop_pattern        = 0x00001D3C,
			.system_cache_cfg         = 0x00001D60,
			.burst_limit              = 0x00001D64,
			.addr_status_0            = 0x00001D68,
			.addr_status_1            = 0x00001D6C,
			.addr_status_2            = 0x00001D70,
			.addr_status_3            = 0x00001D74,
			.debug_status_cfg         = 0x00001D78,
			.debug_status_0           = 0x00001D7C,
			.debug_status_1           = 0x00001D80,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = NULL,
		},
		/* RDI 2 */
		{
			.cfg                      = 0x00001E00,
			.image_addr               = 0x00001E04,
			.frame_incr               = 0x00001E08,
			.image_cfg_0              = 0x00001E0C,
			.image_cfg_1              = 0x00001E10,
			.image_cfg_2              = 0x00001E14,
			.packer_cfg               = 0x00001E18,
			.frame_header_addr        = 0x00001E20,
			.frame_header_incr        = 0x00001E24,
			.frame_header_cfg         = 0x00001E28,
			.line_done_cfg            = 0x00001E2C,
			.irq_subsample_period     = 0x00001E30,
			.irq_subsample_pattern    = 0x00001E34,
			.framedrop_period         = 0x00001E38,
			.framedrop_pattern        = 0x00001E3C,
			.system_cache_cfg         = 0x00001E60,
			.burst_limit              = 0x00001E64,
			.addr_status_0            = 0x00001E68,
			.addr_status_1            = 0x00001E6C,
			.addr_status_2            = 0x00001E70,
			.addr_status_3            = 0x00001E74,
			.debug_status_cfg         = 0x00001E78,
			.debug_status_0           = 0x00001E7C,
			.debug_status_1           = 0x00001E80,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
		},
		/* RDI 3 */
		{
			.cfg                      = 0x00001F00,
			.image_addr               = 0x00001F04,
			.frame_incr               = 0x00001F08,
			.image_cfg_0              = 0x00001F0C,
			.image_cfg_1              = 0x00001F10,
			.image_cfg_2              = 0x00001F14,
			.packer_cfg               = 0x00001F18,
			.frame_header_addr        = 0x00001F20,
			.frame_header_incr        = 0x00001F24,
			.frame_header_cfg         = 0x00001F28,
			.line_done_cfg            = 0x00001F2C,
			.irq_subsample_period     = 0x00001F30,
			.irq_subsample_pattern    = 0x00001F34,
			.framedrop_period         = 0x00001F38,
			.framedrop_pattern        = 0x00001F3C,
			.system_cache_cfg         = 0x00001F60,
			.burst_limit              = 0x00001F64,
			.addr_status_0            = 0x00001F68,
			.addr_status_1            = 0x00001F6C,
			.addr_status_2            = 0x00001F70,
			.addr_status_3            = 0x00001F74,
			.debug_status_cfg         = 0x00001F78,
			.debug_status_0           = 0x00001F7C,
			.debug_status_1           = 0x00001F80,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_3,
			.ubwc_regs                = NULL,
		},
	},
	.num_out = 4,
	.vfe_out_hw_info = {
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI0,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI1,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI2,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_2,
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI3,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_3,
		},
	},
	.comp_done_shift = 4,
	.top_irq_shift   = 4,
};

static struct cam_vfe_hw_info cam_vfe_lite48x_hw_info = {
	.irq_reg_info                  = &vfe48x_top_irq_reg_info,

	.bus_version                   = CAM_VFE_BUS_VER_3_0,
	.bus_hw_info                   = &vfe48x_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_3_0,
	.top_hw_info                   = &vfe48x_top_hw_info,

};

#endif /* _CAM_VFE_LITE48x_H_ */
