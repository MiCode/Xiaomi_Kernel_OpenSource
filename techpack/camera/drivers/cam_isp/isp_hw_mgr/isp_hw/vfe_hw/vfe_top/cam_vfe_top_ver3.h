/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_VFE_TOP_VER3_H_
#define _CAM_VFE_TOP_VER3_H_

#include "cam_vfe_camif_ver3.h"
#include "cam_vfe_camif_lite_ver3.h"
#include "cam_vfe_fe_ver1.h"
#include "cam_vfe_top_common.h"

#define CAM_SHIFT_TOP_CORE_CFG_MUXSEL_PDAF       31
#define CAM_SHIFT_TOP_CORE_CFG_VID_DS16_R2PD     30
#define CAM_SHIFT_TOP_CORE_CFG_VID_DS4_R2PD      29
#define CAM_SHIFT_TOP_CORE_CFG_DISP_DS16_R2PD    28
#define CAM_SHIFT_TOP_CORE_CFG_DISP_DS4_R2PD     27
#define CAM_SHIFT_TOP_CORE_CFG_DSP_STREAMING     25
#define CAM_SHIFT_TOP_CORE_CFG_STATS_IHIST       10
#define CAM_SHIFT_TOP_CORE_CFG_STATS_HDR_BE       9
#define CAM_SHIFT_TOP_CORE_CFG_STATS_HDR_BHIST    8
#define CAM_SHIFT_TOP_CORE_CFG_INPUTMUX_PP        5

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
	uint32_t custom_frame_idx;
	uint32_t dsp_status;
	uint32_t diag_config;
	uint32_t diag_sensor_status_0;
	uint32_t diag_sensor_status_1;
	uint32_t bus_overflow_status;
	uint32_t top_debug_cfg;
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
	uint32_t top_debug_12;
	uint32_t top_debug_13;
};

struct cam_vfe_camif_common_cfg {
	uint32_t     vid_ds16_r2pd;
	uint32_t     vid_ds4_r2pd;
	uint32_t     disp_ds16_r2pd;
	uint32_t     disp_ds4_r2pd;
	uint32_t     dsp_streaming_tap_point;
	uint32_t     ihist_src_sel;
	uint32_t     hdr_be_src_sel;
	uint32_t     hdr_bhist_src_sel;
	uint32_t     input_mux_sel_pdaf;
	uint32_t     input_mux_sel_pp;
};

struct cam_vfe_top_ver3_hw_info {
	struct cam_vfe_top_ver3_reg_offset_common  *common_reg;
	struct cam_vfe_camif_ver3_hw_info           camif_hw_info;
	struct cam_vfe_camif_lite_ver3_hw_info      pdlib_hw_info;
	struct cam_vfe_camif_lite_ver3_hw_info
		*rdi_hw_info[CAM_VFE_RDI_VER2_MAX];
	struct cam_vfe_camif_lite_ver3_hw_info      lcr_hw_info;
	struct cam_vfe_fe_ver1_hw_info              fe_hw_info;
	uint32_t                                    num_mux;
	uint32_t mux_type[CAM_VFE_TOP_MUX_MAX];
};

int cam_vfe_top_ver3_init(struct cam_hw_soc_info     *soc_info,
	struct cam_hw_intf                           *hw_intf,
	void                                         *top_hw_info,
	void                                         *vfe_irq_controller,
	struct cam_vfe_top                          **vfe_top);

int cam_vfe_top_ver3_deinit(struct cam_vfe_top      **vfe_top);

#endif /* _CAM_VFE_TOP_VER3_H_ */
