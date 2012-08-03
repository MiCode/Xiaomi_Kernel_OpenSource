/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/bitmap.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#include "mdss_mdp.h"

#define SMP_MB_CNT (mdss_res->smp_mb_cnt)

static DEFINE_MUTEX(mdss_mdp_sspp_lock);
static DECLARE_BITMAP(mdss_mdp_smp_mmb_pool, MDSS_MDP_SMP_MMB_BLOCKS);

static struct mdss_mdp_pipe mdss_mdp_pipe_list[MDSS_MDP_MAX_SSPP];

static u32 mdss_mdp_smp_mmb_reserve(unsigned long *smp, size_t n)
{
	u32 i, mmb;

	/* reserve more blocks if needed, but can't free mmb at this point */
	for (i = bitmap_weight(smp, SMP_MB_CNT); i < n; i++) {
		if (bitmap_full(mdss_mdp_smp_mmb_pool, SMP_MB_CNT))
			break;

		mmb = find_first_zero_bit(mdss_mdp_smp_mmb_pool, SMP_MB_CNT);
		set_bit(mmb, smp);
		set_bit(mmb, mdss_mdp_smp_mmb_pool);
	}
	return i;
}

static void mdss_mdp_smp_mmb_set(int client_id, unsigned long *smp)
{
	u32 mmb, off, data, s;

	for_each_set_bit(mmb, smp, SMP_MB_CNT) {
		off = (mmb / 3) * 4;
		s = (mmb % 3) * 8;
		data = MDSS_MDP_REG_READ(MDSS_MDP_REG_SMP_ALLOC_W0 + off);
		data &= ~(0xFF << s);
		data |= client_id << s;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_SMP_ALLOC_W0 + off, data);
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_SMP_ALLOC_R0 + off, data);
	}
}

static void mdss_mdp_smp_mmb_free(unsigned long *smp)
{
	if (!bitmap_empty(smp, SMP_MB_CNT)) {
		mdss_mdp_smp_mmb_set(MDSS_MDP_SMP_CLIENT_UNUSED, smp);
		bitmap_andnot(mdss_mdp_smp_mmb_pool, mdss_mdp_smp_mmb_pool,
			      smp, SMP_MB_CNT);
		bitmap_zero(smp, SMP_MB_CNT);
	}
}

static void mdss_mdp_smp_free(struct mdss_mdp_pipe *pipe)
{
	mdss_mdp_smp_mmb_free(&pipe->smp[0]);
	mdss_mdp_smp_mmb_free(&pipe->smp[1]);
	mdss_mdp_smp_mmb_free(&pipe->smp[2]);
}

static int mdss_mdp_smp_reserve(struct mdss_mdp_pipe *pipe)
{
	u32 num_blks = 0, reserved = 0;
	int i;

	if ((pipe->src_planes.num_planes > 1) &&
	    (pipe->type == MDSS_MDP_PIPE_TYPE_RGB))
		return -EINVAL;

	mutex_lock(&mdss_mdp_sspp_lock);
	for (i = 0; i < pipe->src_planes.num_planes; i++) {
		num_blks = DIV_ROUND_UP(2 * pipe->src_planes.ystride[i],
					mdss_res->smp_mb_size);

		pr_debug("reserving %d mmb for pnum=%d plane=%d\n",
				num_blks, pipe->num, i);
		reserved = mdss_mdp_smp_mmb_reserve(&pipe->smp[i], num_blks);

		if (reserved < num_blks)
			break;
	}

	if (reserved < num_blks) {
		pr_err("insufficient MMB blocks\n");
		mdss_mdp_smp_free(pipe);
		return -ENOMEM;
	}
	mutex_unlock(&mdss_mdp_sspp_lock);

	return 0;
}

static int mdss_mdp_smp_alloc(struct mdss_mdp_pipe *pipe)
{
	u32 client_id;
	int i;

	switch (pipe->num) {
	case MDSS_MDP_SSPP_VIG0:
		client_id = MDSS_MDP_SMP_CLIENT_VIG0_FETCH_Y;
		break;
	case MDSS_MDP_SSPP_VIG1:
		client_id = MDSS_MDP_SMP_CLIENT_VIG1_FETCH_Y;
		break;
	case MDSS_MDP_SSPP_VIG2:
		client_id = MDSS_MDP_SMP_CLIENT_VIG2_FETCH_Y;
		break;
	case MDSS_MDP_SSPP_RGB0:
		client_id = MDSS_MDP_SMP_CLIENT_RGB0_FETCH;
		break;
	case MDSS_MDP_SSPP_RGB1:
		client_id = MDSS_MDP_SMP_CLIENT_RGB1_FETCH;
		break;
	case MDSS_MDP_SSPP_RGB2:
		client_id = MDSS_MDP_SMP_CLIENT_RGB2_FETCH;
		break;
	case MDSS_MDP_SSPP_DMA0:
		client_id = MDSS_MDP_SMP_CLIENT_DMA0_FETCH_Y;
		break;
	case MDSS_MDP_SSPP_DMA1:
		client_id = MDSS_MDP_SMP_CLIENT_DMA1_FETCH_Y;
		break;
	default:
		pr_err("no valid smp client for pnum=%d\n", pipe->num);
		return -EINVAL;
	}

	mutex_lock(&mdss_mdp_sspp_lock);
	for (i = 0; i < pipe->src_planes.num_planes; i++)
		mdss_mdp_smp_mmb_set(client_id + i, &pipe->smp[i]);
	mutex_unlock(&mdss_mdp_sspp_lock);
	return 0;
}

void mdss_mdp_pipe_unlock(struct mdss_mdp_pipe *pipe)
{
	atomic_dec(&pipe->ref_cnt);
	mutex_unlock(&pipe->lock);
}

int mdss_mdp_pipe_lock(struct mdss_mdp_pipe *pipe)
{
	if (atomic_inc_not_zero(&pipe->ref_cnt)) {
		if (mutex_lock_interruptible(&pipe->lock)) {
			atomic_dec(&pipe->ref_cnt);
			return -EINTR;
		}
		return 0;
	}
	return -EINVAL;
}

static struct mdss_mdp_pipe *mdss_mdp_pipe_init(u32 pnum)
{
	struct mdss_mdp_pipe *pipe = NULL;

	if (atomic_read(&mdss_mdp_pipe_list[pnum].ref_cnt) == 0) {
		pipe = &mdss_mdp_pipe_list[pnum];
		memset(pipe, 0, sizeof(*pipe));

		mutex_init(&pipe->lock);
		atomic_set(&pipe->ref_cnt, 1);

		if (mdss_mdp_pipe_lock(pipe) == 0) {
			pipe->num = pnum;
			pipe->type = mdss_res->pipe_type_map[pnum];
			pipe->ndx = BIT(pnum);

			pr_debug("ndx=%x pnum=%d\n", pipe->ndx, pipe->num);
		} else {
			atomic_set(&pipe->ref_cnt, 0);
			pipe = NULL;
		}
	}
	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_alloc_pnum(u32 pnum)
{
	struct mdss_mdp_pipe *pipe = NULL;
	mutex_lock(&mdss_mdp_sspp_lock);
	if (mdss_res->pipe_type_map[pnum] != MDSS_MDP_PIPE_TYPE_UNUSED)
		pipe = mdss_mdp_pipe_init(pnum);
	mutex_unlock(&mdss_mdp_sspp_lock);
	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_alloc_locked(u32 type)
{
	struct mdss_mdp_pipe *pipe = NULL;
	int pnum;

	mutex_lock(&mdss_mdp_sspp_lock);
	for (pnum = 0; pnum < MDSS_MDP_MAX_SSPP; pnum++) {
		if (type == mdss_res->pipe_type_map[pnum]) {
			pipe = mdss_mdp_pipe_init(pnum);
			if (pipe)
				break;
		}
	}
	mutex_unlock(&mdss_mdp_sspp_lock);

	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_get_locked(u32 ndx)
{
	struct mdss_mdp_pipe *pipe = NULL;
	int i;

	if (!ndx)
		return NULL;

	mutex_lock(&mdss_mdp_sspp_lock);
	for (i = 0; i < MDSS_MDP_MAX_SSPP; i++) {
		pipe = &mdss_mdp_pipe_list[i];
		if (ndx == pipe->ndx)
			break;
	}
	mutex_unlock(&mdss_mdp_sspp_lock);

	if (i == MDSS_MDP_MAX_SSPP)
		return NULL;

	if (mdss_mdp_pipe_lock(pipe))
		return NULL;

	if (pipe->ndx != ndx) {
		mdss_mdp_pipe_unlock(pipe);
		pipe = NULL;
	}

	return pipe;
}


static void mdss_mdp_pipe_free(struct mdss_mdp_pipe *pipe)
{
	mdss_mdp_smp_free(pipe);
	pipe->ndx = 0;
	atomic_dec(&pipe->ref_cnt);
	mdss_mdp_pipe_unlock(pipe);
}

int mdss_mdp_pipe_destroy(struct mdss_mdp_pipe *pipe)
{
	pr_debug("ndx=%x pnum=%d ref_cnt=%d\n", pipe->ndx, pipe->num,
			atomic_read(&pipe->ref_cnt));

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	mutex_lock(&mdss_mdp_sspp_lock);
	mdss_mdp_pipe_free(pipe);
	mutex_unlock(&mdss_mdp_sspp_lock);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	return 0;
}

static inline void mdss_mdp_pipe_write(struct mdss_mdp_pipe *pipe,
				       u32 reg, u32 val)
{
	int offset = MDSS_MDP_REG_SSPP_OFFSET(pipe->num);
	MDSS_MDP_REG_WRITE(offset + reg, val);
}

static inline u32 mdss_mdp_pipe_read(struct mdss_mdp_pipe *pipe, u32 reg)
{
	int offset = MDSS_MDP_REG_SSPP_OFFSET(pipe->num);
	return MDSS_MDP_REG_READ(offset + reg);
}

static int mdss_mdp_leading_zero(u32 num)
{
	u32 bit = 0x80000000;
	int i;

	for (i = 0; i < 32; i++) {
		if (bit & num)
			return i;
		bit >>= 1;
	}

	return i;
}

static u32 mdss_mdp_scale_phase_step(int f_num, u32 src, u32 dst)
{
	u32 val, s;
	int n;

	n = mdss_mdp_leading_zero(src);
	if (n > f_num)
		n = f_num;
	s = src << n;	/* maximum to reduce lose of resolution */
	val = s / dst;
	if (n < f_num) {
		n = f_num - n;
		val <<= n;
		val |= ((s % dst) << n) / dst;
	}

	return val;
}

static int mdss_mdp_scale_setup(struct mdss_mdp_pipe *pipe)
{
	u32 scale_config = 0;
	u32 phasex_step = 0, phasey_step = 0;
	u32 chroma_sample;

	if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA) {
		if (pipe->dst.h != pipe->src.h || pipe->dst.w != pipe->src.w) {
			pr_err("no scaling supported on dma pipe\n");
			return -EINVAL;
		} else {
			return 0;
		}
	}

	chroma_sample = pipe->src_fmt->chroma_sample;
	if (pipe->flags & MDP_SOURCE_ROTATED_90) {
		if (chroma_sample == MDSS_MDP_CHROMA_H1V2)
			chroma_sample = MDSS_MDP_CHROMA_H2V1;
		else if (chroma_sample == MDSS_MDP_CHROMA_H2V1)
			chroma_sample = MDSS_MDP_CHROMA_H1V2;
	}

	if ((pipe->src.h != pipe->dst.h) ||
	    (chroma_sample == MDSS_MDP_CHROMA_420) ||
	    (chroma_sample == MDSS_MDP_CHROMA_H1V2)) {
		pr_debug("scale y - src_h=%d dst_h=%d\n",
				pipe->src.h, pipe->dst.h);

		if ((pipe->src.h / MAX_DOWNSCALE_RATIO) > pipe->dst.h) {
			pr_err("too much downscaling height=%d->%d",
			       pipe->src.h, pipe->dst.h);
			return -EINVAL;
		}

		scale_config |= MDSS_MDP_SCALEY_EN;

		if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG) {
			u32 chr_dst_h = pipe->dst.h;
			if ((chroma_sample == MDSS_MDP_CHROMA_420) ||
			    (chroma_sample == MDSS_MDP_CHROMA_H1V2))
				chr_dst_h *= 2;	/* 2x upsample chroma */

			if (pipe->src.h <= pipe->dst.h)
				scale_config |= /* G/Y, A */
					(MDSS_MDP_SCALE_FILTER_BIL << 10) |
					(MDSS_MDP_SCALE_FILTER_NEAREST << 18);
			else
				scale_config |= /* G/Y, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 10) |
					(MDSS_MDP_SCALE_FILTER_PCMN << 18);

			if (pipe->src.h <= chr_dst_h)
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_BIL << 14);
			else
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_PCMN << 14);

			phasey_step = mdss_mdp_scale_phase_step(
				PHASE_STEP_SHIFT, pipe->src.h, chr_dst_h);

			mdss_mdp_pipe_write(pipe,
					MDSS_MDP_REG_VIG_QSEED2_C12_PHASESTEPY,
					phasey_step);
		} else {
			if (pipe->src.h <= pipe->dst.h)
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_BIL << 10) |
					(MDSS_MDP_SCALE_FILTER_NEAREST << 18);
			else
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 10) |
					(MDSS_MDP_SCALE_FILTER_NEAREST << 18);
		}

		phasey_step = mdss_mdp_scale_phase_step(
			PHASE_STEP_SHIFT, pipe->src.h, pipe->dst.h);
	}

	if ((pipe->src.w != pipe->dst.w) ||
	    (chroma_sample == MDSS_MDP_CHROMA_420) ||
	    (chroma_sample == MDSS_MDP_CHROMA_H2V1)) {
		pr_debug("scale x - src_w=%d dst_w=%d\n",
				pipe->src.w, pipe->dst.w);

		if ((pipe->src.w / MAX_DOWNSCALE_RATIO) > pipe->dst.w) {
			pr_err("too much downscaling width=%d->%d",
			       pipe->src.w, pipe->dst.w);
			return -EINVAL;
		}

		scale_config |= MDSS_MDP_SCALEX_EN;

		if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG) {
			u32 chr_dst_w = pipe->dst.w;

			if ((chroma_sample == MDSS_MDP_CHROMA_420) ||
			    (chroma_sample == MDSS_MDP_CHROMA_H2V1))
				chr_dst_w *= 2;	/* 2x upsample chroma */

			if (pipe->src.w <= pipe->dst.w)
				scale_config |= /* G/Y, A */
					(MDSS_MDP_SCALE_FILTER_BIL << 8) |
					(MDSS_MDP_SCALE_FILTER_NEAREST << 16);
			else
				scale_config |= /* G/Y, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 8) |
					(MDSS_MDP_SCALE_FILTER_PCMN << 16);

			if (pipe->src.w <= chr_dst_w)
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_BIL << 12);
			else
				scale_config |= /* CrCb */
					(MDSS_MDP_SCALE_FILTER_PCMN << 12);

			phasex_step = mdss_mdp_scale_phase_step(
				PHASE_STEP_SHIFT, pipe->src.w, chr_dst_w);
			mdss_mdp_pipe_write(pipe,
					MDSS_MDP_REG_VIG_QSEED2_C12_PHASESTEPX,
					phasex_step);
		} else {
			if (pipe->src.w <= pipe->dst.w)
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_BIL << 8) |
					(MDSS_MDP_SCALE_FILTER_NEAREST << 16);
			else
				scale_config |= /* RGB, A */
					(MDSS_MDP_SCALE_FILTER_PCMN << 8) |
					(MDSS_MDP_SCALE_FILTER_NEAREST << 16);
		}

		phasex_step = mdss_mdp_scale_phase_step(
			PHASE_STEP_SHIFT, pipe->src.w, pipe->dst.w);
	}

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SCALE_CONFIG, scale_config);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SCALE_PHASE_STEP_X, phasex_step);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SCALE_PHASE_STEP_Y, phasey_step);
	return 0;
}

static int mdss_mdp_image_setup(struct mdss_mdp_pipe *pipe)
{
	u32 img_size, src_size, src_xy, dst_size, dst_xy, ystride0, ystride1;

	pr_debug("pnum=%d wh=%dx%d src={%d,%d,%d,%d} dst={%d,%d,%d,%d}\n",
		   pipe->num, pipe->img_width, pipe->img_height,
		   pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
		   pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	if (mdss_mdp_scale_setup(pipe))
		return -EINVAL;

	mdss_mdp_get_plane_sizes(pipe->src_fmt->format, pipe->img_width,
				 pipe->img_height, &pipe->src_planes);

	img_size = (pipe->img_height << 16) | pipe->img_width;
	src_size = (pipe->src.h << 16) | pipe->src.w;
	src_xy = (pipe->src.y << 16) | pipe->src.x;
	dst_size = (pipe->dst.h << 16) | pipe->dst.w;
	dst_xy = (pipe->dst.y << 16) | pipe->dst.x;
	ystride0 =  (pipe->src_planes.ystride[0]) |
		    (pipe->src_planes.ystride[1] << 16);
	ystride1 =  (pipe->src_planes.ystride[2]) |
		    (pipe->src_planes.ystride[3] << 16);

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_IMG_SIZE, img_size);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_SIZE, src_size);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_XY, src_xy);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_SIZE, dst_size);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_XY, dst_xy);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_YSTRIDE0, ystride0);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_YSTRIDE1, ystride1);

	return 0;
}

static int mdss_mdp_format_setup(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_format_params *fmt;
	u32 opmode, chroma_samp, unpack, src_format;

	fmt = pipe->src_fmt;

	opmode = pipe->bwc_mode;
	if (pipe->flags & MDP_FLIP_LR)
		opmode |= MDSS_MDP_OP_FLIP_LR;
	if (pipe->flags & MDP_FLIP_UD)
		opmode |= MDSS_MDP_OP_FLIP_UD;

	pr_debug("pnum=%d format=%d opmode=%x\n", pipe->num, fmt->format,
			opmode);

	chroma_samp = fmt->chroma_sample;
	if (pipe->flags & MDP_SOURCE_ROTATED_90) {
		if (chroma_samp == MDSS_MDP_CHROMA_H2V1)
			chroma_samp = MDSS_MDP_CHROMA_H1V2;
		else if (chroma_samp == MDSS_MDP_CHROMA_H1V2)
			chroma_samp = MDSS_MDP_CHROMA_H2V1;
	}

	src_format = (chroma_samp << 23) |
		     (fmt->fetch_planes << 19) |
		     (fmt->bits[C3_ALPHA] << 6) |
		     (fmt->bits[C2_R_Cr] << 4) |
		     (fmt->bits[C1_B_Cb] << 2) |
		     (fmt->bits[C0_G_Y] << 0);

	if (pipe->flags & MDP_ROT_90)
		src_format |= BIT(11); /* ROT90 */

	if (fmt->alpha_enable &&
			fmt->fetch_planes != MDSS_MDP_PLANE_INTERLEAVED)
		src_format |= BIT(8); /* SRCC3_EN */

	if (fmt->fetch_planes != MDSS_MDP_PLANE_PLANAR) {
		unpack = (fmt->element[3] << 24) | (fmt->element[2] << 16) |
			(fmt->element[1] << 8) | (fmt->element[0] << 0);

		src_format |= ((fmt->unpack_count - 1) << 12) |
			  (fmt->unpack_tight << 17) |
			  (fmt->unpack_align_msb << 18) |
			  ((fmt->bpp - 1) << 9);
	} else {
		unpack = 0;
	}

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_FORMAT, src_format);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_UNPACK_PATTERN, unpack);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_OP_MODE, opmode);

	return 0;
}

static int mdss_mdp_vig_setup(struct mdss_mdp_pipe *pipe)
{
	u32 opmode = 0;

	pr_debug("pnum=%x\n", pipe->num);

	if (pipe->src_fmt->is_yuv)
		opmode |= (0 << 19) |	/* DST_DATA=RGB */
			  (1 << 18) |	/* SRC_DATA=YCBCR */
			  (1 << 17);	/* CSC_1_EN */

	/* only need to program once */
	if (pipe->play_cnt == 0) {
		mdss_mdp_csc_setup(MDSS_MDP_BLOCK_SSPP, pipe->num, 1,
				   MDSS_MDP_CSC_YUV2RGB);
	}
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_VIG_OP_MODE, opmode);

	return 0;
}

static int mdss_mdp_src_addr_setup(struct mdss_mdp_pipe *pipe,
				   struct mdss_mdp_data *data)
{
	int is_rot = pipe->mixer->rotator_mode;
	int ret = 0;

	pr_debug("pnum=%d\n", pipe->num);

	if (!is_rot)
		data->bwc_enabled = pipe->bwc_mode;

	ret = mdss_mdp_data_check(data, &pipe->src_planes);
	if (ret)
		return ret;

	/* planar format expects YCbCr, swap chroma planes if YCrCb */
	if (!is_rot && (pipe->src_fmt->fetch_planes == MDSS_MDP_PLANE_PLANAR) &&
	    (pipe->src_fmt->element[0] == C2_R_Cr))
		swap(data->p[1].addr, data->p[2].addr);

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC0_ADDR, data->p[0].addr);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC1_ADDR, data->p[1].addr);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC2_ADDR, data->p[2].addr);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC3_ADDR, data->p[3].addr);

	return 0;
}

int mdss_mdp_pipe_queue_data(struct mdss_mdp_pipe *pipe,
			     struct mdss_mdp_data *src_data)
{
	int ret = 0;
	u32 params_changed;

	if (!pipe) {
		pr_err("pipe not setup properly for queue\n");
		return -ENODEV;
	}

	if (!pipe->mixer) {
		pr_err("pipe mixer not setup properly for queue\n");
		return -ENODEV;
	}

	pr_debug("pnum=%x mixer=%d play_cnt=%u\n", pipe->num,
		 pipe->mixer->num, pipe->play_cnt);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	params_changed = pipe->params_changed;
	if (params_changed) {
		pipe->params_changed = 0;

		ret = mdss_mdp_image_setup(pipe);
		if (ret) {
			pr_err("image setup error for pnum=%d\n", pipe->num);
			goto done;
		}

		ret = mdss_mdp_format_setup(pipe);
		if (ret) {
			pr_err("format %d setup error pnum=%d\n",
			       pipe->src_fmt->format, pipe->num);
			goto done;
		}

		if (pipe->type == MDSS_MDP_PIPE_TYPE_VIG)
			mdss_mdp_vig_setup(pipe);

		ret = mdss_mdp_smp_reserve(pipe);
		if (ret) {
			pr_err("unable to reserve smp for pnum=%d\n",
			       pipe->num);
			goto done;
		}

		mdss_mdp_smp_alloc(pipe);
	}

	ret = mdss_mdp_src_addr_setup(pipe, src_data);
	if (ret) {
		pr_err("addr setup error for pnum=%d\n", pipe->num);
		goto done;
	}

	mdss_mdp_mixer_pipe_update(pipe, params_changed);

	pipe->play_cnt++;

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	return ret;
}
