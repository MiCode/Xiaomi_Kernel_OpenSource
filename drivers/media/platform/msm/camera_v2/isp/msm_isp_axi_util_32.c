/* Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
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
#include <asm/div64.h>
#include "msm_isp_util_32.h"
#include "msm_isp_axi_util_32.h"

#define SRC_TO_INTF(src) \
	((src < RDI_INTF_0 || src == VFE_AXI_SRC_MAX) ? VFE_PIX_0 : \
	(VFE_RAW_0 + src - RDI_INTF_0))

#define HANDLE_TO_IDX(handle) (handle & 0xFF)

int msm_isp_axi_create_stream(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe32_axi_stream_request_cmd *stream_cfg_cmd)
{
	int i, rc = -1;

	for (i = 0; i < MAX_NUM_STREAM; i++) {
		if (axi_data->stream_info[i].state == AVAILABLE)
			break;
	}

	if (i == MAX_NUM_STREAM) {
		pr_err("%s: No free stream\n", __func__);
		return rc;
	}

	if ((axi_data->stream_handle_cnt << 8) == 0)
		axi_data->stream_handle_cnt++;

	stream_cfg_cmd->axi_stream_handle =
		(++axi_data->stream_handle_cnt) << 8 | i;

	memset(&axi_data->stream_info[i], 0,
		   sizeof(struct msm_vfe_axi_stream));
	spin_lock_init(&axi_data->stream_info[i].lock);
	axi_data->stream_info[i].session_id = stream_cfg_cmd->session_id;
	axi_data->stream_info[i].stream_id = stream_cfg_cmd->stream_id;
	axi_data->stream_info[i].buf_divert = stream_cfg_cmd->buf_divert;
	axi_data->stream_info[i].state = INACTIVE;
	axi_data->stream_info[i].stream_handle =
		stream_cfg_cmd->axi_stream_handle;
	axi_data->stream_info[i].controllable_output =
		stream_cfg_cmd->controllable_output;
	if (stream_cfg_cmd->controllable_output)
		stream_cfg_cmd->frame_skip_pattern = SKIP_ALL;
	return 0;
}

void msm_isp_axi_destroy_stream(
	struct msm_vfe_axi_shared_data *axi_data, int stream_idx)
{
	if (axi_data->stream_info[stream_idx].state != AVAILABLE) {
		axi_data->stream_info[stream_idx].state = AVAILABLE;
		axi_data->stream_info[stream_idx].stream_handle = 0;
	} else {
		pr_err("%s: stream does not exist\n", __func__);
	}
}

int msm_isp_validate_axi_request(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe32_axi_stream_request_cmd *stream_cfg_cmd)
{
	int rc = -1, i;
	struct msm_vfe_axi_stream *stream_info = NULL;

	if (HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle) < MAX_NUM_STREAM) {
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle)];
	} else {
		pr_err("%s: Invalid axi_stream_handle\n", __func__);
		return rc;
	}

	if (!stream_info) {
		pr_err("%s: Stream info is NULL\n", __func__);
		return -EINVAL;
	}

	switch (stream_cfg_cmd->output_format) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
	case V4L2_PIX_FMT_QBGGR8:
	case V4L2_PIX_FMT_QGBRG8:
	case V4L2_PIX_FMT_QGRBG8:
	case V4L2_PIX_FMT_QRGGB8:
	case V4L2_PIX_FMT_QBGGR10:
	case V4L2_PIX_FMT_QGBRG10:
	case V4L2_PIX_FMT_QGRBG10:
	case V4L2_PIX_FMT_QRGGB10:
	case V4L2_PIX_FMT_QBGGR12:
	case V4L2_PIX_FMT_QGBRG12:
	case V4L2_PIX_FMT_QGRBG12:
	case V4L2_PIX_FMT_QRGGB12:
	case V4L2_PIX_FMT_QBGGR14:
	case V4L2_PIX_FMT_QGBRG14:
	case V4L2_PIX_FMT_QGRBG14:
	case V4L2_PIX_FMT_QRGGB14:
	case V4L2_PIX_FMT_P16BGGR10:
	case V4L2_PIX_FMT_P16GBRG10:
	case V4L2_PIX_FMT_P16GRBG10:
	case V4L2_PIX_FMT_P16RGGB10:
	case V4L2_PIX_FMT_JPEG:
	case V4L2_PIX_FMT_META:
		stream_info->num_planes = 1;
		stream_info->format_factor = ISP_Q2;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV14:
	case V4L2_PIX_FMT_NV41:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		stream_info->num_planes = 2;
		stream_info->format_factor = 1.5 * ISP_Q2;
		break;
	/*TD: Add more image format*/
	default:
		msm_isp_print_fourcc_error(__func__,
				stream_cfg_cmd->output_format);
		return rc;
	}

	if (axi_data->hw_info->num_wm - axi_data->num_used_wm <
		stream_info->num_planes) {
		pr_err("%s: No free write masters\n", __func__);
		return rc;
	}

	if ((stream_info->num_planes > 1) &&
			(axi_data->hw_info->num_comp_mask -
			axi_data->num_used_composite_mask < 1)) {
		pr_err("%s: No free composite mask\n", __func__);
		return rc;
	}

	if (stream_cfg_cmd->init_frame_drop >= MAX_INIT_FRAME_DROP) {
		pr_err("%s: Invalid skip pattern\n", __func__);
		return rc;
	}

	if (stream_cfg_cmd->frame_skip_pattern >= MAX_SKIP) {
		pr_err("%s: Invalid skip pattern\n", __func__);
		return rc;
	}

	for (i = 0; i < stream_info->num_planes; i++) {
		stream_info->plane_cfg[i] = stream_cfg_cmd->plane_cfg[i];
		stream_info->max_width = max(stream_info->max_width,
			stream_cfg_cmd->plane_cfg[i].output_width);
	}

	stream_info->output_format = stream_cfg_cmd->output_format;
	stream_info->runtime_output_format = stream_info->output_format;
	stream_info->stream_src = stream_cfg_cmd->stream_src;
	stream_info->frame_based = stream_cfg_cmd->frame_base;
	return 0;
}

static uint32_t msm_isp_axi_get_plane_size(
	struct msm_vfe_axi_stream *stream_info, int plane_idx)
{
	uint32_t size = 0;
	struct msm_vfe_axi_plane_cfg *plane_cfg = stream_info->plane_cfg;

	switch (stream_info->output_format) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_QBGGR8:
	case V4L2_PIX_FMT_QGBRG8:
	case V4L2_PIX_FMT_QGRBG8:
	case V4L2_PIX_FMT_QRGGB8:
	case V4L2_PIX_FMT_JPEG:
	case V4L2_PIX_FMT_META:
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_QBGGR10:
	case V4L2_PIX_FMT_QGBRG10:
	case V4L2_PIX_FMT_QGRBG10:
	case V4L2_PIX_FMT_QRGGB10:
		/* TODO: fix me */
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_QBGGR12:
	case V4L2_PIX_FMT_QGBRG12:
	case V4L2_PIX_FMT_QGRBG12:
	case V4L2_PIX_FMT_QRGGB12:
	case V4L2_PIX_FMT_SBGGR14:
	case V4L2_PIX_FMT_SGBRG14:
	case V4L2_PIX_FMT_SGRBG14:
	case V4L2_PIX_FMT_SRGGB14:
	case V4L2_PIX_FMT_QBGGR14:
	case V4L2_PIX_FMT_QGBRG14:
	case V4L2_PIX_FMT_QGRBG14:
	case V4L2_PIX_FMT_QRGGB14:
		/* TODO: fix me */
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_P16BGGR10:
	case V4L2_PIX_FMT_P16GBRG10:
	case V4L2_PIX_FMT_P16GRBG10:
	case V4L2_PIX_FMT_P16RGGB10:
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		if (plane_cfg[plane_idx].output_plane_format == Y_PLANE)
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width;
		else
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_NV14:
	case V4L2_PIX_FMT_NV41:
		if (plane_cfg[plane_idx].output_plane_format == Y_PLANE)
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width;
		else
			size = plane_cfg[plane_idx].output_height *
				plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		size = plane_cfg[plane_idx].output_height *
			plane_cfg[plane_idx].output_width;
		break;
	/*TD: Add more image format*/
	default:
		msm_isp_print_fourcc_error(__func__,
				stream_info->output_format);
		break;
	}
	return size;
}

void msm_isp_axi_reserve_wm(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info)
{
	int i, j;

	for (i = 0; i < stream_info->num_planes; i++) {
		for (j = 0; j < axi_data->hw_info->num_wm; j++) {
			if (!axi_data->free_wm[j]) {
				axi_data->free_wm[j] =
					stream_info->stream_handle;
				axi_data->wm_image_size[j] =
					msm_isp_axi_get_plane_size(
						stream_info, i);
				axi_data->num_used_wm++;
				break;
			}
		}
		stream_info->wm[i] = j;
	}
}

void msm_isp_axi_free_wm(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info)
{
	int i;

	for (i = 0; i < stream_info->num_planes; i++) {
		axi_data->free_wm[stream_info->wm[i]] = 0;
		axi_data->num_used_wm--;
	}
	if (stream_info->stream_src <= IDEAL_RAW)
		axi_data->num_pix_stream++;
	else if (stream_info->stream_src < VFE_AXI_SRC_MAX)
		axi_data->num_rdi_stream++;
}

void msm_isp_axi_reserve_comp_mask(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info)
{
	int i;
	uint8_t comp_mask = 0;

	for (i = 0; i < stream_info->num_planes; i++)
		comp_mask |= 1 << stream_info->wm[i];

	for (i = 0; i < axi_data->hw_info->num_comp_mask; i++) {
		if (!axi_data->composite_info[i].stream_handle) {
			axi_data->composite_info[i].stream_handle =
				stream_info->stream_handle;
			axi_data->composite_info[i].
				stream_composite_mask = comp_mask;
			axi_data->num_used_composite_mask++;
			break;
		}
	}
	stream_info->comp_mask_index = i;
}

static void msm_isp_axi_free_comp_mask(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info)
{
	axi_data->composite_info[stream_info->comp_mask_index].
		stream_composite_mask = 0;
	axi_data->composite_info[stream_info->comp_mask_index].
		stream_handle = 0;
	axi_data->num_used_composite_mask--;
}

static int msm_isp_axi_get_bufq_handles(
		struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info)
{
	int rc = 0;

	if (stream_info->stream_id & ISP_SCRATCH_BUF_BIT) {
		stream_info->bufq_handle =
			vfe_dev->buf_mgr->ops->get_bufq_handle(
		vfe_dev->buf_mgr, stream_info->session_id,
		stream_info->stream_id & ~ISP_SCRATCH_BUF_BIT);
		if (stream_info->bufq_handle == 0) {
			pr_err("%s: Stream 0x%x has no valid buffer queue\n",
				__func__, (unsigned int)stream_info->stream_id);
			rc = -EINVAL;
			return rc;
		}

		stream_info->bufq_scratch_handle =
			vfe_dev->buf_mgr->ops->get_bufq_handle(
		vfe_dev->buf_mgr, stream_info->session_id,
		stream_info->stream_id);
		if (stream_info->bufq_scratch_handle == 0) {
			pr_err("%s: Stream 0x%x has no valid buffer queue\n",
				__func__, (unsigned int)stream_info->stream_id);
			rc = -EINVAL;
			return rc;
		}
	} else {
		stream_info->bufq_handle =
			vfe_dev->buf_mgr->ops->get_bufq_handle(
		vfe_dev->buf_mgr, stream_info->session_id,
		stream_info->stream_id);
		if (stream_info->bufq_handle == 0) {
			pr_err("%s: Stream 0x%x has no valid buffer queue\n",
				__func__, (unsigned int)stream_info->stream_id);
			rc = -EINVAL;
			return rc;
		}
	}
	return rc;
}

int msm_isp_axi_check_stream_state(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int rc = 0, i;
	unsigned long flags;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;
	enum msm_vfe_axi_state valid_state =
		(stream_cfg_cmd->cmd == START_STREAM) ? INACTIVE : ACTIVE;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			MAX_NUM_STREAM) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		spin_lock_irqsave(&stream_info->lock, flags);
		if (stream_info->state != valid_state) {
			if ((stream_info->state == PAUSING ||
				stream_info->state == PAUSED ||
				stream_info->state == RESUME_PENDING ||
				stream_info->state == RESUMING) &&
				(stream_cfg_cmd->cmd == STOP_STREAM ||
				stream_cfg_cmd->cmd == STOP_IMMEDIATELY)) {
				stream_info->state = ACTIVE;
			} else {
				pr_err("%s: Invalid stream state: %d\n",
					__func__, stream_info->state);
				spin_unlock_irqrestore(
					&stream_info->lock, flags);
				rc = -EINVAL;
				break;
			}
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);

		if (stream_cfg_cmd->cmd == START_STREAM) {
			rc = msm_isp_axi_get_bufq_handles(vfe_dev, stream_info);
			if (rc)
				break;
		}
	}
	return rc;
}

void msm_isp_update_framedrop_reg(struct vfe_device *vfe_dev,
	uint8_t input_src)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;

	for (i = 0; i < MAX_NUM_STREAM; i++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->state != ACTIVE)
			continue;

		if (stream_info->runtime_framedrop_update) {
			stream_info->runtime_init_frame_drop--;
			if (stream_info->runtime_init_frame_drop == 0) {
				stream_info->runtime_framedrop_update = 0;
				vfe_dev->hw_info->vfe_ops.axi_ops.
				cfg_framedrop(vfe_dev, stream_info);
			}
		}
		if (stream_info->stream_type == BURST_STREAM &&
			((1 << SRC_TO_INTF(stream_info->stream_src)) &
			input_src)) {
			if (stream_info->runtime_framedrop_update_burst) {
				stream_info->runtime_framedrop_update_burst = 0;
				stream_info->runtime_burst_frame_count =
				    stream_info->runtime_init_frame_drop +
				    (stream_info->runtime_num_burst_capture -
					1) *
				    (stream_info->framedrop_period + 1) + 1;
				vfe_dev->hw_info->vfe_ops.axi_ops.
					cfg_framedrop(vfe_dev, stream_info);
			} else {
				stream_info->runtime_burst_frame_count--;
				if (stream_info->
				    runtime_burst_frame_count == 0) {
					vfe_dev->hw_info->vfe_ops.axi_ops.
					cfg_framedrop(vfe_dev, stream_info);
				}
			}
		}
	}
}

void msm_isp_reset_framedrop(struct vfe_device *vfe_dev,
			struct msm_vfe_axi_stream *stream_info)
{
	stream_info->runtime_init_frame_drop = stream_info->init_frame_drop;
	stream_info->runtime_burst_frame_count =
		stream_info->burst_frame_count;
	stream_info->runtime_num_burst_capture =
		stream_info->num_burst_capture;
	stream_info->runtime_framedrop_update = stream_info->framedrop_update;
	vfe_dev->hw_info->vfe_ops.axi_ops.cfg_framedrop(vfe_dev, stream_info);
}

void msm_isp_notify(struct vfe_device *vfe_dev, uint32_t event_type,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts)
{
	struct msm_isp32_event_data event_data;

	memset(&event_data, 0, sizeof(event_data));
	switch (event_type) {
	case ISP_EVENT_SOF:
		if ((frame_src == VFE_PIX_0) && (vfe_dev->isp_sof_debug < 5)) {
			pr_err("%s: PIX0 frame id: %u\n", __func__,
				vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		vfe_dev->isp_sof_debug++;
		}
		vfe_dev->axi_data.src_info[frame_src].frame_id++;
		if (vfe_dev->axi_data.src_info[frame_src].frame_id == 0)
			vfe_dev->axi_data.src_info[frame_src].frame_id = 1;
		ISP_DBG("%s: frame_src %d frame id: %u\n", __func__,
			frame_src,
			vfe_dev->axi_data.src_info[frame_src].frame_id);
		break;
	case ISP_EVENT_REG_UPDATE:
		vfe_dev->axi_data.src_info[frame_src].last_updt_frm_id = 0;
		break;
	default:
		break;
	}

	event_data.input_intf = frame_src;
	event_data.frame_id = vfe_dev->axi_data.src_info[frame_src].frame_id;
	event_data.timestamp = ts->event_time;
	event_data.mono_timestamp = ts->buf_time;
	msm_isp_send_event(vfe_dev, event_type | frame_src, &event_data);
}

void msm_isp_calculate_framedrop(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe32_axi_stream_request_cmd *stream_cfg_cmd)
{
	uint32_t framedrop_period = 0;
	struct msm_vfe_axi_stream *stream_info = NULL;

	if (HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle) < MAX_NUM_STREAM) {
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle)];
	} else {
		pr_err("%s: Invalid stream handle", __func__);
		return;
	}
	if (!stream_info) {
		pr_err("%s: Stream info is NULL\n", __func__);
		return;
	}

	framedrop_period = msm_isp_get_framedrop_period(
	   stream_cfg_cmd->frame_skip_pattern);
	stream_info->frame_skip_pattern =
		stream_cfg_cmd->frame_skip_pattern;
	if (stream_cfg_cmd->frame_skip_pattern == SKIP_ALL)
		stream_info->framedrop_pattern = 0x0;
	else
		stream_info->framedrop_pattern = 0x1;
	stream_info->framedrop_period = framedrop_period - 1;

	if (stream_cfg_cmd->init_frame_drop < framedrop_period) {
		stream_info->framedrop_pattern <<=
			stream_cfg_cmd->init_frame_drop;
		stream_info->init_frame_drop = 0;
		stream_info->framedrop_update = 0;
	} else {
		stream_info->init_frame_drop = stream_cfg_cmd->init_frame_drop;
		stream_info->framedrop_update = 1;
	}

	if (stream_cfg_cmd->burst_count > 0) {
		stream_info->stream_type = BURST_STREAM;
		stream_info->num_burst_capture =
			stream_cfg_cmd->burst_count;
		stream_info->burst_frame_count =
		stream_cfg_cmd->init_frame_drop +
			(stream_cfg_cmd->burst_count - 1) *
			framedrop_period + 1;
	} else {
		stream_info->stream_type = CONTINUOUS_STREAM;
		stream_info->burst_frame_count = 0;
		stream_info->num_burst_capture = 0;
	}
}

static void msm_isp_calculate_bandwidth(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info)
{
	int bpp = 0;

	if (stream_info->stream_src < RDI_INTF_0) {
		stream_info->bandwidth =
			(axi_data->src_info[VFE_PIX_0].pixel_clock /
			axi_data->src_info[VFE_PIX_0].width) *
			stream_info->max_width;
		stream_info->bandwidth = (unsigned long)stream_info->bandwidth *
			stream_info->format_factor / ISP_Q2;
	} else {
		int rdi = SRC_TO_INTF(stream_info->stream_src);

		bpp = msm_isp_get_bit_per_pixel(stream_info->output_format);
		if (rdi < VFE_SRC_MAX)
			stream_info->bandwidth =
				(axi_data->src_info[rdi].pixel_clock / 8) * bpp;
		else
			pr_err("%s: Invalid rdi interface\n", __func__);
	}
}

#ifdef CONFIG_MSM_AVTIMER
void msm_isp_start_avtimer(void)
{
	avcs_core_open();
	avcs_core_disable_power_collapse(1);
}

static inline void msm_isp_get_avtimer_ts(
		struct msm_isp_timestamp *time_stamp)
{
	int rc = 0;
	uint32_t avtimer_usec = 0;
	uint64_t avtimer_tick = 0;

	rc = avcs_core_query_timer(&avtimer_tick);
	if (rc < 0) {
		pr_err("%s: Error: Invalid AVTimer Tick, rc=%d\n",
			   __func__, rc);
		/* In case of error return zero AVTimer Tick Value */
		time_stamp->vt_time.tv_sec = 0;
		time_stamp->vt_time.tv_usec = 0;
	} else {
		avtimer_usec = do_div(avtimer_tick, USEC_PER_SEC);
		time_stamp->vt_time.tv_sec = (uint32_t)(avtimer_tick);
		time_stamp->vt_time.tv_usec = avtimer_usec;
		pr_debug("%s: AVTimer TS = %u:%u\n", __func__,
			(uint32_t)(avtimer_tick), avtimer_usec);
	}
}
#else
void msm_isp_start_avtimer(void)
{
	pr_err("AV Timer is not supported\n");
}

inline void msm_isp_get_avtimer_ts(
		struct msm_isp_timestamp *time_stamp)
{
	pr_err_ratelimited("%s: Error: AVTimer driver not available\n",
		__func__);
	time_stamp->vt_time.tv_sec = 0;
	time_stamp->vt_time.tv_usec = 0;
}
#endif

int msm_isp_request_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i;
	uint32_t io_format = 0;
	struct msm_vfe32_axi_stream_request_cmd *stream_cfg_cmd = arg;
	struct msm_vfe_axi_stream *stream_info;

	rc = msm_isp_axi_create_stream(
		&vfe_dev->axi_data, stream_cfg_cmd);
	if (rc) {
		pr_err("%s: create stream failed\n", __func__);
		return rc;
	}

	rc = msm_isp_validate_axi_request(
		&vfe_dev->axi_data, stream_cfg_cmd);
	if (rc) {
		pr_err("%s: Request validation failed\n", __func__);
		if (HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle) <
			MAX_NUM_STREAM)
			msm_isp_axi_destroy_stream(&vfe_dev->axi_data,
			      HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle));
		return rc;
	}
	stream_info = &vfe_dev->axi_data.
		stream_info[HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle)];
	if (!stream_info) {
		pr_err("%s: can not find stream handle %x\n", __func__,
			stream_cfg_cmd->axi_stream_handle);
		return -EINVAL;
	}

	stream_info->memory_input = stream_cfg_cmd->memory_input;

	msm_isp_axi_reserve_wm(&vfe_dev->axi_data, stream_info);

	if (stream_info->stream_src < RDI_INTF_0) {
		io_format = vfe_dev->axi_data.src_info[VFE_PIX_0].input_format;
		if (stream_info->stream_src == CAMIF_RAW ||
			stream_info->stream_src == IDEAL_RAW) {
			if (stream_info->stream_src == CAMIF_RAW &&
				io_format != stream_info->output_format)
				pr_debug("%s: Overriding input format\n",
					__func__);

			io_format = stream_info->output_format;
		}
		rc = vfe_dev->hw_info->vfe_ops.axi_ops.cfg_io_format(
			vfe_dev, stream_info->stream_src, io_format);
		if (rc) {
			pr_err("%s: cfg io format failed\n", __func__);
			msm_isp_axi_free_wm(&vfe_dev->axi_data,
				stream_info);
			msm_isp_axi_destroy_stream(&vfe_dev->axi_data,
				HANDLE_TO_IDX(
				stream_cfg_cmd->axi_stream_handle));
			return rc;
		}
	}

	msm_isp_calculate_framedrop(&vfe_dev->axi_data, stream_cfg_cmd);
	if (stream_cfg_cmd->vt_enable && !vfe_dev->vt_enable) {
		vfe_dev->vt_enable = stream_cfg_cmd->vt_enable;
		msm_isp_start_avtimer();
	}
	if (stream_info->num_planes > 1) {
		msm_isp_axi_reserve_comp_mask(
			&vfe_dev->axi_data, stream_info);
		vfe_dev->hw_info->vfe_ops.axi_ops.
		cfg_comp_mask(vfe_dev, stream_info);
	} else {
		vfe_dev->hw_info->vfe_ops.axi_ops.
			cfg_wm_irq_mask(vfe_dev, stream_info);
	}

	for (i = 0; i < stream_info->num_planes; i++) {
		vfe_dev->hw_info->vfe_ops.axi_ops.
			cfg_wm_reg(vfe_dev, stream_info, i);

		vfe_dev->hw_info->vfe_ops.axi_ops.
			cfg_wm_xbar_reg(vfe_dev, stream_info, i);
	}
	return rc;
}

int msm_isp_release_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i;
	struct msm_vfe_axi_stream_release_cmd *stream_release_cmd = arg;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_stream_cfg_cmd stream_cfg;


	if (HANDLE_TO_IDX(stream_release_cmd->stream_handle) >=
		MAX_NUM_STREAM) {
		pr_err("%s: Invalid stream handle\n", __func__);
		return -EINVAL;
	}
	stream_info = &axi_data->stream_info[
		HANDLE_TO_IDX(stream_release_cmd->stream_handle)];
	if (stream_info->state == AVAILABLE) {
		pr_err("%s: Stream already released\n", __func__);
		return -EINVAL;
	} else if (stream_info->state != INACTIVE) {
		stream_cfg.cmd = STOP_STREAM;
		stream_cfg.num_streams = 1;
		stream_cfg.stream_handle[0] = stream_release_cmd->stream_handle;
		msm_isp_cfg_axi_stream(vfe_dev, (void *) &stream_cfg);
	}

	for (i = 0; i < stream_info->num_planes; i++) {
		vfe_dev->hw_info->vfe_ops.axi_ops.
			clear_wm_reg(vfe_dev, stream_info, i);

		vfe_dev->hw_info->vfe_ops.axi_ops.
		clear_wm_xbar_reg(vfe_dev, stream_info, i);
	}

	if (stream_info->num_planes > 1) {
		vfe_dev->hw_info->vfe_ops.axi_ops.
			clear_comp_mask(vfe_dev, stream_info);
		msm_isp_axi_free_comp_mask(&vfe_dev->axi_data, stream_info);
	} else {
		vfe_dev->hw_info->vfe_ops.axi_ops.
		clear_wm_irq_mask(vfe_dev, stream_info);
	}

	vfe_dev->hw_info->vfe_ops.axi_ops.clear_framedrop(vfe_dev, stream_info);
	msm_isp_axi_free_wm(axi_data, stream_info);

	msm_isp_axi_destroy_stream(&vfe_dev->axi_data,
		HANDLE_TO_IDX(stream_release_cmd->stream_handle));

	return rc;
}

static void msm_isp_axi_stream_enable_cfg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

	if (stream_info->state == INACTIVE)
		return;
	for (i = 0; i < stream_info->num_planes; i++) {
		if (stream_info->state == START_PENDING ||
			stream_info->state == RESUME_PENDING) {
			vfe_dev->hw_info->vfe_ops.axi_ops.
				enable_wm(vfe_dev, stream_info->wm[i], 1);
		} else {
			vfe_dev->hw_info->vfe_ops.axi_ops.
				enable_wm(vfe_dev, stream_info->wm[i], 0);
			/* Issue a reg update for Raw Snapshot Case
			 * since we dont have reg update ack
			 */
			if (stream_info->stream_src == CAMIF_RAW ||
				stream_info->stream_src == IDEAL_RAW) {
				vfe_dev->hw_info->vfe_ops.core_ops.
				reg_update(vfe_dev, (1 << VFE_PIX_0));
			}
		}
	}

	if (stream_info->state == START_PENDING)
		axi_data->num_active_stream++;
	else if (stream_info->state == STOP_PENDING)
		axi_data->num_active_stream--;
}

void msm_isp_axi_stream_update(struct vfe_device *vfe_dev, uint8_t input_src)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

	for (i = 0; i < MAX_NUM_STREAM; i++) {
		if (axi_data->stream_info[i].state == START_PENDING ||
				axi_data->stream_info[i].state ==
					STOP_PENDING) {
			if ((1 <<
				SRC_TO_INTF(axi_data->stream_info[i].
				stream_src)) &
				input_src) {
				msm_isp_axi_stream_enable_cfg(
				   vfe_dev, &axi_data->stream_info[i]);
				axi_data->stream_info[i].state =
					axi_data->stream_info[i].state ==
					START_PENDING ? STARTING : STOPPING;
			}
		} else if (axi_data->stream_info[i].state == STARTING ||
			axi_data->stream_info[i].state == STOPPING) {
			if ((1 <<
				SRC_TO_INTF(axi_data->stream_info[i].
				stream_src)) &
				input_src) {
				axi_data->stream_info[i].state =
				axi_data->stream_info[i].state == STARTING ?
					ACTIVE : INACTIVE;
				vfe_dev->axi_data.stream_update--;
			}
		}
	}

	if (vfe_dev->axi_data.pipeline_update == DISABLE_CAMIF ||
		(vfe_dev->axi_data.pipeline_update ==
		DISABLE_CAMIF_IMMEDIATELY)) {
		vfe_dev->hw_info->vfe_ops.stats_ops.
			enable_module(vfe_dev, 0xFF, 0);
		vfe_dev->axi_data.pipeline_update = NO_UPDATE;
	}

	if (vfe_dev->axi_data.stream_update == 0)
		complete(&vfe_dev->stream_config_complete);
}

static void msm_isp_reload_ping_pong_offset(struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info)
{
	int i, j;
	uint32_t flag;
	struct msm_isp_buffer *buf;

	for (i = 0; i < 2; i++) {
		buf = stream_info->buf[i];
		if (!buf)
			continue;
		flag = i ? VFE_PONG_FLAG : VFE_PING_FLAG;
		for (j = 0; j < stream_info->num_planes; j++) {
			vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
				vfe_dev, stream_info->wm[j], flag,
				buf->mapped_info[j].paddr +
				stream_info->plane_cfg[j].plane_addr_offset);
		}
	}
}

void msm_isp_axi_cfg_update(struct vfe_device *vfe_dev)
{
	int i, j;
	uint32_t update_state;
	unsigned long flags;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;

	for (i = 0; i < MAX_NUM_STREAM; i++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->stream_type == BURST_STREAM ||
			stream_info->state == AVAILABLE)
			continue;
		spin_lock_irqsave(&stream_info->lock, flags);
		if (stream_info->state == PAUSING) {
			/*AXI Stopped, apply update*/
			stream_info->state = PAUSED;
			msm_isp_reload_ping_pong_offset(vfe_dev, stream_info);
			for (j = 0; j < stream_info->num_planes; j++)
				vfe_dev->hw_info->vfe_ops.axi_ops.
					cfg_wm_reg(vfe_dev, stream_info, j);
			/*Resume AXI*/
			stream_info->state = RESUME_PENDING;
			msm_isp_axi_stream_enable_cfg(
				vfe_dev, &axi_data->stream_info[i]);
			stream_info->state = RESUMING;
		} else if (stream_info->state == RESUMING) {
			stream_info->runtime_output_format =
				stream_info->output_format;
			stream_info->state = ACTIVE;
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	update_state = atomic_dec_return(&axi_data->axi_cfg_update);
}

static void msm_isp_cfg_pong_address(struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info)
{
	int i;
	struct msm_isp_buffer *buf = stream_info->buf[0];

	for (i = 0; i < stream_info->num_planes; i++)
		vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
			vfe_dev, stream_info->wm[i],
			VFE_PONG_FLAG, buf->mapped_info[i].paddr +
			stream_info->plane_cfg[i].plane_addr_offset);
	stream_info->buf[1] = buf;
}

static void msm_isp_get_done_buf(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	struct msm_isp_buffer **done_buf)
{
	uint32_t pingpong_bit = 0, i;

	pingpong_bit = (~(pingpong_status >> stream_info->wm[0]) & 0x1);
	for (i = 0; i < stream_info->num_planes; i++) {
		if (pingpong_bit !=
			(~(pingpong_status >> stream_info->wm[i]) & 0x1)) {
			pr_debug("%s: Write master ping pong mismatch. Status: 0x%x\n",
				__func__, pingpong_status);
		}
	}

	*done_buf = stream_info->buf[pingpong_bit];

	if (stream_info->controllable_output) {
		stream_info->buf[pingpong_bit] = NULL;
		stream_info->request_frm_num--;
	}
}

static int msm_isp_cfg_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	uint32_t pingpong_bit)
{
	int i, rc = -1;
	struct msm_isp_buffer *buf = NULL;
	uint32_t bufq_handle = 0, frame_id = 0;
	uint32_t stream_idx = HANDLE_TO_IDX(stream_info->stream_handle);

	if (stream_idx >= MAX_NUM_STREAM) {
		pr_err("%s: Invalid stream_idx", __func__);
		return rc;
	}

	if (stream_info->controllable_output && !stream_info->request_frm_num)
		return 0;

	frame_id = vfe_dev->axi_data.
		src_info[SRC_TO_INTF(stream_info->stream_src)].frame_id;
	if (frame_id && stream_info->frame_id &&
		stream_info->frame_id == frame_id) {
		/* This could happen if reg update ack is delayed */
		pr_err("%s: Duplicate frame streamId:%d stream_fid:%d frame_id:%d\n",
			__func__, stream_info->stream_id, stream_info->frame_id,
			frame_id);
		vfe_dev->error_info.stream_framedrop_count[stream_idx]++;
		return rc;
	}

	bufq_handle = stream_info->bufq_handle;
	if (SRC_TO_INTF(stream_info->stream_src) < VFE_SRC_MAX)
		rc = vfe_dev->buf_mgr->ops->get_buf(vfe_dev->buf_mgr,
				vfe_dev->pdev->id, bufq_handle,
				MSM_ISP_INVALID_BUF_INDEX, &buf);
	else {
		pr_err("%s: Invalid stream index\n", __func__);
		rc = -1;
	}

	if (rc < 0) {
		vfe_dev->error_info.stream_framedrop_count[stream_idx]++;
		return rc;
	}

	if (buf->num_planes != stream_info->num_planes) {
		pr_err("%s: Invalid buffer\n", __func__);
		rc = -EINVAL;
		goto buf_error;
	}

	for (i = 0; i < stream_info->num_planes; i++)
		vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
			vfe_dev, stream_info->wm[i],
			pingpong_status, buf->mapped_info[i].paddr +
			stream_info->plane_cfg[i].plane_addr_offset);

	stream_info->buf[pingpong_bit] = buf;

	return 0;
buf_error:
	vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
		buf->bufq_handle, buf->buf_idx);
	return rc;
}

static void msm_isp_process_done_buf(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, struct msm_isp_buffer *buf,
	struct msm_isp_timestamp *ts)
{
	int rc;
	struct msm_isp32_event_data buf_event;
	struct timeval *time_stamp;
	uint32_t stream_idx = HANDLE_TO_IDX(stream_info->stream_handle);
	uint32_t frame_id;
	uint32_t buf_src;

	memset(&buf_event, 0, sizeof(buf_event));

	if (stream_idx >= MAX_NUM_STREAM) {
		pr_err("%s: Invalid stream_idx", __func__);
		return;
	}

	if (SRC_TO_INTF(stream_info->stream_src) < VFE_SRC_MAX)
		frame_id = vfe_dev->axi_data.
			src_info[SRC_TO_INTF(stream_info->stream_src)].frame_id;
	else {
		pr_err("%s: Invalid stream index, put buf back to vb2 queue\n",
			__func__);
		vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx);
		return;
	}

	if (buf && ts) {
		if (vfe_dev->vt_enable) {
			msm_isp_get_avtimer_ts(ts);
			time_stamp = &ts->vt_time;
		} else
			time_stamp = &ts->buf_time;

		rc = vfe_dev->buf_mgr->ops->get_buf_src(vfe_dev->buf_mgr,
						buf->bufq_handle, &buf_src);
		if (stream_info->buf_divert && rc == 0 &&
				buf_src != MSM_ISP_BUFFER_SRC_SCRATCH) {
			rc = vfe_dev->buf_mgr->ops->buf_divert(vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx,
				time_stamp, frame_id);
			/* Buf divert return value represent whether the buf
			 * can be diverted. A positive return value means
			 * other ISP hardware is still processing the frame.
			 */
			if (rc == 0) {
				buf_event.input_intf =
					SRC_TO_INTF(stream_info->stream_src);
				buf_event.frame_id = frame_id;
				buf_event.timestamp = *time_stamp;
				buf_event.u.buf_done.session_id =
					stream_info->session_id;
				buf_event.u.buf_done.stream_id =
					stream_info->stream_id;
				buf_event.u.buf_done.handle =
					stream_info->bufq_handle;
				buf_event.u.buf_done.buf_idx = buf->buf_idx;
				buf_event.u.buf_done.output_format =
					stream_info->runtime_output_format;
				msm_isp_send_event(vfe_dev,
					ISP_EVENT_BUF_DIVERT + stream_idx,
					&buf_event);
			}
		} else {
			buf_event.input_intf =
				SRC_TO_INTF(stream_info->stream_src);
			buf_event.frame_id = frame_id;
			buf_event.timestamp = ts->buf_time;
			buf_event.u.buf_done.session_id =
				stream_info->session_id;
			buf_event.u.buf_done.stream_id =
				stream_info->stream_id;
			buf_event.u.buf_done.output_format =
				stream_info->runtime_output_format;
			msm_isp_send_event(vfe_dev,
				ISP_EVENT_BUF_DONE, &buf_event);
			vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx,
				time_stamp, frame_id,
				stream_info->runtime_output_format);
		}
	}
}

static enum msm_isp_camif_update_state
	msm_isp_get_camif_update_state(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int i;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint8_t pix_stream_cnt = 0, cur_pix_stream_cnt;

	cur_pix_stream_cnt =
		axi_data->src_info[VFE_PIX_0].pix_stream_count +
		axi_data->src_info[VFE_PIX_0].raw_stream_count;
	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		stream_info =
			&axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		if (stream_info->stream_src  < RDI_INTF_0)
			pix_stream_cnt++;
	}

	if ((pix_stream_cnt) &&
	    (axi_data->src_info[VFE_PIX_0].input_mux != EXTERNAL_READ)) {

		if (cur_pix_stream_cnt == 0 && pix_stream_cnt &&
			stream_cfg_cmd->cmd == START_STREAM)
			return ENABLE_CAMIF;
		else if (cur_pix_stream_cnt &&
			(cur_pix_stream_cnt - pix_stream_cnt) == 0 &&
			stream_cfg_cmd->cmd == STOP_STREAM)
			return DISABLE_CAMIF;
		else if (cur_pix_stream_cnt &&
			(cur_pix_stream_cnt - pix_stream_cnt) == 0 &&
			stream_cfg_cmd->cmd == STOP_IMMEDIATELY)
			return DISABLE_CAMIF_IMMEDIATELY;
	}

	return NO_UPDATE;
}

static void msm_isp_update_camif_output_count(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int i;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			MAX_NUM_STREAM) {
			return;
		}
		stream_info =
			&axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		if (stream_info->stream_src >= RDI_INTF_0)
			continue;
		if (stream_info->stream_src == PIX_ENCODER ||
			stream_info->stream_src == PIX_VIEWFINDER ||
			stream_info->stream_src == PIX_VIDEO ||
			stream_info->stream_src == IDEAL_RAW) {
			if (stream_cfg_cmd->cmd == START_STREAM)
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					pix_stream_count++;
			else
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					pix_stream_count--;
		} else if (stream_info->stream_src == CAMIF_RAW) {
			if (stream_cfg_cmd->cmd == START_STREAM)
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					raw_stream_count++;
			else
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					raw_stream_count--;
		}
	}
}


static void msm_isp_update_rdi_output_count(
	  struct vfe_device *vfe_dev,
	  struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int i;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])
			>= MAX_NUM_STREAM)
			return;
		stream_info =
			&axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		if (stream_info->stream_src < RDI_INTF_0)
			continue;
		if (stream_info->stream_src == RDI_INTF_0) {
			if (stream_cfg_cmd->cmd == START_STREAM)
				vfe_dev->axi_data.src_info[VFE_RAW_0].
					raw_stream_count++;
			else
				vfe_dev->axi_data.src_info[VFE_RAW_0].
					raw_stream_count--;
		} else if (stream_info->stream_src == RDI_INTF_1) {
			if (stream_cfg_cmd->cmd == START_STREAM)
				vfe_dev->axi_data.src_info[VFE_RAW_1].
					raw_stream_count++;
			else
				vfe_dev->axi_data.src_info[VFE_RAW_1].
					raw_stream_count--;
		} else if (stream_info->stream_src == RDI_INTF_2) {
			if (stream_cfg_cmd->cmd == START_STREAM)
				vfe_dev->axi_data.src_info[VFE_RAW_2].
					raw_stream_count++;
			else
				vfe_dev->axi_data.src_info[VFE_RAW_2].
					raw_stream_count--;
		}

	}
}

static uint8_t msm_isp_get_curr_stream_cnt(
	  struct vfe_device *vfe_dev)
{
	uint8_t curr_stream_cnt = 0;

	curr_stream_cnt = vfe_dev->axi_data.src_info[VFE_RAW_0].
					raw_stream_count +
					vfe_dev->axi_data.src_info[VFE_RAW_1].
					raw_stream_count +
					vfe_dev->axi_data.src_info[VFE_RAW_2].
					raw_stream_count +
					vfe_dev->axi_data.src_info[VFE_PIX_0].
					pix_stream_count +
					vfe_dev->axi_data.src_info[VFE_PIX_0].
					raw_stream_count;
	return curr_stream_cnt;
}

/*Factor in Q2 format*/
#define ISP_DEFAULT_FORMAT_FACTOR 6
#define ISP_BUS_UTILIZATION_FACTOR 1536 /* 1.5 in Q10 format */
static int msm_isp_update_stream_bandwidth(struct vfe_device *vfe_dev)
{
	int i, rc = 0;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint64_t total_pix_bandwidth = 0, total_rdi_bandwidth = 0;
	uint32_t num_pix_streams = 0;
	uint32_t num_rdi_streams = 0;
	uint32_t total_streams   = 0;
	uint64_t total_bandwidth = 0;
	uint32_t bus_util_factor = ISP_BUS_UTILIZATION_FACTOR;

	for (i = 0; i < MAX_NUM_STREAM; i++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->state == ACTIVE ||
			stream_info->state == START_PENDING) {
			if (stream_info->stream_src < RDI_INTF_0) {
				total_pix_bandwidth += stream_info->bandwidth;
				num_pix_streams++;
			} else {
				total_rdi_bandwidth += stream_info->bandwidth;
				num_rdi_streams++;
			}
		}
	}
	total_bandwidth = total_pix_bandwidth + total_rdi_bandwidth;
	total_streams = num_pix_streams + num_rdi_streams;
	if (vfe_dev->bus_util_factor)
		bus_util_factor = vfe_dev->bus_util_factor;
	ISP_DBG("%s: bus_util_factor = %u\n", __func__, bus_util_factor);

	if (total_streams == 1)
		rc = msm_isp_update_bandwidth(ISP_VFE0 + vfe_dev->pdev->id,
		total_bandwidth,
		(total_bandwidth * bus_util_factor / ISP_Q10));
	else
		rc = msm_isp_update_bandwidth(ISP_VFE0 + vfe_dev->pdev->id,
		(total_bandwidth + MSM_ISP_MIN_AB),  (total_bandwidth *
		bus_util_factor / ISP_Q10 + MSM_ISP_MIN_IB));
	if (rc < 0)
		pr_err("%s: update failed\n", __func__);

	return rc;
}

static int msm_isp_axi_wait_for_cfg_done(struct vfe_device *vfe_dev,
	enum msm_isp_camif_update_state camif_update)
{
	int rc;
	unsigned long flags;

	spin_lock_irqsave(&vfe_dev->shared_data_lock, flags);
	init_completion(&vfe_dev->stream_config_complete);
	vfe_dev->axi_data.pipeline_update = camif_update;
	spin_unlock_irqrestore(&vfe_dev->shared_data_lock, flags);
	rc = wait_for_completion_timeout(
		&vfe_dev->stream_config_complete,
		msecs_to_jiffies(VFE_MAX_CFG_TIMEOUT));
	if (rc == 0) {
		pr_err("%s: wait timeout\n", __func__);
		rc = -1;
	} else {
		rc = 0;
	}
	return rc;
}

static int msm_isp_init_stream_ping_pong_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int rc = 0;
	/*Set address for both PING & PONG register */
	rc = msm_isp_cfg_ping_pong_address(vfe_dev,
		stream_info, VFE_PING_FLAG, 0);
	if (rc < 0) {
		pr_err("%s: No free buffer for ping\n",
			   __func__);
		return rc;
	}

	/* For burst stream of one capture, only one buffer
	 * is allocated. Duplicate ping buffer address to pong
	 * buffer to ensure hardware write to a valid address
	 */
	if (stream_info->stream_type == BURST_STREAM &&
		stream_info->runtime_num_burst_capture <= 1) {
		msm_isp_cfg_pong_address(vfe_dev, stream_info);
	} else {
		rc = msm_isp_cfg_ping_pong_address(vfe_dev,
			stream_info, VFE_PONG_FLAG, 1);
		if (rc < 0) {
			pr_err("%s: No free buffer for pong\n",
				   __func__);
			return rc;
		}
	}
	return rc;
}

static void msm_isp_deinit_stream_ping_pong_reg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int i;

	for (i = 0; i < 2; i++) {
		struct msm_isp_buffer *buf;

		buf = stream_info->buf[i];
		if (buf) {
			vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx);
		}
	}
}

static void msm_isp_get_stream_wm_mask(
	struct msm_vfe_axi_stream *stream_info,
	uint32_t *wm_reload_mask)
{
	int i;

	for (i = 0; i < stream_info->num_planes; i++)
		*wm_reload_mask |= (1 << stream_info->wm[i]);
}

int msm_isp_axi_halt(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_halt_cmd *halt_cmd)
{
	int rc = 0;

	if (halt_cmd->overflow_detected) {
		/*Store current IRQ mask*/
		if (vfe_dev->error_info.overflow_recover_irq_mask0 == 0) {
			vfe_dev->hw_info->vfe_ops.core_ops.get_irq_mask(vfe_dev,
			&vfe_dev->error_info.overflow_recover_irq_mask0,
			&vfe_dev->error_info.overflow_recover_irq_mask1);
		}
		atomic_set(&vfe_dev->error_info.overflow_state,
			OVERFLOW_DETECTED);
	}

	rc = vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev, 1);

	if (halt_cmd->stop_camif) {
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, DISABLE_CAMIF_IMMEDIATELY);
	}

	return rc;
}

int msm_isp_axi_reset(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_reset_cmd *reset_cmd)
{
	int rc = 0, i, j;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_isp_bufq *bufq = NULL;
	struct msm_isp_timestamp timestamp;

	if (!reset_cmd) {
		pr_err("%s: NULL pointer reset cmd %pK\n", __func__, reset_cmd);
		rc = -1;
		return rc;
	}

	msm_isp_get_timestamp(&timestamp, vfe_dev);
	rc = vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev,
		0, reset_cmd->blocking);

	for (i = 0, j = 0; j < axi_data->num_active_stream &&
		i < MAX_NUM_STREAM; i++, j++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->stream_src >= VFE_AXI_SRC_MAX) {
			rc = -1;
			pr_err("%s invalid  stream src = %d\n", __func__,
				stream_info->stream_src);
			break;
		}
		if (stream_info->state != ACTIVE) {
			j--;
			continue;
		}

		bufq = vfe_dev->buf_mgr->ops->get_bufq(vfe_dev->buf_mgr,
			stream_info->bufq_handle);
		if (!bufq) {
			pr_err("%s: bufq null %pK by handle %x\n", __func__,
				bufq, stream_info->bufq_handle);
			continue;
		}

		if (bufq->buf_type != ISP_SHARE_BUF) {
			msm_isp_deinit_stream_ping_pong_reg(vfe_dev,
				stream_info);
		} else {
			vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
				stream_info->bufq_handle,
				MSM_ISP_BUFFER_FLUSH_ALL,
				&timestamp.buf_time,
				reset_cmd->frame_id);
		}
		axi_data->src_info[SRC_TO_INTF(stream_info->stream_src)].
			frame_id = reset_cmd->frame_id;
		msm_isp_reset_burst_count_and_frame_drop(vfe_dev, stream_info);
	}

	if (rc < 0)
		pr_err("%s Error! reset hw Timed out\n", __func__);

	return rc;
}

int msm_isp_axi_restart(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_restart_cmd *restart_cmd)
{
	int rc = 0, i, j;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t wm_reload_mask = 0x0;

	for (i = 0, j = 0; j < axi_data->num_active_stream &&
		i < MAX_NUM_STREAM; i++, j++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->state != ACTIVE) {
			j--;
			continue;
		}
		msm_isp_get_stream_wm_mask(stream_info, &wm_reload_mask);
		msm_isp_init_stream_ping_pong_reg(vfe_dev, stream_info);
	}

	vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev, wm_reload_mask);
	rc = vfe_dev->hw_info->vfe_ops.axi_ops.restart(vfe_dev, 0,
		restart_cmd->enable_camif);
	if (rc < 0)
		pr_err("%s Error restarting HW\n", __func__);

	return rc;
}

static int msm_isp_axi_update_cgc_override(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd,
	uint8_t cgc_override)
{
	int i = 0, j = 0;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
				MAX_NUM_STREAM) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		for (j = 0; j < stream_info->num_planes; j++) {
			if (vfe_dev->hw_info->vfe_ops.axi_ops.
				update_cgc_override)
				vfe_dev->hw_info->vfe_ops.axi_ops.
					update_cgc_override(vfe_dev,
					stream_info->wm[j], cgc_override);
		}
	}
	return 0;
}

static int msm_isp_start_axi_stream(struct vfe_device *vfe_dev,
			struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd,
			enum msm_isp_camif_update_state camif_update)
{
	int i, rc = 0;
	uint8_t src_state, wait_for_complete = 0;
	uint32_t wm_reload_mask = 0x0;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint8_t init_frm_drop = 0;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			MAX_NUM_STREAM) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		if (SRC_TO_INTF(stream_info->stream_src) < VFE_SRC_MAX)
			src_state = axi_data->src_info[
				SRC_TO_INTF(stream_info->stream_src)].active;
		else {
			pr_err("%s: invalid src info index\n", __func__);
			return -EINVAL;
		}

		msm_isp_calculate_bandwidth(axi_data, stream_info);
		msm_isp_reset_framedrop(vfe_dev, stream_info);
		init_frm_drop = stream_info->init_frame_drop;
		msm_isp_get_stream_wm_mask(stream_info, &wm_reload_mask);
		rc = msm_isp_init_stream_ping_pong_reg(vfe_dev, stream_info);
		if (rc < 0) {
			pr_err("%s: No buffer for stream %d\n",
					__func__,
				HANDLE_TO_IDX(
				stream_cfg_cmd->stream_handle[i]));
			return rc;
		}

		stream_info->state = START_PENDING;
		if (src_state) {
			wait_for_complete = 1;
		} else {
			if (vfe_dev->dump_reg)
				msm_camera_io_dump_2(vfe_dev->vfe_base, 0x900);

			/*Configure AXI start bits to start immediately*/
			msm_isp_axi_stream_enable_cfg(vfe_dev, stream_info);
			stream_info->state = ACTIVE;
		}
		if (SRC_TO_INTF(stream_info->stream_src) != VFE_PIX_0 &&
			stream_info->stream_src < VFE_AXI_SRC_MAX) {
			vfe_dev->axi_data.src_info[SRC_TO_INTF(
				stream_info->stream_src)].frame_id =
						init_frm_drop;
		}
	}
	msm_isp_update_stream_bandwidth(vfe_dev);
	vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev, wm_reload_mask);
	vfe_dev->hw_info->vfe_ops.core_ops.reg_update(vfe_dev, 0xF);
	msm_isp_update_camif_output_count(vfe_dev, stream_cfg_cmd);
	msm_isp_update_rdi_output_count(vfe_dev, stream_cfg_cmd);
	/*Configure UB*/
	vfe_dev->hw_info->vfe_ops.axi_ops.cfg_ub(vfe_dev);
	if (camif_update == ENABLE_CAMIF) {
		atomic_set(&vfe_dev->error_info.overflow_state,
				NO_OVERFLOW);
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id = 0;
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, camif_update);
	}

	if (wait_for_complete) {
		vfe_dev->axi_data.stream_update = stream_cfg_cmd->num_streams;
		rc = msm_isp_axi_wait_for_cfg_done(vfe_dev, camif_update);
	}

	return rc;
}

static int msm_isp_stop_axi_stream(struct vfe_device *vfe_dev,
			struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd,
			enum msm_isp_camif_update_state camif_update)
{
	int i, rc = 0;
	uint8_t wait_for_complete_for_this_stream = 0, cur_stream_cnt = 0;
	uint8_t wait_for_complete = 0;
	struct msm_vfe_axi_stream *stream_info = NULL;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int    ext_read =
		axi_data->src_info[VFE_PIX_0].input_mux == EXTERNAL_READ;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM ||
		stream_cfg_cmd->num_streams == 0)
		return -EINVAL;

	msm_isp_update_camif_output_count(vfe_dev, stream_cfg_cmd);
	msm_isp_update_rdi_output_count(vfe_dev, stream_cfg_cmd);
	cur_stream_cnt = msm_isp_get_curr_stream_cnt(vfe_dev);

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			MAX_NUM_STREAM) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		wait_for_complete_for_this_stream = 0;
		stream_info->state = STOP_PENDING;
		if (stream_info->stream_src == CAMIF_RAW ||
			stream_info->stream_src == IDEAL_RAW) {
			/* We dont get reg update IRQ for raw snapshot
			 * so frame skip cant be ocnfigured
			 */
			if ((camif_update != DISABLE_CAMIF_IMMEDIATELY) &&
					(!ext_read))
				wait_for_complete_for_this_stream = 1;
		} else if (stream_info->stream_type == BURST_STREAM &&
				stream_info->runtime_num_burst_capture == 0) {
			/* Configure AXI writemasters to stop immediately
			 * since for burst case, write masters already skip
			 * all frames.
			 */
			if (stream_info->stream_src == RDI_INTF_0 ||
				stream_info->stream_src == RDI_INTF_1 ||
				stream_info->stream_src == RDI_INTF_2)
				wait_for_complete_for_this_stream = 1;
		} else {
			if  ((camif_update != DISABLE_CAMIF_IMMEDIATELY) &&
				(!ext_read) &&
				!(stream_info->stream_src == RDI_INTF_0 ||
				stream_info->stream_src == RDI_INTF_1 ||
				stream_info->stream_src == RDI_INTF_2))
				wait_for_complete_for_this_stream = 1;
		}
		if (!wait_for_complete_for_this_stream) {
			msm_isp_axi_stream_enable_cfg(vfe_dev, stream_info);
			stream_info->state = INACTIVE;
			vfe_dev->hw_info->vfe_ops.core_ops.
				reg_update(vfe_dev, 0xF);
		}
		wait_for_complete |= wait_for_complete_for_this_stream;
	}
	if (wait_for_complete) {
		vfe_dev->axi_data.stream_update = stream_cfg_cmd->num_streams;
		vfe_dev->hw_info->vfe_ops.core_ops.
				reg_update(vfe_dev, 0xF);
		rc = msm_isp_axi_wait_for_cfg_done(vfe_dev, camif_update);
		if (rc < 0) {
			pr_err("%s: wait for config done failed\n", __func__);
			for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
				stream_info = &axi_data->stream_info[
				HANDLE_TO_IDX(
					stream_cfg_cmd->stream_handle[i])];
				stream_info->state = STOP_PENDING;
				vfe_dev->hw_info->vfe_ops.core_ops.
					reg_update(vfe_dev, 0xF);
				msm_isp_axi_stream_enable_cfg(
					vfe_dev, stream_info);
				stream_info->state = INACTIVE;
			}
		}
	}
	if (camif_update == DISABLE_CAMIF) {
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, DISABLE_CAMIF);
	} else if ((camif_update == DISABLE_CAMIF_IMMEDIATELY) ||
					(ext_read)) {
		/*during stop immediately, stop output then stop input*/
		if (!ext_read)
			vfe_dev->hw_info->vfe_ops.core_ops.
				update_camif_state(vfe_dev,
						DISABLE_CAMIF_IMMEDIATELY);
	}
	if (cur_stream_cnt == 0) {
		vfe_dev->ignore_error = 1;
		vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev, 1);
		vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev, 1, 1);
		vfe_dev->hw_info->vfe_ops.core_ops.init_hw_reg(vfe_dev);
		vfe_dev->ignore_error = 0;
	}
	msm_isp_update_stream_bandwidth(vfe_dev);

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		msm_isp_deinit_stream_ping_pong_reg(vfe_dev, stream_info);
	}

	return rc;
}


int msm_isp_cfg_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd = arg;
	enum msm_isp_camif_update_state camif_update;

	rc = msm_isp_axi_check_stream_state(vfe_dev, stream_cfg_cmd);
	if (rc < 0) {
		pr_err("%s: Invalid stream state\n", __func__);
		return rc;
	}

	camif_update = msm_isp_get_camif_update_state(vfe_dev, stream_cfg_cmd);

	if (stream_cfg_cmd->cmd == START_STREAM) {
		msm_isp_axi_update_cgc_override(vfe_dev, stream_cfg_cmd, 1);

		rc = msm_isp_start_axi_stream(
		   vfe_dev, stream_cfg_cmd, camif_update);
	} else {
		rc = msm_isp_stop_axi_stream(
		   vfe_dev, stream_cfg_cmd, camif_update);

		msm_isp_axi_update_cgc_override(vfe_dev, stream_cfg_cmd, 0);
	}

	if (rc < 0)
		pr_err("%s: start/stop stream failed\n", __func__);
	return rc;
}

static int msm_isp_request_frame(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t request_frm_num)
{
	struct msm_vfe32_axi_stream_request_cmd stream_cfg_cmd;
	int rc = 0;
	uint32_t pingpong_status, pingpong_bit, wm_reload_mask = 0x0;

	if (!stream_info->controllable_output)
		return 0;

	if (!request_frm_num) {
		pr_err("%s: Invalid frame request.\n", __func__);
		return -EINVAL;
	}

	stream_info->request_frm_num += request_frm_num;

	stream_cfg_cmd.axi_stream_handle = stream_info->stream_handle;
	stream_cfg_cmd.frame_skip_pattern = NO_SKIP;
	stream_cfg_cmd.init_frame_drop = 0;
	stream_cfg_cmd.burst_count = stream_info->request_frm_num;
	msm_isp_calculate_framedrop(&vfe_dev->axi_data, &stream_cfg_cmd);
	msm_isp_reset_framedrop(vfe_dev, stream_info);

	if (stream_info->request_frm_num != request_frm_num) {
		pingpong_status =
			vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(
				vfe_dev);
		pingpong_bit = (~(pingpong_status >> stream_info->wm[0]) & 0x1);

		if (!stream_info->buf[pingpong_bit]) {
			rc = msm_isp_cfg_ping_pong_address(vfe_dev, stream_info,
				pingpong_status, pingpong_bit);
			if (rc) {
				pr_err("%s:%d fail to set ping pong address\n",
					__func__, __LINE__);
				return rc;
			}
		}

		if (!stream_info->buf[!pingpong_bit] && request_frm_num > 1) {
			rc = msm_isp_cfg_ping_pong_address(vfe_dev, stream_info,
				~pingpong_status, !pingpong_bit);
			if (rc) {
				pr_err("%s:%d fail to set ping pong address\n",
					__func__, __LINE__);
				return rc;
			}
		}
	} else {
		if (!stream_info->buf[0]) {
			rc = msm_isp_cfg_ping_pong_address(vfe_dev, stream_info,
				VFE_PING_FLAG, 0);
			if (rc) {
				pr_err("%s:%d fail to set ping pong address\n",
					__func__, __LINE__);
				return rc;
			}
		}

		if (!stream_info->buf[1] && request_frm_num > 1) {
			rc = msm_isp_cfg_ping_pong_address(vfe_dev, stream_info,
				VFE_PONG_FLAG, 1);
			if (rc) {
				pr_err("%s:%d fail to set ping pong address\n",
					__func__, __LINE__);
				return rc;
			}
		}

		msm_isp_get_stream_wm_mask(stream_info, &wm_reload_mask);
		vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev,
			wm_reload_mask);
	}

	return rc;
}

int msm_isp_update_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i, j;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream_update_cmd *update_cmd = arg;
	struct msm_vfe_axi_stream_cfg_update_info *update_info;
	uint32_t frame_id;
	struct msm_isp_timestamp timestamp;

	if (update_cmd->update_type == UPDATE_STREAM_AXI_CONFIG &&
		atomic_read(&axi_data->axi_cfg_update)) {
		pr_err("%s: AXI stream config updating\n", __func__);
		return -EBUSY;
	}

	/*num_stream is uint32 and update_info[] bound by MAX_NUM_STREAM*/
	if (update_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = &update_cmd->update_info[i];
		/*check array reference bounds*/
		if (HANDLE_TO_IDX(update_info->stream_handle) >=
			MAX_NUM_STREAM) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
				HANDLE_TO_IDX(update_info->stream_handle)];
		if (stream_info->state != ACTIVE &&
			stream_info->state != INACTIVE) {
			pr_err("%s: Invalid stream state\n", __func__);
			return -EINVAL;
		}
	}

	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = &update_cmd->update_info[i];
		stream_info = &axi_data->stream_info[
				HANDLE_TO_IDX(update_info->stream_handle)];

		switch (update_cmd->update_type) {
		case ENABLE_STREAM_BUF_DIVERT:
			stream_info->buf_divert = 1;
			break;
		case DISABLE_STREAM_BUF_DIVERT:
			msm_isp_get_timestamp(&timestamp, vfe_dev);
			stream_info->buf_divert = 0;
			frame_id = vfe_dev->axi_data.
					src_info[SRC_TO_INTF(
					stream_info->stream_src)].
					frame_id;
			vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
					stream_info->bufq_handle,
					MSM_ISP_BUFFER_FLUSH_DIVERTED,
					&timestamp.buf_time, frame_id);
			break;
		case UPDATE_STREAM_FRAMEDROP_PATTERN: {
			uint32_t framedrop_period =
				msm_isp_get_framedrop_period(
				   update_info->skip_pattern);
			if (update_info->skip_pattern == SKIP_ALL)
				stream_info->framedrop_pattern = 0x0;
			else
				stream_info->framedrop_pattern = 0x1;
			stream_info->framedrop_period = framedrop_period - 1;
			if (stream_info->stream_type == BURST_STREAM) {
				stream_info->runtime_framedrop_update_burst = 1;
			} else {
				stream_info->runtime_init_frame_drop = 0;
				vfe_dev->hw_info->vfe_ops.axi_ops.
					cfg_framedrop(vfe_dev, stream_info);
			}
			break;
		}
		case UPDATE_STREAM_AXI_CONFIG: {
			for (j = 0; j < stream_info->num_planes; j++) {
				stream_info->plane_cfg[j] =
					update_info->plane_cfg[j];
			}
			stream_info->output_format = update_info->output_format;
			if (stream_info->state == ACTIVE) {
				stream_info->state = PAUSE_PENDING;
				msm_isp_axi_stream_enable_cfg(
					vfe_dev, stream_info);
				stream_info->state = PAUSING;
				atomic_set(&axi_data->axi_cfg_update,
					UPDATE_REQUESTED);
			} else {
				for (j = 0; j < stream_info->num_planes; j++)
					vfe_dev->hw_info->vfe_ops.axi_ops.
					cfg_wm_reg(vfe_dev, stream_info, j);
				stream_info->runtime_output_format =
					stream_info->output_format;
			}
			break;
		}
		case UPDATE_STREAM_REQUEST_FRAMES: {
			rc = msm_isp_request_frame(vfe_dev, stream_info,
				update_info->frame_id);
			if (rc)
				pr_err("%s failed to request frame!\n",
					__func__);
			break;
		}
		default:
			pr_err("%s: Invalid update type\n", __func__);
			return -EINVAL;
		}
	}
	return rc;
}

void msm_isp_process_axi_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	int i, rc = 0;
	struct msm_isp_buffer *done_buf = NULL;
	uint32_t comp_mask = 0, wm_mask = 0;
	uint32_t pingpong_status, stream_idx;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_composite_info *comp_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t pingpong_bit = 0, frame_id = 0;

	comp_mask = vfe_dev->hw_info->vfe_ops.axi_ops.
		get_comp_mask(irq_status0, irq_status1);
	wm_mask = vfe_dev->hw_info->vfe_ops.axi_ops.
		get_wm_mask(irq_status0, irq_status1);
	if (!(comp_mask || wm_mask))
		return;

	ISP_DBG("%s: status: 0x%x\n", __func__, irq_status0);
	pingpong_status =
		vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(vfe_dev);
	for (i = 0; i < axi_data->hw_info->num_comp_mask; i++) {
		rc = 0;
		comp_info = &axi_data->composite_info[i];
		if (comp_mask & (1 << i)) {
			stream_idx = HANDLE_TO_IDX(comp_info->stream_handle);
			if ((!comp_info->stream_handle) ||
				(stream_idx >= MAX_NUM_STREAM)) {
				pr_err("%s: Invalid handle for composite irq\n",
					__func__);
			} else {
				stream_idx =
					HANDLE_TO_IDX(comp_info->stream_handle);
				stream_info =
					&axi_data->stream_info[stream_idx];

				pingpong_bit = (~(pingpong_status >>
					stream_info->wm[0]) & 0x1);

				if (stream_info->stream_type == BURST_STREAM)
					stream_info->
						runtime_num_burst_capture--;

				msm_isp_get_done_buf(vfe_dev, stream_info,
					pingpong_status, &done_buf);
				if (stream_info->stream_type ==
					CONTINUOUS_STREAM ||
					stream_info->
					runtime_num_burst_capture > 1) {
					rc = msm_isp_cfg_ping_pong_address(
							vfe_dev, stream_info,
							pingpong_status,
							pingpong_bit);
				}
				frame_id = vfe_dev->axi_data.
					src_info[SRC_TO_INTF(
					stream_info->stream_src)].
					frame_id;
				stream_info->frame_id = frame_id;
				ISP_DBG("%s: stream id:%d frame id:%d\n",
					__func__, stream_info->stream_id,
					stream_info->frame_id);
				if (done_buf && !rc)
					msm_isp_process_done_buf(vfe_dev,
					stream_info, done_buf, ts);
			}
		}
		wm_mask &= ~(comp_info->stream_composite_mask);
	}

	for (i = 0; i < axi_data->hw_info->num_wm; i++) {
		if (wm_mask & (1 << i)) {
			stream_idx = HANDLE_TO_IDX(axi_data->free_wm[i]);
			if ((!axi_data->free_wm[i]) ||
				(stream_idx >= MAX_NUM_STREAM)) {
				pr_err("%s: Invalid handle for wm irq\n",
					__func__);
				continue;
			}
			stream_info = &axi_data->stream_info[stream_idx];

			pingpong_bit = (~(pingpong_status >>
				stream_info->wm[0]) & 0x1);

			if (stream_info->stream_type == BURST_STREAM)
				stream_info->runtime_num_burst_capture--;

			msm_isp_get_done_buf(vfe_dev, stream_info,
						pingpong_status, &done_buf);
			if (stream_info->stream_type == CONTINUOUS_STREAM ||
				stream_info->runtime_num_burst_capture > 1) {
				rc = msm_isp_cfg_ping_pong_address(vfe_dev,
					stream_info, pingpong_status,
					pingpong_bit);
			}
			stream_info->frame_id = frame_id;
			ISP_DBG("%s: stream id:%d frame id:%d\n",
				__func__, stream_info->stream_id,
				stream_info->frame_id);
			if (done_buf && !rc)
				msm_isp_process_done_buf(vfe_dev,
				stream_info, done_buf, ts);
		}
	}
}
int msm_isp_user_buf_done(struct vfe_device *vfe_dev,
	struct msm_isp32_event_data *buf_cmd)
{
	int rc = 0;
	struct msm_isp32_event_data buf_event;

	memset(&buf_event, 0, sizeof(buf_event));
	buf_event.input_intf = buf_cmd->input_intf;
	buf_event.frame_id = buf_cmd->frame_id;
	buf_event.timestamp = buf_cmd->timestamp;
	buf_event.u.buf_done.session_id =
	  buf_cmd->u.buf_done.session_id;
	buf_event.u.buf_done.stream_id =
	  buf_cmd->u.buf_done.stream_id;
	buf_event.u.buf_done.output_format =
	  buf_cmd->u.buf_done.output_format;
	buf_event.u.buf_done.buf_idx =
	  buf_cmd->u.buf_done.buf_idx;
	buf_event.u.buf_done.handle =
	   buf_cmd->u.buf_done.handle;

	vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
			buf_event.u.buf_done.handle,
			buf_event.u.buf_done.buf_idx,
			&buf_event.timestamp, buf_event.frame_id,
			buf_event.u.buf_done.output_format);
	return rc;
}
