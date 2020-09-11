/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include "mtk_rsc.h"
#include "mtk_rsc-ctx.h"
#include "mtk_rsc-v4l2.h"


static struct mtk_rsc_ctx_format in_fmts[] = {
	{
		.fmt.img = {
			.pixelformat  = V4L2_PIX_FMT_MTISP_B8,
			.depth    = { 8 },
			.row_depth  = { 8 },
			.num_planes = 1,
		},
	},
	{
		.fmt.meta = {
		.dataformat = V4L2_META_FMT_MTISP_PARAMS,
		.max_buffer_size =
			MTK_ISP_CTX_RSC_TUNING_DATA_NUM * sizeof(u32),
		},
	},
};

static struct mtk_rsc_ctx_format out_fmts[] = {
	{
		.fmt.meta = {
		.dataformat = V4L2_META_FMT_MTISP_PARAMS,
		.max_buffer_size = sizeof(u32),
		},
	},
};

static struct mtk_rsc_ctx_queue_desc
output_queues[MTK_ISP_CTX_RSC_TOTAL_OUTPUT] = {
	{
		.id = MTK_ISP_CTX_RSC_PRE_RRZO_IN,
		.name = "pre_rrzo_in",
		.capture = 0,
		.image = 1,
		.fmts = in_fmts,
		.num_fmts = ARRAY_SIZE(in_fmts),
		.default_fmt_idx = 0,
	},
	{
		.id = MTK_ISP_CTX_RSC_CUR_RRZO_IN,
		.name = "cur_rrzo_in",
		.capture = 0,
		.image = 1,
		.fmts = in_fmts,
		.num_fmts = ARRAY_SIZE(in_fmts),
		.default_fmt_idx = 0,
	},
	{
		.id = MTK_ISP_CTX_RSC_TUNING_IN,
		.name = "tuning_in",
		.capture = 0,
		.image = 0,
		.fmts = in_fmts,
		.num_fmts = ARRAY_SIZE(in_fmts),
		.default_fmt_idx = 1,
	},

};

static struct mtk_rsc_ctx_queue_desc
capture_queues[MTK_ISP_CTX_RSC_TOTAL_CAPTURE] = {
	{
		.id = MTK_ISP_CTX_RSC_RESULT_OUT,
		.name = "result_out",
		.capture = 1,
		.image = 0,
		.fmts = out_fmts,
		.num_fmts = ARRAY_SIZE(out_fmts),
		.default_fmt_idx = 0,
	},
};


static struct mtk_rsc_ctx_queues_setting queues_setting = {
	.master = MTK_ISP_CTX_RSC_PRE_RRZO_IN,
	.output_queue_descs = output_queues,
	.total_output_queues = MTK_ISP_CTX_RSC_TOTAL_OUTPUT,
	.capture_queue_descs = capture_queues,
	.total_capture_queues = MTK_ISP_CTX_RSC_TOTAL_CAPTURE,
};

int mtk_isp_ctx_rsc_init(struct mtk_rsc_ctx *ctx)
{
	/* Initialize main data structure */
	return mtk_rsc_ctx_core_queue_setup(ctx, &queues_setting);
}
EXPORT_SYMBOL_GPL(mtk_isp_ctx_rsc_init);
