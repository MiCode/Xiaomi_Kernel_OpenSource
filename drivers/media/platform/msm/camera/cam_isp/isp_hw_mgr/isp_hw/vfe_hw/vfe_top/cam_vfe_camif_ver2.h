/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#ifndef _CAM_VFE_CAMIF_VER2_H_
#define _CAM_VFE_CAMIF_VER2_H_

#include "cam_isp_hw.h"
#include "cam_vfe_top.h"

struct cam_vfe_camif_ver2_reg {
	uint32_t     camif_cmd;
	uint32_t     camif_config;
	uint32_t     line_skip_pattern;
	uint32_t     pixel_skip_pattern;
	uint32_t     skip_period;
	uint32_t     irq_subsample_pattern;
	uint32_t     epoch_irq;
	uint32_t     raw_crop_width_cfg;
	uint32_t     raw_crop_height_cfg;
	uint32_t     reg_update_cmd;
};

struct cam_vfe_camif_reg_data {
	uint32_t     raw_crop_first_pixel_shift;
	uint32_t     raw_crop_first_pixel_mask;

	uint32_t     raw_crop_last_pixel_shift;
	uint32_t     raw_crop_last_pixel_mask;

	uint32_t     raw_crop_first_line_shift;
	uint32_t     raw_crop_first_line_mask;

	uint32_t     raw_crop_last_line_shift;
	uint32_t     raw_crop_last_line_mask;

	uint32_t     input_mux_sel_shift;
	uint32_t     input_mux_sel_mask;
	uint32_t     extern_reg_update_shift;
	uint32_t     extern_reg_update_mask;

	uint32_t     pixel_pattern_shift;
	uint32_t     pixel_pattern_mask;

	uint32_t     dsp_mode_shift;
	uint32_t     dsp_mode_mask;
	uint32_t     dsp_en_shift;
	uint32_t     dsp_en_mask;

	uint32_t     reg_update_cmd_data;
	uint32_t     epoch_line_cfg;
	uint32_t     sof_irq_mask;
	uint32_t     epoch0_irq_mask;
	uint32_t     reg_update_irq_mask;
	uint32_t     eof_irq_mask;
	uint32_t     error_irq_mask0;
	uint32_t     error_irq_mask1;
};

struct cam_vfe_camif_ver2_hw_info {
	struct cam_vfe_top_ver2_reg_offset_common   *common_reg;
	struct cam_vfe_camif_ver2_reg               *camif_reg;
	struct cam_vfe_camif_reg_data               *reg_data;
};

int cam_vfe_camif_ver2_acquire_resource(
	struct cam_isp_resource_node  *camif_res,
	void                          *acquire_param);

int cam_vfe_camif_ver2_init(
	struct cam_hw_intf            *hw_intf,
	struct cam_hw_soc_info        *soc_info,
	void                          *camif_hw_info,
	struct cam_isp_resource_node  *camif_node);

int cam_vfe_camif_ver2_deinit(
	struct cam_isp_resource_node  *camif_node);

#endif /* _CAM_VFE_CAMIF_VER2_H_ */
