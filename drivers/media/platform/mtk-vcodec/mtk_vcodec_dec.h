/*
* Copyright (c) 2015 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*         Tiffany Lin <tiffany.lin@mediatek.com>
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

#ifndef _MTK_VCODEC_DEC_H_
#define _MTK_VCODEC_DEC_H_

#include <media/videobuf2-core.h>

/**
 * struct vdec_fb  - decoder frame buffer
 * @base_y	: Y plane memory info
 * @base_c	: C plane memory info
 * @status      : frame buffer status (vdec_fb_status)
 */
struct vdec_fb {
	struct mtk_vcodec_mem base_y;
	struct mtk_vcodec_mem base_c;
	unsigned int status;
};

/**
 * struct mtk_video_buf - Private data related to each VB2 buffer.
 * @b:			VB2 buffer
 * @list:			link list
 * @used:		Output buffer contain decoded frame data
 * @ready_to_display:	Output buffer not display yet
 * @nonrealdisplay:	Output buffer is not display frame
 * @queued_in_vb2:	Output buffer is queue in vb2
 * @queued_in_v4l2:	Output buffer is in v4l2
 * @lastframe:		Intput buffer is last buffer - EOS
 * @frame_buffer:		Decode status of output buffer
 * @lock:			V4L2 and decode thread should get mutex
 *			before r/w info in mtk_video_buf
 */
struct mtk_video_buf {
	struct vb2_buffer b;
	struct list_head list;

	bool used;
	bool ready_to_display;
	bool nonrealdisplay;
	bool queued_in_vb2;
	bool queued_in_v4l2;
	bool lastframe;
	struct vdec_fb frame_buffer;
	struct mutex lock;
};

/*
 * mtk_vdec_lock/mtk_vdec_unlock are for ctx instance to
 * get/release lock before/after access decoder hw.
 * mtk_vdec_lock get decoder hw lock and set curr_ctx
 * to idx of ctx instance that get lock
 */
int mtk_vdec_unlock(struct mtk_vcodec_ctx *ctx);
int mtk_vdec_lock(struct mtk_vcodec_ctx *ctx);
int m2mctx_vdec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq);
void mtk_vcodec_dec_set_default_params(struct mtk_vcodec_ctx *ctx);
void mtk_vcodec_vdec_release(struct mtk_vcodec_ctx *ctx);
int mtk_vdec_ctrls_setup(struct mtk_vcodec_ctx *ctx);
void mtk_vdec_ctrls_free(struct mtk_vcodec_ctx *ctx);

#endif /* _MTK_VCODEC_DEC_H_ */
