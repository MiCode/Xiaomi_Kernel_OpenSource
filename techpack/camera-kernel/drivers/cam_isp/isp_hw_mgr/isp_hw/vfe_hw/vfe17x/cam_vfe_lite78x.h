/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */


#ifndef _CAM_VFE_LITE78X_H_
#define _CAM_VFE_LITE78X_H_
#include "cam_vfe_camif_ver3.h"
#include "cam_vfe_top_ver4.h"
#include "cam_vfe_core.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_irq_controller.h"

#define CAM_VFE_78X_NUM_DBG_REG 5

static struct cam_vfe_top_ver4_module_desc vfe_lite78x_ipp_mod_desc[] = {
	{
		.id = 0,
		.desc = "CLC_BLS",
	},
	{
		.id = 1,
		.desc = "CLC_GLUT",
	},
	{
		.id = 2,
		.desc = "CLC_STATS_BG",
	},
};

static struct cam_vfe_top_ver4_wr_client_desc vfe_lite78x_wr_client_desc[] = {
	{
		.wm_id = 0,
		.desc = "RDI_0",
	},
	{
		.wm_id = 1,
		.desc = "RDI_1",
	},
	{
		.wm_id = 2,
		.desc = "RDI_2",
	},
	{
		.wm_id = 3,
		.desc = "RDI_3",
	},
	{
		.wm_id = 4,
		.desc = "GAMMA",
	},
	{
		.wm_id = 5,
		.desc = "BE",
	},
};

static struct cam_irq_register_set vfe_lite78x_top_irq_reg_set[2] = {
	{
		.mask_reg_offset   = 0x00001024,
		.clear_reg_offset  = 0x0000102C,
		.status_reg_offset = 0x0000101C,
	},
	{
		.mask_reg_offset   = 0x00001028,
		.clear_reg_offset  = 0x00001030,
		.status_reg_offset = 0x00001020,
	},
};

static struct cam_irq_controller_reg_info vfe_lite78x_top_irq_reg_info = {
	.num_registers = 2,
	.irq_reg_set = vfe_lite78x_top_irq_reg_set,
	.global_clear_offset  = 0x00001038,
	.global_clear_bitmask = 0x00000001,
	.clear_all_bitmask = 0xFFFFFFFF,
};

static struct cam_vfe_top_ver4_reg_offset_common vfe_lite78x_top_common_reg = {
	.hw_version               = 0x00001000,
	.hw_capability            = 0x00001004,
	.core_cgc_ovd_0           = 0x00001014,
	.ahb_cgc_ovd              = 0x00001018,
	.core_cfg_0               = 0x0000103C,
	.diag_config              = 0x00001040,
	.diag_sensor_status_0     = 0x00001044,
	.diag_sensor_status_1     = 0x00001048,
	.ipp_violation_status     = 0x00001054,
	.bus_violation_status     = 0x00001264,
	.bus_overflow_status      = 0x00001268,
	.top_debug_cfg            = 0x00001074,
	.num_top_debug_reg        = CAM_VFE_78X_NUM_DBG_REG,
	.top_debug                = {
		0x0000105C,
		0x00001060,
		0x00001064,
		0x00001068,
		0x0000106C,
	},
};

static struct cam_vfe_ver4_path_reg_data vfe_lite78x_ipp_reg_data =
{
	.sof_irq_mask                    = 0x1,
	.eof_irq_mask                    = 0x2,
	.error_irq_mask                  = 0x2,
	.enable_diagnostic_hw            = 0x1,
	.top_debug_cfg_en                = 0x3,
	.ipp_violation_mask              = 0x10,
};

static struct cam_vfe_ver4_path_reg_data vfe_lite78x_rdi_reg_data[4] = {

	{
		.sof_irq_mask                    = 0x4,
		.eof_irq_mask                    = 0x8,
		.error_irq_mask                  = 0x0,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x3,
	},
	{
		.sof_irq_mask                    = 0x10,
		.eof_irq_mask                    = 0x20,
		.error_irq_mask                  = 0x0,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x3,
	},
	{
		.sof_irq_mask                    = 0x40,
		.eof_irq_mask                    = 0x80,
		.error_irq_mask                  = 0x0,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x3,
	},
	{
		.sof_irq_mask                    = 0x100,
		.eof_irq_mask                    = 0x200,
		.error_irq_mask                  = 0x0,
		.enable_diagnostic_hw            = 0x1,
		.top_debug_cfg_en                = 0x3,
	},
};

static struct cam_vfe_ver4_path_hw_info
	vfe_lite78x_rdi_hw_info[CAM_VFE_RDI_VER2_MAX] = {
	{
		.common_reg     = &vfe_lite78x_top_common_reg,
		.reg_data       = &vfe_lite78x_rdi_reg_data[0],
	},
	{
		.common_reg     = &vfe_lite78x_top_common_reg,
		.reg_data       = &vfe_lite78x_rdi_reg_data[1],
	},
	{
		.common_reg     = &vfe_lite78x_top_common_reg,
		.reg_data       = &vfe_lite78x_rdi_reg_data[2],
	},
	{
		.common_reg     = &vfe_lite78x_top_common_reg,
		.reg_data       = &vfe_lite78x_rdi_reg_data[3],
	},
};

static struct cam_vfe_top_ver4_debug_reg_info vfe78x_dbg_reg_info[CAM_VFE_78X_NUM_DBG_REG][8] = {
	VFE_DBG_INFO_ARRAY_4bit(
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved"
	),
	{
		VFE_DBG_INFO(32, "non-CLC info"),
		VFE_DBG_INFO(32, "non-CLC info"),
		VFE_DBG_INFO(32, "non-CLC info"),
		VFE_DBG_INFO(32, "non-CLC info"),
		VFE_DBG_INFO(32, "non-CLC info"),
		VFE_DBG_INFO(32, "non-CLC info"),
		VFE_DBG_INFO(32, "non-CLC info"),
		VFE_DBG_INFO(32, "non-CLC info"),
	},
	VFE_DBG_INFO_ARRAY_4bit(
		"PP_THROTTLE",
		"STATS_BG_THROTTLE",
		"STATS_BG",
		"BLS",
		"GLUT",
		"unused",
		"unused",
		"unused"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"RDI_0",
		"RDI_1",
		"RDI_2",
		"RDI_3",
		"PP_STATS_BG",
		"PP_GLUT",
		"PP_STATS_BG",
		"PP_GLUT"
	),
	VFE_DBG_INFO_ARRAY_4bit(
		"unused",
		"unused",
		"unused",
		"unused",
		"unused",
		"unused",
		"unused",
		"unused"
	),
};

static struct cam_vfe_top_ver4_hw_info vfe_lite78x_top_hw_info = {
	.common_reg = &vfe_lite78x_top_common_reg,
	.rdi_hw_info[0] = &vfe_lite78x_rdi_hw_info[0],
	.rdi_hw_info[1] = &vfe_lite78x_rdi_hw_info[1],
	.rdi_hw_info[2] = &vfe_lite78x_rdi_hw_info[2],
	.rdi_hw_info[3] = &vfe_lite78x_rdi_hw_info[3],
	.vfe_full_hw_info = {
		.common_reg     = &vfe_lite78x_top_common_reg,
		.reg_data       = &vfe_lite78x_ipp_reg_data,
	},
	.ipp_module_desc        = vfe_lite78x_ipp_mod_desc,
	.wr_client_desc         = vfe_lite78x_wr_client_desc,
	.num_mux = 5,
	.mux_type = {
		CAM_VFE_CAMIF_VER_4_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
		CAM_VFE_RDI_VER_1_0,
	},
	.debug_reg_info = &vfe78x_dbg_reg_info,
};

static struct cam_irq_register_set vfe_lite78x_bus_irq_reg[1] = {
	{
		.mask_reg_offset   = 0x00001218,
		.clear_reg_offset  = 0x00001220,
		.status_reg_offset = 0x00001228,
	},
};

static struct cam_vfe_bus_ver3_hw_info vfe_lite78x_bus_hw_info = {
	.common_reg = {
		.hw_version                       = 0x00001200,
		.cgc_ovd                          = 0x00001208,
		.if_frameheader_cfg               = {
			0x00001234,
			0x00001238,
			0x0000123C,
			0x00001240,
			0x00001244,
		},
		.pwr_iso_cfg                      = 0x0000125C,
		.overflow_status_clear            = 0x00001260,
		.ccif_violation_status            = 0x00001264,
		.overflow_status                  = 0x00001268,
		.image_size_violation_status      = 0x00001270,
		.debug_status_top_cfg             = 0x000012D4,
		.debug_status_top                 = 0x000012D8,
		.test_bus_ctrl                    = 0x000012DC,
		.irq_reg_info = {
			.num_registers            = 1,
			.irq_reg_set              = vfe_lite78x_bus_irq_reg,
			.global_clear_offset      = 0x00001230,
			.global_clear_bitmask     = 0x00000001,
		},
	},
	.num_client = 6,
	.bus_client_reg = {
		/* BUS Client 0 RDI0 */
		{
			.cfg                      = 0x00001400,
			.image_addr               = 0x00001404,
			.frame_incr               = 0x00001408,
			.image_cfg_0              = 0x0000140C,
			.image_cfg_1              = 0x00001410,
			.image_cfg_2              = 0x00001414,
			.packer_cfg               = 0x00001418,
			.frame_header_addr        = 0x00001420,
			.frame_header_incr        = 0x00001424,
			.frame_header_cfg         = 0x00001428,
			.line_done_cfg            = 0x0000142C,
			.irq_subsample_period     = 0x00001430,
			.irq_subsample_pattern    = 0x00001434,
			.mmu_prefetch_cfg         = 0x00001460,
			.mmu_prefetch_max_offset  = 0x00001464,
			.system_cache_cfg         = 0x00001468,
			.addr_cfg                 = 0x00001470,
			.addr_status_0            = 0x00001474,
			.addr_status_1            = 0x00001478,
			.addr_status_2            = 0x0000147C,
			.addr_status_3            = 0x00001480,
			.debug_status_cfg         = 0x00001484,
			.debug_status_0           = 0x00001488,
			.debug_status_1           = 0x0000148C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_1,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 1 RDI1 */
		{
			.cfg                      = 0x00001500,
			.image_addr               = 0x00001504,
			.frame_incr               = 0x00001508,
			.image_cfg_0              = 0x0000150C,
			.image_cfg_1              = 0x00001510,
			.image_cfg_2              = 0x00001514,
			.packer_cfg               = 0x00001518,
			.frame_header_addr        = 0x00001520,
			.frame_header_incr        = 0x00001524,
			.frame_header_cfg         = 0x00001528,
			.line_done_cfg            = 0x0000152C,
			.irq_subsample_period     = 0x00001530,
			.irq_subsample_pattern    = 0x00001534,
			.mmu_prefetch_cfg         = 0x00001560,
			.mmu_prefetch_max_offset  = 0x00001564,
			.system_cache_cfg         = 0x00001568,
			.addr_cfg                 = 0x00001570,
			.addr_status_0            = 0x00001574,
			.addr_status_1            = 0x00001578,
			.addr_status_2            = 0x0000157C,
			.addr_status_3            = 0x00001580,
			.debug_status_cfg         = 0x00001584,
			.debug_status_0           = 0x00001588,
			.debug_status_1           = 0x0000158C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_2,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 2 RDI2 */
		{
			.cfg                      = 0x00001600,
			.image_addr               = 0x00001604,
			.frame_incr               = 0x00001608,
			.image_cfg_0              = 0x0000160C,
			.image_cfg_1              = 0x00001610,
			.image_cfg_2              = 0x00001614,
			.packer_cfg               = 0x00001618,
			.frame_header_addr        = 0x00001620,
			.frame_header_incr        = 0x00001624,
			.frame_header_cfg         = 0x00001628,
			.line_done_cfg            = 0x0000162C,
			.irq_subsample_period     = 0x00001630,
			.irq_subsample_pattern    = 0x00001634,
			.mmu_prefetch_cfg         = 0x00001660,
			.mmu_prefetch_max_offset  = 0x00001664,
			.system_cache_cfg         = 0x00001668,
			.addr_cfg                 = 0x00001670,
			.addr_status_0            = 0x00001674,
			.addr_status_1            = 0x00001678,
			.addr_status_2            = 0x0000167C,
			.addr_status_3            = 0x00001680,
			.debug_status_cfg         = 0x00001684,
			.debug_status_0           = 0x00001688,
			.debug_status_1           = 0x0000168C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_3,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 3 RDI3 */
		{
			.cfg                      = 0x00001700,
			.image_addr               = 0x00001704,
			.frame_incr               = 0x00001708,
			.image_cfg_0              = 0x0000170C,
			.image_cfg_1              = 0x00001710,
			.image_cfg_2              = 0x00001714,
			.packer_cfg               = 0x00001718,
			.frame_header_addr        = 0x00001720,
			.frame_header_incr        = 0x00001724,
			.frame_header_cfg         = 0x00001728,
			.line_done_cfg            = 0x0000172C,
			.irq_subsample_period     = 0x00001730,
			.irq_subsample_pattern    = 0x00001734,
			.mmu_prefetch_cfg         = 0x00001760,
			.mmu_prefetch_max_offset  = 0x00001764,
			.system_cache_cfg         = 0x00001768,
			.addr_cfg                 = 0x00001770,
			.addr_status_0            = 0x00001774,
			.addr_status_1            = 0x00001778,
			.addr_status_2            = 0x0000177C,
			.addr_status_3            = 0x00001780,
			.debug_status_cfg         = 0x00001784,
			.debug_status_0           = 0x00001788,
			.debug_status_1           = 0x0000178C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_4,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 4 Gamma */
		{
			.cfg                      = 0x00001800,
			.image_addr               = 0x00001804,
			.frame_incr               = 0x00001808,
			.image_cfg_0              = 0x0000180C,
			.image_cfg_1              = 0x00001810,
			.image_cfg_2              = 0x00001814,
			.packer_cfg               = 0x00001818,
			.frame_header_addr        = 0x00001820,
			.frame_header_incr        = 0x00001824,
			.frame_header_cfg         = 0x00001828,
			.line_done_cfg            = 0x0000182C,
			.irq_subsample_period     = 0x00001830,
			.irq_subsample_pattern    = 0x00001834,
			.mmu_prefetch_cfg         = 0x00001860,
			.mmu_prefetch_max_offset  = 0x00001864,
			.system_cache_cfg         = 0x00001868,
			.addr_cfg                 = 0x00001870,
			.addr_status_0            = 0x00001874,
			.addr_status_1            = 0x00001878,
			.addr_status_2            = 0x0000187C,
			.addr_status_3            = 0x00001880,
			.debug_status_cfg         = 0x00001884,
			.debug_status_0           = 0x00001888,
			.debug_status_1           = 0x0000188C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = NULL,
		},
		/* BUS Client 5 Stats BE */
		{
			.cfg                      = 0x00001900,
			.image_addr               = 0x00001904,
			.frame_incr               = 0x00001908,
			.image_cfg_0              = 0x0000190C,
			.image_cfg_1              = 0x00001910,
			.image_cfg_2              = 0x00001914,
			.packer_cfg               = 0x00001918,
			.frame_header_addr        = 0x00001920,
			.frame_header_incr        = 0x00001924,
			.frame_header_cfg         = 0x00001928,
			.line_done_cfg            = 0x0000192C,
			.irq_subsample_period     = 0x00001930,
			.irq_subsample_pattern    = 0x00001934,
			.mmu_prefetch_cfg         = 0x00001960,
			.mmu_prefetch_max_offset  = 0x00001964,
			.system_cache_cfg         = 0x00001968,
			.addr_cfg                 = 0x00001970,
			.addr_status_0            = 0x00001974,
			.addr_status_1            = 0x00001978,
			.addr_status_2            = 0x0000197C,
			.addr_status_3            = 0x00001980,
			.debug_status_cfg         = 0x00001984,
			.debug_status_0           = 0x00001988,
			.debug_status_1           = 0x0000198C,
			.comp_group               = CAM_VFE_BUS_VER3_COMP_GRP_0,
			.ubwc_regs                = NULL,
		},
	},
	.num_out = 6,
	.vfe_out_hw_info = {
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI0,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_1,
			.num_wm        = 1,
			.line_based    = 1,
			.mid[0]        = 8,
			.wm_idx        = {
				0,
			},
			.name          = {
				"LITE_0",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI1,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_2,
			.num_wm        = 1,
			.line_based    = 1,
			.mid[0]        = 9,
			.wm_idx        = {
				1,
			},
			.name          = {
				"LITE_1",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI2,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_3,
			.num_wm        = 1,
			.line_based    = 1,
			.mid[0]        = 10,
			.wm_idx        = {
				2,
			},
			.name          = {
				"LITE_2",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_RDI3,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_4,
			.num_wm        = 1,
			.line_based    = 1,
			.mid[0]        = 11,
			.wm_idx        = {
				3,
			},
			.name          = {
				"LITE_3",
			},
		},
		{
			.vfe_out_type  =
				CAM_VFE_BUS_VER3_VFE_OUT_PREPROCESS_RAW,
			.max_width     = 1920,
			.max_height    = 1080,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.num_wm        = 1,
			.mid[0]        = 12,
			.wm_idx        = {
				4,
			},
			.name          = {
				"PREPROCESS_RAW",
			},
		},
		{
			.vfe_out_type  = CAM_VFE_BUS_VER3_VFE_OUT_STATS_BG,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_VFE_BUS_VER3_SRC_GRP_0,
			.num_wm        = 1,
			.mid[0]        = 13,
			.wm_idx        = {
				5,
			},
			.name          = {
				"STATS_BG",
			},
		},
	},
	.num_comp_grp    = 5,
	.support_consumed_addr = true,
	.comp_done_shift = 0,
	.top_irq_shift   = 0,
	.max_out_res = CAM_ISP_IFE_OUT_RES_BASE + 34,
};

static struct cam_vfe_irq_hw_info vfe_lite78x_irq_hw_info = {
	.reset_mask    = 0,
	.supported_irq = CAM_VFE_HW_IRQ_CAP_LITE_EXT_CSID,
	.top_irq_reg   = &vfe_lite78x_top_irq_reg_info,
};

static struct cam_vfe_hw_info cam_vfe_lite78x_hw_info = {
	.irq_hw_info                   = &vfe_lite78x_irq_hw_info,

	.bus_version                   = CAM_VFE_BUS_VER_3_0,
	.bus_hw_info                   = &vfe_lite78x_bus_hw_info,

	.top_version                   = CAM_VFE_TOP_VER_4_0,
	.top_hw_info                   = &vfe_lite78x_top_hw_info,
};

#endif /* _CAM_VFE_LITE78X_H_ */
