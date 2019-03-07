/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Yunfei Dong <yunfei.dong@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_VCODEC_DEC_STATELESS_H_
#define _MTK_VCODEC_DEC_STATELESS_H_

#include "mtk_vcodec_drv.h"

#define valid_cid_control(ctx, ctrl)                    \
	do { \
		if (!ctx->ctrls[(ctrl)]) {                      \
			mtk_v4l2_err("Invalid " #ctrl " control\n"); \
			ret = -EINVAL; \
		} \
	} while (0)

/**
 * struct mtk_slice_control  - CID control type
 * @id	: CID control id
 * @v4l2_ctrl_type	: CID control type
 * @name      : CID control name
 * @codec_type      : codec type (V4L2 pixel format) for CID control type
 */
struct mtk_slice_control {
	u32 id;
	enum v4l2_ctrl_type type;
	const char *name;
	int codec_type;
};

/**
 * enum mtk_slice_control_id - MTK CID controls id
 */
enum mtk_slice_control_id {
	MTK_SLICE_CTRL_DEC_H264_SPS = 0,
	MTK_SLICE_CTRL_DEC_H264_PPS,
	MTK_SLICE_CTRL_DEC_H264_SCALING_MATRIX,
	MTK_SLICE_CTRL_DEC_H264_SLICE_PARAM,
	MTK_SLICE_CTRL_DEC_H264_DECODE_PARAM,
	MTK_MIN_BUFFERS_FOR_CAPTURE,
};

/**
 * mtk_put_out_to_done() - put output buffer to done list
 * @ctx	: [in] v4l2 context
 * @bs	: [in] output buffer address info
 * @flags	: [in] buffer flags
 */
void mtk_vdec_stateless_out_to_done(struct mtk_vcodec_ctx *ctx,
				    struct mtk_vcodec_mem *bs, int flags);

/**
 * mtk_put_cap_to_disp() - put capture buffer to done list
 * @ctx	: [in] v4l2 context
 * @fb	: [in] capture buffer address info
 * @flags	: [in] buffer flags
 */
void mtk_vdec_stateless_cap_to_disp(struct mtk_vcodec_ctx *ctx,
				    struct vdec_fb *fb, int flags);

#endif /* _MTK_VCODEC_DEC_STATELESS_H_ */
