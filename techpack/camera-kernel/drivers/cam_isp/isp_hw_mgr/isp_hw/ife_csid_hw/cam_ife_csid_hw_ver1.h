/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_IFE_CSID_HW_VER1_H_
#define _CAM_IFE_CSID_HW_VER1_H_

#define IFE_CSID_VER1_RX_DL0_EOT_CAPTURED                  BIT(0)
#define IFE_CSID_VER1_RX_DL1_EOT_CAPTURED                  BIT(1)
#define IFE_CSID_VER1_RX_DL2_EOT_CAPTURED                  BIT(2)
#define IFE_CSID_VER1_RX_DL3_EOT_CAPTURED                  BIT(3)
#define IFE_CSID_VER1_RX_DL0_SOT_CAPTURED                  BIT(4)
#define IFE_CSID_VER1_RX_DL1_SOT_CAPTURED                  BIT(5)
#define IFE_CSID_VER1_RX_DL2_SOT_CAPTURED                  BIT(6)
#define IFE_CSID_VER1_RX_DL3_SOT_CAPTURED                  BIT(7)
#define IFE_CSID_VER1_RX_LONG_PKT_CAPTURED                 BIT(8)
#define IFE_CSID_VER1_RX_SHORT_PKT_CAPTURED                BIT(9)
#define IFE_CSID_VER1_RX_CPHY_PKT_HDR_CAPTURED             BIT(10)
#define IFE_CSID_VER1_RX_CPHY_EOT_RECEPTION                BIT(11)
#define IFE_CSID_VER1_RX_CPHY_SOT_RECEPTION                BIT(12)
#define IFE_CSID_VER1_RX_CPHY_PH_CRC                       BIT(13)
#define IFE_CSID_VER1_RX_WARNING_ECC                       BIT(14)
#define IFE_CSID_VER1_RX_LANE0_FIFO_OVERFLOW               BIT(15)
#define IFE_CSID_VER1_RX_LANE1_FIFO_OVERFLOW               BIT(16)
#define IFE_CSID_VER1_RX_LANE2_FIFO_OVERFLOW               BIT(17)
#define IFE_CSID_VER1_RX_LANE3_FIFO_OVERFLOW               BIT(18)
#define IFE_CSID_VER1_RX_ERROR_CRC                         BIT(19)
#define IFE_CSID_VER1_RX_ERROR_ECC                         BIT(20)
#define IFE_CSID_VER1_RX_MMAPPED_VC_DT                     BIT(21)
#define IFE_CSID_VER1_RX_UNMAPPED_VC_DT                    BIT(22)
#define IFE_CSID_VER1_RX_STREAM_UNDERFLOW                  BIT(23)
#define IFE_CSID_VER1_RX_UNBOUNDED_FRAME                   BIT(24)
#define IFE_CSID_VER1_RX_TG_DONE                           BIT(25)
#define IFE_CSID_VER1_RX_TG_FIFO_OVERFLOW                  BIT(26)
#define IFE_CSID_VER1_RX_RST_DONE                          BIT(27)

#define IFE_CSID_VER1_TOP_IRQ_DONE                         BIT(0)
#define IFE_CSID_VER1_PATH_INFO_RST_DONE                   BIT(1)
#define IFE_CSID_VER1_PATH_ERROR_FIFO_OVERFLOW             BIT(2)
#define IFE_CSID_VER1_PATH_INFO_SUBSAMPLED_EOF             BIT(3)
#define IFE_CSID_VER1_PATH_INFO_SUBSAMPLED_SOF             BIT(4)
#define IFE_CSID_VER1_PATH_INFO_FRAME_DROP_EOF             BIT(5)
#define IFE_CSID_VER1_PATH_INFO_FRAME_DROP_EOL             BIT(6)
#define IFE_CSID_VER1_PATH_INFO_FRAME_DROP_SOL             BIT(7)
#define IFE_CSID_VER1_PATH_INFO_FRAME_DROP_SOF             BIT(8)
#define IFE_CSID_VER1_PATH_INFO_INPUT_EOF                  BIT(9)
#define IFE_CSID_VER1_PATH_INFO_INPUT_EOL                  BIT(10)
#define IFE_CSID_VER1_PATH_INFO_INPUT_SOL                  BIT(11)
#define IFE_CSID_VER1_PATH_INFO_INPUT_SOF                  BIT(12)
#define IFE_CSID_VER1_PATH_ERROR_PIX_COUNT                 BIT(13)
#define IFE_CSID_VER1_PATH_ERROR_LINE_COUNT                BIT(14)
#define IFE_CSID_VER1_PATH_ERROR_CCIF_VIOLATION            BIT(15)
#define IFE_CSID_VER1_PATH_OVERFLOW_RECOVERY               BIT(17)

#define CAM_IFE_CSID_VER1_EVT_PAYLOAD_MAX  256

/*
 * struct cam_ife_csid_ver1_common_reg_info: Structure to hold Common info
 * holds register address offsets
 * shift values
 * masks
 */
struct cam_ife_csid_ver1_common_reg_info {
	/* MIPI CSID registers */
	uint32_t hw_version_addr;
	uint32_t cfg0_addr;
	uint32_t ctrl_addr;
	uint32_t reset_addr;
	uint32_t rst_strobes_addr;
	uint32_t test_bus_ctrl_addr;
	uint32_t top_irq_status_addr;
	uint32_t top_irq_mask_addr;
	uint32_t top_irq_clear_addr;
	uint32_t top_irq_set_addr;
	uint32_t irq_cmd_addr;

	/*Shift Bit Configurations*/
	uint32_t rst_done_shift_val;
	uint32_t timestamp_stb_sel_shift_val;
	uint32_t decode_format_shift_val;
	uint32_t path_en_shift_val;
	uint32_t dt_id_shift_val;
	uint32_t vc_shift_val;
	uint32_t dt_shift_val;
	uint32_t fmt_shift_val;
	uint32_t num_bytes_out_shift_val;
	uint32_t crop_shift_val;
	uint32_t debug_frm_drop_rst_shift_val;
	uint32_t debug_timestamp_rst_shift_val;
	uint32_t debug_format_measure_rst_shift_val;
	uint32_t debug_misr_rst_shift_val;
	uint32_t num_padding_pixels_shift_val;
	uint32_t num_padding_rows_shift_val;
	uint32_t fmt_measure_num_lines_shift_val;
	uint32_t num_vbi_lines_shift_val;
	uint32_t num_hbi_cycles_shift_val;
	uint32_t multi_vcdt_vc1_shift_val;
	uint32_t multi_vcdt_dt1_shift_val;
	uint32_t multi_vcdt_ts_combo_en_shift_val;
	uint32_t multi_vcdt_en_shift_val;

	/* config Values */
	uint32_t major_version;
	uint32_t minor_version;
	uint32_t version_incr;
	uint32_t num_udis;
	uint32_t num_rdis;
	uint32_t num_pix;
	uint32_t num_ppp;
	uint32_t rst_sw_reg_stb;
	uint32_t rst_hw_reg_stb;
	uint32_t rst_sw_hw_reg_stb;
	uint32_t path_rst_stb_all;
	uint32_t drop_supported;
	uint32_t multi_vcdt_supported;
	uint32_t timestamp_strobe_val;
	uint32_t early_eof_supported;
	uint32_t global_reset;
	uint32_t rup_supported;

	/* Masks */
	uint32_t ipp_irq_mask_all;
	uint32_t rdi_irq_mask_all;
	uint32_t ppp_irq_mask_all;
	uint32_t udi_irq_mask_all;
	uint32_t measure_en_hbi_vbi_cnt_mask;
	uint32_t measure_pixel_line_en_mask;
	uint32_t fmt_measure_num_line_mask;
	uint32_t fmt_measure_num_pxl_mask;
	uint32_t hblank_max_mask;
	uint32_t hblank_min_mask;
	uint32_t crop_pix_start_mask;
	uint32_t crop_pix_end_mask;
	uint32_t crop_line_start_mask;
	uint32_t crop_line_end_mask;
	uint32_t format_measure_height_mask_val;
	uint32_t format_measure_height_shift_val;
	uint32_t format_measure_width_mask_val;
	uint32_t format_measure_width_shift_val;
};

/*
 * struct cam_ife_csid_ver1_path_reg_info: Structure to hold PXL reg info
 * holds register address offsets
 * shift values
 * masks
 */
struct cam_ife_csid_ver1_path_reg_info {
	/* Pxl path register offsets*/
	uint32_t irq_status_addr;
	uint32_t irq_mask_addr;
	uint32_t irq_clear_addr;
	uint32_t irq_set_addr;

	uint32_t cfg0_addr;
	uint32_t cfg1_addr;
	uint32_t ctrl_addr;
	uint32_t frm_drop_pattern_addr;
	uint32_t frm_drop_period_addr;
	uint32_t irq_subsample_pattern_addr;
	uint32_t irq_subsample_period_addr;
	uint32_t hcrop_addr;
	uint32_t vcrop_addr;
	uint32_t pix_drop_pattern_addr;
	uint32_t pix_drop_period_addr;
	uint32_t line_drop_pattern_addr;
	uint32_t line_drop_period_addr;
	uint32_t rst_strobes_addr;
	uint32_t status_addr;
	uint32_t misr_val_addr;
	uint32_t misr_val0_addr;
	uint32_t misr_val1_addr;
	uint32_t misr_val2_addr;
	uint32_t misr_val3_addr;
	uint32_t format_measure_cfg0_addr;
	uint32_t format_measure_cfg1_addr;
	uint32_t format_measure0_addr;
	uint32_t format_measure1_addr;
	uint32_t format_measure2_addr;
	uint32_t yuv_chroma_conversion_addr;
	uint32_t timestamp_curr0_sof_addr;
	uint32_t timestamp_curr1_sof_addr;
	uint32_t timestamp_prev0_sof_addr;
	uint32_t timestamp_prev1_sof_addr;
	uint32_t timestamp_curr0_eof_addr;
	uint32_t timestamp_curr1_eof_addr;
	uint32_t timestamp_prev0_eof_addr;
	uint32_t timestamp_prev1_eof_addr;
	uint32_t err_recovery_cfg0_addr;
	uint32_t err_recovery_cfg1_addr;
	uint32_t err_recovery_cfg2_addr;
	uint32_t multi_vcdt_cfg0_addr;
	uint32_t byte_cntr_ping_addr;
	uint32_t byte_cntr_pong_addr;
	/* shift bit configuration */
	uint32_t timestamp_en_shift_val;
	uint32_t crop_v_en_shift_val;
	uint32_t crop_h_en_shift_val;
	uint32_t drop_v_en_shift_val;
	uint32_t drop_h_en_shift_val;
	uint32_t bin_h_en_shift_val;
	uint32_t bin_v_en_shift_val;
	uint32_t bin_en_shift_val;
	uint32_t bin_qcfa_en_shift_val;
	uint32_t halt_mode_master;
	uint32_t halt_mode_internal;
	uint32_t halt_mode_global;
	uint32_t halt_mode_slave;
	uint32_t halt_mode_shift;
	uint32_t halt_master_sel_master_val;
	uint32_t halt_master_sel_shift;
	uint32_t halt_frame_boundary;
	uint32_t resume_frame_boundary;
	uint32_t halt_immediate;
	uint32_t halt_cmd_shift;
	uint32_t mipi_pack_supported;
	uint32_t packing_fmt_shift_val;
	uint32_t format_measure_en_shift_val;
	uint32_t plain_fmt_shift_val;
	uint32_t packing_format;
	uint32_t pix_store_en_shift_val;
	uint32_t early_eof_en_shift_val;
	/* config Values */
	uint32_t ccif_violation_en;
	uint32_t binning_supported;
	uint32_t overflow_ctrl_mode_val;
	uint32_t overflow_ctrl_en;
	uint32_t fatal_err_mask;
	uint32_t non_fatal_err_mask;
};

/*
 * struct struct cam_ife_csid_ver1_tpg_reg_info: Structure to hold TPG reg info
 * holds register address offsets
 * shift values
 * masks
 */
struct cam_ife_csid_ver1_tpg_reg_info {
	uint32_t ctrl_addr;
	uint32_t vc_cfg0_addr;
	uint32_t vc_cfg1_addr;
	uint32_t lfsr_seed_addr;
	uint32_t dt_n_cfg_0_addr;
	uint32_t dt_n_cfg_1_addr;
	uint32_t dt_n_cfg_2_addr;
	uint32_t color_bars_cfg_addr;
	uint32_t color_box_cfg_addr;
	uint32_t common_gen_cfg_addr;
	uint32_t cgen_n_cfg_addr;
	uint32_t cgen_n_x0_addr;
	uint32_t cgen_n_x1_addr;
	uint32_t cgen_n_x2_addr;
	uint32_t cgen_n_xy_addr;
	uint32_t cgen_n_y1_addr;
	uint32_t cgen_n_y2_addr;

	/*configurations */
	uint32_t dtn_cfg_offset;
	uint32_t cgen_cfg_offset;
	uint32_t cpas_ife_reg_offset;
	uint32_t hbi;
	uint32_t vbi;
	uint32_t lfsr_seed;
	uint32_t width_shift;
	uint32_t height_shift;
	uint32_t fmt_shift;
	uint32_t color_bar;
	uint32_t line_interleave_mode;
	uint32_t payload_mode;
	uint32_t num_frames;
	uint32_t num_active_dt;
	uint32_t ctrl_cfg;
	uint32_t phy_sel;
	uint32_t num_frame_shift;
	uint32_t line_interleave_shift;
	uint32_t num_active_dt_shift;
	uint32_t vbi_shift;
	uint32_t hbi_shift;
	uint32_t color_bar_shift;
	uint32_t num_active_lanes_mask;
};

/*
 * struct cam_ife_csid_ver1_reg_info: Structure for Reg info
 *
 * @csi2_reg:            csi2 reg info
 * @ipp_reg:             IPP reg
 * @ppp_reg:             PPP reg
 * @rdi_reg:             RDI reg
 * @udi_reg:             UDI reg
 * @start_pixel:         start pixel for horizontal crop
 * @tpg_reg:             TPG reg
 * @cmn_reg:             Common reg info
 * @csid_cust_node_map:  Customer node map
 * @fused_max_width:     Max width based on fuse registers
 * @width_fuse_max_val:  Max value of width fuse
 * @used_hw_capabilities Hw capabilities
 */
struct cam_ife_csid_ver1_reg_info {
	struct cam_ife_csid_csi2_rx_reg_info        *csi2_reg;
	struct cam_ife_csid_ver1_path_reg_info      *ipp_reg;
	struct cam_ife_csid_ver1_path_reg_info      *ppp_reg;
	struct cam_ife_csid_ver1_path_reg_info      *rdi_reg
		[CAM_IFE_CSID_RDI_MAX];
	struct cam_ife_csid_ver1_path_reg_info      *udi_reg
		[CAM_IFE_CSID_UDI_MAX];
	struct cam_ife_csid_ver1_tpg_reg_info       *tpg_reg;
	struct cam_ife_csid_ver1_common_reg_info    *cmn_reg;
	const uint32_t                               csid_cust_node_map[
		CAM_IFE_CSID_HW_NUM_MAX];
	const uint32_t                               fused_max_width[
		CAM_IFE_CSID_WIDTH_FUSE_VAL_MAX];
	const uint32_t                               width_fuse_max_val;
};

/*
 * struct cam_ife_csid_ver1_path_cfg: place holder for path parameters
 *
 * @cid:                 cid value for path
 * @in_format:           input format
 * @out_format:          output format
 * @start_pixel:         start pixel for horizontal crop
 * @end_pixel:           end pixel for horizontal  crop
 * @start_line:          start line for vertical crop
 * @end_line:            end line for vertical crop
 * @width:               width of incoming data
 * @height:              height of incoming data
 * @master_idx:          master idx
 * @horizontal_bin:      horizontal binning enable/disable on path
 * @vertical_bin:        vertical binning enable/disable on path
 * @qcfa_bin    :        qcfa binning enable/disable on path
 * @hor_ver_bin :        horizontal vertical binning enable/disable on path
 * @num_bytes_out:       Number of bytes out
 * @sync_mode   :        Sync mode--> master/slave/none
 * @crop_enable:         flag to indicate crop enable
 * @drop_enable:         flag to indicate drop enable
 *
 */
struct cam_ife_csid_ver1_path_cfg {
	uint32_t                        cid;
	uint32_t                        in_format;
	uint32_t                        out_format;
	uint32_t                        start_pixel;
	uint32_t                        end_pixel;
	uint32_t                        width;
	uint32_t                        start_line;
	uint32_t                        end_line;
	uint32_t                        height;
	uint32_t                        master_idx;
	uint32_t                        horizontal_bin;
	uint32_t                        vertical_bin;
	uint32_t                        qcfa_bin;
	uint32_t                        hor_ver_bin;
	uint32_t                        num_bytes_out;
	enum cam_isp_hw_sync_mode       sync_mode;
	bool                            crop_enable;
	bool                            drop_enable;
};

/**
 * struct cam_csid_evt_payload- payload for csid hw event
 * @list       : list head
 * @irq_status : IRQ Status register
 * @priv       : Private data of payload
 * @evt_type   : Event type from CSID
 * @hw_idx     : Hw index
 */
struct cam_ife_csid_ver1_evt_payload {
	struct list_head   list;
	uint32_t           irq_status[CAM_IFE_CSID_IRQ_REG_MAX];
	void              *priv;
	uint32_t           evt_type;
	uint32_t           hw_idx;
};

/**
 * struct             cam_ife_csid_tpg_cfg- csid tpg configuration data
 * @width:            width
 * @height:           height
 * @test_pattern :    pattern
 * @encode_format:    decode format
 * @usage_type:       whether dual isp is required
 * @vc:               virtual channel
 * @dt:               data type
 *
 */
struct cam_ife_csid_ver1_tpg_cfg  {
	uint32_t                        width;
	uint32_t                        height;
	uint32_t                        test_pattern;
	uint32_t                        encode_format;
	uint32_t                        usage_type;
	uint32_t                        vc;
	uint32_t                        dt;
};

/*
 * struct cam_ife_csid_ver1_hw: place holder for csid hw
 *
 * @hw_intf:              hw intf
 * @hw_info:              hw info
 * @core_info:            csid core info
 * @tasklet:              tasklet to handle csid errors
 * @token:                private data to be sent with callback
 * @counters:             counters used in csid hw
 * @path_res:             array of path resources
 * @cid_data:             cid data
 * @log_buf:              Log Buffer to dump info
 * @rx_cfg:               rx configuration
 * @flags:                flags
 * @irq_complete:         complete variable for reset irq
 * @debug_info:           Debug info to capture debug info
 * @timestamp:            Timestamp to maintain evt timestamps
 * @free_payload_list:    List of free payload list
 * @evt_payload:          Event payload
 * @clk_rate:             clk rate for csid hw
 * @res_type:             cur res type for active hw
 * @lock_state :          spin lock
 *
 */
struct cam_ife_csid_ver1_hw {
	struct cam_hw_intf                            *hw_intf;
	struct cam_hw_info                            *hw_info;
	struct cam_ife_csid_core_info                 *core_info;
	void                                          *tasklet;
	void                                          *token;
	struct cam_ife_csid_hw_counters                counters;
	struct cam_ife_csid_ver1_tpg_cfg               tpg_cfg;
	struct cam_isp_resource_node                   path_res
		[CAM_IFE_PIX_PATH_RES_MAX];
	struct list_head                               free_payload_list;
	struct cam_ife_csid_ver1_evt_payload           evt_payload
		[CAM_IFE_CSID_VER1_EVT_PAYLOAD_MAX];
	struct completion                              irq_complete
		[CAM_IFE_CSID_IRQ_REG_MAX];
	struct cam_ife_csid_cid_data                   cid_data
		[CAM_IFE_CSID_CID_MAX];
	uint8_t                                        log_buf
		[CAM_IFE_CSID_LOG_BUF_LEN];
	struct cam_ife_csid_rx_cfg                     rx_cfg;
	struct cam_ife_csid_hw_flags                   flags;
	struct cam_ife_csid_debug_info                 debug_info;
	struct cam_ife_csid_timestamp                  timestamp;
	uint64_t                                       clk_rate;
	uint32_t                                       res_type;
	spinlock_t                                     lock_state;
	cam_hw_mgr_event_cb_func                       event_cb;
};

int cam_ife_csid_hw_ver1_init(struct cam_hw_intf  *csid_hw_intf,
	struct cam_ife_csid_core_info *csid_core_info,
	bool is_custom);

int cam_ife_csid_hw_ver1_deinit(struct cam_hw_info *hw_priv);

#endif
