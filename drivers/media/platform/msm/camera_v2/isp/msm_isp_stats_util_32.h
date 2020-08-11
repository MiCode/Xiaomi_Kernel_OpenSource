/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2018, 2020 The Linux Foundation. All rights reserved.
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
#ifndef __MSM_ISP_STATS_UTIL_H__
#define __MSM_ISP_STATS_UTIL_H__

#include "msm_isp_32.h"
#define STATS_IDX(idx) (idx & 0xFF)

void msm_isp_process_stats_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts);
int msm_isp_stats_create_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_request_cmd *stream_req_cmd);
void msm_isp_stats_stream_update(struct vfe_device *vfe_dev);
int msm_isp_cfg_stats_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_update_stats_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_release_stats_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_request_stats_stream(struct vfe_device *vfe_dev, void *arg);
void msm_isp_update_stats_framedrop_reg(struct vfe_device *vfe_dev);
#endif /* __MSM_ISP_STATS_UTIL_H__ */
