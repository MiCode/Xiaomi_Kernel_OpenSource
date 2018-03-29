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
#include <linux/file.h>

#include "mtk_vcodec_drv.h"
#include "mtk_vpu.h"
#include "vdec_vp9_if.h"
#include "vdec_vp9_core.h"
#include "vdec_vp9_debug.h"

static void vp9_setup_buf(struct vdec_vp9_inst *inst)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;

	drv->seg_id_buf.va = (unsigned long)vpu_mapping_dm_addr(inst->dev,
		drv->seg_id_buf.vpua);
	drv->seg_id_buf.pa = vpu_mapping_iommu_dm_addr(
		inst->dev, drv->seg_id_buf.vpua);

	drv->tile_buf.va = (unsigned long)vpu_mapping_dm_addr(inst->dev,
		drv->tile_buf.vpua);
	drv->tile_buf.pa = vpu_mapping_iommu_dm_addr(
		inst->dev, drv->tile_buf.vpua);

	drv->count_tbl_buf.va = (unsigned long)vpu_mapping_dm_addr(inst->dev,
		drv->count_tbl_buf.vpua);
	drv->count_tbl_buf.pa = vpu_mapping_iommu_dm_addr(
		inst->dev, drv->count_tbl_buf.vpua);

	drv->mv_buf.va = (unsigned long)inst->work_buf.mv_buf.va;
	drv->mv_buf.pa = (unsigned long)inst->work_buf.mv_buf.dma_addr;
	drv->mv_buf.sz = (unsigned long)inst->work_buf.mv_buf.size;

	mtk_vcodec_debug(inst, "VP9_SEG_ID_Addr: 0x%x -> 0x%lX (0x%lX)",
		     drv->seg_id_buf.vpua, drv->seg_id_buf.va,
		     drv->seg_id_buf.pa);
	mtk_vcodec_debug(inst, "VP9_TILE_Addr: 0x%x -> 0x%lX (0x%lX)",
		     drv->tile_buf.vpua, drv->tile_buf.va,
		     drv->tile_buf.pa);
	mtk_vcodec_debug(inst, "VP9_COUNT_TBL_Addr: 0x%x -> 0x%lX (0x%lX)",
		     drv->count_tbl_buf.vpua, drv->count_tbl_buf.va,
		     drv->count_tbl_buf.pa);
	mtk_vcodec_debug(inst, "VP9_MV_BUF_Addr: 0x%lX (0x%lX)",
		     drv->mv_buf.va, drv->mv_buf.pa);
}

static void vp9_ref_cnt_fb(struct vdec_vp9_inst *inst, int *idx,
			   int new_idx)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;
	int ref_idx = *idx;

	if (ref_idx >= 0 && drv->frm_bufs[ref_idx].ref_cnt > 0) {
		drv->frm_bufs[ref_idx].ref_cnt--;

		if (drv->frm_bufs[ref_idx].ref_cnt == 0) {
			if (!vp9_is_sf_ref_fb(inst,
					      drv->frm_bufs[ref_idx].buf.fb)) {
				struct vdec_fb *fb;

				fb = vp9_rm_from_fb_use_list(inst,
				     drv->frm_bufs[ref_idx].buf.fb->base_y.va);
				vp9_add_to_fb_free_list(inst, fb);
			} else
				vp9_free_sf_ref_fb(
					inst, drv->frm_bufs[ref_idx].buf.fb);
		}
	}

	*idx = new_idx;
	drv->frm_bufs[new_idx].ref_cnt++;
}

static bool vp9_realloc_work_buf(struct vdec_vp9_inst *inst)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;
	int result;
	struct mtk_vcodec_mem *mem;

	inst->frm_hdr.width = inst->vpu.drv->pic_w;
	inst->frm_hdr.height = inst->vpu.drv->pic_h;
	inst->work_buf.frmbuf_width = inst->vpu.drv->buf_w;
	inst->work_buf.frmbuf_height = inst->vpu.drv->buf_h;

	mtk_vcodec_debug(inst, "BUF CHG(%d): w/h/sb_w/sb_h=%d/%d/%d/%d",
		     inst->frm_hdr.resolution_changed,
		     inst->frm_hdr.width,
		     inst->frm_hdr.height,
		     inst->work_buf.frmbuf_width,
		     inst->work_buf.frmbuf_height);

	if ((inst->frm_hdr.width > 4096) ||
	    (inst->frm_hdr.height > 2304)) {
		mtk_vcodec_err(inst, "Invalid w/h %d/%d",
			inst->frm_hdr.width,
			inst->frm_hdr.height);
		return false;
	}

	mem = &inst->work_buf.mv_buf;
	/* Free First */
	if (mem->va)
		mtk_vcodec_mem_free(inst->ctx, mem);
	/* Alloc Later */
	mem->size = ((inst->work_buf.frmbuf_width / 64) *
		    (inst->work_buf.frmbuf_height / 64) + 2) * 36 * 16;

	result = mtk_vcodec_mem_alloc(inst->ctx, mem);
	if (result) {
		mtk_vcodec_err(inst, "Cannot allocate mv_buf");
		return false;
	}
	/* Set the va again */
	drv->mv_buf.va = (unsigned long)inst->work_buf.mv_buf.va;
	drv->mv_buf.pa = (unsigned long)inst->work_buf.mv_buf.dma_addr;
	drv->mv_buf.sz = (unsigned long)inst->work_buf.mv_buf.size;

	vp9_free_all_sf_ref_fb(inst);
	drv->sf_next_ref_fb_idx = vp9_get_sf_ref_fb(inst);

	inst->frm_hdr.resolution_changed = true;



	return true;
}

static void vp9_swap_frm_bufs(struct vdec_vp9_inst *inst)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;
	struct vp9_fb_info *frm_to_show;
	int ref_index = 0, mask;

	for (mask = inst->vpu.drv->refresh_frm_flags; mask; mask >>= 1) {
		if (mask & 1)
			vp9_ref_cnt_fb(inst, &drv->ref_frm_map[ref_index],
				       drv->new_fb_idx);
		++ref_index;
	}

	frm_to_show = &drv->frm_bufs[drv->new_fb_idx].buf;
	--drv->frm_bufs[drv->new_fb_idx].ref_cnt;

	if (frm_to_show->fb != inst->cur_fb) {
		if ((frm_to_show->fb != NULL) &&
			(inst->cur_fb->base_y.size >= frm_to_show->fb->base_y.size)) {
			memcpy((void *)inst->cur_fb->base_y.va,
				(void *)frm_to_show->fb->base_y.va,
				inst->work_buf.frmbuf_width *
				inst->work_buf.frmbuf_height);
			memcpy((void *)inst->cur_fb->base_c.va,
				(void *)frm_to_show->fb->base_c.va,
				inst->work_buf.frmbuf_width *
				inst->work_buf.frmbuf_height / 2);
		} else {
			mtk_vcodec_debug(inst,
				"inst->cur_fb->base_y.size=%zx, frm_to_show->fb.base_y.size=%zx",
				inst->cur_fb->base_y.size,
				frm_to_show->fb->base_y.size);
		}
		if (!vp9_is_sf_ref_fb(inst, inst->cur_fb)) {
			if (inst->frm_hdr.show_frame)
				vp9_add_to_fb_disp_list(inst, inst->cur_fb);
		}
	} else {
		if (!vp9_is_sf_ref_fb(inst, inst->cur_fb)) {
			if (inst->frm_hdr.show_frame)
				vp9_add_to_fb_disp_list(inst, frm_to_show->fb);
		}
	}

	if (drv->frm_bufs[drv->new_fb_idx].ref_cnt == 0) {
		if (!vp9_is_sf_ref_fb(
			inst, drv->frm_bufs[drv->new_fb_idx].buf.fb)) {
			struct vdec_fb *fb;

			fb = vp9_rm_from_fb_use_list(inst,
			     drv->frm_bufs[drv->new_fb_idx].buf.fb->base_y.va);

			vp9_add_to_fb_free_list(inst, fb);
		} else
			vp9_free_sf_ref_fb(
				inst, drv->frm_bufs[drv->new_fb_idx].buf.fb);
	}

	if (drv->sf_frm_cnt > 0 && drv->sf_frm_idx != drv->sf_frm_cnt - 1)
		drv->sf_next_ref_fb_idx = vp9_get_sf_ref_fb(inst);

}

static bool vp9_wait_dec_end(struct vdec_vp9_inst *inst)
{
	struct mtk_vcodec_ctx *ctx = inst->ctx;
	unsigned int irq_status;

	mtk_vcodec_wait_for_done_ctx(inst->ctx, MTK_INST_IRQ_RECEIVED, 1000,
				     true);
	irq_status = ctx->irq_status;
	mtk_vcodec_debug(inst, "isr return %x", irq_status);

	if (irq_status & 0x10000)
		return true;
	else
		return false;
}

/* End */

bool vp9_get_hw_reg_base(struct vdec_vp9_inst *inst)
{
	mtk_vcodec_debug_enter(inst);

	inst->hw_reg_base.sys = mtk_vcodec_get_reg_addr(inst->ctx,
							  VDEC_SYS);
	inst->hw_reg_base.misc = mtk_vcodec_get_reg_addr(inst->ctx,
							   VDEC_MISC);
	inst->hw_reg_base.ld = mtk_vcodec_get_reg_addr(inst->ctx, VDEC_LD);
	inst->hw_reg_base.top = mtk_vcodec_get_reg_addr(inst->ctx,
							  VDEC_TOP);
	inst->hw_reg_base.cm = mtk_vcodec_get_reg_addr(inst->ctx, VDEC_CM);
	inst->hw_reg_base.av = mtk_vcodec_get_reg_addr(inst->ctx, VDEC_AV);
	inst->hw_reg_base.hwp = mtk_vcodec_get_reg_addr(inst->ctx, VDEC_PP);
	inst->hw_reg_base.hwb = mtk_vcodec_get_reg_addr(inst->ctx,
							  VDEC_HWB);
	inst->hw_reg_base.hwg = mtk_vcodec_get_reg_addr(inst->ctx,
							  VDEC_HWG);

	return true;
}

bool vp9_alloc_work_buf(struct vdec_vp9_inst *inst)
{
	int result;
	struct mtk_vcodec_mem *mem;
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;

	/* mv_buf */
	mem = &inst->work_buf.mv_buf;
	mem->size = ((inst->work_buf.frmbuf_width / 64) *
		    (inst->work_buf.frmbuf_height / 64) + 2) * 36 * 16;

	result = mtk_vcodec_mem_alloc(inst->ctx, mem);
	if (result) {
		mtk_vcodec_err(inst, "Cannot allocate mv_buf");
		return false;
	}

	drv->sf_next_ref_fb_idx = vp9_get_sf_ref_fb(inst);
	vp9_setup_buf(inst);
	return true;
}

bool vp9_free_work_buf(struct vdec_vp9_inst *inst)
{
	struct mtk_vcodec_mem *mem;

	mtk_vcodec_debug_enter(inst);

	mem = &inst->work_buf.mv_buf;
	if (mem->va)
		mtk_vcodec_mem_free(inst->ctx, mem);

	vp9_free_all_sf_ref_fb(inst);
	return true;
}

struct vdec_vp9_inst *vp9_alloc_inst(void *ctx)
{
	int result;
	struct mtk_vcodec_mem mem;
	struct vdec_vp9_inst *inst;

	mem.size = sizeof(struct vdec_vp9_inst) * 2;
	result = mtk_vcodec_mem_alloc(ctx, &mem);
	if (result)
		return NULL;

	inst = mem.va;
	inst->mem = mem;

	return inst;
}

void vp9_free_handle(struct vdec_vp9_inst *inst)
{
	struct mtk_vcodec_mem mem;

	mem = inst->mem;
	if (mem.va)
		mtk_vcodec_mem_free(inst->ctx, &mem);
}

bool vp9_init_proc(struct vdec_vp9_inst *inst,
		   struct vdec_pic_info *pic_info)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;

	pic_info->pic_w = drv->pic_w;
	pic_info->pic_h = drv->pic_h;
	pic_info->buf_w = drv->buf_w;
	pic_info->buf_h = drv->buf_h;

	mtk_vcodec_debug(inst,
			"(PicW,PicH,BufW,BufH) = (%d,%d,%d,%d) profile=%d",
			pic_info->pic_w, pic_info->pic_h,
			pic_info->buf_w, pic_info->buf_h, drv->profile);

	inst->frm_hdr.width = pic_info->pic_w;
	inst->frm_hdr.height = pic_info->pic_h;
	inst->work_buf.frmbuf_width = pic_info->buf_w;
	inst->work_buf.frmbuf_height = pic_info->buf_h;

	/* ----> HW limitation */
	if ((inst->frm_hdr.width > 4096) ||
	    (inst->frm_hdr.height > 2304)) {
		mtk_vcodec_err(inst, "Invalid w/h %d/%d",
			inst->frm_hdr.width,
			inst->frm_hdr.height);
		return false;
	}

	/* ----> HW limitation */
	if (drv->profile > 0) {
		mtk_vcodec_err(inst, "vp9_dec DO NOT support profile(%d) > 0",
			     drv->profile);
		return false;
	}
	if ((inst->work_buf.frmbuf_width > 4096) ||
	    (inst->work_buf.frmbuf_height > 2304)) {
		mtk_vcodec_err(inst, "vp9_dec DO NOT support (W,H) = (%d,%d)",
			     inst->work_buf.frmbuf_width,
			     inst->work_buf.frmbuf_height);
		return false;
	}
	/* <---- HW limitation */

	return true;
}

bool vp9_dec_proc(struct vdec_vp9_inst *inst, struct mtk_vcodec_mem *bs,
		  struct vdec_fb *fb)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;
	unsigned int i;

	mtk_vcodec_debug_enter(inst);

	drv->bs = *bs;
	drv->fb = *fb;
	/* TBD: use barrel shifter if fast enough */
	if (!drv->sf_init) {
		unsigned int sf_bs_sz;
		unsigned int sf_bs_off;
		unsigned char *sf_bs_src;
		unsigned char *sf_bs_dst;

		sf_bs_sz = bs->size > VP9_SUPER_FRAME_BS_SZ ?
			VP9_SUPER_FRAME_BS_SZ :	bs->size;
		sf_bs_off = VP9_SUPER_FRAME_BS_SZ - sf_bs_sz;
		sf_bs_src = bs->va + bs->size - sf_bs_sz;
		sf_bs_dst = drv->sf_bs_buf + sf_bs_off;
		memcpy(sf_bs_dst, sf_bs_src, sf_bs_sz);
	} else {
		if ((drv->sf_frm_cnt > 0) &&
		    (drv->sf_frm_idx < drv->sf_frm_cnt)) {
			unsigned int idx = drv->sf_frm_idx;

			/* TBD: use barrel shifter to reposition bit stream */
			memcpy((void *)drv->input_ctx.v_frm_sa,
			       (void *)(drv->input_ctx.v_frm_sa +
			       drv->sf_frm_offset[idx]),
			       drv->sf_frm_sz[idx]);
		}
	}

	if (0 != vp9_dec_vpu_start(inst)) {
		mtk_vcodec_err(inst, "vp9_dec_vpu_start failed");
		return false;
	}

	if (drv->sf_frm_cnt > 0) {
		if (drv->sf_frm_idx < drv->sf_frm_cnt)
			inst->cur_fb =
				&drv->sf_ref_fb[drv->sf_next_ref_fb_idx].fb;
		else
			inst->cur_fb = fb;
	} else {
		inst->cur_fb = fb;
	}
	drv->frm_bufs[drv->new_fb_idx].buf.fb = inst->cur_fb;
	if (!vp9_is_sf_ref_fb(inst, inst->cur_fb))
		vp9_add_to_fb_use_list(inst, inst->cur_fb);

	if (drv->resolution_changed) {
		if (!vp9_realloc_work_buf(inst))
			return false;
		return true;
	}

	mtk_vcodec_debug(inst, "[#pic %d]", drv->frm_num);

	/* the same as VP9_SKIP_FRAME */
	inst->frm_hdr.show_frame = drv->show_frm;

	if (drv->show_exist)
		mtk_vcodec_debug(inst,
			"drv->new_fb_idx=%d, drv->frm_to_show=%d",
			drv->new_fb_idx, drv->frm_to_show);

	if (drv->show_exist && (drv->frm_to_show < VP9_MAX_FRM_BUFF_NUM)) {
		mtk_vcodec_debug(inst,
			"Skip Decode drv->new_fb_idx=%d, drv->frm_to_show=%d",
			drv->new_fb_idx, drv->frm_to_show);
		vp9_ref_cnt_fb(inst, &drv->new_fb_idx, drv->frm_to_show);
		return true;
	}

	/* VPU assign the buffer pointer in its address space, reassign here */
	for (i = 0; i < REFS_PER_FRAME; i++) {
		unsigned int idx = drv->frm_refs[i].idx;

		drv->frm_refs[i].buf = &drv->frm_bufs[idx].buf;
	}

	mtk_vcodec_debug_leave(inst);

	return true;
}

bool vp9_check_proc(struct vdec_vp9_inst *inst)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;
	bool ret = false;

	mtk_vcodec_debug_enter(inst);

	if (drv->show_exist) {
		vp9_swap_frm_bufs(inst);
		mtk_vcodec_debug(inst, "Decode Ok @%d (show_exist)",
				 drv->frm_num);
		drv->frm_num++;
		return true;
	}

	ret = vp9_wait_dec_end(inst);
	if (!ret) {
		mtk_vcodec_err(inst, "Decode NG, Decode Timeout @[%d]",
			       drv->frm_num);
		return false;
	}

	if (0 != vp9_dec_vpu_end(inst)) {
		mtk_vcodec_err(inst, "vp9_dec_vpu_end failed");
		return false;
	}

	vp9_swap_frm_bufs(inst);
	mtk_vcodec_debug(inst, "Decode Ok @%d (%d/%d)", drv->frm_num,
		     inst->frm_hdr.width, inst->frm_hdr.height);

	drv->frm_num++;

	mtk_vcodec_debug_leave(inst);

	return true;
}

int vp9_get_sf_ref_fb(struct vdec_vp9_inst *inst)
{
	int i;
	struct mtk_vcodec_mem *mem;
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;

	for (i = 0; i < VP9_MAX_FRM_BUFF_NUM - 1; i++) {
		if (inst->work_buf.sf_ref_buf[i].base_y.va &&
		    drv->sf_ref_fb[i].used == 0) {
			return i;
		}
	}

	for (i = 0; i < VP9_MAX_FRM_BUFF_NUM - 1; i++) {
		if (inst->work_buf.sf_ref_buf[i].base_y.va == NULL)
			break;
	}

	if (i == VP9_MAX_FRM_BUFF_NUM - 1) {
		mtk_vcodec_err(inst, "List Full");
		return -1;
	}

	mem = &inst->work_buf.sf_ref_buf[i].base_y;
	mem->size = inst->work_buf.frmbuf_width *
		    inst->work_buf.frmbuf_height;

	if ((inst->frm_hdr.width > 4096) ||
	    (inst->frm_hdr.height > 2304)) {
		mtk_vcodec_err(inst, "Invalid w/h %d/%d",
			inst->frm_hdr.width,
			inst->frm_hdr.height);
		return -1;
	}

	if (mtk_vcodec_mem_alloc(inst->ctx, mem)) {
		mtk_vcodec_err(inst, "Cannot allocate sf_ref_buf y_buf");
		return -1;
	}
	drv->sf_ref_fb[i].fb.base_y.va =
				inst->work_buf.sf_ref_buf[i].base_y.va;
	drv->sf_ref_fb[i].fb.base_y.dma_addr =
				inst->work_buf.sf_ref_buf[i].base_y.dma_addr;
	drv->sf_ref_fb[i].fb.base_y.size =
				inst->work_buf.sf_ref_buf[i].base_y.size;

	mem = &inst->work_buf.sf_ref_buf[i].base_c;

	mem->size = inst->work_buf.frmbuf_width *
		    inst->work_buf.frmbuf_height / 2;

	if (mtk_vcodec_mem_alloc(inst->ctx, mem)) {
		mtk_vcodec_err(inst, "Cannot allocate sf_ref_buf c_buf");
		return -1;
	}
	drv->sf_ref_fb[i].fb.base_c.va =
				inst->work_buf.sf_ref_buf[i].base_c.va;
	drv->sf_ref_fb[i].fb.base_c.dma_addr =
				inst->work_buf.sf_ref_buf[i].base_c.dma_addr;
	drv->sf_ref_fb[i].fb.base_c.size =
				inst->work_buf.sf_ref_buf[i].base_c.size;

	drv->sf_ref_fb[i].used = 0;
	drv->sf_ref_fb[i].idx = i;

	return i;
}

bool vp9_free_sf_ref_fb(struct vdec_vp9_inst *inst, struct vdec_fb *fb)
{
	struct vp9_sf_ref_fb *sf_ref_fb =
		container_of(fb, struct vp9_sf_ref_fb, fb);

	sf_ref_fb->used = 0;

	return true;
}

void vp9_free_all_sf_ref_fb(struct vdec_vp9_inst *inst)
{
	int i;
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;

	for (i = 0; i < VP9_MAX_FRM_BUFF_NUM - 1; i++) {
		if (inst->work_buf.sf_ref_buf[i].base_y.va) {
			mtk_vcodec_mem_free(inst->ctx,
				    &inst->work_buf.sf_ref_buf[i].base_y);
			mtk_vcodec_mem_free(inst->ctx,
				    &inst->work_buf.sf_ref_buf[i].base_c);
			drv->sf_ref_fb[i].used = 0;
		}
	}
}

bool vp9_is_sf_ref_fb(struct vdec_vp9_inst *inst, struct vdec_fb *fb)
{
	int i;
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;

	for (i = 0; i < VP9_MAX_FRM_BUFF_NUM - 1; i++) {
		if (fb == &drv->sf_ref_fb[i].fb)
			break;
	}

	if (i == VP9_MAX_FRM_BUFF_NUM - 1)
		return false;

	return true;
}

bool vp9_is_last_sub_frm(struct vdec_vp9_inst *inst)
{
	struct vdec_vp9_vpu_drv *drv = inst->vpu.drv;

	if (drv->sf_frm_cnt <= 0 || drv->sf_frm_idx == drv->sf_frm_cnt)
		return true;

	return false;
}

bool vp9_add_to_fb_disp_list(struct vdec_vp9_inst *inst,
			     struct vdec_fb *fb)
{
	struct vdec_fb_node *node;

	if (!fb)
		return false;

	node = list_first_entry_or_null(&inst->available_fb_node_list,
					struct vdec_fb_node, list);
	if (node) {
		node->fb = fb;
		list_move_tail(&node->list, &inst->fb_disp_list);
	} else {
		mtk_vcodec_debug(inst, "List Full");
		return false;
	}

	mtk_vcodec_debug_leave(inst);

	return true;
}

struct vdec_fb *vp9_rm_from_fb_disp_list(struct vdec_vp9_inst
		*inst)
{
	struct vdec_fb_node *node;
	struct vdec_fb *fb = NULL;

	node = list_first_entry_or_null(&inst->fb_disp_list,
					struct vdec_fb_node, list);
	if (node) {
		fb = (struct vdec_fb *)node->fb;
		fb->status |= FB_ST_DISPLAY;
		list_move_tail(&node->list, &inst->available_fb_node_list);
		mtk_vcodec_debug(inst, "[FB] get disp fb %p st=%d",
				 node->fb, fb->status);
	} else
		mtk_vcodec_debug(inst, "[FB] there is no disp fb");

	return fb;
}

bool vp9_add_to_fb_use_list(struct vdec_vp9_inst *inst,
			    struct vdec_fb *fb)
{
	struct vdec_fb_node *node;

	if (!fb)
		return false;

	node = list_first_entry_or_null(&inst->available_fb_node_list,
					struct vdec_fb_node, list);
	if (node) {
		node->fb = fb;
		list_move_tail(&node->list, &inst->fb_use_list);
	} else {
		mtk_vcodec_debug(inst, "No free fb node");
		return false;
	}

	mtk_vcodec_debug_leave(inst);

	return true;
}

struct vdec_fb *vp9_rm_from_fb_use_list(struct vdec_vp9_inst
					*inst, void *addr)
{
	struct vdec_fb *fb = NULL;
	struct vdec_fb_node *node;

	list_for_each_entry(node, &inst->fb_use_list, list) {
		fb = (struct vdec_fb *)node->fb;
		if (fb->base_y.va == addr) {
			list_move_tail(&node->list,
				       &inst->available_fb_node_list);
			break;
		}
	}

	mtk_vcodec_debug_leave(inst);
	return fb;
}


bool vp9_add_to_fb_free_list(struct vdec_vp9_inst *inst,
			     struct vdec_fb *fb)
{
	struct vdec_fb_node *node;

	node = list_first_entry_or_null(&inst->available_fb_node_list,
					struct vdec_fb_node, list);
	if (node) {
		node->fb = fb;
		list_move_tail(&node->list, &inst->fb_free_list);
	} else
		mtk_vcodec_debug(inst, "No free fb node");

	mtk_vcodec_debug_leave(inst);

	return true;
}


struct vdec_fb *vp9_rm_from_fb_free_list(struct vdec_vp9_inst
		*inst)
{
	struct vdec_fb_node *node;
	struct vdec_fb *fb = NULL;

	node = list_first_entry_or_null(&inst->fb_free_list,
					struct vdec_fb_node, list);
	if (node) {
		fb = (struct vdec_fb *)node->fb;
		fb->status |= FB_ST_FREE;
		list_move_tail(&node->list, &inst->available_fb_node_list);
		mtk_vcodec_debug(inst, "[FB] get free fb %p st=%d",
				 node->fb, fb->status);
	} else
		mtk_vcodec_debug(inst, "[FB] there is no free fb");

	mtk_vcodec_debug_leave(inst);

	return fb;
}

bool vp9_fb_use_list_to_fb_free_list(struct vdec_vp9_inst *inst)
{

	struct vdec_fb_node *node, *tmp;

	list_for_each_entry_safe(node, tmp, &inst->fb_use_list, list)
		list_move_tail(&node->list, &inst->fb_free_list);

	mtk_vcodec_debug_leave(inst);
	return true;
}

void vp9_reset(struct vdec_vp9_inst *inst)
{
	vp9_fb_use_list_to_fb_free_list(inst);

	vp9_free_all_sf_ref_fb(inst);
	inst->vpu.drv->sf_next_ref_fb_idx = vp9_get_sf_ref_fb(inst);

	if (0 != vp9_dec_vpu_reset(inst))
		mtk_vcodec_debug(inst, "vp9_dec_vpu_reset failed");

	vp9_setup_buf(inst);
}
