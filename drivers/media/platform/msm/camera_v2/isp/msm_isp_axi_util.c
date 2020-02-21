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
#include "msm_isp_util.h"
#include "msm_isp_stats_util.h"
#include "msm_isp_axi_util.h"
#include "msm_isp48.h"
#include "trace/events/msm_cam.h"

#define HANDLE_TO_IDX(handle) (handle & 0xFF)
#define ISP_SOF_DEBUG_COUNT 0
#define OTHER_VFE(vfe_id) (vfe_id == ISP_VFE0 ? ISP_VFE1 : ISP_VFE0)

#ifdef CONFIG_MSM_AVTIMER
static struct avtimer_fptr_t avtimer_func;
#endif
static void msm_isp_reload_ping_pong_offset(
		struct msm_vfe_axi_stream *stream_info);

static void __msm_isp_axi_stream_update(
			struct msm_vfe_axi_stream *stream_info,
			struct msm_isp_timestamp *ts);

static int msm_isp_process_done_buf(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, struct msm_isp_buffer *buf,
	struct timeval *time_stamp, uint32_t frame_id);
static void msm_isp_free_pending_buffer(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	struct msm_isp_timestamp *ts);
static int msm_isp_update_stream_bandwidth(
		struct msm_vfe_axi_stream *stream_info, int enable);

#define DUAL_VFE_AND_VFE1(s, v) ((s->stream_src < RDI_INTF_0) && \
			v->is_split && vfe_dev->pdev->id == ISP_VFE1)

#define RDI_OR_NOT_DUAL_VFE(v, s) (!v->is_split || \
			((s->stream_src >= RDI_INTF_0) && \
			(stream_info->stream_src <= RDI_INTF_2)))

static int msm_isp_axi_create_stream(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_shared_data *axi_data,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd,
	struct msm_vfe_axi_stream *stream_info)
{
	uint32_t i = 0;
	int rc = 0;

	if (stream_info->state != AVAILABLE) {
		pr_err("%s:%d invalid state %d expected %d\n",
			__func__, __LINE__, stream_info->state,
			AVAILABLE);
		return -EINVAL;
	}

	if (stream_info->num_isp == 0) {
		stream_info->session_id = stream_cfg_cmd->session_id;
		stream_info->stream_id = stream_cfg_cmd->stream_id;
		stream_info->buf_divert = stream_cfg_cmd->buf_divert;
		stream_info->stream_src = stream_cfg_cmd->stream_src;
		stream_info->controllable_output =
			stream_cfg_cmd->controllable_output;
		stream_info->activated_framedrop_period =
					MSM_VFE_STREAM_STOP_PERIOD;
		if (stream_cfg_cmd->controllable_output)
			stream_cfg_cmd->frame_skip_pattern = SKIP_ALL;
		INIT_LIST_HEAD(&stream_info->request_q);
	} else {
		/* check if the stream has been added for the vfe-device */
		if (stream_info->vfe_mask & (1 << vfe_dev->pdev->id)) {
			pr_err("%s: stream %pK/%x is already added for vfe dev %d vfe_mask %x\n",
				__func__, stream_info, stream_info->stream_id,
				vfe_dev->pdev->id, stream_info->vfe_mask);
			return -EINVAL;
		}
		if (stream_info->session_id != stream_cfg_cmd->session_id) {
			pr_err("%s: dual stream session id mismatch %d/%d\n",
				__func__, stream_info->session_id,
				stream_cfg_cmd->session_id);
			rc = -EINVAL;
		}
		if (stream_info->stream_id != stream_cfg_cmd->stream_id) {
			pr_err("%s: dual stream stream id mismatch %d/%d\n",
				__func__, stream_info->stream_id,
				stream_cfg_cmd->stream_id);
			rc = -EINVAL;
		}
		if (stream_info->controllable_output !=
			stream_cfg_cmd->controllable_output) {
			pr_err("%s: dual stream controllable_op mismatch %d/%d\n",
				__func__, stream_info->controllable_output,
				stream_cfg_cmd->controllable_output);
			rc = -EINVAL;
		}
		if (stream_info->buf_divert != stream_cfg_cmd->buf_divert) {
			pr_err("%s: dual stream buf_divert mismatch %d/%d\n",
				__func__, stream_info->buf_divert,
				stream_cfg_cmd->buf_divert);
			rc = -EINVAL;
		}
		if (rc)
			return rc;
	}
	stream_info->vfe_dev[stream_info->num_isp] = vfe_dev;
	stream_info->num_isp++;

	if ((axi_data->stream_handle_cnt << 8) == 0)
		axi_data->stream_handle_cnt++;

	stream_cfg_cmd->axi_stream_handle =
		(++axi_data->stream_handle_cnt) << 8 | stream_info->stream_src;

	ISP_DBG("%s: vfe %d handle %x\n", __func__, vfe_dev->pdev->id,
		stream_cfg_cmd->axi_stream_handle);

	stream_info->stream_handle[stream_info->num_isp - 1] =
		stream_cfg_cmd->axi_stream_handle;
	stream_info->vfe_mask |= (1 << vfe_dev->pdev->id);

	if (!vfe_dev->is_split || stream_cfg_cmd->stream_src >= RDI_INTF_0 ||
		stream_info->num_isp == MAX_VFE) {
		stream_info->state = INACTIVE;

		for (i = 0; i < MSM_ISP_COMP_IRQ_MAX; i++)
			stream_info->composite_irq[i] = 0;
	}
	return 0;
}

static void msm_isp_axi_destroy_stream(
	struct vfe_device *vfe_dev, struct msm_vfe_axi_stream *stream_info)
{
	int k;
	int j;
	int i;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	/*
	 * For the index being removed, shift everything to it's right by 1
	 * so that the index being removed becomes the last index
	 */
	for (i = vfe_idx, k = vfe_idx + 1; k < stream_info->num_isp; k++, i++) {
		stream_info->vfe_dev[i] = stream_info->vfe_dev[k];
		stream_info->stream_handle[i] = stream_info->stream_handle[k];
		stream_info->bandwidth[i] = stream_info->bandwidth[k];
		stream_info->max_width[i] = stream_info->max_width[k];
		stream_info->comp_mask_index[i] =
				stream_info->comp_mask_index[k];
		for (j = 0; j < stream_info->num_planes; j++) {
			stream_info->plane_cfg[i][j] =
				stream_info->plane_cfg[k][j];
			stream_info->wm[i][j] = stream_info->wm[k][j];
		}
	}

	stream_info->num_isp--;
	stream_info->vfe_dev[stream_info->num_isp] = NULL;
	stream_info->stream_handle[stream_info->num_isp] = 0;
	stream_info->bandwidth[stream_info->num_isp] = 0;
	stream_info->max_width[stream_info->num_isp] = 0;
	stream_info->comp_mask_index[stream_info->num_isp] = -1;
	stream_info->vfe_mask &= ~(1 << vfe_dev->pdev->id);
	for (j = 0; j < stream_info->num_planes; j++) {
		stream_info->wm[stream_info->num_isp][j] = -1;
		memset(&stream_info->plane_cfg[stream_info->num_isp][j],
			0, sizeof(
			stream_info->plane_cfg[stream_info->num_isp][j]));
	}

	if (stream_info->num_isp == 0) {
		/* release the bufq */
		for (k = 0; k < VFE_BUF_QUEUE_MAX; k++)
			stream_info->bufq_handle[k] = 0;
		stream_info->vfe_mask = 0;
		stream_info->state = AVAILABLE;
		memset(&stream_info->request_queue_cmd,
			0, sizeof(stream_info->request_queue_cmd));
	}
}

static int msm_isp_validate_axi_request(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	int rc = -1, i;
	int vfe_idx;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;

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
	case V4L2_PIX_FMT_SBGGR10DPCM6:
	case V4L2_PIX_FMT_SGBRG10DPCM6:
	case V4L2_PIX_FMT_SGRBG10DPCM6:
	case V4L2_PIX_FMT_SRGGB10DPCM6:
	case V4L2_PIX_FMT_SBGGR10DPCM8:
	case V4L2_PIX_FMT_SGBRG10DPCM8:
	case V4L2_PIX_FMT_SGRBG10DPCM8:
	case V4L2_PIX_FMT_SRGGB10DPCM8:
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
	case V4L2_PIX_FMT_P16BGGR12:
	case V4L2_PIX_FMT_P16GBRG12:
	case V4L2_PIX_FMT_P16GRBG12:
	case V4L2_PIX_FMT_P16RGGB12:
	case V4L2_PIX_FMT_JPEG:
	case V4L2_PIX_FMT_META:
	case V4L2_PIX_FMT_META10:
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y10:
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

	vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++) {
		stream_info->plane_cfg[vfe_idx][i] =
			stream_cfg_cmd->plane_cfg[i];
		stream_info->max_width[vfe_idx] =
			max(stream_info->max_width[vfe_idx],
			stream_cfg_cmd->plane_cfg[i].output_width);
	}

	stream_info->output_format = stream_cfg_cmd->output_format;
	stream_info->runtime_output_format = stream_info->output_format;
	stream_info->stream_src = stream_cfg_cmd->stream_src;
	stream_info->frame_based = stream_cfg_cmd->frame_base;
	return 0;
}

static uint32_t msm_isp_axi_get_plane_size(
	struct msm_vfe_axi_stream *stream_info, int vfe_idx, int plane_idx)
{
	uint32_t size = 0;
	struct msm_vfe_axi_plane_cfg *plane_cfg =
				stream_info->plane_cfg[vfe_idx];
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
	case V4L2_PIX_FMT_SBGGR10DPCM6:
	case V4L2_PIX_FMT_SGBRG10DPCM6:
	case V4L2_PIX_FMT_SGRBG10DPCM6:
	case V4L2_PIX_FMT_SRGGB10DPCM6:
	case V4L2_PIX_FMT_SBGGR10DPCM8:
	case V4L2_PIX_FMT_SGBRG10DPCM8:
	case V4L2_PIX_FMT_SGRBG10DPCM8:
	case V4L2_PIX_FMT_SRGGB10DPCM8:
	case V4L2_PIX_FMT_QBGGR10:
	case V4L2_PIX_FMT_QGBRG10:
	case V4L2_PIX_FMT_QGRBG10:
	case V4L2_PIX_FMT_QRGGB10:
	case V4L2_PIX_FMT_META10:
	case V4L2_PIX_FMT_Y10:
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
	case V4L2_PIX_FMT_P16BGGR12:
	case V4L2_PIX_FMT_P16GBRG12:
	case V4L2_PIX_FMT_P16GRBG12:
	case V4L2_PIX_FMT_P16RGGB12:
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

static void msm_isp_axi_reserve_wm(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int i, j;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++) {
		for (j = 0; j < axi_data->hw_info->num_wm; j++) {
			if (!axi_data->free_wm[j]) {
				axi_data->free_wm[j] =
					stream_info->stream_handle[vfe_idx];
				axi_data->wm_image_size[j] =
					msm_isp_axi_get_plane_size(
						stream_info, vfe_idx, i);
				axi_data->num_used_wm++;
				break;
			}
		}
		ISP_DBG("%s vfe %d stream_handle %x wm %d\n", __func__,
			vfe_dev->pdev->id,
			stream_info->stream_handle[vfe_idx], j);
		stream_info->wm[vfe_idx][i] = j;
		/* setup var to ignore bus error from RDI wm */
		if (stream_info->stream_src >= RDI_INTF_0) {
			if (vfe_dev->hw_info->vfe_ops.core_ops
				.set_bus_err_ign_mask)
				vfe_dev->hw_info->vfe_ops.core_ops
					.set_bus_err_ign_mask(vfe_dev, j, 1);
		}
	}
}

void msm_isp_axi_free_wm(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int i;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++) {
		axi_data->free_wm[stream_info->wm[vfe_idx][i]] = 0;
		axi_data->num_used_wm--;
		if (stream_info->stream_src >= RDI_INTF_0) {
			if (vfe_dev->hw_info->vfe_ops.core_ops
				.set_bus_err_ign_mask)
				vfe_dev->hw_info->vfe_ops.core_ops
					.set_bus_err_ign_mask(vfe_dev,
						stream_info->wm[vfe_idx][i], 0);
		}
	}
	if (stream_info->stream_src <= IDEAL_RAW)
		axi_data->num_pix_stream++;
	else if (stream_info->stream_src < VFE_AXI_SRC_MAX)
		axi_data->num_rdi_stream++;
}

static void msm_isp_axi_reserve_comp_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	int i;
	uint8_t comp_mask = 0;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++)
		comp_mask |= 1 << stream_info->wm[vfe_idx][i];

	for (i = 0; i < axi_data->hw_info->num_comp_mask; i++) {
		if (!axi_data->composite_info[i].stream_handle) {
			axi_data->composite_info[i].stream_handle =
				stream_info->stream_handle[vfe_idx];
			axi_data->composite_info[i].stream_composite_mask =
				comp_mask;
			axi_data->num_used_composite_mask++;
			break;
		}
	}
	stream_info->comp_mask_index[vfe_idx] = i;
}

static void msm_isp_axi_free_comp_mask(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	axi_data->composite_info[stream_info->comp_mask_index[vfe_idx]].
		stream_composite_mask = 0;
	axi_data->composite_info[stream_info->comp_mask_index[vfe_idx]].
		stream_handle = 0;
	axi_data->num_used_composite_mask--;
}

/**
 * msm_isp_cfg_framedrop_reg() - Program the period and pattern
 * @stream_info: The stream for which programming is done
 *
 * This function calculates the period and pattern to be configured
 * for the stream based on the current frame id of the stream's input
 * source and the initial framedrops.
 *
 * Returns void.
 */
static void msm_isp_cfg_framedrop_reg(
	struct msm_vfe_axi_stream *stream_info)
{
	struct vfe_device *vfe_dev = stream_info->vfe_dev[0];
	uint32_t runtime_init_frame_drop;
	uint32_t framedrop_pattern = 0;
	uint32_t framedrop_period = MSM_VFE_STREAM_STOP_PERIOD;
	enum msm_vfe_input_src frame_src = SRC_TO_INTF(stream_info->stream_src);
	int i;

	if (vfe_dev == NULL) {
		pr_err("%s %d returning vfe_dev is NULL\n",
			__func__,  __LINE__);
		return;
	}

	if (vfe_dev->axi_data.src_info[frame_src].frame_id >=
		stream_info->init_frame_drop)
		runtime_init_frame_drop = 0;
	else
		runtime_init_frame_drop = stream_info->init_frame_drop -
			vfe_dev->axi_data.src_info[frame_src].frame_id;

	if (!runtime_init_frame_drop)
		framedrop_period = stream_info->current_framedrop_period;

	if (framedrop_period != MSM_VFE_STREAM_STOP_PERIOD)
		framedrop_pattern = 0x1;

	if (WARN_ON(framedrop_period == 0))
		pr_err("%s framedrop_period is 0", __func__);

	for (i = 0; i < stream_info->num_isp; i++) {
		vfe_dev = stream_info->vfe_dev[i];
		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_framedrop(
					vfe_dev, stream_info, framedrop_pattern,
					framedrop_period);
	}

	ISP_DBG("%s: stream %x src %x framedrop pattern %x period %u\n",
			__func__,
			stream_info->stream_handle[0], stream_info->stream_src,
			framedrop_pattern, framedrop_period);

	stream_info->requested_framedrop_period = framedrop_period;
}

static int msm_isp_composite_irq(struct vfe_device *vfe_dev,
				struct msm_vfe_axi_stream *stream_info,
				enum msm_isp_comp_irq_types irq)
{
	/* interrupt recv on same vfe w/o recv on other vfe */
	if (stream_info->composite_irq[irq] & (1 << vfe_dev->pdev->id)) {
		msm_isp_dump_ping_pong_mismatch(vfe_dev);
		pr_err("%s: irq %d out of sync for dual vfe on vfe %d\n",
			__func__, irq, vfe_dev->pdev->id);
		return -EINVAL;
	}

	stream_info->composite_irq[irq] |= (1 << vfe_dev->pdev->id);
	if (stream_info->composite_irq[irq] != stream_info->vfe_mask)
		return 1;

	stream_info->composite_irq[irq] = 0;

	return 0;
}

/**
 * msm_isp_update_framedrop_reg() - Update frame period pattern on h/w
 * @stream_info: Stream for which update is to be performed
 *
 * If the period and pattern needs to be updated for a stream then it is
 * updated here. Updates happen if initial frame drop reaches 0 or burst
 * streams have been provided new skip pattern from user space.
 *
 * Returns void
 */
static void msm_isp_update_framedrop_reg(struct msm_vfe_axi_stream *stream_info,
		uint32_t drop_reconfig)
{
	if (stream_info->stream_type == BURST_STREAM) {
		if (stream_info->runtime_num_burst_capture == 0 ||
			(stream_info->runtime_num_burst_capture == 1 &&
			stream_info->activated_framedrop_period == 1))
			stream_info->current_framedrop_period =
				MSM_VFE_STREAM_STOP_PERIOD;
	}

	if (stream_info->undelivered_request_cnt > 0)
		stream_info->current_framedrop_period =
			MSM_VFE_STREAM_STOP_PERIOD;
	/*
	 * re-configure the period pattern, only if it's not already
	 * set to what we want
	 */
	if (stream_info->current_framedrop_period !=
		stream_info->requested_framedrop_period) {
		msm_isp_cfg_framedrop_reg(stream_info);
	}
}

void msm_isp_process_reg_upd_epoch_irq(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src,
	enum msm_isp_comp_irq_types irq,
	struct msm_isp_timestamp *ts)
{
	int i;
	struct msm_vfe_axi_stream *stream_info;
	unsigned long flags;
	int ret;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = msm_isp_get_stream_common_data(vfe_dev, i);
		if (SRC_TO_INTF(stream_info->stream_src) !=
			frame_src) {
			continue;
		}
		if (stream_info->state == AVAILABLE ||
			stream_info->state == INACTIVE)
			continue;

		spin_lock_irqsave(&stream_info->lock, flags);
		if (!vfe_dev->dual_vfe_sync_mode) {
			ret = msm_isp_composite_irq(vfe_dev, stream_info, irq);
			if (ret) {
				spin_unlock_irqrestore(&stream_info->lock,
					flags);
				if (ret < 0) {
					msm_isp_halt_send_error(vfe_dev,
						ISP_EVENT_PING_PONG_MISMATCH);
					return;
				}
				continue;
			}
		}
		switch (irq) {
		case MSM_ISP_COMP_IRQ_REG_UPD:
			stream_info->activated_framedrop_period =
				stream_info->requested_framedrop_period;
			/* Free Pending Buffers which are backed-up due to
			 * delay in RUP from userspace to Avoid pageFault
			 */
			msm_isp_free_pending_buffer(vfe_dev, stream_info, ts);
			__msm_isp_axi_stream_update(stream_info, ts);
			break;
		case MSM_ISP_COMP_IRQ_EPOCH:
			if (stream_info->state == ACTIVE) {
				struct vfe_device *temp = NULL;
				struct msm_vfe_common_dev_data *c_data;
				uint32_t drop_reconfig =
					vfe_dev->isp_page->drop_reconfig;
				if (stream_info->num_isp > 1 &&
					vfe_dev->pdev->id == ISP_VFE0 &&
					!vfe_dev->dual_vfe_sync_mode) {
					c_data = vfe_dev->common_data;
					temp = c_data->dual_vfe_res->vfe_dev[
						ISP_VFE1];
					drop_reconfig =
						temp->isp_page->drop_reconfig;
				}
				msm_isp_update_framedrop_reg(stream_info,
					drop_reconfig);
			}
			break;
		default:
			WARN(1, "Invalid irq %d\n", irq);
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
	uint32_t framedrop_period = 0;

	stream_info->runtime_num_burst_capture = stream_info->num_burst_capture;

	/**
	 *  only reset none controllable output stream, since the
	 *  controllable stream framedrop period will be controlled
	 *  by the request frame api
	 */
	if (!stream_info->controllable_output) {
		framedrop_period =
			msm_isp_get_framedrop_period(
			stream_info->frame_skip_pattern);
		if (stream_info->frame_skip_pattern == SKIP_ALL)
			stream_info->current_framedrop_period =
				MSM_VFE_STREAM_STOP_PERIOD;
		else
			stream_info->current_framedrop_period =
				framedrop_period;
	}

	msm_isp_cfg_framedrop_reg(stream_info);
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
	uint32_t stream_idx;

	if (!vfe_dev || !sof_info) {
		pr_err("%s %d failed: vfe_dev %pK sof_info %pK\n", __func__,
			__LINE__, vfe_dev, sof_info);
		return;
	}
	sof_info->regs_not_updated = 0;
	sof_info->reg_update_fail_mask = 0;
	sof_info->stream_get_buf_fail_mask = 0;

	axi_data = &vfe_dev->axi_data;

	for (i = 0; i < RDI_INTF_0; i++) {
		stream_info = msm_isp_get_stream_common_data(vfe_dev,
								i);
		stream_idx = HANDLE_TO_IDX(stream_info->stream_handle[0]);

		/*
		 * Process drop only if controllable ACTIVE PIX stream &&
		 * reg_not_updated
		 * OR stream is in RESUMING state.
		 * Other cases there is no drop to report, so continue.
		 */
		if (!((stream_info->state == ACTIVE &&
			stream_info->controllable_output &&
			(SRC_TO_INTF(stream_info->stream_src) ==
			VFE_PIX_0)) ||
			stream_info->state == RESUMING))
			continue;

		if (stream_info->controllable_output &&
			!vfe_dev->reg_updated) {
			if (stream_info->undelivered_request_cnt) {
				/* report that registers are not updated
				 * and return empty buffer for controllable
				 * outputs
				 */
				sof_info->regs_not_updated =
					!vfe_dev->reg_updated;
				pr_err("Drop frame no reg update\n");
				if (msm_isp_drop_frame(vfe_dev, stream_info, ts,
					sof_info)) {
					pr_err("drop frame failed\n");
				}
			}
		}

		if (stream_info->state == RESUMING &&
			!stream_info->controllable_output) {
			ISP_DBG("%s: axi_updating_mask strm_id %x frm_id %d\n",
				__func__, stream_idx,
				vfe_dev->axi_data.src_info[SRC_TO_INTF(
					stream_info->stream_src)].frame_id);
			sof_info->axi_updating_mask |=
				1 << stream_idx;
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
				vfe_dev->error_info.stream_framedrop_count[i] =
					0;
			}
		}
		vfe_dev->error_info.framedrop_flag = 0;
	}
}

static void msm_isp_sync_dual_cam_frame_id(
		struct vfe_device *vfe_dev,
		struct master_slave_resource_info *ms_res,
		enum msm_vfe_input_src frame_src,
		struct msm_isp_timestamp *ts)
{
	struct msm_vfe_src_info *src_info =
		&vfe_dev->axi_data.src_info[frame_src];
	int i;
	uint32_t frame_id = src_info->frame_id;
	uint32_t master_time = 0, current_time;

	if (src_info->dual_hw_ms_info.sync_state ==
		ms_res->dual_sync_mode) {
		(frame_src == VFE_PIX_0) ? src_info->frame_id +=
				vfe_dev->axi_data.src_info[
					frame_src].sof_counter_step :
			src_info->frame_id++;
		return;
	}

	/* find highest frame id */
	for (i = 0; i < MAX_VFE * VFE_SRC_MAX; i++) {
		if (ms_res->src_info[i] == NULL)
			continue;
		if (src_info == ms_res->src_info[i] ||
			ms_res->src_info[i]->active == 0)
			continue;
		if (frame_id >= ms_res->src_info[i]->frame_id)
			continue;
		frame_id = ms_res->src_info[i]->frame_id;
		master_time = ms_res->src_info[
				i]->dual_hw_ms_info.sof_info.mono_timestamp_ms;
	}
	/* copy highest frame id to the intf based on sof delta */
	current_time = ts->buf_time.tv_sec * 1000 +
		ts->buf_time.tv_usec / 1000;

	if (current_time > master_time &&
		(current_time - master_time) > ms_res->sof_delta_threshold) {
		if (frame_src == VFE_PIX_0)
			frame_id += vfe_dev->axi_data.src_info[
					frame_src].sof_counter_step;
		else
			frame_id += 1;
	} else {
		for (i = 0; i < MAX_VFE * VFE_SRC_MAX; i++) {
			if (ms_res->src_info[i] == NULL)
				continue;
			if (src_info == ms_res->src_info[i] ||
				((1 << ms_res->src_info[
					i]->dual_hw_ms_info.index) &
				ms_res->active_src_mask) == 0)
				continue;
			if (ms_res->src_info[i]->frame_id == frame_id)
				ms_res->src_sof_mask |= (1 <<
				ms_res->src_info[i]->dual_hw_ms_info.index);
		}
	}
	/* the number of frames that are dropped */
	vfe_dev->isp_page->dual_cam_drop =
				frame_id - (src_info->frame_id + 1);
	ms_res->active_src_mask |= (1 << src_info->dual_hw_ms_info.index);
	src_info->frame_id = frame_id;
	src_info->dual_hw_ms_info.sync_state = MSM_ISP_DUAL_CAM_SYNC;
}

void msm_isp_increment_frame_id(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts)
{
	struct msm_vfe_src_info *src_info = NULL;
	struct msm_vfe_sof_info *sof_info = NULL;
	enum msm_vfe_dual_hw_type dual_hw_type;
	enum msm_vfe_dual_hw_ms_type ms_type;
	unsigned long flags;
	int i;
	struct master_slave_resource_info *ms_res =
				&vfe_dev->common_data->ms_resource;

	spin_lock_irqsave(&vfe_dev->common_data->common_dev_data_lock, flags);
	dual_hw_type =
		vfe_dev->axi_data.src_info[frame_src].dual_hw_type;
	ms_type =
		vfe_dev->axi_data.src_info[
		frame_src].dual_hw_ms_info.dual_hw_ms_type;

	src_info = &vfe_dev->axi_data.src_info[frame_src];
	if (dual_hw_type == DUAL_HW_MASTER_SLAVE) {
		msm_isp_sync_dual_cam_frame_id(vfe_dev, ms_res, frame_src, ts);
		if (src_info->dual_hw_ms_info.sync_state ==
			MSM_ISP_DUAL_CAM_SYNC) {
			/*
			 * for dual hw check that we recv sof from all
			 * linked intf
			 */
			if (ms_res->src_sof_mask & (1 <<
				src_info->dual_hw_ms_info.index)) {
				pr_err_ratelimited("Frame out of sync on vfe %d\n",
					vfe_dev->pdev->id);
				/* Notify to do reconfig at SW sync drop*/
				vfe_dev->isp_page->dual_cam_drop_detected = 1;
				/*
				 * set this isp as async mode to force
				 *it sync again at the next sof
				 */
				src_info->dual_hw_ms_info.sync_state =
							MSM_ISP_DUAL_CAM_ASYNC;
				/*
				 * set the other isp as async mode to force
				 * it sync again at the next sof
				 */
				for (i = 0; i < MAX_VFE * VFE_SRC_MAX; i++) {
					if (ms_res->src_info[i] == NULL)
						continue;
					if (src_info == ms_res->src_info[i] ||
						ms_res->src_info[i]->active ==
							0)
						continue;
					ms_res->src_info[
					i]->dual_hw_ms_info.sync_state =
							MSM_ISP_DUAL_CAM_ASYNC;
				}
			}
			ms_res->src_sof_mask |= (1 <<
					src_info->dual_hw_ms_info.index);
			if (ms_res->active_src_mask == ms_res->src_sof_mask)
				ms_res->src_sof_mask = 0;
		}
		sof_info = &vfe_dev->axi_data.src_info[
			frame_src].dual_hw_ms_info.sof_info;
		sof_info->frame_id = vfe_dev->axi_data.src_info[
			frame_src].frame_id;
		sof_info->timestamp_ms = ts->event_time.tv_sec * 1000 +
			ts->event_time.tv_usec / 1000;
		sof_info->mono_timestamp_ms = ts->buf_time.tv_sec * 1000 +
			ts->buf_time.tv_usec / 1000;
		spin_unlock_irqrestore(
			&vfe_dev->common_data->common_dev_data_lock, flags);
	} else {
		spin_unlock_irqrestore(
			&vfe_dev->common_data->common_dev_data_lock, flags);
		if (frame_src == VFE_PIX_0) {
			vfe_dev->axi_data.src_info[frame_src].frame_id +=
				vfe_dev->axi_data.src_info[
					frame_src].sof_counter_step;
			ISP_DBG("%s: vfe %d sof_step %d frame_id %d\n",
			__func__,
			vfe_dev->pdev->id,
			vfe_dev->axi_data.src_info[frame_src].sof_counter_step,
			vfe_dev->axi_data.src_info[frame_src].frame_id);
		} else {
			vfe_dev->axi_data.src_info[frame_src].frame_id++;
		}
	}
	if (vfe_dev->is_split && vfe_dev->dual_vfe_sync_mode) {
		struct vfe_device *temp_dev;
		int other_vfe_id = (vfe_dev->pdev->id == ISP_VFE0 ?
					ISP_VFE1 : ISP_VFE0);
		temp_dev = vfe_dev->common_data->dual_vfe_res->vfe_dev[
			other_vfe_id];
		temp_dev->axi_data.src_info[frame_src].frame_id =
			vfe_dev->axi_data.src_info[frame_src].frame_id;
	}

	if (frame_src == VFE_PIX_0) {
		if (vfe_dev->isp_page == NULL)
			pr_err("Invalid ISP PAGE");
		else
			vfe_dev->isp_page->kernel_sofid =
				vfe_dev->axi_data.src_info[frame_src].frame_id;

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
	}
}

static void msm_isp_update_pd_stats_idx(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src)
{
	struct msm_vfe_axi_stream *pd_stream_info = NULL;
	uint32_t pingpong_status = 0, pingpong_bit = 0;
	struct msm_isp_buffer *done_buf = NULL;
	int vfe_idx = -1;
	unsigned long flags;

	if (frame_src < VFE_RAW_0 || frame_src >  VFE_RAW_2)
		return;

	pd_stream_info = msm_isp_get_stream_common_data(vfe_dev,
		RDI_INTF_0 + frame_src - VFE_RAW_0);

	if (pd_stream_info && (pd_stream_info->state == ACTIVE) &&
		(pd_stream_info->rdi_input_type ==
		MSM_CAMERA_RDI_PDAF)) {
		vfe_idx = msm_isp_get_vfe_idx_for_stream(
				vfe_dev, pd_stream_info);
		pingpong_status =
			vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(
						vfe_dev);
		pingpong_bit = ((pingpong_status >>
			pd_stream_info->wm[vfe_idx][0]) & 0x1);
		done_buf = pd_stream_info->buf[pingpong_bit];
		spin_lock_irqsave(
			&vfe_dev->common_data->common_dev_data_lock, flags);
		if (done_buf)
			vfe_dev->common_data->pd_buf_idx = done_buf->buf_idx;
		else
			vfe_dev->common_data->pd_buf_idx = 0xF;
		spin_unlock_irqrestore(
			&vfe_dev->common_data->common_dev_data_lock, flags);
	}
}

void msm_isp_notify(struct vfe_device *vfe_dev, uint32_t event_type,
	enum msm_vfe_input_src frame_src, struct msm_isp_timestamp *ts)
{
	struct msm_isp_event_data event_data;
	struct msm_vfe_sof_info *sof_info = NULL, *self_sof = NULL;
	enum msm_vfe_dual_hw_ms_type ms_type;
	unsigned long flags;

	memset(&event_data, 0, sizeof(event_data));

	switch (event_type) {
	case ISP_EVENT_SOF:
		if (frame_src == VFE_PIX_0) {
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

		ISP_DBG("%s: vfe %d frame_src %d frameid %d\n", __func__,
			vfe_dev->pdev->id, frame_src,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		trace_msm_cam_isp_status_dump("SOFNOTIFY:", vfe_dev->pdev->id,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id,
			0, 0, 0);

		/*
		 * Cannot support dual_cam and framedrop same time in union.
		 * If need to support framedrop as well, move delta calculation
		 * to userspace
		 */
		spin_lock_irqsave(
			&vfe_dev->common_data->common_dev_data_lock,
			flags);
		if (vfe_dev->common_data->ms_resource.dual_sync_mode ==
						MSM_ISP_DUAL_CAM_SYNC &&
			vfe_dev->axi_data.src_info[frame_src].dual_hw_type ==
			DUAL_HW_MASTER_SLAVE) {
			struct master_slave_resource_info *ms_res =
				&vfe_dev->common_data->ms_resource;
			self_sof = &vfe_dev->axi_data.src_info[
				frame_src].dual_hw_ms_info.sof_info;
			ms_type = vfe_dev->axi_data.src_info[
				frame_src].dual_hw_ms_info.dual_hw_ms_type;
			/* only send back time delta for primatry intf */
			if (ms_res->primary_slv_idx > 0 &&
					ms_type == MS_TYPE_MASTER)
				sof_info = &ms_res->src_info[
				ms_res->primary_slv_idx]->dual_hw_ms_info
					.sof_info;
			if (ms_type != MS_TYPE_MASTER &&
				ms_res->master_index > 0)
				sof_info = &ms_res->src_info[
					ms_res->master_index]->dual_hw_ms_info
						.sof_info;
			if (sof_info) {
				event_data.u.sof_info.ms_delta_info.delta[0] =
					self_sof->mono_timestamp_ms -
					sof_info->mono_timestamp_ms;
				event_data.u.sof_info.ms_delta_info
					.num_delta_info = 1;
			}
		}
		spin_unlock_irqrestore(
			&vfe_dev->common_data->common_dev_data_lock, flags);
		if (frame_src == VFE_PIX_0)
			msm_isp_check_for_output_error(vfe_dev, ts,
					&event_data.u.sof_info);
		/*
		 * Get and store the buf idx for PD stats
		 * this is to send the PD stats buffer address
		 * in BF stats done.
		 */
		msm_isp_update_pd_stats_idx(vfe_dev, frame_src);
		break;

	default:
		break;
	}

	if ((vfe_dev->nanosec_ts_enable) &&
		(event_type == ISP_EVENT_SOF) &&
			(frame_src == VFE_PIX_0)) {
		struct msm_isp_event_data_nanosec event_data_nanosec;

		event_data_nanosec.frame_id =
			vfe_dev->axi_data.src_info[frame_src].frame_id;
		event_data_nanosec.nano_timestamp = ts->buf_time_ns;
		msm_isp_send_event_update_nanosec(vfe_dev,
			ISP_EVENT_SOF_UPDATE_NANOSEC,
			&event_data_nanosec);
	}

	event_data.frame_id = vfe_dev->axi_data.src_info[frame_src].frame_id;
	event_data.timestamp = ts->event_time;
	event_data.mono_timestamp = ts->buf_time;
	msm_isp_send_event(vfe_dev, event_type | frame_src, &event_data);
}

/**
 * msm_isp_calculate_framedrop() - Setup frame period and pattern
 * @vfe_dev: vfe device.
 * @stream_cfg_cmd: User space input parameter for perion/pattern.
 *
 * Initialize the h/w stream framedrop period and pattern sent
 * by user space.
 *
 * Returns 0 on success else error code.
 */
static int msm_isp_calculate_framedrop(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd)
{
	uint32_t framedrop_period = 0;
	struct msm_vfe_axi_stream *stream_info = NULL;

	if (HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle)
		< VFE_AXI_SRC_MAX) {
		stream_info = msm_isp_get_stream_common_data(vfe_dev,
			HANDLE_TO_IDX(stream_cfg_cmd->axi_stream_handle));
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

static void msm_isp_calculate_bandwidth(
	struct msm_vfe_axi_stream *stream_info)
{
	int bpp = 0;
	struct vfe_device *vfe_dev;
	struct msm_vfe_axi_shared_data *axi_data;
	int i;

	for (i = 0; i < stream_info->num_isp; i++) {
		vfe_dev = stream_info->vfe_dev[i];
		axi_data = &vfe_dev->axi_data;
		if (stream_info->stream_src < RDI_INTF_0) {
			stream_info->bandwidth[i] =
				(vfe_dev->vfe_clk_info[
				vfe_dev->hw_info->vfe_clk_idx].clk_rate /
				axi_data->src_info[VFE_PIX_0].width) *
				stream_info->max_width[i];
			stream_info->bandwidth[i] =
				(unsigned long)stream_info->bandwidth[i] *
				stream_info->format_factor / ISP_Q2;
		} else {
			int rdi = SRC_TO_INTF(stream_info->stream_src);

			bpp = msm_isp_get_bit_per_pixel(
					stream_info->output_format);
			if (rdi < VFE_SRC_MAX) {
				stream_info->bandwidth[i] =
				(vfe_dev->vfe_clk_info[
				vfe_dev->hw_info->vfe_clk_idx].clk_rate /
				8) * bpp;
			} else {
				pr_err("%s: Invalid rdi interface\n", __func__);
			}
		}
	}
}

#ifdef CONFIG_MSM_AVTIMER
/**
 * msm_isp_set_avtimer_fptr() - Set avtimer function pointer
 * @avtimer: struct of type avtimer_fptr_t to hold function pointer.
 *
 * Initialize the function pointers sent by the avtimer driver
 *
 */
void msm_isp_set_avtimer_fptr(struct avtimer_fptr_t avtimer)
{
	avtimer_func.fptr_avtimer_open   = avtimer.fptr_avtimer_open;
	avtimer_func.fptr_avtimer_enable = avtimer.fptr_avtimer_enable;
	avtimer_func.fptr_avtimer_get_time = avtimer.fptr_avtimer_get_time;
}
EXPORT_SYMBOL(msm_isp_set_avtimer_fptr);

void msm_isp_start_avtimer(void)
{
	if (avtimer_func.fptr_avtimer_open &&
			avtimer_func.fptr_avtimer_enable) {
		avtimer_func.fptr_avtimer_open();
		avtimer_func.fptr_avtimer_enable(1);
	}
}
void msm_isp_stop_avtimer(void)
{
	if (avtimer_func.fptr_avtimer_enable)
		avtimer_func.fptr_avtimer_enable(0);
}

void msm_isp_get_avtimer_ts(
		struct msm_isp_timestamp *time_stamp)
{
	int rc = 0;
	uint32_t avtimer_usec = 0;
	uint64_t avtimer_tick = 0;

	if (avtimer_func.fptr_avtimer_get_time) {
		rc = avtimer_func.fptr_avtimer_get_time(&avtimer_tick);
		if (rc < 0) {
			pr_err_ratelimited("%s: Error: Invalid AVTimer Tick, rc=%d\n",
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
}
#else
void msm_isp_start_avtimer(void)
{
	pr_err("AV Timer is not supported\n");
}

void msm_isp_get_avtimer_ts(
		struct msm_isp_timestamp *time_stamp)
{
	struct timespec ts;

	pr_debug("%s: AVTimer driver not available using system time\n",
		__func__);

	get_monotonic_boottime(&ts);
	time_stamp->vt_time.tv_sec    = ts.tv_sec;
	time_stamp->vt_time.tv_usec   = ts.tv_nsec/1000;
}
#endif

int msm_isp_request_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i = 0;
	uint32_t io_format = 0;
	struct msm_vfe_axi_stream_request_cmd *stream_cfg_cmd = arg;
	struct msm_vfe_axi_stream *stream_info;

	if (stream_cfg_cmd->stream_src >= VFE_AXI_SRC_MAX) {
		pr_err("%s:%d invalid stream_src %d\n", __func__, __LINE__,
			stream_cfg_cmd->stream_src);
		return -EINVAL;
	}
	stream_info = msm_isp_get_stream_common_data(vfe_dev,
					stream_cfg_cmd->stream_src);

	if (stream_info->num_isp >= 0) {
		rc = msm_isp_axi_create_stream(vfe_dev,
		&vfe_dev->axi_data, stream_cfg_cmd, stream_info);
	}

	if (rc) {
		pr_err("%s: create stream failed\n", __func__);
		return rc;
	}

	rc = msm_isp_validate_axi_request(
		vfe_dev, stream_info, stream_cfg_cmd);
	if (rc) {
		msm_isp_axi_destroy_stream(vfe_dev, stream_info);
		pr_err("%s: Request validation failed\n", __func__);
		return rc;
	}

	stream_info->rdi_input_type = stream_cfg_cmd->rdi_input_type;
	vfe_dev->reg_update_requested &=
		~(BIT(SRC_TO_INTF(stream_info->stream_src)));

	msm_isp_axi_reserve_wm(vfe_dev, stream_info);

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

	if (!stream_info->controllable_output) {
		/*
		 * check that the parameters passed from second vfe is same
		 * as first vfe, do this only for non controllable stream
		 * right now because user driver has bug where it sends
		 * mismatch info for controllable streams
		 */
		if (stream_info->num_isp > 1) {
			if (stream_cfg_cmd->init_frame_drop !=
				stream_info->init_frame_drop) {
				pr_err("%s: stream %d init drop mismatch %d/%d\n",
					__func__, stream_info->stream_id,
					stream_info->init_frame_drop,
					stream_cfg_cmd->init_frame_drop);
				rc = -EINVAL;
			}
			if (stream_cfg_cmd->frame_skip_pattern !=
				stream_info->frame_skip_pattern) {
				pr_err("%s: stream %d skip pattern mismatch %d/%d\n",
					__func__, stream_info->stream_id,
					stream_info->frame_skip_pattern,
					stream_cfg_cmd->frame_skip_pattern);
				rc = -EINVAL;
			}
			if (stream_info->stream_type == CONTINUOUS_STREAM &&
				stream_cfg_cmd->burst_count > 0) {
				pr_err("%s: stream %d stream type mismatch\n",
					__func__, stream_info->stream_id);
				rc = -EINVAL;
			}
			if (stream_info->stream_type == BURST_STREAM &&
				stream_info->num_burst_capture !=
				stream_cfg_cmd->burst_count) {
				pr_err("%s: stream %d stream burst count mismatch %d/%d\n",
					__func__, stream_info->stream_id,
					stream_info->num_burst_capture,
					stream_cfg_cmd->burst_count);
				rc = -EINVAL;
			}
		} else {
			rc = msm_isp_calculate_framedrop(vfe_dev,
							stream_cfg_cmd);
		}
		if (rc)
			goto done;
	} else {
		stream_info->stream_type = BURST_STREAM;
		stream_info->num_burst_capture = 0;
		stream_info->frame_skip_pattern = NO_SKIP;
		stream_info->init_frame_drop = stream_cfg_cmd->init_frame_drop;
		stream_info->current_framedrop_period =
				MSM_VFE_STREAM_STOP_PERIOD;
	}
	if (stream_cfg_cmd->vt_enable && !vfe_dev->vt_enable) {
		vfe_dev->vt_enable = stream_cfg_cmd->vt_enable;
		msm_isp_start_avtimer();
	}

	if (vfe_dev->dual_vfe_sync_mode &&
		SRC_TO_INTF(stream_info->stream_src) == VFE_PIX_0)
		msm_isp_axi_reserve_comp_mask(vfe_dev, stream_info);
	else if (stream_info->num_planes > 1)
		msm_isp_axi_reserve_comp_mask(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++) {
		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_wm_reg(vfe_dev,
			stream_info, i);

		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_wm_xbar_reg(vfe_dev,
			stream_info, i);
	}
	if (stream_info->state == INACTIVE) {
		/* initialize the WM ping pong with scratch buffer */
		msm_isp_cfg_stream_scratch(stream_info, VFE_PING_FLAG);
		msm_isp_cfg_stream_scratch(stream_info, VFE_PONG_FLAG);
	}
done:
	if (rc) {
		msm_isp_axi_free_wm(vfe_dev, stream_info);
		msm_isp_axi_destroy_stream(vfe_dev, stream_info);
	}
	return rc;
}

int msm_isp_release_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i = 0;
	struct msm_vfe_axi_stream_release_cmd *stream_release_cmd = arg;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_stream_cfg_cmd stream_cfg;
	int vfe_idx;

	if (HANDLE_TO_IDX(stream_release_cmd->stream_handle) >=
		VFE_AXI_SRC_MAX) {
		pr_err("%s: Invalid stream handle\n", __func__);
		return -EINVAL;
	}
	stream_info = msm_isp_get_stream_common_data(vfe_dev,
		HANDLE_TO_IDX(stream_release_cmd->stream_handle));

	vfe_idx = msm_isp_get_vfe_idx_for_stream_user(vfe_dev, stream_info);
	if (vfe_idx == -ENOTTY ||
		stream_release_cmd->stream_handle !=
		stream_info->stream_handle[vfe_idx]) {
		pr_err("%s: Invalid stream %pK handle %x/%x vfe_idx %d vfe_dev %d num_isp %d\n",
			__func__, stream_info,
			stream_release_cmd->stream_handle,
			vfe_idx != -ENOTTY ?
			stream_info->stream_handle[vfe_idx] : 0, vfe_idx,
			vfe_dev->pdev->id, stream_info->num_isp);
		return -EINVAL;
	}

	if (stream_info->state != INACTIVE && stream_info->state != AVAILABLE) {
		stream_cfg.cmd = STOP_STREAM;
		stream_cfg.num_streams = 1;
		stream_cfg.stream_handle[0] = stream_release_cmd->stream_handle;
		msm_isp_cfg_axi_stream(vfe_dev, (void *) &stream_cfg);
	}

	for (i = 0; i < stream_info->num_planes; i++) {
		vfe_dev->hw_info->vfe_ops.axi_ops.clear_wm_reg(vfe_dev,
			stream_info, i);

		vfe_dev->hw_info->vfe_ops.axi_ops.clear_wm_xbar_reg(vfe_dev,
			stream_info, i);
	}

	if (vfe_dev->dual_vfe_sync_mode &&
		SRC_TO_INTF(stream_info->stream_src) == VFE_PIX_0)
		msm_isp_axi_free_comp_mask(vfe_dev, stream_info);
	else if (stream_info->num_planes > 1)
		msm_isp_axi_free_comp_mask(vfe_dev, stream_info);

	vfe_dev->hw_info->vfe_ops.axi_ops.clear_framedrop(vfe_dev, stream_info);
	msm_isp_axi_free_wm(vfe_dev, stream_info);

	msm_isp_axi_destroy_stream(vfe_dev, stream_info);

	return rc;
}

void msm_isp_release_all_axi_stream(struct vfe_device *vfe_dev)
{
	struct msm_vfe_axi_stream_release_cmd
			stream_release_cmd[VFE_AXI_SRC_MAX];
	struct msm_vfe_axi_stream_cfg_cmd stream_cfg_cmd;
	struct msm_vfe_axi_stream *stream_info;
	int i;
	int vfe_idx;
	int num_stream = 0;
	unsigned long flags;

	stream_cfg_cmd.cmd = STOP_STREAM;
	stream_cfg_cmd.num_streams = 0;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = msm_isp_get_stream_common_data(vfe_dev, i);
		spin_lock_irqsave(&stream_info->lock, flags);
		vfe_idx = msm_isp_get_vfe_idx_for_stream_user(
						vfe_dev, stream_info);
		if (-ENOTTY == vfe_idx) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			continue;
		}
		stream_release_cmd[num_stream++].stream_handle =
			stream_info->stream_handle[vfe_idx];
		if (stream_info->state == INACTIVE) {
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
		msm_isp_cfg_axi_stream(vfe_dev, (void *) &stream_cfg_cmd);

	for (i = 0; i < num_stream; i++)
		msm_isp_release_axi_stream(vfe_dev, &stream_release_cmd[i]);
}

static void msm_isp_axi_stream_enable_cfg(
	struct msm_vfe_axi_stream *stream_info)
{
	int enable_wm = 0;
	struct vfe_device *vfe_dev;
	struct msm_vfe_axi_shared_data *axi_data;
	uint32_t stream_idx = stream_info->stream_src;
	int k;
	int i;

	WARN_ON(stream_idx >= VFE_AXI_SRC_MAX);

	WARN_ON(stream_info->state != START_PENDING &&
		stream_info->state != RESUME_PENDING &&
		stream_info->state != STOP_PENDING &&
		stream_info->state != PAUSE_PENDING);

	if (stream_info->state == START_PENDING ||
		stream_info->state == RESUME_PENDING) {
		enable_wm = 1;
	} else {
		enable_wm = 0;
	}

	for (k = 0; k < stream_info->num_isp; k++) {
		vfe_dev = stream_info->vfe_dev[k];
		axi_data = &vfe_dev->axi_data;
		for (i = 0; i < stream_info->num_planes; i++) {
			vfe_dev->hw_info->vfe_ops.axi_ops.enable_wm(
				vfe_dev->vfe_base,
				stream_info->wm[k][i], enable_wm);
			if (enable_wm)
				continue;
			/*
			 * Issue a reg update for Raw Snapshot Case
			 * since we dont have reg update ack
			 */
			if (vfe_dev->axi_data.src_info[
				VFE_PIX_0].raw_stream_count > 0
				&& vfe_dev->axi_data.src_info[
				VFE_PIX_0].stream_count == 0) {
				if (stream_info->stream_src == CAMIF_RAW ||
					stream_info->stream_src == IDEAL_RAW) {
					vfe_dev->hw_info->vfe_ops.core_ops
						.reg_update(vfe_dev,
							VFE_PIX_0);
				}
			}
		}
		if (stream_info->state == START_PENDING)
			axi_data->num_active_stream++;
		else if (stream_info->state == STOP_PENDING)
			axi_data->num_active_stream--;
	}
}

static void msm_isp_free_pending_buffer(
			struct vfe_device *vfe_dev,
			struct msm_vfe_axi_stream *stream_info,
			struct msm_isp_timestamp *ts)
{
	struct timeval *time_stamp;
	struct msm_isp_buffer *done_buf = NULL;
	uint32_t frame_id;
	int rc;

	if (!stream_info->controllable_output ||
		!stream_info->pending_buf_info.is_buf_done_pending)	{
		return;
	}

	if (vfe_dev->vt_enable) {
		msm_isp_get_avtimer_ts(ts);
		time_stamp = &ts->vt_time;
	} else {
		time_stamp = &ts->buf_time;
	}

	done_buf = stream_info->pending_buf_info.buf;
	frame_id = stream_info->pending_buf_info.frame_id;
	if (done_buf) {
		rc = msm_isp_process_done_buf(vfe_dev, stream_info,
			done_buf, time_stamp, frame_id);
		if (rc == 0) {
			stream_info->pending_buf_info.buf = NULL;
			stream_info->pending_buf_info.is_buf_done_pending = 0;
		}
	}
}

static void __msm_isp_axi_stream_update(
			struct msm_vfe_axi_stream *stream_info,
			struct msm_isp_timestamp *ts)
{
	int j;
	int intf = SRC_TO_INTF(stream_info->stream_src);
	struct vfe_device *vfe_dev;
	int k;

	switch (stream_info->state) {
	case UPDATING:
		stream_info->state = ACTIVE;
		complete_all(&stream_info->active_comp);
		break;
	case STOP_PENDING:
		msm_isp_axi_stream_enable_cfg(stream_info);
		stream_info->state = STOPPING;
		break;
	case START_PENDING:
		msm_isp_axi_stream_enable_cfg(stream_info);
		stream_info->state = STARTING;
		break;
	case STOPPING:
		stream_info->state = INACTIVE;
		for (k = 0; k < MSM_ISP_COMP_IRQ_MAX; k++)
			stream_info->composite_irq[k] = 0;
		complete_all(&stream_info->inactive_comp);
		break;
	case STARTING:
		stream_info->state = ACTIVE;
		complete_all(&stream_info->active_comp);
		break;
	case PAUSING:
		stream_info->state = PAUSED;
		msm_isp_reload_ping_pong_offset(stream_info);
		for (j = 0; j < stream_info->num_planes; j++) {
			for (k = 0; k < stream_info->num_isp; k++) {
				vfe_dev = stream_info->vfe_dev[k];
				vfe_dev->hw_info->vfe_ops.axi_ops.cfg_wm_reg(
					vfe_dev, stream_info, j);
			}
		}
		stream_info->state = RESUME_PENDING;
		msm_isp_axi_stream_enable_cfg(stream_info);
		stream_info->state = RESUMING;
		break;
	case RESUMING:
		stream_info->runtime_output_format = stream_info->output_format;
		stream_info->state = ACTIVE;
		complete_all(&stream_info->active_comp);
		for (j = 0; j < stream_info->num_isp; j++) {
			/* notify that all streams have been updated */
			msm_isp_notify(stream_info->vfe_dev[j],
				ISP_EVENT_STREAM_UPDATE_DONE, intf, ts);
			atomic_set(&stream_info->vfe_dev[
				j]->axi_data.axi_cfg_update[intf], 0);
		}
		stream_info->update_vfe_mask = 0;
		break;
	default:
		break;
	}
}

void msm_isp_axi_stream_update(struct vfe_device *vfe_dev,
	enum msm_vfe_input_src frame_src,
	struct msm_isp_timestamp *ts)
{
	int i;
	unsigned long flags;
	struct msm_vfe_axi_stream *stream_info;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = msm_isp_get_stream_common_data(vfe_dev, i);
		if (SRC_TO_INTF(stream_info->stream_src) !=
			frame_src) {
			ISP_DBG("%s stream_src %d frame_src %d\n", __func__,
				SRC_TO_INTF(
				stream_info->stream_src),
				frame_src);
			continue;
		}
		if (stream_info->state == AVAILABLE)
			continue;
		spin_lock_irqsave(&stream_info->lock, flags);
		__msm_isp_axi_stream_update(stream_info, ts);
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}
}

static void msm_isp_reload_ping_pong_offset(
		struct msm_vfe_axi_stream *stream_info)
{
	int i, j;
	uint32_t bit;
	struct msm_isp_buffer *buf;
	int32_t buf_size_byte = 0;
	int32_t word_per_line = 0;
	int k;
	struct vfe_device *vfe_dev;

	for (k = 0; k < stream_info->num_isp; k++) {
		vfe_dev = stream_info->vfe_dev[k];
		for (i = 0; i < 2; i++) {
			buf = stream_info->buf[i];
			if (!buf)
				continue;

			bit = i ? 0 : 1;

			for (j = 0; j < stream_info->num_planes; j++) {
				word_per_line = msm_isp_cal_word_per_line(
				stream_info->output_format,
				stream_info->plane_cfg[k][j].output_stride);
				if (word_per_line < 0) {
					/* 0 means no prefetch*/
					word_per_line = 0;
					buf_size_byte = 0;
				} else {
					buf_size_byte = (word_per_line * 8 *
					stream_info->plane_cfg[
					k][j].output_scan_lines) -
					stream_info->plane_cfg[k][
					j].plane_addr_offset;
				}

				vfe_dev->hw_info->vfe_ops.axi_ops
					.update_ping_pong_addr(
					vfe_dev->vfe_base,
					stream_info->wm[k][j],
					bit,
					buf->mapped_info[j].paddr +
					stream_info->plane_cfg[k][
						j].plane_addr_offset,
					buf_size_byte);
			}
		}
	}
}

static int msm_isp_update_deliver_count(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_bit,
	struct msm_isp_buffer *done_buf)
{
	int rc = 0;

	if (!stream_info->controllable_output)
		goto done;

	if (!stream_info->undelivered_request_cnt ||
		(done_buf == NULL)) {
		pr_err_ratelimited("%s:%d error undelivered_request_cnt 0\n",
			__func__, __LINE__);
		rc = -EINVAL;
		goto done;
	} else {
		if ((done_buf->is_drop_reconfig == 1) &&
			(stream_info->sw_ping_pong_bit == -1)) {
			goto done;
		}
		/*After wm reload, we get bufdone for ping buffer*/
		if (stream_info->sw_ping_pong_bit == -1)
			stream_info->sw_ping_pong_bit = 0;
		if (done_buf->is_drop_reconfig != 1)
			stream_info->undelivered_request_cnt--;
		if (pingpong_bit != stream_info->sw_ping_pong_bit) {
			pr_err("%s:%d ping pong bit actual %d sw %d\n",
				__func__, __LINE__, pingpong_bit,
				stream_info->sw_ping_pong_bit);
			rc = -EINVAL;
			goto done;
		}
		stream_info->sw_ping_pong_bit ^= 1;
	}
done:
	return rc;
}

void msm_isp_halt_send_error(struct vfe_device *vfe_dev, uint32_t event)
{
	struct msm_isp_event_data error_event;
	struct msm_vfe_axi_halt_cmd halt_cmd;
	struct vfe_device *temp_dev = NULL;
	uint32_t irq_status0 = 0, irq_status1 = 0;
	struct vfe_device *vfe_dev_other = NULL;
	uint32_t vfe_id_other = 0;
	unsigned long flags;

	if (atomic_read(&vfe_dev->error_info.overflow_state) !=
		NO_OVERFLOW)
		/* Recovery is already in Progress */
		return;

	/* if there are no active streams - do not start recovery */
	if (!vfe_dev->axi_data.num_active_stream)
		return;

	/* if there are no active streams - do not start recovery */
	if (vfe_dev->is_split) {
		if (vfe_dev->pdev->id == ISP_VFE0)
			vfe_id_other = ISP_VFE1;
		else
			vfe_id_other = ISP_VFE0;

		spin_lock_irqsave(
			&vfe_dev->common_data->common_dev_data_lock, flags);
		vfe_dev_other = vfe_dev->common_data->dual_vfe_res->vfe_dev[
			vfe_id_other];
		if (!vfe_dev->axi_data.num_active_stream ||
			!vfe_dev_other->axi_data.num_active_stream) {
			spin_unlock_irqrestore(
				&vfe_dev->common_data->common_dev_data_lock,
				flags);
			pr_err("%s:skip the recovery as no active streams\n",
				 __func__);
			return;
		}
		spin_unlock_irqrestore(
			&vfe_dev->common_data->common_dev_data_lock, flags);
	} else if (!vfe_dev->axi_data.num_active_stream)
		return;

	if (event == ISP_EVENT_PING_PONG_MISMATCH &&
		vfe_dev->axi_data.recovery_count < MAX_RECOVERY_THRESHOLD) {
		pr_err("%s: ping pong mismatch on vfe%d recovery count %d\n",
			__func__, vfe_dev->pdev->id,
			vfe_dev->axi_data.recovery_count);
		msm_isp_process_overflow_irq(vfe_dev,
			&irq_status0, &irq_status1, 1);
		vfe_dev->axi_data.recovery_count++;
		return;
	}
	memset(&halt_cmd, 0, sizeof(struct msm_vfe_axi_halt_cmd));
	memset(&error_event, 0, sizeof(struct msm_isp_event_data));
	halt_cmd.stop_camif = 1;
	halt_cmd.overflow_detected = 0;
	halt_cmd.blocking_halt = 0;

	pr_err("%s: vfe%d fatal error!\n", __func__, vfe_dev->pdev->id);

	atomic_set(&vfe_dev->error_info.overflow_state,
		HALT_ENFORCED);

	vfe_dev->hw_info->vfe_ops.core_ops.set_halt_restart_mask(vfe_dev);
	if (vfe_dev->is_split) {
		int other_vfe_id = (vfe_dev->pdev->id == ISP_VFE0 ?
					ISP_VFE1 : ISP_VFE0);
		temp_dev = vfe_dev->common_data->dual_vfe_res->vfe_dev[
			other_vfe_id];
		atomic_set(&temp_dev->error_info.overflow_state,
			HALT_ENFORCED);
		temp_dev->hw_info->vfe_ops.core_ops.set_halt_restart_mask(
			temp_dev);
	}
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
	int k;

	for (j = 0; j < VFE_AXI_SRC_MAX; j++) {
		stream_info = msm_isp_get_stream_common_data(vfe_dev, j);
		if (stream_info->state == INACTIVE ||
			stream_info->state == AVAILABLE)
			continue;

		for (pingpong_bit = 0; pingpong_bit < 2; pingpong_bit++) {
			dma_addr_t temp;

			buf = stream_info->buf[pingpong_bit];
			if (buf == NULL) {
				pr_err("%s: buf NULL for stream %x num_isp %d\n",
					__func__,
					stream_info->stream_src,
					stream_info->num_isp);
				continue;
			}
			temp = buf->mapped_info[0].paddr +
				buf->mapped_info[0].len;
			pr_err("%s: stream %x ping bit %d uses buffer %pK-%pK, num_isp %d\n",
				__func__, stream_info->stream_src,
				pingpong_bit,
				&buf->mapped_info[0].paddr, &temp,
				stream_info->num_isp);

			for (i = 0; i < stream_info->num_planes; i++) {
				for (k = 0; k < stream_info->num_isp; k++) {
					pr_debug(
					"%s: stream_id %x ping-pong %d	plane %d start_addr %pK	addr_offset %x len %zx stride %d scanline %d\n"
					, __func__, stream_info->stream_id,
					pingpong_bit, i,
					(void *)buf->mapped_info[i].paddr,
					stream_info->plane_cfg[k][
					i].plane_addr_offset,
					buf->mapped_info[i].len,
					stream_info->plane_cfg[k][
					i].output_stride,
					stream_info->plane_cfg[k][
					i].output_scan_lines
					);
				}
			}
		}
	}

	return 0;
}

static struct msm_isp_buffer *msm_isp_get_stream_buffer(
			struct vfe_device *vfe_dev,
			struct msm_vfe_axi_stream *stream_info)
{
	int rc = 0;
	uint32_t bufq_handle = 0;
	struct msm_isp_buffer *buf = NULL;
	struct msm_vfe_frame_request_queue *queue_req;
	uint32_t buf_index = MSM_ISP_INVALID_BUF_INDEX;

	if (!stream_info->controllable_output) {
		bufq_handle = stream_info->bufq_handle
					[VFE_BUF_QUEUE_DEFAULT];
	} else {
		queue_req = list_first_entry_or_null(
			&stream_info->request_q,
			struct msm_vfe_frame_request_queue, list);
		if (!queue_req)
			return buf;

		bufq_handle = stream_info->bufq_handle
					[queue_req->buff_queue_id];

		if (!bufq_handle ||
			stream_info->request_q_cnt <= 0) {
			pr_err_ratelimited("%s: Drop request. Shared stream is stopped.\n",
			__func__);
			return buf;
		}
		buf_index = queue_req->buf_index;
		queue_req->cmd_used = 0;
		list_del(&queue_req->list);
		stream_info->request_q_cnt--;
	}
	rc = vfe_dev->buf_mgr->ops->get_buf(vfe_dev->buf_mgr,
		vfe_dev->pdev->id, bufq_handle, buf_index, &buf);

	if (rc == -EFAULT) {
		msm_isp_halt_send_error(vfe_dev,
			ISP_EVENT_BUF_FATAL_ERROR);
		return buf;
	}
	if (rc < 0)
		return buf;

	if (buf->num_planes != stream_info->num_planes) {
		pr_err("%s: Invalid buffer\n", __func__);
		vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
				bufq_handle, buf->buf_idx);
		buf = NULL;
	}
	return buf;
}

int msm_isp_cfg_offline_ping_pong_address(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	uint32_t buf_idx)
{
	int i, rc = 0;
	struct msm_isp_buffer *buf = NULL;
	uint32_t pingpong_bit;
	uint32_t buffer_size_byte = 0;
	int32_t word_per_line = 0;
	dma_addr_t paddr;
	uint32_t bufq_handle = 0;
	int vfe_idx;

	bufq_handle = stream_info->bufq_handle[VFE_BUF_QUEUE_DEFAULT];

	if (!vfe_dev->is_split) {
		rc = vfe_dev->buf_mgr->ops->get_buf_by_index(
			vfe_dev->buf_mgr, bufq_handle, buf_idx, &buf);
		if (rc < 0 || !buf) {
			pr_err("%s: No fetch buffer rc= %d buf= %pK\n",
				__func__, rc, buf);
			return -EINVAL;
		}

		if (buf->num_planes != stream_info->num_planes) {
			pr_err("%s: Invalid buffer\n", __func__);
			vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
				bufq_handle, buf->buf_idx);
			return -EINVAL;
		}
		vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
		pingpong_bit = ((pingpong_status >>
			stream_info->wm[vfe_idx][0]) & 0x1);

		for (i = 0; i < stream_info->num_planes; i++) {
			word_per_line = msm_isp_cal_word_per_line(
				stream_info->output_format,
				stream_info->plane_cfg[vfe_idx][
					i].output_stride);
			if (word_per_line < 0) {
				/* 0 means no prefetch*/
				word_per_line = 0;
				buffer_size_byte = 0;
			} else {
				buffer_size_byte = (word_per_line * 8 *
					stream_info->plane_cfg[vfe_idx][
						i].output_scan_lines) -
					stream_info->plane_cfg[vfe_idx][
						i].plane_addr_offset;
			}
			paddr = buf->mapped_info[i].paddr;

			vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
				vfe_dev->vfe_base, stream_info->wm[vfe_idx][i],
				pingpong_bit, paddr +
				stream_info->plane_cfg[vfe_idx][
				i].plane_addr_offset,
				buffer_size_byte);
				stream_info->buf[!pingpong_bit] = buf;
				buf->pingpong_bit = !pingpong_bit;
		}
		buf->state = MSM_ISP_BUFFER_STATE_DEQUEUED;
		stream_info->buf[!pingpong_bit] = buf;
		buf->pingpong_bit = !pingpong_bit;
	}
	return rc;

}

static int msm_isp_cfg_ping_pong_address(
	struct msm_vfe_axi_stream *stream_info, uint32_t pingpong_status,
	struct msm_isp_buffer *buf)
{
	int i;
	int j;
	uint32_t pingpong_bit;
	struct vfe_device *vfe_dev = stream_info->vfe_dev[0];
	uint32_t buffer_size_byte = 0;
	int32_t word_per_line = 0;
	dma_addr_t paddr;


	/* Isolate pingpong_bit from pingpong_status */
	pingpong_bit = ((pingpong_status >>
		stream_info->wm[0][0]) & 0x1);

	/* return if buffer already present */
	if (stream_info->buf[!pingpong_bit]) {
		pr_err("stream %x buffer already set for pingpong %d\n",
			stream_info->stream_src, !pingpong_bit);
		return 1;
	}

	if (buf == NULL)
		buf = msm_isp_get_stream_buffer(vfe_dev, stream_info);

	if (!buf) {
		msm_isp_cfg_stream_scratch(stream_info, pingpong_status);
		if (stream_info->controllable_output)
			return 1;
		return 0;
	}

	for (i = 0; i < stream_info->num_planes; i++) {
		paddr = buf->mapped_info[i].paddr;
		ISP_DBG(
			"%s: vfe %d config buf %d to pingpong %d stream %x\n",
			__func__, vfe_dev->pdev->id,
			buf->buf_idx, !pingpong_bit,
			stream_info->stream_id);
		for (j = 0; j < stream_info->num_isp; j++) {
			vfe_dev = stream_info->vfe_dev[j];
			word_per_line =
				msm_isp_cal_word_per_line(
				stream_info->output_format,
				stream_info->plane_cfg[j][i].output_stride);
			if (word_per_line < 0) {
				/* 0 means no prefetch*/
				word_per_line = 0;
				buffer_size_byte = 0;
			} else {
				buffer_size_byte =
					(word_per_line * 8 *
					stream_info->plane_cfg[j][
					i].output_scan_lines) -
					stream_info->plane_cfg[j][
					i].plane_addr_offset;
			}
			vfe_dev->hw_info->vfe_ops.axi_ops.update_ping_pong_addr(
					vfe_dev->vfe_base,
					stream_info->wm[j][i],
					pingpong_bit, paddr +
					stream_info->plane_cfg[j][
					i].plane_addr_offset,
					buffer_size_byte);
		}
	}
	stream_info->buf[!pingpong_bit] = buf;
	buf->pingpong_bit = !pingpong_bit;
	return 0;
}

static void msm_isp_handle_done_buf_frame_id_mismatch(
	struct vfe_device *vfe_dev, struct msm_vfe_axi_stream *stream_info,
	struct msm_isp_buffer *buf, struct timeval *time_stamp,
	uint32_t frame_id)
{
	struct msm_isp_event_data error_event;
	int ret = 0;

	memset(&error_event, 0, sizeof(error_event));
	error_event.frame_id =
		vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
	error_event.u.error_info.err_type =
		ISP_ERROR_FRAME_ID_MISMATCH;
	if (stream_info->buf_divert)
		vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx);
	else
		ret = vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx, time_stamp,
			frame_id,
			stream_info->runtime_output_format);
	if (ret == -EFAULT) {
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_BUF_FATAL_ERROR);
		return;
	}
	msm_isp_send_event(vfe_dev, ISP_EVENT_ERROR,
		&error_event);
	pr_err("%s: Error! frame id mismatch!! 1st buf frame %d,curr frame %d\n",
		__func__, buf->frame_id, frame_id);
	vfe_dev->buf_mgr->frameId_mismatch_recovery = 1;
}

static int msm_isp_process_done_buf(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, struct msm_isp_buffer *buf,
	struct timeval *time_stamp, uint32_t frame_id)
{
	int rc;
	unsigned long flags;
	struct msm_isp_event_data buf_event;
	uint32_t stream_idx = stream_info->stream_src;
	uint32_t buf_src;
	uint8_t drop_frame = 0;
	struct msm_isp_bufq *bufq = NULL;
	memset(&buf_event, 0, sizeof(buf_event));

	if (stream_idx >= VFE_AXI_SRC_MAX) {
		pr_err_ratelimited("%s: Invalid stream_idx", __func__);
		return -EINVAL;
	}

	if (SRC_TO_INTF(stream_info->stream_src) >= VFE_SRC_MAX) {
		pr_err_ratelimited("%s: Invalid stream index, put buf back to vb2 queue\n",
			__func__);
		rc = vfe_dev->buf_mgr->ops->put_buf(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx);
		return -EINVAL;
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
	rc = vfe_dev->buf_mgr->ops->get_buf_src(vfe_dev->buf_mgr,
					buf->bufq_handle, &buf_src);
	if (rc != 0) {
		pr_err_ratelimited("%s: Error getting buf_src\n", __func__);
		return -EINVAL;
	}

	if (drop_frame) {
		buf->buf_debug.put_state[
			buf->buf_debug.put_state_last] =
			MSM_ISP_BUFFER_STATE_DROP_SKIP;
		buf->buf_debug.put_state_last ^= 1;
		if (stream_info->buf_divert)
			vfe_dev->buf_mgr->ops->put_buf(
				vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx);
		else
			rc = vfe_dev->buf_mgr->ops->buf_done(
				vfe_dev->buf_mgr,
				buf->bufq_handle, buf->buf_idx,
				time_stamp, frame_id,
				stream_info->runtime_output_format);

		if (rc == -EFAULT) {
			msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_BUF_FATAL_ERROR);
			return rc;
		}
		if (!rc) {
			ISP_DBG("%s:%d vfe_id %d Buffer dropped %d\n",
				__func__, __LINE__, vfe_dev->pdev->id,
				frame_id);
			/*
			 * Return rc which is 0 at this point so that
			 * we can cfg ping pong and we can continue
			 * streaming
			 */
			return rc;
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
	if (vfe_dev->fetch_engine_info.is_busy &&
		SRC_TO_INTF(stream_info->stream_src) == VFE_PIX_0) {
		vfe_dev->fetch_engine_info.is_busy = 0;
	}

	if (stream_info->buf_divert &&
		buf_src != MSM_ISP_BUFFER_SRC_SCRATCH) {

		bufq = vfe_dev->buf_mgr->ops->get_bufq(vfe_dev->buf_mgr,
			buf->bufq_handle);
		if (!bufq) {
			pr_err("%s: Invalid bufq buf_handle %x\n",
				__func__, buf->bufq_handle);
			return -EINVAL;
		}

		/* divert native buffers */
		vfe_dev->buf_mgr->ops->buf_divert(vfe_dev->buf_mgr,
			buf->bufq_handle, buf->buf_idx, time_stamp,
			frame_id);

		if ((bufq != NULL) && bufq->buf_type == ISP_SHARE_BUF)
			msm_isp_send_event(
				vfe_dev->common_data->dual_vfe_res->vfe_dev[
				ISP_VFE1], ISP_EVENT_BUF_DIVERT, &buf_event);
		else
			msm_isp_send_event(vfe_dev,
			ISP_EVENT_BUF_DIVERT, &buf_event);
	} else {
		ISP_DBG("%s: vfe_id %d send buf done buf-id %d bufq %x\n",
			__func__, vfe_dev->pdev->id, buf->buf_idx,
			buf->bufq_handle);
		msm_isp_send_event(vfe_dev, ISP_EVENT_BUF_DONE,
			&buf_event);
		buf->buf_debug.put_state[
			buf->buf_debug.put_state_last] =
			MSM_ISP_BUFFER_STATE_PUT_BUF;
		buf->buf_debug.put_state_last ^= 1;
		rc = vfe_dev->buf_mgr->ops->buf_done(vfe_dev->buf_mgr,
		 buf->bufq_handle, buf->buf_idx, time_stamp,
		 frame_id, stream_info->runtime_output_format);
		if (rc == -EFAULT) {
			msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_BUF_FATAL_ERROR);
			return rc;
		}
	}

	return 0;
}

int msm_isp_drop_frame(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, struct msm_isp_timestamp *ts,
	struct msm_isp_sof_info *sof_info)
{
	struct msm_isp_buffer *done_buf = NULL;
	uint32_t pingpong_status;
	unsigned long flags;
	struct msm_isp_bufq *bufq = NULL;
	uint32_t pingpong_bit;
	int vfe_idx;
	int rc = -1;

	if (!vfe_dev || !stream_info || !ts || !sof_info) {
		pr_err("%s %d vfe_dev %pK stream_info %pK ts %pK op_info %pK\n",
			 __func__, __LINE__, vfe_dev, stream_info, ts,
			sof_info);
		return -EINVAL;
	}
	vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	pingpong_status =
		~vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(vfe_dev);

	spin_lock_irqsave(&stream_info->lock, flags);
	pingpong_bit =
		(~(pingpong_status >> stream_info->wm[vfe_idx][0]) & 0x1);
	done_buf = stream_info->buf[pingpong_bit];
	if (done_buf &&
		(stream_info->composite_irq[MSM_ISP_COMP_IRQ_EPOCH] == 0)) {
		if ((stream_info->sw_ping_pong_bit != -1) &&
			!vfe_dev->reg_updated) {
			rc = msm_isp_cfg_ping_pong_address(
				stream_info, ~pingpong_status, done_buf);
			if (rc < 0) {
				ISP_DBG("%s: Error configuring ping_pong\n",
					__func__);
				bufq = vfe_dev->buf_mgr->ops->get_bufq(
					vfe_dev->buf_mgr,
					done_buf->bufq_handle);
				if (!bufq) {
					spin_unlock_irqrestore(
						&stream_info->lock,
						flags);
					pr_err("%s: Invalid bufq buf_handle %x\n",
						__func__,
						done_buf->bufq_handle);
					return -EINVAL;
				}
				sof_info->reg_update_fail_mask_ext |=
					(bufq->bufq_handle & 0xFF);
			}
		}
		/*Avoid Drop Frame and re-issue pingpong cfg*/
		/*this notify is per ping and pong buffer*/
		done_buf->is_drop_reconfig = 1;
		stream_info->current_framedrop_period = 1;
		/*Avoid Multiple request frames for single SOF*/
		vfe_dev->axi_data.src_info[VFE_PIX_0].accept_frame = false;

		if (stream_info->current_framedrop_period !=
			stream_info->requested_framedrop_period) {
			msm_isp_cfg_framedrop_reg(stream_info);
		}
	}
	spin_unlock_irqrestore(&stream_info->lock, flags);

	/* if buf done will not come, we need to process it ourself */
	if (stream_info->activated_framedrop_period ==
		MSM_VFE_STREAM_STOP_PERIOD) {
		/* no buf done come */
		msm_isp_process_axi_irq_stream(vfe_dev, stream_info,
			pingpong_status, ts);
		if (done_buf)
			done_buf->is_drop_reconfig = 0;
	}
	return 0;
}

/**
 * msm_isp_input_disable() - Disable the input for given vfe
 * @vfe_dev: The vfe device whose input is to be disabled
 *
 * Returns - void
 *
 * If stream count on an input line is 0 then disable the input
 */
static void msm_isp_input_disable(struct vfe_device *vfe_dev, int cmd_type)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int stream_count;
	int total_stream_count = 0;
	int i;
	struct msm_vfe_src_info *src_info;
	struct msm_vfe_hardware_info *hw_info;
	int ext_read =
		(axi_data->src_info[VFE_PIX_0].input_mux == EXTERNAL_READ);

	for (i = 0; i < VFE_SRC_MAX; i++)
		total_stream_count += axi_data->src_info[i].stream_count +
				axi_data->src_info[i].raw_stream_count;

	for (i = 0; i < VFE_SRC_MAX; i++) {
		stream_count = axi_data->src_info[i].stream_count +
				axi_data->src_info[i].raw_stream_count;
		if (stream_count)
			continue;
		if (axi_data->src_info[i].active == 0)
			continue;
		/* deactivate the input line */
		axi_data->src_info[i].active = 0;
		src_info = &axi_data->src_info[i];

		if (src_info->dual_hw_type == DUAL_HW_MASTER_SLAVE) {
			struct master_slave_resource_info *ms_res =
				&vfe_dev->common_data->ms_resource;
			unsigned long flags;

			spin_lock_irqsave(
				&vfe_dev->common_data->common_dev_data_lock,
				flags);
			if (src_info->dual_hw_ms_info.index ==
				ms_res->master_index)
				ms_res->master_index = -1;
			if (src_info->dual_hw_ms_info.index ==
				ms_res->primary_slv_idx)
				ms_res->primary_slv_idx = -1;
			ms_res->active_src_mask &= ~(1 <<
				src_info->dual_hw_ms_info.index);
			ms_res->src_sof_mask &= ~(1 <<
				src_info->dual_hw_ms_info.index);
			ms_res->src_info[src_info->dual_hw_ms_info.index] =
				NULL;
			ms_res->num_src--;
			if (ms_res->num_src == 0)
				ms_res->dual_sync_mode = MSM_ISP_DUAL_CAM_ASYNC;
			src_info->dual_hw_ms_info.sync_state =
						MSM_ISP_DUAL_CAM_ASYNC;
			src_info->dual_hw_type = DUAL_NONE;
			src_info->dual_hw_ms_info.index = -1;
			spin_unlock_irqrestore(
				&vfe_dev->common_data->common_dev_data_lock,
				flags);
		}
		if (i != VFE_PIX_0 || ext_read)
			continue;
		if (total_stream_count == 0 || cmd_type == STOP_IMMEDIATELY)
			vfe_dev->hw_info->vfe_ops.core_ops.update_camif_state(
				vfe_dev, DISABLE_CAMIF_IMMEDIATELY);
		else
			vfe_dev->hw_info->vfe_ops.core_ops.update_camif_state(
				vfe_dev, DISABLE_CAMIF);
	}
	hw_info = vfe_dev->hw_info;
	/*
	 * halt and reset hardware if all streams are disabled, in this case
	 * ispif is halted immediately as well
	 */
	if (total_stream_count == 0) {
		vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev, 1);
		msm_isp_flush_tasklet(vfe_dev);
		vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev, 0, 1);
		if (msm_vfe_is_vfe48(vfe_dev))
			vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev,
								0, 1);
		vfe_dev->hw_info->vfe_ops.core_ops.init_hw_reg(vfe_dev);

		if (vfe_dev->dual_vfe_sync_mode)
			hw_info->vfe_ops.platform_ops.clear_dual_vfe_mode(
				vfe_dev);
	}

}

/**
 * msm_isp_input_enable() - Enable the input for given vfe
 * @vfe_dev: The vfe device whose input is to be enabled
 *
 * Returns - void
 *
 * Enable inout line if it is not enabled
 */
static void msm_isp_input_enable(struct vfe_device *vfe_dev,
				int sync_frame_id_src)
{
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int ext_read =
		(axi_data->src_info[VFE_PIX_0].input_mux == EXTERNAL_READ);
	int stream_count;
	int i;

	/* if dual vfe mode is enabled and split needed, set dual vfe mode */
	if (vfe_dev->dual_vfe_sync_mode) {
		vfe_dev->hw_info->vfe_ops.platform_ops.set_dual_vfe_mode(
			vfe_dev);
		ISP_DBG("%s: set dual vfe sync mode %d enable %d\n",
		__func__, vfe_dev->dual_vfe_sync_mode,
		vfe_dev->dual_vfe_sync_enable);
	}

	for (i = 0; i < VFE_SRC_MAX; i++) {
		stream_count = axi_data->src_info[i].stream_count +
				axi_data->src_info[i].raw_stream_count;
		if (stream_count == 0)
			continue;
		if (axi_data->src_info[i].active)
			continue;
		/* activate the input since it is deactivated */
		axi_data->src_info[i].frame_id = 0;
		vfe_dev->irq_sof_id = 0;
		if (axi_data->src_info[i].input_mux != EXTERNAL_READ)
			axi_data->src_info[i].active = 1;
		if (i >= VFE_RAW_0 && sync_frame_id_src) {
			/*
			 * Incase PIX and RDI streams are part
			 * of same session, this will ensure
			 * RDI stream will have same frame id
			 * as of PIX stream
			 */
			axi_data->src_info[i].frame_id =
				axi_data->src_info[VFE_PIX_0].frame_id;
		}
		/* when start reset overflow state and cfg ub for this intf */
		vfe_dev->hw_info->vfe_ops.axi_ops.cfg_ub(vfe_dev, i);
		atomic_set(&vfe_dev->error_info.overflow_state,
			NO_OVERFLOW);
		if (i != VFE_PIX_0 || ext_read)
			continue;
		/* for camif input the camif needs enabling */
		vfe_dev->hw_info->vfe_ops.core_ops.update_camif_state(vfe_dev,
			ENABLE_CAMIF);
	}
}

/**
 * msm_isp_update_intf_stream_cnt() - Update the stream count in axi interface
 * @stream_info: The stream that is either being enabled/disabled
 * @enable: 0 means stream is being disabled, else enabled
 *
 * Returns - void
 */
static void msm_isp_update_intf_stream_cnt(
	struct msm_vfe_axi_stream *stream_info,
	int enable)
{
	int i;

	switch (stream_info->stream_src) {
	case PIX_ENCODER:
	case PIX_VIEWFINDER:
	case PIX_VIDEO:
	case IDEAL_RAW:
	case RDI_INTF_0:
	case RDI_INTF_1:
	case RDI_INTF_2:
		for (i = 0; i < stream_info->num_isp; i++) {
			if (enable)
				stream_info->vfe_dev[i]->axi_data.src_info[
					SRC_TO_INTF(stream_info->stream_src)].
					stream_count++;
			else
				stream_info->vfe_dev[i]->axi_data.src_info[
					SRC_TO_INTF(stream_info->stream_src)].
					stream_count--;
		}
		break;
	case CAMIF_RAW:
		for (i = 0; i < stream_info->num_isp; i++) {
			if (enable)
				stream_info->vfe_dev[i]->axi_data.src_info[
					SRC_TO_INTF(stream_info->stream_src)].
					raw_stream_count++;
			else
				stream_info->vfe_dev[i]->axi_data.src_info[
					SRC_TO_INTF(stream_info->stream_src)].
					raw_stream_count--;
		}
		break;
	default:
		WARN(1, "Invalid steam src %d\n", stream_info->stream_src);
	}
}

/*Factor in Q2 format*/
#define ISP_DEFAULT_FORMAT_FACTOR 6
#define ISP_BUS_UTILIZATION_FACTOR 6
static int msm_isp_update_stream_bandwidth(
	struct msm_vfe_axi_stream *stream_info, int enable)
{
	int i, rc = 0;
	uint64_t total_bandwidth = 0;
	int vfe_idx;
	struct vfe_device *vfe_dev;

	for (i = 0; i < stream_info->num_isp; i++) {
		vfe_dev = stream_info->vfe_dev[i];
		vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev,
					stream_info);
		if (enable) {
			total_bandwidth =
				vfe_dev->total_bandwidth +
				stream_info->bandwidth[vfe_idx];
		} else {
			total_bandwidth = vfe_dev->total_bandwidth -
				stream_info->bandwidth[vfe_idx];
		}
		vfe_dev->total_bandwidth = total_bandwidth;
		rc = msm_isp_update_bandwidth(ISP_VFE0 + vfe_dev->pdev->id,
			(total_bandwidth + vfe_dev->hw_info->min_ab),
			(total_bandwidth + vfe_dev->hw_info->min_ib));

		if (rc < 0)
			pr_err("%s: update failed rc %d stream src %d vfe dev %d\n",
				__func__, rc, stream_info->stream_src,
				vfe_dev->pdev->id);
	}
	return rc;
}

int msm_isp_ab_ib_update_lpm_mode(struct vfe_device *vfe_dev, void *arg)
{
	int i, rc = 0;
	uint32_t intf;
	unsigned long flags;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_dual_lpm_mode *ab_ib_vote = NULL;

	ab_ib_vote = (struct msm_vfe_dual_lpm_mode *)arg;
	if (!ab_ib_vote) {
		pr_err("%s: ab_ib_vote is NULL !!!\n", __func__);
		rc = -1;
		return rc;
	}
	if (ab_ib_vote->num_src >= VFE_AXI_SRC_MAX) {
		pr_err("%s: ab_ib_vote num_src is exceeding limit\n",
			__func__);
		rc = -1;
		return rc;
	}
	if (ab_ib_vote->num_src >= VFE_AXI_SRC_MAX) {
		pr_err("%s: ab_ib_vote num_src is exceeding limit\n",
			__func__);
		rc = -1;
		return rc;
	}
	if (ab_ib_vote->lpm_mode) {
		for (i = 0; i < ab_ib_vote->num_src; i++) {
			stream_info =
				msm_isp_get_stream_common_data(vfe_dev,
					ab_ib_vote->stream_src[i]);
			if (stream_info == NULL)
				continue;
			/* loop all stream on current session */
			spin_lock_irqsave(&stream_info->lock, flags);
			intf = SRC_TO_INTF(stream_info->stream_src);
			vfe_dev->axi_data.src_info[intf].lpm =
				ab_ib_vote->lpm_mode;
			if (stream_info->lpm_mode ||
				stream_info->state == INACTIVE) {
				spin_unlock_irqrestore(&stream_info->lock,
							flags);
				continue;
			}
			stream_info->lpm_mode = ab_ib_vote->lpm_mode;
			spin_unlock_irqrestore(&stream_info->lock, flags);
			msm_isp_update_stream_bandwidth(stream_info, 0);
		}
	} else {
		for (i = 0; i < ab_ib_vote->num_src; i++) {
			stream_info =
				msm_isp_get_stream_common_data(vfe_dev,
					ab_ib_vote->stream_src[i]);
			if (stream_info == NULL)
				continue;
			spin_lock_irqsave(&stream_info->lock, flags);
			intf = SRC_TO_INTF(stream_info->stream_src);
			vfe_dev->axi_data.src_info[intf].lpm =
				ab_ib_vote->lpm_mode;
			if (stream_info->lpm_mode == 0 ||
				stream_info->state == INACTIVE) {
				spin_unlock_irqrestore(&stream_info->lock,
							flags);
				continue;
			}
			stream_info->lpm_mode = 0;
			spin_unlock_irqrestore(&stream_info->lock, flags);
			msm_isp_update_stream_bandwidth(stream_info, 1);
		}
	}
	return rc;
}

static int msm_isp_init_stream_ping_pong_reg(
	struct msm_vfe_axi_stream *stream_info)
{
	int rc = 0;

	/* Set address for both PING & PO NG register */
	rc = msm_isp_cfg_ping_pong_address(
		stream_info, VFE_PING_FLAG, NULL);
	/* No buffer available on start is not error */
	if (rc == -ENOMEM && stream_info->stream_type != BURST_STREAM)
		return 0;
	if (rc < 0) {
		pr_err("%s: No free buffer for ping\n",
			   __func__);
		return rc;
	}
	if (stream_info->stream_type != BURST_STREAM ||
		stream_info->runtime_num_burst_capture > 1) {
		rc = msm_isp_cfg_ping_pong_address(
			stream_info, VFE_PONG_FLAG, NULL);
		/* No buffer available on start is not error */
		if (rc == -ENOMEM)
			return 0;
	}

	if (rc < 0) {
		pr_err("%s: No free buffer for pong\n",
			__func__);
		return rc;
	}

	return rc;
}

static void msm_isp_get_stream_wm_mask(
	struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info,
	uint32_t *wm_reload_mask)
{
	int i;
	int vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (i = 0; i < stream_info->num_planes; i++)
		*wm_reload_mask |= (1 << stream_info->wm[vfe_idx][i]);
}

int msm_isp_axi_halt(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_halt_cmd *halt_cmd)
{
	int rc = 0;

	if (atomic_read(&vfe_dev->error_info.overflow_state) ==
		OVERFLOW_DETECTED)
		pr_err("%s: VFE%d Bus overflow detected: start recovery!\n",
			__func__, vfe_dev->pdev->id);

	/* take care of pending items in tasklet before halt */
	msm_isp_flush_tasklet(vfe_dev);

	if (halt_cmd->stop_camif) {
		vfe_dev->hw_info->vfe_ops.core_ops.update_camif_state(
				vfe_dev, DISABLE_CAMIF_IMMEDIATELY);
	}
	rc |= vfe_dev->hw_info->vfe_ops.axi_ops.halt(vfe_dev,
		halt_cmd->blocking_halt);

	return rc;
}

int msm_isp_axi_reset(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_reset_cmd *reset_cmd)
{
	int rc = 0, i, k;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	uint32_t bufq_handle = 0, bufq_id = 0;
	struct msm_isp_timestamp timestamp;
	struct msm_vfe_frame_request_queue *queue_req;
	unsigned long flags;
	uint32_t pingpong_status;
	int vfe_idx;
	uint32_t pingpong_bit = 0;
	uint32_t frame_id = 0;
	struct timeval *time_stamp;

	if (!reset_cmd) {
		pr_err("%s: NULL pointer reset cmd %pK\n", __func__, reset_cmd);
		rc = -1;
		return rc;
	}

	msm_isp_get_timestamp(&timestamp, vfe_dev);
	time_stamp = &timestamp.buf_time;

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = msm_isp_get_stream_common_data(
						vfe_dev, i);
		if (stream_info->stream_src >= VFE_AXI_SRC_MAX) {
			rc = -1;
			pr_err("%s invalid  stream src = %d\n",
				__func__,
				stream_info->stream_src);
			break;
		}
		if (stream_info->state == AVAILABLE ||
			stream_info->state == INACTIVE)
			continue;

		/* handle dual stream on ISP_VFE1 turn */
		if (stream_info->num_isp > 1 &&
			vfe_dev->pdev->id == ISP_VFE0)
			continue;

		/* set ping pong to scratch before flush */
		spin_lock_irqsave(&stream_info->lock, flags);
		frame_id = vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id;
		if (stream_info->controllable_output &&
			stream_info->undelivered_request_cnt > 0) {
			pingpong_status = VFE_PING_FLAG;
			pingpong_bit = (~(pingpong_status >>
						stream_info->wm[0][0]) & 0x1);
			if (stream_info->buf[pingpong_bit] != NULL) {
				msm_isp_process_done_buf(vfe_dev, stream_info,
						stream_info->buf[pingpong_bit],
						time_stamp,
						frame_id);
			}
			pingpong_status = VFE_PONG_FLAG;
			pingpong_bit = (~(pingpong_status >>
						stream_info->wm[0][0]) & 0x1);
			if (stream_info->buf[pingpong_bit] != NULL) {
				msm_isp_process_done_buf(vfe_dev, stream_info,
						stream_info->buf[pingpong_bit],
						time_stamp,
						frame_id);
			}
		}
		msm_isp_cfg_stream_scratch(stream_info,
					VFE_PING_FLAG);
		msm_isp_cfg_stream_scratch(stream_info,
					VFE_PONG_FLAG);
		stream_info->undelivered_request_cnt = 0;
		spin_unlock_irqrestore(&stream_info->lock,
					flags);
		while (!list_empty(&stream_info->request_q)) {
			queue_req = list_first_entry_or_null(
				&stream_info->request_q,
				struct msm_vfe_frame_request_queue, list);
			if (queue_req) {
				queue_req->cmd_used = 0;
				list_del(&queue_req->list);
			}
		}
		for (bufq_id = 0; bufq_id < VFE_BUF_QUEUE_MAX;
			bufq_id++) {
			bufq_handle = stream_info->bufq_handle[bufq_id];
			if (!bufq_handle)
				continue;
			rc = vfe_dev->buf_mgr->ops->flush_buf(
				vfe_dev->buf_mgr,
				bufq_handle, MSM_ISP_BUFFER_FLUSH_ALL,
				&timestamp.buf_time,
				reset_cmd->frame_id);
			if (rc == -EFAULT) {
				msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_BUF_FATAL_ERROR);
				return rc;
			}
		}

		for (k = 0; k < stream_info->num_isp; k++) {
			struct vfe_device *temp_vfe_dev =
					stream_info->vfe_dev[k];
			vfe_idx = msm_isp_get_vfe_idx_for_stream(
					temp_vfe_dev, stream_info);
			if (vfe_dev->dual_vfe_sync_mode &&
				(SRC_TO_INTF(stream_info->stream_src) ==
							VFE_PIX_0)) {
				temp_vfe_dev->hw_info->vfe_ops.axi_ops
					.cfg_comp_mask(temp_vfe_dev,
					stream_info);
			} else {
				if (stream_info->num_planes > 1) {
					temp_vfe_dev->hw_info->vfe_ops.axi_ops
						.cfg_comp_mask(temp_vfe_dev,
							stream_info);
				} else {
					temp_vfe_dev->hw_info->vfe_ops.axi_ops
						.cfg_wm_irq_mask(temp_vfe_dev,
							stream_info);
				}
			}
			axi_data = &temp_vfe_dev->axi_data;
			axi_data->src_info[SRC_TO_INTF(
				stream_info->stream_src)].frame_id =
				reset_cmd->frame_id;
			temp_vfe_dev->irq_sof_id = reset_cmd->frame_id;
		}
		msm_isp_reset_burst_count_and_frame_drop(
			vfe_dev, stream_info);
	}

	vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev,
		0, reset_cmd->blocking);
	/*
	 * call reset a second time for vfe48, calling
	 * only once causes bus error on camif enable
	 */
	if (msm_vfe_is_vfe48(vfe_dev))
		vfe_dev->hw_info->vfe_ops.core_ops.reset_hw(vfe_dev,
			0, reset_cmd->blocking);

	if (rc < 0)
		pr_err("%s Error! reset hw Timed out\n", __func__);

	return 0;
}

int msm_isp_axi_restart(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_restart_cmd *restart_cmd)
{
	int rc = 0, i, k, j;
	struct msm_vfe_axi_stream *stream_info;
	uint32_t wm_reload_mask[MAX_VFE] = {0, 0};
	unsigned long flags;
	int vfe_idx;

	vfe_dev->buf_mgr->frameId_mismatch_recovery = 0;
	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = msm_isp_get_stream_common_data(
					vfe_dev, i);
		if (stream_info->state == AVAILABLE ||
			stream_info->state == INACTIVE)
			continue;
		/* handle dual stream on ISP_VFE1 turn */
		if (stream_info->num_isp > 1 &&
			vfe_dev->pdev->id == ISP_VFE0)
			continue;
		spin_lock_irqsave(&stream_info->lock, flags);
		for (j = 0; j < MSM_ISP_COMP_IRQ_MAX; j++)
			stream_info->composite_irq[j] = 0;
		for (k = 0; k < stream_info->num_isp; k++) {
			struct vfe_device *temp_vfe_dev =
				stream_info->vfe_dev[k];
			vfe_idx = msm_isp_get_vfe_idx_for_stream(
					temp_vfe_dev, stream_info);
			for (j = 0; j < stream_info->num_planes; j++)
				temp_vfe_dev->hw_info->vfe_ops.axi_ops
					.enable_wm(temp_vfe_dev->vfe_base,
					stream_info->wm[vfe_idx][j], 1);
			msm_isp_get_stream_wm_mask(temp_vfe_dev, stream_info,
				&wm_reload_mask[temp_vfe_dev->pdev->id]);
		}
		msm_isp_init_stream_ping_pong_reg(stream_info);
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	for (k = 0; k < MAX_VFE; k++) {
		struct vfe_device *temp_vfe_dev =
			vfe_dev->common_data->dual_vfe_res->vfe_dev[k];
		if (wm_reload_mask[k])
			temp_vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(
				temp_vfe_dev,
				temp_vfe_dev->vfe_base, wm_reload_mask[k]);
	}

	vfe_dev->hw_info->vfe_ops.axi_ops.restart(vfe_dev, 0,
		restart_cmd->enable_camif);

	return rc;
}

static int msm_isp_axi_update_cgc_override(struct vfe_device *vfe_dev_ioctl,
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd,
	uint8_t cgc_override)
{
	int i = 0, j = 0;
	struct msm_vfe_axi_stream *stream_info;
	int k;
	struct vfe_device *vfe_dev;
	int vfe_idx;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
				VFE_AXI_SRC_MAX) {
			return -EINVAL;
		}
		stream_info = msm_isp_get_stream_common_data(vfe_dev_ioctl,
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]));
		if (!stream_info) {
			pr_err("%s: stream_info is NULL", __func__);
			return -EINVAL;
		}
		for (j = 0; j < stream_info->num_planes; j++) {
			for (k = 0; k < stream_info->num_isp; k++) {
				vfe_dev = stream_info->vfe_dev[k];
				if (!vfe_dev->hw_info->vfe_ops.axi_ops
					.update_cgc_override)
					continue;
				vfe_idx = msm_isp_get_vfe_idx_for_stream(
						vfe_dev, stream_info);
				vfe_dev->hw_info->vfe_ops.axi_ops
					.update_cgc_override(vfe_dev,
					stream_info->wm[vfe_idx][j],
					cgc_override);
			}
		}
	}
	return 0;
}

/**
 * msm_isp_axi_wait_for_stream_cfg_done() - Wait for a stream completion
 * @stream_info: The stream to wait on
 * @active: Reset means wait for stream to be INACTIVE else wait for ACTIVE
 *
 * Returns - 0 on success else error code
 */
static int msm_isp_axi_wait_for_stream_cfg_done(
			struct msm_vfe_axi_stream *stream_info, int active)
{
	int rc = -1;
	unsigned long flags;

	/* No need to wait if stream is already in required state */
	spin_lock_irqsave(&stream_info->lock, flags);
	if (active && ACTIVE == stream_info->state)
		rc = 0;
	if (!active && INACTIVE == stream_info->state)
		rc = 0;
	spin_unlock_irqrestore(&stream_info->lock, flags);
	if (rc == 0)
		return rc;

	rc = wait_for_completion_timeout(
			active ? &stream_info->active_comp :
			&stream_info->inactive_comp,
			msecs_to_jiffies(VFE_MAX_CFG_TIMEOUT));

	if (rc <= 0) {
		rc = rc ? rc : -ETIMEDOUT;
		pr_err("%s: wait for stream %x/%x state %d config failed %d\n",
			__func__,
			stream_info->stream_id,
			stream_info->stream_src,
			stream_info->state,
			rc);
		rc = -EINVAL;
	} else {
		rc = 0;
	}
	return rc;
}

/**
 * msm_isp_axi_wait_for_streams() - Wait for completion of a number of streams
 * @streams: The streams to wait on
 * @num_stream: Number of streams to wait on
 * @active: Reset means wait for stream to be INACTIVE else wait for ACTIVE
 *
 * Returns - 0 on success else error code
 */
static int msm_isp_axi_wait_for_streams(struct msm_vfe_axi_stream **streams,
		int num_stream, int active)
{
	int i;
	int rc = 0;
	struct msm_vfe_axi_stream *stream_info;

	for (i = 0; i < num_stream; i++) {
		stream_info = streams[i];
		rc |= msm_isp_axi_wait_for_stream_cfg_done(stream_info, active);
	}
	return rc;
}

static int __msm_isp_check_stream_state(struct msm_vfe_axi_stream *stream_info,
					int cmd)
{
	switch (stream_info->state) {
	case AVAILABLE:
		return -EINVAL;
	case PAUSING:
	case RESUMING:
	case RESUME_PENDING:
	case ACTIVE:
	case PAUSED:
		if (cmd != 0)
			return -EALREADY;
		break;
	case INACTIVE:
		if (cmd == 0)
			return -EALREADY;
		break;
	/*
	 * stream cannot be in following states since we always
	 * wait in ioctl for stream to be active or inactive
	 */
	case UPDATING:
	case START_PENDING:
	case STARTING:
	case STOPPING:
	case STOP_PENDING:
	case PAUSE_PENDING:
	default:
		WARN(1, "Invalid state %d\n", stream_info->state);
	}
	return 0;
}


static void __msm_isp_stop_axi_streams(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream **streams, int num_streams, int cmd_type)
{
	int i;
	struct msm_vfe_axi_shared_data *axi_data;
	struct msm_isp_timestamp timestamp;
	uint32_t bufq_id = 0, bufq_handle = 0;
	struct msm_vfe_axi_stream *stream_info;
	unsigned long flags;
	uint32_t intf;
	int rc;
	struct vfe_device *update_vfes[MAX_VFE] = {NULL, NULL};
	int k;

	msm_isp_get_timestamp(&timestamp, vfe_dev);

	for (i = 0; i < num_streams; i++) {
		stream_info = streams[i];
		msm_isp_update_intf_stream_cnt(stream_info, 0);
		for (k = 0; k < stream_info->num_isp; k++) {
			vfe_dev = stream_info->vfe_dev[k];
			update_vfes[vfe_dev->pdev->id] = vfe_dev;
		}
	}
	for (k = 0; k < MAX_VFE; k++) {
		if (!update_vfes[k])
			continue;
		msm_isp_input_disable(update_vfes[k], cmd_type);
	}

	for (i = 0; i < num_streams; i++) {
		stream_info = streams[i];
		spin_lock_irqsave(&stream_info->lock, flags);
		/*
		 * since we can get here from start axi stream error path due
		 * to which the stream may be intermittent state like
		 * STARTING/START_PENDING, force the stream to move out of
		 * intermittent state so it can be made INACTIVE. The
		 * intermittent states update variables so better to go through
		 * those state transitions instead of directly forcing stream to
		 * be INACTIVE
		 */
		memset(&stream_info->sw_skip, 0,
			sizeof(struct msm_isp_sw_framskip));
		intf = SRC_TO_INTF(stream_info->stream_src);
		if (stream_info->lpm_mode == 0 &&
			stream_info->state != PAUSED) {
			while (stream_info->state != ACTIVE)
				__msm_isp_axi_stream_update(stream_info,
						&timestamp);
		}
		msm_isp_cfg_stream_scratch(stream_info, VFE_PING_FLAG);
		msm_isp_cfg_stream_scratch(stream_info, VFE_PONG_FLAG);
		stream_info->undelivered_request_cnt = 0;
		if (stream_info->controllable_output &&
			stream_info->pending_buf_info.is_buf_done_pending) {
			msm_isp_free_pending_buffer(vfe_dev, stream_info,
				&timestamp);
			stream_info->pending_buf_info.is_buf_done_pending = 0;
		}
		for (k = 0; k < stream_info->num_isp; k++) {
			vfe_dev = stream_info->vfe_dev[k];
			if (vfe_dev->dual_vfe_sync_mode &&
				(SRC_TO_INTF(stream_info->stream_src) ==
							VFE_PIX_0)) {
				vfe_dev->hw_info->vfe_ops.axi_ops
					.clear_comp_mask(vfe_dev, stream_info);
			} else {
				if (stream_info->num_planes > 1) {
					vfe_dev->hw_info->vfe_ops.axi_ops
						.clear_comp_mask(vfe_dev,
							stream_info);
				} else {
					vfe_dev->hw_info->vfe_ops.axi_ops
						.clear_wm_irq_mask(vfe_dev,
							stream_info);
				}
			}
		}
		init_completion(&stream_info->inactive_comp);
		stream_info->state = STOP_PENDING;
		if (stream_info->lpm_mode ||
			stream_info->state == PAUSED) {
			/* don't wait for reg update */
			while (stream_info->state != INACTIVE)
				__msm_isp_axi_stream_update(stream_info,
							&timestamp);
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	for (k = 0; k < MAX_VFE; k++) {
		if (!update_vfes[k])
			continue;
		vfe_dev = update_vfes[k];
		/* make sure all stats are stopped if camif is stopped */
		if (vfe_dev->axi_data.src_info[VFE_PIX_0].active == 0)
			msm_isp_stop_all_stats_stream(vfe_dev);
	}

	for (i = 0; i < num_streams; i++) {
		stream_info = streams[i];
		spin_lock_irqsave(&stream_info->lock, flags);
		intf = SRC_TO_INTF(stream_info->stream_src);
		if (((stream_info->stream_type == BURST_STREAM) &&
			stream_info->runtime_num_burst_capture == 0) ||
			(stream_info->vfe_dev[0]->axi_data.src_info[
				intf].active == 0)) {
			while (stream_info->state != INACTIVE)
				__msm_isp_axi_stream_update(
					stream_info, &timestamp);
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	}

	rc = msm_isp_axi_wait_for_streams(streams, num_streams, 0);
	if (rc) {
		pr_err("%s: wait for stream comp failed, retry...\n", __func__);
		for (i = 0; i < num_streams; i++) {
			stream_info = streams[i];
			if (stream_info->state == INACTIVE)
				continue;
			spin_lock_irqsave(&stream_info->lock, flags);
			__msm_isp_axi_stream_update(stream_info,
						&timestamp);
			spin_unlock_irqrestore(&stream_info->lock, flags);
		}
		rc = msm_isp_axi_wait_for_streams(streams, num_streams, 0);
		if (rc) {
			pr_err("%s: wait for stream comp failed, force streams to inactive\n",
				__func__);
			for (i = 0; i < num_streams; i++) {
				stream_info = streams[i];
				if (stream_info->state == INACTIVE)
					continue;
				spin_lock_irqsave(&stream_info->lock, flags);
				while (stream_info->state != INACTIVE)
					__msm_isp_axi_stream_update(
						stream_info, &timestamp);
				spin_unlock_irqrestore(&stream_info->lock,
						flags);
			}
		}
	}
	/* clear buffers that are dequeued */
	for (i = 0; i < num_streams; i++) {
		stream_info = streams[i];
		if (stream_info->lpm_mode == 0)
			msm_isp_update_stream_bandwidth(stream_info, 0);
		for (bufq_id = 0; bufq_id < VFE_BUF_QUEUE_MAX; bufq_id++) {
			bufq_handle = stream_info->bufq_handle[bufq_id];
			if (!bufq_handle)
				continue;
			vfe_dev = stream_info->vfe_dev[0];
			rc = vfe_dev->buf_mgr->ops->flush_buf(
				vfe_dev->buf_mgr,
				bufq_handle, MSM_ISP_BUFFER_FLUSH_ALL,
				&timestamp.buf_time, 0);
			if (rc == -EFAULT)
				msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_BUF_FATAL_ERROR);
		}
	}

	for (i = 0; i < num_streams; i++) {
		stream_info = streams[i];
		intf = SRC_TO_INTF(stream_info->stream_src);
		for (k = 0; k < stream_info->num_isp; k++) {
			vfe_dev = stream_info->vfe_dev[k];
			axi_data = &vfe_dev->axi_data;
			if (axi_data->src_info[intf].stream_count == 0)
				vfe_dev->reg_update_requested &=
				~(BIT(intf));
		}
	}
}

static int msm_isp_start_axi_stream(struct vfe_device *vfe_dev_ioctl,
			struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int i, rc = 0;
	uint8_t src_state;
	uint32_t wm_reload_mask[MAX_VFE] = {0, 0};
	struct msm_vfe_axi_stream *stream_info;
	uint32_t src_mask = 0;
	unsigned long flags;
	struct msm_vfe_axi_stream *streams[MAX_NUM_STREAM];
	int num_streams = 0;
	struct msm_isp_timestamp timestamp;
	struct vfe_device *update_vfes[MAX_VFE] = {NULL, NULL};
	int k;
	struct vfe_device *vfe_dev;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev_ioctl->axi_data;
	uint32_t intf;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	msm_isp_get_timestamp(&timestamp, vfe_dev_ioctl);
	mutex_lock(&vfe_dev_ioctl->buf_mgr->lock);
	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (stream_cfg_cmd->stream_handle[i] == 0)
			continue;
		stream_info = msm_isp_get_stream_common_data(vfe_dev_ioctl,
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]));
		if (!stream_info) {
			pr_err("%s: stream_info is NULL", __func__);
			mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);
			return -EINVAL;
		}
		if (SRC_TO_INTF(stream_info->stream_src) < VFE_SRC_MAX)
			src_state = axi_data->src_info[
				SRC_TO_INTF(stream_info->stream_src)].active;

		else {
			ISP_DBG("%s: invalid src info index\n", __func__);
			rc = -EINVAL;
			mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);
			goto error;
		}
		spin_lock_irqsave(&stream_info->lock, flags);
		rc = __msm_isp_check_stream_state(stream_info, 1);
		if (-EALREADY == rc) {
			rc = 0;
			spin_unlock_irqrestore(&stream_info->lock, flags);
			continue;
		}
		if (rc) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);
			goto error;
		}
		msm_isp_calculate_bandwidth(stream_info);
		for (k = 0; k < stream_info->num_isp; k++) {
			msm_isp_get_stream_wm_mask(stream_info->vfe_dev[k],
				stream_info, &wm_reload_mask[
					stream_info->vfe_dev[k]->pdev->id]);
			src_state = stream_info->vfe_dev[k]->axi_data.src_info[
				SRC_TO_INTF(stream_info->stream_src)].active;
			if (update_vfes[stream_info->vfe_dev[k]->pdev->id])
				continue;
			update_vfes[stream_info->vfe_dev[k]->pdev->id] =
							stream_info->vfe_dev[k];
		}
		msm_isp_reset_framedrop(vfe_dev_ioctl, stream_info);
		rc = msm_isp_init_stream_ping_pong_reg(stream_info);
		if (rc < 0) {
			pr_err("%s: No buffer for stream%x\n", __func__,
				stream_info->stream_id);
			spin_unlock_irqrestore(&stream_info->lock, flags);
			mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);
			goto error;
		}
		for (k = 0; k < stream_info->num_isp; k++) {
			vfe_dev = stream_info->vfe_dev[k];
			if (vfe_dev->dual_vfe_sync_mode &&
				(SRC_TO_INTF(stream_info->stream_src)
							== VFE_PIX_0)) {
				vfe_dev->hw_info->vfe_ops.axi_ops.cfg_comp_mask(
					vfe_dev, stream_info);
			} else {
				if (stream_info->num_planes > 1) {
					vfe_dev->hw_info->vfe_ops.axi_ops
					.cfg_comp_mask(vfe_dev, stream_info);
				} else {
					vfe_dev->hw_info->vfe_ops.axi_ops
						.cfg_wm_irq_mask(vfe_dev,
							stream_info);
				}
			}
		}
		intf = SRC_TO_INTF(stream_info->stream_src);
		stream_info->lpm_mode = vfe_dev_ioctl->axi_data.src_info[
					intf].lpm;
		if (stream_info->lpm_mode == 0) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			msm_isp_update_stream_bandwidth(stream_info, 1);
			spin_lock_irqsave(&stream_info->lock, flags);
		}
		init_completion(&stream_info->active_comp);
		stream_info->state = START_PENDING;
		msm_isp_update_intf_stream_cnt(stream_info, 1);

		ISP_DBG("%s, Stream 0x%x src_state %d on vfe %d\n", __func__,
			stream_info->stream_src, src_state,
			vfe_dev_ioctl->pdev->id);
		if (src_state) {
			src_mask |= (1 << SRC_TO_INTF(stream_info->stream_src));
			if (stream_info->lpm_mode) {
				while (stream_info->state != ACTIVE)
					__msm_isp_axi_stream_update(
						stream_info, &timestamp);
			}
		} else {
			for (k = 0; k < stream_info->num_isp; k++) {
				vfe_dev = stream_info->vfe_dev[k];

				if (vfe_dev->dump_reg)
					msm_camera_io_dump(vfe_dev->vfe_base,
						0x1000, 1);
			}

			/* Configure AXI start bits to start immediately */
			while (stream_info->state != ACTIVE)
				__msm_isp_axi_stream_update(
						stream_info, &timestamp);

			for (k = 0; k < stream_info->num_isp; k++) {
				vfe_dev = stream_info->vfe_dev[k];
				vfe_dev->hw_info->vfe_ops.core_ops.reg_update(
					vfe_dev,
					SRC_TO_INTF(stream_info->stream_src));
			}
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
		streams[num_streams++] = stream_info;
	}
	mutex_unlock(&vfe_dev_ioctl->buf_mgr->lock);

	for (i = 0; i < MAX_VFE; i++) {
		vfe_dev = update_vfes[i];
		if (!vfe_dev)
			continue;
		vfe_dev->hw_info->vfe_ops.axi_ops.reload_wm(vfe_dev,
			vfe_dev->vfe_base, wm_reload_mask[i]);

		msm_isp_input_enable(vfe_dev,
			stream_cfg_cmd->sync_frame_id_src);
	}

	rc = msm_isp_axi_wait_for_streams(streams, num_streams, 1);
	if (rc < 0) {
		pr_err("%s: wait for config done failed\n", __func__);
		goto error;
	}

	return 0;
error:
	__msm_isp_stop_axi_streams(vfe_dev_ioctl, streams, num_streams,
				STOP_STREAM);

	return rc;
}

static int msm_isp_stop_axi_stream(struct vfe_device *vfe_dev_ioctl,
			struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd)
{
	int i, rc = 0;
	struct msm_vfe_axi_stream *stream_info = NULL;
	struct msm_vfe_axi_stream *streams[MAX_NUM_STREAM];
	int num_streams = 0;
	unsigned long flags;

	if (stream_cfg_cmd->num_streams > MAX_NUM_STREAM ||
		stream_cfg_cmd->num_streams == 0)
		return -EINVAL;

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (stream_cfg_cmd->stream_handle[i] == 0)
			continue;
		stream_info = msm_isp_get_stream_common_data(vfe_dev_ioctl,
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]));
		if (!stream_info) {
			pr_err("%s: stream_info is NULL", __func__);
			return -EINVAL;
		}
		spin_lock_irqsave(&stream_info->lock, flags);
		rc = __msm_isp_check_stream_state(stream_info, 0);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		if (rc) {
			/*
			 * continue stopping other streams as error here means
			 * stream is already not active
			 */
			rc = 0;
			continue;
		}
		streams[num_streams++] = stream_info;
	}
	__msm_isp_stop_axi_streams(vfe_dev_ioctl, streams, num_streams,
				stream_cfg_cmd->cmd);

	return rc;
}

int msm_isp_cfg_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0;
	struct msm_vfe_axi_stream_cfg_cmd *stream_cfg_cmd = arg;
	uint32_t stream_idx[MAX_NUM_STREAM];
	int i;
	int vfe_idx;
	struct msm_vfe_axi_stream *stream_info;

	memset(stream_idx, 0, sizeof(stream_idx));

	for (i = 0; i < stream_cfg_cmd->num_streams; i++) {
		if (HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]) >=
			VFE_AXI_SRC_MAX)
			return -EINVAL;
		stream_info = msm_isp_get_stream_common_data(vfe_dev,
			HANDLE_TO_IDX(stream_cfg_cmd->stream_handle[i]));
		if (!stream_info) {
			pr_err("%s: stream_info is NULL", __func__);
			return -EINVAL;
		}
		vfe_idx = msm_isp_get_vfe_idx_for_stream_user(vfe_dev,
								stream_info);
		if (vfe_idx == -ENOTTY || stream_info->stream_handle[vfe_idx] !=
					stream_cfg_cmd->stream_handle[i]) {
			pr_err("%s: Invalid stream handle %x vfe_idx %d expected %x\n",
				__func__, stream_cfg_cmd->stream_handle[i],
				vfe_idx,
				(vfe_idx != -ENOTTY) ?
				stream_info->stream_handle[vfe_idx] : 0);
			return -EINVAL;
		}
		/* check for duplicate stream handle */
		if (stream_idx[stream_info->stream_src] ==
			stream_cfg_cmd->stream_handle[i])
			stream_cfg_cmd->stream_handle[i] = 0;
		else
			stream_idx[stream_info->stream_src] =
				stream_cfg_cmd->stream_handle[i];
	}
	if (stream_cfg_cmd->cmd == START_STREAM) {
		msm_isp_axi_update_cgc_override(vfe_dev, stream_cfg_cmd, 1);

		rc = msm_isp_start_axi_stream(
			vfe_dev, stream_cfg_cmd);
	} else {
		rc = msm_isp_stop_axi_stream(
			vfe_dev, stream_cfg_cmd);

		msm_isp_axi_update_cgc_override(vfe_dev, stream_cfg_cmd, 0);

		/*
		 * Use different ret value to not overwrite the error from
		 * msm_isp_stop_axi_stream
		 */
		if (vfe_dev->axi_data.num_active_stream == 0)
			vfe_dev->hvx_cmd = HVX_DISABLE;
		if (vfe_dev->is_split) {
			struct vfe_device *vfe_temp =
				vfe_dev->common_data->dual_vfe_res->vfe_dev[
					ISP_VFE0];
			if (vfe_temp->axi_data.num_active_stream == 0)
				vfe_temp->hvx_cmd = HVX_DISABLE;
		}
	}

	if (rc < 0)
		pr_err("%s: start/stop %d stream failed\n", __func__,
			stream_cfg_cmd->cmd);
	return rc;
}

static int msm_isp_return_empty_buffer(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t user_stream_id,
	uint32_t frame_id, uint32_t buf_index,
	enum msm_vfe_input_src frame_src)
{
	int rc = -1;
	struct msm_isp_buffer *buf = NULL;
	uint32_t bufq_handle = 0;
	uint32_t stream_idx;
	struct msm_isp_event_data error_event;
	struct msm_isp_timestamp timestamp;

	if (!vfe_dev || !stream_info) {
		pr_err("%s %d failed: vfe_dev %pK stream_info %pK\n", __func__,
			__LINE__, vfe_dev, stream_info);
		return -EINVAL;
	}

	stream_idx = stream_info->stream_src;
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
		vfe_dev->pdev->id, bufq_handle, buf_index, &buf);
	if (rc == -EFAULT) {
		msm_isp_halt_send_error(vfe_dev, ISP_EVENT_BUF_FATAL_ERROR);
		return rc;
	}

	if (rc < 0 || buf == NULL) {
		pr_err("Skip framedrop report due to no buffer\n");
		return rc;
	}

	msm_isp_get_timestamp(&timestamp, vfe_dev);
	buf->buf_debug.put_state[buf->buf_debug.put_state_last] =
		MSM_ISP_BUFFER_STATE_DROP_REG;
	buf->buf_debug.put_state_last ^= 1;
	rc = vfe_dev->buf_mgr->ops->buf_err(vfe_dev->buf_mgr,
		buf->bufq_handle, buf->buf_idx,
		&timestamp.buf_time, frame_id,
		stream_info->runtime_output_format);
	if (rc == -EFAULT) {
		msm_isp_halt_send_error(vfe_dev,
			ISP_EVENT_BUF_FATAL_ERROR);
		return rc;
	}

	memset(&error_event, 0, sizeof(error_event));
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
	uint32_t frame_id, uint32_t buf_index)
{
	struct msm_vfe_axi_stream_request_cmd stream_cfg_cmd;
	struct msm_vfe_frame_request_queue *queue_req;
	uint32_t pingpong_status;
	unsigned long flags;
	int rc = 0;
	enum msm_vfe_input_src frame_src = 0;
	int k;
	uint32_t wm_mask = 0;
	int vfe_idx;
	uint32_t pingpong_bit = 0;

	if (!vfe_dev || !stream_info) {
		pr_err("%s %d failed: vfe_dev %pK stream_info %pK\n", __func__,
			__LINE__, vfe_dev, stream_info);
		return -EINVAL;
	}

	/* return early for dual vfe */
	if (stream_info->num_isp > 1 &&
		vfe_dev->pdev->id == ISP_VFE1 &&
		vfe_dev->dual_vfe_sync_mode)
		return 0;
	if (stream_info->num_isp > 1 &&
		vfe_dev->pdev->id == ISP_VFE0 &&
		!vfe_dev->dual_vfe_sync_mode)
		return 0;

	if (stream_info->stream_src >= VFE_AXI_SRC_MAX) {
		pr_err("%s:%d invalid stream src %d\n", __func__, __LINE__,
			stream_info->stream_src);
		return -EINVAL;
	}

	frame_src = SRC_TO_INTF(stream_info->stream_src);
	pingpong_status =
		vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(vfe_dev);

	/* As MCT is still processing it, need to drop the additional requests*/
	if (vfe_dev->isp_page->drop_reconfig &&
		frame_src == VFE_PIX_0) {
		pr_err("%s: MCT has not yet delayed %d drop request %d\n",
			__func__, vfe_dev->isp_page->drop_reconfig, frame_id);
		goto error;
	}

	/*
	 * If PIX stream is active then RDI path uses SOF frame ID of PIX
	 * In case of standalone RDI streaming, SOF are used from
	 * individual intf.
	 */
	/*
	 * If frame_id = 1 then no eof check is needed
	 */
	if (vfe_dev->axi_data.src_info[frame_src].active &&
		frame_src == VFE_PIX_0 &&
		vfe_dev->axi_data.src_info[frame_src].accept_frame == false &&
		(stream_info->undelivered_request_cnt <=
			MAX_BUFFERS_IN_HW)
		) {
		pr_debug("%s:%d invalid time to request frame %d try drop_reconfig\n",
			__func__, __LINE__, frame_id);
		vfe_dev->isp_page->drop_reconfig = 1;
		return 0;
	} else if ((vfe_dev->axi_data.src_info[frame_src].active) &&
			((frame_id ==
			vfe_dev->axi_data.src_info[frame_src].frame_id) ||
			(frame_id == vfe_dev->irq_sof_id)) &&
			(stream_info->undelivered_request_cnt <=
				MAX_BUFFERS_IN_HW)) {
		vfe_dev->isp_page->drop_reconfig = 1;
		pr_debug("%s: vfe_%d request_frame %d cur frame id %d pix %d try drop_reconfig\n",
			__func__, vfe_dev->pdev->id, frame_id,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id,
			vfe_dev->axi_data.src_info[VFE_PIX_0].active);
		return 0;
	} else if ((vfe_dev->axi_data.src_info[frame_src].active && (frame_id !=
		vfe_dev->axi_data.src_info[frame_src].frame_id +
		vfe_dev->axi_data.src_info[frame_src].sof_counter_step)) ||
		((!vfe_dev->axi_data.src_info[frame_src].active))) {
		pr_debug("%s:%d invalid frame id %d cur frame id %d pix %d\n",
			__func__, __LINE__, frame_id,
			vfe_dev->axi_data.src_info[frame_src].frame_id,
			vfe_dev->axi_data.src_info[frame_src].active);
		goto error;
	}
	if (stream_info->undelivered_request_cnt >= MAX_BUFFERS_IN_HW) {
		pr_debug("%s:%d invalid undelivered_request_cnt %d frame id %d\n",
			__func__, __LINE__,
			stream_info->undelivered_request_cnt,
			vfe_dev->axi_data.src_info[VFE_PIX_0].frame_id);
		goto error;
	}
	if ((frame_src == VFE_PIX_0) && !stream_info->undelivered_request_cnt &&
		MSM_VFE_STREAM_STOP_PERIOD !=
		stream_info->activated_framedrop_period) {
		/* wm is reloaded if undelivered_request_cnt is zero.
		 * As per the hw behavior wm should be disabled or skip writing
		 * before reload happens other wise wm could start writing from
		 * middle of the frame and could result in image corruption.
		 * instead of dropping frame in this error scenario use
		 * drop_reconfig flag to process the request in next sof.
		 */
		pr_debug("%s:%d vfe %d frame_id %d prev_pattern %x stream_id %x\n",
			__func__, __LINE__, vfe_dev->pdev->id, frame_id,
			stream_info->activated_framedrop_period,
			stream_info->stream_id);
		vfe_dev->isp_page->drop_reconfig = 1;
		return 0;
	}

	spin_lock_irqsave(&stream_info->lock, flags);
	vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	/* When wm reloaded, pingpong status register would be stale, pingpong
	 * status would be updated only after AXI_DONE interrupt processed.
	 * So, we should avoid reading value from pingpong status register
	 * until buf_done happens for ping buffer.
	 */
	if ((stream_info->undelivered_request_cnt == 1) &&
		(stream_info->sw_ping_pong_bit != -1)) {
		pingpong_status =
			vfe_dev->hw_info->vfe_ops.axi_ops.get_pingpong_status(
				vfe_dev);
		pingpong_bit = ((pingpong_status >>
					stream_info->wm[vfe_idx][0]) & 0x1);
		if (stream_info->sw_ping_pong_bit == !pingpong_bit) {
			ISP_DBG("%s:Return Empty Buffer stream id 0x%X\n",
				__func__, stream_info->stream_id);
			rc = msm_isp_return_empty_buffer(vfe_dev, stream_info,
				user_stream_id, frame_id, buf_index,
				frame_src);
			spin_unlock_irqrestore(&stream_info->lock,
					flags);
			return 0;
		}
	}

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
		pr_err("%s:%d request frame failed on hw stream 0x%x, request stream %d due to no bufq idx: %d\n",
			__func__, __LINE__,
			stream_info->stream_handle[0],
			user_stream_id, queue_req->buff_queue_id);
		return 0;
	}
	queue_req->buf_index = buf_index;
	queue_req->cmd_used = 1;

	stream_info->request_q_idx =
		(stream_info->request_q_idx + 1) % MSM_VFE_REQUESTQ_SIZE;
	list_add_tail(&queue_req->list, &stream_info->request_q);
	stream_info->request_q_cnt++;

	stream_info->undelivered_request_cnt++;
	stream_cfg_cmd.axi_stream_handle = stream_info->stream_handle[vfe_idx];
	stream_cfg_cmd.frame_skip_pattern = NO_SKIP;
	stream_cfg_cmd.init_frame_drop = 0;
	stream_cfg_cmd.burst_count = stream_info->request_q_cnt;

	if (stream_info->undelivered_request_cnt == 1) {
		rc = msm_isp_cfg_ping_pong_address(stream_info,
			VFE_PING_FLAG, NULL);
		if (rc) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			stream_info->undelivered_request_cnt--;
			queue_req = list_first_entry_or_null(
				&stream_info->request_q,
				struct msm_vfe_frame_request_queue, list);
			if (queue_req) {
				queue_req->cmd_used = 0;
				list_del(&queue_req->list);
				stream_info->request_q_cnt--;
			}
			pr_err_ratelimited("%s:%d fail to cfg HAL buffer stream %x\n",
				__func__, __LINE__, stream_info->stream_id);
			return rc;
		}

		for (k = 0; k < stream_info->num_isp; k++) {
			wm_mask = 0;
			msm_isp_get_stream_wm_mask(stream_info->vfe_dev[k],
				stream_info, &wm_mask);
			stream_info->vfe_dev[
				k]->hw_info->vfe_ops.axi_ops.reload_wm(
				stream_info->vfe_dev[k],
				stream_info->vfe_dev[k]->vfe_base, wm_mask);

		}
		/* sw_ping_pong_bit is updated only when AXI_DONE.
		 * so now reset this bit to -1.
		 */
		stream_info->sw_ping_pong_bit = -1;
	} else if (stream_info->undelivered_request_cnt == 2) {
		if (stream_info->sw_ping_pong_bit == -1) {
			/* This means wm is reloaded & ping buffer is
			 * already configured. And AXI_DONE for ping
			 * is still pending. So, config pong buffer
			 * now.
			 */
			rc = msm_isp_cfg_ping_pong_address(stream_info,
				VFE_PONG_FLAG, NULL);
		} else {
			rc = msm_isp_cfg_ping_pong_address(
					stream_info, pingpong_status, NULL);
		}
		if (rc) {
			stream_info->undelivered_request_cnt--;
			spin_unlock_irqrestore(&stream_info->lock,
						flags);
			queue_req = list_first_entry_or_null(
				&stream_info->request_q,
				struct msm_vfe_frame_request_queue, list);
			if (queue_req) {
				queue_req->cmd_used = 0;
				list_del(&queue_req->list);
				stream_info->request_q_cnt--;
			}
			pr_err_ratelimited("%s:%d fail to cfg HAL buffer\n",
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

	rc = msm_isp_calculate_framedrop(vfe_dev, &stream_cfg_cmd);
	if (rc == 0)
		msm_isp_reset_framedrop(vfe_dev, stream_info);

	/*Avoid Multiple request frames for single SOF*/
	vfe_dev->axi_data.src_info[frame_src].accept_frame = false;

	spin_unlock_irqrestore(&stream_info->lock, flags);

	return rc;
error:
	rc = msm_isp_return_empty_buffer(vfe_dev, stream_info,
		user_stream_id, frame_id, buf_index, frame_src);
	if (rc < 0)
		pr_err("%s:%d failed: return_empty_buffer src %d\n",
		__func__, __LINE__, frame_src);
	return 0;

}

static int msm_isp_add_buf_queue(struct vfe_device *vfe_dev,
	struct msm_vfe_axi_stream *stream_info, uint32_t stream_id)
{
	int rc = 0;
	uint32_t bufq_id = 0;
	unsigned long flags;

	if (stream_id == stream_info->stream_id)
		bufq_id = VFE_BUF_QUEUE_DEFAULT;
	else
		bufq_id = VFE_BUF_QUEUE_SHARED;

	spin_lock_irqsave(&stream_info->lock, flags);

	if (stream_info->bufq_handle[bufq_id] == 0) {
		stream_info->bufq_handle[bufq_id] =
			vfe_dev->buf_mgr->ops->get_bufq_handle(vfe_dev->buf_mgr,
				stream_info->session_id, stream_id);
		if (stream_info->bufq_handle[bufq_id] == 0) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			pr_err("%s: failed: No valid buffer queue for stream: 0x%x\n",
				__func__, stream_id);
			return -EINVAL;
		}
	} else {
		uint32_t bufq_handle = vfe_dev->buf_mgr->ops->get_bufq_handle(
						vfe_dev->buf_mgr,
						stream_info->session_id,
						stream_id);
		if (bufq_handle != stream_info->bufq_handle[bufq_id]) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			pr_err("%s: Stream %x already has buffer q %x cannot add handle %x\n",
				__func__, stream_id,
				stream_info->bufq_handle[bufq_id], bufq_handle);
			return -EINVAL;
		}
	}

	spin_unlock_irqrestore(&stream_info->lock, flags);

	ISP_DBG("%d: Add bufq handle:0x%x, idx:%d, for stream %d on VFE %d\n",
		__LINE__, stream_info->bufq_handle[bufq_id],
		bufq_id, stream_info->stream_handle[0],
		vfe_dev->pdev->id);

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

	if (stream_info->bufq_handle[bufq_id]) {
		stream_info->bufq_handle[bufq_id] = 0;
		if (stream_info->state == ACTIVE) {
			init_completion(&stream_info->active_comp);
			stream_info->state = UPDATING;
		}
	}
	spin_unlock_irqrestore(&stream_info->lock, flags);
	if (stream_info->state == UPDATING)
		msm_isp_axi_wait_for_stream_cfg_done(stream_info, 1);

}

/**
 * msm_isp_stream_axi_cfg_update() - Apply axi config update to a stream
 * @vfe_dev: The vfe device on which the update is to be applied
 * @stream_info: Stream for which update is to be applied
 * @update_info: Parameters of the update
 *
 * Returns - 0 on success else error code
 *
 * For dual vfe stream apply the update once update for both vfe is
 * received.
 */
static int msm_isp_stream_axi_cfg_update(struct vfe_device *vfe_dev,
			struct msm_vfe_axi_stream *stream_info,
			struct msm_vfe_axi_stream_cfg_update_info *update_info)
{
	int j;
	int k;
	unsigned long flags;
	int vfe_idx;

	spin_lock_irqsave(&stream_info->lock, flags);
	if (stream_info->state != ACTIVE) {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		pr_err("Invalid stream state for axi update %d\n",
			stream_info->state);
		return -EINVAL;
	}
	if (stream_info->update_vfe_mask) {
		if (stream_info->update_vfe_mask & (1 << vfe_dev->pdev->id)) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			pr_err("%s: Stream %pK/%x Update already in progress for vfe %d\n",
				__func__, stream_info, stream_info->stream_src,
				vfe_dev->pdev->id);
			return -EINVAL;
		}
	}
	vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);

	for (j = 0; j < stream_info->num_planes; j++)
		stream_info->plane_cfg[vfe_idx][j] = update_info->plane_cfg[j];

	stream_info->update_vfe_mask |= (1 << vfe_dev->pdev->id);
	/* wait for update from all vfe's under stream before applying */
	if (stream_info->update_vfe_mask != stream_info->vfe_mask) {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		return 0;
	}

	atomic_set(&vfe_dev->axi_data.axi_cfg_update[
		SRC_TO_INTF(stream_info->stream_src)], 1);
	stream_info->output_format = update_info->output_format;
	init_completion(&stream_info->active_comp);
	if (((vfe_dev->hw_info->runtime_axi_update == 0) ||
		(vfe_dev->dual_vfe_enable == 1)))  {
		stream_info->state = PAUSE_PENDING;
		msm_isp_axi_stream_enable_cfg(stream_info);
		stream_info->state = PAUSING;
	} else {
		for (j = 0; j < stream_info->num_planes; j++) {
			for (k = 0; k < stream_info->num_isp; k++) {
				vfe_dev = stream_info->vfe_dev[k];
				vfe_dev->hw_info->vfe_ops.axi_ops.cfg_wm_reg(
					vfe_dev, stream_info, j);
			}
		}
		stream_info->state = RESUMING;
	}
	spin_unlock_irqrestore(&stream_info->lock, flags);
	return 0;
}

int msm_isp_update_axi_stream(struct vfe_device *vfe_dev, void *arg)
{
	int rc = 0, i, j, k;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_stream_update_cmd *update_cmd = arg;
	struct msm_vfe_axi_stream_cfg_update_info *update_info = NULL;
	struct msm_isp_sw_framskip *sw_skip_info = NULL;
	unsigned long flags;
	struct msm_isp_timestamp timestamp;
	uint32_t frame_id;
	int vfe_idx;

	/*num_stream is uint32 and update_info[] bound by MAX_NUM_STREAM*/
	if (update_cmd->num_streams > MAX_NUM_STREAM)
		return -EINVAL;

	for (i = 0; i < update_cmd->num_streams; i++) {
		update_info = (struct msm_vfe_axi_stream_cfg_update_info *)
			&update_cmd->update_info[i];
		/*check array reference bounds*/
		if (HANDLE_TO_IDX(update_info->stream_handle) >=
			VFE_AXI_SRC_MAX) {
			return -EINVAL;
		}
		stream_info = msm_isp_get_stream_common_data(vfe_dev,
			HANDLE_TO_IDX(update_info->stream_handle));
		if (!stream_info) {
			pr_err("%s: stream_info is null", __func__);
			return -EINVAL;
		}
		if (SRC_TO_INTF(stream_info->stream_src) >= VFE_SRC_MAX)
			continue;
		if (stream_info->state != ACTIVE &&
			stream_info->state != INACTIVE &&
			update_cmd->update_type !=
			UPDATE_STREAM_REQUEST_FRAMES &&
			update_cmd->update_type !=
			UPDATE_STREAM_REMOVE_BUFQ &&
			update_cmd->update_type !=
			UPDATE_STREAM_SW_FRAME_DROP &&
			update_cmd->update_type !=
			UPDATE_STREAM_REQUEST_FRAMES_VER2) {
			pr_err("%s: Invalid stream state %d, update cmd %d\n",
				__func__, stream_info->state,
				stream_info->stream_id);
			return -EINVAL;
		}
		if (update_cmd->update_type == UPDATE_STREAM_AXI_CONFIG &&
			stream_info->state != ACTIVE) {
			pr_err("%s: AXI stream config updating\n", __func__);
			return -EBUSY;
		}
	}

	switch (update_cmd->update_type) {
	case ENABLE_STREAM_BUF_DIVERT:
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			stream_info->buf_divert = 1;
		}
		break;
	case DISABLE_STREAM_BUF_DIVERT:
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			stream_info->buf_divert = 0;
			msm_isp_get_timestamp(&timestamp, vfe_dev);
			frame_id = vfe_dev->axi_data.src_info[
				SRC_TO_INTF(stream_info->stream_src)].frame_id;
			/* set ping pong address to scratch before flush */
			spin_lock_irqsave(&stream_info->lock, flags);
			msm_isp_cfg_stream_scratch(stream_info,
					VFE_PING_FLAG);
			msm_isp_cfg_stream_scratch(stream_info,
					VFE_PONG_FLAG);
			spin_unlock_irqrestore(&stream_info->lock, flags);
			rc = vfe_dev->buf_mgr->ops->flush_buf(
				vfe_dev->buf_mgr,
				stream_info->bufq_handle
					[VFE_BUF_QUEUE_DEFAULT],
				MSM_ISP_BUFFER_FLUSH_DIVERTED,
				&timestamp.buf_time, frame_id);
			if (rc == -EFAULT) {
				msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_BUF_FATAL_ERROR);
				return rc;
			}
		}
		break;
	case UPDATE_STREAM_FRAMEDROP_PATTERN: {
		for (i = 0; i < update_cmd->num_streams; i++) {
			uint32_t framedrop_period =
				msm_isp_get_framedrop_period(
					update_info->skip_pattern);
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
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
				msm_isp_cfg_framedrop_reg(stream_info);
			spin_unlock_irqrestore(&stream_info->lock, flags);
		}
		break;
	}
	case UPDATE_STREAM_SW_FRAME_DROP: {
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			sw_skip_info = &update_info->sw_skip_info;
			if (sw_skip_info &&
				sw_skip_info->stream_src_mask != 0) {
				/* SW image buffer drop */
				pr_debug("%x sw skip type %x mode %d min %d max %d\n",
					stream_info->stream_id,
					sw_skip_info->stats_type_mask,
					sw_skip_info->skip_mode,
					sw_skip_info->min_frame_id,
					sw_skip_info->max_frame_id);
				spin_lock_irqsave(&stream_info->lock, flags);
				stream_info->sw_skip = *sw_skip_info;
				spin_unlock_irqrestore(&stream_info->lock,
					flags);
			}
		}
		break;
	}
	case UPDATE_STREAM_AXI_CONFIG: {
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			rc = msm_isp_stream_axi_cfg_update(vfe_dev, stream_info,
						update_info);
			if (rc)
				return rc;
		}
		break;
	}
	case UPDATE_STREAM_REQUEST_FRAMES: {
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			mutex_lock(&vfe_dev->buf_mgr->lock);
			rc = msm_isp_request_frame(vfe_dev, stream_info,
				update_info->user_stream_id,
				update_info->frame_id,
				MSM_ISP_INVALID_BUF_INDEX);
			mutex_unlock(&vfe_dev->buf_mgr->lock);
			if (rc)
				pr_err("%s failed to request frame!\n",
					__func__);
		}
		break;
	}
	case UPDATE_STREAM_ADD_BUFQ: {
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			rc = msm_isp_add_buf_queue(vfe_dev, stream_info,
				update_info->user_stream_id);
			if (rc)
				pr_err("%s failed to add bufq!\n", __func__);
		}
		break;
	}
	case UPDATE_STREAM_REMOVE_BUFQ: {
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			msm_isp_remove_buf_queue(vfe_dev, stream_info,
				update_info->user_stream_id);
			pr_debug("%s, Remove bufq for Stream 0x%x\n",
				__func__, stream_info->stream_id);
		}
		break;
	}
	case UPDATE_STREAM_REQUEST_FRAMES_VER2: {
		struct msm_vfe_axi_stream_cfg_update_info_req_frm *req_frm =
			&update_cmd->req_frm_ver2;
		stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(req_frm->stream_handle));
		if (!stream_info) {
			pr_err("%s: stream_info is null", __func__);
			return -EINVAL;
		}
		mutex_lock(&vfe_dev->buf_mgr->lock);
		rc = msm_isp_request_frame(vfe_dev, stream_info,
			req_frm->user_stream_id,
			req_frm->frame_id,
			req_frm->buf_index);
		mutex_unlock(&vfe_dev->buf_mgr->lock);
		if (rc)
			pr_err("%s failed to request frame!\n",
				__func__);
		break;
	}
	case UPDATE_STREAM_OFFLINE_AXI_CONFIG: {
		for (i = 0; i < update_cmd->num_streams; i++) {
			update_info =
				(struct msm_vfe_axi_stream_cfg_update_info *)
				&update_cmd->update_info[i];
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
				HANDLE_TO_IDX(update_info->stream_handle));
			if (!stream_info) {
				pr_err("%s: stream_info is null", __func__);
				return -EINVAL;
			}
			vfe_idx = msm_isp_get_vfe_idx_for_stream(
				vfe_dev, stream_info);
			for (j = 0; j < stream_info->num_planes; j++) {
				stream_info->plane_cfg[vfe_idx][j] =
					update_info->plane_cfg[j];
				for (k = 0; k < stream_info->num_isp; k++) {
					vfe_dev = stream_info->vfe_dev[k];
					vfe_dev->hw_info->vfe_ops.axi_ops
						.cfg_wm_reg(vfe_dev,
						stream_info, j);
				}
			}
		}
		break;
	}
	default:
		pr_err("%s: Invalid update type %d\n", __func__,
			update_cmd->update_type);
		return -EINVAL;
	}

	return rc;
}

void msm_isp_process_axi_irq_stream(struct vfe_device *vfe_dev,
		struct msm_vfe_axi_stream *stream_info,
		uint32_t pingpong_status,
		struct msm_isp_timestamp *ts)
{
	int rc = -1;
	uint32_t pingpong_bit = 0, i;
	struct msm_isp_buffer *done_buf = NULL;
	unsigned long flags;
	struct timeval *time_stamp;
	uint32_t frame_id, buf_index = -1;
	int vfe_idx;
	struct vfe_device *temp_dev;
	int other_vfe_id;

	if (!ts) {
		pr_err("%s: Error! Invalid argument\n", __func__);
		return;
	}

	if (vfe_dev->vt_enable) {
		msm_isp_get_avtimer_ts(ts);
		time_stamp = &ts->vt_time;
	} else {
		time_stamp = &ts->buf_time;
	}

	frame_id = vfe_dev->axi_data.src_info[
			SRC_TO_INTF(stream_info->stream_src)].frame_id;

	spin_lock_irqsave(&stream_info->lock, flags);
	vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev, stream_info);
	pingpong_bit = (~(pingpong_status >>
			stream_info->wm[vfe_idx][0]) & 0x1);
	for (i = 0; i < stream_info->num_planes; i++) {
		if (pingpong_bit !=
			(~(pingpong_status >>
			stream_info->wm[vfe_idx][i]) & 0x1)) {
			spin_unlock_irqrestore(&stream_info->lock, flags);
			msm_isp_dump_ping_pong_mismatch(vfe_dev);
			pr_err("%s: Write master ping pong mismatch. Status: 0x%x %x\n",
				__func__, pingpong_status,
				stream_info->stream_src);
			msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_PING_PONG_MISMATCH);
			return;
		}
	}
	if (stream_info->state == INACTIVE) {
		WARN_ON(stream_info->buf[pingpong_bit] != NULL);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		return;
	}
	if (vfe_dev->dual_vfe_sync_mode)
		rc = 0;
	else
		rc = msm_isp_composite_irq(vfe_dev, stream_info,
			MSM_ISP_COMP_IRQ_PING_BUFDONE + pingpong_bit);

	if (rc) {
		spin_unlock_irqrestore(&stream_info->lock, flags);
		if (rc < 0)
			msm_isp_halt_send_error(vfe_dev,
					ISP_EVENT_PING_PONG_MISMATCH);
		return;
	}

	done_buf = stream_info->buf[pingpong_bit];

	if (vfe_dev->buf_mgr->frameId_mismatch_recovery == 1) {
		if (done_buf) {
			if (done_buf->is_drop_reconfig == 1)
				done_buf->is_drop_reconfig = 0;
		}
		pr_err_ratelimited("%s: Mismatch Recovery in progress, drop frame!\n",
			__func__);
		spin_unlock_irqrestore(&stream_info->lock, flags);
		return;
	}

	if (done_buf)
		buf_index = done_buf->buf_idx;

	ISP_DBG("%s: vfe %d: stream 0x%x, frame id %d, pingpong bit %d\n",
		__func__,
		vfe_dev->pdev->id,
		stream_info->stream_id,
		frame_id,
		pingpong_bit);

	stream_info->frame_id++;
	stream_info->buf[pingpong_bit] = NULL;

	if (stream_info->controllable_output &&
		(done_buf != NULL) &&
		(stream_info->sw_ping_pong_bit == -1) &&
		(done_buf->is_drop_reconfig == 1)) {
		/* When wm reloaded and corresponding reg_update fail
		 * then buffer is reconfig as PING buffer. so, avoid
		 * NULL assignment to PING buffer and eventually
		 * next AXI_DONE or buf_done can be successful
		 */
		stream_info->buf[pingpong_bit] = done_buf;
	}

	if (stream_info->stream_type == CONTINUOUS_STREAM ||
		stream_info->runtime_num_burst_capture > 1) {
		rc = msm_isp_cfg_ping_pong_address(
			stream_info, pingpong_status, NULL);
		if (rc < 0)
			ISP_DBG("%s: Error configuring ping_pong\n",
				__func__);
	} else if (done_buf && (done_buf->is_drop_reconfig != 1)) {
		int32_t frame_id_diff;
		/* irq_sof should be always >= tasklet SOF id
		 * For dual camera usecase irq_sof could be behind
		 * as software frameid sync logic epoch event could
		 * update slave frame id so update if irqsof < tasklet sof
		 */
		if (vfe_dev->irq_sof_id < frame_id)
			vfe_dev->irq_sof_id = frame_id;

		frame_id_diff = vfe_dev->irq_sof_id - frame_id;
		if (stream_info->controllable_output && frame_id_diff > 1) {
			pr_err_ratelimited("%s: scheduling problem do recovery irq_sof_id %d frame_id %d\n",
				__func__, vfe_dev->irq_sof_id, frame_id);
			/* scheduling problem need to do recovery */
			stream_info->buf[pingpong_bit] = done_buf;
			spin_unlock_irqrestore(&stream_info->lock, flags);
			msm_isp_halt_send_error(vfe_dev,
				ISP_EVENT_PING_PONG_MISMATCH);
			return;
		}
		msm_isp_cfg_stream_scratch(stream_info, pingpong_status);
	}
	if (!done_buf) {
		if (stream_info->buf_divert) {
			vfe_dev->error_info.stream_framedrop_count[
				stream_info->bufq_handle[
				VFE_BUF_QUEUE_DEFAULT] & 0xFF]++;
			vfe_dev->error_info.framedrop_flag = 1;
			if (vfe_dev->is_split) {
				other_vfe_id = OTHER_VFE(vfe_dev->pdev->id);
				temp_dev =
				vfe_dev->common_data->dual_vfe_res->vfe_dev[
					other_vfe_id];
				temp_dev->error_info.stream_framedrop_count[
				stream_info->bufq_handle[
				VFE_BUF_QUEUE_DEFAULT] & 0xFF]++;
				temp_dev->error_info.framedrop_flag = 1;
			}

		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
		return;
	}

	if (stream_info->stream_type == BURST_STREAM &&
		stream_info->runtime_num_burst_capture) {
		ISP_DBG("%s: burst_frame_count: %d\n",
			__func__,
			stream_info->runtime_num_burst_capture);
		stream_info->runtime_num_burst_capture--;
	}

	rc = msm_isp_update_deliver_count(vfe_dev, stream_info,
					pingpong_bit, done_buf);
	if (rc) {
		if (done_buf->is_drop_reconfig == 1)
			done_buf->is_drop_reconfig = 0;
		spin_unlock_irqrestore(&stream_info->lock, flags);
		pr_err_ratelimited("%s:VFE%d get done buf fail\n",
			__func__, vfe_dev->pdev->id);
		msm_isp_halt_send_error(vfe_dev,
			ISP_EVENT_PING_PONG_MISMATCH);
		return;
	}


	if ((done_buf->frame_id != frame_id) &&
		vfe_dev->axi_data.enable_frameid_recovery) {
		if (done_buf->is_drop_reconfig == 1)
			done_buf->is_drop_reconfig = 0;
		spin_unlock_irqrestore(&stream_info->lock, flags);
		msm_isp_handle_done_buf_frame_id_mismatch(vfe_dev,
			stream_info, done_buf, time_stamp, frame_id);
		return;
	}

	if (done_buf->is_drop_reconfig == 1) {
	/* When ping/pong buf is already reconfigured
	 * then dont issue buf-done for current buffer
	 */
		done_buf->is_drop_reconfig = 0;
		if (!stream_info->buf[pingpong_bit]) {
			/* samebuffer is not re-programeed so write scratch */
			msm_isp_cfg_stream_scratch(stream_info,
				pingpong_status);
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
	} else {
		/* If there is no regupdate from userspace then dont
		 * free buffer immediately, delegate it to RegUpdateAck
		 */
		if (stream_info->controllable_output &&
			!(vfe_dev->reg_update_requested &
				BIT((uint32_t)VFE_PIX_0))) {
			stream_info->pending_buf_info.is_buf_done_pending = 1;
			stream_info->pending_buf_info.buf = done_buf;
			stream_info->pending_buf_info.frame_id = frame_id;
		}
		spin_unlock_irqrestore(&stream_info->lock, flags);
		if (stream_info->pending_buf_info.is_buf_done_pending != 1) {
			msm_isp_process_done_buf(vfe_dev, stream_info,
				done_buf, time_stamp, frame_id);
		}
	}
}

void msm_isp_process_axi_irq(struct vfe_device *vfe_dev,
	uint32_t irq_status0, uint32_t irq_status1, uint32_t dual_irq_status,
	uint32_t pingpong_status, struct msm_isp_timestamp *ts)
{
	int i, rc = 0;
	uint32_t comp_mask = 0, wm_mask = 0;
	uint32_t stream_idx;
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_composite_info *comp_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int wm;

	if (vfe_dev->dual_vfe_sync_mode)
		comp_mask =
		vfe_dev->hw_info->vfe_ops.axi_ops.get_comp_mask(
			dual_irq_status, irq_status1);
	else
		comp_mask =
		vfe_dev->hw_info->vfe_ops.axi_ops.get_comp_mask(
			irq_status0, irq_status1);
	wm_mask =
		vfe_dev->hw_info->vfe_ops.axi_ops.get_wm_mask(
			irq_status0, irq_status1);

	if (!(comp_mask || wm_mask))
		return;

	ISP_DBG("%s: status0: 0x%x dual_irq_status %x\n",
	__func__, irq_status0, dual_irq_status);

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
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
								stream_idx);
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
			stream_info = msm_isp_get_stream_common_data(vfe_dev,
								stream_idx);
			msm_isp_process_axi_irq_stream(vfe_dev, stream_info,
						pingpong_status, ts);
		}
	}
}

void msm_isp_axi_disable_all_wm(struct vfe_device *vfe_dev)
{
	struct msm_vfe_axi_stream *stream_info;
	struct msm_vfe_axi_shared_data *axi_data = &vfe_dev->axi_data;
	int i, j;
	int vfe_idx;

	if (!vfe_dev || !axi_data) {
		pr_err("%s: error  %pK %pK\n", __func__, vfe_dev, axi_data);
		return;
	}

	for (i = 0; i < VFE_AXI_SRC_MAX; i++) {
		stream_info = msm_isp_get_stream_common_data(vfe_dev, i);

		if (stream_info->state != ACTIVE)
			continue;

		vfe_idx = msm_isp_get_vfe_idx_for_stream(vfe_dev,
				stream_info);
		for (j = 0; j < stream_info->num_planes; j++)
			vfe_dev->hw_info->vfe_ops.axi_ops.enable_wm(
				vfe_dev->vfe_base,
				stream_info->wm[vfe_idx][j], 0);
	}
}
