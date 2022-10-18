/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE680_H_
#define _CAM_SFE680_H_
#include "cam_sfe_core.h"
#include "cam_sfe_bus.h"
#include "cam_sfe_bus_rd.h"
#include "cam_sfe_bus_wr.h"


static struct cam_sfe_top_module_desc sfe_680_mod_desc[] = {
	{
		.id = 0,
		.desc = "CRC_ZSL",
	},
	{
		.id = 1,
		.desc = "COMP",
	},
	{
		.id = 2,
		.desc = "CRC_PREV",
	},
	{
		.id = 3,
		.desc = "HDRC",
	},
	{
		.id = 4,
		.desc = "DECOMP",
	},
	{
		.id = 5,
		.desc = "BPC_PDPC",
	},
	{
		.id = 6,
		.desc = "BHIST_CH0",
	},
	{
		.id = 7,
		.desc = "BG_CH0",
	},
	{
		.id = 8,
		.desc = "LSC_CH0",
	},
	{
		.id = 9,
		.desc = "CRC_CH0",
	},
	{
		.id = 10,
		.desc = "CCIF_2x2_2x1",
	},
	{
		.id = 11,
		.desc = "GAIN_CH0",
	},
	{
		.id = 12,
		.desc = "BHIST_CH1",
	},
	{
		.id = 13,
		.desc = "BG_CH1",
	},
	{
		.id = 14,
		.desc = "LSC_CH1",
	},
	{
		.id = 15,
		.desc = "CRC_CH1",
	},
	{
		.id = 16,
		.desc = "GAIN_CH1",
	},
	{
		.id = 17,
		.desc = "BHIST_CH2",
	},
	{
		.id = 18,
		.desc = "BG_CH2",
	},
	{
		.id = 19,
		.desc = "LSC_CH2",
	},
	{
		.id = 20,
		.desc = "CRC_CH2",
	},
	{
		.id = 21,
		.desc = "GAIN_CH2",
	},
	{
		.id = 22,
		.desc = "LCR",
	},
	{
		.id = 23,
		.desc = "QCFA_DEMUX",
	},
};

static struct cam_sfe_wr_client_desc sfe_680_wr_client_desc[] = {
	{
		.wm_id = 0,
		.desc = "REMOSAIC",
	},
	{
		.wm_id = 1,
		.desc = "LCR",
	},
	{
		.wm_id = 2,
		.desc = "STATS_BE0",
	},
	{
		.wm_id = 3,
		.desc = "STATS_BHIST0",
	},
	{
		.wm_id = 4,
		.desc = "STATS_BE1",
	},
	{
		.wm_id = 5,
		.desc = "STATS_BHIST1",
	},
	{
		.wm_id = 6,
		.desc = "STATS_BE2",
	},
	{
		.wm_id = 7,
		.desc = "STATS_BHIST2",
	},
	{
		.wm_id = 8,
		.desc = "RDI_0",
	},
	{
		.wm_id = 9,
		.desc = "RDI_1",
	},
	{
		.wm_id = 10,
		.desc = "RDI_2",
	},
	{
		.wm_id = 11,
		.desc = "RDI_3",
	},
	{
		.wm_id = 12,
		.desc = "RDI_4",
	},
};

static struct cam_sfe_mode sfe_680_mode[] = {
	{
		.value = 0x1,
		.desc = "FS Mode",
	},
	{
		.value = 0x3,
		.desc = "Offline Mode",
	},
	{
		.value = 0x4,
		.desc = "sHDR mode",
	},
};

static struct cam_sfe_top_err_irq_desc sfe_680_top_irq_err_desc[] = {
	{
		.bitmask = BIT(14),
		.err_name = "PP_VIOLATION",
		.desc = "CCIF protocol violation within any of the modules in pixel pipeline",
	},
	{
		.bitmask = BIT(15),
		.err_name = "DIAG_VIOLATION",
		.desc = "HBI is less than the minimum required HBI",
	},
	{
		.bitmask = BIT(16),
		.err_name = "LINE_SMOOTH_VIOLATION",
		.desc = "Line Smoothner IRQ fire",
	},
};

static struct cam_sfe_top_debug_info sfe680_clc_dbg_module_info[CAM_SFE_TOP_DBG_REG_MAX][8] = {
	SFE_DBG_INFO_ARRAY_4bit(
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved",
		"test_bus_reserved"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"zsl_throttle",
		"crc_zsl",
		"comp_zsl",
		"crc_prev",
		"hdrc_ch2",
		"hdrc_ch1",
		"hdrc_ch0",
		"stats_bhist_ch0"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"stats_bg_ch0",
		"lsc_ch0",
		"crc_ch0",
		"ccif_2x2_to_2x1",
		"decomp",
		"msb_align_ch0",
		"bpc_pdpc",
		"ch0_gain"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"bhist_ch1",
		"stats_bg_ch1",
		"lsc_ch1",
		"crc_ch1",
		"msb_align_ch1",
		"ch1_gain",
		"bhist_ch2",
		"stats_bg_ch2"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"lsc_ch2",
		"crc_ch2",
		"msb_align_ch2",
		"ch2_gain",
		"lcr_throttle",
		"lcr",
		"demux_fetch2",
		"demux_fetch1"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"demux_fetch0",
		"csid_ccif",
		"RDI4",
		"RDI3",
		"RDI2",
		"RDI1",
		"RDI0",
		"bhist2_bus_wr"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"bg2_bus_wr",
		"bhist1_bus_wr",
		"bg1_bus_wr",
		"bhist0_bus_wr",
		"bg0_bus_wr",
		"lcr_bus_wr",
		"zsl_bus_wr",
		"sfe_op_throttle"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"line_smooth",
		"pp",
		"bus_conv_ch2",
		"bus_conv_ch1",
		"bus_conv_ch0",
		"fe_ch2",
		"fe_ch1",
		"fe_ch0"
	),
	SFE_DBG_INFO_ARRAY_4bit(
		"rdi4",
		"rdi3",
		"rdi2",
		"rdi1",
		"rdi0",
		"pixel",
		"reserved",
		"reserved"
	),
};

static struct cam_sfe_top_common_reg_offset  sfe680_top_commong_reg  = {
	.hw_version                    = 0x00000000,
	.hw_capability                 = 0x00000004,
	.stats_feature                 = 0x00000008,
	.core_cgc_ctrl                 = 0x00000010,
	.ahb_clk_ovd                   = 0x00000014,
	.core_cfg                      = 0x00000018,
	.ipp_violation_status          = 0x00000030,
	.diag_config                   = 0x00000034,
	.diag_sensor_status_0          = 0x00000038,
	.diag_sensor_status_1          = 0x0000003C,
	.diag_sensor_frame_cnt_status0 = 0x00000040,
	.diag_sensor_frame_cnt_status1 = 0x00000044,
	.stats_ch2_throttle_cfg        = 0x000000B0,
	.stats_ch1_throttle_cfg        = 0x000000B4,
	.stats_ch0_throttle_cfg        = 0x000000B8,
	.lcr_throttle_cfg              = 0x000000BC,
	.hdr_throttle_cfg              = 0x000000C0,
	.sfe_op_throttle_cfg           = 0x000000C4,
	.bus_overflow_status           = 0x00000868,
	.top_debug_cfg                 = 0x0000007C,
	.lcr_supported                 = true,
	.ir_supported                  = false,
	.qcfa_only                     = true,
	.num_sfe_mode                  = ARRAY_SIZE(sfe_680_mode),
	.sfe_mode                      = sfe_680_mode,
	.ipp_violation_mask            = 0x4000,
	.num_debug_registers           = 12,
	.top_cc_test_bus_supported     = false,
	.top_debug = {
		0x0000004C,
		0x00000050,
		0x00000054,
		0x00000058,
		0x0000005C,
		0x00000060,
		0x00000064,
		0x00000068,
		0x0000006C,
		0x00000070,
		0x00000074,
		0x00000078,
	},
};

static struct cam_sfe_modules_common_reg_offset sfe680_modules_common_reg = {
	.demux_module_cfg              = 0x00003060,
	.demux_xcfa_cfg                = 0x00003064,
	.demux_hdr_cfg                 = 0x00003074,
	.demux_lcr_sel                 = 0x00003078,
	.hdrc_remo_mod_cfg             = 0x00005860,
	.hdrc_remo_xcfa_bin_cfg        = 0x00005A78,
	.xcfa_hdrc_remo_out_mux_cfg    = 0x00005A74,
};

static struct cam_sfe_top_common_reg_data sfe_680_top_common_reg_data = {
	.error_irq_mask                = 0x1C000,
	.enable_diagnostic_hw          = 0x1,
	.top_debug_cfg_en              = 0x3,
	.sensor_sel_shift              = 0x1,
};

static struct cam_sfe_path_common_reg_data sfe_680_pix_reg_data = {
	.sof_irq_mask                  = 0x4,
	.eof_irq_mask                  = 0x8,
	.subscribe_irq_mask            = 0xC,
};

static struct cam_sfe_path_common_reg_data sfe_680_rdi0_reg_data = {
	.sof_irq_mask                  = 0x10,
	.eof_irq_mask                  = 0x20,
	.subscribe_irq_mask            = 0x30,
};

static struct cam_sfe_path_common_reg_data sfe_680_rdi1_reg_data = {
	.sof_irq_mask                  = 0x40,
	.eof_irq_mask                  = 0x80,
	.subscribe_irq_mask            = 0xC0,
};

static struct cam_sfe_path_common_reg_data sfe_680_rdi2_reg_data = {
	.sof_irq_mask                  = 0x100,
	.eof_irq_mask                  = 0x200,
	.subscribe_irq_mask            = 0x300,
};

static struct cam_sfe_path_common_reg_data sfe_680_rdi3_reg_data = {
	.sof_irq_mask                  = 0x400,
	.eof_irq_mask                  = 0x800,
	.subscribe_irq_mask            = 0xC00,
};

static struct cam_sfe_path_common_reg_data sfe_680_rdi4_reg_data = {
	.sof_irq_mask                  = 0x1000,
	.eof_irq_mask                  = 0x2000,
	.subscribe_irq_mask            = 0x3000,
};

static struct cam_sfe_top_hw_info sfe680_top_hw_info = {
	.common_reg = &sfe680_top_commong_reg,
	.modules_hw_info = &sfe680_modules_common_reg,
	.common_reg_data = &sfe_680_top_common_reg_data,
	.ipp_module_desc =  sfe_680_mod_desc,
	.wr_client_desc  =  sfe_680_wr_client_desc,
	.pix_reg_data    = &sfe_680_pix_reg_data,
	.rdi_reg_data[0] = &sfe_680_rdi0_reg_data,
	.rdi_reg_data[1] = &sfe_680_rdi1_reg_data,
	.rdi_reg_data[2] = &sfe_680_rdi2_reg_data,
	.rdi_reg_data[3] = &sfe_680_rdi3_reg_data,
	.rdi_reg_data[4] = &sfe_680_rdi4_reg_data,
	.num_inputs = 6,
	.input_type = {
		CAM_SFE_PIX_VER_1_0,
		CAM_SFE_RDI_VER_1_0,
		CAM_SFE_RDI_VER_1_0,
		CAM_SFE_RDI_VER_1_0,
		CAM_SFE_RDI_VER_1_0,
		CAM_SFE_RDI_VER_1_0,
	},
	.num_top_errors  = ARRAY_SIZE(sfe_680_top_irq_err_desc),
	.top_err_desc    = sfe_680_top_irq_err_desc,
	.num_clc_module  = 9,
	.clc_dbg_mod_info = &sfe680_clc_dbg_module_info,
};

static struct cam_irq_register_set sfe680_bus_rd_irq_reg[1] = {
	{
		.mask_reg_offset   = 0x00000404,
		.clear_reg_offset  = 0x00000408,
		.status_reg_offset = 0x00000410,
	},
};

static struct cam_sfe_bus_rd_hw_info sfe680_bus_rd_hw_info = {
	.common_reg = {
		.hw_version                   = 0x00000400,
		.misr_reset                   = 0x0000041C,
		.pwr_iso_cfg                  = 0x00000424,
		.input_if_cmd                 = 0x00000414,
		.test_bus_ctrl                = 0x0000042C,
		.security_cfg                 = 0x00000420,
		.cons_violation_status        = 0x00000434,
		.irq_reg_info = {
			.num_registers     = 1,
			.irq_reg_set          = sfe680_bus_rd_irq_reg,
			.global_clear_offset  = 0x0000040C,
			.global_clear_bitmask = 0x00000001,
			.clear_all_bitmask = 0xFFFFFFFF,
		},
	},
	.num_client = 3,
	.bus_client_reg = {
		/* BUS Client 0 */
		{
			.cfg                      = 0x00000450,
			.image_addr               = 0x00000458,
			.buf_width                = 0x0000045C,
			.buf_height               = 0x00000460,
			.stride                   = 0x00000464,
			.unpacker_cfg             = 0x00000468,
			.latency_buf_allocation   = 0x0000047C,
			.system_cache_cfg         = 0x0000049C,
		},
		/* BUS Client 1 */
		{
			.cfg                      = 0x000004F0,
			.image_addr               = 0x000004F8,
			.buf_width                = 0x000004FC,
			.buf_height               = 0x00000500,
			.stride                   = 0x00000504,
			.unpacker_cfg             = 0x00000508,
			.latency_buf_allocation   = 0x0000051C,
			.system_cache_cfg         = 0x0000053C,
		},
		/* BUS Client 2 */
		{
			.cfg                      = 0x00000590,
			.image_addr               = 0x00000598,
			.buf_width                = 0x0000059C,
			.buf_height               = 0x000005A0,
			.stride                   = 0x000005A4,
			.unpacker_cfg             = 0x000005A8,
			.latency_buf_allocation   = 0x000005BC,
			.system_cache_cfg         = 0x000005DC,
		},
	},
	.num_bus_rd_resc = 3,
	.sfe_bus_rd_info = {
		{
			.sfe_bus_rd_type = CAM_SFE_BUS_RD_RDI0,
			.mid[0] = 0,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.sfe_bus_rd_type = CAM_SFE_BUS_RD_RDI1,
			.mid[0] = 1,
			.max_width     = -1,
			.max_height    = -1,
		},
		{
			.sfe_bus_rd_type = CAM_SFE_BUS_RD_RDI2,
			.mid[0] = 2,
			.max_width     = -1,
			.max_height    = -1,
		},
	},
	.top_irq_shift = 0x1,
	/*
	 * Refer to CAMNOC HPG for the updated value for a given target
	 * 48 OTs, 2 SFEs each with 3 RDs, 48 / 6 = 8
	 * We can allocate 256 * 8 = 2048 bytes. 256 bytes being
	 * the minimum
	 */
	.latency_buf_allocation = 2048,
};

static struct cam_irq_register_set sfe680_bus_wr_irq_reg[1] = {
	{
		.mask_reg_offset   = 0x00000818,
		.clear_reg_offset  = 0x00000820,
		.status_reg_offset = 0x00000828,
	},
};

static struct cam_sfe_bus_wr_hw_info sfe680_bus_wr_hw_info = {
	.common_reg = {
		.hw_version                       = 0x00000800,
		.cgc_ovd                          = 0x00000808,
		.if_frameheader_cfg               = {
			0x00000834,
			0x00000838,
			0x0000083C,
			0x00000840,
			0x00000844,
			0x00000848,
		},
		.pwr_iso_cfg                      = 0x0000085C,
		.overflow_status_clear            = 0x00000860,
		.ccif_violation_status            = 0x00000864,
		.overflow_status                  = 0x00000868,
		.image_size_violation_status      = 0x00000870,
		.debug_status_top_cfg             = 0x000008D4,
		.debug_status_top                 = 0x000008D8,
		.test_bus_ctrl                    = 0x000008DC,
		.top_irq_mask_0                   = 0x00000020,
		.irq_reg_info = {
			.num_registers     = 1,
			.irq_reg_set          = sfe680_bus_wr_irq_reg,
			.global_clear_offset  = 0x00000830,
			.global_clear_bitmask = 0x00000001,
			.clear_all_bitmask = 0xFFFFFFFF,
		},
	},
	.num_client = 13,
	.bus_client_reg = {
		/* BUS Client 0 REMOSAIC */
		{
			.cfg                      = 0x00000A00,
			.image_addr               = 0x00000A04,
			.frame_incr               = 0x00000A08,
			.image_cfg_0              = 0x00000A0C,
			.image_cfg_1              = 0x00000A10,
			.image_cfg_2              = 0x00000A14,
			.packer_cfg               = 0x00000A18,
			.frame_header_addr        = 0x00000A20,
			.frame_header_incr        = 0x00000A24,
			.frame_header_cfg         = 0x00000A28,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00000A30,
			.irq_subsample_pattern    = 0x00000A34,
			.framedrop_period         = 0x00000A38,
			.framedrop_pattern        = 0x00000A3C,
			.system_cache_cfg         = 0x00000A68,
			.addr_status_0            = 0x00000A70,
			.addr_status_1            = 0x00000A74,
			.addr_status_2            = 0x00000A78,
			.addr_status_3            = 0x00000A7C,
			.debug_status_cfg         = 0x00000A80,
			.debug_status_0           = 0x00000A84,
			.debug_status_1           = 0x00000A88,
			.mmu_prefetch_cfg         = 0x00000A60,
			.mmu_prefetch_max_offset  = 0x00000A64,
			.bw_limiter_addr          = 0x00000A1C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_0,
		},
		/* BUS Client 1 LCR */
		{
			.cfg                      = 0x00000B00,
			.image_addr               = 0x00000B04,
			.frame_incr               = 0x00000B08,
			.image_cfg_0              = 0x00000B0C,
			.image_cfg_1              = 0x00000B10,
			.image_cfg_2              = 0x00000B14,
			.packer_cfg               = 0x00000B18,
			.frame_header_addr        = 0x00000B20,
			.frame_header_incr        = 0x00000B24,
			.frame_header_cfg         = 0x00000B28,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00000B30,
			.irq_subsample_pattern    = 0x00000B34,
			.framedrop_period         = 0x00000B38,
			.framedrop_pattern        = 0x00000B3C,
			.system_cache_cfg         = 0x00000B68,
			.addr_status_0            = 0x00000B70,
			.addr_status_1            = 0x00000B74,
			.addr_status_2            = 0x00000B78,
			.addr_status_3            = 0x00000B7C,
			.debug_status_cfg         = 0x00000B80,
			.debug_status_0           = 0x00000B84,
			.debug_status_1           = 0x00000B88,
			.mmu_prefetch_cfg         = 0x00000B60,
			.mmu_prefetch_max_offset  = 0x00000B64,
			.bw_limiter_addr          = 0x00000B1C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_1,
		},
		/* BUS Client 2 STATS_BE_0 */
		{
			.cfg                      = 0x00000C00,
			.image_addr               = 0x00000C04,
			.frame_incr               = 0x00000C08,
			.image_cfg_0              = 0x00000C0C,
			.image_cfg_1              = 0x00000C10,
			.image_cfg_2              = 0x00000C14,
			.packer_cfg               = 0x00000C18,
			.frame_header_addr        = 0x00000C20,
			.frame_header_incr        = 0x00000C24,
			.frame_header_cfg         = 0x00000C28,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00000C30,
			.irq_subsample_pattern    = 0x00000C34,
			.framedrop_period         = 0x00000C38,
			.framedrop_pattern        = 0x00000C3C,
			.system_cache_cfg         = 0x00000C68,
			.addr_status_0            = 0x00000C70,
			.addr_status_1            = 0x00000C74,
			.addr_status_2            = 0x00000C78,
			.addr_status_3            = 0x00000C7C,
			.debug_status_cfg         = 0x00000C80,
			.debug_status_0           = 0x00000C84,
			.debug_status_1           = 0x00000C88,
			.mmu_prefetch_cfg         = 0x00000C60,
			.mmu_prefetch_max_offset  = 0x00000C64,
			.bw_limiter_addr          = 0x00000C1C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_2,
		},
		/* BUS Client 3 STATS_BHIST_0 */
		{
			.cfg                      = 0x00000D00,
			.image_addr               = 0x00000D04,
			.frame_incr               = 0x00000D08,
			.image_cfg_0              = 0x00000D0C,
			.image_cfg_1              = 0x00000D10,
			.image_cfg_2              = 0x00000D14,
			.packer_cfg               = 0x00000D18,
			.frame_header_addr        = 0x00000D20,
			.frame_header_incr        = 0x00000D24,
			.frame_header_cfg         = 0x00000D28,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00000D30,
			.irq_subsample_pattern    = 0x00000D34,
			.framedrop_period         = 0x00000D38,
			.framedrop_pattern        = 0x00000D3C,
			.system_cache_cfg         = 0x00000D68,
			.addr_status_0            = 0x00000D70,
			.addr_status_1            = 0x00000D74,
			.addr_status_2            = 0x00000D78,
			.addr_status_3            = 0x00000D7C,
			.debug_status_cfg         = 0x00000D80,
			.debug_status_0           = 0x00000D84,
			.debug_status_1           = 0x00000D88,
			.mmu_prefetch_cfg         = 0x00000D60,
			.mmu_prefetch_max_offset  = 0x00000D64,
			.bw_limiter_addr          = 0x00000D1C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_2,
		},
		/* BUS Client 4 STATS_BE_1 */
		{
			.cfg                      = 0x00000E00,
			.image_addr               = 0x00000E04,
			.frame_incr               = 0x00000E08,
			.image_cfg_0              = 0x00000E0C,
			.image_cfg_1              = 0x00000E10,
			.image_cfg_2              = 0x00000E14,
			.packer_cfg               = 0x00000E18,
			.frame_header_addr        = 0x00000E20,
			.frame_header_incr        = 0x00000E24,
			.frame_header_cfg         = 0x00000E28,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00000E30,
			.irq_subsample_pattern    = 0x00000E34,
			.framedrop_period         = 0x00000E38,
			.framedrop_pattern        = 0x00000E3C,
			.system_cache_cfg         = 0x00000E68,
			.addr_status_0            = 0x00000E70,
			.addr_status_1            = 0x00000E74,
			.addr_status_2            = 0x00000E78,
			.addr_status_3            = 0x00000E7C,
			.debug_status_cfg         = 0x00000E80,
			.debug_status_0           = 0x00000E84,
			.debug_status_1           = 0x00000E88,
			.mmu_prefetch_cfg         = 0x00000E60,
			.mmu_prefetch_max_offset  = 0x00000E64,
			.bw_limiter_addr          = 0x00000E1C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_3,
		},
		/* BUS Client 5 STATS_BHIST_1 */
		{
			.cfg                      = 0x00000F00,
			.image_addr               = 0x00000F04,
			.frame_incr               = 0x00000F08,
			.image_cfg_0              = 0x00000F0C,
			.image_cfg_1              = 0x00000F10,
			.image_cfg_2              = 0x00000F14,
			.packer_cfg               = 0x00000F18,
			.frame_header_addr        = 0x00000F20,
			.frame_header_incr        = 0x00000F24,
			.frame_header_cfg         = 0x00000F28,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00000F30,
			.irq_subsample_pattern    = 0x00000F34,
			.framedrop_period         = 0x00000F38,
			.framedrop_pattern        = 0x00000F3C,
			.system_cache_cfg         = 0x00000F68,
			.addr_status_0            = 0x00000F70,
			.addr_status_1            = 0x00000F74,
			.addr_status_2            = 0x00000F78,
			.addr_status_3            = 0x00000F7C,
			.debug_status_cfg         = 0x00000F80,
			.debug_status_0           = 0x00000F84,
			.debug_status_1           = 0x00000F88,
			.mmu_prefetch_cfg         = 0x00000F60,
			.mmu_prefetch_max_offset  = 0x00000F64,
			.bw_limiter_addr          = 0x00000F1C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_3,
		},
		/* BUS Client 6 STATS_BE_2 */
		{
			.cfg                      = 0x00001000,
			.image_addr               = 0x00001004,
			.frame_incr               = 0x00001008,
			.image_cfg_0              = 0x0000100C,
			.image_cfg_1              = 0x00001010,
			.image_cfg_2              = 0x00001014,
			.packer_cfg               = 0x00001018,
			.frame_header_addr        = 0x00001020,
			.frame_header_incr        = 0x00001024,
			.frame_header_cfg         = 0x00001028,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00001030,
			.irq_subsample_pattern    = 0x00001034,
			.framedrop_period         = 0x00001038,
			.framedrop_pattern        = 0x0000103C,
			.system_cache_cfg         = 0x00001068,
			.addr_status_0            = 0x00001070,
			.addr_status_1            = 0x00001074,
			.addr_status_2            = 0x00001078,
			.addr_status_3            = 0x0000107C,
			.debug_status_cfg         = 0x00001080,
			.debug_status_0           = 0x00001084,
			.debug_status_1           = 0x00001088,
			.mmu_prefetch_cfg         = 0x00001060,
			.mmu_prefetch_max_offset  = 0x00001064,
			.bw_limiter_addr          = 0x0000101C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_4,
		},
		/* BUS Client 7 STATS_BHIST_2 */
		{
			.cfg                      = 0x00001100,
			.image_addr               = 0x00001104,
			.frame_incr               = 0x00001108,
			.image_cfg_0              = 0x0000110C,
			.image_cfg_1              = 0x00001110,
			.image_cfg_2              = 0x00001114,
			.packer_cfg               = 0x00001118,
			.frame_header_addr        = 0x00001120,
			.frame_header_incr        = 0x00001124,
			.frame_header_cfg         = 0x00001128,
			.line_done_cfg            = 0,
			.irq_subsample_period     = 0x00001130,
			.irq_subsample_pattern    = 0x00001134,
			.framedrop_period         = 0x00001138,
			.framedrop_pattern        = 0x0000113C,
			.system_cache_cfg         = 0x00001168,
			.addr_status_0            = 0x00001170,
			.addr_status_1            = 0x00001174,
			.addr_status_2            = 0x00001178,
			.addr_status_3            = 0x0000117C,
			.debug_status_cfg         = 0x00001180,
			.debug_status_0           = 0x00001184,
			.debug_status_1           = 0x00001188,
			.mmu_prefetch_cfg         = 0x00001160,
			.mmu_prefetch_max_offset  = 0x00001164,
			.bw_limiter_addr          = 0x0000111C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_4,
		},
		/* BUS Client 8 RDI0 */
		{
			.cfg                      = 0x00001200,
			.image_addr               = 0x00001204,
			.frame_incr               = 0x00001208,
			.image_cfg_0              = 0x0000120C,
			.image_cfg_1              = 0x00001210,
			.image_cfg_2              = 0x00001214,
			.packer_cfg               = 0x00001218,
			.frame_header_addr        = 0x00001220,
			.frame_header_incr        = 0x00001224,
			.frame_header_cfg         = 0x00001228,
			.line_done_cfg            = 0x0000122C,
			.irq_subsample_period     = 0x00001230,
			.irq_subsample_pattern    = 0x00001234,
			.framedrop_period         = 0x00001238,
			.framedrop_pattern        = 0x0000123C,
			.system_cache_cfg         = 0x00001268,
			.addr_status_0            = 0x00001270,
			.addr_status_1            = 0x00001274,
			.addr_status_2            = 0x00001278,
			.addr_status_3            = 0x0000127C,
			.debug_status_cfg         = 0x00001280,
			.debug_status_0           = 0x00001284,
			.debug_status_1           = 0x00001288,
			.mmu_prefetch_cfg         = 0x00001260,
			.mmu_prefetch_max_offset  = 0x00001264,
			.bw_limiter_addr          = 0x0000121C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_5,
		},
		/* BUS Client 9 RDI1 */
		{
			.cfg                      = 0x00001300,
			.image_addr               = 0x00001304,
			.frame_incr               = 0x00001308,
			.image_cfg_0              = 0x0000130C,
			.image_cfg_1              = 0x00001310,
			.image_cfg_2              = 0x00001314,
			.packer_cfg               = 0x00001318,
			.frame_header_addr        = 0x00001320,
			.frame_header_incr        = 0x00001324,
			.frame_header_cfg         = 0x00001328,
			.line_done_cfg            = 0x0000132C,
			.irq_subsample_period     = 0x00001330,
			.irq_subsample_pattern    = 0x00001334,
			.framedrop_period         = 0x00001338,
			.framedrop_pattern        = 0x0000133C,
			.system_cache_cfg         = 0x00001368,
			.addr_status_0            = 0x00001370,
			.addr_status_1            = 0x00001374,
			.addr_status_2            = 0x00001378,
			.addr_status_3            = 0x0000137C,
			.debug_status_cfg         = 0x00001380,
			.debug_status_0           = 0x00001384,
			.debug_status_1           = 0x00001388,
			.mmu_prefetch_cfg         = 0x00001360,
			.mmu_prefetch_max_offset  = 0x00001364,
			.bw_limiter_addr          = 0x0000131C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_6,
		},
		/* BUS Client 10 RDI2 */
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
			.framedrop_period         = 0x00001438,
			.framedrop_pattern        = 0x0000143C,
			.system_cache_cfg         = 0x00001468,
			.addr_status_0            = 0x00001470,
			.addr_status_1            = 0x00001474,
			.addr_status_2            = 0x00001478,
			.addr_status_3            = 0x0000147C,
			.debug_status_cfg         = 0x00001480,
			.debug_status_0           = 0x00001484,
			.debug_status_1           = 0x00001488,
			.mmu_prefetch_cfg         = 0x00001460,
			.mmu_prefetch_max_offset  = 0x00001464,
			.bw_limiter_addr          = 0x0000141C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_7,
		},
		/* BUS Client 11 RDI3 */
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
			.framedrop_period         = 0x00001538,
			.framedrop_pattern        = 0x0000153C,
			.system_cache_cfg         = 0x00001568,
			.addr_status_0            = 0x00001570,
			.addr_status_1            = 0x00001574,
			.addr_status_2            = 0x00001578,
			.addr_status_3            = 0x0000157C,
			.debug_status_cfg         = 0x00001580,
			.debug_status_0           = 0x00001584,
			.debug_status_1           = 0x00001588,
			.mmu_prefetch_cfg         = 0x00001560,
			.mmu_prefetch_max_offset  = 0x00001564,
			.bw_limiter_addr          = 0x0000151C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_8,
		},
		/* BUS Client 12 RDI4 */
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
			.framedrop_period         = 0x00001638,
			.framedrop_pattern        = 0x0000163C,
			.system_cache_cfg         = 0x00001668,
			.addr_status_0            = 0x00001670,
			.addr_status_1            = 0x00001674,
			.addr_status_2            = 0x00001678,
			.addr_status_3            = 0x0000167C,
			.debug_status_cfg         = 0x00001680,
			.debug_status_0           = 0x00001684,
			.debug_status_1           = 0x00001688,
			.mmu_prefetch_cfg         = 0x00001660,
			.mmu_prefetch_max_offset  = 0x00001664,
			.bw_limiter_addr          = 0x0000161C,
			.comp_group               = CAM_SFE_BUS_WR_COMP_GRP_9,
		},
	},
	.num_out = 13,
	.sfe_out_hw_info = {
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_RDI0,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_1,
			.mid[0]        = 25,
			.num_wm        = 1,
			.wm_idx        = 8,
			.en_line_done  = 1,
			.name          = "RDI_0",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_RDI1,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_2,
			.mid[0]        = 26,
			.num_wm        = 1,
			.wm_idx        = 9,
			.en_line_done  = 1,
			.name          = "RDI_1",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_RDI2,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_3,
			.mid[0]        = 27,
			.num_wm        = 1,
			.wm_idx        = 10,
			.en_line_done  = 1,
			.name          = "RDI_2",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_RDI3,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_4,
			.mid[0]        = 28,
			.num_wm        = 1,
			.wm_idx        = 11,
			.name          = "RDI_3",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_RDI4,
			.max_width     = -1,
			.max_height    = -1,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_4,
			.mid[0]        = 29,
			.num_wm        = 1,
			.wm_idx        = 12,
			.name          = "RDI_4",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_RAW_DUMP,
			.max_width     = 9312,
			.max_height    = 6992,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 16,
			.mid[1]        = 17,
			.num_wm        = 1,
			.wm_idx        = 0,
			.name          = "REMOSIAC",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_LCR,
			.max_width     = 9312,
			.max_height    = 2048,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 18,
			.num_wm        = 1,
			.wm_idx        = 1,
			.name          = "LCR",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_BE_0,
			.max_width     = 7296,
			.max_height    = 5472,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 19,
			.num_wm        = 1,
			.wm_idx        = 2,
			.name          = "STATS_BE_0",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_BHIST_0,
			.max_width     = 7296,
			.max_height    = 5472,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 20,
			.num_wm        = 1,
			.wm_idx        = 3,
			.name          = "STATS_BHIST_0",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_BE_1,
			.max_width     = 7296,
			.max_height    = 5472,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 21,
			.num_wm        = 1,
			.wm_idx        = 4,
			.name          = "STATS_BE_1",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_BHIST_1,
			.max_width     = 7296,
			.max_height    = 5472,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 22,
			.num_wm        = 1,
			.wm_idx        = 5,
			.name          = "STATS_BHIST_1",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_BE_2,
			.max_width     = 7296,
			.max_height    = 5472,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 23,
			.num_wm        = 1,
			.wm_idx        = 6,
			.name          = "STATS_BE_2",
		},
		{
			.sfe_out_type  = CAM_SFE_BUS_SFE_OUT_BHIST_2,
			.max_width     = 7296,
			.max_height    = 5472,
			.source_group  = CAM_SFE_BUS_WR_SRC_GRP_0,
			.mid[0]        = 24,
			.num_wm        = 1,
			.wm_idx        = 7,
			.name          = "STATS_BHIST_2",
		},
	},
	.num_cons_err = 29,
	.constraint_error_list = {
		{
			.bitmask = BIT(0),
			.error_description = "PPC 1x1 input not supported"
		},
		{
			.bitmask = BIT(1),
			.error_description = "PPC 1x2 input not supported"
		},
		{
			.bitmask = BIT(2),
			.error_description = "PPC 2x1 input not supported"
		},
		{
			.bitmask = BIT(3),
			.error_description = "PPC 2x2 input not supported"
		},
		{
			.bitmask = BIT(4),
			.error_description = "Pack 8 BPP format not supported"
		},
		{
			.bitmask = BIT(5),
			.error_description = "Pack 16 format not supported"
		},
		{
			.bitmask = BIT(6),
			.error_description = "Pack 32 BPP format not supported"
		},
		{
			.bitmask = BIT(7),
			.error_description = "Pack 64 BPP format not supported"
		},
		{
			.bitmask = BIT(8),
			.error_description = "Pack MIPI 20 format not supported"
		},
		{
			.bitmask = BIT(9),
			.error_description = "Pack MIPI 14 format not supported"
		},
		{
			.bitmask = BIT(10),
			.error_description = "Pack MIPI 12 format not supported"
		},
		{
			.bitmask = BIT(11),
			.error_description = "Pack MIPI 10 format not supported"
		},
		{
			.bitmask = BIT(12),
			.error_description = "Pack 128 BPP format not supported"
		},
		{
			.bitmask = BIT(13),
			.error_description = "UBWC NV12 format not supported"
		},
		{
			.bitmask = BIT(14),
			.error_description = "UBWC NV12 4R format not supported"
		},
		{
			.bitmask = BIT(15),
			.error_description = "UBWC TP10 format not supported"
		},
		{
			.bitmask = BIT(16),
			.error_description = "Frame based Mode not supported"
		},
		{
			.bitmask = BIT(17),
			.error_description = "Index based Mode not supported"
		},
		{
			.bitmask = BIT(18),
			.error_description = "FIFO image addr unalign"
		},
		{
			.bitmask = BIT(19),
			.error_description = "FIFO ubwc addr unalign"
		},
		{
			.bitmask = BIT(20),
			.error_description = "FIFO frmheader addr unalign"
		},
		{
			.bitmask = BIT(21),
			.error_description = "Image address unalign"
		},
		{
			.bitmask = BIT(22),
			.error_description = "UBWC address unalign"
		},
		{
			.bitmask = BIT(23),
			.error_description = "Frame Header address unalign"
		},
		{
			.bitmask = BIT(24),
			.error_description = "Stride unalign"
		},
		{
			.bitmask = BIT(25),
			.error_description = "X Initialization unalign"
		},
		{
			.bitmask = BIT(26),
			.error_description = "Image Width unalign"
		},
		{
			.bitmask = BIT(27),
			.error_description = "Image Height unalign"
		},
		{
			.bitmask = BIT(28),
			.error_description = "Meta Stride unalign"
		},
	},
	.num_comp_grp         = 10,
	.comp_done_shift      = 17,
	.line_done_cfg        = 0x11,
	.top_irq_shift        = 0x0,
	.pack_align_shift     = 0x5,
	.max_bw_counter_limit = 0xFF,
};

static struct cam_irq_register_set sfe680_top_irq_reg_set[1] = {
	{
	.mask_reg_offset   = 0x00000020,
	.clear_reg_offset  = 0x00000024,
	.status_reg_offset = 0x00000028,
	},
};

static struct cam_irq_controller_reg_info sfe680_top_irq_reg_info = {
	.num_registers = 1,
	.irq_reg_set = sfe680_top_irq_reg_set,
	.global_clear_offset  = 0x0000001C,
	.global_clear_bitmask = 0x00000001,
	.clear_all_bitmask = 0xFFFFFFFF,
};

struct cam_sfe_hw_info cam_sfe680_hw_info = {
	.irq_reg_info                  = &sfe680_top_irq_reg_info,

	.bus_wr_version                = CAM_SFE_BUS_WR_VER_1_0,
	.bus_wr_hw_info                = &sfe680_bus_wr_hw_info,

	.bus_rd_version                = CAM_SFE_BUS_RD_VER_1_0,
	.bus_rd_hw_info                = &sfe680_bus_rd_hw_info,

	.top_version                   = CAM_SFE_TOP_VER_1_0,
	.top_hw_info                   = &sfe680_top_hw_info,
};

#endif /* _CAM_SFE680_H_ */
