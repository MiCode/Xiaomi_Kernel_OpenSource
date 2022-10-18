/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CAM_TFE_CSID_530_H_
#define _CAM_TFE_CSID_530_H_

#include "cam_tfe_csid_core.h"

#define CAM_TFE_CSID_VERSION_V530                 0x50030000

static struct cam_tfe_csid_pxl_reg_offset  cam_tfe_csid_530_ipp_reg_offset = {
	.csid_pxl_irq_status_addr            = 0x30,
	.csid_pxl_irq_mask_addr              = 0x34,
	.csid_pxl_irq_clear_addr             = 0x38,
	.csid_pxl_irq_set_addr               = 0x3c,

	.csid_pxl_cfg0_addr                  = 0x200,
	.csid_pxl_cfg1_addr                  = 0x204,
	.csid_pxl_ctrl_addr                  = 0x208,
	.csid_pxl_hcrop_addr                 = 0x21c,
	.csid_pxl_vcrop_addr                 = 0x220,
	.csid_pxl_rst_strobes_addr           = 0x240,
	.csid_pxl_status_addr                = 0x254,
	.csid_pxl_misr_val_addr              = 0x258,
	.csid_pxl_timestamp_curr0_sof_addr   = 0x290,
	.csid_pxl_timestamp_curr1_sof_addr   = 0x294,
	.csid_pxl_timestamp_perv0_sof_addr   = 0x298,
	.csid_pxl_timestamp_perv1_sof_addr   = 0x29c,
	.csid_pxl_timestamp_curr0_eof_addr   = 0x2a0,
	.csid_pxl_timestamp_curr1_eof_addr   = 0x2a4,
	.csid_pxl_timestamp_perv0_eof_addr   = 0x2a8,
	.csid_pxl_timestamp_perv1_eof_addr   = 0x2ac,
	.csid_pxl_err_recovery_cfg0_addr     = 0x2d0,
	.csid_pxl_err_recovery_cfg1_addr     = 0x2d4,
	.csid_pxl_err_recovery_cfg2_addr     = 0x2d8,
	/* configurations */
	.pix_store_en_shift_val              = 7,
	.early_eof_en_shift_val              = 29,
	.halt_master_sel_shift               = 4,
	.halt_mode_shift                     = 2,
	.halt_master_sel_master_val          = 3,
	.halt_master_sel_slave_val           = 0,
	.binning_supported                   = 0,
	.is_multi_vc_dt_supported            = false,
};

static struct cam_tfe_csid_rdi_reg_offset cam_tfe_csid_530_rdi_0_reg_offset = {
	.csid_rdi_irq_status_addr                 = 0x40,
	.csid_rdi_irq_mask_addr                   = 0x44,
	.csid_rdi_irq_clear_addr                  = 0x48,
	.csid_rdi_irq_set_addr                    = 0x4c,

	.csid_rdi_cfg0_addr                       = 0x300,
	.csid_rdi_cfg1_addr                       = 0x304,
	.csid_rdi_ctrl_addr                       = 0x308,
	.csid_rdi_rst_strobes_addr                = 0x340,
	.csid_rdi_status_addr                     = 0x350,
	.csid_rdi_misr_val0_addr                  = 0x354,
	.csid_rdi_misr_val1_addr                  = 0x358,
	.csid_rdi_timestamp_curr0_sof_addr        = 0x390,
	.csid_rdi_timestamp_curr1_sof_addr        = 0x394,
	.csid_rdi_timestamp_prev0_sof_addr        = 0x398,
	.csid_rdi_timestamp_prev1_sof_addr        = 0x39c,
	.csid_rdi_timestamp_curr0_eof_addr        = 0x3a0,
	.csid_rdi_timestamp_curr1_eof_addr        = 0x3a4,
	.csid_rdi_timestamp_prev0_eof_addr        = 0x3a8,
	.csid_rdi_timestamp_prev1_eof_addr        = 0x3ac,
	.csid_rdi_err_recovery_cfg0_addr          = 0x3b0,
	.csid_rdi_err_recovery_cfg1_addr          = 0x3b4,
	.csid_rdi_err_recovery_cfg2_addr          = 0x3b8,
	.csid_rdi_byte_cntr_ping_addr             = 0x3e0,
	.csid_rdi_byte_cntr_pong_addr             = 0x3e4,
	/* configurations */
	.is_multi_vc_dt_supported                 = false,
};

static struct cam_tfe_csid_rdi_reg_offset cam_tfe_csid_530_rdi_1_reg_offset = {
	.csid_rdi_irq_status_addr                 = 0x50,
	.csid_rdi_irq_mask_addr                   = 0x54,
	.csid_rdi_irq_clear_addr                  = 0x58,
	.csid_rdi_irq_set_addr                    = 0x5c,

	.csid_rdi_cfg0_addr                       = 0x400,
	.csid_rdi_cfg1_addr                       = 0x404,
	.csid_rdi_ctrl_addr                       = 0x408,
	.csid_rdi_rst_strobes_addr                = 0x440,
	.csid_rdi_status_addr                     = 0x450,
	.csid_rdi_misr_val0_addr                  = 0x454,
	.csid_rdi_misr_val1_addr                  = 0x458,
	.csid_rdi_timestamp_curr0_sof_addr        = 0x490,
	.csid_rdi_timestamp_curr1_sof_addr        = 0x494,
	.csid_rdi_timestamp_prev0_sof_addr        = 0x498,
	.csid_rdi_timestamp_prev1_sof_addr        = 0x49c,
	.csid_rdi_timestamp_curr0_eof_addr        = 0x4a0,
	.csid_rdi_timestamp_curr1_eof_addr        = 0x4a4,
	.csid_rdi_timestamp_prev0_eof_addr        = 0x4a8,
	.csid_rdi_timestamp_prev1_eof_addr        = 0x4ac,
	.csid_rdi_err_recovery_cfg0_addr          = 0x4b0,
	.csid_rdi_err_recovery_cfg1_addr          = 0x4b4,
	.csid_rdi_err_recovery_cfg2_addr          = 0x4b8,
	.csid_rdi_byte_cntr_ping_addr             = 0x4e0,
	.csid_rdi_byte_cntr_pong_addr             = 0x4e4,
	/* configurations */
	.is_multi_vc_dt_supported                 = false,
};

static struct cam_tfe_csid_rdi_reg_offset cam_tfe_csid_530_rdi_2_reg_offset = {
	.csid_rdi_irq_status_addr                 = 0x60,
	.csid_rdi_irq_mask_addr                   = 0x64,
	.csid_rdi_irq_clear_addr                  = 0x68,
	.csid_rdi_irq_set_addr                    = 0x6c,

	.csid_rdi_cfg0_addr                       = 0x500,
	.csid_rdi_cfg1_addr                       = 0x504,
	.csid_rdi_ctrl_addr                       = 0x508,
	.csid_rdi_rst_strobes_addr                = 0x540,
	.csid_rdi_status_addr                     = 0x550,
	.csid_rdi_misr_val0_addr                  = 0x554,
	.csid_rdi_misr_val1_addr                  = 0x558,
	.csid_rdi_timestamp_curr0_sof_addr        = 0x590,
	.csid_rdi_timestamp_curr1_sof_addr        = 0x594,
	.csid_rdi_timestamp_prev0_sof_addr        = 0x598,
	.csid_rdi_timestamp_prev1_sof_addr        = 0x59c,
	.csid_rdi_timestamp_curr0_eof_addr        = 0x5a0,
	.csid_rdi_timestamp_curr1_eof_addr        = 0x5a4,
	.csid_rdi_timestamp_prev0_eof_addr        = 0x5a8,
	.csid_rdi_timestamp_prev1_eof_addr        = 0x5ac,
	.csid_rdi_err_recovery_cfg0_addr          = 0x5b0,
	.csid_rdi_err_recovery_cfg1_addr          = 0x5b4,
	.csid_rdi_err_recovery_cfg2_addr          = 0x5b8,
	.csid_rdi_byte_cntr_ping_addr             = 0x5e0,
	.csid_rdi_byte_cntr_pong_addr             = 0x5e4,
	/* configurations */
	.is_multi_vc_dt_supported                 = false,
};

static struct cam_tfe_csid_csi2_rx_reg_offset
	cam_tfe_csid_530_csi2_reg_offset = {
	.csid_csi2_rx_irq_status_addr                 = 0x20,
	.csid_csi2_rx_irq_mask_addr                   = 0x24,
	.csid_csi2_rx_irq_clear_addr                  = 0x28,
	.csid_csi2_rx_irq_set_addr                    = 0x2c,

	/*CSI2 rx control */
	.phy_sel_base                                 = 1,
	.csid_csi2_rx_cfg0_addr                       = 0x100,
	.csid_csi2_rx_cfg1_addr                       = 0x104,
	.csid_csi2_rx_capture_ctrl_addr               = 0x108,
	.csid_csi2_rx_rst_strobes_addr                = 0x110,
	.csid_csi2_rx_cap_unmap_long_pkt_hdr_0_addr   = 0x120,
	.csid_csi2_rx_cap_unmap_long_pkt_hdr_1_addr   = 0x124,
	.csid_csi2_rx_captured_short_pkt_0_addr       = 0x128,
	.csid_csi2_rx_captured_short_pkt_1_addr       = 0x12c,
	.csid_csi2_rx_captured_long_pkt_0_addr        = 0x130,
	.csid_csi2_rx_captured_long_pkt_1_addr        = 0x134,
	.csid_csi2_rx_captured_long_pkt_ftr_addr      = 0x138,
	.csid_csi2_rx_captured_cphy_pkt_hdr_addr      = 0x13c,
	.csid_csi2_rx_total_pkts_rcvd_addr            = 0x160,
	.csid_csi2_rx_stats_ecc_addr                  = 0x164,
	.csid_csi2_rx_total_crc_err_addr              = 0x168,

	.phy_tpg_base_id                              = 0,
	.csi2_rst_srb_all                             = 0x3FFF,
	.csi2_rst_done_shift_val                      = 27,
	.csi2_irq_mask_all                            = 0xFFFFFFF,
	.csi2_misr_enable_shift_val                   = 6,
	.csi2_capture_long_pkt_en_shift               = 0,
	.csi2_capture_short_pkt_en_shift              = 1,
	.csi2_capture_cphy_pkt_en_shift               = 2,
	.csi2_capture_long_pkt_dt_shift               = 4,
	.csi2_capture_long_pkt_vc_shift               = 10,
	.csi2_capture_short_pkt_vc_shift              = 12,
	.csi2_capture_cphy_pkt_dt_shift               = 14,
	.csi2_capture_cphy_pkt_vc_shift               = 20,
	.csi2_rx_phy_num_mask                         = 0x7,
	.csi2_rx_long_pkt_hdr_rst_stb_shift           = 0x1,
	.csi2_rx_short_pkt_hdr_rst_stb_shift          = 0x2,
	.csi2_rx_cphy_pkt_hdr_rst_stb_shift           = 0x3,
	.need_to_sel_tpg_mux                          = false,
};

static struct cam_tfe_csid_common_reg_offset
	cam_tfe_csid_530_cmn_reg_offset = {
	.csid_hw_version_addr                         = 0x0,
	.csid_cfg0_addr                               = 0x4,
	.csid_ctrl_addr                               = 0x8,
	.csid_rst_strobes_addr                        = 0x10,

	.csid_test_bus_ctrl_addr                      = 0x14,
	.csid_top_irq_status_addr                     = 0x70,
	.csid_top_irq_mask_addr                       = 0x74,
	.csid_top_irq_clear_addr                      = 0x78,
	.csid_top_irq_set_addr                        = 0x7c,
	.csid_irq_cmd_addr                            = 0x80,

	/*configurations */
	.major_version                                = 5,
	.minor_version                                = 3,
	.version_incr                                 = 0,
	.num_rdis                                     = 3,
	.num_pix                                      = 1,
	.csid_reg_rst_stb                             = 1,
	.csid_rst_stb                                 = 0x1e,
	.csid_rst_stb_sw_all                          = 0x1f,
	.ipp_path_rst_stb_all                         = 0x17,
	.rdi_path_rst_stb_all                         = 0x97,
	.path_rst_done_shift_val                      = 1,
	.path_en_shift_val                            = 31,
	.dt_id_shift_val                              = 27,
	.vc_shift_val                                 = 22,
	.dt_shift_val                                 = 16,
	.fmt_shift_val                                = 12,
	.plain_fmt_shit_val                           = 10,
	.crop_v_en_shift_val                          = 6,
	.crop_h_en_shift_val                          = 5,
	.crop_shift                                   = 16,
	.ipp_irq_mask_all                             = 0x3FFFF,
	.rdi_irq_mask_all                             = 0x3FFFF,
	.top_tfe2_pix_pipe_fuse_reg                   = 0xFE4,
	.top_tfe2_fuse_reg                            = 0xFE8,
	.format_measure_support                       = false,
};

static struct cam_tfe_csid_reg_offset cam_tfe_csid_530_reg_offset = {
	.cmn_reg          = &cam_tfe_csid_530_cmn_reg_offset,
	.csi2_reg         = &cam_tfe_csid_530_csi2_reg_offset,
	.ipp_reg          = &cam_tfe_csid_530_ipp_reg_offset,
	.rdi_reg = {
		&cam_tfe_csid_530_rdi_0_reg_offset,
		&cam_tfe_csid_530_rdi_1_reg_offset,
		&cam_tfe_csid_530_rdi_2_reg_offset,
		},
};

static struct cam_tfe_csid_hw_info cam_tfe_csid530_hw_info = {
	.csid_reg = &cam_tfe_csid_530_reg_offset,
	.hw_dts_version = CAM_TFE_CSID_VERSION_V530,
};

#endif /*_CAM_TFE_CSID_530_H_ */
