/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 *		   Tiffany Lin <tiffany.lin@mediatek.com>
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
 * enum vdec_src_chg_type - decoder src change type
 * @VDEC_NO_CHANGE      : no change
 * @VDEC_RES_CHANGE     : resolution change
 * @VDEC_REALLOC_MV_BUF : realloc mv buf
 * @VDEC_HW_NOT_SUPPORT : hw not support
 */
enum vdec_src_chg_type {
	VDEC_NO_CHANGE              = (0 << 0),
	VDEC_RES_CHANGE             = (1 << 0),
	VDEC_REALLOC_MV_BUF         = (1 << 1),
	VDEC_HW_NOT_SUPPORT         = (1 << 2),
};

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

/* For GET_PARAM_DISP_FRAME_BUFFER and GET_PARAM_FREE_FRAME_BUFFER,
 * the caller does not own the returned buffer. The buffer will not be
 *				released before vdec_if_deinit.
 * GET_PARAM_DISP_FRAME_BUFFER	: get next displayable frame buffer,
 *				struct vdec_fb**
 * GET_PARAM_FREE_FRAME_BUFFER	: get non-referenced framebuffer, vdec_fb**
 * GET_PARAM_PIC_INFO		: get picture info, struct vdec_pic_info*
 * GET_PARAM_CROP_INFO		: get crop info, struct v4l2_crop*
 * GET_PARAM_DPB_SIZE		: get dpb size, unsigned int*
 * GET_PARAM_FRAME_INTERVAL	: get frame interval info*
 * GET_PARAM_ERRORMB_MAP	: get error mocroblock when decode error*
 */
enum vdec_get_param_type {
	GET_PARAM_DISP_FRAME_BUFFER,
	GET_PARAM_FREE_FRAME_BUFFER,
	GET_PARAM_PIC_INFO,
	GET_PARAM_CROP_INFO,
	GET_PARAM_DPB_SIZE,
	GET_PARAM_FRAME_INTERVAL,
	GET_PARAM_ERRORMB_MAP
};

/*
 * enum vdec_set_param_type - The type of set parameter used in vdec_if_set_param()
 * (VCU related: If you change the order, you must also update the VCU codes.)
 * VDEC_SET_PARAM_DECODE_MODE			: set decoder mode*
 * VDEC_SET_PARAM_FRAME_SIZE			: set container frame size*
 * VDEC_SET_PARAM_FIXED_MAX_OUTPUT_BUFFER	: set fixed maximum buffer size*
 * VDEC_SET_PARAM_UFO_MODE			: set UFO mode*
 * SET_PARAM_CRC_PATH				: CRC path*
 * SET_PARAM_GOLDEN_PATH			: GOLDEN path*
 */
enum vdec_set_param_type {
	SET_PARAM_DECODE_MODE,
	SET_PARAM_FRAME_SIZE,
	SET_PARAM_SET_FIXED_MAX_OUTPUT_BUFFER,
	SET_PARAM_UFO_MODE,
	SET_PARAM_CRC_PATH,
	SET_PARAM_GOLDEN_PATH
};

/**
 * struct vdec_fb_node  - decoder frame buffer node
 * @list	: list to hold this node
 * @fb	: point to frame buffer (vdec_fb), fb could point to frame buffer and
 *	working buffer this is for maintain buffers in different state
 */
struct vdec_fb_node {
	struct list_head list;
	struct vdec_fb *fb;
};

/**
 * vdec_if_init() - initialize decode driver
 * @ctx	: [in] v4l2 context
 * @fourcc	: [in] video format fourcc, V4L2_PIX_FMT_H264/VP8/VP9..
 */
int vdec_if_init(struct mtk_vcodec_ctx *ctx, unsigned int fourcc);

/**
 * vdec_if_deinit() - deinitialize decode driver
 * @ctx	: [in] v4l2 context
 *
 */
void vdec_if_deinit(struct mtk_vcodec_ctx *ctx);

/**
 * vdec_if_decode() - trigger decode
 * @ctx	: [in] v4l2 context
 * @bs	: [in] input bitstream
 * @fb	: [in] frame buffer to store decoded frame, when null menas parse
 *	header only
 * @res_chg	: [out] resolution change happens if current bs have different
 *	picture width/height
 * Note: To flush the decoder when reaching EOF, set input bitstream as NULL.
 */
int vdec_if_decode(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
		   struct vdec_fb *fb, unsigned int *src_chg);

/**
 * vdec_if_get_param() - get driver's parameter
 * @ctx	: [in] v4l2 context
 * @type	: [in] input parameter type
 * @out	: [out] buffer to store query result
 */
int vdec_if_get_param(struct mtk_vcodec_ctx *ctx, enum vdec_get_param_type type,
		      void *out);

/*
 * vdec_if_set_param - Set parameter to driver
 * @ctx	: [in] v4l2 context
 * @type	: [in] input parameter type
 * @out	: [in] input parameter
 * Return: 0 if setting param successfully, otherwise it is failed.
 */
int vdec_if_set_param(struct mtk_vcodec_ctx *ctx,
		      enum vdec_set_param_type type,
		      void *in);
#endif
