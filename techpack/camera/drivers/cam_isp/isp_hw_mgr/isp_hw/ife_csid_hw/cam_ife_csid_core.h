/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_HW_H_
#define _CAM_IFE_CSID_HW_H_

#include "cam_hw.h"
#include "cam_ife_csid_hw_intf.h"
#include "cam_ife_csid_soc.h"
#include "cam_ife_csid_core.h"

#define CSID_CSI2_RX_INFO_PHY_DL0_EOT_CAPTURED    BIT(0)
#define CSID_CSI2_RX_INFO_PHY_DL1_EOT_CAPTURED    BIT(1)
#define CSID_CSI2_RX_INFO_PHY_DL2_EOT_CAPTURED    BIT(2)
#define CSID_CSI2_RX_INFO_PHY_DL3_EOT_CAPTURED    BIT(3)
#define CSID_CSI2_RX_INFO_PHY_DL0_SOT_CAPTURED    BIT(4)
#define CSID_CSI2_RX_INFO_PHY_DL1_SOT_CAPTURED    BIT(5)
#define CSID_CSI2_RX_INFO_PHY_DL2_SOT_CAPTURED    BIT(6)
#define CSID_CSI2_RX_INFO_PHY_DL3_SOT_CAPTURED    BIT(7)
#define CSID_CSI2_RX_INFO_LONG_PKT_CAPTURED       BIT(8)
#define CSID_CSI2_RX_INFO_SHORT_PKT_CAPTURED      BIT(9)
#define CSID_CSI2_RX_INFO_CPHY_PKT_HDR_CAPTURED   BIT(10)
#define CSID_CSI2_RX_ERROR_CPHY_EOT_RECEPTION     BIT(11)
#define CSID_CSI2_RX_ERROR_CPHY_SOT_RECEPTION     BIT(12)
#define CSID_CSI2_RX_ERROR_CPHY_PH_CRC            BIT(13)
#define CSID_CSI2_RX_WARNING_ECC                  BIT(14)
#define CSID_CSI2_RX_ERROR_LANE0_FIFO_OVERFLOW    BIT(15)
#define CSID_CSI2_RX_ERROR_LANE1_FIFO_OVERFLOW    BIT(16)
#define CSID_CSI2_RX_ERROR_LANE2_FIFO_OVERFLOW    BIT(17)
#define CSID_CSI2_RX_ERROR_LANE3_FIFO_OVERFLOW    BIT(18)
#define CSID_CSI2_RX_ERROR_CRC                    BIT(19)
#define CSID_CSI2_RX_ERROR_ECC                    BIT(20)
#define CSID_CSI2_RX_ERROR_MMAPPED_VC_DT          BIT(21)
#define CSID_CSI2_RX_ERROR_UNMAPPED_VC_DT         BIT(22)
#define CSID_CSI2_RX_ERROR_STREAM_UNDERFLOW       BIT(23)
#define CSID_CSI2_RX_ERROR_UNBOUNDED_FRAME        BIT(24)
#define CSID_CSI2_RX_INFO_TG_DONE                 BIT(25)
#define CSID_CSI2_RX_ERROR_TG_FIFO_OVERFLOW       BIT(26)
#define CSID_CSI2_RX_INFO_RST_DONE                BIT(27)

#define CSID_TOP_IRQ_DONE                         BIT(0)
#define CSID_PATH_INFO_RST_DONE                   BIT(1)
#define CSID_PATH_ERROR_FIFO_OVERFLOW             BIT(2)
#define CSID_PATH_INFO_SUBSAMPLED_EOF             BIT(3)
#define CSID_PATH_INFO_SUBSAMPLED_SOF             BIT(4)
#define CSID_PATH_INFO_FRAME_DROP_EOF             BIT(5)
#define CSID_PATH_INFO_FRAME_DROP_EOL             BIT(6)
#define CSID_PATH_INFO_FRAME_DROP_SOL             BIT(7)
#define CSID_PATH_INFO_FRAME_DROP_SOF             BIT(8)
#define CSID_PATH_INFO_INPUT_EOF                  BIT(9)
#define CSID_PATH_INFO_INPUT_EOL                  BIT(10)
#define CSID_PATH_INFO_INPUT_SOL                  BIT(11)
#define CSID_PATH_INFO_INPUT_SOF                  BIT(12)
#define CSID_PATH_ERROR_PIX_COUNT                 BIT(13)
#define CSID_PATH_ERROR_LINE_COUNT                BIT(14)
#define CSID_PATH_ERROR_CCIF_VIOLATION            BIT(15)
#define CSID_PATH_OVERFLOW_RECOVERY               BIT(17)

/*
 * Debug values enable the corresponding interrupts and debug logs provide
 * necessary information
 */
#define CSID_DEBUG_ENABLE_SOF_IRQ                 BIT(0)
#define CSID_DEBUG_ENABLE_EOF_IRQ                 BIT(1)
#define CSID_DEBUG_ENABLE_SOT_IRQ                 BIT(2)
#define CSID_DEBUG_ENABLE_EOT_IRQ                 BIT(3)
#define CSID_DEBUG_ENABLE_SHORT_PKT_CAPTURE       BIT(4)
#define CSID_DEBUG_ENABLE_LONG_PKT_CAPTURE        BIT(5)
#define CSID_DEBUG_ENABLE_CPHY_PKT_CAPTURE        BIT(6)
#define CSID_DEBUG_ENABLE_HBI_VBI_INFO            BIT(7)
#define CSID_DEBUG_DISABLE_EARLY_EOF              BIT(8)

#define CAM_CSID_EVT_PAYLOAD_MAX                  10

/* enum cam_csid_path_halt_mode select the path halt mode control */
enum cam_csid_path_halt_mode {
	CSID_HALT_MODE_INTERNAL,
	CSID_HALT_MODE_GLOBAL,
	CSID_HALT_MODE_MASTER,
	CSID_HALT_MODE_SLAVE,
};

/**
 *enum cam_csid_path_timestamp_stb_sel - select the sof/eof strobes used to
 *        capture the timestamp
 */
enum cam_csid_path_timestamp_stb_sel {
	CSID_TIMESTAMP_STB_PRE_HALT,
	CSID_TIMESTAMP_STB_POST_HALT,
	CSID_TIMESTAMP_STB_POST_IRQ,
	CSID_TIMESTAMP_STB_MAX,
};

/**
 * enum cam_ife_pix_path_res_id - Specify the csid patch
 */
enum cam_ife_csid_irq_reg {
	CAM_IFE_CSID_IRQ_REG_RDI_0,
	CAM_IFE_CSID_IRQ_REG_RDI_1,
	CAM_IFE_CSID_IRQ_REG_RDI_2,
	CAM_IFE_CSID_IRQ_REG_RDI_3,
	CAM_IFE_CSID_IRQ_REG_TOP,
	CAM_IFE_CSID_IRQ_REG_RX,
	CAM_IFE_CSID_IRQ_REG_IPP,
	CAM_IFE_CSID_IRQ_REG_PPP,
	CAM_IFE_CSID_IRQ_REG_UDI_0,
	CAM_IFE_CSID_IRQ_REG_UDI_1,
	CAM_IFE_CSID_IRQ_REG_UDI_2,
	CAM_IFE_CSID_IRQ_REG_MAX,
};

struct cam_ife_csid_pxl_reg_offset {
	/* Pxl path register offsets*/
	uint32_t csid_pxl_irq_status_addr;
	uint32_t csid_pxl_irq_mask_addr;
	uint32_t csid_pxl_irq_clear_addr;
	uint32_t csid_pxl_irq_set_addr;

	uint32_t csid_pxl_cfg0_addr;
	uint32_t csid_pxl_cfg1_addr;
	uint32_t csid_pxl_ctrl_addr;
	uint32_t csid_pxl_frm_drop_pattern_addr;
	uint32_t csid_pxl_frm_drop_period_addr;
	uint32_t csid_pxl_irq_subsample_pattern_addr;
	uint32_t csid_pxl_irq_subsample_period_addr;
	uint32_t csid_pxl_hcrop_addr;
	uint32_t csid_pxl_vcrop_addr;
	uint32_t csid_pxl_pix_drop_pattern_addr;
	uint32_t csid_pxl_pix_drop_period_addr;
	uint32_t csid_pxl_line_drop_pattern_addr;
	uint32_t csid_pxl_line_drop_period_addr;
	uint32_t csid_pxl_rst_strobes_addr;
	uint32_t csid_pxl_status_addr;
	uint32_t csid_pxl_misr_val_addr;
	uint32_t csid_pxl_format_measure_cfg0_addr;
	uint32_t csid_pxl_format_measure_cfg1_addr;
	uint32_t csid_pxl_format_measure0_addr;
	uint32_t csid_pxl_format_measure1_addr;
	uint32_t csid_pxl_format_measure2_addr;
	uint32_t csid_pxl_timestamp_curr0_sof_addr;
	uint32_t csid_pxl_timestamp_curr1_sof_addr;
	uint32_t csid_pxl_timestamp_perv0_sof_addr;
	uint32_t csid_pxl_timestamp_perv1_sof_addr;
	uint32_t csid_pxl_timestamp_curr0_eof_addr;
	uint32_t csid_pxl_timestamp_curr1_eof_addr;
	uint32_t csid_pxl_timestamp_perv0_eof_addr;
	uint32_t csid_pxl_timestamp_perv1_eof_addr;
	uint32_t csid_pxl_err_recovery_cfg0_addr;
	uint32_t csid_pxl_err_recovery_cfg1_addr;
	uint32_t csid_pxl_err_recovery_cfg2_addr;
	uint32_t csid_pxl_multi_vcdt_cfg0_addr;

	/* configuration */
	uint32_t pix_store_en_shift_val;
	uint32_t early_eof_en_shift_val;
	uint32_t horizontal_bin_en_shift_val;
	uint32_t quad_cfa_bin_en_shift_val;
	uint32_t ccif_violation_en;
	uint32_t overflow_ctrl_en;
};

struct cam_ife_csid_rdi_reg_offset {
	uint32_t csid_rdi_irq_status_addr;
	uint32_t csid_rdi_irq_mask_addr;
	uint32_t csid_rdi_irq_clear_addr;
	uint32_t csid_rdi_irq_set_addr;

	/*RDI N register address */
	uint32_t csid_rdi_cfg0_addr;
	uint32_t csid_rdi_cfg1_addr;
	uint32_t csid_rdi_ctrl_addr;
	uint32_t csid_rdi_frm_drop_pattern_addr;
	uint32_t csid_rdi_frm_drop_period_addr;
	uint32_t csid_rdi_irq_subsample_pattern_addr;
	uint32_t csid_rdi_irq_subsample_period_addr;
	uint32_t csid_rdi_rpp_hcrop_addr;
	uint32_t csid_rdi_rpp_vcrop_addr;
	uint32_t csid_rdi_rpp_pix_drop_pattern_addr;
	uint32_t csid_rdi_rpp_pix_drop_period_addr;
	uint32_t csid_rdi_rpp_line_drop_pattern_addr;
	uint32_t csid_rdi_rpp_line_drop_period_addr;
	uint32_t csid_rdi_yuv_chroma_conversion_addr;
	uint32_t csid_rdi_rst_strobes_addr;
	uint32_t csid_rdi_status_addr;
	uint32_t csid_rdi_misr_val0_addr;
	uint32_t csid_rdi_misr_val1_addr;
	uint32_t csid_rdi_misr_val2_addr;
	uint32_t csid_rdi_misr_val3_addr;
	uint32_t csid_rdi_format_measure_cfg0_addr;
	uint32_t csid_rdi_format_measure_cfg1_addr;
	uint32_t csid_rdi_format_measure0_addr;
	uint32_t csid_rdi_format_measure1_addr;
	uint32_t csid_rdi_format_measure2_addr;
	uint32_t csid_rdi_timestamp_curr0_sof_addr;
	uint32_t csid_rdi_timestamp_curr1_sof_addr;
	uint32_t csid_rdi_timestamp_prev0_sof_addr;
	uint32_t csid_rdi_timestamp_prev1_sof_addr;
	uint32_t csid_rdi_timestamp_curr0_eof_addr;
	uint32_t csid_rdi_timestamp_curr1_eof_addr;
	uint32_t csid_rdi_timestamp_prev0_eof_addr;
	uint32_t csid_rdi_timestamp_prev1_eof_addr;
	uint32_t csid_rdi_err_recovery_cfg0_addr;
	uint32_t csid_rdi_err_recovery_cfg1_addr;
	uint32_t csid_rdi_err_recovery_cfg2_addr;
	uint32_t csid_rdi_multi_vcdt_cfg0_addr;
	uint32_t csid_rdi_byte_cntr_ping_addr;
	uint32_t csid_rdi_byte_cntr_pong_addr;

	/* configuration */
	uint32_t packing_format;
	uint32_t ccif_violation_en;
	uint32_t overflow_ctrl_en;
};

struct cam_ife_csid_udi_reg_offset {
	uint32_t csid_udi_irq_status_addr;
	uint32_t csid_udi_irq_mask_addr;
	uint32_t csid_udi_irq_clear_addr;
	uint32_t csid_udi_irq_set_addr;

	/* UDI N register address */
	uint32_t csid_udi_cfg0_addr;
	uint32_t csid_udi_cfg1_addr;
	uint32_t csid_udi_ctrl_addr;
	uint32_t csid_udi_frm_drop_pattern_addr;
	uint32_t csid_udi_frm_drop_period_addr;
	uint32_t csid_udi_irq_subsample_pattern_addr;
	uint32_t csid_udi_irq_subsample_period_addr;
	uint32_t csid_udi_rpp_hcrop_addr;
	uint32_t csid_udi_rpp_vcrop_addr;
	uint32_t csid_udi_rpp_pix_drop_pattern_addr;
	uint32_t csid_udi_rpp_pix_drop_period_addr;
	uint32_t csid_udi_rpp_line_drop_pattern_addr;
	uint32_t csid_udi_rpp_line_drop_period_addr;
	uint32_t csid_udi_yuv_chroma_conversion_addr;
	uint32_t csid_udi_rst_strobes_addr;
	uint32_t csid_udi_status_addr;
	uint32_t csid_udi_misr_val0_addr;
	uint32_t csid_udi_misr_val1_addr;
	uint32_t csid_udi_misr_val2_addr;
	uint32_t csid_udi_misr_val3_addr;
	uint32_t csid_udi_format_measure_cfg0_addr;
	uint32_t csid_udi_format_measure_cfg1_addr;
	uint32_t csid_udi_format_measure0_addr;
	uint32_t csid_udi_format_measure1_addr;
	uint32_t csid_udi_format_measure2_addr;
	uint32_t csid_udi_timestamp_curr0_sof_addr;
	uint32_t csid_udi_timestamp_curr1_sof_addr;
	uint32_t csid_udi_timestamp_prev0_sof_addr;
	uint32_t csid_udi_timestamp_prev1_sof_addr;
	uint32_t csid_udi_timestamp_curr0_eof_addr;
	uint32_t csid_udi_timestamp_curr1_eof_addr;
	uint32_t csid_udi_timestamp_prev0_eof_addr;
	uint32_t csid_udi_timestamp_prev1_eof_addr;
	uint32_t csid_udi_err_recovery_cfg0_addr;
	uint32_t csid_udi_err_recovery_cfg1_addr;
	uint32_t csid_udi_err_recovery_cfg2_addr;
	uint32_t csid_udi_multi_vcdt_cfg0_addr;
	uint32_t csid_udi_byte_cntr_ping_addr;
	uint32_t csid_udi_byte_cntr_pong_addr;

	/* configuration */
	uint32_t packing_format;
	uint32_t ccif_violation_en;
	uint32_t overflow_ctrl_en;
};

struct cam_ife_csid_csi2_rx_reg_offset {
	uint32_t csid_csi2_rx_irq_status_addr;
	uint32_t csid_csi2_rx_irq_mask_addr;
	uint32_t csid_csi2_rx_irq_clear_addr;
	uint32_t csid_csi2_rx_irq_set_addr;
	uint32_t csid_csi2_rx_cfg0_addr;
	uint32_t csid_csi2_rx_cfg1_addr;
	uint32_t csid_csi2_rx_capture_ctrl_addr;
	uint32_t csid_csi2_rx_rst_strobes_addr;
	uint32_t csid_csi2_rx_de_scramble_cfg0_addr;
	uint32_t csid_csi2_rx_de_scramble_cfg1_addr;
	uint32_t csid_csi2_rx_cap_unmap_long_pkt_hdr_0_addr;
	uint32_t csid_csi2_rx_cap_unmap_long_pkt_hdr_1_addr;
	uint32_t csid_csi2_rx_captured_short_pkt_0_addr;
	uint32_t csid_csi2_rx_captured_short_pkt_1_addr;
	uint32_t csid_csi2_rx_captured_long_pkt_0_addr;
	uint32_t csid_csi2_rx_captured_long_pkt_1_addr;
	uint32_t csid_csi2_rx_captured_long_pkt_ftr_addr;
	uint32_t csid_csi2_rx_captured_cphy_pkt_hdr_addr;
	uint32_t csid_csi2_rx_lane0_misr_addr;
	uint32_t csid_csi2_rx_lane1_misr_addr;
	uint32_t csid_csi2_rx_lane2_misr_addr;
	uint32_t csid_csi2_rx_lane3_misr_addr;
	uint32_t csid_csi2_rx_total_pkts_rcvd_addr;
	uint32_t csid_csi2_rx_stats_ecc_addr;
	uint32_t csid_csi2_rx_total_crc_err_addr;
	uint32_t csid_csi2_rx_de_scramble_type3_cfg0_addr;
	uint32_t csid_csi2_rx_de_scramble_type3_cfg1_addr;
	uint32_t csid_csi2_rx_de_scramble_type2_cfg0_addr;
	uint32_t csid_csi2_rx_de_scramble_type2_cfg1_addr;
	uint32_t csid_csi2_rx_de_scramble_type1_cfg0_addr;
	uint32_t csid_csi2_rx_de_scramble_type1_cfg1_addr;
	uint32_t csid_csi2_rx_de_scramble_type0_cfg0_addr;
	uint32_t csid_csi2_rx_de_scramble_type0_cfg1_addr;

	/*configurations */
	uint32_t csi2_rst_srb_all;
	uint32_t csi2_rst_done_shift_val;
	uint32_t csi2_irq_mask_all;
	uint32_t csi2_misr_enable_shift_val;
	uint32_t csi2_vc_mode_shift_val;
	uint32_t csi2_capture_long_pkt_en_shift;
	uint32_t csi2_capture_short_pkt_en_shift;
	uint32_t csi2_capture_cphy_pkt_en_shift;
	uint32_t csi2_capture_long_pkt_dt_shift;
	uint32_t csi2_capture_long_pkt_vc_shift;
	uint32_t csi2_capture_short_pkt_vc_shift;
	uint32_t csi2_capture_cphy_pkt_dt_shift;
	uint32_t csi2_capture_cphy_pkt_vc_shift;
	uint32_t csi2_rx_phy_num_mask;
};

struct cam_ife_csid_csi2_tpg_reg_offset {
	uint32_t csid_tpg_ctrl_addr;
	uint32_t csid_tpg_vc_cfg0_addr;
	uint32_t csid_tpg_vc_cfg1_addr;
	uint32_t csid_tpg_lfsr_seed_addr;
	uint32_t csid_tpg_dt_n_cfg_0_addr;
	uint32_t csid_tpg_dt_n_cfg_1_addr;
	uint32_t csid_tpg_dt_n_cfg_2_addr;
	uint32_t csid_tpg_color_bars_cfg_addr;
	uint32_t csid_tpg_color_box_cfg_addr;
	uint32_t csid_tpg_common_gen_cfg_addr;
	uint32_t csid_tpg_cgen_n_cfg_addr;
	uint32_t csid_tpg_cgen_n_x0_addr;
	uint32_t csid_tpg_cgen_n_x1_addr;
	uint32_t csid_tpg_cgen_n_x2_addr;
	uint32_t csid_tpg_cgen_n_xy_addr;
	uint32_t csid_tpg_cgen_n_y1_addr;
	uint32_t csid_tpg_cgen_n_y2_addr;

	/*configurations */
	uint32_t tpg_dtn_cfg_offset;
	uint32_t tpg_cgen_cfg_offset;
	uint32_t tpg_cpas_ife_reg_offset;

};
struct cam_ife_csid_common_reg_offset {
	/* MIPI CSID registers */
	uint32_t csid_hw_version_addr;
	uint32_t csid_cfg0_addr;
	uint32_t csid_ctrl_addr;
	uint32_t csid_reset_addr;
	uint32_t csid_rst_strobes_addr;

	uint32_t csid_test_bus_ctrl_addr;
	uint32_t csid_top_irq_status_addr;
	uint32_t csid_top_irq_mask_addr;
	uint32_t csid_top_irq_clear_addr;
	uint32_t csid_top_irq_set_addr;
	uint32_t csid_irq_cmd_addr;

	/*configurations */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
	uint32_t num_udis;
	uint32_t num_rdis;
	uint32_t num_pix;
	uint32_t num_ppp;
	uint32_t csid_reg_rst_stb;
	uint32_t csid_rst_stb;
	uint32_t csid_rst_stb_sw_all;
	uint32_t path_rst_stb_all;
	uint32_t path_rst_done_shift_val;
	uint32_t path_en_shift_val;
	uint32_t packing_fmt_shift_val;
	uint32_t dt_id_shift_val;
	uint32_t vc_shift_val;
	uint32_t dt_shift_val;
	uint32_t fmt_shift_val;
	uint32_t plain_fmt_shit_val;
	uint32_t crop_v_en_shift_val;
	uint32_t crop_h_en_shift_val;
	uint32_t drop_v_en_shift_val;
	uint32_t drop_h_en_shift_val;
	uint32_t crop_shift;
	uint32_t ipp_irq_mask_all;
	uint32_t rdi_irq_mask_all;
	uint32_t ppp_irq_mask_all;
	uint32_t udi_irq_mask_all;
	uint32_t measure_en_hbi_vbi_cnt_mask;
	uint32_t format_measure_en_val;
	uint32_t num_bytes_out_shift_val;
};

/**
 * struct cam_ife_csid_reg_offset- CSID instance register info
 *
 * @cmn_reg:  csid common registers info
 * @ipp_reg:  ipp register offset information
 * @ppp_reg:  ppp register offset information
 * @rdi_reg:  rdi register offset information
 * @udi_reg:  udi register offset information
 * @tpg_reg:  tpg register offset information
 *
 */
struct cam_ife_csid_reg_offset {
	const struct cam_ife_csid_common_reg_offset   *cmn_reg;
	const struct cam_ife_csid_csi2_rx_reg_offset  *csi2_reg;
	const struct cam_ife_csid_pxl_reg_offset      *ipp_reg;
	const struct cam_ife_csid_pxl_reg_offset      *ppp_reg;
	const struct cam_ife_csid_rdi_reg_offset *rdi_reg[CAM_IFE_CSID_RDI_MAX];
	const struct cam_ife_csid_udi_reg_offset *udi_reg[CAM_IFE_CSID_UDI_MAX];
	const struct cam_ife_csid_csi2_tpg_reg_offset *tpg_reg;
};


/**
 * struct cam_ife_csid_hw_info- CSID HW info
 *
 * @csid_reg:        csid register offsets
 * @hw_dts_version:  HW DTS version
 * @csid_max_clk:    maximim csid clock
 *
 */
struct cam_ife_csid_hw_info {
	const struct cam_ife_csid_reg_offset   *csid_reg;
	uint32_t                                hw_dts_version;
	uint32_t                                csid_max_clk;

};



/**
 * struct cam_ife_csid_csi2_rx_cfg- csid csi2 rx configuration data
 * @phy_sel:     input resource type for sensor only
 * @lane_type:   lane type: c-phy or d-phy
 * @lane_num :   active lane number
 * @lane_cfg:    lane configurations: 4 bits per lane
 *
 */
struct cam_ife_csid_csi2_rx_cfg  {
	uint32_t                        phy_sel;
	uint32_t                        lane_type;
	uint32_t                        lane_num;
	uint32_t                        lane_cfg;
};

/**
 * struct             cam_ife_csid_tpg_cfg- csid tpg configuration data
 * @width:            width
 * @height:           height
 * @test_pattern :    pattern
 * @in_format:        decode format
 * @usage_type:       whether dual isp is required
 *
 */
struct cam_ife_csid_tpg_cfg  {
	uint32_t                        width;
	uint32_t                        height;
	uint32_t                        test_pattern;
	uint32_t                        in_format;
	uint32_t                        usage_type;
};

/**
 * struct cam_ife_csid_cid_data- cid configuration private data
 *
 * @vc:               Virtual channel
 * @dt:               Data type
 * @cnt:              Cid resource reference count.
 * @tpg_set:          Tpg used for this cid resource
 * @is_valid_vc1_dt1: Valid vc1 and dt1
 *
 */
struct cam_ife_csid_cid_data {
	uint32_t                     vc;
	uint32_t                     dt;
	uint32_t                     vc1;
	uint32_t                     dt1;
	uint32_t                     cnt;
	uint32_t                     tpg_set;
	uint32_t                     is_valid_vc1_dt1;
};


/**
 * struct cam_ife_csid_path_cfg- csid path configuration details. It is stored
 *                          as private data for IPP/ RDI paths
 * @vc :            Virtual channel number
 * @dt :            Data type number
 * @cid             cid number, it is same as DT_ID number in HW
 * @in_format:      input decode format
 * @out_format:     output format
 * @crop_enable:    crop is enable or disabled, if enabled
 *                  then remaining parameters are valid.
 * @drop_enable:    flag to indicate pixel drop enable or disable
 * @start_pixel:    start pixel
 * @end_pixel:      end_pixel
 * @width:          width
 * @start_line:     start line
 * @end_line:       end_line
 * @height:         heigth
 * @sync_mode:       Applicable for IPP/RDI path reservation
 *                  Reserving the path for master IPP or slave IPP
 *                  master (set value 1), Slave ( set value 2)
 *                  for RDI, set  mode to none
 * @master_idx:     For Slave reservation, Give master IFE instance Index.
 *                  Slave will synchronize with master Start and stop operations
 * @clk_rate        Clock rate
 * @num_bytes_out:  Number of output bytes per cycle
 *
 */
struct cam_ife_csid_path_cfg {
	uint32_t                        vc;
	uint32_t                        dt;
	uint32_t                        vc1;
	uint32_t                        dt1;
	uint32_t                        is_valid_vc1_dt1;
	uint32_t                        cid;
	uint32_t                        in_format;
	uint32_t                        out_format;
	bool                            crop_enable;
	bool                            drop_enable;
	uint32_t                        start_pixel;
	uint32_t                        end_pixel;
	uint32_t                        width;
	uint32_t                        start_line;
	uint32_t                        end_line;
	uint32_t                        height;
	enum cam_isp_hw_sync_mode       sync_mode;
	uint32_t                        master_idx;
	uint64_t                        clk_rate;
	uint32_t                        horizontal_bin;
	uint32_t                        qcfa_bin;
	uint32_t                        num_bytes_out;
};

/**
 * struct cam_csid_evt_payload- payload for csid hw event
 * @list       : list head
 * @evt_type   : Event type from CSID
 * @irq_status : IRQ Status register
 * @hw_idx     : Hw index
 * @priv       : Private data of payload
 */
struct cam_csid_evt_payload {
	struct list_head   list;
	uint32_t           evt_type;
	uint32_t           irq_status[CAM_IFE_CSID_IRQ_REG_MAX];
	uint32_t           hw_idx;
	void              *priv;
};

/**
 * struct cam_ife_csid_hw- csid hw device resources data
 *
 * @hw_intf:                  contain the csid hw interface information
 * @hw_info:                  csid hw device information
 * @csid_info:                csid hw specific information
 * @tasklet:                  tasklet to handle csid errors
 * @priv:                     private data to be sent with callback
 * @free_payload_list:        list head for payload
 * @evt_payload:              Event payload to be passed to tasklet
 * @res_type:                 CSID in resource type
 * @csi2_rx_cfg:              Csi2 rx decoder configuration for csid
 * @tpg_cfg:                  TPG configuration
 * @csi2_rx_reserve_cnt:      CSI2 reservations count value
 * @csi2_cfg_cnt:             csi2 configuration count
 * @tpg_start_cnt:            tpg start count
 * @ipp_res:                  image pixel path resource
 * @ppp_res:                  phase pxl path resource
 * @rdi_res:                  raw dump image path resources
 * @udi_res:                  udi path resources
 * @cid_res:                  cid resources state
 * @csid_top_reset_complete:  csid top reset completion
 * @csid_csi2_reset_complete: csi2 reset completion
 * @csid_ipp_reset_complete:  ipp reset completion
 * @csid_ppp_complete:        ppp reset completion
 * @csid_rdin_reset_complete: rdi n completion
 * @csid_udin_reset_complete: udi n completion
 * @csid_debug:               csid debug information to enable the SOT, EOT,
 *                            SOF, EOF, measure etc in the csid hw
 * @clk_rate                  Clock rate
 * @sof_irq_triggered:        Flag is set on receiving event to enable sof irq
 *                            incase of SOF freeze.
 * @irq_debug_cnt:            Counter to track sof irq's when above flag is set.
 * @error_irq_count           Error IRQ count, if continuous error irq comes
 *                            need to stop the CSID and mask interrupts.
 * @binning_enable            Flag is set if hardware supports QCFA binning
 * @binning_supported         Flag is set if sensor supports QCFA binning
 *
 * @prev_boot_timestamp       first bootime stamp at the start
 * @prev_qtimer_ts            stores csid timestamp
 * @fatal_err_detected        flag to indicate fatal errror is reported
 * @event_cb                  Callback to hw manager if CSID event reported
 */
struct cam_ife_csid_hw {
	struct cam_hw_intf              *hw_intf;
	struct cam_hw_info              *hw_info;
	struct cam_ife_csid_hw_info     *csid_info;
	void                            *tasklet;
	void                            *priv;
	struct list_head                 free_payload_list;
	struct cam_csid_evt_payload      evt_payload[CAM_CSID_EVT_PAYLOAD_MAX];
	uint32_t                         res_type;
	struct cam_ife_csid_csi2_rx_cfg  csi2_rx_cfg;
	struct cam_ife_csid_tpg_cfg      tpg_cfg;
	uint32_t                         csi2_reserve_cnt;
	uint32_t                         csi2_cfg_cnt;
	uint32_t                         tpg_start_cnt;
	struct cam_isp_resource_node     ipp_res;
	struct cam_isp_resource_node     ppp_res;
	struct cam_isp_resource_node     rdi_res[CAM_IFE_CSID_RDI_MAX];
	struct cam_isp_resource_node     udi_res[CAM_IFE_CSID_UDI_MAX];
	struct cam_isp_resource_node     cid_res[CAM_IFE_CSID_CID_MAX];
	struct completion                csid_top_complete;
	struct completion                csid_csi2_complete;
	struct completion                csid_ipp_complete;
	struct completion                csid_ppp_complete;
	struct completion    csid_rdin_complete[CAM_IFE_CSID_RDI_MAX];
	struct completion    csid_udin_complete[CAM_IFE_CSID_UDI_MAX];
	uint64_t                         csid_debug;
	uint64_t                         clk_rate;
	bool                             sof_irq_triggered;
	uint32_t                         irq_debug_cnt;
	uint32_t                         error_irq_count;
	uint32_t                         device_enabled;
	spinlock_t                       lock_state;
	uint32_t                         binning_enable;
	uint32_t                         binning_supported;
	uint64_t                         prev_boot_timestamp;
	uint64_t                         prev_qtimer_ts;
	bool                             fatal_err_detected;
	cam_hw_mgr_event_cb_func         event_cb;
};

int cam_ife_csid_hw_probe_init(struct cam_hw_intf  *csid_hw_intf,
	uint32_t csid_idx, bool is_custom);

int cam_ife_csid_hw_deinit(struct cam_ife_csid_hw *ife_csid_hw);

int cam_ife_csid_cid_reserve(struct cam_ife_csid_hw *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *cid_reserv);

int cam_ife_csid_path_reserve(struct cam_ife_csid_hw *csid_hw,
	struct cam_csid_hw_reserve_resource_args  *reserve);

#endif /* _CAM_IFE_CSID_HW_H_ */
