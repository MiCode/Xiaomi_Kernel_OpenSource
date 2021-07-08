/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 *
 * Author: Floria Huang <floria.huang@mediatek.com>
 *
 */

#ifndef _MTK_WPE_V4L2_VNODE_H_
#define _MTK_WPE_V4L2_VNODE_H_

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

#define defaultdesc 0

#define MTK_WPE_OUTPUT_MIN_WIDTH		2U
#define MTK_WPE_OUTPUT_MIN_HEIGHT		2U
#define MTK_WPE_OUTPUT_MAX_WIDTH		18472U
#define MTK_WPE_OUTPUT_MAX_HEIGHT		13856U

#define MTK_WPE_MAP_OUTPUT_MIN_WIDTH	2U
#define MTK_WPE_MAP_OUTPUT_MIN_HEIGHT	2U
#define MTK_WPE_MAP_OUTPUT_MAX_WIDTH	640U
#define MTK_WPE_MAP_OUTPUT_MAX_HEIGHT	480U

#define MTK_WPE_PSP_OUTPUT_WIDTH	    8U
#define MTK_WPE_PSP_OUTPUT_HEIGHT	    33U

#define MTK_WPE_CAPTURE_MIN_WIDTH		2U
#define MTK_WPE_CAPTURE_MIN_HEIGHT		2U
#define MTK_WPE_CAPTURE_MAX_WIDTH		18472U
#define MTK_WPE_CAPTURE_MAX_HEIGHT		13856U


static const struct mtk_imgsys_dev_format wpe_wpei_fmts[] = {
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
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV12M,
		.depth      = { 8, 4 },
		.row_depth  = { 8, 8 },
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
	{
		.format = V4L2_PIX_FMT_NV21M,
		.depth      = { 8, 4 },
		.row_depth  = { 8, 8 },
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
	/* YUV422-10bit-1P-Packed */
	{
		.format	= V4L2_PIX_FMT_YUYV_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_YVYU_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_UYVY_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_VYUY_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
#if 0
	/* Bayer 8 bit */
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
#endif
	/* Bayer 10 bit */
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	/* Bayer 12 bit */
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
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

static const struct mtk_imgsys_dev_format wpe_veci_fmts[] = {
	/* WarpMap, 2 plane, packed in 4-byte */
	{
		.format = V4L2_PIX_FMT_WARP2P,
		.depth = { 32 },
		.row_depth = { 32 },
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


static const struct mtk_imgsys_dev_format wpe_pspi_fmts[] = {
	{
		.format = V4L2_PIX_FMT_GREY,
		.depth = { 32 },
		.row_depth = { 32 },
		.num_planes = 1,
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

static const struct mtk_imgsys_dev_format wpe_wpeo_fmts[] = {
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
	/* YUV420, 2 plane 8 bit */
	{
		.format = V4L2_PIX_FMT_NV12,
		.depth = { 12 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 2,
	},
	{
		.format = V4L2_PIX_FMT_NV12M,
		.depth      = { 8, 4 },
		.row_depth  = { 8, 8 },
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
	{
		.format = V4L2_PIX_FMT_NV21M,
		.depth      = { 8, 4 },
		.row_depth  = { 8, 8 },
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
	/* YUV422-10bit-1P-Packed */
	{
		.format	= V4L2_PIX_FMT_YUYV_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_YVYU_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_UYVY_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
	{
		.format	= V4L2_PIX_FMT_VYUY_Y210P,
		.depth		= { 20 },
		.row_depth	= { 10 },
		.num_planes	= 1,
		.num_cplanes = 1,
	},
#if 0
	/* Bayer 8 bit */
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB8,
		.depth = { 8 },
		.row_depth = { 8 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 64,//wpei, 64-pix align
	},
#endif
	/* Bayer 10 bit */
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB10,
		.depth = { 10 },
		.row_depth = { 10 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 80,//wpei, 64-pix align
	},
	/* Bayer 12 bit */
	{
		.format = V4L2_PIX_FMT_MTISP_SBGGR12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGBRG12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SGRBG12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
	},
	{
		.format = V4L2_PIX_FMT_MTISP_SRGGB12,
		.depth = { 12 },
		.row_depth = { 12 },
		.num_planes = 1,
		.num_cplanes = 1,
		.pass_1_align = 128,//wpei, 64-pix align
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

static const struct mtk_imgsys_dev_format wpe_msko_fmts[] = {
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

static const struct v4l2_frmsizeenum wpe_in_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_WPE_CAPTURE_MAX_WIDTH,
	.stepwise.min_width = MTK_WPE_CAPTURE_MIN_WIDTH,
	.stepwise.max_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
	.stepwise.min_height = MTK_WPE_CAPTURE_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum wpe_in_map_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_WPE_MAP_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_WPE_MAP_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_WPE_MAP_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_WPE_MAP_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct v4l2_frmsizeenum wpe_in_psp_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_WPE_PSP_OUTPUT_WIDTH,
	.stepwise.min_width = MTK_WPE_PSP_OUTPUT_WIDTH,
	.stepwise.max_height = MTK_WPE_PSP_OUTPUT_HEIGHT,
	.stepwise.min_height = MTK_WPE_PSP_OUTPUT_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};


static const struct v4l2_frmsizeenum wpe_out_frmsizeenum = {
	.type = V4L2_FRMSIZE_TYPE_CONTINUOUS,
	.stepwise.max_width = MTK_WPE_OUTPUT_MAX_WIDTH,
	.stepwise.min_width = MTK_WPE_OUTPUT_MIN_WIDTH,
	.stepwise.max_height = MTK_WPE_OUTPUT_MAX_HEIGHT,
	.stepwise.min_height = MTK_WPE_OUTPUT_MIN_HEIGHT,
	.stepwise.step_height = 1,
	.stepwise.step_width = 1,
};

static const struct mtk_imgsys_video_device_desc wpe_setting[] = {
	/* Input Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WWPEI_OUT,
		.name = "WPEI_E Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpei_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpei_fmts),
		.default_fmt_idx = 4,
		.default_width = MTK_WPE_OUTPUT_MAX_WIDTH,
		.default_height = MTK_WPE_OUTPUT_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WPE main image input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WVECI_OUT,
		.name = "VECI_E Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_veci_fmts,
		.num_fmts = ARRAY_SIZE(wpe_veci_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 1,
		.frmsizeenum = &wpe_in_map_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WarpMap input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WPSP_COEFI_OUT,
		.name = "PSPI_E Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_pspi_fmts,
		.num_fmts = ARRAY_SIZE(wpe_pspi_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 2,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "PSP coef. table input",
	},
	/* WPE_EIS Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WWPEO_CAPTURE,
		.name = "WPEO_E Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpeo_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpeo_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WPE image output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WMSKO_CAPTURE,
		.name = "MSKO_E Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_msko_fmts,
		.num_fmts = ARRAY_SIZE(wpe_msko_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 1,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WPE valid map output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTWPEI_OUT,
		.name = "WPEI_T Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpei_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpei_fmts),
		.default_fmt_idx = 4,
		.default_width = MTK_WPE_OUTPUT_MAX_WIDTH,
		.default_height = MTK_WPE_OUTPUT_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WPE main image input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTVECI_OUT,
		.name = "VECI_T Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_veci_fmts,
		.num_fmts = ARRAY_SIZE(wpe_veci_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 1,
		.frmsizeenum = &wpe_in_map_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WarpMap input",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTPSP_COEFI_OUT,
		.name = "PSPI_T Input",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_pspi_fmts,
		.num_fmts = ARRAY_SIZE(wpe_pspi_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 2,
		.frmsizeenum = &wpe_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "PSP coef. table input",
	},
	/* WPE_TNR Output Video Node */
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTWPEO_CAPTURE,
		.name = "WPEO_T Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_wpeo_fmts,
		.num_fmts = ARRAY_SIZE(wpe_wpeo_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WPE image output",
	},
	{
		.id = MTK_IMGSYS_VIDEO_NODE_ID_WTMSKO_CAPTURE,
		.name = "MSKO_T Output",
		.cap = V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = wpe_msko_fmts,
		.num_fmts = ARRAY_SIZE(wpe_msko_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_WPE_CAPTURE_MAX_WIDTH,
		.default_height = MTK_WPE_CAPTURE_MAX_HEIGHT,
		.dma_port = 1,
		.frmsizeenum = &wpe_out_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_cap_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "WPE valid map output",
	},
};

#endif // _MTK_WPE_V4L2_VNODE_H_
