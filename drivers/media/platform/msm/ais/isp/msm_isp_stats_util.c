/* Copyright (c) 2013-2018, The Linux Foundation. All rights reserved.
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
#include <linux/atomic.h>
#include <media/v4l2-subdev.h>
#include <media/ais/msm_ais_isp.h>
#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp_stats_util.h"

static inline void msm_isp_stats_cfg_wm_scratch(struct vfe_device *vfe_dev,
				struct msm_vfe_stats_stream *stream_info,
				uint32_t pingpong_status)
{
	vfe_dev->hw_info->vfe_ops.stats_ops.update_ping_pong_addr(
		vfe_dev->vfe_base, stream_info,
		pingpong_status, vfe_dev->buf_mgr->scratch_buf_addr);
}

static inline void msm_isp_stats_cfg_stream_scratch(struct vfe_device *vfe_dev,
				struct msm_vfe_stats_stream *stream_info,
				uint32_t pingpong_status)
{
	uint32_t stats_idx = STATS_IDX(stream_info->stream_handle);
	uint32_t pingpong_bit;
	uint32_t stats_pingpong_offset =
		vfe_dev->hw_info->stats_hw_info->stats_ping_pong_offset[
		stats_idx];

	pingpong_bit = (~(pingpong_status >> stats_pingpong_offset) & 0x1);

	msm_isp_stats_cfg_wm_scratch(vfe_dev, stream_info,
		pingpong_status);
	stream_info->buf[pingpong_bit] = NULL;
}

static int msm_isp_stats_cfg_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info, uint32_t pingpong_status)
{
	int rc = -1, vfe_id = 0;
	struct msm_isp_buffer *buf;
	uint32_t pingpong_bit = 0;
	uint32_t stats_pingpong_offset;
	uint32_t bufq_handle = stream_info->bufq_handle;
	uint32_t stats_idx = STATS_IDX(stream_info->stream_handle);
	struct dual_vfe_resource *dual_vfe_res = NULL;
	struct msm_vfe_stats_stream *dual_vfe_stream_info = NULL;

	if (stats_idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type ||
		stats_idx >= MSM_ISP_STATS_MAX) {
		pr_err("%s Invalid stats index %d", __func__, stats_idx);
		return -EINVAL;
	}

	stats_pingpong_offset =
		vfe_dev->hw_info->stats_hw_info->stats_ping_pong_offset[
		stats_idx];

	pingpong_bit = (~(pingpong_status >> stats_pingpong_offset) & 0x1);

	rc = vfe_dev->buf_mgr->ops->get_buf(vfe_dev->buf_mgr,
			bufq_handle,
			MSM_ISP_INVALID_BUF_INDEX, &buf);
	if (rc == -EFAULT) {
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_BUF_FATAL_ERROR);
		return rc;
	}
	if (rc < 0 || NULL == buf)
		vfe_dev->error_info.stats_framedrop_count[stats_idx]++;

	if (buf && buf->num_planes != 1) {
		pr_err("%s: Invalid buffer\n", __func__);
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_BUF_FATAL_ERROR);
		rc = -EINVAL;
		goto buf_error;
	}
	if (vfe_dev->is_split) {
		dual_vfe_res = vfe_dev->common_data->dual_vfe_res;
		if (!dual_vfe_res->vfe_base[ISP_VFE0] ||
			!dual_vfe_res->stats_data[ISP_VFE0] ||
			!dual_vfe_res->vfe_base[ISP_VFE1] ||
			!dual_vfe_res->stats_data[ISP_VFE1]) {
			pr_err("%s:%d error vfe0 %pK %pK vfe1 %pK %pK\n",
				__func__, __LINE__,
				dual_vfe_res->vfe_base[ISP_VFE0],
				dual_vfe_res->stats_data[ISP_VFE0],
				dual_vfe_res->vfe_base[ISP_VFE1],
				dual_vfe_res->stats_data[ISP_VFE1]);
		} else {
			for (vfe_id = 0; vfe_id < MAX_VFE; vfe_id++) {
				dual_vfe_stream_info = &dual_vfe_res->
					stats_data[vfe_id]->
					stream_info[stats_idx];
				if (buf)
					vfe_dev->hw_info->vfe_ops.stats_ops.
						update_ping_pong_addr(
						dual_vfe_res->vfe_base[vfe_id],
						dual_vfe_stream_info,
						pingpong_status,
						buf->mapped_info[0].paddr +
						dual_vfe_stream_info->
						buffer_offset);
				else
					msm_isp_stats_cfg_stream_scratch(
						vfe_dev,
						dual_vfe_stream_info,
						pingpong_status);

				dual_vfe_stream_info->buf[pingpong_bit]
					= buf;
			}
		}
	} else {
		if (buf)
			vfe_dev->hw_info->vfe_ops.stats_ops.
				update_ping_pong_addr(
				vfe_dev->vfe_base, stream_info,
				pingpong_status, buf->mapped_info[0].paddr +
				stream_info->buffer_offset);
		else
			msm_isp_stats_cfg_stream_scratch(vfe_dev,
					stream_info, pingpong_status);

		stream_info->buf[pingpong_bit] = buf;
	}

	if (buf)
		buf->pingpong_bit = pingpong_bit;
	return 0;
buf_error:
	vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
		buf->bufq_handle, buf->buf_idx);
	return rc;
}

static int32_t msm_isp_stats_buf_divert(struct vfe_device *vfe_dev,
	struct msm_isp_timestamp *ts,
	struct msm_isp_event_data *buf_event,
	struct msm_vfe_stats_stream *stream_info,
	uint32_t *comp_stats_type_mask, uint32_t pingpong_status)
{
	int32_t rc = 0, frame_id = 0, drop_buffer = 0;
	struct msm_isp_stats_event *stats_event = NULL;
	struct msm_isp_sw_framskip *sw_skip = NULL;
	int32_t buf_index = -1;
	uint32_t pingpong_bit;
	struct msm_isp_buffer *done_buf;
	uint32_t stats_pingpong_offset;
	uint32_t stats_idx;

	if (!vfe_dev || !ts || !buf_event || !stream_info) {
		pr_err("%s:%d failed: invalid params %pK %pK %pK %pK\n",
			__func__, __LINE__, vfe_dev, ts, buf_event,
			stream_info);
		return -EINVAL;
	}
	frame_id = vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
	sw_skip = &stream_info->sw_skip;
	stats_event = &buf_event->u.stats;

	if (sw_skip->stats_type_mask &
		(1 << stream_info->stats_type)) {
		/* Hw stream output of this src is requested
		 *  for drop
		 */
		if (sw_skip->skip_mode == SKIP_ALL) {
			/* drop all buffers */
			drop_buffer = 1;
		} else if (sw_skip->skip_mode == SKIP_RANGE &&
		(sw_skip->min_frame_id <= frame_id &&
		sw_skip->max_frame_id >= frame_id)) {
			drop_buffer = 1;
		} else if (frame_id > sw_skip->max_frame_id) {
			memset(sw_skip, 0, sizeof
				(struct msm_isp_sw_framskip));
		}
	}
	stats_idx = STATS_IDX(stream_info->stream_handle);

	stats_pingpong_offset =
			vfe_dev->hw_info->stats_hw_info->stats_ping_pong_offset[
			stats_idx];
	pingpong_bit = (~(pingpong_status >> stats_pingpong_offset) & 0x1);

	done_buf = stream_info->buf[pingpong_bit];

	if (done_buf)
		buf_index = done_buf->buf_idx;

	rc = vfe_dev->buf_mgr->ops->update_put_buf_cnt(
		vfe_dev->buf_mgr, vfe_dev->pdev->id, stream_info->bufq_handle,
		buf_index, &ts->buf_time,
		frame_id, pingpong_bit);

	if (rc < 0) {
		if (rc == -EFAULT)
			msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_PING_PONG_MISMATCH);
		pr_err("stats_buf_divert: update put buf cnt fail\n");
		return rc;
	}

	if (rc > 0) {
		ISP_DBG("%s: vfe_id %d buf_id %d bufq %x put_cnt 1\n", __func__,
			vfe_dev->pdev->id, buf_index,
			stream_info->bufq_handle);
		return rc;
	}

	/* Program next buffer */
	rc = msm_isp_stats_cfg_ping_pong_address(vfe_dev, stream_info,
						pingpong_status);
	if (rc)
		return rc;

	if (drop_buffer && done_buf) {
		rc = vfe_dev->buf_mgr->ops->buf_done(
			vfe_dev->buf_mgr,
			done_buf->bufq_handle,
			done_buf->buf_idx, &ts->buf_time, frame_id, 0);
		if (rc == -EFAULT)
			msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_BUF_FATAL_ERROR);
		return rc;
	}

	if (done_buf) {
		stats_event->stats_buf_idxs
			[stream_info->stats_type] =
			done_buf->buf_idx;
		if (comp_stats_type_mask == NULL) {
			stats_event->stats_mask =
				1 << stream_info->stats_type;
			ISP_DBG("%s: stats frameid: 0x%x %d bufq %x\n",
				__func__, buf_event->frame_id,
				stream_info->stats_type, done_buf->bufq_handle);
			msm_isp_send_event(vfe_dev,
				ISP_EVENT_STATS_NOTIFY +
				stream_info->stats_type,
				buf_event);
		} else {
			*comp_stats_type_mask |=
				1 << stream_info->stats_type;
		}
	}

	return rc;
}

static int32_t msm_isp_stats_configure(struct vfe_device *vfe_dev,
	uint32_t stats_irq_mask, struct msm_isp_timestamp *ts,
	uint32_t pingpong_status, bool is_composite)
{
	int i, rc = 0;
	struct msm_isp_event_data buf_event;
	struct msm_isp_stats_event *stats_event = &buf_event.u.stats;
	struct msm_vfe_stats_stream *stream_info = NULL;
	uint32_t comp_stats_type_mask = 0;
	int result = 0;

	memset(&buf_event, 0, sizeof(struct msm_isp_event_data));
	buf_event.timestamp = ts->buf_time;
	buf_event.frame_id = vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++) {
		if (!(stats_irq_mask & (1 << i)))
			continue;
		stream_info = &vfe_dev->stats_data.stream_info[i];
		if (stream_info->state == STATS_INACTIVE) {
			pr_debug("%s: Warning! Stream already inactive. Drop irq handling\n",
				__func__);
			continue;
		}

		rc = msm_isp_stats_buf_divert(vfe_dev, ts,
				&buf_event, stream_info,
				is_composite ? &comp_stats_type_mask : NULL,
				pingpong_status);
		if (rc < 0) {
			pr_err("%s:%d failed: stats buf divert rc %d\n",
				__func__, __LINE__, rc);
			result = rc;
		}
	}
	if (is_composite && comp_stats_type_mask) {
		ISP_DBG("%s:vfe_id %d comp_stats frameid %x,comp_mask %x\n",
			__func__, vfe_dev->pdev->id, buf_event.frame_id,
			comp_stats_type_mask);
		stats_event->stats_mask = comp_stats_type_mask;
		msm_isp_send_event(vfe_dev,
			ISP_EVENT_COMP_STATS_NOTIFY, &buf_event);
		comp_stats_type_mask = 0;
	}
	return result;
}

void msm_isp_process_stats_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	uint32_t pingpong_status, struct msm_isp_timestamp *ts)
{
	int j, rc;
	uint32_t atomic_stats_mask = 0;
	uint32_t stats_comp_mask = 0, stats_irq_mask = 0;
	bool comp_flag = false;
	uint32_t num_stats_comp_mask =
		vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask;

	stats_comp_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_comp_mask(irq_status0, irq_status1);
	stats_irq_mask = vfe_dev->hw_info->vfe_ops.stats_ops.
		get_wm_mask(irq_status0, irq_status1);
	if (!(stats_comp_mask || stats_irq_mask))
		return;

	ISP_DBG("%s: vfe %d status: 0x%x\n", __func__, vfe_dev->pdev->id,
		irq_status0);

	/* Clear composite mask irq bits, they will be restored by comp mask */
	for (j = 0; j < num_stats_comp_mask; j++) {
		stats_irq_mask &= ~atomic_read(
			&vfe_dev->stats_data.stats_comp_mask[j]);
	}

	/* Process non-composite irq */
	if (stats_irq_mask) {
		rc = msm_isp_stats_configure(vfe_dev, stats_irq_mask, ts,
			pingpong_status, comp_flag);
	}

	/* Process composite irq */
	if (stats_comp_mask) {
		for (j = 0; j < num_stats_comp_mask; j++) {
			if (!(stats_comp_mask & (1 << j)))
				continue;

			atomic_stats_mask = atomic_read(
				&vfe_dev->stats_data.stats_comp_mask[j]);

			rc = msm_isp_stats_configure(vfe_dev, atomic_stats_mask,
				ts, pingpong_status, !comp_flag);
		}
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

	if (stats_idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
		pr_err("%s Invalid stats index %d", __func__, stats_idx);
		return -EINVAL;
	}

	stream_info = &stats_data->stream_info[stats_idx];
	if (stream_info->state != STATS_AVAILABLE) {
		pr_err("%s: Stats already requested\n", __func__);
		return rc;
	}

	if (stream_req_cmd->framedrop_pattern >= MAX_SKIP) {
		pr_err("%s: Invalid framedrop pattern\n", __func__);
		return rc;
	}

	if (stream_req_cmd->irq_subsample_pattern >= MAX_SKIP) {
		pr_err("%s: Invalid irq subsample pattern\n", __func__);
		return rc;
	}

	stream_info->session_id = stream_req_cmd->session_id;
	stream_info->stream_id = stream_req_cmd->stream_id;
	stream_info->composite_flag = stream_req_cmd->composite_flag;
	stream_info->stats_type = stream_req_cmd->stats_type;
	stream_info->buffer_offset = stream_req_cmd->buffer_offset;
	stream_info->framedrop_pattern = stream_req_cmd->framedrop_pattern;
	stream_info->init_stats_frame_drop = stream_req_cmd->init_frame_drop;
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
	int rc = -1;
	struct msm_vfe_stats_stream_request_cmd *stream_req_cmd = arg;
	struct msm_vfe_stats_stream *stream_info = NULL;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	uint32_t framedrop_period;
	uint32_t stats_idx;

	rc = msm_isp_stats_create_stream(vfe_dev, stream_req_cmd);
	if (rc < 0) {
		pr_err("%s: create stream failed\n", __func__);
		return rc;
	}

	stats_idx = STATS_IDX(stream_req_cmd->stream_handle);

	if (stats_idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
		pr_err("%s Invalid stats index %d", __func__, stats_idx);
		return -EINVAL;
	}

	stream_info = &stats_data->stream_info[stats_idx];

	framedrop_period = msm_isp_get_framedrop_period(
	   stream_req_cmd->framedrop_pattern);

	if (stream_req_cmd->framedrop_pattern == SKIP_ALL)
		stream_info->framedrop_pattern = 0x0;
	else
		stream_info->framedrop_pattern = 0x1;
	stream_info->framedrop_period = framedrop_period - 1;

	if (stream_info->init_stats_frame_drop == 0)
		vfe_dev->hw_info->vfe_ops.stats_ops.cfg_wm_reg(vfe_dev,
			stream_info);

	msm_isp_stats_cfg_stream_scratch(vfe_dev, stream_info,
					VFE_PING_FLAG);
	msm_isp_stats_cfg_stream_scratch(vfe_dev, stream_info,
					VFE_PONG_FLAG);
	return rc;
}

int msm_isp_release_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = -1;
	struct msm_vfe_stats_stream_cfg_cmd stream_cfg_cmd;
	struct msm_vfe_stats_stream_release_cmd *stream_release_cmd = arg;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	int stats_idx = STATS_IDX(stream_release_cmd->stream_handle);
	struct msm_vfe_stats_stream *stream_info = NULL;

	if (stats_idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
		pr_err("%s Invalid stats index %d", __func__, stats_idx);
		return -EINVAL;
	}

	stream_info = &stats_data->stream_info[stats_idx];
	if (stream_info->state == STATS_AVAILABLE) {
		pr_err("%s: stream already release\n", __func__);
		return rc;
	} else if (stream_info->state != STATS_INACTIVE) {
		stream_cfg_cmd.enable = 0;
		stream_cfg_cmd.num_streams = 1;
		stream_cfg_cmd.stream_handle[0] =
			stream_release_cmd->stream_handle;
		rc = msm_isp_cfg_stats_stream(vfe_dev, &stream_cfg_cmd);
	}

	vfe_dev->hw_info->vfe_ops.stats_ops.clear_wm_reg(vfe_dev, stream_info);
	memset(stream_info, 0, sizeof(struct msm_vfe_stats_stream));
	return 0;
}

static int msm_isp_init_stats_ping_pong_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream *stream_info)
{
	int rc = 0;

	stream_info->bufq_handle =
		vfe_dev->buf_mgr->ops->get_bufq_handle(
		vfe_dev->buf_mgr, stream_info->session_id,
		stream_info->stream_id);
	if (stream_info->bufq_handle == 0) {
		pr_err("%s: no buf configured for stream: 0x%x\n",
			__func__, stream_info->stream_handle);
		return -EINVAL;
	}

	if ((vfe_dev->is_split && vfe_dev->pdev->id == 1) ||
		!vfe_dev->is_split) {
		rc = msm_isp_stats_cfg_ping_pong_address(vfe_dev,
			stream_info, VFE_PING_FLAG);
		if (rc < 0) {
			pr_err("%s: No free buffer for ping\n", __func__);
			return rc;
		}
		rc = msm_isp_stats_cfg_ping_pong_address(vfe_dev,
			stream_info, VFE_PONG_FLAG);
		if (rc < 0) {
			pr_err("%s: No free buffer for pong\n", __func__);
			return rc;
		}
	}
	return rc;
}

void msm_isp_update_stats_framedrop_reg(struct vfe_device *vfe_dev)
{
	int i;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	struct msm_vfe_stats_stream *stream_info = NULL;

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++) {
		stream_info = &stats_data->stream_info[i];
		if (stream_info->state != STATS_ACTIVE)
			continue;

		if (stream_info->init_stats_frame_drop) {
			stream_info->init_stats_frame_drop--;
			if (stream_info->init_stats_frame_drop == 0) {
				vfe_dev->hw_info->vfe_ops.stats_ops.cfg_wm_reg(
					vfe_dev, stream_info);
			}
		}
	}
}

void msm_isp_stats_stream_update(struct vfe_device *vfe_dev)
{
	int i;
	uint32_t enable = 0;
	uint8_t comp_flag = 0;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	struct msm_vfe_stats_ops *stats_ops =
		&vfe_dev->hw_info->vfe_ops.stats_ops;

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++) {
		if (stats_data->stream_info[i].state == STATS_START_PENDING ||
				stats_data->stream_info[i].state ==
					STATS_STOP_PENDING) {
			enable = stats_data->stream_info[i].state ==
				STATS_START_PENDING ? 1 : 0;
			stats_data->stream_info[i].state =
				stats_data->stream_info[i].state ==
				STATS_START_PENDING ?
				STATS_STARTING : STATS_STOPPING;
			vfe_dev->hw_info->vfe_ops.stats_ops.enable_module(
				vfe_dev, BIT(i), enable);
			comp_flag = stats_data->stream_info[i].composite_flag;
			if (comp_flag)
				stats_ops->cfg_comp_mask(vfe_dev, BIT(i),
					(comp_flag - 1), enable);
		} else if (stats_data->stream_info[i].state == STATS_STARTING ||
			stats_data->stream_info[i].state == STATS_STOPPING) {
			stats_data->stream_info[i].state =
				stats_data->stream_info[i].state ==
				STATS_STARTING ? STATS_ACTIVE : STATS_INACTIVE;
		}
	}
	atomic_sub(1, &stats_data->stats_update);
	if (!atomic_read(&stats_data->stats_update))
		complete(&vfe_dev->stats_config_complete);
}

static int msm_isp_stats_wait_for_cfg_done(struct vfe_device *vfe_dev)
{
	int rc;

	init_completion(&vfe_dev->stats_config_complete);
	atomic_set(&vfe_dev->stats_data.stats_update, 2);
	rc = wait_for_completion_timeout(
		&vfe_dev->stats_config_complete,
		msecs_to_jiffies(VFE_MAX_CFG_TIMEOUT));
	if (rc == 0) {
		pr_err("%s: wait timeout\n", __func__);
		rc = -1;
	} else {
		rc = 0;
	}
	return rc;
}

static int msm_isp_stats_update_cgc_override(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd)
{
	int i;
	uint32_t stats_mask = 0, idx;

	if (stream_cfg_cmd->num_streams > MSM_ISP_STATS_MAX) {
		pr_err("%s invalid num_streams %d\n", __func__,
			stream_cfg_cmd->num_streams);
		return -EINVAL;
	}

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);

		if (idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s Invalid stats index %d", __func__, idx);
			return -EINVAL;
		}
		stats_mask |= 1 << idx;
	}

	if (vfe_dev->hw_info->vfe_ops.stats_ops.update_cgc_override) {
		vfe_dev->hw_info->vfe_ops.stats_ops.update_cgc_override(
			vfe_dev, stats_mask, stream_cfg_cmd->enable);
	}
	return 0;
}

int msm_isp_stats_reset(struct vfe_device *vfe_dev)
{
	int i = 0, rc = 0;
	struct msm_vfe_stats_stream *stream_info = NULL;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	struct msm_isp_timestamp timestamp;

	msm_isp_get_timestamp(&timestamp, vfe_dev);

	for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
		stream_info = &stats_data->stream_info[i];
		if (stream_info->state != STATS_ACTIVE)
			continue;

		rc = vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
			vfe_dev->pdev->id, stream_info->bufq_handle,
			MSM_ISP_BUFFER_FLUSH_ALL, &timestamp.buf_time,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		if (rc == -EFAULT) {
			msm_isp_halt_send_error(vfe_dev,
				ISP_EVENT_BUF_FATAL_ERROR);
			return rc;
		}
	}

	return rc;
}

int msm_isp_stats_restart(struct vfe_device *vfe_dev)
{
	int i = 0;
	struct msm_vfe_stats_stream *stream_info = NULL;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;

	for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
		stream_info = &stats_data->stream_info[i];
			if (stream_info->state < STATS_ACTIVE)
				continue;
		msm_isp_init_stats_ping_pong_reg(vfe_dev, stream_info);
	}

	return 0;
}

static int msm_isp_start_stats_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd)
{
	int i, rc = 0;
	uint32_t stats_mask = 0, idx;
	uint32_t comp_stats_mask[MAX_NUM_STATS_COMP_MASK] = {0};
	uint32_t num_stats_comp_mask = 0;
	struct msm_vfe_stats_stream *stream_info;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;

	if (stream_cfg_cmd->num_streams > MSM_ISP_STATS_MAX) {
		pr_err("%s invalid num_streams %d\n", __func__,
			stream_cfg_cmd->num_streams);
		return -EINVAL;
	}
	mutex_lock(&vfe_dev->buf_mgr->lock);

	num_stats_comp_mask =
		vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask;
	rc = vfe_dev->hw_info->vfe_ops.stats_ops.check_streams(
		stats_data->stream_info);
	if (rc < 0) {
		mutex_unlock(&vfe_dev->buf_mgr->lock);
		return rc;
	}

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);

		if (idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s Invalid stats index %d", __func__, idx);
			mutex_unlock(&vfe_dev->buf_mgr->lock);
			return -EINVAL;
		}

		stream_info = &stats_data->stream_info[idx];
		if (stream_info->stream_handle !=
				stream_cfg_cmd->stream_handle[i]) {
			pr_err("%s: Invalid stream handle: 0x%x received\n",
				__func__, stream_cfg_cmd->stream_handle[i]);
			continue;
		}

		if (stream_info->composite_flag > num_stats_comp_mask) {
			pr_err("%s: comp grp %d exceed max %d\n",
				__func__, stream_info->composite_flag,
				num_stats_comp_mask);
			mutex_unlock(&vfe_dev->buf_mgr->lock);
			return -EINVAL;
		}
		rc = msm_isp_init_stats_ping_pong_reg(vfe_dev, stream_info);
		if (rc < 0) {
			pr_err("%s: No buffer for stream%d\n", __func__, idx);
			mutex_unlock(&vfe_dev->buf_mgr->lock);
			return rc;
		}
		if (!stream_info->composite_flag)
			vfe_dev->hw_info->vfe_ops.stats_ops.
				cfg_wm_irq_mask(vfe_dev, stream_info);

		if (vfe_dev->axi_data.src_info[VFE_PIX_0].active)
			stream_info->state = STATS_START_PENDING;
		else
			stream_info->state = STATS_ACTIVE;

		stats_data->num_active_stream++;
		stats_mask |= 1 << idx;

		if (stream_info->composite_flag > 0)
			comp_stats_mask[stream_info->composite_flag-1] |=
				1 << idx;

		ISP_DBG("%s: stats_mask %x %x active streams %d\n",
			__func__, comp_stats_mask[0],
			comp_stats_mask[1],
			stats_data->num_active_stream);

	}
	mutex_unlock(&vfe_dev->buf_mgr->lock);

	if (vfe_dev->axi_data.src_info[VFE_PIX_0].active) {
		rc = msm_isp_stats_wait_for_cfg_done(vfe_dev);
	} else {
		vfe_dev->hw_info->vfe_ops.stats_ops.enable_module(
			vfe_dev, stats_mask, stream_cfg_cmd->enable);
		for (i = 0; i < num_stats_comp_mask; i++) {
			vfe_dev->hw_info->vfe_ops.stats_ops.cfg_comp_mask(
				vfe_dev, comp_stats_mask[i], i,  1);
		}
	}
	return rc;
}

static int msm_isp_stop_stats_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd)
{
	int i, rc = 0;
	uint32_t stats_mask = 0, idx;
	uint32_t comp_stats_mask[MAX_NUM_STATS_COMP_MASK] = {0};
	uint32_t num_stats_comp_mask = 0;
	struct msm_vfe_stats_stream *stream_info;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	struct msm_isp_timestamp timestamp;

	msm_isp_get_timestamp(&timestamp, vfe_dev);

	num_stats_comp_mask =
		vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {

		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);

		if (idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s Invalid stats index %d", __func__, idx);
			return -EINVAL;
		}

		stream_info = &stats_data->stream_info[idx];
		if (stream_info->stream_handle !=
				stream_cfg_cmd->stream_handle[i]) {
			pr_err("%s: Invalid stream handle: 0x%x received\n",
				__func__, stream_cfg_cmd->stream_handle[i]);
			continue;
		}

		if (stream_info->composite_flag > num_stats_comp_mask) {
			pr_err("%s: comp grp %d exceed max %d\n",
				__func__, stream_info->composite_flag,
				num_stats_comp_mask);
			return -EINVAL;
		}

		if (!stream_info->composite_flag)
			vfe_dev->hw_info->vfe_ops.stats_ops.
				clear_wm_irq_mask(vfe_dev, stream_info);

		if (vfe_dev->axi_data.src_info[VFE_PIX_0].active)
			stream_info->state = STATS_STOP_PENDING;
		else
			stream_info->state = STATS_INACTIVE;

		stats_data->num_active_stream--;
		stats_mask |= 1 << idx;

		if (stream_info->composite_flag > 0)
			comp_stats_mask[stream_info->composite_flag-1] |=
				1 << idx;

		msm_isp_stats_cfg_stream_scratch(vfe_dev, stream_info,
						VFE_PING_FLAG);
		msm_isp_stats_cfg_stream_scratch(vfe_dev, stream_info,
						VFE_PONG_FLAG);

		ISP_DBG("%s: stats_mask %x %x active streams %d\n",
			__func__, comp_stats_mask[0],
			comp_stats_mask[1],
			stats_data->num_active_stream);
	}

	if (vfe_dev->axi_data.src_info[VFE_PIX_0].active) {
		rc = msm_isp_stats_wait_for_cfg_done(vfe_dev);
	} else {
		vfe_dev->hw_info->vfe_ops.stats_ops.enable_module(
			vfe_dev, stats_mask, stream_cfg_cmd->enable);
		for (i = 0; i < num_stats_comp_mask; i++) {
			vfe_dev->hw_info->vfe_ops.stats_ops.cfg_comp_mask(
			   vfe_dev, comp_stats_mask[i], i, 0);
		}
	}

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);

		if (idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s Invalid stats index %d", __func__, idx);
			return -EINVAL;
		}

		stream_info = &stats_data->stream_info[idx];
		rc = vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
			vfe_dev->pdev->id, stream_info->bufq_handle,
			MSM_ISP_BUFFER_FLUSH_ALL, &timestamp.buf_time,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		if (rc == -EFAULT) {
			msm_isp_halt_send_error(vfe_dev,
				ISP_EVENT_BUF_FATAL_ERROR);
			return rc;
		}
	}
	return rc;
}

int msm_isp_cfg_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;

	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd = arg;

	if (vfe_dev->stats_data.num_active_stream == 0)
		vfe_dev->hw_info->vfe_ops.stats_ops.cfg_ub(vfe_dev);

	if (stream_cfg_cmd->num_streams > MSM_ISP_STATS_MAX) {
		pr_err("%s invalid num_streams %d\n", __func__,
			stream_cfg_cmd->num_streams);
		return -EINVAL;
	}

	if (stream_cfg_cmd->enable) {
		msm_isp_stats_update_cgc_override(vfe_dev, stream_cfg_cmd);

		rc = msm_isp_start_stats_stream(vfe_dev, stream_cfg_cmd);
	} else {
		rc = msm_isp_stop_stats_stream(vfe_dev, stream_cfg_cmd);

		msm_isp_stats_update_cgc_override(vfe_dev, stream_cfg_cmd);
	}

	return rc;
}

int msm_isp_update_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i;
	struct msm_vfe_stats_stream *stream_info;
	struct msm_vfe_stats_shared_data *stats_data = &vfe_dev->stats_data;
	struct msm_vfe_axi_stream_update_cmd *update_cmd = arg;
	struct msm_vfe_axi_stream_cfg_update_info *update_info = NULL;
	struct msm_isp_sw_framskip *sw_skip_info = NULL;

	if (update_cmd->num_streams > MSM_ISP_STATS_MAX) {
		pr_err("%s: Invalid num_streams %d\n",
			__func__, update_cmd->num_streams);
		return -EINVAL;
	}

	/* validate request */
	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = (struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
		/* check array reference bounds */
		if (STATS_IDX(update_info->stream_handle)
			>= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s: stats idx %d out of bound!", __func__,
			STATS_IDX(update_info->stream_handle));
			return -EINVAL;
		}
	}

	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = (struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
		stream_info = &stats_data->stream_info[
			STATS_IDX(
			update_info->stream_handle)];
		if (stream_info->stream_handle !=
			update_info->stream_handle) {
			pr_err("%s: stats stream handle %x %x mismatch!\n",
				__func__, stream_info->stream_handle,
				update_info->stream_handle);
			continue;
		}

		switch (update_cmd->update_type) {
		case UPDATE_STREAM_STATS_FRAMEDROP_PATTERN: {
			uint32_t framedrop_period =
				msm_isp_get_framedrop_period(
				   update_info->skip_pattern);
			if (update_info->skip_pattern ==
				SKIP_ALL)
				stream_info->framedrop_pattern = 0x0;
			else
				stream_info->framedrop_pattern = 0x1;
			stream_info->framedrop_period = framedrop_period - 1;
			if (stream_info->init_stats_frame_drop == 0)
				vfe_dev->hw_info->vfe_ops.stats_ops.cfg_wm_reg(
					vfe_dev, stream_info);
			break;
		}
		case UPDATE_STREAM_SW_FRAME_DROP: {
			sw_skip_info =
			&update_info->sw_skip_info;
			if (!stream_info->sw_skip.stream_src_mask)
				stream_info->sw_skip = *sw_skip_info;

			if (sw_skip_info->stats_type_mask != 0) {
				/* No image buffer skip, only stats skip */
				pr_debug("%s:%x skip type %x mode %d min %d max %d\n",
					__func__, stream_info->stream_id,
					sw_skip_info->stats_type_mask,
					sw_skip_info->skip_mode,
					sw_skip_info->min_frame_id,
					sw_skip_info->max_frame_id);
				stream_info->sw_skip.stats_type_mask =
					sw_skip_info->stats_type_mask;
			}
			break;
		}

		default:
			pr_err("%s: Invalid update type\n", __func__);
			return -EINVAL;
		}
	}
	return rc;
}

void msm_isp_stats_disable(struct vfe_device *vfe_dev)
{
	int i;
	unsigned int mask = 0;

	if (!vfe_dev) {
		pr_err("%s:  error NULL ptr\n", __func__);
		return;
	}

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++)
		mask |= 1 << i;

	vfe_dev->hw_info->vfe_ops.stats_ops.enable_module(vfe_dev, mask, 0);
}
