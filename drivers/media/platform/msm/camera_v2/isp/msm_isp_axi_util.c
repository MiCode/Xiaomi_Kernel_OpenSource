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
#include "msm_isp_axi_util.h"

int msm_isp_axi_create_stream(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int i, rc = -1;
	for (i = 0; i < MAX_NUM_STREAM; i++) {
		if (axi_data->stream_info[i].state == AVALIABLE)
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

	axi_data->stream_info[i].session_id = stream_cfg_cmd->session_id;
	axi_data->stream_info[i].stream_id = stream_cfg_cmd->stream_id;
	axi_data->stream_info[i].buf_divert = stream_cfg_cmd->buf_divert;
	axi_data->stream_info[i].state = INACTIVE;
	axi_data->stream_info[i].stream_handle =
		stream_cfg_cmd->axi_stream_handle;
	return 0;
}

void msm_isp_axi_destroy_stream(
	struct msm_vfe_axi_shared_data *axi_data, int stream_idx)
{
	if (axi_data->stream_info[stream_idx].state != AVALIABLE) {
		axi_data->stream_info[stream_idx].state = AVALIABLE;
		axi_data->stream_info[stream_idx].stream_handle = 0;
	} else {
		pr_err("%s: stream does not exist\n", __func__);
	}
}

int msm_isp_validate_axi_request(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int rc = -1;
	struct msm_vfe_axi_stream *stream_info =
		&axi_data->stream_info[
			(stream_cfg_cmd->axi_stream_handle & 0xFF)];

	switch (stream_cfg_cmd->output_format) {
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
		stream_info->num_planes = 1;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		stream_info->num_planes = 2;
		break;
	/*TD: Add more image format*/
	default:
		pr_err("%s: Invalid output format\n", __func__);
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

	if (stream_cfg_cmd->stream_src == RDI) {
		if (axi_data->hw_info->num_rdi -
			axi_data->num_used_rdi < stream_info->num_planes) {
			pr_err("%s: No free RDI\n", __func__);
			return rc;
		}
	}

	if (stream_cfg_cmd->init_frame_drop >= MAX_INIT_FRAME_DROP) {
		pr_err("%s: Invalid skip pattern\n", __func__);
		return rc;
	}

	if (stream_cfg_cmd->frame_skip_pattern >= MAX_SKIP) {
		pr_err("%s: Invalid skip pattern\n", __func__);
		return rc;
	}

	stream_info->stream_src = stream_cfg_cmd->stream_src;
	return 0;
}

static uint32_t msm_isp_axi_get_plane_size(
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd, int plane_idx)
{
	uint32_t size = 0;
	struct msm_vfe_axi_plane_cfg *plane_cfg = stream_cfg_cmd->plane_cfg;
	switch (stream_cfg_cmd->output_format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		/* TODO: fix me */
		size = plane_cfg[plane_idx].output_height *
		plane_cfg[plane_idx].output_width;
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		/* TODO: fix me */
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
				plane_cfg[plane_idx].output_width / 2;
		break;
	/*TD: Add more image format*/
	default:
		pr_err("%s: Invalid output format\n", __func__);
		break;
	}
	return size;
}

void msm_isp_axi_reserve_wm(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int i, j;
	struct msm_vfe_axi_stream *stream_info =
		&axi_data->stream_info[
			(stream_cfg_cmd->axi_stream_handle & 0xFF)];

	for (i = 0; i < stream_info->num_planes; i++) {
		for (j = 0; j < axi_data->hw_info->num_wm; j++) {
			if (!axi_data->free_wm[j]) {
				axi_data->free_wm[j] =
					stream_cfg_cmd->axi_stream_handle;
				axi_data->wm_image_size[j] =
					msm_isp_axi_get_plane_size(
						stream_cfg_cmd, i);
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
}

void msm_isp_axi_reserve_rdi(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int i, j;
	struct msm_vfe_axi_stream *stream_info =
	&axi_data->stream_info[(stream_cfg_cmd->axi_stream_handle & 0xFF)];

	for (i = 0; i < stream_info->num_planes; i++) {
		uint8_t csid = stream_cfg_cmd->plane_cfg[i].csid_src;

		for (j = 0; j < axi_data->hw_info->num_rdi; j++) {
			if (!axi_data->free_rdi[j]) {
				axi_data->free_rdi[j] = 1;
				axi_data->num_used_rdi++;
				break;
			}
		}
		stream_info->rdi[i] = j;

		for (j = 0; j < axi_data->hw_info->num_rdi; j++) {
			if (!axi_data->free_rdi_master[csid][j]) {
				axi_data->free_rdi_master[csid][j] = 1;
				axi_data->num_used_rdi++;
				break;
			}
		}
		stream_info->rdi_master[i] =
			csid * axi_data->hw_info->num_rdi_master + j;
	}
	return;
}

void msm_isp_axi_reserve_comp_mask(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int i;
	uint8_t comp_mask = 0;
	struct msm_vfe_axi_stream *stream_info =
		&axi_data->stream_info[
			(stream_cfg_cmd->axi_stream_handle & 0xFF)];
	for (i = 0; i < stream_info->num_planes; i++)
		comp_mask |= 1 << stream_info->wm[i];

	for (i = 0; i < axi_data->hw_info->num_comp_mask; i++) {
		if (!axi_data->composite_info[i].stream_handle) {
			axi_data->composite_info[i].stream_handle =
			stream_cfg_cmd->axi_stream_handle;
			axi_data->composite_info[i].
				stream_composite_mask = comp_mask;
			axi_data->num_used_composite_mask++;
			break;
		}
	}
	stream_info->comp_mask_index = i;
	return;
}

void msm_isp_axi_free_comp_mask(struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream *stream_info)
{
	axi_data->composite_info[stream_info->comp_mask_index].
		stream_composite_mask = 0;
	axi_data->composite_info[stream_info->comp_mask_index].
		stream_handle = 0;
	axi_data->num_used_composite_mask--;
}

int msm_isp_axi_check_stream_state(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int rc = 0, i, j;
	uint8_t src_state;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;
	enum msm_vfe_axi_state valid_state =
		(stream_cfg_cmd->cmd == START_STREAM) ? INACTIVE : ACTIVE;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		stream_info = &axi_data->stream_info[
			(stream_cfg_cmd->stream_handle[i] & 0xFF)];
		if (stream_info->state != valid_state) {
			pr_err("%s: Invalid stream state\n", __func__);
			rc = -EINVAL;
			break;
		}
		/*
		 * For RDI stream, if multiple RDIs are used
		 * check if all the RDI srcs are in the same state, on/off
		 */
		if (stream_info->stream_src == RDI) {
			src_state = axi_data->src_info[
				stream_info->rdi[0]+1].active;
			for (j = 0; j < stream_info->num_planes; j++) {
				if (src_state !=
						axi_data->src_info[
						stream_info->rdi[j]+1].active) {
					pr_err("%s: RDI stream has inconsistent state\n",
						__func__);
					rc = -EINVAL;
					break;
				}
			}
		}

		if (stream_cfg_cmd->cmd == START_STREAM) {
			stream_info->bufq_handle =
				vfe_dev->buf_mgr->ops->get_bufq_handle(
			vfe_dev->buf_mgr, stream_info->session_id,
			stream_info->stream_id);
			if (stream_info->bufq_handle == 0) {
				pr_err("%s: Stream has no valid buffer queue\n",
					__func__);
				rc = -EINVAL;
				break;
			}
		}
	}
	return rc;
}

void msm_isp_update_framedrop_reg(struct vfe_device *vfe_dev)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;
	for (i = 0; i < MAX_NUM_STREAM; i++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->framedrop_update) {
			if (stream_info->init_frame_drop == 0) {
				stream_info->framedrop_update = 0;
				vfe_dev->hw_info->vfe_ops.axi_ops.
				cfg_framedrop(vfe_dev, stream_info);
			}
		}
		if (stream_info->stream_type == BURST_STREAM) {
			if (stream_info->burst_frame_count == 0 &&
					stream_info->state == ACTIVE) {
				vfe_dev->hw_info->vfe_ops.axi_ops.
				cfg_framedrop(vfe_dev, stream_info);
				if (stream_info->stream_src == RDI) {
					uint32_t wm_reload_mask = 0,
					reg_update_mask = 0;
					stream_info->state = STOP_PENDING;
					msm_isp_axi_stream_enable_cfg(
						vfe_dev, stream_info,
						&wm_reload_mask,
						&reg_update_mask);
				}
			}
		}
	}
}

void msm_isp_update_framedrop_count(
	struct vfe_device *vfe_dev)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;
	for (i = 0; i < MAX_NUM_STREAM; i++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->framedrop_update) {
			stream_info->init_frame_drop--;
			if (stream_info->init_frame_drop == 1) {
				vfe_dev->hw_info->vfe_ops.core_ops.
					reg_update(vfe_dev, 0xF);
			}
		}
		if (stream_info->stream_type == BURST_STREAM) {
			stream_info->burst_frame_count--;
			if (stream_info->burst_frame_count == 1) {
				vfe_dev->hw_info->vfe_ops.core_ops.
					reg_update(vfe_dev, 0xF);
			} else if (stream_info->burst_frame_count == 0) {
				if (stream_info->stream_src != RDI) {
					vfe_dev->hw_info->vfe_ops.core_ops.
						update_camif_state(vfe_dev,
						DISABLE_CAMIF);
					pr_err("%s: pending burst_cnt = %d, disable camif\n",
						__func__,
						stream_info->burst_frame_count);
				}
			}
		}
	}
}

void msm_isp_sof_notify(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src, struct timeval *tv) {
	struct msm_isp_event_data sof_event;
	switch (frame_src) {
	case VFE_PIX_0:
		ISP_DBG("%s: PIX0 frame id: %lu\n", __func__,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		msm_isp_update_framedrop_count(vfe_dev);
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id++;
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id == 0)
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id = 1;
		break;
	case VFE_RAW_0:
	case VFE_RAW_1:
	case VFE_RAW_2:
		break;
	default:
		pr_err("%s: invalid frame src %d received\n",
			__func__, frame_src);
		break;
	}

	sof_event.frame_id = vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
	sof_event.timestamp = *tv;
	msm_isp_send_event(vfe_dev, ISP_EVENT_SOF, &sof_event);
}

void msm_isp_calculate_framedrop(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	struct msm_vfe_axi_stream *stream_info =
		&axi_data->stream_info[
		(stream_cfg_cmd->axi_stream_handle & 0xFF)];
	uint32_t framedrop_period = 1;

	switch (stream_cfg_cmd->frame_skip_pattern) {
	case NO_SKIP:
		stream_info->framedrop_pattern = VFE_NO_DROP;
		framedrop_period = 1;
		break;
	case EVERY_2FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_2FRAME;
		framedrop_period = 2;
		break;
	case EVERY_4FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_4FRAME;
		framedrop_period = 4;
		break;
	case EVERY_8FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_8FRAME;
		framedrop_period = 8;
		break;
	case EVERY_16FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_16FRAME;
		framedrop_period = 16;
		break;
	case EVERY_32FRAME:
		stream_info->framedrop_pattern = VFE_DROP_EVERY_32FRAME;
		framedrop_period = 32;
		break;
	default:
		stream_info->framedrop_pattern = VFE_NO_DROP;
		framedrop_period = 1;
		break;
	}

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

int msm_isp_request_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i;
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd = arg;
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
		msm_isp_axi_destroy_stream(&vfe_dev->axi_data,
			(stream_cfg_cmd->axi_stream_handle & 0xFF));
		return rc;
	}

	msm_isp_axi_reserve_wm(&vfe_dev->axi_data, stream_cfg_cmd);
	if (stream_cfg_cmd->stream_src == RDI)
		msm_isp_axi_reserve_rdi(&vfe_dev->axi_data, stream_cfg_cmd);

	stream_info =
		&vfe_dev->axi_data.
			stream_info[(stream_cfg_cmd->axi_stream_handle & 0xFF)];

	if (stream_cfg_cmd->stream_src == CAMIF_RAW ||
		stream_cfg_cmd->stream_src == IDEAL_RAW)
			vfe_dev->hw_info->vfe_ops.axi_ops.
				cfg_io_format(vfe_dev, stream_cfg_cmd);

	msm_isp_calculate_framedrop(&vfe_dev->axi_data, stream_cfg_cmd);
	vfe_dev->hw_info->vfe_ops.axi_ops.cfg_framedrop(vfe_dev, stream_info);

	if (stream_info->num_planes > 1) {
		msm_isp_axi_reserve_comp_mask(
			&vfe_dev->axi_data, stream_cfg_cmd);
		vfe_dev->hw_info->vfe_ops.axi_ops.
		cfg_comp_mask(vfe_dev, stream_info);
	} else {
		vfe_dev->hw_info->vfe_ops.axi_ops.
			cfg_wm_irq_mask(vfe_dev, stream_info);
	}

	for (i = 0; i < stream_info->num_planes; i++) {
		vfe_dev->hw_info->vfe_ops.axi_ops.
			cfg_wm_reg(vfe_dev, stream_cfg_cmd, i);

	vfe_dev->hw_info->vfe_ops.axi_ops.
		cfg_wm_xbar_reg(vfe_dev, stream_cfg_cmd, i);

	if (stream_cfg_cmd->stream_src == RDI)
		vfe_dev->hw_info->vfe_ops.axi_ops.
			cfg_rdi_reg(vfe_dev, stream_cfg_cmd, i);
	}
	return rc;
}

int msm_isp_release_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i;
	struct msm_vfe_axi_stream_release_cmd *stream_release_cmd = arg;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info =
		&axi_data->stream_info[
		(stream_release_cmd->stream_handle & 0xFF)];
	struct msm_vfe_axi_stream_cfg_cmd stream_cfg;

	if (stream_info->state == AVALIABLE) {
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
		msm_isp_axi_free_comp_mask(&vfe_dev->axi_data, stream_info);
		vfe_dev->hw_info->vfe_ops.axi_ops.
		clear_comp_mask(vfe_dev, stream_info);
	} else {
		vfe_dev->hw_info->vfe_ops.axi_ops.
		clear_wm_irq_mask(vfe_dev, stream_info);
	}

	vfe_dev->hw_info->vfe_ops.axi_ops.clear_framedrop(vfe_dev, stream_info);
	msm_isp_axi_free_wm(axi_data, stream_info);

	msm_isp_axi_destroy_stream(&vfe_dev->axi_data,
		(stream_release_cmd->stream_handle & 0xFF));

	return rc;
}

void msm_isp_axi_stream_enable_cfg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint32_t *wm_reload_mask, uint32_t *reg_update_mask)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	if (stream_info->state == INACTIVE)
		return;
	for (i = 0; i < stream_info->num_planes; i++) {
		/*TD: Frame base command*/
		if (stream_info->state == START_PENDING)
			vfe_dev->hw_info->vfe_ops.axi_ops.
				enable_wm(vfe_dev, stream_info->wm[i], 1);
		else
			vfe_dev->hw_info->vfe_ops.axi_ops.
				enable_wm(vfe_dev, stream_info->wm[i], 0);

	*wm_reload_mask |= (1 << stream_info->wm[i]);
	if (stream_info->stream_src == RDI)
		*reg_update_mask |= (1 << stream_info->rdi[i]);
	}
	if (stream_info->state == START_PENDING) {
		axi_data->num_active_stream++;
		stream_info->state = ACTIVE;
	} else {
		axi_data->num_active_stream--;
		stream_info->state = INACTIVE;
	}
}

void msm_isp_axi_stream_update(struct vfe_device *vfe_dev)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t wm_reload_mask = 0x0, reg_update_mask = 0x1;
	int send_update_complete = 0;
	for (i = 0; i < MAX_NUM_STREAM; i++) {
		if (axi_data->stream_info[i].state == START_PENDING ||
				axi_data->stream_info[i].state ==
					STOP_PENDING) {
			msm_isp_axi_stream_enable_cfg(
				vfe_dev, &axi_data->stream_info[i],
				&wm_reload_mask, &reg_update_mask);
			if (axi_data->stream_info[i].state == STOP_PENDING)
				axi_data->stream_info[i].state = STOPPING;
		} else if (axi_data->stream_info[i].state == STOPPING) {
			send_update_complete = 1;
			axi_data->stream_info[i].state = INACTIVE;
		}
	}
	/*Reload AXI*/
	vfe_dev->hw_info->vfe_ops.axi_ops.
		reload_wm(vfe_dev, wm_reload_mask);
	/*Reg update per src*/
	vfe_dev->hw_info->vfe_ops.core_ops.
		reg_update(vfe_dev, reg_update_mask);
	if (send_update_complete) {
		ISP_DBG("%s: send update complete\n", __func__);
		vfe_dev->axi_data.stream_update = 0;
		complete(&vfe_dev->stream_config_complete);
	}
}

int msm_isp_cfg_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	struct timeval *tv)
{
	int i, rc = -1;
	struct msm_isp_buffer *buf = NULL;
	struct msm_isp_event_data buf_event;
	uint32_t pingpong_bit = 0;
	uint32_t bufq_handle = stream_info->bufq_handle;
	uint32_t stream_idx = stream_info->stream_handle & 0xFF;

	pingpong_bit = (~(pingpong_status >> stream_info->wm[0]) & 0x1);
	rc = vfe_dev->buf_mgr->ops->get_buf(
		vfe_dev->buf_mgr, bufq_handle, &buf);
	if (rc < 0) {
		if (stream_info->stream_type == BURST_STREAM &&
				stream_info->num_burst_capture <= 1) {
			rc = 0;
			if (pingpong_bit)
				buf = stream_info->buf[0];
			else
				buf = stream_info->buf[1];
		} else {
			vfe_dev->error_info.
				stream_framedrop_count[stream_idx]++;
			return rc;
		}
	}

	if (buf->num_planes != stream_info->num_planes) {
		pr_err("%s: Invalid buffer\n", __func__);
		rc = -EINVAL;
		goto buf_error;
	}
	for (i = 0; i < stream_info->num_planes; i++) {
		if (pingpong_bit !=
			(~(pingpong_status >> stream_info->wm[i]) & 0x1)) {
			pr_warn("%s: Write master ping pong mismatch. Status: 0x%x\n",
				__func__, pingpong_status);
		}
	}
	for (i = 0; i < stream_info->num_planes; i++)
		vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
		vfe_dev, stream_info->wm[i],
		pingpong_status, buf->mapped_info[i].paddr);

	if (stream_info->buf[pingpong_bit] && tv) {
		if (stream_info->buf_divert) {
			vfe_dev->buf_mgr->ops->buf_divert(vfe_dev->buf_mgr,
				stream_info->buf[pingpong_bit]->bufq_handle,
				stream_info->buf[pingpong_bit]->buf_idx,
				tv, stream_info->frame_id);

			buf_event.frame_id = stream_info->frame_id;
			buf_event.timestamp = *tv;
			buf_event.u.buf_done.session_id =
				stream_info->session_id;
			buf_event.u.buf_done.stream_id =
				stream_info->stream_id;
			buf_event.u.buf_done.handle =
				stream_info->bufq_handle;
			buf_event.u.buf_done.buf_idx =
			stream_info->buf[pingpong_bit]->buf_idx;
			msm_isp_send_event(
				vfe_dev, ISP_EVENT_BUF_DIVERT, &buf_event);
		} else {
			vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
				stream_info->buf[pingpong_bit]->bufq_handle,
				stream_info->buf[pingpong_bit]->buf_idx,
				tv, stream_info->frame_id);
		}
	}

	stream_info->buf[pingpong_bit] = buf;
	return 0;
buf_error:
	vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
		buf->bufq_handle, buf->buf_idx);
	return rc;
}

enum msm_isp_camif_update_state
	msm_isp_update_camif_output_count(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int i;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint8_t cur_pix_count = axi_data->src_info[VFE_PIX_0].
		pix_stream_count;
	uint8_t cur_raw_count = axi_data->src_info[VFE_PIX_0].
		raw_stream_count;
	uint8_t pix_stream_cnt = 0;
	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		stream_info =
			&axi_data->stream_info[
			(stream_cfg_cmd->stream_handle[i] & 0xFF)];
		if (stream_info->stream_src  != RDI)
			pix_stream_cnt++;
		if (stream_info->stream_src == PIX_ENCODER ||
				stream_info->stream_src == PIX_VIEWFINDER) {
			if (stream_cfg_cmd->cmd == START_STREAM)
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					pix_stream_count++;
			else
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					pix_stream_count--;
		} else if (stream_info->stream_src == CAMIF_RAW ||
				stream_info->stream_src == IDEAL_RAW) {
			if (stream_cfg_cmd->cmd == START_STREAM)
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					raw_stream_count++;
			else
				vfe_dev->axi_data.src_info[VFE_PIX_0].
					raw_stream_count--;
		}
	}
	if (pix_stream_cnt) {
		if ((cur_pix_count + cur_raw_count == 0) &&
				(axi_data->src_info[VFE_PIX_0].
				pix_stream_count +
				axi_data->src_info[VFE_PIX_0].
					raw_stream_count != 0)) {
			return ENABLE_CAMIF;
		}

		if ((cur_pix_count + cur_raw_count != 0) &&
				(axi_data->src_info[VFE_PIX_0].
				pix_stream_count +
				axi_data->src_info[VFE_PIX_0].
					raw_stream_count == 0)) {
			return DISABLE_CAMIF;
		}
	}

	return NO_UPDATE;
}

void msm_camera_io_dump_2(void __iomem *addr, int size)
{
	char line_str[128], *p_str;
	int i;
	u32 *p = (u32 *) addr;
	u32 data;
	ISP_DBG("%s: %p %d\n", __func__, addr, size);
	line_str[0] = '\0';
	p_str = line_str;
	for (i = 0; i < size/4; i++) {
		if (i % 4 == 0) {
			snprintf(p_str, 12, "%08x: ", (u32) p);
			p_str += 10;
		}
		data = readl_relaxed(p++);
		snprintf(p_str, 12, "%08x ", data);
		p_str += 9;
		if ((i + 1) % 4 == 0) {
			ISP_DBG("%s\n", line_str);
			line_str[0] = '\0';
			p_str = line_str;
		}
	}
	if (line_str[0] != '\0')
		ISP_DBG("%s\n", line_str);
}

int msm_isp_cfg_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i;
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd = arg;
	uint32_t wm_reload_mask = 0x0, reg_update_mask = 0x1;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint8_t src_state;
	enum msm_isp_camif_update_state camif_update;
	uint8_t wait_for_complete = 0;
	rc = msm_isp_axi_check_stream_state(vfe_dev, stream_cfg_cmd);
	if (rc < 0) {
		pr_err("%s: Invalid stream state\n", __func__);
		return rc;
	}

	if (axi_data->num_active_stream == 0) {
		/*Configure UB*/
		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_ub(vfe_dev);
	}

	camif_update =
		msm_isp_update_camif_output_count(vfe_dev, stream_cfg_cmd);

	if (camif_update == DISABLE_CAMIF)
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, DISABLE_CAMIF);

	/*
	* Stream start either immediately or at reg update
	* Depends on whether the stream src is active
	* If source is on, start and stop have to be done during reg update
	* If source is off, start can happen immediately or during reg update
	* stop has to be done immediately.
	*/
	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		stream_info =
			&axi_data->stream_info[
				(stream_cfg_cmd->stream_handle[i] & 0xFF)];


		if (stream_info->stream_src == RDI)
			src_state =
				axi_data->src_info[
					stream_info->rdi[0]+1].active;
		else
			src_state = axi_data->src_info[0].active;

		stream_info->state = (stream_cfg_cmd->cmd == START_STREAM) ?
			START_PENDING : STOP_PENDING;

		if (stream_cfg_cmd->cmd == START_STREAM) {
			/*Set address for both PING & PONG register */
			rc = msm_isp_cfg_ping_pong_address(vfe_dev,
				stream_info, VFE_PING_FLAG, NULL);
			rc = msm_isp_cfg_ping_pong_address(vfe_dev,
				stream_info, VFE_PONG_FLAG, NULL);
		}
		if (src_state && camif_update != DISABLE_CAMIF) {
			/*On the fly stream start/stop */
			wait_for_complete = 1;
			reg_update_mask = 0xF; /*TD: Maybe set this per src*/
		} else {
			if (vfe_dev->dump_reg)
				msm_camera_io_dump_2(vfe_dev->vfe_base, 0x900);
			/*Configure AXI start bits to start immediately*/
			msm_isp_axi_stream_enable_cfg(
				vfe_dev, stream_info,
				&wm_reload_mask, &reg_update_mask);
		}
	}
	if (!wait_for_complete) {
		/*Reload AXI*/
		vfe_dev->hw_info->vfe_ops.axi_ops.
			reload_wm(vfe_dev, wm_reload_mask);
		/*Reg update per src*/
		vfe_dev->hw_info->vfe_ops.core_ops.
			reg_update(vfe_dev, reg_update_mask);

		if (camif_update == ENABLE_CAMIF)
			vfe_dev->hw_info->vfe_ops.core_ops.
				update_camif_state(vfe_dev, camif_update);
	} else {
		unsigned long flags;
		spin_lock_irqsave(&vfe_dev->shared_data_lock, flags);
		init_completion(&vfe_dev->stream_config_complete);
		axi_data->stream_update = 1;
		spin_unlock_irqrestore(&vfe_dev->shared_data_lock, flags);
		/*Reload AXI*/
		vfe_dev->hw_info->vfe_ops.axi_ops.
			reload_wm(vfe_dev, wm_reload_mask);
		/*Reg update per src*/
		vfe_dev->hw_info->vfe_ops.core_ops.
			reg_update(vfe_dev, reg_update_mask);
		rc = wait_for_completion_interruptible_timeout(
			&vfe_dev->stream_config_complete,
			msecs_to_jiffies(500));
		if (rc == 0) {
			pr_err("%s: wait timeout\n", __func__);
			rc = -1;
		}
	}
	return rc;
}

int msm_isp_update_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream_update_cmd *update_cmd = arg;
	stream_info = &axi_data->stream_info[
			(update_cmd->stream_handle & 0xFF)];
	if (stream_info->state != ACTIVE && stream_info->state != INACTIVE) {
		pr_err("%s: Invalid stream state\n", __func__);
		return -EINVAL;
	}

	switch (update_cmd->update_type) {
	case ENABLE_STREAM_BUF_DIVERT:
		stream_info->buf_divert = 1;
		break;
	case DISABLE_STREAM_BUF_DIVERT:
		stream_info->buf_divert = 0;
		vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
				stream_info->bufq_handle,
				MSM_ISP_BUFFER_FLUSH_DIVERTED);
		break;
	case UPDATE_STREAM_FRAMEDROP_PATTERN:
		break;
	default:
		pr_err("%s: Invalid update type\n", __func__);
		return -EINVAL;
	}
	return rc;
}

void msm_isp_process_axi_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct timeval *tv)
{
	int i;
	uint32_t comp_mask = 0, wm_mask = 0;
	uint32_t pingpong_status, stream_idx;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_composite_info *comp_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

	ISP_DBG("%s: status: 0x%x\n", __func__, irq_status0);
	comp_mask = vfe_dev->hw_info->vfe_ops.axi_ops.
		get_comp_mask(irq_status0, irq_status1);
	wm_mask = vfe_dev->hw_info->vfe_ops.axi_ops.
		get_wm_mask(irq_status0, irq_status1);
	if (!(comp_mask || wm_mask))
		return;

	pingpong_status =
		vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(vfe_dev);

	for (i = 0; i < axi_data->hw_info->num_comp_mask; i++) {
		comp_info = &axi_data->composite_info[i];
		if (comp_mask & (1 << i)) {
			if (!comp_info->stream_handle) {
				pr_err("%s: Invalid handle for composite irq\n",
					__func__);
			} else {
				stream_idx = comp_info->stream_handle & 0xFF;
				stream_info =
					&axi_data->stream_info[stream_idx];
				ISP_DBG("%s: stream%d frame id: 0x%x\n",
					__func__,
					stream_idx, stream_info->frame_id);
				stream_info->frame_id++;
				msm_isp_cfg_ping_pong_address(vfe_dev,
					stream_info, pingpong_status, tv);
			}
		}
		wm_mask &= ~(comp_info->stream_composite_mask);
	}

	for (i = 0; i < axi_data->hw_info->num_wm; i++) {
		if (wm_mask & (1 << i)) {
			if (!axi_data->free_wm[i]) {
				pr_err("%s: Invalid handle for wm irq\n",
					__func__);
				continue;
			}
			stream_idx = axi_data->free_wm[i] & 0xFF;
			stream_info = &axi_data->stream_info[stream_idx];
			stream_info->frame_id++;
			msm_isp_cfg_ping_pong_address(vfe_dev,
			stream_info, pingpong_status, tv);
		}
	}
	return;
}
