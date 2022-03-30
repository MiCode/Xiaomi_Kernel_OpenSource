/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#ifndef _MTK_PQDIP_V4L2_VNODE_H_
#define _MTK_PQDIP_V4L2_VNODE_H_

#include <linux/videodev2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-hw.h"

#include "mtk_imgsys_v4l2.h"
#include "mtk_header_desc.h"

#define defaultdesc 0

#define MTK_PQDIP_OUTPUT_MIN_WIDTH		2U
#define MTK_PQDIP_OUTPUT_MIN_HEIGHT		2U
#define MTK_PQDIP_OUTPUT_MAX_WIDTH		5376U
#define MTK_PQDIP_OUTPUT_MAX_HEIGHT		4032U
#define MTK_PQDIP_CAPTURE_MIN_WIDTH		2U
#define MTK_PQDIP_CAPTURE_MIN_HEIGHT		2U
#define MTK_PQDIP_CAPTURE_MAX_WIDTH		5376U
#define MTK_PQDIP_CAPTURE_MAX_HEIGHT		4032U

static const struct mtk_imgsys_dev_format pqdip_pimgi_fmts[] = {
	{
		.format = V4L2_PIX_FMT_VYUY,
		.depth	 = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.depth	 = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.depth	 = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format	= V4L2_PIX_FMT_YUV420M,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_YVU420M,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_NV12M,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.num_cplanes = 1,
	},
	{	// Must have for SMVR/Multis-cale for every video_device nodes
		.format = V4L2_META_FMT_MTISP_DESC,
		.num_planes = 1,
#if defaultdesc
		.depth = { 8 },
		.row_depth = { 8 },
		.num_cplanes = 1,
#endif
		.buffer_size = sizeof(struct header_desc),
	},
	{
		.format = V4L2_META_FMT_MTISP_SD,
		.num_planes = 1,
#if defaultdesc
		.depth = { 8 },
		.row_depth = { 8 },
		.num_cplanes = 1,
#endif
		.buffer_size = sizeof(struct singlenode_desc),
	},
	{
		.format = V4L2_META_FMT_MTISP_DESC_NORM,
		.num_planes = 1,
		.buffer_size = sizeof(struct header_desc_norm),
	},

};

static const struct mtk_imgsys_dev_format pqdip_wroto_fmts[] = {
	{
		.format = V4L2_PIX_FMT_VYUY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_YVU420,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 3,
		.pass_1_align = 4,
	},
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
		.pass_1_align = 4,
	},
	{
		.format	= V4L2_PIX_FMT_YUV420M,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format	= V4L2_PIX_FMT_YVU420M,
		.depth		= { 8, 2, 2 },
		.row_depth	= { 8, 4, 4 },
		.num_planes	= 3,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{
		.format	= V4L2_PIX_FMT_NV12M,
		.depth		= { 8, 4 },
		.row_depth	= { 8, 8 },
		.num_planes	= 2,
		.num_cplanes = 1,
		.pass_1_align = 4,
	},
	{	// Must have for SMVR/Multis-cale for every video_device nodes
		.format = V4L2_META_FMT_MTISP_DESC,
		.num_planes = 1,
#if defaultdesc
		.depth = { 8 },
		.row_depth = { 8 },
		.num_cplanes = 1,
#endif
		.buffer_size = sizeof(struct header_desc),
	},
	{
		.format = V4L2_META_FMT_MTISP_DESC_NORM,
		.num_planes = 1,
		.buffer_size = sizeof(struct header_desc_norm),
	},

};

static const struct mtk_imgsys_dev_format pqdip_tccso_fmts[] = {
	{
		.format	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	{	// Must have for SMVR/Multis-cale for every video_device nodes
		.format = V4L2_META_FMT_MTISP_DESC,
		.num_planes = 1,
#if defaultdesc
		.depth = { 8 },
		.row_depth = { 8 },
		.num_cplanes = 1,
#endif
		.buffer_size = sizeof(struct header_desc),
	},
	{
		.format = V4L2_META_FMT_MTISP_DESC_NORM,
		.num_planes = 1,
		.buffer_size = sizeof(struct header_desc_norm),
	},

};

static const struct v4l2_frmsizeenum pqdip_in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_PQDIP_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_PQDIP_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_PQDIP_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum pqdip_out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_PQDIP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_PQDIP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_PQDIP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_PQDIP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc pqdip_setting[] = {
	/* Input Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_PIMGI_OUT,
		.name = "PIMGI Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_pimgi_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_pimgi_fmts),
		.default_fmt_idx = 4,
		.default_width = MTK_PQDIP_OUTPUT_MAX_WIDTH,
		.default_height = MTK_PQDIP_OUTPUT_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &pqdip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Imgi image source",
	},
	/* Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WROT_A_CAPTURE,
		.name = "WROTO A Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_wroto_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_wroto_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.dma_port = 0,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Output quality enhanced image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WROT_B_CAPTURE,
		.name = "WROTO B Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_wroto_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_wroto_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.dma_port = 1,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Output quality enhanced image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TCCSO_A_CAPTURE,
		.name = "TCCSO A Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_tccso_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_tccso_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.dma_port = 2,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Output tone curve statistics",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TCCSO_B_CAPTURE,
		.name = "TCCSO B Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = pqdip_tccso_fmts,
		.num_fmts = ARRAY_SIZE(pqdip_tccso_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_PQDIP_CAPTURE_MAX_WIDTH,
		.dma_port = 3,
		.frmsizeenum = &pqdip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Output tone curve statistics",
	},
};

#endif // _MTK_PQDIP_V4L2_VNODE_H_
