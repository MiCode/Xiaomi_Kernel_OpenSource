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

#ifndef _VDEC_VP9_CORE_H_
#define _VDEC_VP9_CORE_H_

#include "vdec_drv_base.h"
#include "mtk_vcodec_intr.h"
#include "mtk_vcodec_util.h"
#include "vdec_vp9_vpu.h"

#define VP9_SUPER_FRAME_BS_SZ 64

#define REFS_PER_FRAME 3
#define REF_FRAMES_LOG2 3
#define REF_FRAMES (1 << REF_FRAMES_LOG2)
#define VP9_MAX_FRM_BUFF_NUM (9 + 0)
#define VP9_MAX_FRM_BUFF_NODE_NUM (9 + 9)

struct vdec_vp9_hw_reg_base {
	void __iomem *sys;
	void __iomem *misc;
	void __iomem *ld;
	void __iomem *top;
	void __iomem *cm;
	void __iomem *av;
	void __iomem *hwp;
	void __iomem *hwb;
	void __iomem *hwg;
};

struct vdec_vp9_frm_hdr {
	unsigned int width;
	unsigned int height;
	unsigned char show_frame; /* for fb_status |= FB_ST_NOT_DISPLAY */
	unsigned char resolution_changed;
};

struct vdec_vp9_work_buf {
	unsigned int frmbuf_width;
	unsigned int frmbuf_height;
	struct mtk_vcodec_mem seg_id_buf;
	struct mtk_vcodec_mem tile_buf;
	struct mtk_vcodec_mem count_tbl_buf;
	struct mtk_vcodec_mem prob_tbl_buf;
	struct mtk_vcodec_mem mv_buf;
	struct vdec_fb sf_ref_buf[VP9_MAX_FRM_BUFF_NUM-1];

};

struct vp9_input_ctx {
	unsigned long v_fifo_sa;
	unsigned long v_fifo_ea;
	unsigned long p_fifo_sa;
	unsigned long p_fifo_ea;
	unsigned long v_frm_sa;
	unsigned long v_frm_ea;
	unsigned long p_frm_sa;
	unsigned long p_frm_end;
	unsigned int frm_sz;
	unsigned int uncompress_sz;
};

struct vp9_dram_buf {
	unsigned long va;
	unsigned long pa;
	unsigned int sz;
	unsigned int vpua;
};

struct vp9_fb_info {
	struct vdec_fb *fb;
	struct vp9_dram_buf y_buf;
	struct vp9_dram_buf c_buf;
	unsigned int y_width;
	unsigned int y_height;
	unsigned int y_crop_width;
	unsigned int y_crop_height;

	unsigned int c_width;
	unsigned int c_height;
	unsigned int c_crop_width;
	unsigned int c_crop_height;

	unsigned int frm_num;
};

struct vp9_ref_cnt_buf {
	struct vp9_fb_info buf;
	unsigned int ref_cnt;
};

struct vp9_scale_factors {
	int x_scale_fp; /* horizontal fixed point scale factor */
	int y_scale_fp; /* vertical fixed point scale factor */
	int x_step_q4;
	int y_step_q4;
	unsigned int ref_scaling_en;
};

struct vp9_ref_buf {
	struct vp9_fb_info *buf;
	struct vp9_scale_factors scale_factors;
	unsigned int idx; /* index of frm_bufs[VP9_MAX_FRM_BUFF_NUM] */
};

struct vp9_sf_ref_fb {
	struct vdec_fb fb;
	int used;
	int idx;
};


/*
 * struct vdec_vp9_vpu_drv - shared buffer between host and VPU driver
 */
struct vdec_vp9_vpu_drv {
	unsigned char sf_bs_buf[VP9_SUPER_FRAME_BS_SZ];
	struct vp9_sf_ref_fb sf_ref_fb[VP9_MAX_FRM_BUFF_NUM-1];
	int sf_next_ref_fb_idx;
	unsigned int sf_frm_cnt;
	unsigned int sf_frm_offset[VP9_MAX_FRM_BUFF_NUM-1];
	unsigned int sf_frm_sz[VP9_MAX_FRM_BUFF_NUM-1];
	unsigned int sf_frm_idx;
	unsigned int sf_init;
	struct vdec_fb fb;
	struct mtk_vcodec_mem bs;
	struct vdec_fb cur_fb;
	unsigned int pic_w;
	unsigned int pic_h;
	unsigned int buf_w;
	unsigned int buf_h;
	unsigned int profile;
	unsigned int show_frm;
	unsigned int show_exist;
	unsigned int frm_to_show;
	unsigned int refresh_frm_flags;
	unsigned int resolution_changed;

	struct vp9_input_ctx input_ctx;
	/* should not be changed except reallocate */
	struct vp9_ref_cnt_buf frm_bufs[VP9_MAX_FRM_BUFF_NUM];
	int ref_frm_map[REF_FRAMES]; /* maps fb_idx to reference slot */
	unsigned int new_fb_idx;
	unsigned int frm_num;
	/* Dram Buffer */
	struct vp9_dram_buf seg_id_buf;
	struct vp9_dram_buf tile_buf;
	struct vp9_dram_buf count_tbl_buf;
	struct vp9_dram_buf prob_tbl_buf;
	struct vp9_dram_buf mv_buf;
	/* Each frame can reference REFS_PER_FRAME buffers */
	struct vp9_ref_buf frm_refs[REFS_PER_FRAME];

};

struct vdec_vp9_vpu_inst {
	wait_queue_head_t wq_hd;
	int signaled;
	int failure;
	unsigned int h_drv;
	struct vdec_vp9_vpu_drv *drv;
};

/* VP9 VDEC handle */
struct vdec_vp9_inst {
	struct vdec_vp9_hw_reg_base hw_reg_base;
	struct vdec_vp9_work_buf work_buf;
	struct vdec_vp9_frm_hdr frm_hdr;
	struct vdec_fb_node dec_fb[VP9_MAX_FRM_BUFF_NODE_NUM];
	struct list_head available_fb_node_list;
	struct list_head fb_use_list;
	struct list_head fb_free_list;
	struct list_head fb_disp_list;
	struct vdec_fb *cur_fb;
	unsigned int frm_cnt;
	unsigned int total_frm_cnt;
	void *ctx;
	struct platform_device *dev;
	struct vdec_vp9_vpu_inst vpu;
	unsigned int show_reg;
	struct file *log;
	struct mtk_vcodec_mem mem;
};

bool vp9_get_hw_reg_base(struct vdec_vp9_inst *handle);
bool vp9_alloc_work_buf(struct vdec_vp9_inst *handle);
bool vp9_free_work_buf(struct vdec_vp9_inst *handle);
struct vdec_vp9_inst *vp9_alloc_inst(void *ctx);
void vp9_free_handle(struct vdec_vp9_inst *handle);
bool vp9_init_proc(struct vdec_vp9_inst *handle,
		   struct vdec_pic_info *pic_info);
bool vp9_dec_proc(struct vdec_vp9_inst *handle, struct mtk_vcodec_mem *bs,
		  struct vdec_fb *fb);
bool vp9_check_proc(struct vdec_vp9_inst *handle);
int vp9_get_sf_ref_fb(struct vdec_vp9_inst *handle);
bool vp9_free_sf_ref_fb(struct vdec_vp9_inst *handle, struct vdec_fb *fb);
void vp9_free_all_sf_ref_fb(struct vdec_vp9_inst *handle);
bool vp9_is_sf_ref_fb(struct vdec_vp9_inst *handle, struct vdec_fb *fb);
bool vp9_is_last_sub_frm(struct vdec_vp9_inst *handle);

bool vp9_add_to_fb_disp_list(struct vdec_vp9_inst *handle,
			     struct vdec_fb *fb);
struct vdec_fb *vp9_rm_from_fb_disp_list(struct vdec_vp9_inst
		*handle);
bool vp9_add_to_fb_use_list(struct vdec_vp9_inst *handle,
			    struct vdec_fb *fb);
struct vdec_fb *vp9_rm_from_fb_use_list(struct vdec_vp9_inst
					*handle, void *addr);
bool vp9_add_to_fb_free_list(struct vdec_vp9_inst *handle,
			     struct vdec_fb *fb);
struct vdec_fb *vp9_rm_from_fb_free_list(struct vdec_vp9_inst
		*handle);
bool vp9_fb_use_list_to_fb_free_list(struct vdec_vp9_inst
				     *handle);
void vp9_reset(struct vdec_vp9_inst *handle);

#endif
