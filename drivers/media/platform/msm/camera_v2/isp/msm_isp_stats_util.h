/* Copyright (c) 2013-2016, 2018 The Linux Foundation. All rights reserved.
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

#include "msm_isp.h"
#define STATS_IDX(idx) (idx & 0xFF)

void msm_isp_process_stats_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	uint32_t pingpong_status, struct msm_isp_timestamp *ts);
void msm_isp_stats_stream_update(struct vfe_device *vfe_dev);
int msm_isp_cfg_stats_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_update_stats_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_release_stats_stream(struct vfe_device *vfe_dev, void *arg);
int msm_isp_request_stats_stream(struct vfe_device *vfe_dev, void *arg);
void msm_isp_stats_disable(struct vfe_device *vfe_dev);
int msm_isp_stats_reset(struct vfe_device *vfe_dev);
int msm_isp_stats_restart(struct vfe_device *vfe_dev);
void msm_isp_release_all_stats_stream(struct vfe_device *vfe_dev);
void msm_isp_process_stats_reg_upd_epoch_irq(struct vfe_device *vfe_dev,
		enum msm_isp_comp_irq_types irq);
void msm_isp_stop_all_stats_stream(struct vfe_device *vfe_dev);

static inline int msm_isp_get_vfe_idx_for_stats_stream_user(
				struct vfe_device *vfe_dev,
				struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx;

	for (vfe_idx = 0; vfe_idx < stream_info->num_isp; vfe_idx++)
		if (stream_info->vfe_dev[vfe_idx] == vfe_dev)
			return vfe_idx;
	return -ENOTTY;
}

static inline int msm_isp_get_vfe_idx_for_stats_stream(
				struct vfe_device *vfe_dev,
				struct msm_vfe_stats_stream *stream_info)
{
	int vfe_idx = msm_isp_get_vfe_idx_for_stats_stream_user(vfe_dev,
							stream_info);

	if (vfe_idx < 0) {
		WARN(1, "%s vfe index missing for stream %d vfe %d\n",
			__func__, stream_info->stats_type, vfe_dev->pdev->id);
		vfe_idx = 0;
	}
	return vfe_idx;
}

static inline struct msm_vfe_stats_stream *
				msm_isp_get_stats_stream_common_data(
				struct vfe_device *vfe_dev,
				enum msm_isp_stats_type idx)
{
	if (vfe_dev->is_split)
		return &vfe_dev->common_data->stats_streams[idx];
	else
		return &vfe_dev->common_data->stats_streams[idx +
					MSM_ISP_STATS_MAX * vfe_dev->pdev->id];
}

static inline struct msm_vfe_stats_stream *
	msm_isp_get_stats_stream(struct dual_vfe_resource *dual_vfe_res,
					int vfe_id,
					enum msm_isp_stats_type idx)
{
	return msm_isp_get_stats_stream_common_data(
				dual_vfe_res->vfe_dev[vfe_id], idx);
}
#endif /* __MSM_ISP_STATS_UTIL_H__ */
