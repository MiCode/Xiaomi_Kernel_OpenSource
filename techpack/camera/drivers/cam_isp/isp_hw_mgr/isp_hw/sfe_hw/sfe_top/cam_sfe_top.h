/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_TOP_H_
#define _CAM_SFE_TOP_H_

#include "cam_cpas_api.h"

#define CAM_SFE_PIX_VER_1_0    0x10
#define CAM_SFE_RDI_VER_1_0    0x1000
#define CAM_SFE_TOP_VER_1_0    0x10000

#define CAM_SFE_DELAY_BW_REDUCTION_NUM_FRAMES  18
#define CAM_SFE_TOP_IN_PORT_MAX                6
#define CAM_SFE_RDI_MAX                        5

#define CAM_SHIFT_TOP_CORE_CFG_MODE_SEL        2
#define CAM_SHIFT_TOP_CORE_CFG_OPS_MODE_CFG    1
#define CAM_SHIFT_TOP_CORE_CFG_FS_MODE_CFG     0

struct cam_sfe_top {
	void                   *top_priv;
	struct cam_hw_ops       hw_ops;
};

struct cam_sfe_top_common_reg_offset {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t stats_feature;
	uint32_t core_cgc_ctrl;
	uint32_t ahb_clk_ovd;
	uint32_t core_cfg;
	uint32_t violation_status;
	uint32_t diag_config;
	uint32_t diag_sensor_status_0;
	uint32_t diag_sensor_status_1;
	uint32_t diag_sensor_frame_cnt_status0;
	uint32_t diag_sensor_frame_cnt_status1;
	uint32_t top_debug_0;
	uint32_t top_debug_1;
	uint32_t top_debug_2;
	uint32_t top_debug_3;
	uint32_t top_debug_4;
	uint32_t top_debug_5;
	uint32_t top_debug_6;
	uint32_t top_debug_7;
	uint32_t top_debug_8;
	uint32_t top_debug_9;
	uint32_t top_debug_10;
	uint32_t top_debug_11;
	uint32_t top_debug_cfg;
	uint32_t stats_ch2_throttle_cfg;
	uint32_t stats_ch1_throttle_cfg;
	uint32_t stats_ch0_throttle_cfg;
	uint32_t lcr_throttle_cfg;
	uint32_t hdr_throttle_cfg;
	uint32_t sfe_op_throttle_cfg;
};

struct cam_sfe_modules_common_reg_offset {
	uint32_t demux_module_cfg;
	uint32_t demux_qcfa_cfg;
	uint32_t demux_hdr_cfg;
	uint32_t demux_lcr_sel;
	uint32_t hdrc_remo_mod_cfg;
	uint32_t hdrc_remo_qcfa_bin_cfg;
	uint32_t qcfa_hdrc_remo_out_mux_cfg;
};

struct cam_sfe_path_common_reg_data {
	uint32_t sof_irq_mask;
	uint32_t eof_irq_mask;
	uint32_t error_irq_mask;
	uint32_t subscribe_irq_mask;
	uint32_t enable_diagnostic_hw;
	uint32_t top_debug_cfg_en;
};

struct cam_sfe_top_hw_info {
	struct cam_sfe_top_common_reg_offset     *common_reg;
	struct cam_sfe_modules_common_reg_offset *modules_hw_info;
	struct cam_sfe_path_common_reg_data      *pix_reg_data;
	struct cam_sfe_path_common_reg_data      *rdi_reg_data[CAM_SFE_RDI_MAX];
	uint32_t                                  num_inputs;
	uint32_t                                  input_type[
		CAM_SFE_TOP_IN_PORT_MAX];
};

int cam_sfe_top_init(
	uint32_t                            hw_version,
	struct cam_hw_soc_info             *soc_info,
	struct cam_hw_intf                 *hw_intf,
	void                               *top_hw_info,
	void                               *sfe_irq_controller,
	struct cam_sfe_top                **sfe_top);

int cam_sfe_top_deinit(
	uint32_t                       hw_version,
	struct cam_sfe_top           **sfe_top);

#endif /* _CAM_SFE_TOP_H_ */
