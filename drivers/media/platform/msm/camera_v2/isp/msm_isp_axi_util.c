/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#include "msm_isp_util.h"
#include "msm_isp_axi_util.h"

#define HANDLE_TO_IDX(handle) (handle & 0xFF)
#define ISP_SOF_DEBUG_COUNT 0

static int msm_isp_update_dual_HW_ms_info_at_start(
	struct vfe_device *vfe_dev,
	enum msm_vfe_input_src stream_src);

#define DUAL_VFE_AND_VFE1(s, v) ((s->stream_src < RDI_INTF_0) && \
			v->is_split && vfe_dev->pdev->id == ISP_VFE1)

#define RDI_OR_NOT_DUAL_VFE(v, s) (!v->is_split || \
			((s->stream_src >= RDI_INTF_0) && \
			(stream_info->stream_src <= RDI_INTF_2)))

static inline struct msm_vfe_axi_stream *__dual_vfe_stream(
			struct dual_vfe_resource *d,
			struct msm_vfe_axi_stream *s, int vfe_id)
{
	struct msm_vfe_axi_stream *stream_info = NULL;
	struct msm_vfe_axi_shared_data *axi_data;
	uint32_t handle = 0;

	axi_data = d->axi_data[vfe_id];
	handle = HANDLE_TO_IDX(s->stream_handle);
	if (!axi_data || (handle >= VFE_AXI_SRC_MAX)) {
		pr_err("%s:%d invalid vfe0 axi data handle %d\n",
			__func__, __LINE__,
			handle);
		return NULL;
	}
	stream_info = &axi_data->stream_info[handle];
	return stream_info;
}

int msm_isp_axi_create_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int i = stream_cfg_cmd->stream_src;

	if (i >= VFE_AXI_SRC_MAX) {
		pr_err("%s:%d invalid stream_src %d\n", __func__, __LINE__,
			stream_cfg_cmd->stream_src);
		return -EINVAL;
	}

	if (axi_data->stream_info[i].state != AVAILABLE) {
		pr_err("%s:%d invalid state %d expected %d for src %d\n",
			__func__, __LINE__, axi_data->stream_info[i].state,
			AVAILABLE, i);
		return -EINVAL;
	}

	if ((axi_data->stream_handle_cnt << 8) == 0)
		axi_data->stream_handle_cnt++;

	stream_cfg_cmd->axi_stream_handle =
		(++axi_data->stream_handle_cnt) << 8 | i;

	ISP_DBG(" vfe %d handle %x\n", vfe_dev->pdev->id,
		stream_cfg_cmd->axi_stream_handle);

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
	axi_data->stream_info[i].prev_framedrop_period = 0x7FFFFFFF;
	if (stream_cfg_cmd->controllable_output)
		stream_cfg_cmd->frame_skip_pattern = SKIP_ALL;
	INIT_LIST_HEAD(&axi_data->stream_info[i].request_q);
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
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int rc = -1, i;
	struct msm_vfe_axi_stream *stream_info = NULL;
	if (HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle)
		< VFE_AXI_SRC_MAX) {
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
	case V4L2_PIX_FMT_GREY:
		stream_info->num_planes = 1;
		stream_info->format_factor = ISP_Q2;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV14:
	case V4L2_PIX_FMT_NV41:
		stream_info->num_planes = 2;
		stream_info->format_factor = 1.5 * ISP_Q2;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		stream_info->num_planes = 2;
		stream_info->format_factor = 2 * ISP_Q2;
		break;
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		stream_info->num_planes = 2;
		stream_info->format_factor = 3 * ISP_Q2;
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
	case V4L2_PIX_FMT_GREY:
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
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
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

void msm_isp_axi_reserve_wm(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_shared_data *axi_data,
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
		ISP_DBG("%s vfe %d stream_handle %x wm %d\n", __func__,
			vfe_dev->pdev->id,
			stream_info->stream_handle, j);
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
			VFE_AXI_SRC_MAX) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		spin_lock_irqsave(&stream_info->lock, flags);
		if (stream_info->state != valid_state) {
			if ((stream_info->state == PAUSING ||
				stream_info->state == PAUSED ||
				stream_info->state == RESUME_PENDING ||
				stream_info->state == RESUMING ||
				stream_info->state == UPDATING) &&
				(stream_cfg_cmd->cmd == STOP_STREAM ||
				stream_cfg_cmd->cmd == STOP_IMMEDIATELY)) {
				stream_info->state = ACTIVE;
			} else {
				pr_err("%s: Invalid stream state: %d\n",
					__func__, stream_info->state);
				spin_unlock_irqrestore(
					&stream_info->lock, flags);
				if (stream_cfg_cmd->cmd == START_STREAM)
					rc = -EINVAL;
				break;
			}
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}
	return rc;
}

/**
 * msm_isp_cfg_framedrop_reg() - Program the period and pattern
 * @vfe_dev: The device for which the period and pattern is programmed
 * @stream_info: The stream for which programming is done
 *
 * This function calculates the period and pattern to be configured
 * for the stream based on the current frame id of the stream's input
 * source and the initial framedrops.
 *
 * Returns void.
 */
static void msm_isp_cfg_framedrop_reg(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_stream *vfe0_stream_info = NULL;
	uint32_t runtime_init_frame_drop;

	uint32_t framedrop_pattern = 0;
	uint32_t framedrop_period = MSM_VFE_STREAM_STOP_PERIOD;
	enum msm_vfe_input_src frame_src = SRC_TO_INTF(stream_info->stream_src);

	if (vfe_dev->axi_data.src_info[frame_src].frame_id >=
		stream_info->init_frame_drop)
		runtime_init_frame_drop = 0;
	else
		runtime_init_frame_drop = stream_info->init_frame_drop -
			vfe_dev->axi_data.src_info[frame_src].frame_id;

	if (!runtime_init_frame_drop)
		framedrop_period = stream_info->current_framedrop_period;

	if (MSM_VFE_STREAM_STOP_PERIOD != framedrop_period)
		framedrop_pattern = 0x1;

	ISP_DBG("%s: stream %x framedrop pattern %x period %u\n", __func__,
		stream_info->stream_handle, framedrop_pattern,
		framedrop_period);

	BUG_ON(0 == framedrop_period);
	if (DUAL_VFE_AND_VFE1(stream_info, vfe_dev)) {
		vfe0_stream_info = __dual_vfe_stream(
				vfe_dev->common_data->dual_vfe_res,
				stream_info, ISP_VFE0);
		if (vfe0_stream_info) {
			vfe_dev->hw_info->vfe_ops.axi_ops.cfg_framedrop(
				vfe_dev->common_data->dual_vfe_res->
				vfe_base[ISP_VFE0],
				vfe0_stream_info, framedrop_pattern,
				framedrop_period);
			vfe_dev->hw_info->vfe_ops.axi_ops.cfg_framedrop(
				vfe_dev->vfe_base, stream_info,
				framedrop_pattern,
				framedrop_period);

			stream_info->prev_framedrop_period =
				(framedrop_period | 0x80000000);
			vfe0_stream_info->prev_framedrop_period =
				(framedrop_period | 0x80000000);
		}
	} else if (RDI_OR_NOT_DUAL_VFE(vfe_dev, stream_info)) {
		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_framedrop(
			vfe_dev->vfe_base, stream_info, framedrop_pattern,
			framedrop_period);
		stream_info->prev_framedrop_period =
				(framedrop_period | 0x80000000);
	}
}

/**
 * msm_isp_update_framedrop_reg() - Update frame period pattern on h/w
 * @vfe_dev: The h/w on which the perion pattern is updated.
 * @frame_src: Input source.
 *
 * If the period and pattern needs to be updated for a stream then it is
 * updated here. Updates happen if initial frame drop reaches 0 or burst
 * streams have been provided new skip pattern from user space.
 *
 * Returns void
 */
void msm_isp_update_framedrop_reg(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;
	unsigned long flags;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		if (SRC_TO_INTF(axi_data->stream_info[i].stream_src) !=
			frame_src) {
			continue;
		}
		stream_info = &axi_data->stream_info[i];
		if (stream_info->state != ACTIVE)
			continue;

		spin_lock_irqsave(&stream_info->lock, flags);

		if (BURST_STREAM == stream_info->stream_type) {
			if (0 == stream_info->runtime_num_burst_capture)
				stream_info->current_framedrop_period =
				MSM_VFE_STREAM_STOP_PERIOD;
		}

		/*
		 * re-configure the period pattern, only if it's not already
		 * set to what we want
		 */
		if (stream_info->current_framedrop_period !=
			stream_info->prev_framedrop_period) {
			/*
			 * If we previously tried to set a valid period which
			 * did not take effect then we may have missed a reg
			 * update, print error to indicate this condition
			 */
			if ((stream_info->prev_framedrop_period & 0x80000000) &&
				(stream_info->current_framedrop_period ==
				(stream_info->prev_framedrop_period &
					~0x80000000)))
				ISP_DBG("Framedop setting for %p not taken effect %x/%x, frame_src %x\n",
					stream_info,
					stream_info->prev_framedrop_period,
					stream_info->current_framedrop_period,
					frame_src);

			msm_isp_cfg_framedrop_reg(vfe_dev, stream_info);
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}
}

/**
 * msm_isp_reset_framedrop() - Compute the framedrop period pattern
 * @vfe_dev: Device for which the period and pattern is computed
 * @stream_info: The stream for the which period and pattern is generated
 *
 * This function is called when stream starts or is reset. It's main
 * purpose is to setup the runtime parameters of framedrop required
 * for the stream.
 *
 * Returms void
 */
void msm_isp_reset_framedrop(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	stream_info->runtime_num_burst_capture = stream_info->num_burst_capture;

	/**
	 *  only reset none controllable output stream, since the
	 *  controllable stream framedrop period will be controlled
	 *  by the request frame api
	 */
	if (!stream_info->controllable_output) {
		stream_info->current_framedrop_period =
			msm_isp_get_framedrop_period(
			stream_info->frame_skip_pattern);
	}

	msm_isp_cfg_framedrop_reg(vfe_dev, stream_info);
	ISP_DBG("%s: init frame drop: %d\n", __func__,
		stream_info->init_frame_drop);
	ISP_DBG("%s: num_burst_capture: %d\n", __func__,
		stream_info->runtime_num_burst_capture);
}

void msm_isp_check_for_output_error(struct vfe_device *vfe_dev,
	struct msm_isp_timestamp *ts, struct msm_isp_sof_info *sof_info)
{
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data;
	int i;

	if (!vfe_dev || !sof_info) {
		pr_err("%s %d failed: vfe_dev %p sof_info %p\n", __func__,
			__LINE__, vfe_dev, sof_info);
		return;
	}
	sof_info->regs_not_updated = 0;
	sof_info->reg_update_fail_mask = 0;
	sof_info->stream_get_buf_fail_mask = 0;

	axi_data = &vfe_dev->axi_data;
	/* report that registers are not updated and return empty buffer for
	 * controllable outputs
	 */
	if (!vfe_dev->reg_updated) {
		sof_info->regs_not_updated =
			vfe_dev->reg_update_requested;
		for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
			stream_info = &axi_data->stream_info[i];
			if (stream_info->state != ACTIVE ||
				!stream_info->controllable_output ||
				(SRC_TO_INTF(stream_info->stream_src) !=
				VFE_PIX_0))
				continue;

			if (stream_info->undelivered_request_cnt) {
				if (msm_isp_drop_frame(vfe_dev, stream_info, ts,
					sof_info)) {
					pr_err("drop frame failed\n");

				}
			}
		}
	}
	vfe_dev->reg_updated = 0;

	/* report frame drop per stream */
	if (vfe_dev->error_info.framedrop_flag) {
		for (i = 0; i < BUF_MGR_NUM_BUF_Q; i++) {
			if (vfe_dev->error_info.stream_framedrop_count[i]) {
				ISP_DBG("%s: get buf failed i %d\n", __func__,
					i);
				sof_info->stream_get_buf_fail_mask |= (1 << i);
				vfe_dev->error_info.
					stream_framedrop_count[i] = 0;
			}
		}
		vfe_dev->error_info.framedrop_flag = 0;
	}
}

void msm_isp_increment_frame_id(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts)
{
	struct msm_vfe_src_info *src_info = NULL;
	struct msm_vfe_sof_info *sof_info = NULL;
	enum msm_vfe_dual_hw_type dual_hw_type;
	enum msm_vfe_dual_hw_ms_type ms_type;
	struct msm_vfe_sof_info *master_sof_info = NULL;
	int32_t time, master_time, delta;
	uint32_t sof_incr = 0;
	unsigned long flags;

	if (vfe_dev->axi_data.src_info[frame_src].frame_id == 0)
		msm_isp_update_dual_HW_ms_info_at_start(vfe_dev, frame_src);

	spin_lock_irqsave(&vfe_dev->common_data->common_dev_data_lock, flags);
	dual_hw_type =
		vfe_dev->axi_data.src_info[frame_src].dual_hw_type;
	ms_type =
		vfe_dev->axi_data.src_info[frame_src].
		dual_hw_ms_info.dual_hw_ms_type;
	/*
	 * Increment frame_id if
	 *   1. Not Master Slave
	 *   2. Master
	 *   3. Slave and Master is Inactive
	 *
	 *   OR
	 * (in other words)
	 * If SLAVE and Master active, don't increment slave frame_id.
	 * Instead use Master frame_id for Slave.
	 */
	if ((dual_hw_type == DUAL_HW_MASTER_SLAVE) &&
		(ms_type == MS_TYPE_SLAVE) &&
		(vfe_dev->common_data->ms_resource.master_active == 1)) {
		/* DUAL_HW_MS_SLAVE  && MASTER active */
		time = ts->buf_time.tv_sec * 1000 +
			ts->buf_time.tv_usec / 1000;
		master_sof_info = &vfe_dev->common_data->ms_resource.
			master_sof_info;
		master_time = master_sof_info->mono_timestamp_ms;
		delta = vfe_dev->common_data->ms_resource.sof_delta_threshold;
		ISP_DBG("%s: vfe %d frame %d Slave time %d Master time %d delta %d\n",
			__func__, vfe_dev->pdev->id,
			vfe_dev->axi_data.src_info[frame_src].frame_id,
			time, master_time, time - master_time);

		if (time - master_time > delta)
			sof_incr = 1;

		/*
		 * If delta < 5ms, slave frame_id = master frame_id
		 * If delta > 5ms, slave frame_id = master frame_id + 1
		 * CANNOT support Batch Mode with this logic currently.
		 */
		vfe_dev->axi_data.src_info[frame_src].frame_id =
			master_sof_info->frame_id + sof_incr;
	} else {
		if (frame_src == VFE_PIX_0) {
			vfe_dev->axi_data.src_info[frame_src].frame_id +=
				vfe_dev->axi_data.src_info[frame_src].
				sof_counter_step;
			src_info = &vfe_dev->axi_data.src_info[frame_src];

			if (!src_info->frame_id &&
				!src_info->reg_update_frame_id &&
				((src_info->frame_id -
				src_info->reg_update_frame_id) >
				(MAX_REG_UPDATE_THRESHOLD *
				src_info->sof_counter_step))) {
				pr_err("%s:%d reg_update not received for %d frames\n",
					__func__, __LINE__,
					src_info->frame_id -
					src_info->reg_update_frame_id);

				msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_REG_UPDATE_MISSING);
			}

		} else
			vfe_dev->axi_data.src_info[frame_src].frame_id++;
	}

	sof_info = vfe_dev->axi_data.src_info[frame_src].
		dual_hw_ms_info.sof_info;
	if (dual_hw_type == DUAL_HW_MASTER_SLAVE &&
		sof_info != NULL) {
		sof_info->frame_id = vfe_dev->axi_data.src_info[frame_src].
			frame_id;
		sof_info->timestamp_ms = ts->event_time.tv_sec * 1000 +
			ts->event_time.tv_usec / 1000;
		sof_info->mono_timestamp_ms = ts->buf_time.tv_sec * 1000 +
			ts->buf_time.tv_usec / 1000;
	}
	spin_unlock_irqrestore(&vfe_dev->common_data->common_dev_data_lock,
		flags);
}

void msm_isp_notify(struct vfe_device *vfe_dev, uint32_t event_type,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts)
{
	struct msm_isp_event_data event_data;
	struct msm_vfe_sof_info *sof_info = NULL, *self_sof = NULL;
	enum msm_vfe_dual_hw_ms_type ms_type;
	int i, j;

	memset(&event_data, 0, sizeof(event_data));

	switch (event_type) {
	case ISP_EVENT_SOF:
		if (frame_src == VFE_PIX_0) {
			/* Frame id is incremented in CAMIF SOF. Event is sent
			 * in EPOCH. If by this time, both VFE dont have same
			 * frame_id, then we have scheduling issues or some
			 * other s/w issue */
			if (vfe_dev->is_split &&
				vfe_dev->common_data->dual_vfe_res->
					axi_data[0]->
					src_info[VFE_PIX_0].frame_id !=
				vfe_dev->common_data->dual_vfe_res->
					axi_data[1]->
					src_info[VFE_PIX_0].frame_id) {
				pr_err_ratelimited("%s: Error! 2 VFE out of sync vfe0 frame_id %u vfe1 %u\n",
					__func__,
					vfe_dev->common_data->dual_vfe_res->
						axi_data[0]->
						src_info[VFE_PIX_0].frame_id,
					vfe_dev->common_data->dual_vfe_res->
						axi_data[1]->
						src_info[VFE_PIX_0].frame_id);
			}
			if (vfe_dev->isp_sof_debug < ISP_SOF_DEBUG_COUNT)
				pr_err("%s: PIX0 frame id: %u\n", __func__,
				vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
			vfe_dev->isp_sof_debug++;
		} else if (frame_src == VFE_RAW_0) {
			if (vfe_dev->isp_raw0_debug < ISP_SOF_DEBUG_COUNT)
				pr_err("%s: RAW_0 frame id: %u\n", __func__,
				vfe_dev->axi_data.src_info[VFE_RAW_0].frame_id);
			vfe_dev->isp_raw0_debug++;
		} else if (frame_src == VFE_RAW_1) {
			if (vfe_dev->isp_raw1_debug < ISP_SOF_DEBUG_COUNT)
				pr_err("%s: RAW_1 frame id: %u\n", __func__,
				vfe_dev->axi_data.src_info[VFE_RAW_1].frame_id);
			vfe_dev->isp_raw1_debug++;
		} else if (frame_src == VFE_RAW_2) {
			if (vfe_dev->isp_raw2_debug < ISP_SOF_DEBUG_COUNT)
				pr_err("%s: RAW_2 frame id: %u\n", __func__,
				vfe_dev->axi_data.src_info[VFE_RAW_2].frame_id);
			vfe_dev->isp_raw2_debug++;
		}

		ISP_DBG("%s: vfe %d frame_src %d frame id: %u\n", __func__,
			vfe_dev->pdev->id, frame_src,
			vfe_dev->axi_data.src_info[frame_src].frame_id);

		/*
		 * Cannot support dual_cam and framedrop same time in union.
		 * If need to support framedrop as well, move delta calculation
		 * to userspace
		 */
		if (vfe_dev->axi_data.src_info[frame_src].dual_hw_type ==
			DUAL_HW_MASTER_SLAVE) {
			spin_lock(&vfe_dev->common_data->common_dev_data_lock);
			self_sof = vfe_dev->axi_data.src_info[frame_src].
				dual_hw_ms_info.sof_info;
			if (!self_sof) {
				spin_unlock(&vfe_dev->common_data->
					common_dev_data_lock);
				break;
			}
			ms_type = vfe_dev->axi_data.src_info[frame_src].
				dual_hw_ms_info.dual_hw_ms_type;
			if (ms_type == MS_TYPE_MASTER) {
				for (i = 0, j = 0; i < MS_NUM_SLAVE_MAX; i++) {
					if (!(vfe_dev->common_data->
						ms_resource.slave_active_mask
						& (1 << i)))
						continue;
					sof_info = &vfe_dev->common_data->
						ms_resource.slave_sof_info[i];
					event_data.u.sof_info.ms_delta_info.
						delta[j] =
						self_sof->mono_timestamp_ms -
						sof_info->mono_timestamp_ms;
					j++;
					if (j == vfe_dev->common_data->
						ms_resource.num_slave)
						break;
				}
				event_data.u.sof_info.ms_delta_info.
					num_delta_info = j;
			} else {
				sof_info = &vfe_dev->common_data->ms_resource.
					master_sof_info;
				event_data.u.sof_info.ms_delta_info.
					num_delta_info = 1;
				event_data.u.sof_info.ms_delta_info.delta[0] =
					self_sof->mono_timestamp_ms -
					sof_info->mono_timestamp_ms;
			}
			spin_unlock(&vfe_dev->common_data->
				common_dev_data_lock);
		} else
			if (frame_src == VFE_PIX_0) {
				msm_isp_check_for_output_error(vfe_dev, ts,
					&event_data.u.sof_info);
			}
		break;

	default:
		break;
	}

	event_data.frame_id = vfe_dev->axi_data.src_info[frame_src].frame_id;
	event_data.timestamp = ts->event_time;
	event_data.mono_timestamp = ts->buf_time;
	msm_isp_send_event(vfe_dev, event_type | frame_src, &event_data);
}

/**
 * msm_isp_calculate_framedrop() - Setup frame period and pattern
 * @axi_data: Structure describing the h/w streams.
 * @stream_cfg_cmd: User space input parameter for perion/pattern.
 *
 * Initialize the h/w stream framedrop period and pattern sent
 * by user space.
 *
 * Returns 0 on success else error code.
 */
int msm_isp_calculate_framedrop(
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	uint32_t framedrop_period = 0;
	struct msm_vfe_axi_stream *stream_info = NULL;
	if (HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle)
		< VFE_AXI_SRC_MAX) {
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle)];
	} else {
		pr_err("%s: Invalid stream handle", __func__);
		return -EINVAL;
	}
	if (!stream_info) {
		pr_err("%s: Stream info is NULL\n", __func__);
		return -EINVAL;
	}

	framedrop_period = msm_isp_get_framedrop_period(
	   stream_cfg_cmd->frame_skip_pattern);
	stream_info->frame_skip_pattern =
		stream_cfg_cmd->frame_skip_pattern;
	if (stream_cfg_cmd->frame_skip_pattern == SKIP_ALL)
		stream_info->current_framedrop_period =
			MSM_VFE_STREAM_STOP_PERIOD;
	else
		stream_info->current_framedrop_period = framedrop_period;

	stream_info->init_frame_drop = stream_cfg_cmd->init_frame_drop;

	if (stream_cfg_cmd->burst_count > 0) {
		stream_info->stream_type = BURST_STREAM;
		stream_info->num_burst_capture =
			stream_cfg_cmd->burst_count;
	} else {
		stream_info->stream_type = CONTINUOUS_STREAM;
	}
	return 0;
}

void msm_isp_calculate_bandwidth(
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

static inline void msm_isp_get_avtimer_ts(
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
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd = arg;
	struct msm_vfe_axi_stream *stream_info;

	rc = msm_isp_axi_create_stream(vfe_dev,
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
			VFE_AXI_SRC_MAX)
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
	vfe_dev->reg_update_requested &=
		~(BIT(SRC_TO_INTF(stream_info->stream_src)));

	msm_isp_axi_reserve_wm(vfe_dev, &vfe_dev->axi_data, stream_info);

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
			goto done;
		}
	}
	rc = msm_isp_calculate_framedrop(&vfe_dev->axi_data, stream_cfg_cmd);
	if (rc)
		goto done;
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
	/* initialize the WM ping pong with scratch buffer */
	msm_isp_cfg_stream_scratch(vfe_dev, stream_info, VFE_PING_FLAG);
	msm_isp_cfg_stream_scratch(vfe_dev, stream_info, VFE_PONG_FLAG);

done:
	if (rc) {
		msm_isp_axi_free_wm(&vfe_dev->axi_data, stream_info);
		msm_isp_axi_destroy_stream(&vfe_dev->axi_data,
			HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle));
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
		VFE_AXI_SRC_MAX) {
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

static int  msm_isp_axi_stream_enable_cfg(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, int32_t dual_vfe_sync)
{
	int i, vfe_id = 0, enable_wm = 0;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t stream_idx = HANDLE_TO_IDX(stream_info->stream_handle);
	struct dual_vfe_resource *dual_vfe_res = NULL;

	if (stream_idx >= MAX_NUM_STREAM) {
		pr_err("%s: Invalid stream_idx", __func__);
		goto error;
	}

	if (stream_info->state == INACTIVE)
		goto error;

	if (stream_info->state == START_PENDING ||
		stream_info->state == RESUME_PENDING) {
		enable_wm = 1;
	} else {
		enable_wm = 0;
	}
	for (i = 0; i < stream_info->num_planes; i++) {
		/*
		 * In case when sensor is streaming, use dual vfe sync mode
		 * to enable wm together and avoid split.
		 */
		if ((stream_info->stream_src < RDI_INTF_0) &&
			vfe_dev->is_split && vfe_dev->pdev->id == ISP_VFE1 &&
			dual_vfe_sync) {
			dual_vfe_res = vfe_dev->common_data->dual_vfe_res;
			if (!dual_vfe_res->vfe_base[ISP_VFE0] ||
				!dual_vfe_res->axi_data[ISP_VFE0] ||
				!dual_vfe_res->vfe_base[ISP_VFE1] ||
				!dual_vfe_res->axi_data[ISP_VFE1]) {
				pr_err("%s:%d failed vfe0 %p %p vfe %p %p\n",
					__func__, __LINE__,
					dual_vfe_res->vfe_base[ISP_VFE0],
					dual_vfe_res->axi_data[ISP_VFE0],
					dual_vfe_res->vfe_base[ISP_VFE1],
					dual_vfe_res->axi_data[ISP_VFE1]);
				goto error;
			}
			for (vfe_id = 0; vfe_id < MAX_VFE; vfe_id++) {
				vfe_dev->hw_info->vfe_ops.axi_ops.
				enable_wm(dual_vfe_res->vfe_base[vfe_id],
					dual_vfe_res->axi_data[vfe_id]->
					stream_info[stream_idx].wm[i],
					enable_wm);
			}
		} else if (!vfe_dev->is_split ||
			(stream_info->stream_src >= RDI_INTF_0 &&
			stream_info->stream_src <= RDI_INTF_2) ||
			!dual_vfe_sync) {
			vfe_dev->hw_info->vfe_ops.axi_ops.
			enable_wm(vfe_dev->vfe_base, stream_info->wm[i],
					enable_wm);
		}
		if (!enable_wm) {
			/* Issue a reg update for Raw Snapshot Case
			 * since we dont have reg update ack
			*/
			if (vfe_dev->axi_data.src_info[VFE_PIX_0].
				raw_stream_count > 0
				&& vfe_dev->axi_data.src_info[VFE_PIX_0].
				pix_stream_count == 0) {
				if (stream_info->stream_src == CAMIF_RAW ||
					stream_info->stream_src == IDEAL_RAW) {
					vfe_dev->hw_info->vfe_ops.core_ops.
						reg_update(vfe_dev,
						VFE_PIX_0);
				}
			}
		}
	}
	if (stream_info->state == START_PENDING)
		axi_data->num_active_stream++;
	else if (stream_info->state == STOP_PENDING)
		axi_data->num_active_stream--;
	return 0;
error:
	return -EINVAL;
}

void msm_isp_axi_stream_update(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src)
{
	int i;
	unsigned long flags;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		if (SRC_TO_INTF(axi_data->stream_info[i].stream_src) !=
			frame_src) {
			ISP_DBG("%s stream_src %d frame_src %d\n", __func__,
				SRC_TO_INTF(
				axi_data->stream_info[i].stream_src),
				frame_src);
			continue;
		}
		if (axi_data->stream_info[i].state == UPDATING)
			axi_data->stream_info[i].state = ACTIVE;
		else if (axi_data->stream_info[i].state == START_PENDING ||
			axi_data->stream_info[i].state == STOP_PENDING) {
			msm_isp_axi_stream_enable_cfg(
				vfe_dev, &axi_data->stream_info[i], 1);
			axi_data->stream_info[i].state =
				axi_data->stream_info[i].state ==
				START_PENDING ? STARTING : STOPPING;
		} else if (axi_data->stream_info[i].state == STARTING ||
			axi_data->stream_info[i].state == STOPPING) {
			axi_data->stream_info[i].state =
				axi_data->stream_info[i].state == STARTING ?
				ACTIVE : INACTIVE;
		}
	}

	spin_lock_irqsave(&vfe_dev->shared_data_lock, flags);
	if (vfe_dev->axi_data.stream_update[frame_src]) {
		vfe_dev->axi_data.stream_update[frame_src]--;
	}
	spin_unlock_irqrestore(&vfe_dev->shared_data_lock, flags);

	if (vfe_dev->axi_data.pipeline_update == DISABLE_CAMIF ||
		(vfe_dev->axi_data.pipeline_update ==
		DISABLE_CAMIF_IMMEDIATELY)) {
		vfe_dev->hw_info->vfe_ops.stats_ops.
			enable_module(vfe_dev, 0xFF, 0);
		vfe_dev->axi_data.pipeline_update = NO_UPDATE;
	}

	if (vfe_dev->axi_data.stream_update[frame_src] == 0)
		complete(&vfe_dev->stream_config_complete);
}

static void msm_isp_reload_ping_pong_offset(struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info)
{
	int i, j;
	uint32_t bit;
	struct msm_isp_buffer *buf;
	int32_t buf_size_byte = 0;
	int32_t word_per_line = 0;

	for (i = 0; i < 2; i++) {
		buf = stream_info->buf[i];
		if (!buf)
			continue;

		bit = i ? 0 : 1;

		for (j = 0; j < stream_info->num_planes; j++) {
			word_per_line = msm_isp_cal_word_per_line(
				stream_info->output_format, stream_info->
				plane_cfg[j].output_stride);
			if (word_per_line < 0) {
				/* 0 means no prefetch*/
				word_per_line = 0;
				buf_size_byte = 0;
			} else {
				buf_size_byte = (word_per_line * 8 *
					stream_info->plane_cfg[j].
					output_scan_lines) - stream_info->
					plane_cfg[j].plane_addr_offset;
			}

			vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
				vfe_dev->vfe_base, stream_info->wm[j], bit,
				buf->mapped_info[j].paddr +
				stream_info->plane_cfg[j].plane_addr_offset,
				buf_size_byte);
		}
	}
}

void msm_isp_axi_cfg_update(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src)
{
	int i, j;
	uint32_t update_state;
	unsigned long flags;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream *stream_info;
	int num_stream = 0;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		if (SRC_TO_INTF(axi_data->stream_info[i].stream_src) !=
			frame_src) {
			continue;
		}
		num_stream++;
		stream_info = &axi_data->stream_info[i];
		if ((stream_info->stream_type == BURST_STREAM &&
			!stream_info->controllable_output) ||
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
				vfe_dev, &axi_data->stream_info[i], 1);
			stream_info->state = RESUMING;
		} else if (stream_info->state == RESUMING) {
			stream_info->runtime_output_format =
				stream_info->output_format;
			stream_info->state = ACTIVE;
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	if (num_stream)
		update_state = atomic_dec_return(
			&axi_data->axi_cfg_update[frame_src]);
}

static int msm_isp_get_done_buf(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	struct msm_isp_buffer **done_buf)
{
	uint32_t pingpong_bit = 0, i;
	int rc = 0;

	pingpong_bit = (~(pingpong_status >> stream_info->wm[0]) & 0x1);
	for (i = 0; i < stream_info->num_planes; i++) {
		if (pingpong_bit !=
			(~(pingpong_status >> stream_info->wm[i]) & 0x1)) {
			pr_debug("%s: Write master ping pong mismatch. Status: 0x%x\n",
				__func__, pingpong_status);
		}
	}

	*done_buf = stream_info->buf[pingpong_bit];
	/* For null buffer there is nothing to do */
	if (NULL == *done_buf) {
		ISP_DBG("%s vfe %d done buf is null\n", __func__,
			vfe_dev->pdev->id);
		return rc;
	}
	ISP_DBG("%s vfe %d pingpong %d buf %d bufq %x\n", __func__,
		vfe_dev->pdev->id, pingpong_bit,
		(*done_buf)->buf_idx, (*done_buf)->bufq_handle);

	/*
	 * Put scratch buffer if there was a valid buffer to avoid getting
	 * pagefaults in case we do not want another frame from the ping/pong
	 * position when current done_buf is obtained from
	 */
	msm_isp_cfg_stream_scratch(vfe_dev, stream_info, pingpong_status);

	if (stream_info->controllable_output) {
		stream_info->buf[pingpong_bit] = NULL;
		if (!stream_info->undelivered_request_cnt) {
			pr_err_ratelimited("%s:%d error undelivered_request_cnt 0\n",
				__func__, __LINE__);
		} else {
			stream_info->undelivered_request_cnt--;
			if (pingpong_bit != stream_info->sw_ping_pong_bit) {
				pr_err("%s:%d ping pong bit actual %d sw %d\n",
					__func__, __LINE__, pingpong_bit,
					stream_info->sw_ping_pong_bit);

				rc = -EINVAL;
			}
			stream_info->sw_ping_pong_bit ^= 1;
		}
	}

	return rc;
}

void msm_isp_halt_send_error(struct vfe_device *vfe_dev, uint32_t event)
{
	uint32_t i = 0;
	struct msm_isp_event_data error_event;
	struct msm_vfe_axi_halt_cmd halt_cmd;

	memset(&halt_cmd, 0, sizeof(struct msm_vfe_axi_halt_cmd));
	memset(&error_event, 0, sizeof(struct msm_isp_event_data));
	halt_cmd.stop_camif = 1;
	halt_cmd.overflow_detected = 0;
	halt_cmd.blocking_halt = 0;

	pr_err("%s: vfe%d fatal error!\n", __func__, vfe_dev->pdev->id);

	atomic_set(&vfe_dev->error_info.overflow_state,
		HALT_ENFORCED);

	/* heavy spin lock in axi halt, avoid spin lock outside. */
	msm_isp_axi_halt(vfe_dev, &halt_cmd);

	for (i = 0; i < VFE_AXI_SRC_MAX; i++)
		vfe_dev->axi_data.stream_info[i].state =
			INACTIVE;

	error_event.frame_id =
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;

	msm_isp_send_event(vfe_dev, event, &error_event);
}

int msm_isp_print_ping_pong_address(struct vfe_device *vfe_dev,
	unsigned long fault_addr)
{
	int i, j;
	struct msm_isp_buffer *buf = NULL;
	uint32_t pingpong_bit;
	struct msm_vfe_axi_stream *stream_info = NULL;

	for (j = 0; j < VFE_AXI_SRC_MAX; j++) {
		stream_info = &vfe_dev->axi_data.stream_info[j];
		if (stream_info->state == INACTIVE)
			continue;

		for (pingpong_bit = 0; pingpong_bit < 2; pingpong_bit++) {
			for (i = 0; i < stream_info->num_planes; i++) {
				buf = stream_info->buf[pingpong_bit];
				if (buf == NULL) {
					pr_err("%s: buf NULL\n", __func__);
					continue;
				}
				pr_debug("%s: stream_id %x ping-pong %d	plane %d start_addr %lu	addr_offset %x len %zx stride %d scanline %d\n"
					, __func__, stream_info->stream_id,
					pingpong_bit, i, (unsigned long)
					buf->mapped_info[i].paddr,
					stream_info->
						plane_cfg[i].plane_addr_offset,
					buf->mapped_info[i].len,
					stream_info->
						plane_cfg[i].output_stride,
					stream_info->
						plane_cfg[i].output_scan_lines
					);
			}
		}
	}

	return 0;
}

static int msm_isp_cfg_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	uint8_t sync_dual_vfe)
{
	int i, rc = -1;
	struct msm_isp_buffer *buf = NULL;
	uint32_t bufq_handle = 0;
	struct msm_vfe_frame_request_queue *queue_req;
	uint32_t pingpong_bit;
	uint32_t buf_cnt = 0;
	uint32_t stream_idx = HANDLE_TO_IDX(stream_info->stream_handle);
	uint32_t buffer_size_byte = 0;
	int32_t word_per_line = 0;
	dma_addr_t paddr;
	struct dual_vfe_resource *dual_vfe_res = NULL;
	uint32_t vfe_id = 0;
	unsigned long flags;

	if (stream_idx >= VFE_AXI_SRC_MAX) {
		pr_err("%s: Invalid stream_idx", __func__);
		return rc;
	}

	if (!stream_info->controllable_output) {
		bufq_handle = stream_info->bufq_handle[VFE_BUF_QUEUE_DEFAULT];
	} else {
		queue_req = list_first_entry_or_null(&stream_info->request_q,
			struct msm_vfe_frame_request_queue, list);
		if (!queue_req)
			return 0;

		bufq_handle = stream_info->
			bufq_handle[queue_req->buff_queue_id];

		if (!bufq_handle || stream_info->request_q_cnt <= 0) {
			pr_err_ratelimited("%s: Drop request. Shared stream is stopped.\n",
				__func__);
			return -EINVAL;
		}
		queue_req->cmd_used = 0;
		list_del(&queue_req->list);
		stream_info->request_q_cnt--;
	}

	rc = vfe_dev->buf_mgr->ops->get_buf(vfe_dev->buf_mgr,
		vfe_dev->pdev->id, bufq_handle, &buf, &buf_cnt);

	if (rc < 0) {
		ISP_DBG("%s vfe %d get_buf fail bufq %x\n", __func__,
			vfe_dev->pdev->id, bufq_handle);
		vfe_dev->error_info.
			stream_framedrop_count[bufq_handle & 0xFF]++;
		vfe_dev->error_info.framedrop_flag = 1;
		return rc;
	}

	if (buf->num_planes != stream_info->num_planes) {
		pr_err("%s: Invalid buffer\n", __func__);
		rc = -EINVAL;
		goto buf_error;
	}

	/* make sure that streams are in right state */
	if ((stream_info->stream_src < RDI_INTF_0) &&
		vfe_dev->is_split &&
		(buf_cnt >= MAX_VFE) && sync_dual_vfe) {
		dual_vfe_res = vfe_dev->common_data->dual_vfe_res;
		if (!dual_vfe_res->vfe_base[ISP_VFE0] ||
			!dual_vfe_res->axi_data[ISP_VFE0] ||
			!dual_vfe_res->vfe_base[ISP_VFE1] ||
			!dual_vfe_res->axi_data[ISP_VFE1]) {
			pr_err("%s:%d failed vfe0 %p %p vfe %p %p\n",
				__func__, __LINE__,
				dual_vfe_res->vfe_base[ISP_VFE0],
				dual_vfe_res->axi_data[ISP_VFE0],
				dual_vfe_res->vfe_base[ISP_VFE1],
				dual_vfe_res->axi_data[ISP_VFE1]);
			rc = -EINVAL;
			goto buf_error;
		}
	} else if (!sync_dual_vfe || !vfe_dev->is_split ||
			(stream_info->stream_src >= RDI_INTF_0 &&
			stream_info->stream_src <= RDI_INTF_2)) {
		dual_vfe_res = NULL;
	} else {
		pingpong_bit = (~(pingpong_status >> stream_info->wm[0]) & 0x1);
		stream_info->buf[pingpong_bit] = buf;
		return 0;
	}

	for (i = 0; i < stream_info->num_planes; i++) {
		word_per_line = msm_isp_cal_word_per_line(
			stream_info->output_format, stream_info->
			plane_cfg[i].output_stride);
		if (word_per_line < 0) {
			/* 0 means no prefetch*/
			word_per_line = 0;
			buffer_size_byte = 0;
		} else {
			buffer_size_byte = (word_per_line * 8 *
				stream_info->plane_cfg[i].
				output_scan_lines) - stream_info->
				plane_cfg[i].plane_addr_offset;
		}

		paddr = buf->mapped_info[i].paddr;
		/* Isolate pingpong_bit from pingpong_status */
		pingpong_bit = ((pingpong_status >>
			stream_info->wm[i]) & 0x1);
		ISP_DBG("%s: vfe %d config buf %d to pingpong %d stream %x\n",
			__func__, vfe_dev->pdev->id,
			buf->buf_idx, !pingpong_bit,
			bufq_handle);

		if (dual_vfe_res) {
			for (vfe_id = 0; vfe_id < MAX_VFE; vfe_id++) {
				if (vfe_id != vfe_dev->pdev->id)
					spin_lock_irqsave(
						&dual_vfe_res->
						axi_data[vfe_id]->
						stream_info[stream_idx].
						lock, flags);

				vfe_dev->hw_info->vfe_ops.axi_ops.
					update_ping_pong_addr(
					dual_vfe_res->vfe_base[vfe_id],
					dual_vfe_res->axi_data[vfe_id]->
					stream_info[stream_idx].wm[i],
					pingpong_bit, paddr +
					dual_vfe_res->axi_data[vfe_id]->
					stream_info[stream_idx].
					plane_cfg[i].plane_addr_offset,
					buffer_size_byte);

				if (i == 0) {
					dual_vfe_res->axi_data[vfe_id]->
						stream_info[stream_idx].
						buf[!pingpong_bit] =
						buf;
				}
				if (vfe_id != vfe_dev->pdev->id)
					spin_unlock_irqrestore(
						&dual_vfe_res->
						axi_data[vfe_id]->
						stream_info[stream_idx].
						lock, flags);
			}
		} else {
			vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
				vfe_dev->vfe_base, stream_info->wm[i],
				pingpong_bit, paddr +
				stream_info->plane_cfg[i].plane_addr_offset,
				buffer_size_byte);
			stream_info->buf[!pingpong_bit] =
				buf;
		}
	}

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
	unsigned long flags;
	struct msm_isp_event_data buf_event;
	struct timeval *time_stamp;
	uint32_t stream_idx = HANDLE_TO_IDX(stream_info->stream_handle);
	uint32_t frame_id;
	uint32_t buf_src;
	uint8_t drop_frame = 0;
	struct msm_isp_bufq *bufq = NULL;
	memset(&buf_event, 0, sizeof(buf_event));

	frame_id = vfe_dev->axi_data.
		src_info[SRC_TO_INTF(stream_info->stream_src)].frame_id;

	if (stream_idx >= VFE_AXI_SRC_MAX) {
		pr_err_ratelimited("%s: Invalid stream_idx", __func__);
		return;
	}

	if (SRC_TO_INTF(stream_info->stream_src) >= VFE_SRC_MAX) {
		pr_err_ratelimited("%s: Invalid stream index, put buf back to vb2 queue\n",
			__func__);
		rc = vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx);
		return;
	}

	if (stream_info->stream_type != BURST_STREAM &&
		(stream_info->sw_skip.stream_src_mask &
		(1 << stream_info->stream_src))) {
		/* Hw stream output of this src is requested for drop */
		if (stream_info->sw_skip.skip_mode == SKIP_ALL) {
			/* drop all buffers */
			drop_frame = 1;
		} else if (stream_info->sw_skip.skip_mode == SKIP_RANGE &&
			(stream_info->sw_skip.min_frame_id <= frame_id &&
			stream_info->sw_skip.max_frame_id >= frame_id)) {
			drop_frame = 1;
		} else if (frame_id > stream_info->sw_skip.max_frame_id) {
			spin_lock_irqsave(&stream_info->lock, flags);
			memset(&stream_info->sw_skip, 0,
					sizeof(struct msm_isp_sw_framskip));
			spin_unlock_irqrestore(&stream_info->lock, flags);
		}
	}

	if (!buf || !ts)
		return;

	if (vfe_dev->vt_enable) {
		msm_isp_get_avtimer_ts(ts);
		time_stamp = &ts->vt_time;
	} else {
		time_stamp = &ts->buf_time;
	}

	rc = vfe_dev->buf_mgr->ops->get_buf_src(vfe_dev->buf_mgr,
					buf->bufq_handle, &buf_src);
	if (rc != 0) {
		pr_err_ratelimited("%s: Error getting buf_src\n", __func__);
		return;
	}

	if (stream_info->buf_divert && rc == 0 &&
			buf_src != MSM_ISP_BUFFER_SRC_SCRATCH) {
		rc = vfe_dev->buf_mgr->ops->buf_divert(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx,
			time_stamp, frame_id);
	} else {
		rc = vfe_dev->buf_mgr->ops->update_put_buf_cnt(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx, frame_id);
	}

	/* Buf divert return value represent whether the buf
	 * can be diverted. A positive return value means
	 * other ISP hardware is still processing the frame.
	 */
	if (rc > 0) {
		ISP_DBG("%s: vfe_id %d bufq %x buf_id %d put_cnt 1\n", __func__,
			vfe_dev->pdev->id, buf->bufq_handle, buf->buf_idx);
		return;
	} else if (rc == 0) {
		if ((buf->frame_id != frame_id) &&
			vfe_dev->axi_data.enable_frameid_recovery) {
			struct msm_isp_event_data error_event;

			error_event.frame_id =
				vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
			error_event.u.error_info.err_type =
				ISP_ERROR_FRAME_ID_MISMATCH;
			rc = vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx);
			msm_isp_send_event(vfe_dev, ISP_EVENT_ERROR,
				&error_event);
			pr_err("%s: Error! frame id mismatch!! 1st buf frame %d,curr frame %d\n",
				__func__, buf->frame_id, frame_id);
			vfe_dev->buf_mgr->frameId_mismatch_recovery = 1;
			return;
		}
		if (drop_frame) {
			/* Put but if dual vfe usecase and
			 * both vfe have done using buf
			 */
			rc = vfe_dev->buf_mgr->ops->put_buf(
				vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx);
			if (!rc) {
				ISP_DBG("%s:%d vfe_id %d Buffer dropped %d\n",
					__func__, __LINE__, vfe_dev->pdev->id,
					frame_id);
				return;
			}
		}

		buf_event.frame_id = frame_id;
		buf_event.timestamp = *time_stamp;
		buf_event.u.buf_done.session_id = stream_info->session_id;
		buf_event.u.buf_done.stream_id = stream_info->stream_id;
		buf_event.u.buf_done.handle = buf->bufq_handle;
		buf_event.u.buf_done.buf_idx = buf->buf_idx;
		buf_event.u.buf_done.output_format =
			stream_info->runtime_output_format;
		if (stream_info->buf_divert &&
			buf_src != MSM_ISP_BUFFER_SRC_SCRATCH) {
			ISP_DBG(
				"%s: vfe_id %d send buf_divert frame_id %d buf-id %d bufq %x\n",
				__func__, vfe_dev->pdev->id, frame_id,
				buf->buf_idx, buf->bufq_handle);

			bufq = vfe_dev->buf_mgr->ops->get_bufq(vfe_dev->buf_mgr,
				buf->bufq_handle);
			if (!bufq) {
				pr_err("%s: Invalid bufq buf_handle %x\n",
					__func__, buf->bufq_handle);
			}

			if ((bufq != NULL) && bufq->buf_type == ISP_SHARE_BUF)
				msm_isp_send_event(vfe_dev->common_data->
					dual_vfe_res->vfe_dev[ISP_VFE1],
					ISP_EVENT_BUF_DIVERT, &buf_event);
			else
				msm_isp_send_event(vfe_dev,
				ISP_EVENT_BUF_DIVERT, &buf_event);
		} else {
			ISP_DBG("%s: vfe_id %d send buf done buf-id %d bufq %x\n",
				__func__, vfe_dev->pdev->id, buf->buf_idx,
				buf->bufq_handle);
			msm_isp_send_event(vfe_dev, ISP_EVENT_BUF_DONE,
				&buf_event);
			vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx, time_stamp,
				frame_id, stream_info->runtime_output_format);
		}
	} else {
		pr_err_ratelimited("%s: Warning! Unexpected return value rc = %d\n",
			__func__, rc);
	}
}

int msm_isp_drop_frame(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, struct msm_isp_timestamp *ts,
	struct msm_isp_sof_info *sof_info)
{
	struct msm_isp_buffer *done_buf = NULL;
	uint32_t pingpong_status, frame_id;
	unsigned long flags;
	struct msm_isp_bufq *bufq = NULL;
	int rc = -1;

	if (!vfe_dev || !stream_info || !ts || !sof_info) {
		pr_err("%s %d vfe_dev %p stream_info %p ts %p op_info %p\n",
			 __func__, __LINE__, vfe_dev, stream_info, ts,
			sof_info);
		return -EINVAL;
	}

	pingpong_status =
		~vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(vfe_dev);

	if (stream_info->stream_type == BURST_STREAM)
		stream_info->runtime_num_burst_capture--;

	spin_lock_irqsave(&stream_info->lock, flags);
	rc = msm_isp_get_done_buf(vfe_dev, stream_info,
		pingpong_status, &done_buf);
	if (rc < 0) {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_PING_PONG_MISMATCH);
		return rc;
	}

	if (stream_info->stream_type == CONTINUOUS_STREAM ||
		stream_info->runtime_num_burst_capture > 1) {
		ISP_DBG("%s: vfe %d drop frame bufq %x\n", __func__,
			vfe_dev->pdev->id, done_buf->bufq_handle);
		msm_isp_cfg_ping_pong_address(vfe_dev, stream_info,
			pingpong_status, 1);
	}

	spin_unlock_irqrestore(&stream_info->lock, flags);

	frame_id = vfe_dev->axi_data.
		src_info[SRC_TO_INTF(stream_info->stream_src)].frame_id;

	if (done_buf) {
		rc = vfe_dev->buf_mgr->ops->update_put_buf_cnt(vfe_dev->buf_mgr,
			done_buf->bufq_handle, done_buf->buf_idx, frame_id);
		if (rc == 0) {
			vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
				done_buf->bufq_handle, done_buf->buf_idx,
				&ts->buf_time, frame_id,
				stream_info->runtime_output_format);
		}

		bufq = vfe_dev->buf_mgr->ops->get_bufq(vfe_dev->buf_mgr,
			done_buf->bufq_handle);
		if (!bufq) {
			pr_err("%s: Invalid bufq buf_handle %x\n",
				__func__, done_buf->bufq_handle);
			return -EINVAL;
		}
		sof_info->reg_update_fail_mask |=
			1 << (bufq->bufq_handle & 0xF);
	}
	return 0;
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
			VFE_AXI_SRC_MAX) {
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

/*Factor in Q2 format*/
#define ISP_DEFAULT_FORMAT_FACTOR 6
#define ISP_BUS_UTILIZATION_FACTOR 6
static int msm_isp_update_stream_bandwidth(struct vfe_device *vfe_dev)
{
	int i, rc = 0;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint64_t total_pix_bandwidth = 0, total_rdi_bandwidth = 0;
	uint32_t num_pix_streams = 0;
	uint64_t total_bandwidth = 0;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->state == ACTIVE ||
			stream_info->state == START_PENDING) {
			if (stream_info->stream_src < RDI_INTF_0) {
				total_pix_bandwidth += stream_info->bandwidth;
				num_pix_streams++;
			} else {
				total_rdi_bandwidth += stream_info->bandwidth;
			}
		}
	}
	total_bandwidth = total_pix_bandwidth + total_rdi_bandwidth;
	rc = msm_isp_update_bandwidth(ISP_VFE0 + vfe_dev->pdev->id,
		(total_bandwidth + MSM_ISP_MIN_AB),
		(total_bandwidth + MSM_ISP_MIN_IB));

	if (rc < 0)
		pr_err("%s: update failed\n", __func__);

	return rc;
}

static int msm_isp_axi_wait_for_cfg_done(struct vfe_device *vfe_dev,
	enum msm_isp_camif_update_state camif_update,
	uint32_t src_mask, int regUpdateCnt)
{
	int rc;
	unsigned long flags;
	enum msm_vfe_input_src i = 0;
	spin_lock_irqsave(&vfe_dev->shared_data_lock, flags);

	for (i = 0; i < VFE_SRC_MAX; i++) {
		if (src_mask & (1 << i)) {
			if (vfe_dev->axi_data.stream_update[i] > 0) {
				pr_err("%s:Stream Update in progress. cnt %d\n",
					__func__,
					vfe_dev->axi_data.stream_update[i]);
				spin_unlock_irqrestore(
					&vfe_dev->shared_data_lock, flags);
				return -EINVAL;
			}
			vfe_dev->axi_data.stream_update[i] = regUpdateCnt;
		}
	}
	if (src_mask) {
		init_completion(&vfe_dev->stream_config_complete);
		vfe_dev->axi_data.pipeline_update = camif_update;
	}
	spin_unlock_irqrestore(&vfe_dev->shared_data_lock, flags);
	rc = wait_for_completion_timeout(
		&vfe_dev->stream_config_complete,
		msecs_to_jiffies(VFE_MAX_CFG_TIMEOUT));
	if (rc == 0) {
		for (i = 0; i < VFE_SRC_MAX; i++) {
			if (src_mask & (1 << i)) {
				spin_lock_irqsave(&vfe_dev->shared_data_lock,
					flags);
				vfe_dev->axi_data.stream_update[i] = 0;
				spin_unlock_irqrestore(&vfe_dev->
					shared_data_lock, flags);
			}
		}
		pr_err("%s: wait timeout\n", __func__);
		rc = -EBUSY;
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

	if (stream_info->stream_type != BURST_STREAM ||
		stream_info->runtime_num_burst_capture > 1)
		rc = msm_isp_cfg_ping_pong_address(vfe_dev,
			stream_info, VFE_PONG_FLAG, 0);

	if (rc < 0) {
		pr_err("%s: No free buffer for pong\n",
			__func__);
		return rc;
	}

	return rc;
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

	if (atomic_read(&vfe_dev->error_info.overflow_state) ==
		OVERFLOW_DETECTED) {
		ISP_DBG("%s: VFE%d already halted, direct return\n",
			__func__, vfe_dev->pdev->id);
		return rc;
	}

	if (halt_cmd->overflow_detected) {
		/*Store current IRQ mask*/
		if (vfe_dev->error_info.overflow_recover_irq_mask0 == 0) {
			vfe_dev->hw_info->vfe_ops.core_ops.get_irq_mask(vfe_dev,
			&vfe_dev->error_info.overflow_recover_irq_mask0,
			&vfe_dev->error_info.overflow_recover_irq_mask1);
		}

		atomic_cmpxchg(&vfe_dev->error_info.overflow_state,
			NO_OVERFLOW, OVERFLOW_DETECTED);
		pr_err("%s: VFE%d Bus overflow detected: start recovery!\n",
			__func__, vfe_dev->pdev->id);
	}

	rc = vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev,
		halt_cmd->blocking_halt);

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
	uint32_t bufq_handle = 0, bufq_id = 0;

	if (!reset_cmd) {
		pr_err("%s: NULL pointer reset cmd %p\n", __func__, reset_cmd);
		rc = -1;
		return rc;
	}

	rc = vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev,
		0, reset_cmd->blocking);

	for (i = 0, j = 0; j < axi_data->num_active_stream &&
		i < VFE_AXI_SRC_MAX; i++, j++) {
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

		for (bufq_id = 0; bufq_id < VFE_BUF_QUEUE_MAX; bufq_id++) {
			bufq_handle = stream_info->bufq_handle[bufq_id];
			if (!bufq_handle)
				continue;

			bufq = vfe_dev->buf_mgr->ops->get_bufq(vfe_dev->buf_mgr,
				bufq_handle);
			if (!bufq) {
				pr_err("%s: bufq null %p by handle %x\n",
					__func__, bufq, bufq_handle);
				continue;
			}

			vfe_dev->buf_mgr->ops->flush_buf(
				vfe_dev->buf_mgr, bufq_handle,
				MSM_ISP_BUFFER_FLUSH_ALL);
			memset(&stream_info->buf, 0, sizeof(stream_info->buf));

			axi_data->src_info[SRC_TO_INTF(stream_info->
				stream_src)]. frame_id = reset_cmd->frame_id;
			msm_isp_reset_burst_count_and_frame_drop(vfe_dev,
				stream_info);
		}
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
	unsigned long flags;

	vfe_dev->buf_mgr->frameId_mismatch_recovery = 0;
	for (i = 0, j = 0; j < axi_data->num_active_stream &&
		i < VFE_AXI_SRC_MAX; i++, j++) {
		stream_info = &axi_data->stream_info[i];
		if (stream_info->state != ACTIVE) {
			j--;
			continue;
		}
		msm_isp_get_stream_wm_mask(stream_info, &wm_reload_mask);
		spin_lock_irqsave(&stream_info->lock, flags);
		msm_isp_init_stream_ping_pong_reg(vfe_dev, stream_info);
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}
	vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev,
		vfe_dev->vfe_base, wm_reload_mask);

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
				VFE_AXI_SRC_MAX) {
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

static int msm_isp_update_dual_HW_ms_info_at_start(
	struct vfe_device *vfe_dev,
	enum msm_vfe_input_src stream_src)
{
	int rc = 0;
	uint32_t j, k, max_sof = 0;
	uint8_t slave_id;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_src_info *src_info = NULL;
	uint32_t vfe_id = 0;
	unsigned long flags;

	if (stream_src >= VFE_SRC_MAX) {
		pr_err("%s: Error! Invalid src %u\n", __func__, stream_src);
		return -EINVAL;
	}

	src_info = &axi_data->src_info[stream_src];
	if (src_info->dual_hw_type != DUAL_HW_MASTER_SLAVE)
		return rc;

	spin_lock_irqsave(&vfe_dev->common_data->common_dev_data_lock, flags);
	if (src_info->dual_hw_ms_info.dual_hw_ms_type ==
		MS_TYPE_MASTER) {
		if (vfe_dev->common_data->ms_resource.master_active == 1) {
			spin_unlock_irqrestore(&vfe_dev->common_data->
				common_dev_data_lock, flags);
			return rc;
		}

		vfe_dev->common_data->ms_resource.master_active = 1;

		/*
		 * If any slaves are active, then find the max slave
		 * frame_id and set it to Master, so master will start
		 * higher and then the slave can copy master frame_id
		 * without repeating.
		 */
		if (!vfe_dev->common_data->ms_resource.slave_active_mask) {
			spin_unlock_irqrestore(&vfe_dev->common_data->
				common_dev_data_lock, flags);
			return rc;
		}

		for (j = 0, k = 0; k < MS_NUM_SLAVE_MAX; k++) {
			if (!(vfe_dev->common_data->ms_resource.
				reserved_slave_mask & (1 << k)))
				continue;

			if (vfe_dev->common_data->ms_resource.slave_active_mask
				& (1 << k) &&
				(vfe_dev->common_data->ms_resource.
					slave_sof_info[k].frame_id > max_sof)) {
				max_sof = vfe_dev->common_data->ms_resource.
					slave_sof_info[k].frame_id;
			}
			j++;
			if (j == vfe_dev->common_data->ms_resource.num_slave)
				break;
		}
		vfe_dev->axi_data.src_info[stream_src].frame_id =
			max_sof + 1;
		if (vfe_dev->is_split) {
			vfe_id = vfe_dev->pdev->id;
			vfe_id = (vfe_id == 0) ? 1 : 0;
			vfe_dev->common_data->dual_vfe_res->axi_data[vfe_id]->
				src_info[stream_src].frame_id = max_sof + 1;
		}

		ISP_DBG("%s: Setting Master frame_id to %u\n", __func__,
			max_sof + 1);
	} else {
		if (src_info->dual_hw_ms_info.sof_info != NULL) {
			slave_id = src_info->dual_hw_ms_info.slave_id;
			vfe_dev->common_data->ms_resource.slave_active_mask |=
				(1 << slave_id);
		}
	}
	spin_unlock_irqrestore(&vfe_dev->common_data->common_dev_data_lock,
		flags);

	return rc;
}

static int msm_isp_update_dual_HW_ms_info_at_stop(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd,
	enum msm_isp_camif_update_state camif_update)
{
	int i, rc = 0;
	uint8_t slave_id;
	struct msm_vfe_axi_stream *stream_info = NULL;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	enum msm_vfe_input_src stream_src = VFE_SRC_MAX;
	struct msm_vfe_src_info *src_info = NULL;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM ||
		stream_cfg_cmd->num_streams == 0)
		return -EINVAL;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			VFE_AXI_SRC_MAX) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		stream_src = SRC_TO_INTF(stream_info->stream_src);

		/* Remove PIX if DISABLE CAMIF */
		if (stream_src == VFE_PIX_0 && !((camif_update == DISABLE_CAMIF)
			|| (camif_update == DISABLE_CAMIF_IMMEDIATELY)))
			continue;

		src_info = &axi_data->src_info[stream_src];
		if (src_info->dual_hw_type != DUAL_HW_MASTER_SLAVE)
			continue;

		spin_lock(&vfe_dev->common_data->common_dev_data_lock);
		if (src_info->dual_hw_ms_info.dual_hw_ms_type ==
			MS_TYPE_MASTER) {
			/*
			 * Once Master is inactive, slave will increment
			 * its own frame_id
			 */
			vfe_dev->common_data->ms_resource.master_active = 0;
		} else {
			slave_id = src_info->dual_hw_ms_info.slave_id;
			vfe_dev->common_data->ms_resource.reserved_slave_mask &=
				~(1 << slave_id);
			vfe_dev->common_data->ms_resource.slave_active_mask &=
				~(1 << slave_id);
			vfe_dev->common_data->ms_resource.num_slave--;
		}
		src_info->dual_hw_ms_info.sof_info = NULL;
		spin_unlock(&vfe_dev->common_data->common_dev_data_lock);
		vfe_dev->vfe_ub_policy = 0;
	}

	return rc;
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
	uint32_t src_mask = 0;
	unsigned long flags;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	if (camif_update == ENABLE_CAMIF) {
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id = 0;
	}

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			VFE_AXI_SRC_MAX) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		if (SRC_TO_INTF(stream_info->stream_src) < VFE_SRC_MAX)
			src_state = axi_data->src_info[
				SRC_TO_INTF(stream_info->stream_src)].active;
		else {
			ISP_DBG("%s: invalid src info index\n", __func__);
			return -EINVAL;
		}

		msm_isp_calculate_bandwidth(axi_data, stream_info);
		msm_isp_get_stream_wm_mask(stream_info, &wm_reload_mask);
		spin_lock_irqsave(&stream_info->lock, flags);
		msm_isp_reset_framedrop(vfe_dev, stream_info);
		rc = msm_isp_init_stream_ping_pong_reg(vfe_dev, stream_info);
		if (rc < 0) {
			pr_err("%s: No buffer for stream%d\n", __func__,
				HANDLE_TO_IDX(
				stream_cfg_cmd->stream_handle[i]));
			spin_unlock_irqrestore(&stream_info->lock, flags);
			return rc;
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);

		stream_info->state = START_PENDING;
		ISP_DBG("start axi Stream 0x%x src_state %d src type %d\n",
			stream_info->stream_id, src_state,
			stream_info->stream_type);
		if (src_state) {
			src_mask |= (1 << SRC_TO_INTF(stream_info->stream_src));
			wait_for_complete = 1;
		} else {
			if (vfe_dev->dump_reg)
				msm_camera_io_dump(vfe_dev->vfe_base,
					0x900, 1);

			/*Configure AXI start bits to start immediately*/
			msm_isp_axi_stream_enable_cfg(vfe_dev, stream_info, 0);
			stream_info->state = ACTIVE;
			vfe_dev->hw_info->vfe_ops.core_ops.reg_update(vfe_dev,
				SRC_TO_INTF(stream_info->stream_src));

			/*
			 * Active bit is set in enable_camif for PIX.
			 * For RDI, set it here
			 */
			if (SRC_TO_INTF(stream_info->stream_src) >= VFE_RAW_0 &&
				SRC_TO_INTF(stream_info->stream_src) <
				VFE_SRC_MAX) {
				/* Incase PIX and RDI streams are part of same
				 * session, this will ensure RDI stream will
				 * have same frame id as of PIX stream
				 */
				if (stream_cfg_cmd->sync_frame_id_src)
					vfe_dev->axi_data.src_info[SRC_TO_INTF(
					stream_info->stream_src)].frame_id =
					vfe_dev->axi_data.src_info[VFE_PIX_0]
					.frame_id;
				else
					vfe_dev->axi_data.src_info[SRC_TO_INTF(
					stream_info->stream_src)].frame_id = 0;
				vfe_dev->axi_data.src_info[SRC_TO_INTF(
					stream_info->stream_src)].active = 1;
			}
		}
	}
	msm_isp_update_stream_bandwidth(vfe_dev);
	vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev,
		vfe_dev->vfe_base, wm_reload_mask);
	msm_isp_update_camif_output_count(vfe_dev, stream_cfg_cmd);

	if (camif_update == ENABLE_CAMIF) {
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, camif_update);
		vfe_dev->axi_data.camif_state = CAMIF_ENABLE;
		vfe_dev->hw_info->vfe_ops.irq_ops.enable_camif_err(vfe_dev, 1);
	}

	if (wait_for_complete) {
		rc = msm_isp_axi_wait_for_cfg_done(vfe_dev, camif_update,
			src_mask, 2);
		if (rc < 0)
			pr_err("%s: wait for config done failed\n", __func__);
	}

	return rc;
}

static int msm_isp_stop_axi_stream(struct vfe_device *vfe_dev,
			struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd,
			enum msm_isp_camif_update_state camif_update)
{
	int i, rc = 0;
	uint8_t wait_for_complete_for_this_stream = 0;
	struct msm_vfe_axi_stream *stream_info = NULL;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int ext_read =
		(axi_data->src_info[VFE_PIX_0].input_mux == EXTERNAL_READ);
	uint32_t src_mask = 0, intf, bufq_id = 0, bufq_handle = 0;
	unsigned long flags;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM ||
		stream_cfg_cmd->num_streams == 0)
		return -EINVAL;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			VFE_AXI_SRC_MAX) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];

		/* set ping pong address to scratch before stream stop */
		spin_lock_irqsave(&stream_info->lock, flags);
		msm_isp_cfg_stream_scratch(vfe_dev, stream_info, VFE_PING_FLAG);
		msm_isp_cfg_stream_scratch(vfe_dev, stream_info, VFE_PONG_FLAG);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		wait_for_complete_for_this_stream = 0;

		stream_info->state = STOP_PENDING;
		ISP_DBG("Stop axi Stream 0x%x,\n",
			stream_info->stream_id);
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
				(!ext_read))
				wait_for_complete_for_this_stream = 1;
		}
		ISP_DBG("%s: vfe_dev %d camif_update %d wait %d\n", __func__,
			vfe_dev->pdev->id,
			camif_update,
			wait_for_complete_for_this_stream);
		intf = SRC_TO_INTF(stream_info->stream_src);
		if (!wait_for_complete_for_this_stream ||
			stream_info->state == INACTIVE ||
			!vfe_dev->axi_data.src_info[intf].active) {
			msm_isp_axi_stream_enable_cfg(vfe_dev, stream_info, 0);
			stream_info->state = INACTIVE;
			vfe_dev->hw_info->vfe_ops.core_ops.reg_update(vfe_dev,
				SRC_TO_INTF(stream_info->stream_src));

			/*
			 * Active bit is reset in disble_camif for PIX.
			 * For RDI, reset it here for not wait_for_complete
			 * This is assuming there is only 1 stream mapped to
			 * each RDI.
			 */
			if (intf >= VFE_RAW_0 &&
				intf < VFE_SRC_MAX) {
				vfe_dev->axi_data.src_info[intf].active = 0;
			}
		} else
			src_mask |= (1 << intf);

	}

	if (src_mask) {
		rc = msm_isp_axi_wait_for_cfg_done(vfe_dev, camif_update,
			src_mask, 2);
		if (rc < 0) {
			pr_err("%s: wait for config done failed, retry...\n",
				__func__);
			for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
				stream_info = &axi_data->stream_info[
				HANDLE_TO_IDX(
					stream_cfg_cmd->stream_handle[i])];
				stream_info->state = STOPPING;
				msm_isp_axi_stream_enable_cfg(
					vfe_dev, stream_info, 0);
				vfe_dev->hw_info->vfe_ops.core_ops.reg_update(
					vfe_dev,
					SRC_TO_INTF(stream_info->stream_src));
				rc = msm_isp_axi_wait_for_cfg_done(vfe_dev,
					camif_update, src_mask, 1);
				if (rc < 0)
					pr_err("%s: vfe%d cfg done failed\n",
						__func__, vfe_dev->pdev->id);
				else
					pr_err("%s: vfe%d retry success! report err!\n",
						__func__, vfe_dev->pdev->id);
				rc = -EBUSY;
			}
		}

		/*
		 * Active bit is reset in disble_camif for PIX.
		 * For RDI, reset it here after wait_for_complete
		 * This is assuming there is only 1 stream mapped to each RDI
		 */
		for (i = VFE_RAW_0; i < VFE_SRC_MAX; i++) {
			if (src_mask & (1 << i)) {
				vfe_dev->axi_data.src_info[i].active = 0;
			}
		}
	}

	if (camif_update == DISABLE_CAMIF) {
		vfe_dev->hw_info->vfe_ops.core_ops.
			update_camif_state(vfe_dev, DISABLE_CAMIF);
		vfe_dev->axi_data.camif_state = CAMIF_DISABLE;
	} else if ((camif_update == DISABLE_CAMIF_IMMEDIATELY) ||
					(ext_read)) {
		/*during stop immediately, stop output then stop input*/
		vfe_dev->hw_info->vfe_ops.irq_ops.enable_camif_err(vfe_dev, 0);
		vfe_dev->ignore_error = 1;
		vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev, 1);
		if (!ext_read)
			vfe_dev->hw_info->vfe_ops.core_ops.
				update_camif_state(vfe_dev,
						DISABLE_CAMIF_IMMEDIATELY);
		vfe_dev->axi_data.camif_state = CAMIF_STOPPED;
		vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev, 0, 1);
		vfe_dev->hw_info->vfe_ops.core_ops.init_hw_reg(vfe_dev);
		vfe_dev->hw_info->vfe_ops.irq_ops.enable_camif_err(vfe_dev, 1);
		vfe_dev->ignore_error = 0;
	}
	msm_isp_update_camif_output_count(vfe_dev, stream_cfg_cmd);
	msm_isp_update_stream_bandwidth(vfe_dev);

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		stream_info = &axi_data->stream_info[
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i])];
		for (bufq_id = 0; bufq_id < VFE_BUF_QUEUE_MAX; bufq_id++) {
			bufq_handle = stream_info->bufq_handle[bufq_id];
			if (!bufq_handle)
				continue;

			vfe_dev->buf_mgr->ops->flush_buf(
				vfe_dev->buf_mgr, bufq_handle,
				MSM_ISP_BUFFER_FLUSH_ALL);
		}
		memset(&stream_info->buf, 0, sizeof(stream_info->buf));
		vfe_dev->reg_update_requested &=
			~(BIT(SRC_TO_INTF(stream_info->stream_src)));
	}

	return rc;
}


int msm_isp_cfg_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, ret;
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd = arg;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	enum msm_isp_camif_update_state camif_update;

	rc = msm_isp_axi_check_stream_state(vfe_dev, stream_cfg_cmd);
	if (rc < 0) {
		pr_err("%s: Invalid stream state\n", __func__);
		return rc;
	}

	if (axi_data->num_active_stream == 0) {
		/*Configure UB*/
		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_ub(vfe_dev);
		/*when start reset overflow state*/
		atomic_set(&vfe_dev->error_info.overflow_state,
			NO_OVERFLOW);
	}
	camif_update = msm_isp_get_camif_update_state(vfe_dev, stream_cfg_cmd);
	if (camif_update == DISABLE_CAMIF)
		vfe_dev->axi_data.camif_state = CAMIF_STOPPING;
	if (stream_cfg_cmd->cmd == START_STREAM) {
		msm_isp_axi_update_cgc_override(vfe_dev, stream_cfg_cmd, 1);

		rc = msm_isp_start_axi_stream(
			vfe_dev, stream_cfg_cmd, camif_update);
	} else {
		rc = msm_isp_stop_axi_stream(
			vfe_dev, stream_cfg_cmd, camif_update);

		msm_isp_axi_update_cgc_override(vfe_dev, stream_cfg_cmd, 0);
		if (axi_data->num_active_stream == 0) {
			/* Reset hvx state */
			vfe_dev->hvx_cmd = HVX_DISABLE;
		}

		/*
		 * Use different ret value to not overwrite the error from
		 * msm_isp_stop_axi_stream
		 */
		ret = msm_isp_update_dual_HW_ms_info_at_stop(
			vfe_dev, stream_cfg_cmd, camif_update);
		if (ret < 0)
			pr_warn("%s: Warning! Update dual_cam failed\n",
				__func__);
	}

	if (rc < 0)
		pr_err("%s: start/stop stream failed\n", __func__);
	return rc;
}

static int msm_isp_return_empty_buffer(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t user_stream_id,
	uint32_t frame_id, enum msm_vfe_input_src frame_src)
{
	int rc = -1;
	struct msm_isp_buffer *buf = NULL;
	uint32_t bufq_handle = 0, buf_cnt = 0;
	uint32_t stream_idx;
	struct msm_isp_event_data error_event;
	struct msm_isp_timestamp timestamp;

	if (!vfe_dev || !stream_info) {
		pr_err("%s %d failed: vfe_dev %p stream_info %p\n", __func__,
			__LINE__, vfe_dev, stream_info);
		return -EINVAL;
	}

	stream_idx = HANDLE_TO_IDX(stream_info->stream_handle);
	if (!stream_info->controllable_output)
		return -EINVAL;

	if (frame_src >= VFE_SRC_MAX) {
		pr_err("%s: Invalid frame_src %d", __func__, frame_src);
		return -EINVAL;
	}

	if (stream_idx >= VFE_AXI_SRC_MAX) {
		pr_err("%s: Invalid stream_idx", __func__);
		return rc;
	}

	if (user_stream_id == stream_info->stream_id)
		bufq_handle = stream_info->bufq_handle[VFE_BUF_QUEUE_DEFAULT];
	else
		bufq_handle = stream_info->bufq_handle[VFE_BUF_QUEUE_SHARED];


	rc = vfe_dev->buf_mgr->ops->get_buf(vfe_dev->buf_mgr,
		vfe_dev->pdev->id, bufq_handle, &buf, &buf_cnt);
	if (rc < 0) {
		vfe_dev->error_info.
			stream_framedrop_count[bufq_handle & 0xFF]++;
		return rc;
	}

	msm_isp_get_timestamp(&timestamp);
	rc = vfe_dev->buf_mgr->ops->update_put_buf_cnt(vfe_dev->buf_mgr,
		buf->bufq_handle, buf->buf_idx, frame_id);
	if (rc == 0) {
		vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx,
			&timestamp.buf_time, frame_id,
			stream_info->runtime_output_format);
	}

	error_event.frame_id = frame_id;
	error_event.u.error_info.err_type = ISP_ERROR_RETURN_EMPTY_BUFFER;
	error_event.u.error_info.session_id = stream_info->session_id;
	error_event.u.error_info.stream_id_mask =
		1 << (bufq_handle & 0xFF);
	msm_isp_send_event(vfe_dev, ISP_EVENT_ERROR, &error_event);

	return 0;
}

static int msm_isp_request_frame(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t user_stream_id,
	uint32_t frame_id)
{
	struct msm_vfe_axi_stream_request_cmd stream_cfg_cmd;
	struct msm_vfe_frame_request_queue *queue_req;
	uint32_t pingpong_status;
	unsigned long flags;
	int rc = 0;
	enum msm_vfe_input_src frame_src = 0;
	struct dual_vfe_resource *dual_vfe_res = NULL;
	uint32_t vfe_id = 0;

	if (!vfe_dev || !stream_info) {
		pr_err("%s %d failed: vfe_dev %p stream_info %p\n", __func__,
			__LINE__, vfe_dev, stream_info);
		return -EINVAL;
	}

	if (!stream_info->controllable_output)
		return 0;


	if (stream_info->stream_src >= VFE_AXI_SRC_MAX) {
		pr_err("%s:%d invalid stream src %d\n", __func__, __LINE__,
			stream_info->stream_src);
		return -EINVAL;
	}

	frame_src = SRC_TO_INTF(stream_info->stream_src);
	/*
	 * If PIX stream is active then RDI path uses SOF frame ID of PIX
	 * In case of standalone RDI streaming, SOF are used from
	 * individual intf.
	 */
	if (((vfe_dev->axi_data.src_info[VFE_PIX_0].active) && (frame_id <=
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id)) ||
		((!vfe_dev->axi_data.src_info[VFE_PIX_0].active) && (frame_id <=
		vfe_dev->axi_data.src_info[frame_src].frame_id)) ||
		stream_info->undelivered_request_cnt >= MAX_BUFFERS_IN_HW) {
		pr_debug("%s:%d invalid request_frame %d cur frame id %d pix %d\n",
			__func__, __LINE__, frame_id,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id,
			vfe_dev->axi_data.src_info[VFE_PIX_0].active);

		rc = msm_isp_return_empty_buffer(vfe_dev, stream_info,
			user_stream_id, frame_id, frame_src);
		if (rc < 0)
			pr_err("%s:%d failed: return_empty_buffer src %d\n",
				__func__, __LINE__, frame_src);
		return 0;
	}
	if ((frame_src == VFE_PIX_0) && !stream_info->undelivered_request_cnt &&
		MSM_VFE_STREAM_STOP_PERIOD !=
		stream_info->current_framedrop_period) {
		pr_debug("%s:%d vfe %d frame_id %d prev_pattern %x stream_id %x\n",
			__func__, __LINE__, vfe_dev->pdev->id, frame_id,
			stream_info->prev_framedrop_period,
			stream_info->stream_id);

		rc = msm_isp_return_empty_buffer(vfe_dev, stream_info,
			user_stream_id, frame_id, frame_src);
		if (rc < 0)
			pr_err("%s:%d failed: return_empty_buffer src %d\n",
				__func__, __LINE__, frame_src);
		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_framedrop(
			vfe_dev->vfe_base, stream_info, 0, 0);
		stream_info->current_framedrop_period =
			MSM_VFE_STREAM_STOP_PERIOD;
		return 0;
	}

	spin_lock_irqsave(&stream_info->lock, flags);
	queue_req = &stream_info->request_queue_cmd[stream_info->request_q_idx];
	if (queue_req->cmd_used) {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		pr_err_ratelimited("%s: Request queue overflow.\n", __func__);
		return -EINVAL;
	}

	if (user_stream_id == stream_info->stream_id)
		queue_req->buff_queue_id = VFE_BUF_QUEUE_DEFAULT;
	else
		queue_req->buff_queue_id = VFE_BUF_QUEUE_SHARED;

	if (!stream_info->bufq_handle[queue_req->buff_queue_id]) {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		pr_err("%s:%d stream is stoped\n", __func__, __LINE__);
		return 0;
	}
	queue_req->cmd_used = 1;

	stream_info->request_q_idx =
		(stream_info->request_q_idx + 1) % MSM_VFE_REQUESTQ_SIZE;
	list_add_tail(&queue_req->list, &stream_info->request_q);
	stream_info->request_q_cnt++;

	stream_info->undelivered_request_cnt++;
	stream_cfg_cmd.axi_stream_handle = stream_info->stream_handle;
	stream_cfg_cmd.frame_skip_pattern = NO_SKIP;
	stream_cfg_cmd.init_frame_drop = 0;
	stream_cfg_cmd.burst_count = stream_info->request_q_cnt;

	if (stream_info->undelivered_request_cnt == 1) {
		rc = msm_isp_cfg_ping_pong_address(vfe_dev, stream_info,
			VFE_PING_FLAG, 1);
		if (rc) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			stream_info->undelivered_request_cnt--;
			pr_err_ratelimited("%s:%d fail to cfg HAL buffer\n",
				__func__, __LINE__);
			return rc;
		}

		dual_vfe_res = vfe_dev->common_data->dual_vfe_res;
		vfe_id = vfe_dev->pdev->id;
		msm_isp_get_stream_wm_mask(stream_info,
			&dual_vfe_res->wm_reload_mask[vfe_id]);
		if (stream_info->stream_src < RDI_INTF_0 &&
			vfe_dev->is_split &&
			vfe_dev->pdev->id == ISP_VFE1) {
			vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev,
				dual_vfe_res->vfe_base[ISP_VFE0],
				dual_vfe_res->wm_reload_mask[ISP_VFE0]);
			vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev,
				vfe_dev->vfe_base,
				dual_vfe_res->wm_reload_mask[ISP_VFE1]);
			dual_vfe_res->wm_reload_mask[ISP_VFE0] = 0;
			dual_vfe_res->wm_reload_mask[ISP_VFE1] = 0;
		} else if (!vfe_dev->is_split ||
			(stream_info->stream_src >= RDI_INTF_0 &&
			stream_info->stream_src <= RDI_INTF_2)){
			vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev,
				vfe_dev->vfe_base,
				dual_vfe_res->wm_reload_mask[vfe_id]);
			dual_vfe_res->wm_reload_mask[vfe_id] = 0;
		}
		stream_info->sw_ping_pong_bit = 0;
	} else if (stream_info->undelivered_request_cnt == 2) {
		pingpong_status =
			vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(
				vfe_dev);

		rc = msm_isp_cfg_ping_pong_address(vfe_dev, stream_info,
			pingpong_status, 1);
		if (rc) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			stream_info->undelivered_request_cnt--;
			pr_err_ratelimited("%s:%d Failed to cfg HAL buffer\n",
				__func__, __LINE__);
			return rc;
		}
	} else {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		stream_info->undelivered_request_cnt--;
		pr_err_ratelimited("%s: Invalid undeliver frame count %d\n",
			__func__, stream_info->undelivered_request_cnt);
		return -EINVAL;
	}

	rc = msm_isp_calculate_framedrop(&vfe_dev->axi_data, &stream_cfg_cmd);
	if (0 == rc)
		msm_isp_reset_framedrop(vfe_dev, stream_info);

	spin_unlock_irqrestore(&stream_info->lock, flags);

	return rc;
}

static int msm_isp_add_buf_queue(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t stream_id)
{
	int rc = 0;
	uint32_t bufq_id = 0;

	if (stream_id == stream_info->stream_id)
		bufq_id = VFE_BUF_QUEUE_DEFAULT;
	else
		bufq_id = VFE_BUF_QUEUE_SHARED;

	stream_info->bufq_handle[bufq_id] =
		vfe_dev->buf_mgr->ops->get_bufq_handle(vfe_dev->buf_mgr,
			stream_info->session_id, stream_id);
	if (stream_info->bufq_handle[bufq_id] == 0) {
		pr_err("%s: failed: No valid buffer queue for stream: 0x%x\n",
			__func__, stream_id);
		rc = -EINVAL;
	}

	return rc;
}

static void msm_isp_remove_buf_queue(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t stream_id)
{
	uint32_t bufq_id = 0;
	unsigned long flags;

	if (stream_id == stream_info->stream_id)
		bufq_id = VFE_BUF_QUEUE_DEFAULT;
	else
		bufq_id = VFE_BUF_QUEUE_SHARED;

	spin_lock_irqsave(&stream_info->lock, flags);
	stream_info->bufq_handle[bufq_id] = 0;
	spin_unlock_irqrestore(&stream_info->lock, flags);

}

int msm_isp_update_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i, j;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	struct msm_vfe_axi_stream_update_cmd *update_cmd = arg;
	struct msm_vfe_axi_stream_cfg_update_info *update_info;
	struct msm_isp_sw_framskip *sw_skip_info = NULL;
	unsigned long flags;

	/*num_stream is uint32 and update_info[] bound by MAX_NUM_STREAM*/
	if (update_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = &update_cmd->update_info[i];
		/*check array reference bounds*/
		if (HANDLE_TO_IDX(update_info->stream_handle) >=
			VFE_AXI_SRC_MAX) {
			return -EINVAL;
		}
		stream_info = &axi_data->stream_info[
				HANDLE_TO_IDX(update_info->stream_handle)];
		if (SRC_TO_INTF(stream_info->stream_src) >= VFE_SRC_MAX)
			continue;
		if (stream_info->state != ACTIVE &&
			stream_info->state != INACTIVE &&
			update_cmd->update_type !=
			UPDATE_STREAM_REQUEST_FRAMES &&
			update_cmd->update_type !=
			UPDATE_STREAM_REMOVE_BUFQ &&
			update_cmd->update_type !=
			UPDATE_STREAM_SW_FRAME_DROP) {
			pr_err("%s: Invalid stream state %d, update cmd %d\n",
				__func__, stream_info->state,
				stream_info->stream_id);
			return -EINVAL;
		}
		if (update_cmd->update_type == UPDATE_STREAM_AXI_CONFIG &&
			atomic_read(&axi_data->axi_cfg_update[
				SRC_TO_INTF(stream_info->stream_src)])) {
			pr_err("%s: AXI stream config updating\n", __func__);
			return -EBUSY;
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
			stream_info->buf_divert = 0;
			vfe_dev->buf_mgr->ops->flush_buf(vfe_dev->buf_mgr,
				stream_info->bufq_handle[VFE_BUF_QUEUE_DEFAULT],
				MSM_ISP_BUFFER_FLUSH_DIVERTED);
			break;
		case UPDATE_STREAM_FRAMEDROP_PATTERN: {
			uint32_t framedrop_period =
				msm_isp_get_framedrop_period(
				   update_info->skip_pattern);
			spin_lock_irqsave(&stream_info->lock, flags);
			/* no change then break early */
			if (stream_info->current_framedrop_period ==
				framedrop_period) {
				spin_unlock_irqrestore(&stream_info->lock,
					flags);
				break;
			}
			if (stream_info->controllable_output) {
				pr_err("Controllable output streams does not support custom frame skip pattern\n");
				spin_unlock_irqrestore(&stream_info->lock,
					flags);
				return -EINVAL;
			}
			if (update_info->skip_pattern == SKIP_ALL)
				stream_info->current_framedrop_period =
					MSM_VFE_STREAM_STOP_PERIOD;
			else
				stream_info->current_framedrop_period =
					framedrop_period;
			if (stream_info->stream_type != BURST_STREAM)
				msm_isp_cfg_framedrop_reg(vfe_dev, stream_info);
			spin_unlock_irqrestore(&stream_info->lock, flags);
			break;
		}
		case UPDATE_STREAM_SW_FRAME_DROP: {
			sw_skip_info = &update_info->sw_skip_info;
			if (sw_skip_info->stream_src_mask != 0) {
				/* SW image buffer drop */
				pr_debug("%s:%x sw skip type %x mode %d min %d max %d\n",
					__func__, stream_info->stream_id,
					sw_skip_info->stats_type_mask,
					sw_skip_info->skip_mode,
					sw_skip_info->min_frame_id,
					sw_skip_info->max_frame_id);
				spin_lock_irqsave(&stream_info->lock, flags);
				stream_info->sw_skip = *sw_skip_info;
				spin_unlock_irqrestore(&stream_info->lock,
					flags);
			}
			break;
		}
		case UPDATE_STREAM_AXI_CONFIG: {
			for (j = 0; j < stream_info->num_planes; j++) {
				stream_info->plane_cfg[j] =
					update_info->plane_cfg[j];
			}
			stream_info->output_format = update_info->output_format;
			if ((stream_info->state == ACTIVE) &&
				((vfe_dev->hw_info->runtime_axi_update == 0) ||
				(vfe_dev->dual_vfe_enable == 1)))  {
				spin_lock_irqsave(&stream_info->lock, flags);
				stream_info->state = PAUSE_PENDING;
				msm_isp_axi_stream_enable_cfg(
					vfe_dev, stream_info, 1);
				stream_info->state = PAUSING;
				atomic_set(&axi_data->
					axi_cfg_update[SRC_TO_INTF(
					stream_info->stream_src)],
					UPDATE_REQUESTED);
				spin_unlock_irqrestore(&stream_info->lock,
					flags);
			} else {
				for (j = 0; j < stream_info->num_planes; j++) {
					vfe_dev->hw_info->vfe_ops.axi_ops.
					cfg_wm_reg(vfe_dev, stream_info, j);
				}

				spin_lock_irqsave(&stream_info->lock, flags);
				if (stream_info->state != ACTIVE) {
					stream_info->runtime_output_format =
						stream_info->output_format;
				} else {
					stream_info->state = RESUMING;
					atomic_set(&axi_data->
						axi_cfg_update[SRC_TO_INTF(
						stream_info->stream_src)],
						APPLYING_UPDATE_RESUME);
				}
				spin_unlock_irqrestore(&stream_info->lock,
					flags);
			}
			break;
		}
		case UPDATE_STREAM_REQUEST_FRAMES: {
			rc = msm_isp_request_frame(vfe_dev, stream_info,
				update_info->user_stream_id,
				update_info->frame_id);
			if (rc)
				pr_err("%s failed to request frame!\n",
					__func__);
			break;
		}
		case UPDATE_STREAM_ADD_BUFQ: {
			rc = msm_isp_add_buf_queue(vfe_dev, stream_info,
				update_info->user_stream_id);
			if (rc)
				pr_err("%s failed to add bufq!\n", __func__);
			break;
		}
		case UPDATE_STREAM_REMOVE_BUFQ: {
			msm_isp_remove_buf_queue(vfe_dev, stream_info,
				update_info->user_stream_id);
			pr_debug("%s, Remove bufq for Stream 0x%x\n",
				__func__, stream_info->stream_id);
			if (stream_info->state == ACTIVE) {
				stream_info->state = UPDATING;
				rc = msm_isp_axi_wait_for_cfg_done(vfe_dev,
					NO_UPDATE, (1 << SRC_TO_INTF(
					stream_info->stream_src)), 2);
				if (rc < 0)
					pr_err("%s: wait for update failed\n",
						__func__);
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

void msm_isp_process_axi_irq_stream(struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info,
		uint32_t pingpong_status,
		struct msm_isp_timestamp *ts)
{
	int rc;
	struct msm_isp_buffer *done_buf = NULL;
	unsigned long flags;

	spin_lock_irqsave(&stream_info->lock, flags);

	if (stream_info->state == INACTIVE) {
		msm_isp_cfg_stream_scratch(vfe_dev, stream_info,
					pingpong_status);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		pr_err_ratelimited("%s: Warning! Stream already inactive. Drop irq handling\n",
			__func__);
		return;
	}
	rc = msm_isp_get_done_buf(vfe_dev, stream_info,
		pingpong_status, &done_buf);

	if (rc < 0) {
		pr_err_ratelimited("%s:VFE%d get done buf fail\n",
			__func__, vfe_dev->pdev->id);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_PING_PONG_MISMATCH);
		return;
	}
	if (vfe_dev->buf_mgr->frameId_mismatch_recovery == 1) {
		pr_err_ratelimited("%s: Mismatch Recovery in progress, drop frame!\n",
			__func__);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		return;
	}

	stream_info->frame_id++;

	if (stream_info->stream_type == BURST_STREAM && done_buf &&
		stream_info->runtime_num_burst_capture) {
		ISP_DBG("%s: burst_frame_count: %d\n",
			__func__,
			stream_info->runtime_num_burst_capture);
		BUG_ON(0 == stream_info->runtime_num_burst_capture);
		stream_info->runtime_num_burst_capture--;
	}

	if (stream_info->stream_type == CONTINUOUS_STREAM ||
		stream_info->runtime_num_burst_capture > 1)
		rc = msm_isp_cfg_ping_pong_address(vfe_dev,
			stream_info, pingpong_status, 1);

	spin_unlock_irqrestore(&stream_info->lock, flags);

	if (done_buf)
		msm_isp_process_done_buf(vfe_dev, stream_info,
					done_buf, ts);
}

void msm_isp_process_axi_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1,
	struct msm_isp_timestamp *ts)
{
	int i, rc = 0;
	uint32_t comp_mask = 0, wm_mask = 0;
	uint32_t pingpong_status, stream_idx;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_composite_info *comp_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int wm;

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
		wm_mask &= ~(comp_info->stream_composite_mask);
		if (comp_mask & (1 << i)) {
			stream_idx = HANDLE_TO_IDX(comp_info->stream_handle);
			if ((!comp_info->stream_handle) ||
				(stream_idx >= VFE_AXI_SRC_MAX)) {
				pr_err_ratelimited("%s: Invalid handle for composite irq\n",
					__func__);
				for (wm = 0; wm < axi_data->hw_info->num_wm;
					wm++)
					if (comp_info->stream_composite_mask &
						(1 << wm))
						msm_isp_cfg_wm_scratch(vfe_dev,
							wm, pingpong_status);
				continue;
			}
			stream_idx = HANDLE_TO_IDX(comp_info->stream_handle);
			stream_info = &axi_data->stream_info[stream_idx];

			msm_isp_process_axi_irq_stream(vfe_dev, stream_info,
						pingpong_status, ts);

		}
	}

	for (i = 0; i < axi_data->hw_info->num_wm; i++) {
		if (wm_mask & (1 << i)) {
			stream_idx = HANDLE_TO_IDX(axi_data->free_wm[i]);
			if ((!axi_data->free_wm[i]) ||
				(stream_idx >= VFE_AXI_SRC_MAX)) {
				pr_err("%s: Invalid handle for wm irq\n",
					__func__);
				msm_isp_cfg_wm_scratch(vfe_dev, i,
					pingpong_status);
				continue;
			}
			stream_info = &axi_data->stream_info[stream_idx];

			msm_isp_process_axi_irq_stream(vfe_dev, stream_info,
						pingpong_status, ts);
		}
	}
	return;
}

void msm_isp_axi_disable_all_wm(struct vfe_device *vfe_dev)
{
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int i, j;

	if (!vfe_dev || !axi_data) {
		pr_err("%s: error  %p %p\n", __func__, vfe_dev, axi_data);
		return;
	}

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = &axi_data->stream_info[i];

		if (stream_info->state != ACTIVE)
			continue;

		for (j = 0; j < stream_info->num_planes; j++)
			vfe_dev->hw_info->vfe_ops.axi_ops.enable_wm(
				vfe_dev->vfe_base,
				stream_info->wm[j], 0);
	}
}
