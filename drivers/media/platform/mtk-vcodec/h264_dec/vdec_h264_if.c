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

#include <linux/module.h>
#include <linux/slab.h>

#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"

#include "vdec_h264_if.h"
#include "vdec_h264_vpu.h"

#define NON_IDR_SLICE				0x01
#define IDR_SLICE				0x05
#define H264_PPS				0x08

#define BUF_HDR_PARSING_SZ			1024
#define BUF_PREDICTION_SZ			(32 * 1024)
#define BUF_PP_SZ				(30 * 4096)
#define BUF_LD_SZ				(15 * 4096)

#define MB_UNIT_SZ				16
#define HW_MB_STORE_SZ				64


static unsigned int get_mv_buf_size(unsigned int width, unsigned int height)
{
	return HW_MB_STORE_SZ * (width/MB_UNIT_SZ) * (height/MB_UNIT_SZ);
}

static int alloc_mv_buf(struct vdec_h264_inst *inst, struct vdec_pic_info *pic,
			bool free)
{
	int i;
	int err;
	struct mtk_vcodec_mem *mem = NULL;
	unsigned int buf_sz;

	buf_sz = get_mv_buf_size(pic->buf_w, pic->buf_h);

	for (i = 0; i < H264_MAX_FB_NUM; i++) {
		mem = &inst->mv_buf[i];
		if (free)
			mtk_vcodec_mem_free(inst->ctx, mem);
		mem->size = buf_sz;
		err = mtk_vcodec_mem_alloc(inst->ctx, mem);
		if (err) {
			mtk_vcodec_err(inst, "failed to allocate mv buf\n");
			return -ENOMEM;
		}
		inst->vsi->mv_buf_dma[i] = mem->dma_addr;
	}

	return 0;
}

static void free_all_working_buf(struct vdec_h264_inst *inst)
{
	int i;
	struct mtk_vcodec_mem *mem = NULL;

	mtk_vcodec_debug_enter(inst);

	inst->vsi->ppl_buf_dma = 0;
	mem = &inst->ppl_buf;
	if (mem->va)
		mtk_vcodec_mem_free(inst->ctx, mem);

	for (i = 0; i < H264_MAX_FB_NUM; i++) {
		inst->vsi->mv_buf_dma[i] = 0;
		mem = &inst->mv_buf[i];
		if (mem->va)
			mtk_vcodec_mem_free(inst->ctx, mem);
	}
}

static int allocate_all_working_buf(struct vdec_h264_inst *inst,
				    struct vdec_pic_info *pic)
{
	int err = 0;

	inst->ppl_buf.size = BUF_PREDICTION_SZ + BUF_PP_SZ + BUF_LD_SZ;
	err = mtk_vcodec_mem_alloc(inst->ctx, &inst->ppl_buf);
	if (err) {
		mtk_vcodec_err(inst, "failed to allocate ppl buf\n");
		return -ENOMEM;
	}

	inst->vsi->ppl_buf_dma = inst->ppl_buf.dma_addr;
	return alloc_mv_buf(inst, pic, false);
}

static void put_fb_to_free(struct vdec_h264_inst *inst, struct vdec_fb *fb)
{
	struct h264_ring_fb_list *list;

	list = &inst->vsi->list_free;
	if (list->count == H264_MAX_FB_NUM) {
		mtk_vcodec_err(inst, "[FB] put fb free_list full\n");
		return;
	}

	mtk_vcodec_debug(inst, "[FB] put fb into free_list @(%p, %llx)\n",
			 fb->base_y.va, (u64)fb->base_y.dma_addr);

	list->fb_list[list->write_idx].vdec_fb_va = (uintptr_t)fb;
	list->write_idx = (list->write_idx == H264_MAX_FB_NUM - 1) ?
			  0 : list->write_idx + 1;
	list->count++;
}

static void get_pic_info(struct vdec_h264_inst *inst,
			 struct vdec_pic_info *pic)
{
	pic->pic_w = inst->vsi->pic.pic_w;
	pic->pic_h = inst->vsi->pic.pic_h;
	pic->buf_w = inst->vsi->pic.buf_w;
	pic->buf_h = inst->vsi->pic.buf_h;
	pic->y_bs_sz = inst->vsi->pic.y_bs_sz;
	pic->c_bs_sz = inst->vsi->pic.c_bs_sz;
	pic->y_len_sz = inst->vsi->pic.y_len_sz;
	pic->c_len_sz = inst->vsi->pic.c_len_sz;

	mtk_vcodec_debug(inst, "pic(%d, %d), buf(%d, %d)",
			 pic->pic_w, pic->pic_h, pic->buf_w, pic->buf_h);
	mtk_vcodec_debug(inst, "Y(%d, %d), C(%d, %d)", pic->y_bs_sz,
			 pic->y_len_sz, pic->c_bs_sz, pic->c_len_sz);
}

static void get_crop_info(struct vdec_h264_inst *inst, struct v4l2_crop *cr)
{
	cr->c.left	= inst->vsi->crop.left;
	cr->c.top	= inst->vsi->crop.top;
	cr->c.width	= inst->vsi->crop.width;
	cr->c.height	= inst->vsi->crop.height;
	mtk_vcodec_debug(inst, "l=%d, t=%d, w=%d, h=%d",
			 cr->c.left, cr->c.top, cr->c.width, cr->c.height);
}

static void get_dpb_size(struct vdec_h264_inst *inst, unsigned int *dpb_sz)
{
	*dpb_sz = inst->vsi->dec.dpb_sz;
	mtk_vcodec_debug(inst, "sz=%d", *dpb_sz);
}

static int vdec_h264_init(struct mtk_vcodec_ctx *ctx,
			  struct mtk_vcodec_mem *bs, unsigned long *h_vdec,
			  struct vdec_pic_info *pic)
{
	struct vdec_h264_inst *inst = NULL;
	int err;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	inst->ctx = ctx;
	inst->dev = mtk_vcodec_get_plat_dev(ctx);
	inst->ctx_id = mtk_vcodec_get_ctx_id(ctx);

	mtk_vcodec_debug(inst, "bs va=%p dma=%llx sz=0x%zx", bs->va,
			 (u64)bs->dma_addr, bs->size);

	err = vdec_h264_vpu_init(inst, (u64)bs->dma_addr, bs->size);
	if (err) {
		mtk_vcodec_err(inst, "vdec_h264 init err=%d\n", err);
		goto error_free_inst;
	}

	get_pic_info(inst, pic);
	if (err)
		goto error_free_inst;

	err = allocate_all_working_buf(inst, pic);
	if (err)
		goto error_free_buf;

	mtk_vcodec_debug(inst, "H264 Instance >> %p", inst);

	*h_vdec = (unsigned long)inst;
	return 0;

error_free_buf:
	free_all_working_buf(inst);
error_free_inst:
	kfree(inst);

	return err;
}

static int vdec_h264_deinit(unsigned long h_vdec)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;

	mtk_vcodec_debug_enter(inst);

	vdec_h264_vpu_deinit(inst);
	free_all_working_buf(inst);

	kfree(inst);
	return 0;
}

static int find_start_code(unsigned char *data, unsigned int data_sz)
{
	if (data_sz >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1)
		return 3;

	if (data_sz >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 &&
	    data[3] == 1)
		return 4;

	return -1;
}

static int vdec_h264_decode(unsigned long h_vdec, struct mtk_vcodec_mem *bs,
			    struct vdec_fb *fb, bool *res_chg)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;
	int idx = 0;
	int err = 0;
	unsigned int nal_start;
	unsigned int nal_type;
	unsigned char *buf;
	unsigned int buf_sz;
	uint64_t vdec_fb_va;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;

	y_fb_dma = fb ? (u64)fb->base_y.dma_addr : 0;
	c_fb_dma = fb ? (u64)fb->base_c.dma_addr : 0;

	vdec_fb_va = (uintptr_t)fb;

	mtk_vcodec_debug(inst, "+ [%d] FB y_dma=%llx c_dma=%llx va=%p\n",
			 inst->num_nalu, y_fb_dma, c_fb_dma, fb);

	/* bs NULL means flush decoder */
	if (bs == NULL)
		return vdec_h264_vpu_reset(inst);

	buf = (unsigned char *)bs->va;
	buf_sz = bs->size;
	idx = find_start_code(buf, buf_sz);
	if (idx < 0)
		goto err_free_fb_out;

	nal_start = buf[idx];
	nal_type = NAL_TYPE(buf[idx]);
	mtk_vcodec_debug(inst, "\n + NALU[%d] type %d +\n", inst->num_nalu,
			 nal_type);

	if (nal_type == H264_PPS) {
		if (buf_sz > BUF_HDR_PARSING_SZ) {
			err = -EILSEQ;
			goto err_free_fb_out;
		}
		buf_sz -= idx;
		memcpy(inst->vpu.hdr_bs_buf, buf+idx, buf_sz);
	}

	err = vdec_h264_vpu_dec_start(inst, buf_sz, nal_start,
				      (u64)bs->dma_addr, y_fb_dma, c_fb_dma,
				      vdec_fb_va);
	if (err) {
		mtk_vcodec_err(inst, "dec_start err = %d\n", err);
		goto err_free_fb_out;
	}

	*res_chg = inst->vsi->dec.resolution_changed;
	if (*res_chg) {
		struct vdec_pic_info pic;

		mtk_vcodec_debug(inst, "- resolution changed -");
		get_pic_info(inst, &pic);

		if (inst->vsi->dec.realloc_mv_buf) {
			err = alloc_mv_buf(inst, &pic, true);
			if (err)
				goto err_free_fb_out;
		}
	}

	if (nal_type == NON_IDR_SLICE || nal_type == IDR_SLICE) {
		/* wait decoder done interrupt */
		err = mtk_vcodec_wait_for_done_ctx(inst->ctx,
						   MTK_INST_IRQ_RECEIVED,
						   WAIT_INTR_TIMEOUT, true);
		if (err)
			goto err_free_fb_out;

		vdec_h264_vpu_dec_end(inst);
	}

	mtk_vcodec_debug(inst, "\n - NALU[%d] type=%d -\n", inst->num_nalu,
			 nal_type);
	inst->num_nalu++;
	return 0;

err_free_fb_out:
	put_fb_to_free(inst, fb);
	mtk_vcodec_err(inst, "\n - NALU[%d] err=%d -\n", inst->num_nalu, err);
	return err;
}

static void vdec_h264_get_fb(struct vdec_h264_inst *inst,
			     struct h264_ring_fb_list *list,
			     bool disp_list, struct vdec_fb **out_fb)
{
	unsigned long vdec_fb_va;
	struct vdec_fb *fb;

	if (list->count == 0) {
		mtk_vcodec_debug(inst, "[FB] there is no %s fb",
				disp_list ? "disp" : "free");
		*out_fb = NULL;
		return;
	}

	vdec_fb_va = (unsigned long)list->fb_list[list->read_idx].vdec_fb_va;
	fb = (struct vdec_fb *)vdec_fb_va;
	if (disp_list)
		fb->status |= FB_ST_DISPLAY;
	else
		fb->status |= FB_ST_FREE;

	*out_fb = fb;
	mtk_vcodec_debug(inst, "[FB] get %s fb st=%d poc=%d %llx",
			disp_list ? "disp" : "free",
			 fb->status, list->fb_list[list->read_idx].poc,
			 list->fb_list[list->read_idx].vdec_fb_va);

	list->read_idx = (list->read_idx == H264_MAX_FB_NUM - 1) ?
			 0 : list->read_idx + 1;
	list->count--;
}

static int vdec_h264_get_param(unsigned long h_vdec,
			       enum vdec_get_param_type type, void *out)
{
	struct vdec_h264_inst *inst = (struct vdec_h264_inst *)h_vdec;

	switch (type) {
	case GET_PARAM_DISP_FRAME_BUFFER:
		vdec_h264_get_fb(inst, &inst->vsi->list_disp, true, out);
		break;

	case GET_PARAM_FREE_FRAME_BUFFER:
		vdec_h264_get_fb(inst, &inst->vsi->list_free, false, out);
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

static struct vdec_common_if vdec_h264_if = {
	vdec_h264_init,
	vdec_h264_decode,
	vdec_h264_get_param,
	vdec_h264_deinit,
};

struct vdec_common_if *get_h264_dec_comm_if(void)
{
	return &vdec_h264_if;
}
