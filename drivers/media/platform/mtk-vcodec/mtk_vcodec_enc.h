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


#ifndef _MTK_VCODEC_ENC_H_
#define _MTK_VCODEC_ENC_H_

#include <media/videobuf2-core.h>

/**
 * struct mtk_video_enc_buf - Private data related to each VB2 buffer.
 * @b:			Pointer to related VB2 buffer.
 * @param_change:	Types of encode parameter change before encode this
 *			buffer
 * @enc_params		Encode parameters changed before encode this buffer
 */
struct mtk_video_enc_buf {
	struct vb2_buffer b;
	struct list_head list;

	enum mtk_encode_param param_change;
	struct mtk_enc_params enc_params;
};

int mtk_venc_unlock(struct mtk_vcodec_ctx *ctx);
int mtk_venc_lock(struct mtk_vcodec_ctx *ctx);
int m2mctx_venc_queue_init(void *priv, struct vb2_queue *src_vq,
	struct vb2_queue *dst_vq);
void mtk_vcodec_venc_release(struct mtk_vcodec_ctx *ctx);
int mtk_venc_ctrls_setup(struct mtk_vcodec_ctx *ctx);
void mtk_venc_ctrls_free(struct mtk_vcodec_ctx *ctx);

#endif /* _MTK_VCODEC_ENC_H_ */
