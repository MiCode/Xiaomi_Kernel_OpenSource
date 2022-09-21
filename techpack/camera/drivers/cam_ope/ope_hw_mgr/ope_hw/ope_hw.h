/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_OPE_HW_H
#define CAM_OPE_HW_H

#define OPE_HW_VER_1_0_0 0x10000000
#define OPE_HW_VER_1_1_0 0x10010000

#define OPE_DEV_OPE  0
#define OPE_DEV_MAX  1

#define MAX_RD_CLIENTS   3
#define MAX_WR_CLIENTS   8
#define MAX_PP_CLIENTS   32

#define MAX_RW_CLIENTS   (MAX_RD_CLIENTS + MAX_WR_CLIENTS)

#define OPE_CDM_BASE     0x0
#define OPE_TOP_BASE     0x1
#define OPE_QOS_BASE     0x2
#define OPE_PP_BASE      0x3
#define OPE_BUS_RD       0x4
#define OPE_BUS_WR       0x5
#define OPE_BASE_MAX     0x6

#define BUS_RD_COMBO_BAYER_MASK   0x1
#define BUS_RD_COMBO_YUV_MASK     0x2
#define BUS_RD_COMBO_MAX          0x2

#define BUS_RD_BAYER         0x0
#define BUS_RD_YUV           0x1

#define BUS_WR_COMBO_YUV_MASK    0x1
#define BUS_WR_COMBO_MAX         0x1

#define BUS_WR_YUV           0x0

#define BUS_WR_VIDEO_Y       0x0
#define BUS_WR_VIDEO_C       0x1
#define BUS_WR_DISP_Y        0x2
#define BUS_WR_DISP_C        0x3
#define BUS_WR_ARGB          0x4
#define BUS_WR_STATS_RS      0x5
#define BUS_WR_STATS_IHIST   0x6
#define BUS_WR_STATS_LTM     0x7

#define OPE_WAIT_COMP_RUP     0x1
#define OPE_WAIT_COMP_WR_DONE 0x2
#define OPE_WAIT_COMP_IDLE    0x4
#define OPE_WAIT_COMP_GEN_IRQ 0x8

#define OPE_MAX_DEBUG_REGISTER 30

struct cam_ope_pid_mid_info {
	int cam_ope_res_type;
	uint32_t pid;
	uint32_t mid;
	bool read;
};

struct cam_ope_common {
	uint32_t mode[CAM_FORMAT_MAX];
	struct cam_ope_pid_mid_info (*ope_mid_info)[MAX_RW_CLIENTS];
};

struct cam_ope_top_reg {
	void *base;
	uint32_t offset;
	uint32_t hw_version;
	uint32_t reset_cmd;
	uint32_t core_clk_cfg_ctrl_0;
	uint32_t ahb_clk_cgc_ctrl;
	uint32_t core_cfg;
	uint32_t irq_status;
	uint32_t irq_mask;
	uint32_t irq_clear;
	uint32_t irq_set;
	uint32_t irq_cmd;
	uint32_t violation_status;
	uint32_t throttle_cnt_cfg;
	uint32_t debug_cfg;
	uint32_t scratch_reg;
	uint32_t num_debug_registers;
	struct cam_ope_debug_register *debug_regs;
};

struct cam_ope_top_reg_val {
	uint32_t hw_version;
	uint32_t major_mask;
	uint32_t major_shift;
	uint32_t minor_mask;
	uint32_t minor_shift;
	uint32_t incr_mask;
	uint32_t incr_shift;
	uint32_t irq_mask;
	uint32_t irq_set_clear;
	uint32_t sw_reset_cmd;
	uint32_t hw_reset_cmd;
	uint32_t core_clk_cfg_ctrl_0;
	uint32_t ahb_clk_cgc_ctrl;
	uint32_t input_format;
	uint32_t input_format_mask;
	uint32_t color_correct_src_sel;
	uint32_t color_correct_src_sel_mask;
	uint32_t stats_ihist_src_sel;
	uint32_t stats_ihist_src_sel_mask;
	uint32_t chroma_up_src_sel;
	uint32_t chroma_up_src_sel_mask;
	uint32_t argb_alpha;
	uint32_t argb_alpha_mask;
	uint32_t rs_throttle_cnt;
	uint32_t rs_throttle_cnt_mask;
	uint32_t ihist_throttle_cnt;
	uint32_t ihist_throttle_cnt_mask;
	uint32_t rst_done;
	uint32_t we_done;
	uint32_t fe_done;
	uint32_t ope_violation;
	uint32_t idle;
	uint32_t debug_cfg_val;
};

struct cam_ope_qos_reg {
	void *base;
	uint32_t offset;
	uint32_t hw_version;
	uint32_t hw_status;
	uint32_t module_cfg;
	uint32_t curve_cfg_0;
	uint32_t curve_cfg_1;
	uint32_t window_cfg;
	uint32_t eos_status_0;
	uint32_t eos_status_1;
	uint32_t eos_status_2;
};

struct cam_ope_qos_reg_val {
	uint32_t hw_version;
	uint32_t proc_interval;
	uint32_t proc_interval_mask;
	uint32_t static_health;
	uint32_t static_health_mask;
	uint32_t module_cfg_en;
	uint32_t module_cfg_en_mask;
	uint32_t yexp_ymin_dec;
	uint32_t yexp_ymin_dec_mask;
	uint32_t ymin_inc;
	uint32_t ymin_inc_mask;
	uint32_t initial_delta;
	uint32_t initial_delta_mask;
	uint32_t window_cfg;
};

struct cam_ope_bus_rd_client_reg {
	uint32_t core_cfg;
	uint32_t ccif_meta_data;
	uint32_t img_addr;
	uint32_t img_cfg;
	uint32_t stride;
	uint32_t unpack_cfg;
	uint32_t latency_buf_allocation;
	uint32_t misr_cfg_0;
	uint32_t misr_cfg_1;
	uint32_t misr_rd_val;
};

struct cam_ope_bus_rd_reg {
	void *base;
	uint32_t offset;
	uint32_t hw_version;
	uint32_t sw_reset;
	uint32_t cgc_override;
	uint32_t irq_mask;
	uint32_t irq_clear;
	uint32_t irq_cmd;
	uint32_t irq_status;
	uint32_t input_if_cmd;
	uint32_t irq_set;
	uint32_t misr_reset;
	uint32_t security_cfg;
	uint32_t iso_cfg;
	uint32_t iso_seed;

	uint32_t num_clients;
	struct cam_ope_bus_rd_client_reg rd_clients[MAX_RD_CLIENTS];
};

struct cam_ope_bus_rd_client_reg_val {
	uint32_t core_cfg;
	uint32_t stripe_location;
	uint32_t stripe_location_mask;
	uint32_t stripe_location_shift;
	uint32_t pix_pattern;
	uint32_t pix_pattern_mask;
	uint32_t pix_pattern_shift;
	uint32_t img_addr;
	uint32_t img_width;
	uint32_t img_width_mask;
	uint32_t img_width_shift;
	uint32_t img_height;
	uint32_t img_height_mask;
	uint32_t img_height_shift;
	uint32_t stride;
	uint32_t mode;
	uint32_t mode_mask;
	uint32_t mode_shift;
	uint32_t alignment;
	uint32_t alignment_mask;
	uint32_t alignment_shift;
	uint32_t latency_buf_allocation;
	uint32_t misr_cfg_samp_mode;
	uint32_t misr_cfg_samp_mode_mask;
	uint32_t misr_cfg_en;
	uint32_t misr_cfg_en_mask;
	uint32_t misr_cfg_1;
	uint32_t misr_rd_val;
	uint32_t input_port_id;
	uint32_t rm_port_id;
	uint32_t format_type;
	uint32_t num_combos_supported;
};

struct cam_ope_bus_rd_reg_val {
	uint32_t hw_version;
	uint32_t sw_reset;
	uint32_t cgc_override;
	uint32_t irq_mask;
	uint32_t go_cmd;
	uint32_t go_cmd_mask;
	uint32_t ica_en;
	uint32_t ica_en_mask;
	uint32_t static_prg;
	uint32_t static_prg_mask;
	uint32_t go_cmd_sel;
	uint32_t go_cmd_sel_mask;
	uint32_t fs_sync_en;
	uint32_t fs_sync_en_mask;
	uint32_t misr_reset;
	uint32_t security_cfg;
	uint32_t iso_bpp_select;
	uint32_t iso_bpp_select_mask;
	uint32_t iso_pattern_select;
	uint32_t iso_pattern_select_mask;
	uint32_t iso_en;
	uint32_t iso_en_mask;
	uint32_t iso_seed;
	uint32_t irq_set_clear;
	uint32_t rst_done;
	uint32_t rup_done;
	uint32_t rd_buf_done;
	uint32_t violation;
	uint32_t latency_buf_size;

	uint32_t num_clients;
	struct cam_ope_bus_rd_client_reg_val rd_clients[MAX_RD_CLIENTS];
};

struct cam_ope_bus_wr_client_reg {
	uint32_t core_cfg;
	uint32_t img_addr;
	uint32_t img_cfg;
	uint32_t x_init;
	uint32_t stride;
	uint32_t pack_cfg;
	uint32_t bw_limit;
	uint32_t frame_header_addr;
	uint32_t subsample_period;
	uint32_t subsample_pattern;
};

struct cam_ope_bus_wr_reg {
	void *base;
	uint32_t offset;
	uint32_t hw_version;
	uint32_t cgc_override;
	uint32_t irq_mask_0;
	uint32_t irq_mask_1;
	uint32_t irq_clear_0;
	uint32_t irq_clear_1;
	uint32_t irq_status_0;
	uint32_t irq_status_1;
	uint32_t irq_cmd;
	uint32_t frame_header_cfg_0;
	uint32_t local_frame_header_cfg_0;
	uint32_t irq_set_0;
	uint32_t irq_set_1;
	uint32_t iso_cfg;
	uint32_t violation_status;
	uint32_t image_size_violation_status;
	uint32_t misr_cfg_0;
	uint32_t misr_cfg_1;
	uint32_t misr_rd_sel;
	uint32_t misr_reset;
	uint32_t misr_val;
	uint32_t num_clients;
	struct cam_ope_bus_wr_client_reg wr_clients[MAX_WR_CLIENTS];
};

struct cam_ope_bus_wr_client_reg_val {
	uint32_t core_cfg_en;
	uint32_t core_cfg_en_mask;
	uint32_t core_cfg_en_shift;
	uint32_t virtual_frame_en;
	uint32_t virtual_frame_en_mask;
	uint32_t virtual_frame_en_shift;
	uint32_t frame_header_en;
	uint32_t frame_header_en_mask;
	uint32_t frame_header_en_shift;
	uint32_t auto_recovery_en;
	uint32_t auto_recovery_en_mask;
	uint32_t auto_recovery_en_shift;
	uint32_t mode;
	uint32_t mode_mask;
	uint32_t mode_shift;
	uint32_t img_addr;
	uint32_t width;
	uint32_t width_mask;
	uint32_t width_shift;
	uint32_t height;
	uint32_t height_mask;
	uint32_t height_shift;
	uint32_t x_init;
	uint32_t stride;
	uint32_t format;
	uint32_t format_mask;
	uint32_t format_shift;
	uint32_t alignment;
	uint32_t alignment_mask;
	uint32_t alignment_shift;
	uint32_t bw_limit_en;
	uint32_t bw_limit_en_mask;
	uint32_t bw_limit_counter;
	uint32_t bw_limit_counter_mask;
	uint32_t frame_header_addr;
	uint32_t subsample_period;
	uint32_t subsample_pattern;
	uint32_t output_port_id;
	uint32_t wm_port_id;
	uint32_t format_type;
	uint32_t num_combos_supported;
};

struct cam_ope_bus_wr_reg_val {
	uint32_t hw_version;
	uint32_t cgc_override;
	uint32_t irq_mask_0;
	uint32_t irq_mask_1;
	uint32_t irq_set_clear;
	uint32_t comp_rup_done;
	uint32_t comp_buf_done;
	uint32_t cons_violation;
	uint32_t violation;
	uint32_t img_size_violation;
	uint32_t frame_header_cfg_0;
	uint32_t local_frame_header_cfg_0;
	uint32_t iso_cfg;
	uint32_t misr_0_en;
	uint32_t misr_0_en_mask;
	uint32_t misr_1_en;
	uint32_t misr_1_en_mask;
	uint32_t misr_2_en;
	uint32_t misr_2_en_mask;
	uint32_t misr_3_en;
	uint32_t misr_3_en_mask;
	uint32_t misr_0_samp_mode;
	uint32_t misr_0_samp_mode_mask;
	uint32_t misr_1_samp_mode;
	uint32_t misr_1_samp_mode_mask;
	uint32_t misr_2_samp_mode;
	uint32_t misr_2_samp_mode_mask;
	uint32_t misr_3_samp_mode;
	uint32_t misr_3_samp_mode_mask;
	uint32_t misr_0_id;
	uint32_t misr_0_id_mask;
	uint32_t misr_1_id;
	uint32_t misr_1_id_mask;
	uint32_t misr_2_id;
	uint32_t misr_2_id_mask;
	uint32_t misr_3_id;
	uint32_t misr_3_id_mask;
	uint32_t misr_rd_misr_sel;
	uint32_t misr_rd_misr_sel_mask;
	uint32_t misr_rd_word_sel;
	uint32_t misr_rd_word_sel_mask;
	uint32_t misr_reset;
	uint32_t misr_val;


	uint32_t num_clients;
	struct cam_ope_bus_wr_client_reg_val wr_clients[MAX_WR_CLIENTS];
};

struct cam_ope_debug_register {
	uint32_t offset;
};

struct cam_ope_bus_pp_client_reg {
	uint32_t hw_status;
};

struct cam_ope_pp_reg {
	void *base;
	uint32_t offset;

	uint32_t num_clients;
	struct cam_ope_bus_pp_client_reg pp_clients[MAX_PP_CLIENTS];
};

struct ope_hw {
	struct cam_ope_top_reg        *top_reg;
	struct cam_ope_top_reg_val    *top_reg_val;

	struct cam_ope_bus_rd_reg     *bus_rd_reg;
	struct cam_ope_bus_rd_reg_val *bus_rd_reg_val;

	struct cam_ope_bus_wr_reg     *bus_wr_reg;
	struct cam_ope_bus_wr_reg_val *bus_wr_reg_val;

	struct cam_ope_qos_reg        *qos_reg;
	struct cam_ope_qos_reg_val    *qos_reg_val;

	struct cam_ope_common         *common;

	struct cam_ope_pp_reg        *pp_reg;
};

struct hw_version_reg {
	uint32_t hw_ver;
	uint32_t reserved;
};

#endif /* CAM_OPE_HW_H */
