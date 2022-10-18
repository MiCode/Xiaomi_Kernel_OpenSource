/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_SFE_TOP_H_
#define _CAM_SFE_TOP_H_

#include "cam_cpas_api.h"

#define CAM_SFE_PIX_VER_1_0    0x10
#define CAM_SFE_RDI_VER_1_0    0x1000
#define CAM_SFE_TOP_VER_1_0    0x10000

#define CAM_SFE_TOP_IN_PORT_MAX                6
#define CAM_SFE_RDI_MAX                        5

#define CAM_SHIFT_TOP_CORE_CFG_MODE_SEL        2
#define CAM_SHIFT_TOP_CORE_CFG_OPS_MODE_CFG    1
#define CAM_SHIFT_TOP_CORE_CFG_FS_MODE_CFG     0

#define CAM_SFE_TOP_DBG_REG_MAX                18
#define CAM_SFE_TOP_TESTBUS_MAX                 2

struct cam_sfe_top_module_desc {
	uint32_t id;
	uint8_t *desc;
};

struct cam_sfe_top {
	void                   *top_priv;
	struct cam_hw_ops       hw_ops;
};

struct cam_sfe_mode {
	int  value;
	char  *desc;
};

struct cam_sfe_wr_client_desc {
	uint32_t  wm_id;
	uint8_t  *desc;
};

struct cam_sfe_top_err_irq_desc {
	uint32_t  bitmask;
	char     *err_name;
	char     *desc;
};

struct cam_sfe_top_debug_info {
	uint32_t  shift;
	char     *clc_name;
};

struct cam_sfe_top_cc_testbus_info {
	uint32_t  mask;
	uint32_t  shift;
	char     *clc_name;
};

struct cam_sfe_testbus_info {
	uint32_t  debugfs_val;
	bool      enable;
	uint32_t  value;
	uint32_t  size;
	struct cam_sfe_top_cc_testbus_info *testbus;
};

struct cam_sfe_top_common_reg_offset {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t stats_feature;
	uint32_t core_cgc_ctrl;
	uint32_t ahb_clk_ovd;
	uint32_t core_cfg;
	uint32_t ipp_violation_status;
	uint32_t diag_config;
	uint32_t diag_sensor_status_0;
	uint32_t diag_sensor_status_1;
	uint32_t diag_sensor_frame_cnt_status0;
	uint32_t diag_sensor_frame_cnt_status1;
	uint32_t stats_ch2_throttle_cfg;
	uint32_t stats_ch1_throttle_cfg;
	uint32_t stats_ch0_throttle_cfg;
	uint32_t lcr_throttle_cfg;
	uint32_t hdr_throttle_cfg;
	uint32_t sfe_op_throttle_cfg;
	uint32_t irc_throttle_cfg;
	uint32_t sfe_single_dual_cfg;
	uint32_t bus_overflow_status;
	uint32_t top_debug_cfg;
	uint32_t top_cc_test_bus_ctrl;
	bool     lcr_supported;
	bool     ir_supported;
	bool     qcfa_only;
	struct   cam_sfe_mode *sfe_mode;
	uint32_t num_sfe_mode;
	uint32_t ipp_violation_mask;
	uint32_t top_debug_testbus_reg;
	uint32_t top_cc_test_bus_supported;
	uint32_t num_debug_registers;
	uint32_t top_debug[CAM_SFE_TOP_DBG_REG_MAX];
};

struct cam_sfe_modules_common_reg_offset {
	uint32_t demux_module_cfg;
	uint32_t demux_xcfa_cfg;
	uint32_t demux_hdr_cfg;
	uint32_t demux_lcr_sel;
	uint32_t hdrc_remo_mod_cfg;
	uint32_t hdrc_remo_xcfa_bin_cfg;
	uint32_t xcfa_hdrc_remo_out_mux_cfg;
};

struct cam_sfe_top_common_reg_data {
	uint32_t error_irq_mask;
	uint32_t enable_diagnostic_hw;
	uint32_t top_debug_cfg_en;
	uint32_t sensor_sel_shift;
};

struct cam_sfe_path_common_reg_data {
	uint32_t sof_irq_mask;
	uint32_t eof_irq_mask;
	uint32_t subscribe_irq_mask;
};

struct cam_sfe_top_hw_info {
	struct cam_sfe_top_common_reg_offset     *common_reg;
	struct cam_sfe_modules_common_reg_offset *modules_hw_info;
	struct cam_sfe_top_common_reg_data       *common_reg_data;
	struct cam_sfe_top_module_desc           *ipp_module_desc;
	struct cam_sfe_wr_client_desc            *wr_client_desc;
	struct cam_sfe_path_common_reg_data      *pix_reg_data;
	struct cam_sfe_path_common_reg_data      *rdi_reg_data[CAM_SFE_RDI_MAX];
	uint32_t                                  num_inputs;
	uint32_t                                  input_type[
		CAM_SFE_TOP_IN_PORT_MAX];
	uint32_t                                  num_top_errors;
	struct cam_sfe_top_err_irq_desc          *top_err_desc;
	uint32_t                                  num_clc_module;
	struct   cam_sfe_top_debug_info         (*clc_dbg_mod_info)[CAM_SFE_TOP_DBG_REG_MAX][8];
	uint32_t                                  num_of_testbus;
	struct cam_sfe_testbus_info               test_bus_info[CAM_SFE_TOP_TESTBUS_MAX];
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

#define SFE_DBG_INFO(shift_val, name) {.shift = shift_val, .clc_name = name}

#define SFE_DBG_INFO_ARRAY_4bit(name1, name2, name3, name4, name5, name6, name7, name8) \
	{                                                                               \
		SFE_DBG_INFO(0, name1),                                                 \
		SFE_DBG_INFO(4, name2),                                                 \
		SFE_DBG_INFO(8, name3),                                                 \
		SFE_DBG_INFO(12, name4),                                                \
		SFE_DBG_INFO(16, name5),                                                \
		SFE_DBG_INFO(20, name6),                                                \
		SFE_DBG_INFO(24, name7),                                                \
		SFE_DBG_INFO(28, name8),                                                \
	}

#endif /* _CAM_SFE_TOP_H_ */
