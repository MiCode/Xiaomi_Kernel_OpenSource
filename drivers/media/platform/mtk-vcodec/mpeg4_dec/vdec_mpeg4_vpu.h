/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Rick Chang <rick.chang@mediatek.com>
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

#ifndef _VDEC_MPEG4_VPU_H_
#define _VDEC_MPEG4_VPU_H_

struct vdec_mpeg4_mem {
	uint32_t size;
	uint64_t dma_addr;
};

struct vdec_mpeg4_wb {
	uint32_t vpu_addr;
	uint64_t dma_addr;
};

struct vdec_mpeg4_bs {
	uint32_t size;
	uint64_t dma_addr;
#ifdef DEBUG_HW_PARSE
	uint64_t va_addr;
#endif
};

struct vdec_mpeg4_fb {
	uint64_t y_dma_addr;
	uint64_t c_dma_addr;
	uint64_t fb_va;
};

struct vdec_mpeg4_vpu_inst;


/*
 * Note these functions are not thread-safe for the same decoder instance.
 * the reason is |signaled|. In h264_dec_vpu_wait_ack,
 * wait_event_interruptible_timeout waits |signaled| to be 1.
 * Suppose wait_event_interruptible_timeout returns and the execution has not
 * reached line 127. If another thread calls vdec_h264_vpu_dec_end,
 * |signaled| will be 1 and wait_event_interruptible_timeout will return
 *  immediately. We enusure thread-safe to add mtk_vdec_lock()/unlock() in
 * vdec_drv_if.c
 *
 */

struct vdec_mpeg4_vpu_inst *vdec_mpeg4_vpu_alloc(void *ctx);
void vdec_mpeg4_vpu_free(struct vdec_mpeg4_vpu_inst *vpu);
void *vdec_mpeg4_vpu_get_shmem(struct vdec_mpeg4_vpu_inst *vpu);
dma_addr_t vdec_mpeg4_vpu_get_dma(struct vdec_mpeg4_vpu_inst *vpu, uint32_t vpu_addr);
int vdec_mpeg4_vpu_init(struct vdec_mpeg4_vpu_inst *vpu, struct vdec_mpeg4_bs *bs);
int vdec_mpeg4_vpu_dec_start(struct vdec_mpeg4_vpu_inst *vpu, struct vdec_mpeg4_bs *bs,
			     struct vdec_mpeg4_fb *fb);
int vdec_mpeg4_vpu_dec_end(struct vdec_mpeg4_vpu_inst *vpu);
int vdec_mpeg4_vpu_deinit(struct vdec_mpeg4_vpu_inst *vpu);
int vdec_mpeg4_vpu_reset(struct vdec_mpeg4_vpu_inst *vpu);

#endif
