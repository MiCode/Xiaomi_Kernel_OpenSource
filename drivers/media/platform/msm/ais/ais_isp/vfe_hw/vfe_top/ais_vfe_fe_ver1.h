/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#ifndef _AIS_VFE_FE_VER1_H_
#define _AIS_VFE_FE_VER1_H_

#include "ais_vfe_top.h"

struct ais_vfe_fe_ver1_reg {
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
	uint32_t     vfe_diag_config;
	uint32_t     vfe_diag_sensor_status;
	uint32_t     fe_cfg;
};

struct ais_vfe_fe_reg_data {
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

	uint32_t     enable_diagnostic_hw;
	uint32_t     fe_mux_data;
	uint32_t     hbi_cnt_shift;
};

struct ais_vfe_fe_ver1_hw_info {
	struct ais_vfe_top_ver2_reg_offset_common   *common_reg;
	struct ais_vfe_fe_ver1_reg              *fe_reg;
	struct ais_vfe_fe_reg_data              *reg_data;
};

#endif /* _AIS_VFE_FE_VER1_H_ */
