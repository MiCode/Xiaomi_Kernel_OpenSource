/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Jungchang Tsao <jungchang.tsao@mediatek.com>
 *	   PC Chen <pc.chen@mediatek.com>
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

#include "mtk_vcodec_intr.h"

#include "vdec_vp8_if.h"
#include "vdec_vp8_vpu.h"

#define VP8_VP_WRAP_SZ				(45 * 4096)
#define VP8_SEGID_DRAM_ADDR			0x3c
#define VP8_HW_DATA_ADDR			0x93C
#define VP8_HW_DATA_VALUE			0x940
#define VP8_BSASET			        0x100
#define VP8_BSDSET				0x104
#define VP8_DEC_DATA_PROCESSING_LOOP		96

static void get_hw_reg_base(struct vdec_vp8_inst *inst)
{
	inst->reg_base.top = mtk_vcodec_get_reg_addr(inst->ctx, VDEC_TOP);
	inst->reg_base.cm = mtk_vcodec_get_reg_addr(inst->ctx, VDEC_CM);
	inst->reg_base.hwd = mtk_vcodec_get_reg_addr(inst->ctx, VDEC_HWD);
}

static void write_hw_data(struct vdec_vp8_inst *inst)
{
	int i, j;
	unsigned int seg_id_addr;
	unsigned int val;
	void __iomem *cm = inst->reg_base.cm;

	seg_id_addr = readl(inst->reg_base.top + VP8_SEGID_DRAM_ADDR) >> 4;

	for (i = 0; i < VP8_HW_DATA_SZ; i++) {
		for (j = 3; j >= 0; j--) {
			val = (1 << 16) + ((seg_id_addr + i) << 2) + j;
			writel(val, cm + VP8_HW_DATA_ADDR);

			val = inst->vsi->segid_wrapper_work[i][j];
			writel(val, cm + VP8_HW_DATA_VALUE);
		}
	}
}

static void read_hw_data(struct vdec_vp8_inst *inst)
{
	int i, j;
	unsigned int seg_id_addr;
	unsigned int val;
	void __iomem *cm = inst->reg_base.cm;

	seg_id_addr = readl(inst->reg_base.top + VP8_SEGID_DRAM_ADDR) >> 4;

	for (i = 0; i < VP8_HW_DATA_SZ; i++) {
		for (j = 3; j >= 0; j--) {
			val = ((seg_id_addr + i) << 2) + j;
			writel(val, cm + VP8_HW_DATA_ADDR);

			val = readl(cm + VP8_HW_DATA_VALUE);
			inst->vsi->segid_wrapper_work[i][j] = val;
		}
	}
}

static void flush_dec_data(struct vdec_vp8_inst *inst)
{
	int i, j, k;
	unsigned int addr = 0, val = 0;
	void __iomem *hwd = inst->reg_base.hwd;
	unsigned char *p = (unsigned char *)&inst->vsi->dec_data[8];

	for (i = 0; i < VP8_DEC_DATA_PROCESSING_LOOP; i++) {
		writel(addr, hwd + VP8_BSASET);
		for (j = 0; j < 3 ; j++) {
			val = 0;
			for (k = 0; k < 4; k++)
				val += *p++ << (k << 3);
			writel(val, hwd + VP8_BSDSET);
		}
		writel(val, hwd + VP8_BSDSET);
		addr += 4;
	}
}

static void load_dec_data(struct vdec_vp8_inst *inst)
{
	int i;
	unsigned int addr = 0;
	unsigned int *p = (unsigned int *)&inst->vsi->dec_data[8];
	void __iomem *hwd = inst->reg_base.hwd;

	for (i = 0; i < VP8_DEC_DATA_PROCESSING_LOOP; i++) {
		writel(addr, hwd + VP8_BSASET);
		*p++ = readl(hwd + VP8_BSDSET);
		*p++ = readl(hwd + VP8_BSDSET);
		*p++ = readl(hwd + VP8_BSDSET) & 0xFFFFFF;
		addr += 4;
	}
}

static void get_pic_info(struct vdec_vp8_inst *inst, struct vdec_pic_info *pic)
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

static int vp8_dec_finish(struct vdec_vp8_inst *inst)
{
	struct vdec_fb_node *node;
	uint64_t prev_y_dma = inst->vsi->dec.prev_y_dma;

	mtk_vcodec_debug(inst, "prev fb base dma=%llx", prev_y_dma);

	/* put last decode ok frame to dec_free_list */
	if (0 != prev_y_dma) {
		list_for_each_entry(node, &inst->dec_use_list, list) {
			struct vdec_fb *fb = (struct vdec_fb *)node->fb;

			if (prev_y_dma == (unsigned long)fb->base_y.dma_addr) {
				list_move_tail(&node->list,
					       &inst->dec_free_list);
				break;
			}
		}
	}

	/* dec_fb_list -> dec_use_list */
	node = list_first_entry(&inst->dec_fb_list, struct vdec_fb_node, list);
	node->fb = inst->cur_fb;
	list_move_tail(&node->list, &inst->dec_use_list);

	/* disp_fb_list -> disp_rdy_list */
	if (inst->vsi->dec.show_frame) {
		node = list_first_entry(&inst->disp_fb_list,
					struct vdec_fb_node, list);
		node->fb = inst->cur_fb;
		list_move_tail(&node->list, &inst->disp_rdy_list);
	}

	return 0;
}

static void move_fb_list_use_to_free(struct vdec_vp8_inst *inst)
{
	struct vdec_fb_node *node, *tmp;

	list_for_each_entry_safe(node, tmp, &inst->dec_use_list, list)
		list_move_tail(&node->list, &inst->dec_free_list);
}

static void init_list(struct vdec_vp8_inst *inst)
{
	int i;

	INIT_LIST_HEAD(&inst->dec_fb_list);
	INIT_LIST_HEAD(&inst->dec_use_list);
	INIT_LIST_HEAD(&inst->dec_free_list);
	INIT_LIST_HEAD(&inst->disp_fb_list);
	INIT_LIST_HEAD(&inst->disp_rdy_list);

	for (i = 0; i < VP8_MAX_FRM_BUFF_NUM; i++) {
		INIT_LIST_HEAD(&inst->dec_fb[i].list);
		inst->dec_fb[i].fb = NULL;
		list_add_tail(&inst->dec_fb[i].list, &inst->dec_fb_list);

		INIT_LIST_HEAD(&inst->disp_fb[i].list);
		inst->disp_fb[i].fb = NULL;
		list_add_tail(&inst->disp_fb[i].list, &inst->disp_fb_list);
	}
}

static void add_fb_to_free_list(struct vdec_vp8_inst *inst, void *fb)
{
	struct vdec_fb_node *node;

	node = list_first_entry(&inst->dec_fb_list, struct vdec_fb_node, list);
	node->fb = fb;
	list_move_tail(&node->list, &inst->dec_free_list);
}

static int alloc_all_working_buf(struct vdec_vp8_inst *inst)
{
	int err;
	struct mtk_vcodec_mem *mem = &inst->vp_wrapper_buf;

	mem->size = VP8_VP_WRAP_SZ;
	err = mtk_vcodec_mem_alloc(inst->ctx, mem);
	if (err) {
		mtk_vcodec_err(inst, "Cannot allocate vp wrapper buffer");
		return -ENOMEM;
	}

	inst->vsi->dec.vp_wrapper_dma = (u64)mem->dma_addr;
	return 0;
}

static void free_all_working_buf(struct vdec_vp8_inst *inst)
{
	struct mtk_vcodec_mem *mem = &inst->vp_wrapper_buf;

	if (mem->va)
		mtk_vcodec_mem_free(inst->ctx, mem);

	inst->vsi->dec.vp_wrapper_dma = 0;
}

static int vdec_vp8_init(struct mtk_vcodec_ctx *ctx, struct mtk_vcodec_mem *bs,
			 unsigned long *h_vdec, struct vdec_pic_info *pic)
{
	struct vdec_vp8_inst *inst;
	unsigned int data;
	unsigned char *bs_va = (unsigned char *)bs->va;
	int err;

	inst = kzalloc(sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	*h_vdec = (unsigned long)inst;

	inst->ctx = ctx;
	inst->dev = mtk_vcodec_get_plat_dev(ctx);
	mtk_vcodec_debug(inst, "bs va=%p dma=%llx sz=0x%zx", bs->va,
			 (u64)bs->dma_addr, bs->size);

	data = (*(bs_va + 9) << 24) | (*(bs_va + 8) << 16) |
	       (*(bs_va + 7) << 8) | *(bs_va + 6);

	err = vdec_vp8_vpu_init(inst, data);
	if (err) {
		mtk_vcodec_err(inst, "vdec_vp8 init err=%d", err);
		goto error_free_inst;
	}

	get_pic_info(inst, pic);
	init_list(inst);
	err = alloc_all_working_buf(inst);
	if (err)
		goto error_free_buf;

	get_hw_reg_base(inst);
	mtk_vcodec_debug(inst, "VP8 Instance >> %p", inst);
	return 0;

error_free_buf:
	free_all_working_buf(inst);
error_free_inst:
	kfree(inst);
	return err;
}

static int vdec_vp8_decode(unsigned long h_vdec, struct mtk_vcodec_mem *bs,
			   struct vdec_fb *fb, bool *res_chg)
{
	struct vdec_vp8_inst *inst = (struct vdec_vp8_inst *)h_vdec;
	struct vdec_vp8_dec_info *dec = &inst->vsi->dec;
	int err = 0;
	uint64_t y_fb_dma;
	uint64_t c_fb_dma;

	y_fb_dma = fb ? (u64)fb->base_y.dma_addr : 0;
	c_fb_dma = fb ? (u64)fb->base_c.dma_addr : 0;

	mtk_vcodec_debug(inst, "+ [%d] FB y_dma=%llx c_dma=%llx fb=%p",
			 inst->frm_cnt, y_fb_dma, c_fb_dma, fb);

	inst->cur_fb = fb;

	/* bs NULL means flush decoder */
	if (bs == NULL) {
		move_fb_list_use_to_free(inst);
		return vdec_vp8_vpu_reset(inst);
	}

	dec->bs_dma = (unsigned long)bs->dma_addr;
	dec->bs_sz = bs->size;
	dec->cur_y_fb_dma = y_fb_dma;
	dec->cur_c_fb_dma = c_fb_dma;

	mtk_vcodec_debug(inst, "\n + FRAME[%d] +\n", inst->frm_cnt);

	write_hw_data(inst);
	flush_dec_data(inst);
	err = vdec_vp8_vpu_dec_start(inst);
	if (err)
		goto error_free_fb;

	if (dec->resolution_changed) {
		mtk_vcodec_debug(inst, "- resolution_changed -");
		*res_chg = true;
		add_fb_to_free_list(inst, fb);
		return 0;
	}

	/* wait decoder done interrupt */
	mtk_vcodec_wait_for_done_ctx(inst->ctx, MTK_INST_IRQ_RECEIVED,
				     WAIT_INTR_TIMEOUT, true);

	if (inst->vsi->load_data)
		load_dec_data(inst);
	vp8_dec_finish(inst);
	read_hw_data(inst);

	err = vdec_vp8_vpu_dec_end(inst);
	if (err)
		goto error;

	mtk_vcodec_debug(inst, "\n - FRAME[%d] - show=%d\n", inst->frm_cnt,
			 dec->show_frame);
	inst->frm_cnt++;
	*res_chg = false;
	return 0;

error_free_fb:
	add_fb_to_free_list(inst, fb);
error:
	mtk_vcodec_err(inst, "\n - FRAME[%d] - err=%d\n", inst->frm_cnt, err);
	return err;
}

static void get_disp_fb(struct vdec_vp8_inst *inst, struct vdec_fb **out_fb)
{
	struct vdec_fb_node *node;
	struct vdec_fb *fb;

	node = list_first_entry_or_null(&inst->disp_rdy_list,
					struct vdec_fb_node, list);
	if (node) {
		list_move_tail(&node->list, &inst->disp_fb_list);
		fb = (struct vdec_fb *)node->fb;
		fb->status |= FB_ST_DISPLAY;
		mtk_vcodec_debug(inst, "[FB] get disp fb %p st=%d",
				 node->fb, fb->status);
	} else {
		fb = NULL;
		mtk_vcodec_debug(inst, "[FB] there is no disp fb");
	}

	*out_fb = fb;
}

static void get_free_fb(struct vdec_vp8_inst *inst, struct vdec_fb **out_fb)
{
	struct vdec_fb_node *node;
	struct vdec_fb *fb;

	node = list_first_entry_or_null(&inst->dec_free_list,
					struct vdec_fb_node, list);
	if (node) {
		list_move_tail(&node->list, &inst->dec_fb_list);
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

static void get_crop_info(struct vdec_vp8_inst *inst, struct v4l2_crop *cr)
{
	cr->c.left = 0;
	cr->c.top = 0;
	cr->c.width = inst->vsi->pic.pic_w;
	cr->c.height = inst->vsi->pic.pic_h;
	mtk_vcodec_debug(inst, "get crop info l=%d, t=%d, w=%d, h=%d\n",
			 cr->c.left, cr->c.top, cr->c.width, cr->c.height);
}

static int vdec_vp8_get_param(unsigned long h_vdec,
			      enum vdec_get_param_type type, void *out)
{
	struct vdec_vp8_inst *inst = (struct vdec_vp8_inst *)h_vdec;

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

	case GET_PARAM_CROP_INFO:
		get_crop_info(inst, out);
		break;

	case GET_PARAM_DPB_SIZE:
		*((unsigned int *)out) = 4;
		break;

	default:
		mtk_vcodec_err(inst, "invalid get parameter type=%d", type);
		return -EINVAL;
	}

	return 0;
}

static int vdec_vp8_deinit(unsigned long h_vdec)
{
	struct vdec_vp8_inst *inst = (struct vdec_vp8_inst *)h_vdec;

	mtk_vcodec_debug_enter(inst);

	vdec_vp8_vpu_deinit(inst);
	free_all_working_buf(inst);
	kfree(inst);

	return 0;
}

static struct vdec_common_if vdec_vp8_if = {
	vdec_vp8_init,
	vdec_vp8_decode,
	vdec_vp8_get_param,
	vdec_vp8_deinit,
};

struct vdec_common_if *get_vp8_dec_comm_if(void)
{
	return &vdec_vp8_if;
}
