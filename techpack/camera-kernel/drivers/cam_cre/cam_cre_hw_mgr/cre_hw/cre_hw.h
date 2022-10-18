/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef CAM_CRE_HW_H
#define CAM_CRE_HW_H

#define CRE_HW_VER_1_0_0 0x10000000

#define CRE_DEV_CRE  0
#define CRE_DEV_MAX  1

#define MAX_CRE_RD_CLIENTS   1
#define MAX_CRE_WR_CLIENTS   1

#define CRE_TOP_BASE     0x0
#define CRE_BUS_RD       0x1
#define CRE_BUS_WR       0x2
#define CRE_BASE_MAX     0x3

#define CRE_WAIT_BUS_WR_RUP    0x1
#define CRE_WAIT_BUS_WR_DONE   0x2
#define CRE_WAIT_BUS_RD_DONE   0x3
#define CRE_WAIT_IDLE_IRQ      0x4

struct cam_cre_top_reg {
	void *base;
	uint32_t offset;
	uint32_t hw_version;
	uint32_t hw_cap;
	uint32_t debug_0;
	uint32_t debug_1;
	uint32_t debug_cfg;
	uint32_t testbus_ctrl;
	uint32_t scratch_0;
	uint32_t irq_status;
	uint32_t irq_mask;
	uint32_t irq_clear;
	uint32_t irq_set;
	uint32_t irq_cmd;
	uint32_t reset_cmd;
	uint32_t core_clk_cfg_ctrl_0;
	uint32_t core_clk_cfg_ctrl_1;
	uint32_t top_spare;
};

struct cam_cre_top_reg_val {
	uint32_t hw_version;
	uint32_t hw_cap;
	uint32_t major_mask;
	uint32_t major_shift;
	uint32_t minor_mask;
	uint32_t minor_shift;
	uint32_t irq_mask;
	uint32_t irq_set;
	uint32_t irq_clear;
	uint32_t irq_cmd_set;
	uint32_t irq_cmd_clear;
	uint32_t idle;
	uint32_t fe_done;
	uint32_t we_done;
	uint32_t rst_done;
	uint32_t sw_reset_cmd;
	uint32_t hw_reset_cmd;
	uint32_t core_clk_cfg_ctrl_0;
	uint32_t core_clk_cfg_ctrl_1;
	uint32_t top_override;
	uint32_t we_override;
	uint32_t fe_override;
	uint32_t ahb_override;
};

struct cam_cre_bus_rd_client_reg {
	uint32_t bus_ctrl;
	uint32_t spare;
	uint32_t cons_violation;
	uint32_t cons_violation_status;
	uint32_t core_cfg;
	uint32_t ccif_meta_data;
	uint32_t img_addr;
	uint32_t rd_width;
	uint32_t rd_height;
	uint32_t rd_stride;
	uint32_t unpacker_cfg;
	uint32_t latency_buf_allocation;
	uint32_t misr_cfg_0;
	uint32_t misr_cfg_1;
	uint32_t misr_rd_val;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
	uint32_t read_buff_cfg;
	uint32_t addr_cfg;
};

struct cam_cre_bus_rd_reg {
	void *base;
	uint32_t offset;
	uint32_t hw_version;
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
	uint32_t test_bus_ctrl;

	uint32_t num_clients;
	struct   cam_cre_bus_rd_client_reg rd_clients[MAX_CRE_RD_CLIENTS];
};

struct cam_cre_bus_rd_client_reg_val {
	uint32_t client_en;
	uint32_t ai_en;
	uint32_t ai_en_mask;
	uint32_t ai_en_shift;
	uint32_t pix_pattern;
	uint32_t pix_pattern_mask;
	uint32_t pix_pattern_shift;
	uint32_t stripe_location;
	uint32_t stripe_location_mask;
	uint32_t stripe_location_shift;
	uint32_t img_addr;
	uint32_t img_width;
	uint32_t img_height;
	uint32_t stride;
	uint32_t alignment;
	uint32_t alignment_mask;
	uint32_t alignment_shift;
	uint32_t format;
	uint32_t format_mask;
	uint32_t format_shift;
	uint32_t latency_buf_size;
	uint32_t latency_buf_size_mask;
	uint32_t misr_cfg_en;
	uint32_t misr_cfg_en_mask;
	uint32_t misr_cfg_samp_mode;
	uint32_t misr_cfg_samp_mode_mask;
	uint32_t misr_cfg_1;
	uint32_t misr_rd_val;
	uint32_t x_int;
	uint32_t x_int_mask;
	uint32_t byte_offset;
	uint32_t byte_offset_mask;
	uint32_t input_port_id;
	uint32_t rm_port_id;
};

struct cam_cre_bus_rd_reg_val {
	uint32_t hw_version;
	uint32_t cgc_override;
	uint32_t irq_mask;
	uint32_t irq_status;
	uint32_t irq_cmd_set;
	uint32_t irq_cmd_clear;
	uint32_t rup_done;
	uint32_t rd_buf_done;
	uint32_t cons_violation;
	uint32_t static_prg;
	uint32_t static_prg_mask;
	uint32_t ica_en;
	uint32_t ica_en_mask;
	uint32_t go_cmd;
	uint32_t go_cmd_mask;
	uint32_t irq_set;
	uint32_t irq_clear;
	uint32_t misr_reset;
	uint32_t security_cfg;
	uint32_t iso_bpp_select;
	uint32_t iso_bpp_select_mask;
	uint32_t iso_pattern_select;
	uint32_t iso_pattern_select_mask;
	uint32_t iso_en;
	uint32_t iso_en_mask;
	uint32_t iso_seed;
	uint32_t bus_ctrl;
	uint32_t spare;
	uint32_t num_clients;
	struct   cam_cre_bus_rd_client_reg_val rd_clients[MAX_CRE_RD_CLIENTS];
};

struct cam_cre_bus_wr_client_reg {
	uint32_t client_cfg;
	uint32_t img_addr;
	uint32_t img_cfg_0;
	uint32_t img_cfg_1;
	uint32_t img_cfg_2;
	uint32_t bw_limit;
	uint32_t packer_cfg;
	uint32_t addr_cfg;
	uint32_t debug_status_cfg;
	uint32_t debug_status_0;
	uint32_t debug_status_1;
};

struct cam_cre_bus_wr_reg {
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
	uint32_t perf_count_cfg_0;
	uint32_t perf_count_cfg_1;
	uint32_t perf_count_cfg_2;
	uint32_t perf_count_cfg_3;
	uint32_t perf_count_val_0;
	uint32_t perf_count_val_1;
	uint32_t perf_count_val_2;
	uint32_t perf_count_val_3;
	uint32_t perf_count_status;
	uint32_t misr_cfg_0;
	uint32_t misr_cfg_1;
	uint32_t misr_rd_sel;
	uint32_t misr_reset;
	uint32_t misr_val;

	uint32_t num_clients;
	struct   cam_cre_bus_wr_client_reg wr_clients[MAX_CRE_WR_CLIENTS];
};

struct cam_cre_bus_wr_client_reg_val {
	uint32_t client_en;
	uint32_t client_en_mask;
	uint32_t client_en_shift;
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
	uint32_t x_init_mask;
	uint32_t stride;
	uint32_t stride_mask;
	uint32_t client_buf_done;
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
	uint32_t output_port_id;
	uint32_t wm_port_id;
};

struct cam_cre_bus_wr_reg_val {
	uint32_t hw_version;
	uint32_t cgc_override;
	uint32_t irq_mask_0;
	uint32_t irq_set_0;
	uint32_t irq_clear_0;
	uint32_t irq_status_0;
	uint32_t img_size_violation;
	uint32_t violation;
	uint32_t cons_violation;
	uint32_t comp_buf_done;
	uint32_t comp_rup_done;
	uint32_t irq_mask_1;
	uint32_t irq_set_1;
	uint32_t irq_clear_1;
	uint32_t irq_status_1;
	uint32_t irq_cmd_set;
	uint32_t irq_cmd_clear;
	uint32_t frame_header_cfg_0;
	uint32_t local_frame_header_cfg_0;
	uint32_t iso_en;
	uint32_t iso_en_mask;
	uint32_t violation_status;
	uint32_t img_size_violation_status;
	uint32_t misr_0_en;
	uint32_t misr_0_en_mask;
	uint32_t misr_0_samp_mode;
	uint32_t misr_0_samp_mode_mask;
	uint32_t misr_0_id;
	uint32_t misr_0_id_mask;
	uint32_t misr_rd_misr_sel;
	uint32_t misr_rd_misr_sel_mask;
	uint32_t misr_rd_word_sel;
	uint32_t misr_rd_word_sel_mask;
	uint32_t misr_reset;
	uint32_t misr_val;

	uint32_t num_clients;
	struct cam_cre_bus_wr_client_reg_val wr_clients[MAX_CRE_WR_CLIENTS];
};

struct cam_cre_debug_register {
	uint32_t offset;
};

struct cam_cre_hw {
	struct cam_cre_top_reg        *top_reg_offset;
	struct cam_cre_top_reg_val    *top_reg_val;

	struct cam_cre_bus_rd_reg     *bus_rd_reg_offset;
	struct cam_cre_bus_rd_reg_val *bus_rd_reg_val;

	struct cam_cre_bus_wr_reg     *bus_wr_reg_offset;
	struct cam_cre_bus_wr_reg_val *bus_wr_reg_val;

	struct cam_cre_common         *common;
};

struct cre_hw_version_reg {
	uint32_t hw_ver;
	uint32_t reserved;
};

#endif /* CAM_CRE_HW_H */
