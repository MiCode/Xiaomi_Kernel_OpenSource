/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _CAM_VFE_TOP_VER2_H_
#define _CAM_VFE_TOP_VER2_H_

#include "cam_vfe_camif_ver2.h"
#include "cam_vfe_camif_lite_ver2.h"
#include "cam_vfe_rdi.h"
#include "cam_vfe_fe_ver1.h"
#include "cam_vfe_top_common.h"

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
	uint32_t                                    num_mux;
	uint32_t mux_type[CAM_VFE_TOP_MUX_MAX];
};

int cam_vfe_top_ver2_init(struct cam_hw_soc_info     *soc_info,
	struct cam_hw_intf                           *hw_intf,
	void                                         *top_hw_info,
	void                                         *vfe_irq_controller,
	struct cam_vfe_top                          **vfe_top_ptr);

int cam_vfe_top_ver2_deinit(struct cam_vfe_top      **vfe_top);

#endif /* _CAM_VFE_TOP_VER2_H_ */
