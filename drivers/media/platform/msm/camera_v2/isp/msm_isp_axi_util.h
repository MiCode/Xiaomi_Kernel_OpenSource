/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#ifndef __MSM_ISP_AXI_UTIL_H__
#define __MSM_ISP_AXI_UTIL_H__

#include "msm_isp.h"

int msm_isp_axi_create_stream(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd);

void msm_isp_axi_destroy_stream(
	struct msm_vfe_axi_shared_data *axi_data, int stream_idx);

int msm_isp_validate_axi_request(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd);

void msm_isp_axi_reserve_wm(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info);

void msm_isp_axi_reserve_comp_mask(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info);

int msm_isp_axi_check_stream_state(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd);

void msm_isp_calculate_framedrop(
		struct msm_vfe_axi_shared_data *axi_data,
		struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd);

int msm_isp_request_axi_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_cfg_axi_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_release_axi_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_update_axi_stream(struct vfe_device *vfe_dev, void *arg);
void msm_isp_axi_cfg_update(struct vfe_device *vfe_dev);

void msm_isp_axi_stream_update(struct vfe_device *vfe_dev);

void msm_isp_update_framedrop_reg(struct vfe_device *vfe_dev);
void msm_isp_sof_notify(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts);
void msm_isp_process_axi_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts);
#endif /* __MSM_ISP_AXI_UTIL_H__ */
