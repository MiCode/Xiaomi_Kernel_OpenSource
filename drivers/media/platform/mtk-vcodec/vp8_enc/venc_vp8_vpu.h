/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Daniel Hsiao <daniel.hsiao@mediatek.com>
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

#ifndef _VENC_VP8_VPU_H_
#define _VENC_VP8_VPU_H_

int vp8_enc_vpu_init(struct venc_vp8_inst *inst);
int vp8_enc_vpu_set_param(struct venc_vp8_inst *inst, unsigned int id,
			  void *param);
int vp8_enc_vpu_encode(struct venc_vp8_inst *inst,
		       struct venc_frm_buf *frm_buf,
		       struct mtk_vcodec_mem *bs_buf);
int vp8_enc_vpu_deinit(struct venc_vp8_inst *inst);

#endif
