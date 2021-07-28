/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *
 */
#ifndef _MTK_IMGSYS_V4L2_VNODE_H_
#define _MTK_IMGSYS_V4L2_VNODE_H_

#include "mtk_imgsys-of.h"
#include "mtk_imgsys-dev.h"
#include "mtk_imgsys-vnode_id.h"
#include "modules/mtk_dip_v4l2_vnode.h"
#include "modules/mtk_traw_v4l2_vnode.h"
#include "modules/mtk_pqdip_v4l2_vnode.h"
#include "modules/mtk_wpe_v4l2_vnode.h"
#include "modules/mtk_me_v4l2_vnode.h"

/*
 * TODO: register module pipeline desc in module order
 */
enum mtk_imgsys_module_id {
	IMGSYS_MODULE_TRAW = 0,
	IMGSYS_MODULE_DIP,
	IMGSYS_MODULE_PQDIP,
	IMGSYS_MODULE_ME,
	IMGSYS_MODULE_WPE,
	IMGSYS_MODULE_ADL,
	IMGSYS_MODULE_MAIN,
	IMGSYS_MODULE_NUM,
};

static const struct mtk_imgsys_mod_pipe_desc module_pipe_isp7[] = {
	[IMGSYS_MODULE_TRAW] = {
		.vnode_desc = traw_setting,
		.node_num = ARRAY_SIZE(traw_setting),
	},
	[IMGSYS_MODULE_DIP] = {
		.vnode_desc = dip_setting,
		.node_num = ARRAY_SIZE(dip_setting),
	},
	[IMGSYS_MODULE_PQDIP] = {
		.vnode_desc = pqdip_setting,
		.node_num = ARRAY_SIZE(pqdip_setting),
	},
	[IMGSYS_MODULE_ME] = {
		.vnode_desc = me_setting,
		.node_num = ARRAY_SIZE(me_setting),
	},
	[IMGSYS_MODULE_WPE] = {
		.vnode_desc = wpe_setting,
		.node_num = ARRAY_SIZE(wpe_setting),
	},
	[IMGSYS_MODULE_ADL] = {
		.vnode_desc = NULL,
		.node_num = 0,
	},
	[IMGSYS_MODULE_MAIN] = {
		.vnode_desc = NULL,
		.node_num = 0,
	}
};


#define MTK_IMGSYS_MODULE_VNUM ARRAY_SIZE(module_pipe_isp7)



static const struct mtk_imgsys_dev_format fw_param_fmts[] = {
#if MTK_CM4_SUPPORT
	{
		.format = V4L2_META_FMT_MTISP_PARAMS,
		.buffer_size = 1024 * (128 + 288),
	},
#else
	{
		.format = V4L2_META_FMT_MTISP_PARAMS,
		.buffer_size = DIP_TUNING_SZ,
	},
	{
		.format = V4L2_META_FMT_MTISP_PARAMS,
		.buffer_size = sizeof(struct dip_param),
	},
#endif
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

static const struct mtk_imgsys_dev_format sd_fmts[] = {
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
		.format = V4L2_META_FMT_MTISP_SDNORM,
		.num_planes = 1,
		.buffer_size = sizeof(struct singlenode_desc_norm),
	},
};

static struct mtk_imgsys_video_device_desc
queues_setting[MTK_IMGSYS_VIDEO_NODE_ID_TOTAL_NUM] = {
	[MTK_IMGSYS_VIDEO_NODE_ID_TUNING_OUT] = {
		.id = MTK_IMGSYS_VIDEO_NODE_ID_TUNING_OUT,
		.name = "Tuning",
		.cap = V4L2_CAP_META_OUTPUT | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.smem_alloc = 0, //meta:1
		.flags = 0,
		.fmts = fw_param_fmts,
		.num_fmts = ARRAY_SIZE(fw_param_fmts),
		.default_fmt_idx = 2, //0,
		.dma_port = 0,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_meta_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_meta_ops,
		.description = "Tuning data",
	},
	[MTK_IMGSYS_VIDEO_NODE_ID_CTRLMETA_OUT] = {
		.id = MTK_IMGSYS_VIDEO_NODE_ID_CTRLMETA_OUT,
		.name = "CtrlMeta",
		.cap = V4L2_CAP_META_OUTPUT | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_META_OUTPUT,
		.smem_alloc = 0, //meta:1
		.flags = 0,
		.fmts = fw_param_fmts,
		.num_fmts = ARRAY_SIZE(fw_param_fmts),
		.default_fmt_idx = 2, //1,
		.dma_port = 0,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_meta_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_meta_ops,
		.description = "Control meta data for flow control",
	},
	[MTK_IMGSYS_VIDEO_NODE_ID_SIGDEV_OUT] = {
		.id = MTK_IMGSYS_VIDEO_NODE_ID_SIGDEV_OUT,
		.name = "Single Device",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = sd_fmts,
		.num_fmts = ARRAY_SIZE(sd_fmts),
		.default_fmt_idx = 0,
		.default_width = MTK_DIP_OUTPUT_MAX_WIDTH,
		.default_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Single Device Node",
	},
	[MTK_IMGSYS_VIDEO_NODE_ID_SIGDEV_NORM_OUT] = {
		.id = MTK_IMGSYS_VIDEO_NODE_ID_SIGDEV_NORM_OUT,
		.name = "SIGDEVN",
		.cap = V4L2_CAP_VIDEO_OUTPUT_MPLANE | V4L2_CAP_STREAMING,
		.buf_type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
		.smem_alloc = 0,
		.flags = MEDIA_LNK_FL_DYNAMIC,
		.fmts = sd_fmts,
		.num_fmts = ARRAY_SIZE(sd_fmts),
		.default_fmt_idx = 1,
		.default_width = MTK_DIP_OUTPUT_MAX_WIDTH,
		.default_height = MTK_DIP_OUTPUT_MAX_HEIGHT,
		.dma_port = 0,
		.frmsizeenum = &dip_in_frmsizeenum,
		.ops = &mtk_imgsys_v4l2_video_out_ioctl_ops,
		.vb2_ops = &mtk_imgsys_vb2_video_ops,
		.description = "Single Device Norm",
	},
};
#ifdef MULTI_PIPE_SUPPORT
static struct mtk_imgsys_video_device_desc
reprocess_queues_setting[MTK_IMGSYS_VIDEO_NODE_ID_TOTAL_NUM];
#endif

static const struct mtk_imgsys_pipe_desc
pipe_settings_isp7[MTK_IMGSYS_PIPE_ID_TOTAL_NUM] = {
	{
		.name = MTK_DIP_DEV_DIP_PREVIEW_NAME,
		.id = MTK_IMGSYS_PIPE_ID_PREVIEW,
		.queue_descs = queues_setting,
		.total_queues = ARRAY_SIZE(queues_setting),
	},
#ifdef MULTI_PIPE_SUPPORT
	{
		.name = MTK_DIP_DEV_DIP_CAPTURE_NAME,
		.id = MTK_IMGSYS_PIPE_ID_CAPTURE,
		.queue_descs = queues_setting,
		.total_queues = ARRAY_SIZE(queues_setting),

	},
	{
		.name = MTK_DIP_DEV_DIP_REPROCESS_NAME,
		.id = MTK_IMGSYS_PIPE_ID_REPROCESS,
		.queue_descs = reprocess_queues_setting,
		.total_queues = ARRAY_SIZE(reprocess_queues_setting),
	},
#endif
};
#endif
