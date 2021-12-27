/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_VFE_CAMIF_LITE_VER2_H_
#define _CAM_VFE_CAMIF_LITE_VER2_H_

#include "cam_isp_hw.h"
#include "cam_vfe_top.h"

#define CAM_VFE_CAMIF_LITE_EVT_MAX     256

struct cam_vfe_camif_lite_ver2_reg {
	uint32_t     camif_lite_cmd;
	uint32_t     camif_lite_config;
	uint32_t     lite_skip_period;
	uint32_t     lite_irq_subsample_pattern;
	uint32_t     lite_epoch_irq;
	uint32_t     reg_update_cmd;
};

struct cam_vfe_camif_lite_ver2_reg_data {
	uint32_t     dual_pd_reg_update_cmd_data;
	uint32_t     lite_epoch_line_cfg;
	uint32_t     lite_sof_irq_mask;
	uint32_t     lite_epoch0_irq_mask;
	uint32_t     dual_pd_reg_upd_irq_mask;
	uint32_t     lite_eof_irq_mask;
	uint32_t     lite_err_irq_mask0;
	uint32_t     lite_err_irq_mask1;
	uint32_t     lite_subscribe_irq_mask0;
	uint32_t     lite_subscribe_irq_mask1;
	uint32_t     extern_reg_update_shift;
	uint32_t     dual_pd_path_sel_shift;
};

struct cam_vfe_camif_lite_ver2_hw_info {
	struct cam_vfe_top_ver2_reg_offset_common   *common_reg;
	struct cam_vfe_camif_lite_ver2_reg          *camif_lite_reg;
	struct cam_vfe_camif_lite_ver2_reg_data     *reg_data;
};

int cam_vfe_camif_lite_ver2_acquire_resource(
	struct cam_isp_resource_node          *camif_lite_res,
	void                                  *acquire_param);

int cam_vfe_camif_lite_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_lite_hw_info,
	struct cam_isp_resource_node  *camif_lite_node,
	void                          *vfe_irq_controller);

int cam_vfe_camif_lite_ver2_deinit(
	struct cam_isp_resource_node  *camif_node);

#endif /* _CAM_VFE_CAMIF_LITE_VER2_H_ */
