/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Daniel Hsiao <daniel.hsiao@mediatek.com>
 *             Kai-Sean Yang <kai-sean.yang@mediatek.com>
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

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/delay.h>
#include <linux/time.h>

#include "vdec_vp9_if.h"
#include "vdec_vp9_core.h"
#include "vdec_vp9_debug.h"

static void init_list(struct vdec_vp9_inst *inst)
{
	int i;

	INIT_LIST_HEAD(&inst->available_fb_node_list);
	INIT_LIST_HEAD(&inst->fb_use_list);
	INIT_LIST_HEAD(&inst->fb_free_list);
	INIT_LIST_HEAD(&inst->fb_disp_list);

	for (i = 0; i < VP9_MAX_FRM_BUFF_NODE_NUM; i++) {
		INIT_LIST_HEAD(&inst->dec_fb[i].list);
		inst->dec_fb[i].fb = NULL;
		list_add_tail(&inst->dec_fb[i].list,
			      &inst->available_fb_node_list);
	}
}


static void get_pic_info(struct vdec_vp9_inst *inst, struct vdec_pic_info *pic)
{

	pic->y_bs_sz = inst->work_buf.frmbuf_width *
		inst->work_buf.frmbuf_height;
	pic->c_bs_sz = pic->y_bs_sz >> 1;
	pic->y_len_sz = 0;
	pic->c_len_sz = 0;

	pic->pic_w = inst->frm_hdr.width;
	pic->pic_h = inst->frm_hdr.height;
	pic->buf_w = inst->work_buf.frmbuf_width;
	pic->buf_h = inst->work_buf.frmbuf_height;
}

static void get_disp_fb(struct vdec_vp9_inst *inst, struct vdec_fb **out_fb)
{
	mtk_vcodec_debug_enter(inst);

	*out_fb = vp9_rm_from_fb_disp_list(inst);
	if (*out_fb)
		(*out_fb)->status |= FB_ST_DISPLAY;
}

static void get_free_fb(struct vdec_vp9_inst *inst, struct vdec_fb **out_fb)
{
	struct vdec_fb_node *node;
	struct vdec_fb *fb = NULL;

	node = list_first_entry_or_null(&inst->fb_free_list,
					struct vdec_fb_node, list);
	if (node) {
		list_move_tail(&node->list, &inst->available_fb_node_list);
		fb = (struct vdec_fb *)node->fb;
		fb->status |= FB_ST_FREE;
		mtk_vcodec_debug(inst, "[FB] get free fb %p st=%d",
				 node->fb, fb->status);
	} else {
		fb = NULL;
		mtk_vcodec_debug(inst, "[FB] there is no free fb");
	}

	*out_fb = fb;

}

static int vdec_vp9_init(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
		 unsigned long *h_vdec, struct vdec_pic_info *pic_info)
{
	struct vdec_vp9_inst *inst;
	unsigned int ipi_data[3];

	if ((bs == NULL) || (pic_info == NULL))
		return -EINVAL;

	inst = vp9_alloc_inst(ctx);
	if (!inst)
		return -ENOMEM;

	inst->frm_cnt = 0;
	inst->total_frm_cnt = 0;
	inst->ctx = ctx;
	inst->dev = mtk_vcodec_get_plat_dev(ctx);
	(*h_vdec) = (unsigned long)inst;

	mtk_vcodec_debug(inst, "Input BS Size = %zd\n", bs->size);

	ipi_data[0] = *((unsigned int *)bs->va);
	ipi_data[1] = *((unsigned int *)(bs->va + 4));
	ipi_data[2] = *((unsigned int *)(bs->va + 8));
	if (0 != vp9_dec_vpu_init(inst, (unsigned int *)ipi_data, 3)) {
		mtk_vcodec_err(inst, "[E]vp9_dec_vpu_init -");
		return -EINVAL;
	}

	if (vp9_get_hw_reg_base(inst) != true) {
		mtk_vcodec_err(inst, "vp9_get_hw_reg_base");
		return -EINVAL;
	}

	if (vp9_init_proc(inst, pic_info) != true) {
		mtk_vcodec_err(inst, "vp9_init_proc");
		return -EINVAL;
	}

	get_pic_info(inst, pic_info);
	init_list(inst);

	if (vp9_alloc_work_buf(inst) != true) {
		mtk_vcodec_err(inst, "vp9_alloc_work_buf");
		return -EINVAL;
	}

	mtk_vcodec_debug_leave(inst);

	return 0;
}

static int vdec_vp9_decode(unsigned long h_vdec, struct mtk_vcodec_mem *bs,
		   struct vdec_fb *fb, bool *res_chg)
{
	int ret = 0;
	struct vdec_vp9_inst *inst = (struct vdec_vp9_inst *)h_vdec;
	struct vdec_vp9_frm_hdr *frm_hdr = &inst->frm_hdr;

	mtk_vcodec_debug_enter(inst);

	*res_chg = false;

	if ((bs == NULL) && (fb == NULL)) {
		mtk_vcodec_debug(inst, "[EOS]");
		vp9_reset(inst);
		return ret;
	}

	if (bs != NULL)
		mtk_vcodec_debug(inst, "Input BS Size = %zd", bs->size);

	memcpy((void *)inst + sizeof(*inst), (void *)inst,
	       sizeof(*inst));

	while (1) {
		frm_hdr->resolution_changed = false;

		if (vp9_dec_proc(inst, bs, fb) != true) {
			mtk_vcodec_err(inst, "vp9_dec_proc");
			ret = -EINVAL;
			goto DECODE_ERROR;
		}

		if (frm_hdr->resolution_changed) {
			unsigned int width = inst->frm_hdr.width;
			unsigned int height = inst->frm_hdr.height;
			unsigned int frmbuf_width =
				inst->work_buf.frmbuf_width;
			unsigned int frmbuf_height =
				inst->work_buf.frmbuf_height;
			struct mtk_vcodec_mem tmp_buf =	inst->work_buf.mv_buf;
			struct vp9_dram_buf tmp_buf2 = inst->vpu.drv->mv_buf;
			struct vdec_fb tmp_buf3 =
				inst->work_buf.sf_ref_buf[0];

			memcpy((void *)inst, (void *)inst + sizeof(*inst),
			       sizeof(*inst));

			inst->frm_hdr.width = width;
			inst->frm_hdr.height = height;
			inst->work_buf.frmbuf_width = frmbuf_width;
			inst->work_buf.frmbuf_height = frmbuf_height;
			inst->work_buf.mv_buf = tmp_buf;
			inst->vpu.drv->mv_buf = tmp_buf2;
			inst->work_buf.sf_ref_buf[0] = tmp_buf3;

			*res_chg = true;
			mtk_vcodec_debug(inst, "VDEC_ST_RESOLUTION_CHANGED");
			vp9_add_to_fb_free_list(inst, fb);
			ret = 0;
			goto DECODE_ERROR;
		}

		if (vp9_check_proc(inst) != true) {
			mtk_vcodec_err(inst, "vp9_check_proc");
			ret = -EINVAL;
			goto DECODE_ERROR;
		}

		inst->total_frm_cnt++;

		if (vp9_is_last_sub_frm(inst))
			break;

		/* for resolution change backup */
		memcpy((void *)inst + sizeof(*inst), (void *)inst,
		       sizeof(*inst));
	}
	inst->frm_cnt++;

DECODE_ERROR:
	if (ret < 0)
		vp9_add_to_fb_free_list(inst, fb);

	mtk_vcodec_debug_leave(inst);

	return ret;
}

static void get_crop_info(struct vdec_vp9_inst *inst, struct v4l2_crop *cr)
{
	cr->c.left = 0;
	cr->c.top = 0;
	cr->c.width = inst->frm_hdr.width;
	cr->c.height = inst->frm_hdr.height;
	mtk_vcodec_debug(inst, "get crop info l=%d, t=%d, w=%d, h=%d\n",
			 cr->c.left, cr->c.top, cr->c.width, cr->c.height);
}

static int vdec_vp9_get_param(unsigned long h_vdec,
			enum vdec_get_param_type type, void *out)
{
	struct vdec_vp9_inst *inst = (struct vdec_vp9_inst *)h_vdec;
	int ret = 0;

	mtk_vcodec_debug_enter(inst);

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
		*((unsigned int *)out) = 9;
		break;
	case GET_PARAM_CROP_INFO:
		get_crop_info(inst, out);
		break;
	default:
		mtk_vcodec_err(inst, "not support type %d", type);
		ret = -EINVAL;
		break;
	}

	mtk_vcodec_debug_leave(inst);

	return ret;
}

static int vdec_vp9_deinit(unsigned long h_vdec)
{
	int ret = 0;
	struct vdec_vp9_inst *inst = (struct vdec_vp9_inst *)h_vdec;

	mtk_vcodec_debug_enter(inst);

	if (0 != vp9_dec_vpu_deinit(inst)) {
		mtk_vcodec_err(inst, "[E]vp9_dec_vpu_deinit");
		ret = -EINVAL;
	}

	if (vp9_free_work_buf(inst) != true) {
		mtk_vcodec_err(inst, "vp9_free_work_buf");
		ret = -EINVAL;
	}

	mtk_vcodec_debug_leave(inst);
	vp9_free_handle(inst);

	return ret;
}

static struct vdec_common_if vdec_vp9_if = {
	vdec_vp9_init,
	vdec_vp9_decode,
	vdec_vp9_get_param,
	vdec_vp9_deinit,
};

struct vdec_common_if *get_vp9_dec_comm_if(void)
{
	return &vdec_vp9_if;
}
