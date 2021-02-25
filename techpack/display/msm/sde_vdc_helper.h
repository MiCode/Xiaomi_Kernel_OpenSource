/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_VDC_HELPER_H__
#define __SDE_VDC_HELPER_H__

#include "msm_drv.h"

#define VDC_BPP(bits_per_pixel) (bits_per_pixel >> 4)

#define VDC_NUM_BUF_RANGES (DSC_NUM_BUF_RANGES - 1)

#define VDC_FLAT_QP_LUT_SIZE 8
#define VDC_MAX_QP_LUT_SIZE 8
#define VDC_TAR_DEL_LUT_SIZE 16
#define VDC_LBDA_BRATE_LUT_SIZE 16
#define VDC_LBDA_BF_LUT_SIZE 16
#define VDC_LBDA_BRATE_REG_SIZE 64

#define VDC_VIDEO_MODE 0
#define VDC_CMD_MODE 1

#define VDC_TRAFFIC_SYNC_PULSES 0
#define VDC_TRAFFIC_SYNC_START_EVENTS 1
#define VDC_TRAFFIC_BURST_MODE 2

#define MAX_PIPELINE_LATENCY 68
#define OB_DATA_WIDTH 128
#define OUT_BUF_FULL_THRESH 2
#define OUT_BUF_UF_MARGIN 3
#define OUT_BUF_OF_MARGIN_TC_10 5
#define OUT_BUF_OF_MARGIN_OB 3
#define OUTPUT_DATA_WIDTH 64
#define OB0_RAM_DEPTH 912
#define OB1_RAM_DEPTH 736

#define SSM_MAX_SE_SIZE 128
#define RC_TARGET_RATE_EXTRA_FTBLS 2

#define NUM_ACTIVE_HS 1
#define MAX_PIXELS_PER_HS_LINE 5120

#define SDE_VDC_PPS_SIZE 128

/**
 * sde_vdc_populate_config - populates the VDC encoder parameters
 * for a given panel configuration
 */
int sde_vdc_populate_config(struct msm_display_vdc_info *vdc_info,
	int intf_width, int traffic_mode);

/**
 * sde_vdc_create_pps_buf_cmd- creates the PPS buffer from the VDC
 * parameters according to the VDC specification
 */
int sde_vdc_create_pps_buf_cmd(struct msm_display_vdc_info *vdc_info,
	char *buf, int pps_id, u32 size);
void sde_vdc_intf_prog_params(struct msm_display_vdc_info *vdc_info,
	int intf_width);

#endif /* __SDE_VDC_HELPER_H__ */
