/* Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/iopoll.h>
#include <linux/mutex.h>

#include "sde_rotator_r1_hwio.h"
#include "sde_rotator_base.h"
#include "sde_rotator_util.h"
#include "sde_rotator_r1_internal.h"
#include "sde_rotator_core.h"
#include "sde_rotator_trace.h"

#define SMP_MB_SIZE		(mdss_res->smp_mb_size)
#define SMP_MB_CNT		(mdss_res->smp_mb_cnt)
#define SMP_MB_ENTRY_SIZE	16
#define MAX_BPP 4

#define PIPE_CLEANUP_TIMEOUT_US 100000

/* following offsets are relative to ctrl register bit offset */
#define CLK_FORCE_ON_OFFSET	0x0
#define CLK_FORCE_OFF_OFFSET	0x1
/* following offsets are relative to status register bit offset */
#define CLK_STATUS_OFFSET	0x0

#define QOS_LUT_NRT_READ	0x0
#define PANIC_LUT_NRT_READ	0x0
#define ROBUST_LUT_NRT_READ	0xFFFF

/* Priority 2, no panic */
#define VBLANK_PANIC_DEFAULT_CONFIG 0x200000

static inline void sde_mdp_pipe_write(struct sde_mdp_pipe *pipe,
				       u32 reg, u32 val)
{
	SDEROT_DBG("pipe%d:%6.6x:%8.8x\n", pipe->num, pipe->offset + reg, val);
	writel_relaxed(val, pipe->base + reg);
}

static int sde_mdp_pipe_qos_lut(struct sde_mdp_pipe *pipe)
{
	u32 qos_lut;

	qos_lut = QOS_LUT_NRT_READ; /* low priority for nrt */

	trace_rot_perf_set_qos_luts(pipe->num, pipe->src_fmt->format,
		qos_lut, sde_mdp_is_linear_format(pipe->src_fmt));

	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_CREQ_LUT,
		qos_lut);

	return 0;
}

/**
 * @sde_mdp_pipe_nrt_vbif_setup -
 * @mdata: pointer to global driver data.
 * @pipe: pointer to a pipe
 *
 * This function assumes that clocks are enabled, so it is callers
 * responsibility to enable clocks before calling this function.
 */
static void sde_mdp_pipe_nrt_vbif_setup(struct sde_rot_data_type *mdata,
					struct sde_mdp_pipe *pipe)
{
	uint32_t nrt_vbif_client_sel;

	if (pipe->type != SDE_MDP_PIPE_TYPE_DMA)
		return;

	nrt_vbif_client_sel = readl_relaxed(mdata->mdp_base +
				MMSS_MDP_RT_NRT_VBIF_CLIENT_SEL);
	if (sde_mdp_is_nrt_vbif_client(mdata, pipe))
		nrt_vbif_client_sel |= BIT(pipe->num - SDE_MDP_SSPP_DMA0);
	else
		nrt_vbif_client_sel &= ~BIT(pipe->num - SDE_MDP_SSPP_DMA0);
	SDEROT_DBG("mdp:%6.6x:%8.8x\n", MMSS_MDP_RT_NRT_VBIF_CLIENT_SEL,
			nrt_vbif_client_sel);
	writel_relaxed(nrt_vbif_client_sel,
			mdata->mdp_base + MMSS_MDP_RT_NRT_VBIF_CLIENT_SEL);
}

/**
 * sde_mdp_qos_vbif_remapper_setup - Program the VBIF QoS remapper
 *		registers based on real or non real time clients
 * @mdata:	Pointer to the global mdss data structure.
 * @pipe:	Pointer to source pipe struct to get xin id's.
 * @is_realtime:	To determine if pipe's client is real or
 *			non real time.
 * This function assumes that clocks are on, so it is caller responsibility to
 * call this function with clocks enabled.
 */
static void sde_mdp_qos_vbif_remapper_setup(struct sde_rot_data_type *mdata,
			struct sde_mdp_pipe *pipe, bool is_realtime)
{
	u32 mask, reg_val, i, vbif_qos;

	if (mdata->npriority_lvl == 0)
		return;

	for (i = 0; i < mdata->npriority_lvl; i++) {
		reg_val = SDE_VBIF_READ(mdata, SDE_VBIF_QOS_REMAP_BASE + i*4);
		mask = 0x3 << (pipe->xin_id * 2);
		reg_val &= ~(mask);
		vbif_qos = is_realtime ?
			mdata->vbif_rt_qos[i] : mdata->vbif_nrt_qos[i];
		reg_val |= vbif_qos << (pipe->xin_id * 2);
		SDE_VBIF_WRITE(mdata, SDE_VBIF_QOS_REMAP_BASE + i*4, reg_val);
	}
}

struct sde_mdp_pipe *sde_mdp_pipe_assign(struct sde_rot_data_type *mdata,
	struct sde_mdp_mixer *mixer, u32 ndx)
{
	struct sde_mdp_pipe *pipe = NULL;
	static struct sde_mdp_pipe sde_pipe[16];
	static const u32 offset[] = {0x00025000, 0x00027000};
	static const u32 xin_id[] = {2, 10};
	static const struct sde_mdp_shared_reg_ctrl clk_ctrl[] = {
		{0x2AC, 8},
		{0x2B4, 8}
	};

	if (ndx >= ARRAY_SIZE(offset)) {
		SDEROT_ERR("invalid parameters\n");
		return ERR_PTR(-EINVAL);
	}

	pipe = &sde_pipe[ndx];
	pipe->num = ndx + SDE_MDP_SSPP_DMA0;
	pipe->offset = offset[pipe->num - SDE_MDP_SSPP_DMA0];
	pipe->xin_id = xin_id[pipe->num - SDE_MDP_SSPP_DMA0];
	pipe->base = mdata->sde_io.base + pipe->offset;
	pipe->type = SDE_MDP_PIPE_TYPE_DMA;
	pipe->mixer_left = mixer;
	pipe->clk_ctrl = clk_ctrl[pipe->num - SDE_MDP_SSPP_DMA0];

	return pipe;
}

int sde_mdp_pipe_destroy(struct sde_mdp_pipe *pipe)
{
	return 0;
}

void sde_mdp_pipe_position_update(struct sde_mdp_pipe *pipe,
		struct sde_rect *src, struct sde_rect *dst)
{
	u32 src_size, src_xy, dst_size, dst_xy;

	src_size = (src->h << 16) | src->w;
	src_xy = (src->y << 16) | src->x;
	dst_size = (dst->h << 16) | dst->w;
	dst_xy = (dst->y << 16) | dst->x;

	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_SIZE, src_size);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_XY, src_xy);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_OUT_SIZE, dst_size);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_OUT_XY, dst_xy);
}

static int sde_mdp_image_setup(struct sde_mdp_pipe *pipe,
					struct sde_mdp_data *data)
{
	u32 img_size, ystride0, ystride1;
	u32 width, height, decimation;
	int ret = 0;
	struct sde_rect dst, src;
	bool rotation = false;

	SDEROT_DBG(
		"ctl: %d pnum=%d wh=%dx%d src={%d,%d,%d,%d} dst={%d,%d,%d,%d}\n",
			pipe->mixer_left->ctl->num, pipe->num,
			pipe->img_width, pipe->img_height,
			pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
			pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	width = pipe->img_width;
	height = pipe->img_height;

	if (pipe->flags & SDE_SOURCE_ROTATED_90)
		rotation = true;

	sde_mdp_get_plane_sizes(pipe->src_fmt, width, height,
			&pipe->src_planes, pipe->bwc_mode, rotation);

	if (data != NULL) {
		ret = sde_mdp_data_check(data, &pipe->src_planes,
			pipe->src_fmt);
		if (ret)
			return ret;
	}

	if ((pipe->flags & SDE_DEINTERLACE) &&
			!(pipe->flags & SDE_SOURCE_ROTATED_90)) {
		int i;

		for (i = 0; i < pipe->src_planes.num_planes; i++)
			pipe->src_planes.ystride[i] *= 2;
		width *= 2;
		height /= 2;
	}

	decimation = ((1 << pipe->horz_deci) - 1) << 8;
	decimation |= ((1 << pipe->vert_deci) - 1);
	if (decimation)
		SDEROT_DBG("Image decimation h=%d v=%d\n",
				pipe->horz_deci, pipe->vert_deci);

	dst = pipe->dst;
	src = pipe->src;

	ystride0 =  (pipe->src_planes.ystride[0]) |
			(pipe->src_planes.ystride[1] << 16);
	ystride1 =  (pipe->src_planes.ystride[2]) |
			(pipe->src_planes.ystride[3] << 16);

	img_size = (height << 16) | width;

	sde_mdp_pipe_position_update(pipe, &src, &dst);

	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_IMG_SIZE, img_size);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_YSTRIDE0, ystride0);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_YSTRIDE1, ystride1);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_DECIMATION_CONFIG,
			decimation);

	return 0;
}

static int sde_mdp_format_setup(struct sde_mdp_pipe *pipe)
{
	struct sde_mdp_format_params *fmt;
	u32 chroma_samp, unpack, src_format;
	u32 secure = 0;
	u32 opmode;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();

	fmt = pipe->src_fmt;

	if (pipe->flags & SDE_SECURE_OVERLAY_SESSION)
		secure = 0xF;

	opmode = pipe->bwc_mode;
	if (pipe->flags & SDE_FLIP_LR)
		opmode |= SDE_MDP_OP_FLIP_LR;
	if (pipe->flags & SDE_FLIP_UD)
		opmode |= SDE_MDP_OP_FLIP_UD;

	SDEROT_DBG("pnum=%d format=%d opmode=%x\n", pipe->num, fmt->format,
			opmode);

	chroma_samp = fmt->chroma_sample;
	if (pipe->flags & SDE_SOURCE_ROTATED_90) {
		if (chroma_samp == SDE_MDP_CHROMA_H2V1)
			chroma_samp = SDE_MDP_CHROMA_H1V2;
		else if (chroma_samp == SDE_MDP_CHROMA_H1V2)
			chroma_samp = SDE_MDP_CHROMA_H2V1;
	}

	src_format = (chroma_samp << 23) |
		     (fmt->fetch_planes << 19) |
		     (fmt->bits[C3_ALPHA] << 6) |
		     (fmt->bits[C2_R_Cr] << 4) |
		     (fmt->bits[C1_B_Cb] << 2) |
		     (fmt->bits[C0_G_Y] << 0);

	if (sde_mdp_is_tilea4x_format(fmt))
		src_format |= BIT(30);

	if (sde_mdp_is_tilea5x_format(fmt))
		src_format |= BIT(31);

	if (pipe->flags & SDE_ROT_90)
		src_format |= BIT(11); /* ROT90 */

	if (fmt->alpha_enable &&
			fmt->fetch_planes != SDE_MDP_PLANE_INTERLEAVED)
		src_format |= BIT(8); /* SRCC3_EN */

	unpack = (fmt->element[3] << 24) | (fmt->element[2] << 16) |
			(fmt->element[1] << 8) | (fmt->element[0] << 0);
	src_format |= ((fmt->unpack_count - 1) << 12) |
			(fmt->unpack_tight << 17) |
			(fmt->unpack_align_msb << 18) |
			((fmt->bpp - 1) << 9);

	if (sde_mdp_is_ubwc_format(fmt))
		opmode |= BIT(0);

	if (fmt->is_yuv)
		src_format |= BIT(15);

	if (fmt->frame_format != SDE_MDP_FMT_LINEAR
		&& mdata->highest_bank_bit) {
		sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_FETCH_CONFIG,
			SDE_MDP_FETCH_CONFIG_RESET_VALUE |
				 mdata->highest_bank_bit << 18);
	}

	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_FORMAT, src_format);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_UNPACK_PATTERN, unpack);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_OP_MODE, opmode);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC_ADDR_SW_STATUS, secure);

	/* clear UBWC error */
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_UBWC_ERROR_STATUS, BIT(31));

	return 0;
}

static int sde_mdp_src_addr_setup(struct sde_mdp_pipe *pipe,
				   struct sde_mdp_data *src_data)
{
	struct sde_mdp_data data = *src_data;
	u32 x = 0, y = 0;
	int ret = 0;

	SDEROT_DBG("pnum=%d\n", pipe->num);

	ret = sde_mdp_data_check(&data, &pipe->src_planes, pipe->src_fmt);
	if (ret)
		return ret;

	sde_rot_data_calc_offset(&data, x, y,
		&pipe->src_planes, pipe->src_fmt);

	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC0_ADDR, data.p[0].addr);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC1_ADDR, data.p[1].addr);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC2_ADDR, data.p[2].addr);
	sde_mdp_pipe_write(pipe, SDE_MDP_REG_SSPP_SRC3_ADDR, data.p[3].addr);

	return 0;
}

static void sde_mdp_set_ot_limit_pipe(struct sde_mdp_pipe *pipe)
{
	struct sde_mdp_set_ot_params ot_params = {0,};

	ot_params.xin_id = pipe->xin_id;
	ot_params.num = pipe->num;
	ot_params.width = pipe->src.w;
	ot_params.height = pipe->src.h;
	ot_params.fps = 60;
	ot_params.reg_off_vbif_lim_conf = MMSS_VBIF_RD_LIM_CONF;
	ot_params.reg_off_mdp_clk_ctrl = pipe->clk_ctrl.reg_off;
	ot_params.bit_off_mdp_clk_ctrl = pipe->clk_ctrl.bit_off +
		CLK_FORCE_ON_OFFSET;
	ot_params.fmt = (pipe->src_fmt) ? pipe->src_fmt->format : 0;

	sde_mdp_set_ot_limit(&ot_params);
}

int sde_mdp_pipe_queue_data(struct sde_mdp_pipe *pipe,
			     struct sde_mdp_data *src_data)
{
	int ret = 0;
	u32 params_changed;
	struct sde_rot_data_type *mdata = sde_rot_get_mdata();

	if (!pipe) {
		SDEROT_ERR("pipe not setup properly for queue\n");
		return -ENODEV;
	}

	/*
	 * Reprogram the pipe when there is no dedicated wfd blk and
	 * virtual mixer is allocated for the DMA pipe during concurrent
	 * line and block mode operations
	 */

	params_changed = (pipe->params_changed);
	if (params_changed) {
		bool is_realtime = !(pipe->mixer_left->rotator_mode);

		sde_mdp_qos_vbif_remapper_setup(mdata, pipe, is_realtime);

		if (mdata->vbif_nrt_io.base)
			sde_mdp_pipe_nrt_vbif_setup(mdata, pipe);
	}

	if (params_changed) {
		pipe->params_changed = 0;

		ret = sde_mdp_image_setup(pipe, src_data);
		if (ret) {
			SDEROT_ERR("image setup error for pnum=%d\n",
					pipe->num);
			goto done;
		}

		ret = sde_mdp_format_setup(pipe);
		if (ret) {
			SDEROT_ERR("format %d setup error pnum=%d\n",
			       pipe->src_fmt->format, pipe->num);
			goto done;
		}

		if (test_bit(SDE_QOS_PER_PIPE_LUT, mdata->sde_qos_map))
			sde_mdp_pipe_qos_lut(pipe);

		sde_mdp_set_ot_limit_pipe(pipe);
	}

	ret = sde_mdp_src_addr_setup(pipe, src_data);
	if (ret) {
		SDEROT_ERR("addr setup error for pnum=%d\n", pipe->num);
		goto done;
	}

	sde_mdp_mixer_pipe_update(pipe, pipe->mixer_left,
			params_changed);
done:
	return ret;
}
