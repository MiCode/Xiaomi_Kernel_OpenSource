/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */

#ifndef _MTK_DIP_V4L2_VNODE_H_
#define _MTK_DIP_V4L2_VNODE_H_

/*#include <linux/platform_device.h>
 * #include <linux/module.h>
 * #include <linux/of_device.h>
 * #include <linux/pm_runtime.h>
 * #include <linux/remoteproc.h>
 * #include <linux/remoteproc/mtk_scp.h>
 */
#include <linux/videodev2.h>
#include <media/videobuf2-dma-contig.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-hw.h"

#include "mtk_imgsys_v4l2.h"
#include "mtk_header_desc.h"
#include "mtk_imgsys-vnode_id.h"

#define defaultdesc 0

static const struct mtk_imgsys_dev_format dip_imgi_fmts[] = {
	/* YUV422, 1 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_UYVY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_VYUY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	/* YUV422, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV16,
		.depth = { 16 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV61,
		.depth = { 16 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* YUV422, 3 plane 8 bit */
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* YUV420, 3 plane 8 bit */
	{
		.format	= V4L2_PIX_FMT_YUV420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 3,
	},
	{
		.format	= V4L2_PIX_FMT_YVU420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 3,
	},
	/* Y8 bit */
	{
		.format	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_vipi_fmts[] = {
	/* YUV422, 1 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_UYVY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_VYUY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	/* YUV422, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV16,
		.depth = { 16 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV61,
		.depth = { 16 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* YUV422, 3 plane 8 bit */
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* YUV420, 3 plane 8 bit */
	{
		.format	= V4L2_PIX_FMT_YUV420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 3,
	},
	{
		.format	= V4L2_PIX_FMT_YVU420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 3,
	},
	/* Y8 bit */
	{
		.format	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_rec_dsi_fmts[] = {
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_rec_dpi_fmts[] = {
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_meta_fmts[] = {
#if 0
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
#endif
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_tnrli_fmts[] = {
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_img2o_fmts[] = {
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* Y8 bit */
	{
		.format	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_img3o_fmts[] = {
	/* YUV422, 1 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_UYVY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YUYV,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_YVYU,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	{
		.format = V4L2_PIX_FMT_VYUY,
		.depth = { 16 },
		.row_depth = { 16 },
		.num_planes = 1,
		.num_cplanes = 1,
	},
	/* YUV422, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV16,
		.depth = { 16 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV61,
		.depth = { 16 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* YUV422, 3 plane 8 bit */
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* YUV420, 3 plane 8 bit */
	{
		.format	= V4L2_PIX_FMT_YUV420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 3,
	},
	{
		.format	= V4L2_PIX_FMT_YVU420,
		.depth		= { 12 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 3,
	},
	/* Y8 bit */
	{
		.format	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct mtk_imgsys_dev_format dip_img4o_fmts[] = {
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV21,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	/* Y8 bit */
	{
		.format	= V4L2_PIX_FMT_GREY,
		.depth		= { 8 },
		.row_depth	= { 8 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	/* Must have for SMVR/Multis-cale for every video_device nodes */
	{
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

static const struct v4l2_frmsizeenum dip_in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum dip_out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_DIP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_DIP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_DIP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc dip_setting[] = {
	/* Input Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMGI_OUT,
		.name = "Imgi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_imgi_fmts,
		.num_fmts = ARRAY_SIZE(dip_imgi_fmts),
		.default_fmt_idx = 4,
		.default_width = MTK_DIP_OUTPUT_MAX_WIDTH,
		.default_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Main image source",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_VIPI_OUT,
		.name = "Vipi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_vipi_fmts,
		.num_fmts = ARRAY_SIZE(dip_vipi_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 1,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Vipi image source",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_REC_DSI_OUT,
		.name = "Rec_Dsi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_rec_dsi_fmts,
		.num_fmts = ARRAY_SIZE(dip_rec_dsi_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 2,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Down Source Image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_REC_DPI_OUT,
		.name = "Rec_Dpi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_rec_dpi_fmts,
		.num_fmts = ARRAY_SIZE(dip_rec_dpi_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 3,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Down Processed Image",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_CNR_BLURMAPI_OUT,
		.name = "Bokeh Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 5,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Bokehi data",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_LFEI_OUT,
		.name = "Dmgi_FM Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 6,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Dmgi_FM data",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_RFEI_OUT,
		.name = "Depi_FM Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 7,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Depi_FM data",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRSI_OUT,
		.name = "Tnrsi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 8,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Statistics input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRWI_OUT,
		.name = "Tnrwi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 9,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Weighting input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRMI_OUT,
		.name = "Tnrmi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 10,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Motion Map input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRCI_OUT,
		.name = "Tnrci Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 11,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Confidence Map input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRLI_OUT,
		.name = "Tnrli Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_tnrli_fmts,
		.num_fmts = ARRAY_SIZE(dip_tnrli_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 12,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Low Frequency Diff input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRVBI_OUT,
		.name = "Tnrvbi Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 13,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Valid Bit Map input",
	},
	/* Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMG2O_CAPTURE,
		.name = "Img2o Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_img2o_fmts,
		.num_fmts = ARRAY_SIZE(dip_img2o_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &dip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Resized output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMG3O_CAPTURE,
		.name = "Img3o Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_img3o_fmts,
		.num_fmts = ARRAY_SIZE(dip_img3o_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 1,
		.frmsizeenum = &dip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Dip output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_IMG4O_CAPTURE,
		.name = "Img4o Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_img4o_fmts,
		.num_fmts = ARRAY_SIZE(dip_img4o_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 2,
		.frmsizeenum = &dip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Nr3d output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_FMO_CAPTURE,
		.name = "FM Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 3,
		.frmsizeenum = &dip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "FM output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRSO_CAPTURE,
		.name = "Tnrso Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 4,
		.frmsizeenum = &dip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "statistics output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRWO_CAPTURE,
		.name = "Tnrwo Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 5,
		.frmsizeenum = &dip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Weighting output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TNRMO_CAPTURE,
		.name = "Tnrmo Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = dip_meta_fmts,
		.num_fmts = ARRAY_SIZE(dip_meta_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_CAPTURE_MAX_WIDTH,
		.default_height = MTK_DIP_CAPTURE_MAX_HEIGHT,
		.dma_port = 6,
		.frmsizeenum = &dip_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Motion Map output",
	},
};

#endif // _MTK_DIP_V4L2_VNODE_H_
