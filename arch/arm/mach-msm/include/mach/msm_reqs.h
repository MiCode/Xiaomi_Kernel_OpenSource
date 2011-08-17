/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_REQS_H
#define __ARCH_ARM_MACH_MSM_REQS_H

#include <linux/kernel.h>

enum system_bus_flow_ids {
	MSM_AXI_FLOW_INVALID = 0,
	MSM_AXI_FLOW_APPLICATION_LOW,
	MSM_AXI_FLOW_APPLICATION_MED,
	MSM_AXI_FLOW_APPLICATION_HI,
	MSM_AXI_FLOW_APPLICATION_MAX,
	MSM_AXI_FLOW_VIDEO_PLAYBACK_LOW,
	MSM_AXI_FLOW_VIDEO_PLAYBACK_MED,
	MSM_AXI_FLOW_VIDEO_PLAYBACK_HI,
	MSM_AXI_FLOW_VIDEO_PLAYBACK_MAX,
	MSM_AXI_FLOW_VIDEO_RECORD_LOW,
	MSM_AXI_FLOW_VIDEO_RECORD_MED,
	MSM_AXI_FLOW_VIDEO_RECORD_HI,
	MSM_AXI_FLOW_GRAPHICS_LOW,
	MSM_AXI_FLOW_GRAPHICS_MED,
	MSM_AXI_FLOW_GRAPHICS_HI,
	MSM_AXI_FLOW_VIEWFINDER_LOW,
	MSM_AXI_FLOW_VIEWFINDER_MED,
	MSM_AXI_FLOW_VIEWFINDER_HI,
	MSM_AXI_FLOW_LAPTOP_DATA_CALL,
	MSM_AXI_FLOW_APPLICATION_DATA_CALL,
	MSM_AXI_FLOW_GPS,
	MSM_AXI_FLOW_TV_OUT_LOW,
	MSM_AXI_FLOW_TV_OUT_MED,
	MSM_AXI_FLOW_ILCDC_WVGA,
	MSM_AXI_FLOW_VOYAGER_DEFAULT,
	MSM_AXI_FLOW_2D_GPU_HIGH,
	MSM_AXI_FLOW_3D_GPU_HIGH,
	MSM_AXI_FLOW_CAMERA_PREVIEW_HIGH,
	MSM_AXI_FLOW_CAMERA_SNAPSHOT_12MP,
	MSM_AXI_FLOW_CAMERA_RECORDING_720P,
	MSM_AXI_FLOW_JPEG_12MP,
	MSM_AXI_FLOW_MDP_LCDC_WVGA_2BPP,
	MSM_AXI_FLOW_MDP_MDDI_WVGA_2BPP,
	MSM_AXI_FLOW_MDP_DTV_720P_2BPP,
	MSM_AXI_FLOW_VIDEO_RECORDING_720P,
	MSM_AXI_FLOW_VIDEO_PLAYBACK_720P,
	MSM_AXI_FLOW_MDP_TVENC_720P_2BPP,
	MSM_AXI_FLOW_VIDEO_PLAYBACK_WVGA,
	MSM_AXI_FLOW_VIDEO_PLAYBACK_QVGA,
	MSM_AXI_FLOW_VIDEO_RECORDING_QVGA,

	MSM_AXI_NUM_FLOWS,
};

#define MSM_REQ_DEFAULT_VALUE 0

/**
 * msm_req_add - Creates an NPA request and returns a handle. Non-blocking.
 * @req_name:	Name of the request
 * @res_name:	Name of the NPA resource the request is for
 */
void *msm_req_add(char *res_name, char *client_name);

/**
 * msm_req_update - Updates an existing NPA request. May block.
 * @req:	Request handle
 * @value:	Request value
 */
int msm_req_update(void *req, s32 value);

/**
 * msm_req_remove - Removes an existing NPA request. May block.
 * @req:	Request handle
 */
int msm_req_remove(void *req);

#endif /* __ARCH_ARM_MACH_MSM_REQS_H */

