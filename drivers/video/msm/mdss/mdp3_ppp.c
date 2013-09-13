/* Copyright (c) 2007, 2013 The Linux Foundation. All rights reserved.
 * Copyright (C) 2007 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/file.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/sync.h>
#include <linux/sw_sync.h>
#include "linux/proc_fs.h"
#include <linux/delay.h>

#include "mdss_fb.h"
#include "mdp3_ppp.h"
#include "mdp3_hwio.h"
#include "mdp3.h"

#define MDP_IS_IMGTYPE_BAD(x) ((x) >= MDP_IMGTYPE_LIMIT)
#define MDP_RELEASE_BW_TIMEOUT 50
#define MDP_BLIT_CLK_RATE	200000000
#define MDP_PPP_MAX_BPP 4
#define MDP_PPP_DYNAMIC_FACTOR 3
#define MDP_PPP_MAX_READ_WRITE 3

static const bool valid_fmt[MDP_IMGTYPE_LIMIT] = {
	[MDP_RGB_565] = true,
	[MDP_BGR_565] = true,
	[MDP_RGB_888] = true,
	[MDP_BGR_888] = true,
	[MDP_BGRA_8888] = true,
	[MDP_RGBA_8888] = true,
	[MDP_ARGB_8888] = true,
	[MDP_XRGB_8888] = true,
	[MDP_RGBX_8888] = true,
	[MDP_Y_CRCB_H2V2] = true,
	[MDP_Y_CBCR_H2V2] = true,
	[MDP_Y_CBCR_H2V2_ADRENO] = true,
	[MDP_Y_CBCR_H2V2_VENUS] = true,
	[MDP_YCRYCB_H2V1] = true,
	[MDP_Y_CBCR_H2V1] = true,
	[MDP_Y_CRCB_H2V1] = true,
};

#define MAX_LIST_WINDOW 16
#define MDP3_PPP_MAX_LIST_REQ 8

struct blit_req_list {
	int count;
	struct mdp_blit_req req_list[MAX_LIST_WINDOW];
	struct mdp3_img_data src_data[MAX_LIST_WINDOW];
	struct mdp3_img_data dst_data[MAX_LIST_WINDOW];
	struct sync_fence *acq_fen[MDP_MAX_FENCE_FD];
	u32 acq_fen_cnt;
	int cur_rel_fen_fd;
	struct sync_pt *cur_rel_sync_pt;
	struct sync_fence *cur_rel_fence;
	struct sync_fence *last_rel_fence;
};

struct blit_req_queue {
	struct blit_req_list req[MDP3_PPP_MAX_LIST_REQ];
	int count;
	int push_idx;
	int pop_idx;
};

struct ppp_status {
	int busy;
	bool wait_for_pop;
	spinlock_t ppp_lock;
	struct completion ppp_comp;
	struct completion pop_q_comp;
	struct mutex req_mutex; /* Protect request queue */
	struct mutex config_ppp_mutex; /* Only one client configure register */
	struct msm_fb_data_type *mfd;

	struct work_struct blit_work;
	struct blit_req_queue req_q;

	struct sw_sync_timeline *timeline;
	int timeline_value;

	struct timer_list free_bw_timer;
	struct work_struct free_bw_work;
	bool bw_on;
};

static struct ppp_status *ppp_stat;


int ppp_get_bpp(uint32_t format, uint32_t fb_format)
{
	int bpp = -EINVAL;
	if (format == MDP_FB_FORMAT)
		format = fb_format;

	bpp = ppp_bpp(format);
	if (bpp <= 0)
		pr_err("%s incorrect format %d\n", __func__, format);
	return bpp;
}

int mdp3_ppp_get_img(struct mdp_img *img, struct mdp_blit_req *req,
		struct mdp3_img_data *data)
{
	struct msmfb_data fb_data;
	fb_data.flags = img->priv;
	fb_data.memory_id = img->memory_id;
	fb_data.offset = 0;

	return mdp3_get_img(&fb_data, data);
}

/* Check format */
int mdp3_ppp_verify_fmt(struct mdp_blit_req *req)
{
	if (MDP_IS_IMGTYPE_BAD(req->src.format) ||
	    MDP_IS_IMGTYPE_BAD(req->dst.format)) {
		pr_err("%s: Color format out of range\n", __func__);
		return -EINVAL;
	}

	if (!valid_fmt[req->src.format] ||
	    !valid_fmt[req->dst.format]) {
		pr_err("%s: Color format not supported\n", __func__);
		return -EINVAL;
	}
	return 0;
}

/* Check resolution */
int mdp3_ppp_verify_res(struct mdp_blit_req *req)
{
	if ((req->src.width == 0) || (req->src.height == 0) ||
	    (req->src_rect.w == 0) || (req->src_rect.h == 0) ||
	    (req->dst.width == 0) || (req->dst.height == 0) ||
	    (req->dst_rect.w == 0) || (req->dst_rect.h == 0)) {
		pr_err("%s: Height/width can't be 0\n", __func__);
		return -EINVAL;
	}

	if (((req->src_rect.x + req->src_rect.w) > req->src.width) ||
	    ((req->src_rect.y + req->src_rect.h) > req->src.height)) {
		pr_err("%s: src roi larger than boundary\n", __func__);
		return -EINVAL;
	}

	if (((req->dst_rect.x + req->dst_rect.w) > req->dst.width) ||
	    ((req->dst_rect.y + req->dst_rect.h) > req->dst.height)) {
		pr_err("%s: dst roi larger than boundary\n", __func__);
		return -EINVAL;
	}
	return 0;
}

/* scaling range check */
int mdp3_ppp_verify_scale(struct mdp_blit_req *req)
{
	u32 src_width, src_height, dst_width, dst_height;

	src_width = req->src_rect.w;
	src_height = req->src_rect.h;

	if (req->flags & MDP_ROT_90) {
		dst_width = req->dst_rect.h;
		dst_height = req->dst_rect.w;
	} else {
		dst_width = req->dst_rect.w;
		dst_height = req->dst_rect.h;
	}

	switch (req->dst.format) {
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CBCR_H2V2:
		src_width = (src_width / 2) * 2;
		src_height = (src_height / 2) * 2;
		dst_width = (dst_width / 2) * 2;
		dst_height = (dst_height / 2) * 2;
		break;

	case MDP_Y_CRCB_H2V1:
	case MDP_Y_CBCR_H2V1:
	case MDP_YCRYCB_H2V1:
		src_width = (src_width / 2) * 2;
		dst_width = (dst_width / 2) * 2;
		break;

	default:
		break;
	}

	if (((MDP_SCALE_Q_FACTOR * dst_width) / src_width >
	     MDP_MAX_X_SCALE_FACTOR)
	    || ((MDP_SCALE_Q_FACTOR * dst_width) / src_width <
		MDP_MIN_X_SCALE_FACTOR)) {
		pr_err("%s: x req scale factor beyond capability\n", __func__);
		return -EINVAL;
	}

	if (((MDP_SCALE_Q_FACTOR * dst_height) / src_height >
	     MDP_MAX_Y_SCALE_FACTOR)
	    || ((MDP_SCALE_Q_FACTOR * dst_height) / src_height <
		MDP_MIN_Y_SCALE_FACTOR)) {
		pr_err("%s: y req scale factor beyond capability\n", __func__);
		return -EINVAL;
	}
	return 0;
}

/* operation check */
int mdp3_ppp_verify_op(struct mdp_blit_req *req)
{
	if (req->flags & MDP_DEINTERLACE) {
		pr_err("\n%s(): deinterlace not supported", __func__);
		return -EINVAL;
	}

	if (req->flags & MDP_SHARPENING) {
		pr_err("\n%s(): sharpening not supported", __func__);
		return -EINVAL;
	}
	return 0;
}

int mdp3_ppp_verify_req(struct mdp_blit_req *req)
{
	int rc;

	if (req == NULL) {
		pr_err("%s: req == null\n", __func__);
		return -EINVAL;
	}

	rc = mdp3_ppp_verify_fmt(req);
	rc |= mdp3_ppp_verify_res(req);
	rc |= mdp3_ppp_verify_scale(req);
	rc |= mdp3_ppp_verify_op(req);

	return rc;
}

int mdp3_ppp_pipe_wait(void)
{
	int ret = 1;
	int wait;
	unsigned long flag;

	/*
	 * wait 5 secs for operation to complete before declaring
	 * the MDP hung
	 */
	spin_lock_irqsave(&ppp_stat->ppp_lock, flag);
	wait = ppp_stat->busy;
	spin_unlock_irqrestore(&ppp_stat->ppp_lock, flag);

	if (wait) {
		ret = wait_for_completion_interruptible_timeout(
		   &ppp_stat->ppp_comp, 5 * HZ);
		if (!ret)
			pr_err("%s: Timed out waiting for the MDP.\n",
				__func__);
	}

	return ret;
}

uint32_t mdp3_calc_tpval(struct ppp_img_desc *img, uint32_t old_tp)
{
	uint32_t tpVal;
	uint8_t plane_tp;

	tpVal = 0;
	if ((img->color_fmt == MDP_RGB_565)
	    || (img->color_fmt == MDP_BGR_565)) {
		/* transparent color conversion into 24 bpp */
		plane_tp = (uint8_t) ((old_tp & 0xF800) >> 11);
		tpVal |= ((plane_tp << 3) | ((plane_tp & 0x1C) >> 2)) << 16;
		plane_tp = (uint8_t) (old_tp & 0x1F);
		tpVal |= ((plane_tp << 3) | ((plane_tp & 0x1C) >> 2)) << 8;

		plane_tp = (uint8_t) ((old_tp & 0x7E0) >> 5);
		tpVal |= ((plane_tp << 2) | ((plane_tp & 0x30) >> 4));
	} else {
		/* 24bit RGB to RBG conversion */
		tpVal = (old_tp & 0xFF00) >> 8;
		tpVal |= (old_tp & 0xFF) << 8;
		tpVal |= (old_tp & 0xFF0000);
	}

	return tpVal;
}

static void mdp3_ppp_intr_handler(int type, void *arg)
{
	spin_lock(&ppp_stat->ppp_lock);
	ppp_stat->busy = false;
	spin_unlock(&ppp_stat->ppp_lock);
	complete(&ppp_stat->ppp_comp);
	mdp3_irq_disable_nosync(type);
}

static int mdp3_ppp_callback_setup(void)
{
	int rc;
	struct mdp3_intr_cb ppp_done_cb = {
		.cb = mdp3_ppp_intr_handler,
		.data = NULL,
	};

	rc = mdp3_set_intr_callback(MDP3_PPP_DONE, &ppp_done_cb);
	return rc;
}

void mdp3_ppp_kickoff(void)
{
	unsigned long flag;
	mdp3_irq_enable(MDP3_PPP_DONE);

	init_completion(&ppp_stat->ppp_comp);

	spin_lock_irqsave(&ppp_stat->ppp_lock, flag);
	ppp_stat->busy = true;
	spin_unlock_irqrestore(&ppp_stat->ppp_lock, flag);
	ppp_enable();

	mdp3_ppp_pipe_wait();
}

int mdp3_ppp_turnon(struct msm_fb_data_type *mfd, int on_off)
{
	struct mdss_panel_info *panel_info = mfd->panel_info;
	uint64_t ab = 0, ib = 0;
	int rate = 0;
	int rc;

	if (on_off) {
		rate = MDP_BLIT_CLK_RATE;
		ab = panel_info->xres * panel_info->yres *
			panel_info->mipi.frame_rate *
			MDP_PPP_MAX_BPP *
			MDP_PPP_DYNAMIC_FACTOR *
			MDP_PPP_MAX_READ_WRITE;
		ib = (ab * 3) / 2;
	}
	mdp3_clk_set_rate(MDP3_CLK_CORE, rate, MDP3_CLIENT_PPP);
	rc = mdp3_clk_enable(on_off);
	if (rc < 0) {
		pr_err("%s: mdp3_clk_enable failed\n", __func__);
		return rc;
	}
	mdp3_bus_scale_set_quota(MDP3_CLIENT_PPP, ab, ib);
	ppp_stat->bw_on = on_off;
	return 0;
}

void mdp3_start_ppp(struct ppp_blit_op *blit_op)
{
	/* Wait for the pipe to clear */
	do { } while (mdp3_ppp_pipe_wait() <= 0);
	config_ppp_op_mode(blit_op);
	mdp3_ppp_kickoff();
}

static void mdp3_ppp_process_req(struct ppp_blit_op *blit_op,
	struct mdp_blit_req *req, struct mdp3_img_data *src_data,
	struct mdp3_img_data *dst_data)
{
	unsigned long srcp0_start, srcp0_len, dst_start, dst_len;
	uint32_t dst_width, dst_height;

	srcp0_start = (unsigned long) src_data->addr;
	srcp0_len = (unsigned long) src_data->len;
	dst_start = (unsigned long) dst_data->addr;
	dst_len = (unsigned long) dst_data->len;

	blit_op->dst.prop.width = req->dst.width;
	blit_op->dst.prop.height = req->dst.height;

	blit_op->dst.color_fmt = req->dst.format;
	blit_op->dst.p0 = (void *) dst_start;
	blit_op->dst.p0 += req->dst.offset;

	blit_op->dst.roi.x = req->dst_rect.x;
	blit_op->dst.roi.y = req->dst_rect.y;
	blit_op->dst.roi.width = req->dst_rect.w;
	blit_op->dst.roi.height = req->dst_rect.h;

	blit_op->src.roi.x = req->src_rect.x;
	blit_op->src.roi.y = req->src_rect.y;
	blit_op->src.roi.width = req->src_rect.w;
	blit_op->src.roi.height = req->src_rect.h;

	blit_op->src.prop.width = req->src.width;
	blit_op->src.color_fmt = req->src.format;


	blit_op->src.p0 = (void *) (srcp0_start + req->src.offset);
	if (blit_op->src.color_fmt == MDP_Y_CBCR_H2V2_ADRENO)
		blit_op->src.p1 =
			(void *) ((uint32_t) blit_op->src.p0 +
				ALIGN((ALIGN(req->src.width, 32) *
				ALIGN(req->src.height, 32)), 4096));
	else if (blit_op->src.color_fmt == MDP_Y_CBCR_H2V2_VENUS)
		blit_op->src.p1 =
			(void *) ((uint32_t) blit_op->src.p0 +
				ALIGN((ALIGN(req->src.width, 128) *
				ALIGN(req->src.height, 32)), 4096));
	else
		blit_op->src.p1 = (void *) ((uint32_t) blit_op->src.p0 +
			req->src.width * req->src.height);

	if (req->flags & MDP_IS_FG)
		blit_op->mdp_op |= MDPOP_LAYER_IS_FG;

	/* blending check */
	if (req->transp_mask != MDP_TRANSP_NOP) {
		blit_op->mdp_op |= MDPOP_TRANSP;
		blit_op->blend.trans_color =
			mdp3_calc_tpval(&blit_op->src, req->transp_mask);
	} else {
		blit_op->blend.trans_color = 0;
	}

	req->alpha &= 0xff;
	if (req->alpha < MDP_ALPHA_NOP) {
		blit_op->mdp_op |= MDPOP_ALPHAB;
		blit_op->blend.const_alpha = req->alpha;
	} else {
		blit_op->blend.const_alpha = 0xff;
	}

	/* rotation check */
	if (req->flags & MDP_FLIP_LR)
		blit_op->mdp_op |= MDPOP_LR;
	if (req->flags & MDP_FLIP_UD)
		blit_op->mdp_op |= MDPOP_UD;
	if (req->flags & MDP_ROT_90)
		blit_op->mdp_op |= MDPOP_ROT90;
	if (req->flags & MDP_DITHER)
		blit_op->mdp_op |= MDPOP_DITHER;

	if (req->flags & MDP_BLEND_FG_PREMULT)
		blit_op->mdp_op |= MDPOP_FG_PM_ALPHA;

	/* scale check */
	if (req->flags & MDP_ROT_90) {
		dst_width = req->dst_rect.h;
		dst_height = req->dst_rect.w;
	} else {
		dst_width = req->dst_rect.w;
		dst_height = req->dst_rect.h;
	}

	if ((blit_op->src.roi.width != dst_width) ||
			(blit_op->src.roi.height != dst_height))
		blit_op->mdp_op |= MDPOP_ASCALE;

	if (req->flags & MDP_BLUR)
		blit_op->mdp_op |= MDPOP_ASCALE | MDPOP_BLUR;
}

static void mdp3_ppp_tile_workaround(struct ppp_blit_op *blit_op,
	struct mdp_blit_req *req)
{
	int dst_h, src_w, i;
	uint32_t mdp_op = blit_op->mdp_op;

	src_w = req->src_rect.w;
	dst_h = blit_op->dst.roi.height;
	/* bg tile fetching HW workaround */
	for (i = 0; i < (req->dst_rect.h / 16); i++) {
		/* this tile size */
		blit_op->dst.roi.height = 16;
		blit_op->src.roi.width =
			(16 * req->src_rect.w) / req->dst_rect.h;

		/* if it's out of scale range... */
		if (((MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
			 blit_op->src.roi.width) > MDP_MAX_X_SCALE_FACTOR)
			blit_op->src.roi.width =
				(MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
				MDP_MAX_X_SCALE_FACTOR;
		else if (((MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
			  blit_op->src.roi.width) < MDP_MIN_X_SCALE_FACTOR)
			blit_op->src.roi.width =
				(MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
				MDP_MIN_X_SCALE_FACTOR;

		mdp3_start_ppp(blit_op);

		/* next tile location */
		blit_op->dst.roi.y += 16;
		blit_op->src.roi.x += blit_op->src.roi.width;

		/* this is for a remainder update */
		dst_h -= 16;
		src_w -= blit_op->src.roi.width;
		/* restore mdp_op since MDPOP_ASCALE have been cleared */
		blit_op->mdp_op = mdp_op;
	}

	if ((dst_h < 0) || (src_w < 0))
		pr_err
			("msm_fb: mdp_blt_ex() unexpected result! line:%d\n",
			 __LINE__);

	/* remainder update */
	if ((dst_h > 0) && (src_w > 0)) {
		u32 tmp_v;

		blit_op->dst.roi.height = dst_h;
		blit_op->src.roi.width = src_w;

		if (((MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
			 blit_op->src.roi.width) > MDP_MAX_X_SCALE_FACTOR) {
			tmp_v =
				(MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
				MDP_MAX_X_SCALE_FACTOR +
				(MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) %
				MDP_MAX_X_SCALE_FACTOR ? 1 : 0;

			/* move x location as roi width gets bigger */
			blit_op->src.roi.x -= tmp_v - blit_op->src.roi.width;
			blit_op->src.roi.width = tmp_v;
		} else if (((MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
			 blit_op->src.roi.width) < MDP_MIN_X_SCALE_FACTOR) {
			tmp_v =
				(MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) /
				MDP_MIN_X_SCALE_FACTOR +
				(MDP_SCALE_Q_FACTOR * blit_op->dst.roi.height) %
				MDP_MIN_X_SCALE_FACTOR ? 1 : 0;

			/*
			 * we don't move x location for continuity of
			 * source image
			 */
			blit_op->src.roi.width = tmp_v;
		}


		mdp3_start_ppp(blit_op);
	}
}

static int mdp3_ppp_blit(struct msm_fb_data_type *mfd,
	struct mdp_blit_req *req, struct mdp3_img_data *src_data,
	struct mdp3_img_data *dst_data)
{
	struct ppp_blit_op blit_op;

	memset(&blit_op, 0, sizeof(blit_op));

	if (req->dst.format == MDP_FB_FORMAT)
		req->dst.format =  mfd->fb_imgType;
	if (req->src.format == MDP_FB_FORMAT)
		req->src.format = mfd->fb_imgType;

	if (mdp3_ppp_verify_req(req)) {
		pr_err("%s: invalid image!\n", __func__);
		return -EINVAL;
	}

	mdp3_ppp_process_req(&blit_op, req, src_data, dst_data);

	if (((blit_op.mdp_op & (MDPOP_TRANSP | MDPOP_ALPHAB)) ||
	     (req->src.format == MDP_ARGB_8888) ||
	     (req->src.format == MDP_BGRA_8888) ||
	     (req->src.format == MDP_RGBA_8888)) &&
	    (blit_op.mdp_op & MDPOP_ROT90) && (req->dst_rect.w <= 16)) {
		mdp3_ppp_tile_workaround(&blit_op, req);
	} else {
		mdp3_start_ppp(&blit_op);
	}

	return 0;
}

static int mdp3_ppp_blit_workaround(struct msm_fb_data_type *mfd,
		struct mdp_blit_req *req, unsigned int remainder,
		struct mdp3_img_data *src_data,
		struct mdp3_img_data *dst_data)
{
	int ret;
	struct mdp_blit_req splitreq;
	int s_x_0, s_x_1, s_w_0, s_w_1, s_y_0, s_y_1, s_h_0, s_h_1;
	int d_x_0, d_x_1, d_w_0, d_w_1, d_y_0, d_y_1, d_h_0, d_h_1;

	/* make new request as provide by user */
	splitreq = *req;

	/* break dest roi at width*/
	d_y_0 = d_y_1 = req->dst_rect.y;
	d_h_0 = d_h_1 = req->dst_rect.h;
	d_x_0 = req->dst_rect.x;

	if (remainder == 14 || remainder == 6)
		d_w_1 = req->dst_rect.w / 2;
	else
		d_w_1 = (req->dst_rect.w - 1) / 2 - 1;

	d_w_0 = req->dst_rect.w - d_w_1;
	d_x_1 = d_x_0 + d_w_0;
	/* blit first region */
	if (((splitreq.flags & 0x07) == 0x07) ||
		((splitreq.flags & 0x07) == 0x05) ||
		((splitreq.flags & 0x07) == 0x02) ||
		((splitreq.flags & 0x07) == 0x0)) {

		if (splitreq.flags & MDP_ROT_90) {
			s_x_0 = s_x_1 = req->src_rect.x;
			s_w_0 = s_w_1 = req->src_rect.w;
			s_y_0 = req->src_rect.y;
			s_h_1 = (req->src_rect.h * d_w_1) /
				req->dst_rect.w;
			s_h_0 = req->src_rect.h - s_h_1;
			s_y_1 = s_y_0 + s_h_0;
			if (d_w_1 >= 8 * s_h_1) {
				s_h_1++;
				s_y_1--;
			}
		} else {
			s_y_0 = s_y_1 = req->src_rect.y;
			s_h_0 = s_h_1 = req->src_rect.h;
			s_x_0 = req->src_rect.x;
			s_w_1 = (req->src_rect.w * d_w_1) /
				req->dst_rect.w;
			s_w_0 = req->src_rect.w - s_w_1;
			s_x_1 = s_x_0 + s_w_0;
			if (d_w_1 >= 8 * s_w_1) {
				s_w_1++;
				s_x_1--;
			}
		}

		splitreq.src_rect.h = s_h_0;
		splitreq.src_rect.y = s_y_0;
		splitreq.dst_rect.h = d_h_0;
		splitreq.dst_rect.y = d_y_0;
		splitreq.src_rect.x = s_x_0;
		splitreq.src_rect.w = s_w_0;
		splitreq.dst_rect.x = d_x_0;
		splitreq.dst_rect.w = d_w_0;
	} else {
		if (splitreq.flags & MDP_ROT_90) {
			s_x_0 = s_x_1 = req->src_rect.x;
			s_w_0 = s_w_1 = req->src_rect.w;
			s_y_0 = req->src_rect.y;
			s_h_1 = (req->src_rect.h * d_w_0) /
				req->dst_rect.w;
			s_h_0 = req->src_rect.h - s_h_1;
			s_y_1 = s_y_0 + s_h_0;
			if (d_w_0 >= 8 * s_h_1) {
				s_h_1++;
				s_y_1--;
			}
		} else {
			s_y_0 = s_y_1 = req->src_rect.y;
			s_h_0 = s_h_1 = req->src_rect.h;
			s_x_0 = req->src_rect.x;
			s_w_1 = (req->src_rect.w * d_w_0) /
				req->dst_rect.w;
			s_w_0 = req->src_rect.w - s_w_1;
			s_x_1 = s_x_0 + s_w_0;
			if (d_w_0 >= 8 * s_w_1) {
				s_w_1++;
				s_x_1--;
			}
		}
		splitreq.src_rect.h = s_h_0;
		splitreq.src_rect.y = s_y_0;
		splitreq.dst_rect.h = d_h_1;
		splitreq.dst_rect.y = d_y_1;
		splitreq.src_rect.x = s_x_0;
		splitreq.src_rect.w = s_w_0;
		splitreq.dst_rect.x = d_x_1;
		splitreq.dst_rect.w = d_w_1;
	}

	/* No need to split in height */
	ret = mdp3_ppp_blit(mfd, &splitreq, src_data, dst_data);

	if (ret)
		return ret;
	/* blit second region */
	if (((splitreq.flags & 0x07) == 0x07) ||
		((splitreq.flags & 0x07) == 0x05) ||
		((splitreq.flags & 0x07) == 0x02) ||
		((splitreq.flags & 0x07) == 0x0)) {
		splitreq.src_rect.h = s_h_1;
		splitreq.src_rect.y = s_y_1;
		splitreq.dst_rect.h = d_h_1;
		splitreq.dst_rect.y = d_y_1;
		splitreq.src_rect.x = s_x_1;
		splitreq.src_rect.w = s_w_1;
		splitreq.dst_rect.x = d_x_1;
		splitreq.dst_rect.w = d_w_1;
	} else {
		splitreq.src_rect.h = s_h_1;
		splitreq.src_rect.y = s_y_1;
		splitreq.dst_rect.h = d_h_0;
		splitreq.dst_rect.y = d_y_0;
		splitreq.src_rect.x = s_x_1;
		splitreq.src_rect.w = s_w_1;
		splitreq.dst_rect.x = d_x_0;
		splitreq.dst_rect.w = d_w_0;
	}

	/* No need to split in height ... just width */
	return mdp3_ppp_blit(mfd, &splitreq, src_data, dst_data);
}

int mdp3_ppp_start_blit(struct msm_fb_data_type *mfd,
		struct mdp_blit_req *req,
		struct mdp3_img_data *src_data,
		struct mdp3_img_data *dst_data)
{
	int ret;
	unsigned int remainder = 0, is_bpp_4 = 0;

	if (unlikely(req->src_rect.h == 0 || req->src_rect.w == 0)) {
		pr_err("mdp_ppp: src img of zero size!\n");
		return -EINVAL;
	}
	if (unlikely(req->dst_rect.h == 0 || req->dst_rect.w == 0))
		return 0;

	if (req->flags & MDP_ROT_90) {
		if (((req->dst_rect.h == 1) && ((req->src_rect.w != 1) ||
			(req->dst_rect.w != req->src_rect.h))) ||
			((req->dst_rect.w == 1) && ((req->src_rect.h != 1) ||
			(req->dst_rect.h != req->src_rect.w)))) {
			pr_err("mdp_ppp: error scaling when size is 1!\n");
			return -EINVAL;
		}
	} else {
		if (((req->dst_rect.w == 1) && ((req->src_rect.w != 1) ||
			(req->dst_rect.h != req->src_rect.h))) ||
			((req->dst_rect.h == 1) && ((req->src_rect.h != 1) ||
			(req->dst_rect.w != req->src_rect.w)))) {
			pr_err("mdp_ppp: error scaling when size is 1!\n");
			return -EINVAL;
		}
	}

	/* MDP width split workaround */
	remainder = (req->dst_rect.w) % 16;
	ret = ppp_get_bpp(req->dst.format, mfd->fb_imgType);
	if (ret <= 0) {
		pr_err("mdp_ppp: incorrect bpp!\n");
		return -EINVAL;
	}
	is_bpp_4 = (ret == 4) ? 1 : 0;

	if ((is_bpp_4 && (remainder == 6 || remainder == 14)))
		ret = mdp3_ppp_blit_workaround(mfd, req, remainder,
				src_data, dst_data);
	else
		ret = mdp3_ppp_blit(mfd, req, src_data, dst_data);
	return ret;
}

void mdp3_ppp_wait_for_fence(struct blit_req_list *req)
{
	int i, ret = 0;
	/* buf sync */
	for (i = 0; i < req->acq_fen_cnt; i++) {
		ret = sync_fence_wait(req->acq_fen[i],
				WAIT_FENCE_FINAL_TIMEOUT);
		if (ret < 0) {
			pr_err("%s: sync_fence_wait failed! ret = %x\n",
				__func__, ret);
			break;
		}
		sync_fence_put(req->acq_fen[i]);
	}

	if (ret < 0) {
		while (i < req->acq_fen_cnt) {
			sync_fence_put(req->acq_fen[i]);
			i++;
		}
	}
	req->acq_fen_cnt = 0;
}

void mdp3_ppp_signal_timeline(struct blit_req_list *req)
{
	sw_sync_timeline_inc(ppp_stat->timeline, 1);
	req->last_rel_fence = req->cur_rel_fence;
	req->cur_rel_fence = 0;
}


static void mdp3_ppp_deinit_buf_sync(struct blit_req_list *req)
{
	int i;

	put_unused_fd(req->cur_rel_fen_fd);
	sync_fence_put(req->cur_rel_fence);
	req->cur_rel_fence = NULL;
	req->cur_rel_fen_fd = 0;
	ppp_stat->timeline_value--;
	for (i = 0; i < req->acq_fen_cnt; i++)
		sync_fence_put(req->acq_fen[i]);
	req->acq_fen_cnt = 0;
}

static int mdp3_ppp_handle_buf_sync(struct blit_req_list *req,
	struct mdp_buf_sync *buf_sync)
{
	int i, fence_cnt = 0, ret = 0;
	int acq_fen_fd[MDP_MAX_FENCE_FD];
	struct sync_fence *fence;

	if ((buf_sync->acq_fen_fd_cnt > MDP_MAX_FENCE_FD) ||
		(ppp_stat->timeline == NULL))
		return -EINVAL;

	if (buf_sync->acq_fen_fd_cnt)
		ret = copy_from_user(acq_fen_fd, buf_sync->acq_fen_fd,
				buf_sync->acq_fen_fd_cnt * sizeof(int));
	if (ret) {
		pr_err("%s: copy_from_user failed\n", __func__);
		return ret;
	}
	for (i = 0; i < buf_sync->acq_fen_fd_cnt; i++) {
		fence = sync_fence_fdget(acq_fen_fd[i]);
		if (fence == NULL) {
			pr_info("%s: null fence! i=%d fd=%d\n", __func__, i,
				acq_fen_fd[i]);
			ret = -EINVAL;
			break;
		}
		req->acq_fen[i] = fence;
	}
	fence_cnt = i;
	if (ret)
		goto buf_sync_err_1;
	req->acq_fen_cnt = fence_cnt;
	if (buf_sync->flags & MDP_BUF_SYNC_FLAG_WAIT)
		mdp3_ppp_wait_for_fence(req);

	req->cur_rel_sync_pt = sw_sync_pt_create(ppp_stat->timeline,
			ppp_stat->timeline_value++);
	if (req->cur_rel_sync_pt == NULL) {
		pr_err("%s: cannot create sync point\n", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_2;
	}
	/* create fence */
	req->cur_rel_fence = sync_fence_create("ppp-fence",
			req->cur_rel_sync_pt);
	if (req->cur_rel_fence == NULL) {
		sync_pt_free(req->cur_rel_sync_pt);
		req->cur_rel_sync_pt = NULL;
		pr_err("%s: cannot create fence\n", __func__);
		ret = -ENOMEM;
		goto buf_sync_err_2;
	}
	/* create fd */
	return ret;
buf_sync_err_2:
	ppp_stat->timeline_value--;
buf_sync_err_1:
	for (i = 0; i < fence_cnt; i++)
		sync_fence_put(req->acq_fen[i]);
	req->acq_fen_cnt = 0;
	return ret;
}

void mdp3_ppp_req_push(struct blit_req_queue *req_q, struct blit_req_list *req)
{
	int idx = req_q->push_idx;
	req_q->req[idx] = *req;
	req_q->count++;
	req_q->push_idx = (req_q->push_idx + 1) % MDP3_PPP_MAX_LIST_REQ;
}

struct blit_req_list *mdp3_ppp_next_req(struct blit_req_queue *req_q)
{
	struct blit_req_list *req;
	if (req_q->count == 0)
		return NULL;
	req = &req_q->req[req_q->pop_idx];
	return req;
}

void mdp3_ppp_req_pop(struct blit_req_queue *req_q)
{
	req_q->count--;
	req_q->pop_idx = (req_q->pop_idx + 1) % MDP3_PPP_MAX_LIST_REQ;
}

void mdp3_free_fw_timer_func(unsigned long arg)
{
	schedule_work(&ppp_stat->free_bw_work);
}

static void mdp3_free_bw_wq_handler(struct work_struct *work)
{
	struct msm_fb_data_type *mfd = ppp_stat->mfd;
	mutex_lock(&ppp_stat->config_ppp_mutex);
	if (ppp_stat->bw_on) {
		mdp3_ppp_turnon(mfd, 0);
		mdp3_iommu_disable(MDP3_CLIENT_PPP);
	}
	mutex_unlock(&ppp_stat->config_ppp_mutex);
}

static void mdp3_ppp_blit_wq_handler(struct work_struct *work)
{
	struct msm_fb_data_type *mfd = ppp_stat->mfd;
	struct blit_req_list *req;
	int i, rc = 0;

	mutex_lock(&ppp_stat->config_ppp_mutex);
	req = mdp3_ppp_next_req(&ppp_stat->req_q);
	if (!req) {
		mutex_unlock(&ppp_stat->config_ppp_mutex);
		return;
	}

	if (!ppp_stat->bw_on) {
		mdp3_iommu_enable(MDP3_CLIENT_PPP);
		mdp3_ppp_turnon(mfd, 1);
	}
	while (req) {
		mdp3_ppp_wait_for_fence(req);
		for (i = 0; i < req->count; i++) {
			if (!(req->req_list[i].flags & MDP_NO_BLIT)) {
				/* Do the actual blit. */
				if (!rc) {
					rc = mdp3_ppp_start_blit(mfd,
						&(req->req_list[i]),
						&req->src_data[i],
						&req->dst_data[i]);
				}
				mdp3_put_img(&req->src_data[i]);
				mdp3_put_img(&req->dst_data[i]);
			}
		}
		/* Signal to release fence */
		mutex_lock(&ppp_stat->req_mutex);
		mdp3_ppp_signal_timeline(req);
		mdp3_ppp_req_pop(&ppp_stat->req_q);
		req = mdp3_ppp_next_req(&ppp_stat->req_q);
		if (ppp_stat->wait_for_pop)
			complete(&ppp_stat->pop_q_comp);
		mutex_unlock(&ppp_stat->req_mutex);
	}
	mod_timer(&ppp_stat->free_bw_timer, jiffies +
		msecs_to_jiffies(MDP_RELEASE_BW_TIMEOUT));
	mutex_unlock(&ppp_stat->config_ppp_mutex);
}

int mdp3_ppp_parse_req(void __user *p,
	struct mdp_async_blit_req_list *req_list_header,
	int async)
{
	struct blit_req_list *req;
	struct blit_req_queue *req_q = &ppp_stat->req_q;
	struct sync_fence *fence = NULL;
	int count, rc, idx, i;
	count = req_list_header->count;

	mutex_lock(&ppp_stat->req_mutex);
	while (req_q->count >= MDP3_PPP_MAX_LIST_REQ) {
		ppp_stat->wait_for_pop = true;
		mutex_unlock(&ppp_stat->req_mutex);
		rc = wait_for_completion_interruptible_timeout(
		   &ppp_stat->pop_q_comp, 5 * HZ);
		if (rc == 0) {
			/* This will only occur if there is serious problem */
			pr_err("%s: timeout exiting queuing request\n",
				   __func__);
			return -EBUSY;
		}
		mutex_lock(&ppp_stat->req_mutex);
		ppp_stat->wait_for_pop = false;
	}
	idx = req_q->push_idx;
	req = &req_q->req[idx];

	if (copy_from_user(&req->req_list, p,
			sizeof(struct mdp_blit_req) * count)) {
		mutex_unlock(&ppp_stat->req_mutex);
		return -EFAULT;
	}

	rc = mdp3_ppp_handle_buf_sync(req, &req_list_header->sync);
	if (rc < 0) {
		pr_err("%s: Failed create sync point\n", __func__);
		mutex_unlock(&ppp_stat->req_mutex);
		return rc;
	}
	req->count = count;

	/* We need to grab ion handle while running in client thread */
	for (i = 0; i < count; i++) {
		rc = mdp3_ppp_get_img(&req->req_list[i].src,
				&req->req_list[i], &req->src_data[i]);
		if (rc < 0 || req->src_data[i].len == 0) {
			pr_err("mdp_ppp: couldn't retrieve src img from mem\n");
			goto parse_err_1;
		}

		rc = mdp3_ppp_get_img(&req->req_list[i].dst,
				&req->req_list[i], &req->dst_data[i]);
		if (rc < 0 || req->dst_data[i].len == 0) {
			mdp3_put_img(&req->src_data[i]);
			pr_err("mdp_ppp: couldn't retrieve dest img from mem\n");
			goto parse_err_1;
		}
	}

	if (async) {
		req->cur_rel_fen_fd = get_unused_fd_flags(0);
		if (req->cur_rel_fen_fd < 0) {
			pr_err("%s: get_unused_fd_flags failed\n", __func__);
			rc  = -ENOMEM;
			goto parse_err_1;
		}
		sync_fence_install(req->cur_rel_fence, req->cur_rel_fen_fd);
		rc = copy_to_user(req_list_header->sync.rel_fen_fd,
			&req->cur_rel_fen_fd, sizeof(int));
		if (rc) {
			pr_err("%s:copy_to_user failed\n", __func__);
			goto parse_err_2;
		}
	} else {
		fence = req->cur_rel_fence;
	}

	mdp3_ppp_req_push(req_q, req);
	mutex_unlock(&ppp_stat->req_mutex);
	schedule_work(&ppp_stat->blit_work);
	if (!async) {
		/* wait for release fence */
		rc = sync_fence_wait(fence,
				5 * MSEC_PER_SEC);
		if (rc < 0)
			pr_err("%s: sync blit! rc = %x\n", __func__, rc);

		sync_fence_put(fence);
		fence = NULL;
	}
	return 0;

parse_err_2:
	put_unused_fd(req->cur_rel_fen_fd);
parse_err_1:
	for (i--; i >= 0; i--) {
		mdp3_put_img(&req->src_data[i]);
		mdp3_put_img(&req->dst_data[i]);
	}
	mdp3_ppp_deinit_buf_sync(req);
	mutex_unlock(&ppp_stat->req_mutex);
	return rc;
}

int mdp3_ppp_res_init(struct msm_fb_data_type *mfd)
{
	const char timeline_name[] = "mdp3_ppp";
	ppp_stat = kzalloc(sizeof(struct ppp_status), GFP_KERNEL);
	if (!ppp_stat) {
		pr_err("%s: kmalloc failed\n", __func__);
		return -ENOMEM;
	}

	/*Setup sync_pt timeline for ppp*/
	ppp_stat->timeline = sw_sync_timeline_create(timeline_name);
	if (ppp_stat->timeline == NULL) {
		pr_err("%s: cannot create time line\n", __func__);
		return -ENOMEM;
	} else {
		ppp_stat->timeline_value = 1;
	}

	INIT_WORK(&ppp_stat->blit_work, mdp3_ppp_blit_wq_handler);
	INIT_WORK(&ppp_stat->free_bw_work, mdp3_free_bw_wq_handler);
	init_completion(&ppp_stat->pop_q_comp);
	spin_lock_init(&ppp_stat->ppp_lock);
	mutex_init(&ppp_stat->req_mutex);
	mutex_init(&ppp_stat->config_ppp_mutex);
	init_timer(&ppp_stat->free_bw_timer);
	ppp_stat->free_bw_timer.function = mdp3_free_fw_timer_func;
	ppp_stat->free_bw_timer.data = 0;
	ppp_stat->busy = false;
	ppp_stat->mfd = mfd;
	mdp3_ppp_callback_setup();
	return 0;
}
