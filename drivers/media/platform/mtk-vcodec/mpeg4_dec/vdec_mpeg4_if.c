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

#include <linux/module.h>
#include <linux/slab.h>

#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"

#include "vdec_mpeg4_if.h"
#include "vdec_mpeg4_vpu.h"

#define MPEG4_MAX_MC_NUM 3

struct vdec_mpeg4_vsi {
	struct vdec_mpeg4_mem mc_buf[MPEG4_MAX_MC_NUM];
	struct vdec_mpeg4_mem dcacmv_buf;
	struct vdec_mpeg4_mem datapar_buf;
	struct vdec_mpeg4_mem mv_buf;
	struct vdec_mpeg4_wb wb;

	struct vdec_mpeg4_fb disp_fb;
	struct vdec_mpeg4_fb disp_ready_fb;
	struct vdec_mpeg4_fb free_fb;
	struct vdec_mpeg4_fb free_ready_fb;

	struct vdec_pic_info pic;

	uint32_t dec_mode;
};

struct vdec_mpeg4_inst {
	void *ctx;

	struct mtk_vcodec_mem mc_buf[MPEG4_MAX_MC_NUM];
	struct mtk_vcodec_mem dcacmv_buf;
	struct mtk_vcodec_mem datapar_buf;
	struct mtk_vcodec_mem mv_buf;

	struct vdec_mpeg4_vsi *vsi;
	struct vdec_mpeg4_vpu_inst *vpu_inst;
};

static int alloc_mc_buffer(struct vdec_mpeg4_inst *inst, struct vdec_pic_info *pic)
{
	struct mtk_vcodec_mem *mem;
	struct vdec_mpeg4_vsi *vsi = inst->vsi;
	int i;
	int err;

	for (i = 0; i < MPEG4_MAX_MC_NUM; i++) {
		mem = &inst->mc_buf[i];
		mem->size = vsi->mc_buf[i].size;

		err = mtk_vcodec_mem_alloc(inst->ctx, mem);
		if (err) {
			mtk_vcodec_err(inst, "failed to allocate mc buf\n");
			return -ENOMEM;
		}

		vsi->mc_buf[i].dma_addr = (uint64_t) mem->dma_addr;
		memset(mem->va + pic->buf_w * pic->buf_h, 0x80, pic->buf_w * pic->buf_h / 2);

		mtk_vcodec_debug(inst, "Get va=%p dma=%llx size=%zx", mem->va,
				 (uint64_t) mem->dma_addr, mem->size);
	}

	return 0;
}

static int alloc_dcacmv_buffer(struct vdec_mpeg4_inst *inst, struct vdec_pic_info *pic)
{
	struct mtk_vcodec_mem *mem;
	struct vdec_mpeg4_vsi *vsi = inst->vsi;
	int err;

	mem = &inst->dcacmv_buf;
	mem->size = vsi->dcacmv_buf.size;

	err = mtk_vcodec_mem_alloc(inst->ctx, mem);
	if (err) {
		mtk_vcodec_err(inst, "failed to allocate mv buf\n");
		return -ENOMEM;
	}

	vsi->dcacmv_buf.dma_addr = (uint64_t) mem->dma_addr;

	mtk_vcodec_debug(inst, "Get va=%p dma=%llx size=%zx", mem->va, (uint64_t) mem->dma_addr,
			 mem->size);
	return 0;
}

static int alloc_datapar_buffer(struct vdec_mpeg4_inst *inst, struct vdec_pic_info *pic)
{
	struct mtk_vcodec_mem *mem;
	struct vdec_mpeg4_vsi *vsi = inst->vsi;
	int err;

	mem = &inst->datapar_buf;
	mem->size = vsi->datapar_buf.size;

	err = mtk_vcodec_mem_alloc(inst->ctx, mem);
	if (err) {
		mtk_vcodec_err(inst, "failed to allocate mv buf\n");
		return -ENOMEM;
	}

	vsi->datapar_buf.dma_addr = (uint64_t) mem->dma_addr;

	mtk_vcodec_debug(inst, "Get va=%p dma=%llx size=%zx", mem->va, (uint64_t) mem->dma_addr,
			 mem->size);
	return 0;
}

static int alloc_mv_buffer(struct vdec_mpeg4_inst *inst, struct vdec_pic_info *pic)
{
	struct mtk_vcodec_mem *mem;
	struct vdec_mpeg4_vsi *vsi = inst->vsi;
	int err;

	mem = &inst->mv_buf;
	mem->size = vsi->mv_buf.size;

	err = mtk_vcodec_mem_alloc(inst->ctx, mem);
	if (err) {
		mtk_vcodec_err(inst, "failed to allocate mv buf\n");
		return -ENOMEM;
	}

	vsi->mv_buf.dma_addr = (uint64_t) mem->dma_addr;

	mtk_vcodec_debug(inst, "Get va=%p dma=%llx size=%zx", mem->va, (uint64_t) mem->dma_addr,
			 mem->size);
	return 0;
}

static void free_all_working_buf(struct vdec_mpeg4_inst *inst)
{
	int i;

	mtk_vcodec_debug_enter(inst);

	for (i = 0; i < MPEG4_MAX_MC_NUM; i++) {
		if (inst->mc_buf[i].va)
			mtk_vcodec_mem_free(inst->ctx, &inst->mc_buf[i]);
	}

	if (inst->dcacmv_buf.va)
		mtk_vcodec_mem_free(inst->ctx, &inst->dcacmv_buf);

	if (inst->datapar_buf.va)
		mtk_vcodec_mem_free(inst->ctx, &inst->datapar_buf);

	if (inst->mv_buf.va)
		mtk_vcodec_mem_free(inst->ctx, &inst->mv_buf);

}

static int alloc_all_working_buf(struct vdec_mpeg4_inst *inst, struct vdec_pic_info *pic)
{
	struct vdec_mpeg4_vsi *vsi = inst->vsi;
	int err;

	err = alloc_mc_buffer(inst, pic);
	if (err)
		goto end;

	err = alloc_dcacmv_buffer(inst, pic);
	if (err)
		goto end;

	err = alloc_datapar_buffer(inst, pic);
	if (err)
		goto end;

	err = alloc_mv_buffer(inst, pic);
	if (err)
		goto end;

	vsi->wb.dma_addr = vdec_mpeg4_vpu_get_dma(inst->vpu_inst, vsi->wb.vpu_addr);
	mtk_vcodec_debug(inst, "wb dma=%llx", vsi->wb.dma_addr);
end:
	return err;
}

static void get_pic_info(struct vdec_mpeg4_inst *inst, struct vdec_pic_info *pic)
{
	struct vdec_mpeg4_vsi *vsi = inst->vsi;

	pic->pic_w = vsi->pic.pic_w;
	pic->pic_h = vsi->pic.pic_h;
	pic->buf_w = vsi->pic.buf_w;
	pic->buf_h = vsi->pic.buf_h;
	pic->y_bs_sz = vsi->pic.y_bs_sz;
	pic->c_bs_sz = vsi->pic.c_bs_sz;
	pic->y_len_sz = vsi->pic.y_len_sz;
	pic->c_len_sz = vsi->pic.c_len_sz;

	mtk_vcodec_debug(inst, "pic(%d, %d), buf(%d, %d)",
			 pic->pic_w, pic->pic_h, pic->buf_w, pic->buf_h);
	mtk_vcodec_debug(inst, "Y(%d, %d), C(%d, %d)", pic->y_bs_sz,
			 pic->y_len_sz, pic->c_bs_sz, pic->c_len_sz);
}

static void get_crop_info(struct vdec_mpeg4_inst *inst, struct v4l2_crop *cr)
{
	struct vdec_mpeg4_vsi *vsi = inst->vsi;

	cr->c.left = 0;
	cr->c.top = 0;
	cr->c.width = vsi->pic.pic_w;
	cr->c.height = vsi->pic.pic_h;

	mtk_vcodec_debug(inst, "get crop info l=%d, t=%d, w=%d, h=%d\n",
			 cr->c.left, cr->c.top, cr->c.width, cr->c.height);
}

static void get_dpb_size(struct vdec_mpeg4_inst *inst, unsigned int *dpb_sz)
{
	*dpb_sz = 3;

	mtk_vcodec_debug(inst, "sz=%u", *dpb_sz);
}

static void get_disp_fb(struct vdec_mpeg4_inst *inst, struct vdec_fb **out_fb)
{
	struct vdec_mpeg4_vsi *vsi = inst->vsi;
	struct vdec_fb *fb = (struct vdec_fb *)(uintptr_t)vsi->disp_fb.fb_va;

	if (fb != NULL) {
		fb->status |= FB_ST_DISPLAY;
		vsi->disp_fb = vsi->disp_ready_fb;
		vsi->disp_ready_fb.fb_va = 0;

		mtk_vcodec_debug(inst, "get_disp_fb (0x%p -> 0x%p, 0x%p)", fb, fb->base_y.va, fb->base_c.va);
	} else
		mtk_vcodec_debug(inst, "get_disp_fb: No more Display Buffer available");

	*out_fb = fb;
}


static void get_free_fb(struct vdec_mpeg4_inst *inst, struct vdec_fb **out_fb)
{
	struct vdec_mpeg4_vsi *vsi = inst->vsi;
	struct vdec_fb *fb = (struct vdec_fb *)(uintptr_t)vsi->free_fb.fb_va;

	if (fb != NULL) {
		fb->status |= FB_ST_FREE;
		vsi->free_fb = vsi->free_ready_fb;
		vsi->free_ready_fb.fb_va = 0;

		mtk_vcodec_debug(inst, "get_free_fb (0x%p -> 0x%p, 0x%p)", fb, fb->base_y.va, fb->base_c.va);
	} else
		mtk_vcodec_debug(inst, "get_free_fb: No more Free Buffer available");

	*out_fb = fb;
}

static void add_free_fb(struct vdec_mpeg4_inst *inst, struct vdec_fb *fb)
{
	struct vdec_mpeg4_vsi *vsi = inst->vsi;

	if (vsi->free_fb.fb_va == 0)
		vsi->free_fb.fb_va = (uintptr_t)fb;
	else
		mtk_vcodec_debug(inst, "free_fb is not empty");
}

static int vdec_mpeg4_init(struct mtk_vcodec_ctx *ctx,
			   struct mtk_vcodec_mem *bs, unsigned long *h_vdec,
			   struct vdec_pic_info *pic)
{
	struct vdec_mpeg4_bs send_bs = { 0 };
	struct vdec_mpeg4_inst *inst = NULL;
	int err;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->ctx = ctx;
	inst->vpu_inst = vdec_mpeg4_vpu_alloc(ctx);
	if (!inst->vpu_inst) {
		kfree(inst);
		return -ENOMEM;
	}

	if (bs) {
		send_bs.dma_addr = (uint64_t) bs->dma_addr;
#ifdef DEBUG_HW_PARSE
		send_bs.va_addr = (uintptr_t) bs->va;
#endif
		send_bs.size = (uint32_t) bs->size;
	}
#ifdef DEBUG_HW_PARSE
	mtk_vcodec_debug(inst, "bs va=%llx dma=%llx sz=0x%x", send_bs.va_addr,
			 send_bs.dma_addr, send_bs.size);
#else
	mtk_vcodec_debug(inst, "dma=%llx sz=0x%x",
			 send_bs.dma_addr, send_bs.size);
#endif
	err = vdec_mpeg4_vpu_init(inst->vpu_inst, &send_bs);
	if (err < 0) {
		mtk_vcodec_err(inst, "mpeg4_drv_init failed ret=%d", err);
		goto error_free_vpu;
	}

	inst->vsi = (struct vdec_mpeg4_vsi *)vdec_mpeg4_vpu_get_shmem(inst->vpu_inst);

	get_pic_info(inst, pic);

	err = alloc_all_working_buf(inst, pic);
	if (err)
		goto error_free_buf;

	*h_vdec = (unsigned long)inst;

	mtk_vcodec_debug(inst, "Mpeg4 Instance >> %p", inst);
	return 0;

error_free_buf:
	free_all_working_buf(inst);
error_free_vpu:
	vdec_mpeg4_vpu_free(inst->vpu_inst);
	kfree(inst);
	return err;
}

static int vdec_mpeg4_decode(unsigned long h_vdec, struct mtk_vcodec_mem *bs,
			     struct vdec_fb *fb, bool *res_chg)
{
	struct vdec_mpeg4_inst *inst = (struct vdec_mpeg4_inst *)h_vdec;
	struct vdec_mpeg4_bs send_bs;
	struct vdec_mpeg4_fb send_fb = { 0 };
	int err;

	mtk_vcodec_debug_enter(inst);

	if (bs) {
#ifdef DEBUG_HW_PARSE
		send_bs.va_addr = (uintptr_t) bs->va;
#endif
		send_bs.dma_addr = (uint64_t) bs->dma_addr;
		send_bs.size = (uint32_t) bs->size;
#ifdef DEBUG_HW_PARSE
		mtk_vcodec_debug(inst, "bs va=%llx dma=%llx sz=0x%x", (uint64_t) send_bs.va_addr,
				 (uint64_t) send_bs.dma_addr, send_bs.size);
#else
		mtk_vcodec_debug(inst, "dma=%llx sz=0x%x",
				 (uint64_t) send_bs.dma_addr, send_bs.size);

#endif
	} else
		return vdec_mpeg4_vpu_reset(inst->vpu_inst);

	if (fb) {
		send_fb.fb_va = (uintptr_t) fb;
		send_fb.y_dma_addr = (uint64_t) fb->base_y.dma_addr;
		send_fb.c_dma_addr = (uint64_t) fb->base_c.dma_addr;
	}
	mtk_vcodec_debug(inst, "fb fb_va=%llx y_dma=%llx c_dma=%llx", (uint64_t) send_fb.fb_va,
			 (uint64_t) send_fb.y_dma_addr, (uint64_t) send_fb.c_dma_addr);

	err = vdec_mpeg4_vpu_dec_start(inst->vpu_inst, &send_bs, &send_fb);
	if (err < 0) {
		mtk_vcodec_err(inst, "vdec_mpeg4_vpu_dec_start failed ret=%d", err);
		goto err_end;
	}

	if (inst->vsi->dec_mode) {
		mtk_vcodec_wait_for_done_ctx(inst->ctx,
					     MTK_INST_IRQ_RECEIVED, WAIT_INTR_TIMEOUT, true);

		err = vdec_mpeg4_vpu_dec_end(inst->vpu_inst);
		if (err < 0) {
			mtk_vcodec_err(inst, "vdec_mpeg4_vpu_dec_end failed ret=%d", err);
			goto err_end;
		}
	}

	mtk_vcodec_debug_leave(inst);
	return 0;

err_end:
	add_free_fb(inst, fb);
	mtk_vcodec_debug_leave(inst);
	return err;
}

static int vdec_mpeg4_deinit(unsigned long h_vdec)
{
	struct vdec_mpeg4_inst *inst = (struct vdec_mpeg4_inst *)h_vdec;

	mtk_vcodec_debug_enter(inst);

	vdec_mpeg4_vpu_deinit(inst->vpu_inst);
	free_all_working_buf(inst);
	vdec_mpeg4_vpu_free(inst->vpu_inst);
	kfree(inst);

	return 0;
}

static int vdec_mpeg4_get_param(unsigned long h_vdec, enum vdec_get_param_type type, void *out)
{
	struct vdec_mpeg4_inst *inst = (struct vdec_mpeg4_inst *)h_vdec;

	switch (type) {
	case GET_PARAM_DISP_FRAME_BUFFER:
		get_disp_fb(inst, out);
		break;
	case GET_PARAM_FREE_FRAME_BUFFER:
		get_free_fb(inst, out);
		break;
	case GET_PARAM_PIC_INFO:
		get_pic_info(inst, out);
		break;
	case GET_PARAM_DPB_SIZE:
		get_dpb_size(inst, out);
		break;
	case GET_PARAM_CROP_INFO:
		get_crop_info(inst, out);
		break;
	default:
		mtk_vcodec_err(inst, "invalid get parameter type=%d\n", type);
		return -EINVAL;
	}

	return 0;
}

static struct vdec_common_if vdec_mpeg4_if = {
	vdec_mpeg4_init,
	vdec_mpeg4_decode,
	vdec_mpeg4_get_param,
	vdec_mpeg4_deinit,
};

struct vdec_common_if *get_mpeg4_dec_comm_if(void)
{
	return &vdec_mpeg4_if;
}
