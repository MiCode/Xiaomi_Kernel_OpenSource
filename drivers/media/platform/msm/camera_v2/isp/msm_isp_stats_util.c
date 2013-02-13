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
#include <linux/io.h>
#include <media/v4l2-subdev.h>
#include "msm_isp_util.h"
#include "msm_isp_stats_util.h"
#define VFE_PING_ACTIVE_FLAG 0xFFFFFFFF
#define VFE_PONG_ACTIVE_FLAG 0x0

int msm_isp_stats_cfg_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info, uint32_t pingpong_status,
	struct msm_isp_buffer *out_buf)
{
	int rc = -1;
	struct msm_isp_buffer *buf;
	int active_bit = 0;
	int buf_idx;
	uint32_t bufq_handle = stream_info->bufq_handle;

	out_buf = NULL;
	active_bit = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_active_pingpong_idx(pingpong_status,
			stream_info->stats_type);
	buf_idx = active_bit^0x1;

	rc = vfe_dev->buf_mgr->ops->get_buf(
		vfe_dev->buf_mgr, bufq_handle, &buf);
	if (rc < 0) {
		pr_err("%s: No free buffer, stats_type = %d\n",
			__func__, stream_info->stats_type);
		return rc;
	}
	vfe_dev->hw_info->vfe_ops.stats_ops.update_ping_pong_addr(
		vfe_dev, stream_info->stats_type,
		pingpong_status, buf->mapped_info[buf_idx].paddr);

	if (stream_info->buf[buf_idx])
		out_buf = stream_info->buf[buf_idx];
	stream_info->buf[buf_idx] = buf;
	return 0;
}

void msm_isp_process_stats_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct timeval *tv)
{
	uint32_t frame_id;
	uint32_t stats_comp_mask = 0, stats_mask = 0;
	ISP_DBG("%s: status: 0x%x\n", __func__, irq_status0);
	stats_comp_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_comp_mask(irq_status0, irq_status1);
	stats_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_wm_mask(irq_status0, irq_status1);
	if (!(stats_comp_mask || stats_mask))
		return;
	frame_id = vfe_dev->hw_info->vfe_ops.stats_ops.get_frame_id(vfe_dev);
	/* TD: process comp/non comp stats */
}

static int msm_isp_stats_reserve_comp_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_shared_data *stats_data,
	struct msm_vfe_stats_stream *stream_info)
{
	int i;
	uint8_t num_stats_comp;
	int8_t comp_idx = -1;
	if (stream_info->comp_flag == 0)
		return 0;

	num_stats_comp = vfe_dev->axi_data.hw_info->num_stats_comp_mask;
	for (i = 0; i < num_stats_comp; i++) {
		if (!stats_data->composite_info[i].stats_mask == 0 &&
				comp_idx < 0)
			comp_idx = i;
		if (stats_data->composite_info[i].comp_flag ==
				stream_info->comp_flag) {
			comp_idx = i;
			break;
		}
	}
	if (comp_idx < 0) {
		pr_err("%s: no more stats comp idx\n", __func__);
		return -EACCES;
	}
	stats_data->composite_info[comp_idx].stats_mask |=
		(1 << stream_info->stats_type);
	if (stats_data->composite_info[comp_idx].comp_flag == 0)
		stats_data->composite_info[comp_idx].comp_flag =
			stream_info->comp_flag;
	stream_info->comp_idx = comp_idx;
	return 0;
}

static int msm_isp_stats_unreserve_comp_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_shared_data *stats_data,
	struct msm_vfe_stats_stream *stream_info)
{
	uint8_t comp_idx = stream_info->comp_idx;

	if (stream_info->comp_flag == 0)
		return 0;
	stats_data->composite_info[comp_idx].stats_mask &=
		~(1 << stream_info->stats_type);
	if (stats_data->composite_info[comp_idx].stats_mask == 0)
		memset(&stats_data->composite_info[comp_idx], 0,
			sizeof(struct msm_vfe_stats_composite_info));
	return 0;
}

int msm_isp_request_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_stats_stream_request_cmd *stream_cfg_cmd = arg;
	struct msm_vfe_stats_stream *stream_info = NULL;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;

	if (stream_cfg_cmd->stats_type < MSM_ISP_STATS_AEC ||
			stream_cfg_cmd->stats_type >= MSM_ISP_STATS_MAX) {
		pr_err("%s: invalid stats type %d received\n",
		__func__, stream_cfg_cmd->stats_type);
		return -EINVAL;
	}
	stream_info = &stats_data->stream_info[stream_cfg_cmd->stats_type];
	if (stream_info->stream_handle != 0) {
		pr_err("%s: stats type %d has been used already\n",
			__func__, stream_cfg_cmd->stats_type);
			return -EBUSY;
	}

	stats_data->stream_handle_cnt++;
	if (stats_data->stream_handle_cnt == 0)
		stats_data->stream_handle_cnt++;
	stream_info->stream_handle =
		stats_data->stream_handle_cnt << 16 |
			stream_cfg_cmd->stats_type;
	stream_info->enable = 0;
	stream_info->stats_type = stream_cfg_cmd->stats_type;
	stream_info->comp_flag = stream_cfg_cmd->comp_flag;
	stream_info->session_id = stream_cfg_cmd->session_id;
	stream_info->stream_id = stream_cfg_cmd->stream_id;
	stream_info->framedrop_pattern = stream_cfg_cmd->framedrop_pattern;
	stream_cfg_cmd->stream_handle =	stream_info->stream_handle;
	msm_isp_stats_reserve_comp_mask(vfe_dev, stats_data, stream_info);
	if (stream_info->comp_flag)
		vfe_dev->hw_info->vfe_ops.stats_ops.
			cfg_comp_mask(vfe_dev, stream_info);
	else
		vfe_dev->hw_info->vfe_ops.stats_ops.
			cfg_wm_irq_mask(vfe_dev, stream_info);
	vfe_dev->hw_info->vfe_ops.stats_ops.
		cfg_wm_reg(vfe_dev, stream_info);
	return rc;
}

int msm_isp_release_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_stats_stream_release_cmd *stream_release_cmd = arg;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	struct msm_vfe_stats_stream *stream_info =
		&stats_data->stream_info[
		(stream_release_cmd->stream_handle & 0xFF)];

	if (stream_info == NULL ||
			stream_info->stream_handle !=
				stream_release_cmd->stream_handle) {
		pr_err("%s: handle mismatch(0x%x, 0x%x\n",
			__func__, stream_info->stream_handle,
			stream_release_cmd->stream_handle);
		rc = -EINVAL;
	}
	if (stream_info->enable) {
		struct msm_vfe_stats_stream_cfg_cmd stop_cmd;
		memset(&stop_cmd, 0, sizeof(stop_cmd));
		stop_cmd.num_streams = 1;
		stop_cmd.stream_handle[0] = stream_release_cmd->stream_handle;
		stop_cmd.enable = 0;
		rc = msm_isp_cfg_stats_stream(vfe_dev, (void *)&stop_cmd);
		if (rc < 0) {
			pr_err("%s: cannot stop stats type %d\n",
				__func__, stream_info->stats_type);
			return -EPERM;
		}
	}
	if (stream_info->bufq_handle) {
		vfe_dev->buf_mgr->ops->release_buf(vfe_dev->buf_mgr,
			stream_info->bufq_handle);
		stream_info->bufq_handle = 0;
	}
	vfe_dev->hw_info->vfe_ops.stats_ops.
		clear_wm_reg(vfe_dev, stream_info);
	if (stream_info->comp_flag) {
		msm_isp_stats_unreserve_comp_mask(vfe_dev,
			stats_data, stream_info);
		vfe_dev->hw_info->vfe_ops.stats_ops.
		clear_comp_mask(vfe_dev, stream_info);
	} else {
		vfe_dev->hw_info->vfe_ops.stats_ops.
		clear_wm_irq_mask(vfe_dev, stream_info);
	}
	vfe_dev->hw_info->vfe_ops.stats_ops.
		clear_framedrop(vfe_dev, stream_info);
	memset(stream_info, 0, sizeof(struct msm_vfe_stats_stream));
	return rc;
}

int msm_isp_cfg_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int i, rc = 0;
	uint32_t pingpong_status = 0;
	struct msm_isp_buffer *buf = NULL;
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd = arg;
	struct msm_vfe_stats_stream *stream_info;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	int idx;
	uint32_t stats_mask = 0;
	uint8_t enable = stream_cfg_cmd->enable;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		idx = stream_cfg_cmd->stream_handle[i] & 0xF;
		stream_info = &stats_data->stream_info[idx];
		if (stream_info->stream_handle !=
				stream_cfg_cmd->stream_handle[i]) {
			pr_err("%s: invalid stream handle -0x%x received\n",
				__func__, stream_cfg_cmd->stream_handle[i]);
			continue;
		}
		if (enable) {
			stream_info->bufq_handle =
				vfe_dev->buf_mgr->ops->get_bufq_handle(
				vfe_dev->buf_mgr, stream_info->session_id,
				stream_info->stream_id);
				if (stream_info->bufq_handle == 0) {
					pr_err("%s: no buf configured for stats type = %d\n",
						__func__,
						stream_info->stats_type);
					return -EINVAL;
				}
		}
		/* config ping address */
		pingpong_status = VFE_PONG_ACTIVE_FLAG;
		stats_mask |= (1 << stream_info->stats_type);
		msm_isp_stats_cfg_ping_pong_address(vfe_dev,
			stream_info, pingpong_status, buf);
		pingpong_status = VFE_PING_ACTIVE_FLAG;
		msm_isp_stats_cfg_ping_pong_address(vfe_dev,
			stream_info, pingpong_status, buf);
	}
	vfe_dev->hw_info->vfe_ops.stats_ops.
		stats_enable(vfe_dev, stats_mask, enable);
	return rc;
}
