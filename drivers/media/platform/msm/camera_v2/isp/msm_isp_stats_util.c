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

static int msm_isp_stats_cfg_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info, uint32_t pingpong_status,
	struct msm_isp_buffer **done_buf)
{
	int rc = -1;
	struct msm_isp_buffer *buf;
	uint32_t pingpong_bit = 0;
	uint32_t bufq_handle = stream_info->bufq_handle;
	uint32_t stats_pingpong_offset =
		STATS_IDX(stream_info->stream_handle) +
		vfe_dev->hw_info->stats_hw_info->stats_ping_pong_offset;

	pingpong_bit = (~(pingpong_status >> stats_pingpong_offset) & 0x1);
	rc = vfe_dev->buf_mgr->ops->get_buf(vfe_dev->buf_mgr,
			vfe_dev->pdev->id, bufq_handle, &buf);
	if (rc < 0) {
		vfe_dev->error_info.stats_framedrop_count[
			STATS_IDX(stream_info->stream_handle)]++;
		return rc;
	}

	if (buf->num_planes != 1) {
		pr_err("%s: Invalid buffer\n", __func__);
		rc = -EINVAL;
		goto buf_error;
	}

	vfe_dev->hw_info->vfe_ops.stats_ops.update_ping_pong_addr(
		vfe_dev, stream_info,
		pingpong_status, buf->mapped_info[0].paddr +
		stream_info->buffer_offset);

	if (stream_info->buf[pingpong_bit] && done_buf)
		*done_buf = stream_info->buf[pingpong_bit];

	stream_info->buf[pingpong_bit] = buf;
	return 0;
buf_error:
	vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
		buf->bufq_handle, buf->buf_idx);
	return rc;
}

void msm_isp_process_stats_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	int i, rc;
	struct msm_isp_event_data buf_event;
	struct msm_isp_stats_event *stats_event = &buf_event.u.stats;
	struct msm_isp_buffer *done_buf;
	struct msm_vfe_stats_stream *stream_info = NULL;
	uint32_t pingpong_status;
	uint32_t stats_comp_mask = 0, stats_irq_mask = 0;
	stats_comp_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_comp_mask(irq_status0, irq_status1);
	stats_irq_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_wm_mask(irq_status0, irq_status1);
	if (!(stats_comp_mask || stats_irq_mask))
		return;
	ISP_DBG("%s: status: 0x%x\n", __func__, irq_status0);

	if (vfe_dev->stats_data.stats_pipeline_policy == STATS_COMP_ALL) {
		if (!stats_comp_mask)
			return;
		stats_irq_mask = 0xFFFFFFFF;
	}

	memset(&buf_event, 0, sizeof(struct msm_isp_event_data));
	pingpong_status = vfe_dev->hw_info->
		vfe_ops.stats_ops.get_pingpong_status(vfe_dev);

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++) {
		if (!(stats_irq_mask & (1 << i)))
			continue;
		stream_info = &vfe_dev->stats_data.stream_info[i];
		done_buf = NULL;
		msm_isp_stats_cfg_ping_pong_address(vfe_dev,
			stream_info, pingpong_status, &done_buf);
		if (done_buf) {
			rc = vfe_dev->buf_mgr->ops->buf_divert(vfe_dev->buf_mgr,
				done_buf->bufq_handle, done_buf->buf_idx,
				&ts->buf_time, vfe_dev->axi_data.
				src_info[VFE_PIX_0].frame_id);
			if (rc == 0) {
				stats_event->stats_mask |=
					1 << stream_info->stats_type;
				stats_event->stats_buf_idxs[
					stream_info->stats_type] =
					done_buf->buf_idx;
			}
		}
	}

	if (stats_event->stats_mask) {
		buf_event.timestamp = ts->event_time;
		buf_event.frame_id =
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
		msm_isp_send_event(vfe_dev, ISP_EVENT_STATS_NOTIFY +
				stream_info->stats_type, &buf_event);
	}
}

int msm_isp_stats_create_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_request_cmd *stream_req_cmd)
{
	int rc = -1;
	struct msm_vfe_stats_stream *stream_info = NULL;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	uint32_t stats_idx;

	if (!(vfe_dev->hw_info->stats_hw_info->stats_capability_mask &
		(1 << stream_req_cmd->stats_type))) {
		pr_err("%s: Stats type not supported\n", __func__);
		return rc;
	}

	stats_idx = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_stats_idx(stream_req_cmd->stats_type);

	stream_info = &stats_data->stream_info[stats_idx];
	if (stream_info->state != STATS_AVALIABLE) {
		pr_err("%s: Stats already requested\n", __func__);
		return rc;
	}

	if (stats_data->stats_pipeline_policy != STATS_COMP_ALL) {
		if (stream_req_cmd->framedrop_pattern >= MAX_SKIP) {
			pr_err("%s: Invalid framedrop pattern\n", __func__);
			return rc;
		}

		if (stream_req_cmd->irq_subsample_pattern >= MAX_SKIP) {
			pr_err("%s: Invalid irq subsample pattern\n", __func__);
			return rc;
		}
	} else {
		if (stats_data->comp_framedrop_pattern >= MAX_SKIP) {
			pr_err("%s: Invalid comp framedrop pattern\n",
				__func__);
			return rc;
		}

		if (stats_data->comp_irq_subsample_pattern >= MAX_SKIP) {
			pr_err("%s: Invalid comp irq subsample pattern\n",
				__func__);
			return rc;
		}
		stream_req_cmd->framedrop_pattern =
			vfe_dev->stats_data.comp_framedrop_pattern;
		stream_req_cmd->irq_subsample_pattern =
			vfe_dev->stats_data.comp_irq_subsample_pattern;
	}

	stream_info->session_id = stream_req_cmd->session_id;
	stream_info->stream_id = stream_req_cmd->stream_id;
	stream_info->stats_type = stream_req_cmd->stats_type;
	stream_info->buffer_offset = stream_req_cmd->buffer_offset;
	stream_info->framedrop_pattern = stream_req_cmd->framedrop_pattern;
	stream_info->irq_subsample_pattern =
		stream_req_cmd->irq_subsample_pattern;
	stream_info->state = STATS_INACTIVE;

	if ((vfe_dev->stats_data.stream_handle_cnt << 8) == 0)
		vfe_dev->stats_data.stream_handle_cnt++;

	stream_req_cmd->stream_handle =
		(++vfe_dev->stats_data.stream_handle_cnt) << 8 | stats_idx;

	stream_info->stream_handle = stream_req_cmd->stream_handle;
	return 0;
}

int msm_isp_request_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_stats_stream_request_cmd *stream_req_cmd = arg;
	struct msm_vfe_stats_stream *stream_info = NULL;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	uint32_t stats_idx;

	rc = msm_isp_stats_create_stream(vfe_dev, stream_req_cmd);
	if (rc < 0) {
		pr_err("%s: create stream failed\n", __func__);
		return rc;
	}

	stats_idx = STATS_IDX(stream_req_cmd->stream_handle);
	stream_info = &stats_data->stream_info[stats_idx];

	switch (stream_info->framedrop_pattern) {
	case NO_SKIP:
		stream_info->framedrop_pattern = VFE_NO_DROP;
		break;
	case EVERY_2FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_2FRAME;
		break;
	case EVERY_4FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_4FRAME;
		break;
	case EVERY_8FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_8FRAME;
		break;
	case EVERY_16FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_16FRAME;
		break;
	case EVERY_32FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_32FRAME;
		break;
	default:
		stream_info->framedrop_pattern = VFE_NO_DROP;
		break;
	}

	if (stats_data->stats_pipeline_policy == STATS_COMP_NONE)
		vfe_dev->hw_info->vfe_ops.stats_ops.
			cfg_wm_irq_mask(vfe_dev, stream_info);

	vfe_dev->hw_info->vfe_ops.stats_ops.cfg_wm_reg(vfe_dev, stream_info);
	return rc;
}

int msm_isp_release_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = -1;
	struct msm_vfe_stats_stream_cfg_cmd stream_cfg_cmd;
	struct msm_vfe_stats_stream_release_cmd *stream_release_cmd = arg;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	int stats_idx = STATS_IDX(stream_release_cmd->stream_handle);
	struct msm_vfe_stats_stream *stream_info =
		&stats_data->stream_info[stats_idx];

	if (stream_info->state == STATS_AVALIABLE) {
		pr_err("%s: stream already release\n", __func__);
		return rc;
	} else if (stream_info->state != STATS_INACTIVE) {
		stream_cfg_cmd.enable = 0;
		stream_cfg_cmd.num_streams = 1;
		stream_cfg_cmd.stream_handle[0] =
			stream_release_cmd->stream_handle;
		rc = msm_isp_cfg_stats_stream(vfe_dev, &stream_cfg_cmd);
	}

	if (stats_data->stats_pipeline_policy == STATS_COMP_NONE)
		vfe_dev->hw_info->vfe_ops.stats_ops.
			clear_wm_irq_mask(vfe_dev, stream_info);

	vfe_dev->hw_info->vfe_ops.stats_ops.clear_wm_reg(vfe_dev, stream_info);
	memset(stream_info, 0, sizeof(struct msm_vfe_stats_stream));
	return 0;
}

int msm_isp_cfg_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int i, rc = 0;
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd = arg;
	struct msm_vfe_stats_stream *stream_info;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	int idx;
	uint32_t stats_mask = 0;

	if (stats_data->num_active_stream == 0)
		vfe_dev->hw_info->vfe_ops.stats_ops.cfg_ub(vfe_dev);

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);
		stream_info = &stats_data->stream_info[idx];
		if (stream_info->stream_handle !=
				stream_cfg_cmd->stream_handle[i]) {
			pr_err("%s: Invalid stream handle: 0x%x received\n",
				__func__, stream_cfg_cmd->stream_handle[i]);
			continue;
		}

		if (stream_cfg_cmd->enable) {
			stream_info->bufq_handle =
				vfe_dev->buf_mgr->ops->get_bufq_handle(
				vfe_dev->buf_mgr, stream_info->session_id,
				stream_info->stream_id);
				if (stream_info->bufq_handle == 0) {
					pr_err("%s: no buf configured for stream: 0x%x\n",
						__func__,
						stream_info->stream_handle);
					return -EINVAL;
				}

			msm_isp_stats_cfg_ping_pong_address(vfe_dev,
				stream_info, VFE_PING_FLAG, NULL);
			msm_isp_stats_cfg_ping_pong_address(vfe_dev,
				stream_info, VFE_PONG_FLAG, NULL);
			stream_info->state = STATS_START_PENDING;
			stats_data->num_active_stream++;
		} else {
			stream_info->state = STATS_STOP_PENDING;
			stats_data->num_active_stream--;
		}
		stats_mask |= 1 << idx;
	}
	vfe_dev->hw_info->vfe_ops.stats_ops.
		enable_module(vfe_dev, stats_mask, stream_cfg_cmd->enable);
	return rc;
}

int msm_isp_cfg_stats_comp_policy(struct vfe_device *vfe_dev, void *arg)
{
	int rc = -1;
	struct msm_vfe_stats_comp_policy_cfg *policy_cfg_cmd = arg;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;

	if (stats_data->num_active_stream != 0) {
		pr_err("%s: Cannot update policy when there are active streams\n",
			   __func__);
		return rc;
	}

	if (policy_cfg_cmd->stats_pipeline_policy >= MAX_STATS_POLICY) {
		pr_err("%s: Invalid stats composite policy\n", __func__);
		return rc;
	}

	if (policy_cfg_cmd->comp_framedrop_pattern >= MAX_SKIP) {
		pr_err("%s: Invalid comp framedrop pattern\n", __func__);
		return rc;
	}

	if (policy_cfg_cmd->comp_irq_subsample_pattern >= MAX_SKIP) {
		pr_err("%s: Invalid comp irq subsample pattern\n", __func__);
		return rc;
	}

	stats_data->stats_pipeline_policy =
		policy_cfg_cmd->stats_pipeline_policy;
	stats_data->comp_framedrop_pattern =
		policy_cfg_cmd->comp_framedrop_pattern;
	stats_data->comp_irq_subsample_pattern =
		policy_cfg_cmd->comp_irq_subsample_pattern;

	vfe_dev->hw_info->vfe_ops.stats_ops.cfg_comp_mask(vfe_dev);

	return 0;
}

