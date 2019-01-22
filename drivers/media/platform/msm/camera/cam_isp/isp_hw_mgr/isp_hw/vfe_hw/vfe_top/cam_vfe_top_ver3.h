/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_TOP_VER3_H_
#define _CAM_VFE_TOP_VER3_H_

#include "cam_vfe_camif_ver3.h"
#include "cam_vfe_camif_lite_ver3.h"
#include "cam_vfe_camif_ver2.h"
#include "cam_vfe_camif_lite_ver2.h"
#include "cam_vfe_rdi.h"

#define CAM_VFE_TOP_VER3_MUX_MAX     6

struct cam_vfe_top_ver3_reg_offset_common {
	uint32_t hw_version;
	uint32_t titan_version;
	uint32_t hw_capability;
	uint32_t lens_feature;
	uint32_t stats_feature;
	uint32_t color_feature;
	uint32_t zoom_feature;
	uint32_t global_reset_cmd;
	uint32_t core_cgc_ovd_0;
	uint32_t core_cgc_ovd_1;
	uint32_t ahb_cgc_ovd;
	uint32_t noc_cgc_ovd;
	uint32_t bus_cgc_ovd;
	uint32_t core_cfg_0;
	uint32_t core_cfg_1;
	uint32_t reg_update_cmd;
	uint32_t trigger_cdm_events;
	uint32_t violation_status;
	uint32_t sbi_frame_idx;
	uint32_t dsp_status;
	uint32_t diag_config;
	uint32_t diag_sensor_status_0;
	uint32_t diag_sensor_status_1;
};

struct cam_vfe_top_ver3_hw_info {
	struct cam_vfe_top_ver3_reg_offset_common  *common_reg;
	struct cam_vfe_camif_ver3_hw_info           camif_hw_info;
	struct cam_vfe_camif_lite_ver3_hw_info      pdlib_hw_info;
	struct cam_vfe_camif_lite_ver3_hw_info
		*rdi_hw_info[CAM_VFE_RDI_VER2_MAX];
	struct cam_vfe_camif_lite_ver3_hw_info      lcr_hw_info;
	struct cam_vfe_fe_ver1_hw_info              fe_hw_info;
	uint32_t mux_type[CAM_VFE_TOP_VER3_MUX_MAX];
};

int cam_vfe_top_ver3_init(struct cam_hw_soc_info     *soc_info,
	struct cam_hw_intf                           *hw_intf,
	void                                         *top_hw_info,
	struct cam_vfe_top                          **vfe_top);

int cam_vfe_top_ver3_deinit(struct cam_vfe_top      **vfe_top);

#endif /* _CAM_VFE_TOP_VER3_H_ */
