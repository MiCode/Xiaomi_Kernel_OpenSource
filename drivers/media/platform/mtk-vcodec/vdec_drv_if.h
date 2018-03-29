/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
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

#ifndef _VDEC_DRV_IF_H_
#define _VDEC_DRV_IF_H_

#include "mtk_vcodec_drv.h"
#include "mtk_vcodec_dec.h"
#include "mtk_vcodec_util.h"

/**
 * struct vdec_fb_status  - decoder frame buffer status
 * @FB_ST_NORMAL	: initial state
 * @FB_ST_DISPLAY	: frmae buffer is ready to be displayed
 * @FB_ST_FREE		: frame buffer is not used by decoder any more
 */
enum vdec_fb_status {
	FB_ST_NORMAL		= 0,
	FB_ST_DISPLAY		= (1 << 0),
	FB_ST_FREE		= (1 << 1)
};

enum vdec_get_param_type {
	GET_PARAM_DISP_FRAME_BUFFER,
	GET_PARAM_FREE_FRAME_BUFFER,
	GET_PARAM_PIC_INFO,
	GET_PARAM_CROP_INFO,
	GET_PARAM_DPB_SIZE
};

/**
 * struct vdec_fb_node  - decoder frame buffer node
 * @list	: list to hold this node
 * @fb		: point to frame buffer (vdec_fb)
 */
struct vdec_fb_node {
	struct list_head list;
	void *fb;
};


/**
 * For the same vdec_handle, these functions below are not thread safe.
 * For different vdec_handle, these functions can be called at the same time.
 */

/**
 * vdec_if_create() - create video decode handle according video format
 * @ctx    : [in] v4l2 context
 * @fmt    : [in] video format fourcc, V4L2_PIX_FMT_H264/VP8/VP9..
 */
int vdec_if_create(struct mtk_vcodec_ctx *ctx, unsigned int fourcc);

/**
 * vdec_if_release() - release decode driver.
 * @handle : [in] video decode handle to be released
 *
 * need to perform driver deinit before driver release.
 */
int vdec_if_release(struct mtk_vcodec_ctx *ctx);

/**
 * vdec_if_init() - initialize decode driver
 * @handle   : [in] video decode handle
 * @bs       : [in] input bitstream
 * @pic	     : [out] width and height of bitstream
 */
int vdec_if_init(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
		 struct vdec_pic_info *pic);

/**
 * vdec_if_get_param() - get driver's parameter
 * @handle : [in] video decode handle
 * @type   : [in] input parameter type
 * @out    : [out] buffer to store query result
 */
int vdec_if_get_param(struct mtk_vcodec_ctx *ctx, enum vdec_get_param_type type,
		      void *out);

/**
 * vdec_if_decode() - trigger decode
 * @handle  : [in] video decode handle
 * @bs      : [in] input bitstream
 * @fb      : [in] frame buffer to store decoded frame
 * @res_chg : [out] resolution change happen
 *
 * while EOF flush decode, need to set input bitstream as NULL
 */
int vdec_if_decode(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
		   struct vdec_fb *fb, bool *res_chg);

#endif
