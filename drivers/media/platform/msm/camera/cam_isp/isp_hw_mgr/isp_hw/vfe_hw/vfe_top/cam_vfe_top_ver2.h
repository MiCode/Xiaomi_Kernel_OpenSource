/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#ifndef _CAM_VFE_TOP_VER2_H_
#define _CAM_VFE_TOP_VER2_H_

#include "cam_vfe_camif_ver2.h"
#include "cam_vfe_camif_lite_ver2.h"
#include "cam_vfe_rdi.h"
#include "cam_vfe_fe_ver1.h"

#define CAM_VFE_TOP_VER2_MUX_MAX     6

#define CAM_VFE_MAX_REG_DUMP_ENTRIES 5

#define CAM_VFE_MAX_LUT_DUMP_ENTRIES 6

enum cam_vfe_top_ver2_module_type {
	CAM_VFE_TOP_VER2_MODULE_LENS,
	CAM_VFE_TOP_VER2_MODULE_STATS,
	CAM_VFE_TOP_VER2_MODULE_COLOR,
	CAM_VFE_TOP_VER2_MODULE_ZOOM,
	CAM_VFE_TOP_VER2_MODULE_MAX,
};

struct cam_vfe_top_ver2_reg_offset_module_ctrl {
	uint32_t reset;
	uint32_t cgc_ovd;
	uint32_t enable;
};

struct cam_vfe_top_reg_dump_entry {
	uint32_t reg_dump_start;
	uint32_t reg_dump_end;
};

struct cam_vfe_top_lut_dump_entry {
	uint32_t lut_word_size;
	uint32_t lut_bank_sel;
	uint32_t lut_addr_size;
};

struct cam_vfe_top_dump_data {
	uint32_t num_reg_dump_entries;
	uint32_t num_lut_dump_entries;
	uint32_t dmi_cfg;
	uint32_t dmi_addr;
	uint32_t dmi_data_path_hi;
	uint32_t dmi_data_path_lo;
	struct cam_vfe_top_reg_dump_entry
	    reg_entry[CAM_VFE_MAX_REG_DUMP_ENTRIES];
	struct cam_vfe_top_lut_dump_entry
	    lut_entry[CAM_VFE_MAX_LUT_DUMP_ENTRIES];
};

struct cam_vfe_top_ver2_reg_offset_common {
	uint32_t hw_version;
	uint32_t hw_capability;
	uint32_t lens_feature;
	uint32_t stats_feature;
	uint32_t color_feature;
	uint32_t zoom_feature;
	uint32_t global_reset_cmd;
	struct cam_vfe_top_ver2_reg_offset_module_ctrl
		*module_ctrl[CAM_VFE_TOP_VER2_MODULE_MAX];
	uint32_t bus_cgc_ovd;
	uint32_t core_cfg;
	uint32_t three_D_cfg;
	uint32_t violation_status;
	uint32_t reg_update_cmd;
};

struct cam_vfe_top_ver2_hw_info {
	struct cam_vfe_top_ver2_reg_offset_common  *common_reg;
	struct cam_vfe_camif_ver2_hw_info           camif_hw_info;
	struct cam_vfe_camif_lite_ver2_hw_info      camif_lite_hw_info;
	struct cam_vfe_rdi_ver2_hw_info             rdi_hw_info;
	struct cam_vfe_fe_ver1_hw_info              fe_hw_info;
	struct cam_vfe_top_dump_data                dump_data;
	uint32_t mux_type[CAM_VFE_TOP_VER2_MUX_MAX];
};

int cam_vfe_top_ver2_init(struct cam_hw_soc_info     *soc_info,
	struct cam_hw_intf                           *hw_intf,
	void                                         *top_hw_info,
	struct cam_vfe_top                          **vfe_top);

int cam_vfe_top_ver2_deinit(struct cam_vfe_top      **vfe_top);

#endif /* _CAM_VFE_TOP_VER2_H_ */
