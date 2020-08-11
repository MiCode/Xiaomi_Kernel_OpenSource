// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
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
#include <media/msmb_isp.h>
#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp_stats_util.h"

static inline void msm_isp_stats_cfg_wm_scratch(struct vfe_device *vfe_dev,
				struct msm_vfe_stats_stream *stream_info,
				uint32_t pingpong_status)
{
	vfe_dev->hw_info->vfe_ops.stats_ops.update_ping_pong_addr(
		vfe_dev, stream_info,
		pingpong_status, vfe_dev->buf_mgr->scratch_buf_stats_addr,
		SZ_32M);
}

static inline void msm_isp_stats_cfg_stream_scratch(
				struct msm_vfe_stats_stream *stream_info,
				uint32_t pingpong_status)
{
	uint32_t stats_idx = STATS_IDX(stream_info->stream_handle[0]);
	uint32_t pingpong_bit;
	uint32_t stats_pingpong_offset;
	struct vfe_device *vfe_dev;
	int i;

	stats_pingpong_offset =
			stream_info->vfe_dev[
			0]->hw_info->stats_hw_info->stats_ping_pong_offset[
			stats_idx];
	pingpong_bit = (~(pingpong_status >> stats_pingpong_offset) & 0x1);

	for (i = 0; i < stream_info->num_isp; i++) {
		vfe_dev = stream_info->vfe_dev[i];
		msm_isp_stats_cfg_wm_scratch(vfe_dev, stream_info,
						pingpong_status);
	}

	stream_info->buf[pingpong_bit] = NULL;
}

static int msm_isp_composite_stats_irq(struct vfe_device *vfe_dev,
				struct msm_vfe_stats_stream *stream_info,
				enum msm_isp_comp_irq_types irq)
{
	/* for dual vfe mode need not check anything*/
	if (vfe_dev->dual_vfe_sync_mode) {
		stream_info->composite_irq[irq] = 0;
		return 0;
	}

	/* interrupt recv on same vfe w/o recv on other vfe */
	if (stream_info->composite_irq[irq] & (1 << vfe_dev->pdev->id)) {
		pr_err("%s: irq %d out of sync for dual vfe on vfe %d\n",
			__func__, irq, vfe_dev->pdev->id);
		return -EFAULT;
	}

	stream_info->composite_irq[irq] |= (1 << vfe_dev->pdev->id);
	if (stream_info->composite_irq[irq] != stream_info->vfe_mask)
		return 1;

	stream_info->composite_irq[irq] = 0;

	return 0;
}

static int msm_isp_stats_cfg_ping_pong_address(
	struct msm_vfe_stats_stream *stream_info, uint32_t pingpong_status)
{
	int rc = -1;
	struct msm_isp_buffer *buf = NULL;
	uint32_t bufq_handle = stream_info->bufq_handle;
	uint32_t stats_idx = STATS_IDX(stream_info->stream_handle[0]);
	struct vfe_device *vfe_dev = stream_info->vfe_dev[0];
	uint32_t stats_pingpong_offset;
	uint32_t pingpong_bit;
	int k;

	if (stats_idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type ||
		stats_idx >= MSM_ISP_STATS_MAX) {
		pr_err("%s Invalid stats index %d\n", __func__, stats_idx);
		return -EINVAL;
	}
	stats_pingpong_offset =
			vfe_dev->hw_info->stats_hw_info->stats_ping_pong_offset
						[stats_idx];
	pingpong_bit = (~(pingpong_status >> stats_pingpong_offset) & 0x1);
	/* if buffer already exists then no need to replace */
	if (stream_info->buf[pingpong_bit])
		return 0;

	rc = vfe_dev->buf_mgr->ops->get_buf(vfe_dev->buf_mgr,
			vfe_dev->pdev->id, bufq_handle,
			MSM_ISP_INVALID_BUF_INDEX, &buf);
	if (rc == -EFAULT) {
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_BUF_FATAL_ERROR);
		return rc;
	}
	if (rc < 0 || NULL == buf) {
		for (k = 0; k < stream_info->num_isp; k++)
			stream_info->vfe_dev[
				k]->error_info.stats_framedrop_count[
					stats_idx]++;
	}

	if (buf && buf->num_planes != 1) {
		pr_err("%s: Invalid buffer\n", __func__);
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_BUF_FATAL_ERROR);
		rc = -EINVAL;
		goto buf_error;
	}

	if (!buf) {
		msm_isp_stats_cfg_stream_scratch(stream_info,
							pingpong_status);
		return 0;
	}
	for (k = 0; k < stream_info->num_isp; k++) {
		vfe_dev = stream_info->vfe_dev[k];
		vfe_dev->hw_info->vfe_ops.stats_ops.update_ping_pong_addr(
			vfe_dev, stream_info, pingpong_status,
			buf->mapped_info[0].paddr +
			stream_info->buffer_offset[k],
			buf->mapped_info[0].len);
	}
	stream_info->buf[pingpong_bit] = buf;
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
	int vfe_idx;
	unsigned long flags;

	if (!vfe_dev || !ts || !buf_event || !stream_info) {
		pr_err("%s:%d failed: invalid params %pK %pK %pK %pK\n",
			__func__, __LINE__, vfe_dev, ts, buf_event,
			stream_info);
		return -EINVAL;
	}
	frame_id = vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;

	spin_lock_irqsave(&stream_info->lock, flags);

	sw_skip = &stream_info->sw_skip;
	stats_event = &buf_event->u.stats;

	if (sw_skip->stats_type_mask &
		(1 << stream_info->stats_type)) {
		/* Hw stream output of this src is requested
		 * for drop
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
	vfe_idx = msm_isp_get_vfe_idx_for_stats_stream(vfe_dev, stream_info);
	stats_idx = STATS_IDX(stream_info->stream_handle[vfe_idx]);

	stats_pingpong_offset =
			vfe_dev->hw_info->stats_hw_info->stats_ping_pong_offset[
			stats_idx];
	pingpong_bit = (~(pingpong_status >> stats_pingpong_offset) & 0x1);

	rc = msm_isp_composite_stats_irq(vfe_dev, stream_info,
			MSM_ISP_COMP_IRQ_PING_BUFDONE + pingpong_bit);
	if (rc) {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		if (rc < 0)
			msm_isp_halt_send_error(vfe_dev,
				ISP_EVENT_PING_PONG_MISMATCH);
		return rc;
	}

	done_buf = stream_info->buf[pingpong_bit];
	/* Program next buffer */
	stream_info->buf[pingpong_bit] = NULL;
	rc = msm_isp_stats_cfg_ping_pong_address(stream_info,
						pingpong_status);
	spin_unlock_irqrestore(&stream_info->lock, flags);

	if (!done_buf)
		return rc;

	buf_index = done_buf->buf_idx;
	if (drop_buffer) {
		vfe_dev->buf_mgr->ops->put_buf(
			vfe_dev->buf_mgr,
			done_buf->bufq_handle,
			done_buf->buf_idx);
	} else {
		/* divert native buffers */
		vfe_dev->buf_mgr->ops->buf_divert(vfe_dev->buf_mgr,
			done_buf->bufq_handle, done_buf->buf_idx,
			&ts->buf_time, frame_id);
	}
	stats_event->stats_buf_idxs
		[stream_info->stats_type] =
		done_buf->buf_idx;

	stats_event->pd_stats_idx = 0xF;
	if (stream_info->stats_type == MSM_ISP_STATS_BF) {
		spin_lock_irqsave(&vfe_dev->common_data->common_dev_data_lock,
							flags);
		stats_event->pd_stats_idx = vfe_dev->common_data->pd_buf_idx;
		vfe_dev->common_data->pd_buf_idx = 0xF;
		spin_unlock_irqrestore(
			&vfe_dev->common_data->common_dev_data_lock, flags);
	}
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
	buf_event.timestamp = ts->event_time;
	buf_event.mono_timestamp = ts->buf_time;

	buf_event.frame_id = vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++) {
		if (!(stats_irq_mask & (1 << i)))
			continue;
		stream_info = msm_isp_get_stats_stream_common_data(vfe_dev, i);
		if (stream_info->state == STATS_INACTIVE ||
			stream_info->state == STATS_STOPPING) {
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
	uint32_t irq_status0, uint32_t irq_status1, uint32_t dual_irq_status,
	uint32_t pingpong_status, struct msm_isp_timestamp *ts)
{
	int j, rc;
	uint32_t atomic_stats_mask = 0;
	uint32_t stats_comp_mask = 0, stats_irq_mask = 0;
	bool comp_flag = false;
	uint32_t num_stats_comp_mask =
		vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask;

	if (vfe_dev->dual_vfe_sync_mode)
		stats_comp_mask =
		vfe_dev->hw_info->vfe_ops.stats_ops.get_comp_mask(
			dual_irq_status, irq_status1);
	else
		stats_comp_mask =
		vfe_dev->hw_info->vfe_ops.stats_ops.get_comp_mask(
			irq_status0, irq_status1);

	stats_irq_mask =
		vfe_dev->hw_info->vfe_ops.stats_ops.get_wm_mask(
			irq_status0, irq_status1);

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
	struct msm_vfe_stats_stream_request_cmd *stream_req_cmd,
	struct msm_vfe_stats_stream *stream_info)
{
	int rc = 0;
	uint32_t stats_idx;
	uint32_t framedrop_pattern;
	uint32_t framedrop_period;
	int i;

	stats_idx = vfe_dev->hw_info->vfe_ops.stats_ops.get_stats_idx(
					stream_req_cmd->stats_type);

	if (!(vfe_dev->hw_info->stats_hw_info->stats_capability_mask &
		(1 << stream_req_cmd->stats_type))) {
		pr_err("%s: Stats type not supported\n", __func__);
		return rc;
	}

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

	if (stream_req_cmd->composite_flag >
		vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask) {
		pr_err("%s: comp grp %d exceed max %d\n",
			__func__, stream_req_cmd->composite_flag,
			vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask);
		return -EINVAL;
	}

	if (stream_info->num_isp == 0) {
		stream_info->session_id = stream_req_cmd->session_id;
		stream_info->stream_id = stream_req_cmd->stream_id;
		stream_info->composite_flag = stream_req_cmd->composite_flag;
		stream_info->stats_type = stream_req_cmd->stats_type;
		framedrop_pattern = stream_req_cmd->framedrop_pattern;
		if (framedrop_pattern == SKIP_ALL)
			framedrop_pattern = 0;
		else
			framedrop_pattern = 1;
		stream_info->framedrop_pattern = framedrop_pattern;
		stream_info->init_stats_frame_drop =
			stream_req_cmd->init_frame_drop;
		stream_info->irq_subsample_pattern =
			stream_req_cmd->irq_subsample_pattern;
		framedrop_period = msm_isp_get_framedrop_period(
			stream_req_cmd->framedrop_pattern);
		stream_info->framedrop_period = framedrop_period;
	} else {
		if (stream_info->vfe_mask & (1 << vfe_dev->pdev->id)) {
			pr_err("%s: stats %d already requested for vfe %d\n",
				__func__, stats_idx, vfe_dev->pdev->id);
			return -EINVAL;
		}
		if (stream_info->session_id != stream_req_cmd->session_id)
			rc = -EINVAL;
		if (stream_info->session_id != stream_req_cmd->session_id)
			rc = -EINVAL;
		if (stream_info->composite_flag !=
			stream_req_cmd->composite_flag)
			rc = -EINVAL;
		if (stream_info->stats_type != stream_req_cmd->stats_type)
			rc = -EINVAL;
		framedrop_pattern = stream_req_cmd->framedrop_pattern;
		if (framedrop_pattern == SKIP_ALL)
			framedrop_pattern = 0;
		else
			framedrop_pattern = 1;
		if (stream_info->framedrop_pattern != framedrop_pattern)
			rc = -EINVAL;
		framedrop_period = msm_isp_get_framedrop_period(
			stream_req_cmd->framedrop_pattern);
		if (stream_info->framedrop_period != framedrop_period)
			rc = -EINVAL;
		if (rc) {
			pr_err("%s: Stats stream param mismatch between vfe\n",
				__func__);
			return rc;
		}
	}
	stream_info->buffer_offset[stream_info->num_isp] =
					stream_req_cmd->buffer_offset;
	stream_info->vfe_dev[stream_info->num_isp] = vfe_dev;
	stream_info->vfe_mask |= (1 << vfe_dev->pdev->id);
	stream_info->num_isp++;
	if (!vfe_dev->is_split || stream_info->num_isp == MAX_VFE) {
		stream_info->state = STATS_INACTIVE;
		for (i = 0; i < MSM_ISP_COMP_IRQ_MAX; i++)
			stream_info->composite_irq[i] = 0;
	}

	if ((vfe_dev->stats_data.stream_handle_cnt << 8) == 0)
		vfe_dev->stats_data.stream_handle_cnt++;

	stream_req_cmd->stream_handle =
		(++vfe_dev->stats_data.stream_handle_cnt) << 8 | stats_idx;

	stream_info->stream_handle[stream_info->num_isp - 1] =
				stream_req_cmd->stream_handle;
	return 0;
}

int msm_isp_request_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = -1;
	struct msm_vfe_stats_stream_request_cmd *stream_req_cmd = arg;
	struct msm_vfe_stats_stream *stream_info = NULL;
	uint32_t stats_idx;

	stats_idx = vfe_dev->hw_info->vfe_ops.stats_ops.get_stats_idx(
					stream_req_cmd->stats_type);

	if (stats_idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
		pr_err("%s Invalid stats index %d\n", __func__, stats_idx);
		return -EINVAL;
	}

	stream_info = msm_isp_get_stats_stream_common_data(vfe_dev, stats_idx);

	rc = msm_isp_stats_create_stream(vfe_dev, stream_req_cmd, stream_info);
	if (rc < 0) {
		pr_err("%s: create stream failed\n", __func__);
		return rc;
	}

	if (stream_info->init_stats_frame_drop == 0)
		vfe_dev->hw_info->vfe_ops.stats_ops.cfg_wm_reg(vfe_dev,
			stream_info);

	if (stream_info->state == STATS_INACTIVE) {
		msm_isp_stats_cfg_stream_scratch(stream_info,
					VFE_PING_FLAG);
		msm_isp_stats_cfg_stream_scratch(stream_info,
					VFE_PONG_FLAG);
	}
	return rc;
}

int msm_isp_release_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = -1;
	struct msm_vfe_stats_stream_cfg_cmd stream_cfg_cmd;
	struct msm_vfe_stats_stream_release_cmd *stream_release_cmd = arg;
	int stats_idx = STATS_IDX(stream_release_cmd->stream_handle);
	struct msm_vfe_stats_stream *stream_info = NULL;
	int vfe_idx;
	int i;
	int k;

	if (stats_idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
		pr_err("%s Invalid stats index %d\n", __func__, stats_idx);
		return -EINVAL;
	}

	stream_info = msm_isp_get_stats_stream_common_data(vfe_dev, stats_idx);
	vfe_idx = msm_isp_get_vfe_idx_for_stats_stream_user(
					vfe_dev, stream_info);
	if (vfe_idx == -ENOTTY || stream_info->stream_handle[vfe_idx] !=
			stream_release_cmd->stream_handle) {
		pr_err("%s: Invalid stream handle %x, expected %x\n",
			__func__, stream_release_cmd->stream_handle,
			vfe_idx != -ENOTTY ?
			stream_info->stream_handle[vfe_idx] : 0);
		return -EINVAL;
	}
	if (stream_info->state == STATS_AVAILABLE) {
		pr_err("%s: stream already release\n", __func__);
		return rc;
	}
	vfe_dev->hw_info->vfe_ops.stats_ops.clear_wm_reg(vfe_dev, stream_info);

	if (stream_info->state != STATS_INACTIVE) {
		stream_cfg_cmd.enable = 0;
		stream_cfg_cmd.num_streams = 1;
		stream_cfg_cmd.stream_handle[0] =
			stream_release_cmd->stream_handle;
		msm_isp_cfg_stats_stream(vfe_dev, &stream_cfg_cmd);
	}

	for (i = vfe_idx, k = vfe_idx + 1; k < stream_info->num_isp; k++, i++) {
		stream_info->vfe_dev[i] = stream_info->vfe_dev[k];
		stream_info->stream_handle[i] = stream_info->stream_handle[k];
		stream_info->buffer_offset[i] = stream_info->buffer_offset[k];
	}

	stream_info->num_isp--;

	stream_info->vfe_dev[stream_info->num_isp] = NULL;
	stream_info->stream_handle[stream_info->num_isp] = 0;
	stream_info->buffer_offset[stream_info->num_isp] = 0;
	stream_info->vfe_mask &= ~(1 << vfe_dev->pdev->id);
	if (stream_info->num_isp == 0)
		stream_info->state = STATS_AVAILABLE;

	return 0;
}

void msm_isp_stop_all_stats_stream(struct vfe_device *vfe_dev)
{
	struct msm_vfe_stats_stream_cfg_cmd stream_cfg_cmd;
	struct msm_vfe_stats_stream *stream_info;
	int i;
	int vfe_idx;
	unsigned long flags;

	stream_cfg_cmd.enable = 0;
	stream_cfg_cmd.num_streams = 0;

	for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
		stream_info =  msm_isp_get_stats_stream_common_data(vfe_dev, i);
		spin_lock_irqsave(&stream_info->lock, flags);
		if (stream_info->state == STATS_AVAILABLE ||
			stream_info->state == STATS_INACTIVE) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			continue;
		}
		vfe_idx = msm_isp_get_vfe_idx_for_stats_stream_user(vfe_dev,
							stream_info);
		if (vfe_idx == -ENOTTY) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			continue;
		}
		stream_cfg_cmd.stream_handle[
			stream_cfg_cmd.num_streams] =
			stream_info->stream_handle[vfe_idx];
		stream_cfg_cmd.num_streams++;
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}
	if (stream_cfg_cmd.num_streams)
		msm_isp_cfg_stats_stream(vfe_dev, &stream_cfg_cmd);
}

void msm_isp_release_all_stats_stream(struct vfe_device *vfe_dev)
{
	struct msm_vfe_stats_stream_release_cmd
				stream_release_cmd[MSM_ISP_STATS_MAX];
	struct msm_vfe_stats_stream *stream_info;
	int i;
	int vfe_idx;
	int num_stream = 0;
	unsigned long flags;

	msm_isp_stop_all_stats_stream(vfe_dev);

	for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
		stream_info =  msm_isp_get_stats_stream_common_data(vfe_dev, i);
		spin_lock_irqsave(&stream_info->lock, flags);
		if (stream_info->state == STATS_AVAILABLE) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			continue;
		}
		vfe_idx = msm_isp_get_vfe_idx_for_stats_stream_user(vfe_dev,
							stream_info);
		if (vfe_idx == -ENOTTY) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			continue;
		}
		stream_release_cmd[num_stream++].stream_handle =
				stream_info->stream_handle[vfe_idx];
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	for (i = 0; i < num_stream; i++)
		msm_isp_release_stats_stream(vfe_dev, &stream_release_cmd[i]);
}

static int msm_isp_init_stats_ping_pong_reg(
	struct msm_vfe_stats_stream *stream_info)
{
	int rc = 0;

	stream_info->bufq_handle =
		stream_info->vfe_dev[0]->buf_mgr->ops->get_bufq_handle(
		stream_info->vfe_dev[0]->buf_mgr, stream_info->session_id,
		stream_info->stream_id);
	if (stream_info->bufq_handle == 0) {
		pr_err("%s: no buf configured for stream: 0x%x\n",
			__func__, stream_info->stream_handle[0]);
		return -EINVAL;
	}

	rc = msm_isp_stats_cfg_ping_pong_address(
		stream_info, VFE_PING_FLAG);
	if (rc < 0) {
		pr_err("%s: No free buffer for ping\n", __func__);
		return rc;
	}
	rc = msm_isp_stats_cfg_ping_pong_address(
		stream_info, VFE_PONG_FLAG);
	if (rc < 0) {
		pr_err("%s: No free buffer for pong\n", __func__);
		return rc;
	}
	return rc;
}

static void __msm_isp_update_stats_framedrop_reg(
	struct msm_vfe_stats_stream *stream_info)
{
	int k;
	struct vfe_device *vfe_dev;

	if (!stream_info->init_stats_frame_drop)
		return;
	stream_info->init_stats_frame_drop--;
	if (stream_info->init_stats_frame_drop)
		return;

	for (k = 0; k < stream_info->num_isp; k++) {
		vfe_dev = stream_info->vfe_dev[k];
		vfe_dev->hw_info->vfe_ops.stats_ops.cfg_wm_reg(vfe_dev,
						stream_info);

	}
}

static void __msm_isp_stats_stream_update(
	struct msm_vfe_stats_stream *stream_info)
{
	uint32_t enable = 0;
	uint8_t comp_flag = 0;
	int k;
	struct vfe_device *vfe_dev;
	int index = STATS_IDX(stream_info->stream_handle[0]);

	switch (stream_info->state) {
	case STATS_INACTIVE:
	case STATS_ACTIVE:
	case STATS_AVAILABLE:
		break;
	case STATS_START_PENDING:
		enable = 1;
	case STATS_STOP_PENDING:
		stream_info->state =
			(stream_info->state == STATS_START_PENDING ?
			STATS_STARTING : STATS_STOPPING);
		for (k = 0; k < stream_info->num_isp; k++) {
			vfe_dev = stream_info->vfe_dev[k];
			vfe_dev->hw_info->vfe_ops.stats_ops.enable_module(
				vfe_dev, BIT(index), enable);
			comp_flag = stream_info->composite_flag;
			if (comp_flag) {
				vfe_dev->hw_info->vfe_ops.stats_ops
					.cfg_comp_mask(vfe_dev, BIT(index),
					(comp_flag - 1), enable);
			} else {
				if (enable)
					vfe_dev->hw_info->vfe_ops.stats_ops
						.cfg_wm_irq_mask(vfe_dev,
							stream_info);
				else
					vfe_dev->hw_info->vfe_ops.stats_ops
							.clear_wm_irq_mask(
							vfe_dev, stream_info);
			}
		}
		break;
	case STATS_STARTING:
		stream_info->state = STATS_ACTIVE;
		complete_all(&stream_info->active_comp);
		break;
	case STATS_STOPPING:
		stream_info->state = STATS_INACTIVE;
		complete_all(&stream_info->inactive_comp);
		break;
	}
}


void msm_isp_stats_stream_update(struct vfe_device *vfe_dev)
{
	int i;
	struct msm_vfe_stats_stream *stream_info;
	unsigned long flags;

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++) {
		stream_info = msm_isp_get_stats_stream_common_data(vfe_dev, i);
		if (stream_info->state == STATS_AVAILABLE ||
			stream_info->state == STATS_INACTIVE)
			continue;
		spin_lock_irqsave(&stream_info->lock, flags);
		__msm_isp_stats_stream_update(stream_info);
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}
}

void msm_isp_process_stats_reg_upd_epoch_irq(struct vfe_device *vfe_dev,
			enum msm_isp_comp_irq_types irq)
{
	int i;
	struct msm_vfe_stats_stream *stream_info;
	unsigned long flags;
	int rc;

	for (i = 0; i < vfe_dev->hw_info->stats_hw_info->num_stats_type; i++) {
		stream_info = msm_isp_get_stats_stream_common_data(vfe_dev, i);
		if (stream_info->state == STATS_AVAILABLE ||
			stream_info->state == STATS_INACTIVE)
			continue;

		spin_lock_irqsave(&stream_info->lock, flags);

		rc = msm_isp_composite_stats_irq(vfe_dev, stream_info, irq);

		if (rc) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			if (-EFAULT == rc) {
				msm_isp_halt_send_error(vfe_dev,
						ISP_EVENT_PING_PONG_MISMATCH);
				return;
			}
			continue;
		}

		if (irq == MSM_ISP_COMP_IRQ_REG_UPD)
			__msm_isp_stats_stream_update(stream_info);
		else if (irq == MSM_ISP_COMP_IRQ_EPOCH &&
			stream_info->state == STATS_ACTIVE)
			__msm_isp_update_stats_framedrop_reg(stream_info);

		spin_unlock_irqrestore(&stream_info->lock, flags);
	}
}

static int msm_isp_stats_wait_for_stream_cfg_done(
		struct msm_vfe_stats_stream *stream_info,
		int active)
{
	int rc = -1;

	if (active && stream_info->state == STATS_ACTIVE)
		rc = 0;
	if (!active && stream_info->state == STATS_INACTIVE)
		rc = 0;
	if (rc == 0)
		return rc;

	rc = wait_for_completion_timeout(active ? &stream_info->active_comp :
		&stream_info->inactive_comp,
		msecs_to_jiffies(VFE_MAX_CFG_TIMEOUT));
	if (rc <= 0) {
		rc = rc ? rc : -ETIMEDOUT;
		pr_err("%s: wait for stats stream %x idx %d state %d active %d config failed %d\n",
			__func__, stream_info->stream_id,
			STATS_IDX(stream_info->stream_handle[0]),
			stream_info->state, active, rc);
	} else {
		rc = 0;
	}
	return rc;
}

static int msm_isp_stats_wait_for_streams(
		struct msm_vfe_stats_stream **streams,
		int num_stream, int active)
{
	int rc = 0;
	int i;
	struct msm_vfe_stats_stream *stream_info;

	for (i = 0; i < num_stream; i++) {
		stream_info = streams[i];
		rc |= msm_isp_stats_wait_for_stream_cfg_done(stream_info,
								active);
	}
	return rc;
}

static int msm_isp_stats_update_cgc_override(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd)
{
	int i;
	uint32_t stats_mask = 0, idx;
	struct vfe_device *update_vfes[MAX_VFE] = {NULL, NULL};
	struct msm_vfe_stats_stream *stream_info;
	int k;

	if (stream_cfg_cmd->num_streams > MSM_ISP_STATS_MAX) {
		pr_err("%s invalid num_streams %d\n", __func__,
			stream_cfg_cmd->num_streams);
		return -EINVAL;
	}

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);

		if (idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s Invalid stats index %d\n", __func__, idx);
			return -EINVAL;
		}
		stream_info =  msm_isp_get_stats_stream_common_data(vfe_dev,
					idx);
		if (stream_info->state == STATS_AVAILABLE)
			continue;

		/*
		 * we update cgc after making streams inactive or before
		 * starting streams, so stream should be in inactive state
		 */
		if (stream_info->state == STATS_INACTIVE)
			stats_mask |= 1 << idx;
		for (k = 0; k < stream_info->num_isp; k++) {
			if (update_vfes[stream_info->vfe_dev[k]->pdev->id])
				continue;
			update_vfes[stream_info->vfe_dev[k]->pdev->id] =
				stream_info->vfe_dev[k];
		}
	}

	for (k = 0; k < MAX_VFE; k++) {
		if (!update_vfes[k])
			continue;
		vfe_dev = update_vfes[k];
		if (vfe_dev->hw_info->vfe_ops.stats_ops.update_cgc_override) {
			vfe_dev->hw_info->vfe_ops.stats_ops.update_cgc_override(
				vfe_dev, stats_mask, stream_cfg_cmd->enable);
		}
	}
	return 0;
}

int msm_isp_stats_reset(struct vfe_device *vfe_dev)
{
	int i = 0, rc = 0;
	struct msm_vfe_stats_stream *stream_info = NULL;
	struct msm_isp_timestamp timestamp;
	unsigned long flags;

	msm_isp_get_timestamp(&timestamp, vfe_dev);

	for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
		stream_info = msm_isp_get_stats_stream_common_data(
				vfe_dev, i);
		if (stream_info->state == STATS_AVAILABLE ||
			stream_info->state == STATS_INACTIVE)
			continue;

		if (stream_info->num_isp > 1 &&
			vfe_dev->pdev->id == ISP_VFE0)
			continue;
		spin_lock_irqsave(&stream_info->lock, flags);
		msm_isp_stats_cfg_stream_scratch(stream_info,
						VFE_PING_FLAG);
		msm_isp_stats_cfg_stream_scratch(stream_info,
						VFE_PONG_FLAG);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		rc = vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
			stream_info->bufq_handle,
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
	unsigned long flags;
	int j;

	for (i = 0; i < MSM_ISP_STATS_MAX; i++) {
		stream_info = msm_isp_get_stats_stream_common_data(
				vfe_dev, i);
		if (stream_info->state == STATS_AVAILABLE ||
			stream_info->state == STATS_INACTIVE)
			continue;
		if (stream_info->num_isp > 1 &&
			vfe_dev->pdev->id == ISP_VFE0)
			continue;
		spin_lock_irqsave(&stream_info->lock, flags);
		for (j = 0; j < MSM_ISP_COMP_IRQ_MAX; j++)
			stream_info->composite_irq[j] = 0;
		msm_isp_init_stats_ping_pong_reg(
				stream_info);
		for (j = 0; j < stream_info->num_isp; j++) {
			struct vfe_device *temp_vfe_dev =
					stream_info->vfe_dev[j];
			uint8_t comp_flag = stream_info->composite_flag;

			temp_vfe_dev->hw_info->vfe_ops.stats_ops.enable_module(
				temp_vfe_dev, BIT(i), 1);
			if (comp_flag)
				temp_vfe_dev->hw_info->vfe_ops.stats_ops
						.cfg_comp_mask(temp_vfe_dev,
						BIT(i), (comp_flag - 1), 1);
			else
				temp_vfe_dev->hw_info->vfe_ops.stats_ops
						.cfg_wm_irq_mask(temp_vfe_dev,
							stream_info);
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	return 0;
}

static int msm_isp_check_stream_cfg_cmd(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd)
{
	int i;
	struct msm_vfe_stats_stream *stream_info;
	uint32_t idx;
	int vfe_idx;
	uint32_t stats_idx[MSM_ISP_STATS_MAX];

	if (stream_cfg_cmd->num_streams > MSM_ISP_STATS_MAX) {
		pr_err("%s invalid num_streams %d\n", __func__,
			stream_cfg_cmd->num_streams);
		return -EINVAL;
	}
	memset(stats_idx, 0, sizeof(stats_idx));
	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);

		if (idx >= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s Invalid stats index %d\n", __func__, idx);
			return -EINVAL;
		}
		stream_info = msm_isp_get_stats_stream_common_data(
				vfe_dev, idx);
		vfe_idx = msm_isp_get_vfe_idx_for_stats_stream_user(vfe_dev,
							stream_info);
		if (vfe_idx == -ENOTTY || stream_info->stream_handle[vfe_idx] !=
			stream_cfg_cmd->stream_handle[i]) {
			pr_err("%s: Invalid stream handle: 0x%x received expected %x\n",
				__func__, stream_cfg_cmd->stream_handle[i],
				vfe_idx == -ENOTTY ? 0 :
				stream_info->stream_handle[vfe_idx]);
			return -EINVAL;
		}
		/* remove duplicate handles */
		if (stats_idx[idx] == stream_cfg_cmd->stream_handle[i])
			stream_cfg_cmd->stream_handle[i] = 0;
		else
			stats_idx[idx] = stream_cfg_cmd->stream_handle[i];
	}
	return 0;
}

static void __msm_isp_stop_stats_streams(
		struct msm_vfe_stats_stream **streams,
		int num_streams,
		struct msm_isp_timestamp timestamp)
{
	int i;
	int k;
	struct msm_vfe_stats_stream *stream_info;
	struct vfe_device *vfe_dev;
	struct msm_vfe_stats_shared_data *stats_data;
	unsigned long flags;

	for (i = 0; i < num_streams; i++) {
		stream_info = streams[i];
		spin_lock_irqsave(&stream_info->lock, flags);
		init_completion(&stream_info->inactive_comp);
		stream_info->state = STATS_STOP_PENDING;
		if (stream_info->vfe_dev[0]->axi_data.src_info[
					VFE_PIX_0].active == 0) {
			while (stream_info->state != STATS_INACTIVE)
				__msm_isp_stats_stream_update(stream_info);
		}
		for (k = 0; k < stream_info->num_isp; k++) {
			stats_data = &stream_info->vfe_dev[k]->stats_data;
			stats_data->num_active_stream--;
		}

		msm_isp_stats_cfg_stream_scratch(
			stream_info, VFE_PING_FLAG);
		msm_isp_stats_cfg_stream_scratch(
			stream_info, VFE_PONG_FLAG);
		vfe_dev = stream_info->vfe_dev[0];
		if (vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
			stream_info->bufq_handle,
			MSM_ISP_BUFFER_FLUSH_ALL, &timestamp.buf_time,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id ==
			-EFAULT))
			msm_isp_halt_send_error(vfe_dev,
				ISP_EVENT_BUF_FATAL_ERROR);
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	if (msm_isp_stats_wait_for_streams(streams, num_streams, 0)) {
		for (i = 0; i < num_streams; i++) {
			stream_info = streams[i];
			if (stream_info->state == STATS_INACTIVE)
				continue;
			spin_lock_irqsave(&stream_info->lock, flags);
			while (stream_info->state != STATS_INACTIVE)
				__msm_isp_stats_stream_update(stream_info);
			spin_unlock_irqrestore(&stream_info->lock, flags);
		}
	}
}

static int msm_isp_check_stats_stream_state(
			struct msm_vfe_stats_stream *stream_info,
			int cmd)
{
	switch (stream_info->state) {
	case STATS_AVAILABLE:
		return -EINVAL;
	case STATS_INACTIVE:
		if (cmd == 0)
			return -EALREADY;
		break;
	case STATS_ACTIVE:
		if (cmd)
			return -EALREADY;
		break;
	default:
		WARN(1, "Invalid stats state %d\n", stream_info->state);
	}
	return 0;
}

static int msm_isp_start_stats_stream(struct vfe_device *vfe_dev_ioctl,
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd)
{
	int i, rc = 0;
	uint32_t stats_mask = 0, idx;
	uint32_t comp_stats_mask[MAX_NUM_STATS_COMP_MASK] = {0};
	uint32_t num_stats_comp_mask = 0;
	struct msm_vfe_stats_stream *stream_info;
	struct msm_vfe_stats_shared_data *stats_data = NULL;
	int num_stream = 0;
	struct msm_vfe_stats_stream *streams[MSM_ISP_STATS_MAX];
	struct msm_isp_timestamp timestamp;
	unsigned long flags;
	int k;
	struct vfe_device *update_vfes[MAX_VFE] = {NULL, NULL};
	uint32_t num_active_streams[MAX_VFE] = {0, 0};
	struct vfe_device *vfe_dev;

	msm_isp_get_timestamp(&timestamp, vfe_dev_ioctl);
	mutex_lock(&vfe_dev_ioctl->buf_mgr->lock);

	num_stats_comp_mask =
		vfe_dev_ioctl->hw_info->stats_hw_info->num_stats_comp_mask;
	if (stream_cfg_cmd->num_streams >= MSM_ISP_STATS_MAX)
		stream_cfg_cmd->num_streams = MSM_ISP_STATS_MAX;
	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (stream_cfg_cmd->stream_handle[i] == 0)
			continue;
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);
		stream_info = msm_isp_get_stats_stream_common_data(
						vfe_dev_ioctl, idx);
		spin_lock_irqsave(&stream_info->lock, flags);
		rc = msm_isp_check_stats_stream_state(stream_info, 1);
		if (rc == -EALREADY) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			rc = 0;
			continue;
		}
		if (rc) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);
			goto error;
		}
		rc = msm_isp_init_stats_ping_pong_reg(
							stream_info);
		if (rc < 0) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			pr_err("%s: No buffer for stream%d\n", __func__, idx);
			mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);
			return rc;
		}
		init_completion(&stream_info->active_comp);
		stream_info->state = STATS_START_PENDING;
		if (vfe_dev_ioctl->axi_data.src_info[VFE_PIX_0].active == 0) {
			while (stream_info->state != STATS_ACTIVE)
				__msm_isp_stats_stream_update(stream_info);
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);

		stats_mask |= 1 << idx;
		for (k = 0; k < stream_info->num_isp; k++) {
			vfe_dev = stream_info->vfe_dev[k];
			stats_data = &vfe_dev->stats_data;
			if (update_vfes[vfe_dev->pdev->id] == NULL) {
				update_vfes[vfe_dev->pdev->id] = vfe_dev;
				num_active_streams[vfe_dev->pdev->id] =
					stats_data->num_active_stream;
			}
			stats_data->num_active_stream++;
		}

		if (stream_info->composite_flag)
			comp_stats_mask[stream_info->composite_flag-1] |=
				1 << idx;

		ISP_DBG("%s: stats_mask %x %x\n",
			__func__, comp_stats_mask[0],
			comp_stats_mask[1]);
		if (stats_data)
			ISP_DBG("%s: active_streams = %d\n", __func__,
				stats_data->num_active_stream);
		streams[num_stream++] = stream_info;
	}
	mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);

	for (k = 0; k < MAX_VFE; k++) {
		if (!update_vfes[k] || num_active_streams[k])
			continue;
		vfe_dev = update_vfes[k];
		vfe_dev->hw_info->vfe_ops.stats_ops.cfg_ub(vfe_dev);
	}

	rc = msm_isp_stats_wait_for_streams(streams, num_stream, 1);
	if (rc)
		goto error;
	return 0;
error:
	__msm_isp_stop_stats_streams(streams, num_stream, timestamp);
	return rc;
}

static int msm_isp_stop_stats_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd)
{
	int i, rc = 0;
	uint32_t idx;
	uint32_t num_stats_comp_mask = 0;
	struct msm_vfe_stats_stream *stream_info;
	struct msm_isp_timestamp timestamp;
	int num_stream = 0;
	struct msm_vfe_stats_stream *streams[MSM_ISP_STATS_MAX];
	unsigned long flags;

	msm_isp_get_timestamp(&timestamp, vfe_dev);

	num_stats_comp_mask =
		vfe_dev->hw_info->stats_hw_info->num_stats_comp_mask;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (stream_cfg_cmd->stream_handle[i] == 0)
			continue;
		idx = STATS_IDX(stream_cfg_cmd->stream_handle[i]);

		stream_info = msm_isp_get_stats_stream_common_data(
					vfe_dev, idx);
		spin_lock_irqsave(&stream_info->lock, flags);
		rc = msm_isp_check_stats_stream_state(stream_info, 0);
		if (rc) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			rc = 0;
			continue;
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
		streams[num_stream++] = stream_info;
	}

	__msm_isp_stop_stats_streams(streams, num_stream, timestamp);

	return rc;
}

int msm_isp_cfg_stats_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_stats_stream_cfg_cmd *stream_cfg_cmd = arg;

	rc = msm_isp_check_stream_cfg_cmd(vfe_dev, stream_cfg_cmd);
	if (rc)
		return rc;

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
	struct msm_vfe_axi_stream_update_cmd *update_cmd = arg;
	struct msm_vfe_axi_stream_cfg_update_info *update_info = NULL;
	struct msm_isp_sw_framskip *sw_skip_info = NULL;
	int vfe_idx;
	int k;

	if (update_cmd->num_streams > MSM_ISP_STATS_MAX) {
		pr_err("%s: Invalid num_streams %d\n",
			__func__, update_cmd->num_streams);
		return -EINVAL;
	}

	/*validate request*/
	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = (struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
		/*check array reference bounds*/
		if (STATS_IDX(update_info->stream_handle)
			>= vfe_dev->hw_info->stats_hw_info->num_stats_type) {
			pr_err("%s: stats idx %d out of bound!\n", __func__,
			STATS_IDX(update_info->stream_handle));
			return -EINVAL;
		}
	}

	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = (struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
		stream_info = msm_isp_get_stats_stream_common_data(vfe_dev,
					STATS_IDX(update_info->stream_handle));
		vfe_idx = msm_isp_get_vfe_idx_for_stats_stream_user(vfe_dev,
						stream_info);
		if (vfe_idx == -ENOTTY || stream_info->stream_handle[vfe_idx] !=
			update_info->stream_handle) {
			pr_err("%s: stats stream handle %x %x mismatch!\n",
				__func__, vfe_idx != -ENOTTY ?
				stream_info->stream_handle[vfe_idx] : 0,
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
			stream_info->framedrop_period = framedrop_period;
			if (stream_info->init_stats_frame_drop == 0)
				for (k = 0; k < stream_info->num_isp; k++)
					stream_info->vfe_dev[
				k]->hw_info->vfe_ops.stats_ops.cfg_wm_reg(
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
