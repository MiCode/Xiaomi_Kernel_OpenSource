/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_CAMIF_LITE_VER3_H_
#define _CAM_VFE_CAMIF_LITE_VER3_H_

#include "cam_isp_hw.h"
#include "cam_vfe_top.h"

struct cam_vfe_camif_lite_ver3_reg {
	uint32_t     lite_hw_version;
	uint32_t     lite_hw_status;
	uint32_t     lite_module_config;
	uint32_t     lite_skip_period;
	uint32_t     lite_irq_subsample_pattern;
	uint32_t     lite_epoch_irq;
	uint32_t     lite_debug_1;
	uint32_t     lite_debug_0;
	uint32_t     lite_test_bus_ctrl;
	uint32_t     camif_lite_spare;
	uint32_t     reg_update_cmd;
};

struct cam_vfe_camif_lite_ver3_reg_data {
	uint32_t     extern_reg_update_shift;
	uint32_t     reg_update_cmd_data;
	uint32_t     epoch_line_cfg;
	uint32_t     sof_irq_mask;
	uint32_t     epoch0_irq_mask;
	uint32_t     epoch1_irq_mask;
	uint32_t     eof_irq_mask;
	uint32_t     error_irq_mask0;
	uint32_t     error_irq_mask2;
	uint32_t     enable_diagnostic_hw;
};

struct cam_vfe_camif_lite_ver3_hw_info {
	struct cam_vfe_top_ver3_reg_offset_common   *common_reg;
	struct cam_vfe_camif_lite_ver3_reg          *camif_lite_reg;
	struct cam_vfe_camif_lite_ver3_reg_data     *reg_data;
};

int cam_vfe_camif_lite_ver3_acquire_resource(
	struct cam_isp_resource_node          *camif_lite_res,
	void                                  *acquire_param);

int cam_vfe_camif_lite_ver3_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_lite_hw_info,
	struct cam_isp_resource_node  *camif_lite_node);

int cam_vfe_camif_lite_ver3_deinit(
	struct cam_isp_resource_node  *camif_node);

#endif /* _CAM_VFE_CAMIF_LITE_VER3_H_ */

