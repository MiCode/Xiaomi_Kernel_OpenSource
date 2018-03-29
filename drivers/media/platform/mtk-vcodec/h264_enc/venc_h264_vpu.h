/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Jungchang Tsao <jungchang.tsao@mediatek.com>
 *         Daniel Hsiao <daniel.hsiao@mediatek.com>
 *         PoChun Lin <pochun.lin@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _VENC_H264_VPU_H_
#define _VENC_H264_VPU_H_

int h264_enc_vpu_init(struct venc_h264_inst *inst);
int h264_enc_vpu_set_param(struct venc_h264_inst *inst, unsigned int id,
			   void *param);
int h264_enc_vpu_encode(struct venc_h264_inst *inst, unsigned int bs_mode,
			struct venc_frm_buf *frm_buf,
			struct mtk_vcodec_mem *bs_buf,
			unsigned int *bs_size);
int h264_enc_vpu_deinit(struct venc_h264_inst *inst);

#endif
