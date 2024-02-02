/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _CAM_VFE_CAMIF_LITE_VER2_H_
#define _CAM_VFE_CAMIF_LITE_VER2_H_

#include "cam_isp_hw.h"
#include "cam_vfe_top.h"

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
	uint32_t     lite_error_irq_mask0;
	uint32_t     lite_error_irq_mask1;
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
	struct cam_isp_resource_node  *camif_lite_node);

int cam_vfe_camif_lite_ver2_deinit(
	struct cam_isp_resource_node  *camif_node);

#endif /* _CAM_VFE_CAMIF_LITE_VER2_H_ */
