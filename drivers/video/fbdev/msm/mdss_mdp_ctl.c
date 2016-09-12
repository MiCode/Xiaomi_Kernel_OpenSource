/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sort.h>
#include <linux/clk.h>
#include <linux/bitmap.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_mdp_trace.h"
#include "mdss_debug.h"

#define MDSS_MDP_QSEED3_VER_DOWNSCALE_LIM 2
#define NUM_MIXERCFG_REGS 3
#define MDSS_MDP_WB_OUTPUT_BPP	3
struct mdss_mdp_mixer_cfg {
	u32 config_masks[NUM_MIXERCFG_REGS];
	bool border_enabled;
	bool cursor_enabled;
};

static struct {
	u32 flush_bit;
	struct mdss_mdp_hwio_cfg base;
	struct mdss_mdp_hwio_cfg ext;
	struct mdss_mdp_hwio_cfg ext2;
} mdp_pipe_hwio[MDSS_MDP_MAX_SSPP] = {
	[MDSS_MDP_SSPP_VIG0]    = {  0, {  0, 3, 0 }, {  0, 1, 3 } },
	[MDSS_MDP_SSPP_VIG1]    = {  1, {  3, 3, 0 }, {  2, 1, 3 } },
	[MDSS_MDP_SSPP_VIG2]    = {  2, {  6, 3, 0 }, {  4, 1, 3 } },
	[MDSS_MDP_SSPP_VIG3]    = { 18, { 26, 3, 0 }, {  6, 1, 3 } },
	[MDSS_MDP_SSPP_RGB0]    = {  3, {  9, 3, 0 }, {  8, 1, 3 } },
	[MDSS_MDP_SSPP_RGB1]    = {  4, { 12, 3, 0 }, { 10, 1, 3 } },
	[MDSS_MDP_SSPP_RGB2]    = {  5, { 15, 3, 0 }, { 12, 1, 3 } },
	[MDSS_MDP_SSPP_RGB3]    = { 19, { 29, 3, 0 }, { 14, 1, 3 } },
	[MDSS_MDP_SSPP_DMA0]    = { 11, { 18, 3, 0 }, { 16, 1, 3 } },
	[MDSS_MDP_SSPP_DMA1]    = { 12, { 21, 3, 0 }, { 18, 1, 3 } },
	[MDSS_MDP_SSPP_DMA2]    = { 24, .ext2 = {  0, 4, 0 } },
	[MDSS_MDP_SSPP_DMA3]    = { 25, .ext2 = {  4, 4, 0 } },
	[MDSS_MDP_SSPP_CURSOR0] = { 22, .ext  = { 20, 4, 0 } },
	[MDSS_MDP_SSPP_CURSOR1] = { 23, .ext  = { 26, 4, 0 } },
};

static struct {
	struct mdss_mdp_hwio_cfg ext2;
} mdp_pipe_rec1_hwio[MDSS_MDP_MAX_SSPP] = {
	[MDSS_MDP_SSPP_DMA0]    = { .ext2 = {  8, 4, 0 } },
	[MDSS_MDP_SSPP_DMA1]    = { .ext2 = { 12, 4, 0 } },
	[MDSS_MDP_SSPP_DMA2]    = { .ext2 = { 16, 4, 0 } },
	[MDSS_MDP_SSPP_DMA3]    = { .ext2 = { 20, 4, 0 } },
};

static void __mdss_mdp_mixer_write_cfg(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_mixer_cfg *cfg);

static inline u64 fudge_factor(u64 val, u32 numer, u32 denom)
{
	u64 result = (val * (u64)numer);
	do_div(result, denom);
	return result;
}

static inline u64 apply_fudge_factor(u64 val,
	struct mult_factor *factor)
{
	return fudge_factor(val, factor->numer, factor->denom);
}

static inline u64 apply_inverse_fudge_factor(u64 val,
	struct mult_factor *factor)
{
	return fudge_factor(val, factor->denom, factor->numer);
}

static DEFINE_MUTEX(mdss_mdp_ctl_lock);

static inline u64 mdss_mdp_get_pclk_rate(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

	return (ctl->intf_type == MDSS_INTF_DSI) ?
		pinfo->mipi.dsi_pclk_rate :
		pinfo->clk_rate;
}

static inline u32 mdss_mdp_clk_fudge_factor(struct mdss_mdp_mixer *mixer,
						u32 rate)
{
	struct mdss_panel_info *pinfo = &mixer->ctl->panel_data->panel_info;

	rate = apply_fudge_factor(rate, &mdss_res->clk_factor);

	/*
	 * If the panel is video mode and its back porch period is
	 * small, the workaround of increasing mdp clk is needed to
	 * avoid underrun.
	 */
	if (mixer->ctl->is_video_mode && pinfo &&
		(pinfo->lcdc.v_back_porch < MDP_MIN_VBP))
		rate = apply_fudge_factor(rate, &mdss_res->clk_factor);

	return rate;
}

struct mdss_mdp_prefill_params {
	u32 smp_bytes;
	u32 xres;
	u32 src_w;
	u32 dst_w;
	u32 src_h;
	u32 dst_h;
	u32 dst_y;
	u32 bpp;
	u32 pnum;
	bool is_yuv;
	bool is_caf;
	bool is_fbc;
	bool is_bwc;
	bool is_tile;
	bool is_hflip;
	bool is_cmd;
	bool is_ubwc;
	bool is_nv12;
};

static inline bool mdss_mdp_perf_is_caf(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	/*
	 * CAF mode filter is enabled when format is yuv and
	 * upscaling. Post processing had the decision to use CAF
	 * under these conditions.
	 */
	return ((mdata->mdp_rev >= MDSS_MDP_HW_REV_102) &&
		pipe->src_fmt->is_yuv && ((pipe->src.h >> pipe->vert_deci) <=
			pipe->dst.h));
}

static inline u32 mdss_mdp_calc_y_scaler_bytes(struct mdss_mdp_prefill_params
	*params, struct mdss_prefill_data *prefill)
{
	u32 y_scaler_bytes = 0, y_scaler_lines = 0;

	if (params->is_yuv) {
		if (params->src_h != params->dst_h) {
			y_scaler_lines = (params->is_caf) ?
				prefill->y_scaler_lines_caf :
				prefill->y_scaler_lines_bilinear;
			/*
			 * y is src_width, u is src_width/2 and v is
			 * src_width/2, so the total is scaler_lines *
			 * src_w * 2
			 */
			y_scaler_bytes = y_scaler_lines * params->src_w * 2;
		}
	} else {
		if (params->src_h != params->dst_h) {
			y_scaler_lines = prefill->y_scaler_lines_bilinear;
			y_scaler_bytes = y_scaler_lines * params->src_w *
				params->bpp;
		}
	}

	return y_scaler_bytes;
}

static inline u32 mdss_mdp_align_latency_buf_bytes(
		u32 latency_buf_bytes, u32 percentage,
		u32 smp_bytes)
{
	u32 aligned_bytes;

	aligned_bytes = ((smp_bytes - latency_buf_bytes) * percentage) / 100;

	pr_debug("percentage=%d, extra_bytes(per)=%d smp_bytes=%d latency=%d\n",
		percentage, aligned_bytes, smp_bytes, latency_buf_bytes);
	return latency_buf_bytes + aligned_bytes;
}

/**
 * @ mdss_mdp_calc_latency_buf_bytes() -
 *                             Get the number of bytes for the
 *                             latency lines.
 * @is_yuv - true if format is yuv
 * @is_bwc - true if BWC is enabled
 * @is_tile - true if it is Tile format
 * @src_w - source rectangle width
 * @bpp - Bytes per pixel of source rectangle
 * @use_latency_buf_percentage - use an extra percentage for
 *				the latency bytes calculation.
 * @smp_bytes - size of the smp for alignment
 * @is_ubwc - true if UBWC is enabled
 * @is_nv12 - true if NV12 format is used
 * @is_hflip - true if HFLIP is enabled
 *
 * Return:
 * The amount of bytes to consider for the latency lines, where:
 *	If use_latency_buf_percentate is  TRUE:
 *		Function will return the amount of bytes for the
 *		latency lines plus a percentage of the
 *		additional bytes allocated to align with the
 *		SMP size. Percentage is determined by
 *		"latency_buff_per", which can be modified
 *		through debugfs.
 *	If use_latency_buf_percentage is FALSE:
 *		Function will return only the the amount of bytes
 *		for the latency lines without any
 *		extra bytes.
 */
u32 mdss_mdp_calc_latency_buf_bytes(bool is_yuv, bool is_bwc,
	bool is_tile, u32 src_w, u32 bpp, bool use_latency_buf_percentage,
	u32 smp_bytes, bool is_ubwc, bool is_nv12, bool is_hflip)
{
	u32 latency_lines = 0, latency_buf_bytes;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (is_hflip && !mdata->hflip_buffer_reused)
		latency_lines = 1;

	if (is_yuv) {
		if (is_ubwc) {
			if (is_nv12)
				latency_lines += 8;
			else
				latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else if (is_bwc) {
			latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else {
			if (!mdata->hflip_buffer_reused)
				latency_lines += 1;
			else
				latency_lines = 2;
			/* multiply * 2 for the two YUV planes */
			latency_buf_bytes = mdss_mdp_align_latency_buf_bytes(
				src_w * bpp * latency_lines,
				use_latency_buf_percentage ?
				mdata->latency_buff_per : 0, smp_bytes) * 2;
		}
	} else {
		if (is_ubwc) {
			latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else if (is_tile) {
			latency_lines += 8;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else if (is_bwc) {
			latency_lines += 4;
			latency_buf_bytes = src_w * bpp * latency_lines;
		} else {
			if (!mdata->hflip_buffer_reused)
				latency_lines += 1;
			else
				latency_lines = 2;
			latency_buf_bytes = mdss_mdp_align_latency_buf_bytes(
				src_w * bpp * latency_lines,
				use_latency_buf_percentage ?
				mdata->latency_buff_per : 0, smp_bytes);
		}
	}

	return latency_buf_bytes;
}

static inline u32 mdss_mdp_calc_scaling_w_h(u32 val, u32 src_h, u32 dst_h,
	u32 src_w, u32 dst_w)
{
	if (dst_h)
		val = mult_frac(val, src_h, dst_h);
	if (dst_w)
		val = mult_frac(val, src_w, dst_w);

	return val;
}

static u32 mdss_mdp_perf_calc_pipe_prefill_video(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes = 0;
	u32 latency_buf_bytes = 0;
	u32 y_buf_bytes = 0;
	u32 y_scaler_bytes = 0;
	u32 pp_bytes = 0, pp_lines = 0;
	u32 post_scaler_bytes = 0;
	u32 fbc_bytes = 0;

	prefill_bytes = prefill->ot_bytes;

	latency_buf_bytes = mdss_mdp_calc_latency_buf_bytes(params->is_yuv,
		params->is_bwc, params->is_tile, params->src_w, params->bpp,
		true, params->smp_bytes, params->is_ubwc, params->is_nv12,
		params->is_hflip);
	prefill_bytes += latency_buf_bytes;
	pr_debug("latency_buf_bytes bw_calc=%d actual=%d\n", latency_buf_bytes,
		params->smp_bytes);

	if (params->is_yuv)
		y_buf_bytes = prefill->y_buf_bytes;

	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);

	prefill_bytes += y_buf_bytes + y_scaler_bytes;

	if (mdata->apply_post_scale_bytes || (params->src_h != params->dst_h) ||
			(params->src_w != params->dst_w)) {
		post_scaler_bytes = prefill->post_scaler_pixels * params->bpp;
		post_scaler_bytes = mdss_mdp_calc_scaling_w_h(post_scaler_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += post_scaler_bytes;
	}

	if (params->xres)
		pp_lines = DIV_ROUND_UP(prefill->pp_pixels, params->xres);
	if (params->xres && params->dst_h && (params->dst_y <= pp_lines))
		pp_bytes = ((params->src_w * params->bpp * prefill->pp_pixels /
				params->xres) * params->src_h) / params->dst_h;
	prefill_bytes += pp_bytes;

	if (params->is_fbc) {
		fbc_bytes = prefill->fbc_lines * params->bpp;
		fbc_bytes = mdss_mdp_calc_scaling_w_h(fbc_bytes, params->src_h,
			params->dst_h, params->src_w, params->dst_w);
	}
	prefill_bytes += fbc_bytes;

	trace_mdp_perf_prefill_calc(params->pnum, latency_buf_bytes,
		prefill->ot_bytes, y_buf_bytes, y_scaler_bytes, pp_lines,
		pp_bytes, post_scaler_bytes, fbc_bytes, prefill_bytes);

	pr_debug("ot=%d y_buf=%d pp_lines=%d pp=%d post_sc=%d fbc_bytes=%d\n",
		prefill->ot_bytes, y_buf_bytes, pp_lines, pp_bytes,
		post_scaler_bytes, fbc_bytes);

	return prefill_bytes;
}

static u32 mdss_mdp_perf_calc_pipe_prefill_cmd(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 ot_bytes = 0;
	u32 latency_lines, latency_buf_bytes;
	u32 y_buf_bytes = 0;
	u32 y_scaler_bytes;
	u32 fbc_cmd_lines = 0, fbc_cmd_bytes = 0;
	u32 post_scaler_bytes = 0;

	/* y_scaler_bytes are same for the first or non first line */
	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);
	prefill_bytes = y_scaler_bytes;

	/* 1st line if fbc is not enabled and 2nd line if fbc is enabled */
	if (((params->dst_y == 0) && !params->is_fbc) ||
		((params->dst_y <= 1) && params->is_fbc)) {
		if (params->is_ubwc) {
			if (params->is_nv12)
				latency_lines = 8;
			else
				latency_lines = 4;
		} else if (params->is_bwc || params->is_tile) {
			latency_lines = 4;
		} else if (params->is_hflip) {
			latency_lines = 1;
		} else {
			latency_lines = 0;
		}
		latency_buf_bytes = params->src_w * params->bpp * latency_lines;
		prefill_bytes += latency_buf_bytes;

		fbc_cmd_lines++;
		if (params->is_fbc)
			fbc_cmd_lines++;
		fbc_cmd_bytes = params->bpp * params->dst_w * fbc_cmd_lines;
		fbc_cmd_bytes = mdss_mdp_calc_scaling_w_h(fbc_cmd_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += fbc_cmd_bytes;
	} else {
		ot_bytes = prefill->ot_bytes;
		prefill_bytes += ot_bytes;

		latency_buf_bytes = mdss_mdp_calc_latency_buf_bytes(
			params->is_yuv, params->is_bwc, params->is_tile,
			params->src_w, params->bpp, true, params->smp_bytes,
			params->is_ubwc, params->is_nv12, params->is_hflip);
		prefill_bytes += latency_buf_bytes;

		if (params->is_yuv)
			y_buf_bytes = prefill->y_buf_bytes;
		prefill_bytes += y_buf_bytes;

		if (mdata->apply_post_scale_bytes ||
				(params->src_h != params->dst_h) ||
				(params->src_w != params->dst_w)) {
			post_scaler_bytes = prefill->post_scaler_pixels *
				params->bpp;
			post_scaler_bytes = mdss_mdp_calc_scaling_w_h(
				post_scaler_bytes, params->src_h,
				params->dst_h, params->src_w,
				params->dst_w);
			prefill_bytes += post_scaler_bytes;
		}
	}

	pr_debug("ot=%d bwc=%d smp=%d y_buf=%d fbc=%d\n", ot_bytes,
		params->is_bwc, latency_buf_bytes, y_buf_bytes, fbc_cmd_bytes);

	return prefill_bytes;
}

u32 mdss_mdp_perf_calc_pipe_prefill_single(struct mdss_mdp_prefill_params
	*params)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_prefill_data *prefill = &mdata->prefill_data;
	u32 prefill_bytes;
	u32 latency_lines, latency_buf_bytes;
	u32 y_scaler_bytes;
	u32 fbc_cmd_lines = 0, fbc_cmd_bytes = 0;

	if (params->is_ubwc) {
		if (params->is_nv12)
			latency_lines = 8;
		else
			latency_lines = 4;
	} else if (params->is_bwc || params->is_tile)
		/* can start processing after receiving 4 lines */
		latency_lines = 4;
	else if (params->is_hflip)
		/* need oneline before reading backwards */
		latency_lines = 1;
	else
		latency_lines = 0;
	latency_buf_bytes = params->src_w * params->bpp * latency_lines;
	prefill_bytes = latency_buf_bytes;

	y_scaler_bytes = mdss_mdp_calc_y_scaler_bytes(params, prefill);
	prefill_bytes += y_scaler_bytes;

	if (params->is_cmd)
		fbc_cmd_lines++;
	if (params->is_fbc)
		fbc_cmd_lines++;

	if (fbc_cmd_lines) {
		fbc_cmd_bytes = params->bpp * params->dst_w * fbc_cmd_lines;
		fbc_cmd_bytes = mdss_mdp_calc_scaling_w_h(fbc_cmd_bytes,
			params->src_h, params->dst_h, params->src_w,
			params->dst_w);
		prefill_bytes += fbc_cmd_bytes;
	}

	return prefill_bytes;
}

u32 mdss_mdp_perf_calc_smp_size(struct mdss_mdp_pipe *pipe,
	bool calc_smp_size)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 smp_bytes;

	if (pipe->type == MDSS_MDP_PIPE_TYPE_CURSOR)
		return 0;

	/* Get allocated or fixed smp bytes */
	smp_bytes = mdss_mdp_smp_get_size(pipe);

	/*
	 * We need to calculate the SMP size for scenarios where
	 * allocation have not happened yet (i.e. during prepare IOCTL).
	 */
	if (calc_smp_size && !mdata->has_pixel_ram) {
		u32 calc_smp_total;
		calc_smp_total = mdss_mdp_smp_calc_num_blocks(pipe);
		calc_smp_total *= mdata->smp_mb_size;

		/*
		 * If the pipe has fixed SMPs, then we must consider
		 * the max smp size.
		 */
		if (calc_smp_total > smp_bytes)
			smp_bytes = calc_smp_total;
	}

	pr_debug("SMP size (bytes) %d for pnum=%d calc=%d\n",
		smp_bytes, pipe->num, calc_smp_size);
	BUG_ON(smp_bytes == 0);

	return smp_bytes;
}

static void mdss_mdp_get_bw_vote_mode(void *data,
	u32 mdp_rev, struct mdss_mdp_perf_params *perf,
	enum perf_calc_vote_mode calc_mode, u32 flags)
{

	if (!data)
		goto exit;

	switch (mdp_rev) {
	case MDSS_MDP_HW_REV_105:
	case MDSS_MDP_HW_REV_109:
		if (calc_mode == PERF_CALC_VOTE_MODE_PER_PIPE) {
			struct mdss_mdp_mixer *mixer =
				(struct mdss_mdp_mixer *)data;

			if ((flags & PERF_CALC_PIPE_SINGLE_LAYER) &&
				!mixer->rotator_mode &&
				(mixer->type == MDSS_MDP_MIXER_TYPE_INTF))
					set_bit(MDSS_MDP_BW_MODE_SINGLE_LAYER,
						perf->bw_vote_mode);
		} else if (calc_mode == PERF_CALC_VOTE_MODE_CTL) {
			struct mdss_mdp_ctl *ctl = (struct mdss_mdp_ctl *)data;

			if (ctl->is_video_mode &&
				(ctl->mfd->split_mode == MDP_SPLIT_MODE_NONE))
					set_bit(MDSS_MDP_BW_MODE_SINGLE_IF,
						perf->bw_vote_mode);
		}
		break;
	default:
		break;
	};

	pr_debug("mode=0x%lx\n", *(perf->bw_vote_mode));

exit:
	return;
}

static u32 __calc_qseed3_mdp_clk_rate(struct mdss_mdp_pipe *pipe,
	struct mdss_rect src, struct mdss_rect dst, u32 src_h,
	u32 fps, u32 v_total)
{
	u64 active_line_cycle, backfill_cycle, total_cycle;
	u64 ver_dwnscale;
	u64 active_line;
	u64 backfill_line;

	ver_dwnscale = (u64)src_h << PHASE_STEP_SHIFT;
	do_div(ver_dwnscale, dst.h);

	if (ver_dwnscale > (MDSS_MDP_QSEED3_VER_DOWNSCALE_LIM
			<< PHASE_STEP_SHIFT)) {
		active_line = MDSS_MDP_QSEED3_VER_DOWNSCALE_LIM
			<< PHASE_STEP_SHIFT;
		backfill_line = ver_dwnscale - active_line;
	} else {
		/* active line same as downscale and no backfill */
		active_line = ver_dwnscale;
		backfill_line = 0;
	}

	active_line_cycle = mult_frac(active_line, src.w,
		4) >> PHASE_STEP_SHIFT; /* 4pix/clk */
	if (active_line_cycle < dst.w)
		active_line_cycle = dst.w;

	backfill_cycle = mult_frac(backfill_line, src.w, 4) /* 4pix/clk */
		>> PHASE_STEP_SHIFT;

	total_cycle = active_line_cycle + backfill_cycle;

	pr_debug("line: active=%lld backfill=%lld vds=%lld\n",
		active_line, backfill_line, ver_dwnscale);
	pr_debug("cycle: total=%lld active=%lld backfill=%lld\n",
		total_cycle, active_line_cycle, backfill_cycle);

	return (u32)total_cycle * (fps * v_total);
}

static inline bool __is_vert_downscaling(u32 src_h,
	struct mdss_rect dst){

	return (src_h > dst.h);
}

static u32 get_pipe_mdp_clk_rate(struct mdss_mdp_pipe *pipe,
	struct mdss_rect src, struct mdss_rect dst,
	u32 fps, u32 v_total, u32 flags)
{
	struct mdss_mdp_mixer *mixer;
	u32 rate, src_h;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	/*
	 * when doing vertical decimation lines will be skipped, hence there is
	 * no need to account for these lines in MDP clock or request bus
	 * bandwidth to fetch them.
	 */
	mixer = pipe->mixer_left;
	src_h = DECIMATED_DIMENSION(src.h, pipe->vert_deci);

	if (mixer->rotator_mode) {

		rate = pipe->src.w * pipe->src.h * fps;
		rate /= 4; /* block mode fetch at 4 pix/clk */
	} else if (test_bit(MDSS_CAPS_QSEED3, mdata->mdss_caps_map) &&
		pipe->scaler.enable && __is_vert_downscaling(src_h, dst)) {

		rate = __calc_qseed3_mdp_clk_rate(pipe, src, dst, src_h,
			fps, v_total);
	} else {

		rate = dst.w;
		if (src_h > dst.h)
			rate = (rate * src_h) / dst.h;

		rate *= v_total * fps;

		/* pipes decoding BWC content have different clk requirement */
		if (pipe->bwc_mode && !pipe->src_fmt->is_yuv &&
		    pipe->src_fmt->bpp == 4) {
			u32 bwc_rate =
			mult_frac((src.w * src_h * fps), v_total, dst.h << 1);
			pr_debug("src: w:%d h:%d fps:%d vtotal:%d dst h:%d\n",
				src.w, src_h, fps, v_total, dst.h);
			pr_debug("pipe%d: bwc_rate=%d normal_rate=%d\n",
				pipe->num, bwc_rate, rate);
			rate = max(bwc_rate, rate);
		}
	}

	if (flags & PERF_CALC_PIPE_APPLY_CLK_FUDGE)
		rate = mdss_mdp_clk_fudge_factor(mixer, rate);

	return rate;
}

static u32 mdss_mdp_get_rotator_fps(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 fps;

	if (pipe->src.w >= 3840 || pipe->src.h >= 3840)
		fps = ROTATOR_LOW_FRAME_RATE;
	else if (mdata->traffic_shaper_en)
		fps = DEFAULT_ROTATOR_FRAME_RATE;
	else if (pipe->frame_rate)
		fps = pipe->frame_rate;
	else
		fps = DEFAULT_FRAME_RATE;

	pr_debug("rotator fps:%d\n", fps);

	return fps;
}

int mdss_mdp_get_panel_params(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer, u32 *fps, u32 *v_total,
	u32 *h_total, u32 *xres)
{

	if (mixer->rotator_mode) {
		*fps = mdss_mdp_get_rotator_fps(pipe);
	} else if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		struct mdss_panel_info *pinfo;

		if (!mixer->ctl)
			return -EINVAL;

		pinfo = &mixer->ctl->panel_data->panel_info;
		if (pinfo->type == MIPI_VIDEO_PANEL) {
			*fps = pinfo->panel_max_fps;
			*v_total = pinfo->panel_max_vtotal;
		} else {
			*fps = mdss_panel_get_framerate(pinfo);
			*v_total = mdss_panel_get_vtotal(pinfo);
		}
		*xres = get_panel_width(mixer->ctl);
		*h_total = mdss_panel_get_htotal(pinfo, false);

		if (is_pingpong_split(mixer->ctl->mfd))
			*h_total += mdss_panel_get_htotal(
				&mixer->ctl->panel_data->next->panel_info,
				false);
	} else {
		*v_total = mixer->height;
		*xres = mixer->width;
		*h_total = mixer->width;
	}

	return 0;
}

int mdss_mdp_get_pipe_overlap_bw(struct mdss_mdp_pipe *pipe,
	struct mdss_rect *roi, u64 *quota, u64 *quota_nocr, u32 flags)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_mixer *mixer = pipe->mixer_left;
	struct mdss_rect src, dst;
	u32 v_total, fps, h_total, xres, src_h;
	*quota = 0;
	*quota_nocr = 0;

	if (mdss_mdp_get_panel_params(pipe, mixer, &fps, &v_total,
			&h_total, &xres)) {
		pr_err(" error retreiving the panel params!\n");
		return -EINVAL;
	}

	dst = pipe->dst;
	src = pipe->src;

	/* crop rectangles */
	if (roi && !mixer->ctl->is_video_mode && !pipe->src_split_req)
		mdss_mdp_crop_rect(&src, &dst, roi);

	/*
	 * when doing vertical decimation lines will be skipped, hence there is
	 * no need to account for these lines in MDP clock or request bus
	 * bandwidth to fetch them.
	 */
	src_h = DECIMATED_DIMENSION(src.h, pipe->vert_deci);

	*quota = fps * src.w * src_h;

	if (pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_420)
		/*
		 * with decimation, chroma is not downsampled, this means we
		 * need to allocate bw for extra lines that will be fetched
		 */
		if (pipe->vert_deci)
			*quota *= 2;
		else
			*quota = (*quota * 3) / 2;
	else
		*quota *= pipe->src_fmt->bpp;

	if (mixer->rotator_mode) {
		if (test_bit(MDSS_QOS_OVERHEAD_FACTOR,
				mdata->mdss_qos_map)) {
			/* rotator read */
			*quota_nocr += (*quota * 2);
			*quota = apply_comp_ratio_factor(*quota,
				pipe->src_fmt, &pipe->comp_ratio);
			/*
			 * rotator write: here we are using src_fmt since
			 * current implementation only supports calculate
			 * bandwidth based in the source parameters.
			 * The correct fine-tuned calculation should use
			 * destination format and destination rectangles to
			 * calculate the bandwidth, but leaving this
			 * calculation as per current support.
			 */
			*quota += apply_comp_ratio_factor(*quota,
				pipe->src_fmt, &pipe->comp_ratio);
		} else {
			*quota *= 2; /* bus read + write */
		}
	} else {

		*quota = DIV_ROUND_UP_ULL(*quota * v_total, dst.h);
		if (!mixer->ctl->is_video_mode)
			*quota = DIV_ROUND_UP_ULL(*quota * h_total, xres);

		*quota_nocr = *quota;

		if (test_bit(MDSS_QOS_OVERHEAD_FACTOR,
				mdata->mdss_qos_map))
			*quota = apply_comp_ratio_factor(*quota,
				pipe->src_fmt, &pipe->comp_ratio);
	}


	pr_debug("quota:%llu nocr:%llu src.w:%d src.h%d comp:[%d, %d]\n",
		*quota, *quota_nocr, src.w, src_h, pipe->comp_ratio.numer,
		pipe->comp_ratio.denom);

	return 0;
}

static inline bool validate_comp_ratio(struct mult_factor *factor)
{
	return factor->numer && factor->denom;
}

u32 apply_comp_ratio_factor(u32 quota,
	struct mdss_mdp_format_params *fmt,
	struct mult_factor *factor)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!mdata || !test_bit(MDSS_QOS_OVERHEAD_FACTOR,
		      mdata->mdss_qos_map))
		return quota;

	/* apply compression ratio, only for compressed formats */
	if (mdss_mdp_is_ubwc_format(fmt) &&
	    validate_comp_ratio(factor))
		quota = apply_inverse_fudge_factor(quota , factor);

	return quota;
}

static u32 mdss_mdp_get_vbp_factor(struct mdss_mdp_ctl *ctl)
{
	u32 fps, v_total, vbp, vbp_fac;
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data)
		return 0;

	pinfo = &ctl->panel_data->panel_info;
	fps = mdss_panel_get_framerate(pinfo);
	v_total = mdss_panel_get_vtotal(pinfo);
	vbp = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width;
	vbp += pinfo->prg_fet;

	vbp_fac = (vbp) ? fps * v_total / vbp : 0;
	pr_debug("vbp_fac=%d vbp=%d v_total=%d\n", vbp_fac, vbp, v_total);

	return vbp_fac;
}

static u32 mdss_mdp_get_vbp_factor_max(struct mdss_mdp_ctl *ctl)
{
	u32 vbp_max = 0;
	int i;
	struct mdss_data_type *mdata;

	if (!ctl || !ctl->mdata)
		return 0;

	mdata = ctl->mdata;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl = mdata->ctl_off + i;
		u32 vbp_fac;

		/* skip command mode interfaces */
		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map)
				&& !ctl->is_video_mode)
			continue;

		if (mdss_mdp_ctl_is_power_on(ctl)) {
			vbp_fac = mdss_mdp_get_vbp_factor(ctl);
			vbp_max = max(vbp_max, vbp_fac);
		}
	}

	return vbp_max;
}

static u32 __calc_prefill_line_time_us(struct mdss_mdp_ctl *ctl)
{
	u32 fps, v_total, vbp, vbp_fac;
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data)
		return 0;

	pinfo = &ctl->panel_data->panel_info;
	fps = mdss_panel_get_framerate(pinfo);
	v_total = mdss_panel_get_vtotal(pinfo);
	vbp = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width;
	vbp += pinfo->prg_fet;

	vbp_fac = mult_frac(USEC_PER_SEC, vbp, fps * v_total); /* use uS */
	pr_debug("vbp_fac=%d vbp=%d v_total=%d fps=%d\n",
		vbp_fac, vbp, v_total, fps);

	return vbp_fac;
}

static u32 __get_min_prefill_line_time_us(struct mdss_mdp_ctl *ctl)
{
	u32 vbp_min = 0;
	int i;
	struct mdss_data_type *mdata;

	if (!ctl || !ctl->mdata)
		return 0;

	mdata = ctl->mdata;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *tmp_ctl = mdata->ctl_off + i;
		u32 vbp_fac;

		/* skip command mode interfaces */
		if (!tmp_ctl->is_video_mode)
			continue;

		if (mdss_mdp_ctl_is_power_on(tmp_ctl)) {
			vbp_fac = __calc_prefill_line_time_us(tmp_ctl);
			vbp_min = min(vbp_min, vbp_fac);
		}
	}

	return vbp_min;
}

static u32 mdss_mdp_calc_prefill_line_time(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_pipe *pipe)
{
	u32 prefill_us = 0;
	u32 prefill_amortized = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_mixer *mixer;
	struct mdss_panel_info *pinfo;
	u32 fps, v_total;

	if (!ctl || !ctl->mdata)
		return 0;

	mixer = pipe->mixer_left;
	if (!mixer)
		return -EINVAL;

	pinfo = &ctl->panel_data->panel_info;
	fps = mdss_panel_get_framerate(pinfo);
	v_total = mdss_panel_get_vtotal(pinfo);

	/* calculate the minimum prefill */
	prefill_us = __get_min_prefill_line_time_us(ctl);

	/* if pipe is amortizable, add the amortized prefill contribution */
	if (mdss_mdp_is_amortizable_pipe(pipe, mixer, mdata)) {
		prefill_amortized = mult_frac(USEC_PER_SEC, pipe->src.y,
			fps * v_total);
		prefill_us += prefill_amortized;
	}

	return prefill_us;
}

static inline bool __is_multirect_high_pipe(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_pipe *next_pipe = pipe->multirect.next;

	return (pipe->src.y > next_pipe->src.y);
}

static u64 mdss_mdp_apply_prefill_factor(u64 prefill_bw,
	struct mdss_mdp_ctl *ctl, struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u64 total_prefill_bw;
	u32 prefill_time_us;

	if (test_bit(MDSS_QOS_TS_PREFILL, mdata->mdss_qos_map)) {

		/*
		 * for multi-rect serial mode, only take the contribution from
		 * pipe that belongs to the rect closest to the origin.
		 */
		if (pipe->multirect.mode == MDSS_MDP_PIPE_MULTIRECT_SERIAL &&
			__is_multirect_high_pipe(pipe)) {
			total_prefill_bw = 0;
			goto exit;
		}

		prefill_time_us = mdss_mdp_calc_prefill_line_time(ctl, pipe);
		total_prefill_bw = prefill_time_us ? DIV_ROUND_UP_ULL(
			USEC_PER_SEC * prefill_bw, prefill_time_us) : 0;
	} else {
		total_prefill_bw = prefill_bw *
			mdss_mdp_get_vbp_factor_max(ctl);
	}

exit:
	return total_prefill_bw;
}

u64 mdss_mdp_perf_calc_simplified_prefill(struct mdss_mdp_pipe *pipe,
	u32 v_total, u32 fps, struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct simplified_prefill_factors *pfactors =
			&mdata->prefill_data.prefill_factors;
	u64 prefill_per_pipe = 0;
	u32 prefill_lines = pfactors->xtra_ff_factor;


	/* do not calculate prefill for command mode */
	if (!ctl->is_video_mode)
		goto exit;

	prefill_per_pipe = pipe->src.w * pipe->src_fmt->bpp;

	/* format factors */
	if (mdss_mdp_is_tile_format(pipe->src_fmt)) {
		if (mdss_mdp_is_nv12_format(pipe->src_fmt))
			prefill_lines += pfactors->fmt_mt_nv12_factor;
		else
			prefill_lines += pfactors->fmt_mt_factor;
	} else {
		prefill_lines += pfactors->fmt_linear_factor;
	}

	/* scaling factors */
	if (pipe->src.h > pipe->dst.h) {
		prefill_lines += pfactors->scale_factor;

		prefill_per_pipe = fudge_factor(prefill_per_pipe,
			DECIMATED_DIMENSION(pipe->src.h, pipe->vert_deci),
			pipe->dst.h);
	}

	prefill_per_pipe *= prefill_lines;
	prefill_per_pipe = mdss_mdp_apply_prefill_factor(prefill_per_pipe,
		ctl, pipe);

	pr_debug("pipe src: %dx%d bpp:%d\n",
		pipe->src.w, pipe->src.h, pipe->src_fmt->bpp);
	pr_debug("ff_factor:%d mt_nv12:%d mt:%d\n",
		pfactors->xtra_ff_factor,
		(mdss_mdp_is_tile_format(pipe->src_fmt) &&
		mdss_mdp_is_nv12_format(pipe->src_fmt)) ?
		pfactors->fmt_mt_nv12_factor : 0,
		mdss_mdp_is_tile_format(pipe->src_fmt) ?
		pfactors->fmt_mt_factor : 0);
	pr_debug("pipe prefill:%llu lines:%d\n",
		prefill_per_pipe, prefill_lines);

exit:
	return prefill_per_pipe;
}

/**
 * mdss_mdp_perf_calc_pipe() - calculate performance numbers required by pipe
 * @pipe:	Source pipe struct containing updated pipe params
 * @perf:	Structure containing values that should be updated for
 *		performance tuning
 * @flags: flags to determine how to perform some of the
 *		calculations, supported flags:
 *
 *	PERF_CALC_PIPE_APPLY_CLK_FUDGE:
 *		Determine if mdp clock fudge is applicable.
 *	PERF_CALC_PIPE_SINGLE_LAYER:
 *		Indicate if the calculation is for a single pipe staged
 *		in the layer mixer
 *	PERF_CALC_PIPE_CALC_SMP_SIZE:
 *		Indicate if the smp size needs to be calculated, this is
 *		for the cases where SMP haven't been allocated yet, so we need
 *		to estimate here the smp size (i.e. PREPARE IOCTL).
 *
 * Function calculates the minimum required performance calculations in order
 * to avoid MDP underflow. The calculations are based on the way MDP
 * fetches (bandwidth requirement) and processes data through MDP pipeline
 * (MDP clock requirement) based on frame size and scaling requirements.
 */

int mdss_mdp_perf_calc_pipe(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_perf_params *perf, struct mdss_rect *roi,
	u32 flags)
{
	struct mdss_mdp_mixer *mixer;
	int fps = DEFAULT_FRAME_RATE;
	u32 v_total = 0, src_h, xres = 0, h_total = 0;
	struct mdss_rect src, dst;
	bool is_fbc = false;
	struct mdss_mdp_prefill_params prefill_params;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool calc_smp_size = false;

	if (!pipe || !perf || !pipe->mixer_left)
		return -EINVAL;

	mixer = pipe->mixer_left;

	dst = pipe->dst;
	src = pipe->src;

	/*
	 * when doing vertical decimation lines will be skipped, hence there is
	 * no need to account for these lines in MDP clock or request bus
	 * bandwidth to fetch them.
	 */
	src_h = DECIMATED_DIMENSION(src.h, pipe->vert_deci);

	if (mdss_mdp_get_panel_params(pipe, mixer, &fps, &v_total,
			&h_total, &xres)) {
		pr_err(" error retreiving the panel params!\n");
		return -EINVAL;
	}

	if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		if (!mixer->ctl)
			return -EINVAL;
		is_fbc = mixer->ctl->panel_data->panel_info.fbc.enabled;
	}

	mixer->ctl->frame_rate = fps;

	/* crop rectangles */
	if (roi && !mixer->ctl->is_video_mode && !pipe->src_split_req)
		mdss_mdp_crop_rect(&src, &dst, roi);

	pr_debug("v_total=%d, xres=%d fps=%d\n", v_total, xres, fps);
	pr_debug("src(w,h)(%d,%d) dst(w,h)(%d,%d) dst_y=%d bpp=%d yuv=%d\n",
		 pipe->src.w, src_h, pipe->dst.w, pipe->dst.h, pipe->dst.y,
		 pipe->src_fmt->bpp, pipe->src_fmt->is_yuv);

	if (mdss_mdp_get_pipe_overlap_bw(pipe, roi, &perf->bw_overlap,
			&perf->bw_overlap_nocr, flags))
		pr_err("failure calculating overlap bw!\n");

	perf->mdp_clk_rate = get_pipe_mdp_clk_rate(pipe, src, dst,
		fps, v_total, flags);

	pr_debug("bw:%llu bw_nocr:%llu clk:%d\n", perf->bw_overlap,
		perf->bw_overlap_nocr, perf->mdp_clk_rate);

	if (pipe->flags & MDP_SOLID_FILL) {
		perf->bw_overlap = 0;
	}

	if (mixer->ctl->intf_num == MDSS_MDP_NO_INTF ||
		mdata->disable_prefill ||
		mixer->ctl->disable_prefill ||
		(pipe->flags & MDP_SOLID_FILL)) {
		perf->prefill_bytes = 0;
		perf->bw_prefill = 0;
		goto exit;
	}

	if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map)) {
		perf->bw_prefill = mdss_mdp_perf_calc_simplified_prefill(pipe,
			v_total, fps, mixer->ctl);
		goto exit;
	}

	calc_smp_size = (flags & PERF_CALC_PIPE_CALC_SMP_SIZE) ? true : false;
	prefill_params.smp_bytes = mdss_mdp_perf_calc_smp_size(pipe,
			calc_smp_size);
	prefill_params.xres = xres;
	prefill_params.src_w = src.w;
	prefill_params.src_h = src_h;
	prefill_params.dst_w = dst.w;
	prefill_params.dst_h = dst.h;
	prefill_params.dst_y = dst.y;
	prefill_params.bpp = pipe->src_fmt->bpp;
	prefill_params.is_yuv = pipe->src_fmt->is_yuv;
	prefill_params.is_caf = mdss_mdp_perf_is_caf(pipe);
	prefill_params.is_fbc = is_fbc;
	prefill_params.is_bwc = pipe->bwc_mode;
	prefill_params.is_tile = mdss_mdp_is_tile_format(pipe->src_fmt);
	prefill_params.is_hflip = pipe->flags & MDP_FLIP_LR;
	prefill_params.is_cmd = !mixer->ctl->is_video_mode;
	prefill_params.pnum = pipe->num;
	prefill_params.is_ubwc = mdss_mdp_is_ubwc_format(pipe->src_fmt);
	prefill_params.is_nv12 = mdss_mdp_is_nv12_format(pipe->src_fmt);

	mdss_mdp_get_bw_vote_mode(mixer, mdata->mdp_rev, perf,
		PERF_CALC_VOTE_MODE_PER_PIPE, flags);

	if (flags & PERF_CALC_PIPE_SINGLE_LAYER)
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_single(&prefill_params);
	else if (!prefill_params.is_cmd)
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_video(&prefill_params);
	else
		perf->prefill_bytes =
			mdss_mdp_perf_calc_pipe_prefill_cmd(&prefill_params);

exit:
	pr_debug("mixer=%d pnum=%d clk_rate=%u bw_overlap=%llu bw_prefill=%llu (%d) %s\n",
		 mixer->num, pipe->num, perf->mdp_clk_rate, perf->bw_overlap,
		 perf->bw_prefill, perf->prefill_bytes, mdata->disable_prefill ?
		 "prefill is disabled" : "");

	return 0;
}

static inline int mdss_mdp_perf_is_overlap(u32 y00, u32 y01, u32 y10, u32 y11)
{
	return (y10 < y00 && y11 >= y01) || (y10 >= y00 && y10 < y01);
}

static inline int cmpu32(const void *a, const void *b)
{
	return (*(u32 *)a < *(u32 *)b) ? -1 : 0;
}

static void mdss_mdp_perf_calc_mixer(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe **pipe_list, int num_pipes,
		u32 flags)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_panel_info *pinfo = NULL;
	int fps = DEFAULT_FRAME_RATE;
	u32 v_total = 0, bpp = MDSS_MDP_WB_OUTPUT_BPP;
	u32 h_total = 0;
	int i;
	u32 max_clk_rate = 0;
	u64 bw_overlap_max = 0;
	u64 bw_overlap[MAX_PIPES_PER_LM] = { 0 };
	u64 bw_overlap_async = 0;
	u32 v_region[MAX_PIPES_PER_LM * 2] = { 0 };
	u32 prefill_val = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool apply_fudge = true;
	struct mdss_mdp_format_params *fmt = NULL;

	BUG_ON(num_pipes > MAX_PIPES_PER_LM);

	memset(perf, 0, sizeof(*perf));

	if (!mixer->rotator_mode) {
		pinfo = &mixer->ctl->panel_data->panel_info;
		if (!pinfo) {
			pr_err("pinfo is NULL\n");
			goto exit;
		}

		if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
			if (pinfo->type == MIPI_VIDEO_PANEL) {
				fps = pinfo->panel_max_fps;
				v_total = pinfo->panel_max_vtotal;
			} else {
				fps = mdss_panel_get_framerate(pinfo);
				v_total = mdss_panel_get_vtotal(pinfo);
			}
			if (is_dest_scaling_enable(mixer))
				h_total = get_ds_output_width(mixer);
			else
				h_total = mixer->width;
		} else {
			v_total = mixer->height;
		}

		/* For writeback panel, mixer type can be other than intf */
		if (pinfo->type == WRITEBACK_PANEL) {
			fmt = mdss_mdp_get_format_params(
				mixer->ctl->dst_format);
			if (fmt)
				bpp = fmt->bpp;
			pinfo = NULL;
		}

		/*
		 * with destination scaling, the increase of clock
		 * calculation should depends on output of size of DS setting.
		 */
		perf->mdp_clk_rate = h_total * v_total * fps;
		perf->mdp_clk_rate =
			mdss_mdp_clk_fudge_factor(mixer, perf->mdp_clk_rate);

		if (!pinfo) { /* perf for bus writeback */
			perf->bw_writeback =
				fps * mixer->width * mixer->height * bpp;

			if (test_bit(MDSS_QOS_OVERHEAD_FACTOR,
					mdata->mdss_qos_map))
				perf->bw_writeback = apply_comp_ratio_factor(
						perf->bw_writeback, fmt,
						&mixer->ctl->dst_comp_ratio);

		} else if (pinfo->type == MIPI_CMD_PANEL) {
			u32 dsi_transfer_rate = mixer->width * v_total;

			/* adjust transfer time from micro seconds */
			dsi_transfer_rate = mult_frac(dsi_transfer_rate,
				1000000, pinfo->mdp_transfer_time_us);

			if (dsi_transfer_rate > perf->mdp_clk_rate)
				perf->mdp_clk_rate = dsi_transfer_rate;
		}

		if (is_dsc_compression(pinfo) &&
		    mixer->ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE)
			perf->mdp_clk_rate *= 2;
	}

	/*
	 * In case of border color, we still need enough mdp clock
	 * to avoid under-run. Clock requirement for border color is
	 * based on mixer width.
	 */
	if (num_pipes == 0)
		goto exit;

	memset(bw_overlap, 0, sizeof(u64) * MAX_PIPES_PER_LM);
	memset(v_region, 0, sizeof(u32) * MAX_PIPES_PER_LM * 2);

	/*
	* Apply this logic only for 8x26 to reduce clock rate
	* for single video playback use case
	*/
	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_101)
		 && mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		u32 npipes = 0;
		for (i = 0; i < num_pipes; i++) {
			pipe = pipe_list[i];
			if (pipe) {
				if (npipes) {
					apply_fudge = true;
					break;
				}
				npipes++;
				apply_fudge = !(pipe->src_fmt->is_yuv)
					|| !(pipe->flags
					& MDP_SOURCE_ROTATED_90);
			}
		}
	}

	if (apply_fudge)
		flags |= PERF_CALC_PIPE_APPLY_CLK_FUDGE;
	if (num_pipes == 1)
		flags |= PERF_CALC_PIPE_SINGLE_LAYER;

	for (i = 0; i < num_pipes; i++) {
		struct mdss_mdp_perf_params tmp;

		memset(&tmp, 0, sizeof(tmp));

		pipe = pipe_list[i];
		if (pipe == NULL)
			continue;

		/*
		 * if is pipe used across two LMs in source split configuration
		 * then it is staged on both LMs. In such cases skip BW calc
		 * for such pipe on right LM to prevent adding BW twice.
		 */
		if (pipe->src_split_req && mixer->is_right_mixer)
			continue;

		if (mdss_mdp_perf_calc_pipe(pipe, &tmp, &mixer->roi,
			flags))
			continue;

		if (!mdss_mdp_is_nrt_ctl_path(mixer->ctl)) {
			u64 per_pipe_ib =
			    test_bit(MDSS_QOS_IB_NOCR, mdata->mdss_qos_map) ?
			    tmp.bw_overlap_nocr : tmp.bw_overlap;

			perf->max_per_pipe_ib = max(perf->max_per_pipe_ib,
			    per_pipe_ib);
		}

		bitmap_or(perf->bw_vote_mode, perf->bw_vote_mode,
			tmp.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

		/*
		 * for async layers, the overlap calculation is skipped
		 * and the bandwidth is added at the end, accounting for
		 * worst case, that async layer might overlap with
		 * all the other layers.
		 */
		if (pipe->async_update) {
			bw_overlap[i] = 0;
			v_region[2*i] = 0;
			v_region[2*i + 1] = 0;
			bw_overlap_async += tmp.bw_overlap;
		} else {
			bw_overlap[i] = tmp.bw_overlap;
			v_region[2*i] = pipe->dst.y;
			v_region[2*i + 1] = pipe->dst.y + pipe->dst.h;
		}

		if (tmp.mdp_clk_rate > max_clk_rate)
			max_clk_rate = tmp.mdp_clk_rate;

		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
			prefill_val += tmp.bw_prefill;
		else
			prefill_val += tmp.prefill_bytes;
	}

	/*
	 * Sort the v_region array so the total display area can be
	 * divided into individual regions. Check how many pipes fetch
	 * data for each region and sum them up, then the worst case
	 * of all regions is ib request.
	 */
	sort(v_region, num_pipes * 2, sizeof(u32), cmpu32, NULL);
	for (i = 1; i < num_pipes * 2; i++) {
		int j;
		u64 bw_max_region = 0;
		u32 y0, y1;
		pr_debug("v_region[%d]%d\n", i, v_region[i]);
		if (v_region[i] == v_region[i-1])
			continue;
		y0 = v_region[i-1];
		y1 = v_region[i];
		for (j = 0; j < num_pipes; j++) {
			if (!bw_overlap[j])
				continue;
			pipe = pipe_list[j];
			if (mdss_mdp_perf_is_overlap(y0, y1, pipe->dst.y,
				(pipe->dst.y + pipe->dst.h)))
				bw_max_region += bw_overlap[j];
			pr_debug("pipe%d rect%d: v[%d](%d,%d)pipe[%d](%d,%d)bw(%llu %llu)\n",
				pipe->num, pipe->multirect.num,
				i, y0, y1, j, pipe->dst.y,
				pipe->dst.y + pipe->dst.h, bw_overlap[j],
				bw_max_region);
		}
		bw_overlap_max = max(bw_overlap_max, bw_max_region);
	}

	perf->bw_overlap += bw_overlap_max + bw_overlap_async;

	if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
		perf->bw_prefill += prefill_val;
	else
		perf->prefill_bytes += prefill_val;

	if (max_clk_rate > perf->mdp_clk_rate)
		perf->mdp_clk_rate = max_clk_rate;

exit:
	pr_debug("final mixer=%d video=%d clk_rate=%u bw=%llu prefill=%d mode=0x%lx\n",
		mixer->num, mixer->ctl->is_video_mode, perf->mdp_clk_rate,
		perf->bw_overlap, prefill_val,
		*(perf->bw_vote_mode));
}

static bool is_mdp_prefetch_needed(struct mdss_panel_info *pinfo)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool enable_prefetch = false;

	if (mdata->mdp_rev >= MDSS_MDP_HW_REV_105) {
		if ((pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width +
			pinfo->lcdc.v_front_porch) < mdata->min_prefill_lines)
			pr_warn_once("low vbp+vfp may lead to perf issues in some cases\n");

		enable_prefetch = true;

		if ((pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width) >=
				MDSS_MDP_MAX_PREFILL_FETCH)
			enable_prefetch = false;
	} else {
		if ((pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width) <
				mdata->min_prefill_lines)
			pr_warn_once("low vbp may lead to display performance issues");
	}

	return enable_prefetch;
}

/**
 * mdss_mdp_get_prefetch_lines: - Number of fetch lines in vertical front porch
 * @pinfo: Pointer to the panel information.
 *
 * Returns the number of fetch lines in vertical front porch at which mdp
 * can start fetching the next frame.
 *
 * In some cases, vertical front porch is too high. In such cases limit
 * the mdp fetch lines  as the last (25 - vbp - vpw) lines of vertical
 * front porch.
 */
int mdss_mdp_get_prefetch_lines(struct mdss_panel_info *pinfo)
{
	int prefetch_avail = 0;
	int v_total, vfp_start;
	u32 prefetch_needed;

	if (!is_mdp_prefetch_needed(pinfo))
		return 0;

	v_total = mdss_panel_get_vtotal(pinfo);
	vfp_start = (pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width +
			pinfo->yres);

	prefetch_avail = v_total - vfp_start;
	prefetch_needed = MDSS_MDP_MAX_PREFILL_FETCH -
		pinfo->lcdc.v_back_porch -
		pinfo->lcdc.v_pulse_width;

	if (prefetch_avail > prefetch_needed)
		prefetch_avail = prefetch_needed;

	return prefetch_avail;
}

static bool mdss_mdp_video_mode_intf_connected(struct mdss_mdp_ctl *ctl)
{
	int i;
	struct mdss_data_type *mdata;

	if (!ctl || !ctl->mdata)
		return 0;

	mdata = ctl->mdata;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl = mdata->ctl_off + i;

		if (ctl->is_video_mode && mdss_mdp_ctl_is_power_on(ctl)) {
			pr_debug("video interface connected ctl:%d\n",
				ctl->num);
			return true;
		}
	}

	return false;
}

static void __mdss_mdp_perf_calc_ctl_helper(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt,
		u32 flags)
{
	struct mdss_mdp_perf_params tmp;
	struct mdss_data_type *mdata = ctl->mdata;

	memset(perf, 0, sizeof(*perf));

	if (ctl->mixer_left) {
		mdss_mdp_perf_calc_mixer(ctl->mixer_left, &tmp,
				left_plist, left_cnt, flags);

		bitmap_or(perf->bw_vote_mode, perf->bw_vote_mode,
			tmp.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

		perf->max_per_pipe_ib = tmp.max_per_pipe_ib;
		perf->bw_overlap += tmp.bw_overlap;
		perf->mdp_clk_rate = tmp.mdp_clk_rate;
		perf->bw_writeback += tmp.bw_writeback;

		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
			perf->bw_prefill += tmp.bw_prefill;
		else
			perf->prefill_bytes += tmp.prefill_bytes;
	}

	if (ctl->mixer_right) {
		mdss_mdp_perf_calc_mixer(ctl->mixer_right, &tmp,
				right_plist, right_cnt, flags);

		bitmap_or(perf->bw_vote_mode, perf->bw_vote_mode,
			tmp.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

		perf->max_per_pipe_ib = max(perf->max_per_pipe_ib,
			tmp.max_per_pipe_ib);
		perf->bw_overlap += tmp.bw_overlap;
		perf->bw_writeback += tmp.bw_writeback;
		if (tmp.mdp_clk_rate > perf->mdp_clk_rate)
			perf->mdp_clk_rate = tmp.mdp_clk_rate;

		if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map))
			perf->bw_prefill += tmp.bw_prefill;
		else
			perf->prefill_bytes += tmp.prefill_bytes;

		if (ctl->intf_type) {
			u64 clk_rate = mdss_mdp_get_pclk_rate(ctl);
			/* minimum clock rate due to inefficiency in 3dmux */
			clk_rate = DIV_ROUND_UP_ULL((clk_rate >> 1) * 9, 8);
			if (clk_rate > perf->mdp_clk_rate)
				perf->mdp_clk_rate = clk_rate;
		}
	}

	/* request minimum bandwidth to have bus clock on when display is on */
	if (perf->bw_overlap == 0)
		perf->bw_overlap = SZ_16M;

	if (!test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map) &&
		(ctl->intf_type != MDSS_MDP_NO_INTF)) {
		u32 vbp_fac = mdss_mdp_get_vbp_factor_max(ctl);

		perf->bw_prefill = perf->prefill_bytes;
		/*
		 * Prefill bandwidth equals the amount of data (number
		 * of prefill_bytes) divided by the the amount time
		 * available (blanking period). It is equivalent that
		 * prefill bytes times a factor in unit Hz, which is
		 * the reciprocal of time.
		 */
		perf->bw_prefill *= vbp_fac;
	}

	perf->bw_ctl = max(perf->bw_prefill, perf->bw_overlap);
	pr_debug("ctl=%d prefill bw=%llu overlap bw=%llu mode=0x%lx writeback:%llu\n",
			ctl->num, perf->bw_prefill, perf->bw_overlap,
			*(perf->bw_vote_mode), perf->bw_writeback);
}

static u32 mdss_check_for_flip(struct mdss_mdp_ctl *ctl)
{
	u32 i, panel_orientation;
	struct mdss_mdp_pipe *pipe;
	u32 flags = 0;

	panel_orientation = ctl->mfd->panel_orientation;
	if (panel_orientation & MDP_FLIP_LR)
		flags |= MDSS_MAX_BW_LIMIT_HFLIP;
	if (panel_orientation & MDP_FLIP_UD)
		flags |= MDSS_MAX_BW_LIMIT_VFLIP;

	for (i = 0; i < MAX_PIPES_PER_LM; i++) {
		if ((flags & MDSS_MAX_BW_LIMIT_HFLIP) &&
				(flags & MDSS_MAX_BW_LIMIT_VFLIP))
			return flags;

		if (ctl->mixer_left && ctl->mixer_left->stage_pipe[i]) {
			pipe = ctl->mixer_left->stage_pipe[i];
			if (pipe->flags & MDP_FLIP_LR)
				flags |= MDSS_MAX_BW_LIMIT_HFLIP;
			if (pipe->flags & MDP_FLIP_UD)
				flags |= MDSS_MAX_BW_LIMIT_VFLIP;
		}

		if (ctl->mixer_right && ctl->mixer_right->stage_pipe[i]) {
			pipe = ctl->mixer_right->stage_pipe[i];
			if (pipe->flags & MDP_FLIP_LR)
				flags |= MDSS_MAX_BW_LIMIT_HFLIP;
			if (pipe->flags & MDP_FLIP_UD)
				flags |= MDSS_MAX_BW_LIMIT_VFLIP;
		}
	}

	return flags;
}

static int mdss_mdp_set_threshold_max_bandwidth(struct mdss_mdp_ctl *ctl)
{
	u32 mode, threshold = 0, max = INT_MAX;
	u32 i = 0;
	struct mdss_max_bw_settings *max_bw_settings =
		ctl->mdata->max_bw_settings;

	if (!ctl->mdata->max_bw_settings_cnt && !ctl->mdata->max_bw_settings)
		return 0;

	mode = ctl->mdata->bw_mode_bitmap;

	if (!((mode & MDSS_MAX_BW_LIMIT_HFLIP) &&
				(mode & MDSS_MAX_BW_LIMIT_VFLIP)))
		mode |= mdss_check_for_flip(ctl);

	pr_debug("final mode = %d, bw_mode_bitmap = %d\n", mode,
			ctl->mdata->bw_mode_bitmap);

	/* Return minimum bandwidth limit */
	for (i = 0; i < ctl->mdata->max_bw_settings_cnt; i++) {
		if (max_bw_settings[i].mdss_max_bw_mode & mode) {
			threshold = max_bw_settings[i].mdss_max_bw_val;
			if (threshold < max)
				max = threshold;
		}
	}

	return max;
}

int mdss_mdp_perf_bw_check(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_pipe **left_plist, int left_cnt,
		struct mdss_mdp_pipe **right_plist, int right_cnt)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_perf_params perf;
	u32 bw, threshold, i, mode_switch, max_bw;
	u64 bw_sum_of_intfs = 0;
	bool is_video_mode;

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (ctl->intf_type == MDSS_MDP_NO_INTF)
		return 0;

	__mdss_mdp_perf_calc_ctl_helper(ctl, &perf,
			left_plist, left_cnt, right_plist, right_cnt,
			PERF_CALC_PIPE_CALC_SMP_SIZE);
	ctl->bw_pending = perf.bw_ctl;

	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *temp = mdata->ctl_off + i;
		if (temp->power_state == MDSS_PANEL_POWER_ON  &&
				(temp->intf_type != MDSS_MDP_NO_INTF))
			bw_sum_of_intfs += temp->bw_pending;
	}

	/* convert bandwidth to kb */
	bw = DIV_ROUND_UP_ULL(bw_sum_of_intfs, 1000);
	pr_debug("calculated bandwidth=%uk\n", bw);

	/* mfd validation happens in func */
	mode_switch = mdss_fb_get_mode_switch(ctl->mfd);
	if (mode_switch)
		is_video_mode = (mode_switch == MIPI_VIDEO_PANEL);
	else
		is_video_mode = ctl->is_video_mode;
	threshold = (is_video_mode ||
		mdss_mdp_video_mode_intf_connected(ctl)) ?
		mdata->max_bw_low : mdata->max_bw_high;

	max_bw = mdss_mdp_set_threshold_max_bandwidth(ctl);

	if (max_bw && (max_bw < threshold))
		threshold = max_bw;

	pr_debug("final threshold bw limit = %d\n", threshold);

	if (bw > threshold) {
		ctl->bw_pending = 0;
		pr_debug("exceeds bandwidth: %ukb > %ukb\n", bw, threshold);
		return -E2BIG;
	}

	return 0;
}

static u32 mdss_mdp_get_max_pipe_bw(struct mdss_mdp_pipe *pipe)
{

	struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;
	struct mdss_max_bw_settings *max_per_pipe_bw_settings;
	u32 flags = 0, threshold = 0, panel_orientation;
	u32 i, max = INT_MAX;

	if (!ctl->mdata->mdss_per_pipe_bw_cnt
			&& !ctl->mdata->max_per_pipe_bw_settings)
		return 0;

	panel_orientation = ctl->mfd->panel_orientation;
	max_per_pipe_bw_settings = ctl->mdata->max_per_pipe_bw_settings;

	/* Check for panel orienatation */
	panel_orientation = ctl->mfd->panel_orientation;
	if (panel_orientation & MDP_FLIP_LR)
		flags |= MDSS_MAX_BW_LIMIT_HFLIP;
	if (panel_orientation & MDP_FLIP_UD)
		flags |= MDSS_MAX_BW_LIMIT_VFLIP;

	/* check for Hflip/Vflip in pipe */
	if (pipe->flags & MDP_FLIP_LR)
		flags |= MDSS_MAX_BW_LIMIT_HFLIP;
	if (pipe->flags & MDP_FLIP_UD)
		flags |= MDSS_MAX_BW_LIMIT_VFLIP;

	flags |= ctl->mdata->bw_mode_bitmap;

	for (i = 0; i < ctl->mdata->mdss_per_pipe_bw_cnt; i++) {
		if (max_per_pipe_bw_settings[i].mdss_max_bw_mode & flags) {
			threshold = max_per_pipe_bw_settings[i].mdss_max_bw_val;
			if (threshold < max)
				max = threshold;
		}
	}

	return max;
}

int mdss_mdp_perf_bw_check_pipe(struct mdss_mdp_perf_params *perf,
		struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = pipe->mixer_left->ctl->mdata;
	struct mdss_mdp_ctl *ctl = pipe->mixer_left->ctl;
	u32 vbp_fac = 0, threshold = 0;
	u64 prefill_bw, pipe_bw, max_pipe_bw;

	/* we only need bandwidth check on real-time clients (interfaces) */
	if (ctl->intf_type == MDSS_MDP_NO_INTF)
		return 0;

	if (test_bit(MDSS_QOS_SIMPLIFIED_PREFILL, mdata->mdss_qos_map)) {
		prefill_bw = perf->bw_prefill;
	} else {
		vbp_fac = mdss_mdp_get_vbp_factor_max(ctl);
		prefill_bw = perf->prefill_bytes * vbp_fac;
	}
	pipe_bw = max(prefill_bw, perf->bw_overlap);
	pr_debug("prefill=%llu, vbp_fac=%u, overlap=%llu\n",
			prefill_bw, vbp_fac, perf->bw_overlap);

	/* convert bandwidth to kb */
	pipe_bw = DIV_ROUND_UP_ULL(pipe_bw, 1000);

	threshold = mdata->max_bw_per_pipe;
	max_pipe_bw = mdss_mdp_get_max_pipe_bw(pipe);

	if (max_pipe_bw && (max_pipe_bw < threshold))
		threshold = max_pipe_bw;

	pr_debug("bw=%llu threshold=%u\n", pipe_bw, threshold);

	if (threshold && pipe_bw > threshold) {
		pr_debug("pipe exceeds bandwidth: %llukb > %ukb\n", pipe_bw,
				threshold);
		return -E2BIG;
	}

	return 0;
}

static void mdss_mdp_perf_calc_ctl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_perf_params *perf)
{
	struct mdss_mdp_pipe *left_plist[MAX_PIPES_PER_LM];
	struct mdss_mdp_pipe *right_plist[MAX_PIPES_PER_LM];
	int i, left_cnt = 0, right_cnt = 0;

	for (i = 0; i < MAX_PIPES_PER_LM; i++) {
		if (ctl->mixer_left && ctl->mixer_left->stage_pipe[i]) {
			left_plist[left_cnt] =
					ctl->mixer_left->stage_pipe[i];
			left_cnt++;
		}

		if (ctl->mixer_right && ctl->mixer_right->stage_pipe[i]) {
			right_plist[right_cnt] =
					ctl->mixer_right->stage_pipe[i];
			right_cnt++;
		}
	}

	__mdss_mdp_perf_calc_ctl_helper(ctl, perf,
		left_plist, left_cnt, right_plist, right_cnt, 0);

	if (ctl->is_video_mode || ((ctl->intf_type != MDSS_MDP_NO_INTF) &&
		mdss_mdp_video_mode_intf_connected(ctl))) {
		perf->bw_ctl =
			max(apply_fudge_factor(perf->bw_overlap,
				&mdss_res->ib_factor_overlap),
			apply_fudge_factor(perf->bw_prefill,
				&mdss_res->ib_factor));
		perf->bw_writeback = apply_fudge_factor(perf->bw_writeback,
				&mdss_res->ib_factor);
	}
	pr_debug("ctl=%d clk_rate=%u\n", ctl->num, perf->mdp_clk_rate);
	pr_debug("bw_overlap=%llu bw_prefill=%llu prefill_bytes=%d\n",
		 perf->bw_overlap, perf->bw_prefill, perf->prefill_bytes);
}

static void set_status(u32 *value, bool status, u32 bit_num)
{
	if (status)
		*value |= BIT(bit_num);
	else
		*value &= ~BIT(bit_num);
}

/**
 * @ mdss_mdp_ctl_perf_set_transaction_status() -
 *                             Set the status of the on-going operations
 *                             for the command mode panels.
 * @ctl - pointer to a ctl
 *
 * This function is called to set the status bit in the perf_transaction_status
 * according to the operation that it is on-going for the command mode
 * panels, where:
 *
 * PERF_SW_COMMIT_STATE:
 *           1 - If SW operation has been commited and bw
 *               has been requested (HW transaction have not started yet).
 *           0 - If there is no SW operation pending
 * PERF_HW_MDP_STATE:
 *           1 - If HW transaction is on-going
 *           0 - If there is no HW transaction on going (ping-pong interrupt
 *               has finished)
 * Only if both states are zero there are no pending operations and
 * BW could be released.
 * State can be queried calling "mdss_mdp_ctl_perf_get_transaction_status"
 */
void mdss_mdp_ctl_perf_set_transaction_status(struct mdss_mdp_ctl *ctl,
	enum mdss_mdp_perf_state_type component, bool new_status)
{
	u32  previous_transaction;
	bool previous_status;
	unsigned long flags;

	if (!ctl || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL))
		return;

	spin_lock_irqsave(&ctl->spin_lock, flags);

	previous_transaction = ctl->perf_transaction_status;
	previous_status = previous_transaction & BIT(component) ?
		PERF_STATUS_BUSY : PERF_STATUS_DONE;

	/*
	 * If we set "done" state when previous state was not "busy",
	 * we want to print a warning since maybe there is a state
	 * that we are not considering
	 */
	WARN((PERF_STATUS_DONE == new_status) &&
		(PERF_STATUS_BUSY != previous_status),
		"unexpected previous state for component: %d\n", component);

	set_status(&ctl->perf_transaction_status, new_status,
		(u32)component);

	pr_debug("ctl:%d component:%d previous:%d status:%d\n",
		ctl->num, component, previous_transaction,
		ctl->perf_transaction_status);
	pr_debug("ctl:%d new_status:%d prev_status:%d\n",
		ctl->num, new_status, previous_status);

	spin_unlock_irqrestore(&ctl->spin_lock, flags);
}

/**
 * @ mdss_mdp_ctl_perf_get_transaction_status() -
 *                             Get the status of the on-going operations
 *                             for the command mode panels.
 * @ctl - pointer to a ctl
 *
 * Return:
 * The status of the transactions for the command mode panels,
 * note that the bandwidth can be released only if all transaction
 * status bits are zero.
 */
u32 mdss_mdp_ctl_perf_get_transaction_status(struct mdss_mdp_ctl *ctl)
{
	unsigned long flags;
	u32 transaction_status;

	if (!ctl)
		return PERF_STATUS_BUSY;

	/*
	 * If Rotator mode and bandwidth has been released; return STATUS_DONE
	 * so the bandwidth is re-calculated.
	 */
	if (ctl->mixer_left && ctl->mixer_left->rotator_mode &&
		!ctl->perf_release_ctl_bw)
			return PERF_STATUS_DONE;

	/*
	 * If Video Mode or not valid data to determine the status, return busy
	 * status, so the bandwidth cannot be freed by the caller
	 */
	if (!ctl || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL)) {
		return PERF_STATUS_BUSY;
	}

	spin_lock_irqsave(&ctl->spin_lock, flags);
	transaction_status = ctl->perf_transaction_status;
	spin_unlock_irqrestore(&ctl->spin_lock, flags);
	pr_debug("ctl:%d status:%d\n", ctl->num,
		transaction_status);

	return transaction_status;
}

/**
 * @ mdss_mdp_ctl_perf_update_traffic_shaper_bw  -
 *				Apply BW fudge factor to rotator
 *				if mdp clock increased during
 *				rotation session.
 * @ctl - pointer to the controller
 * @mdp_clk - new mdp clock
 *
 * If mdp clock increased and traffic shaper is enabled, we need to
 * account for the additional bandwidth that will be requested by
 * the rotator when running at a higher clock, so we apply a fudge
 * factor proportional to the mdp clock increment.
 */
static void mdss_mdp_ctl_perf_update_traffic_shaper_bw(struct mdss_mdp_ctl *ctl,
		u32 mdp_clk)
{
	if ((mdp_clk > 0) && (mdp_clk > ctl->traffic_shaper_mdp_clk)) {
		ctl->cur_perf.bw_ctl = fudge_factor(ctl->cur_perf.bw_ctl,
			mdp_clk, ctl->traffic_shaper_mdp_clk);
		pr_debug("traffic shaper bw:%llu, clk: %d,  mdp_clk:%d\n",
			ctl->cur_perf.bw_ctl, ctl->traffic_shaper_mdp_clk,
				mdp_clk);
	}
}

static u64 mdss_mdp_ctl_calc_client_vote(struct mdss_data_type *mdata,
	struct mdss_mdp_perf_params *perf, bool nrt_client, u32 mdp_clk)
{
	u64 bw_sum_of_intfs = 0;
	int i;
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_perf_params perf_temp;

	bitmap_zero(perf_temp.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		mixer = ctl->mixer_left;
		if (mdss_mdp_ctl_is_power_on(ctl) &&
		    /* RealTime clients */
		    ((!nrt_client && ctl->mixer_left &&
			!ctl->mixer_left->rotator_mode) ||
		    /* Non-RealTime clients */
		    (nrt_client && mdss_mdp_is_nrt_ctl_path(ctl)))) {
			/* Skip rotation layers as bw calc by rot driver */
			if (ctl->mixer_left && ctl->mixer_left->rotator_mode)
				continue;
			/*
			 * If traffic shaper is enabled we must check
			 * if additional bandwidth is required.
			 */
			if (ctl->traffic_shaper_enabled)
				mdss_mdp_ctl_perf_update_traffic_shaper_bw
					(ctl, mdp_clk);

			mdss_mdp_get_bw_vote_mode(ctl, mdata->mdp_rev,
				&perf_temp, PERF_CALC_VOTE_MODE_CTL, 0);

			bitmap_or(perf_temp.bw_vote_mode,
				perf_temp.bw_vote_mode,
				ctl->cur_perf.bw_vote_mode,
				MDSS_MDP_BW_MODE_MAX);

			if (nrt_client && ctl->mixer_left &&
				!ctl->mixer_left->rotator_mode) {
				bw_sum_of_intfs += ctl->cur_perf.bw_writeback;
				continue;
			}

			perf->max_per_pipe_ib = max(perf->max_per_pipe_ib,
				ctl->cur_perf.max_per_pipe_ib);

			bw_sum_of_intfs += ctl->cur_perf.bw_ctl;

			pr_debug("ctl_num=%d bw=%llu mode=0x%lx\n", ctl->num,
				ctl->cur_perf.bw_ctl,
				*(ctl->cur_perf.bw_vote_mode));
		}
	}

	return bw_sum_of_intfs;
}

/* apply any adjustments to the ib quota */
static inline u64 __calc_bus_ib_quota(struct mdss_data_type *mdata,
	struct mdss_mdp_perf_params *perf, bool nrt_client, u64 bw_vote)
{
	u64 bus_ib_quota;

	if (test_bit(MDSS_QOS_PER_PIPE_IB, mdata->mdss_qos_map)) {
		if (!nrt_client)
			bus_ib_quota = perf->max_per_pipe_ib;
		else
			bus_ib_quota = 0;
	} else {
		bus_ib_quota = bw_vote;
	}

	if (test_bit(MDSS_MDP_BW_MODE_SINGLE_LAYER,
		perf->bw_vote_mode) &&
		(bus_ib_quota >= PERF_SINGLE_PIPE_BW_FLOOR)) {
		struct mult_factor ib_factor_vscaling;
		ib_factor_vscaling.numer = 2;
		ib_factor_vscaling.denom = 1;
		bus_ib_quota = apply_fudge_factor(bus_ib_quota,
			&ib_factor_vscaling);
	}

	if (test_bit(MDSS_QOS_PER_PIPE_IB, mdata->mdss_qos_map) &&
			!nrt_client)
		bus_ib_quota = apply_fudge_factor(bus_ib_quota,
			&mdata->per_pipe_ib_factor);

	return bus_ib_quota;
}

static void mdss_mdp_ctl_update_client_vote(struct mdss_data_type *mdata,
	struct mdss_mdp_perf_params *perf, bool nrt_client, u64 bw_vote)
{
	u64 bus_ab_quota, bus_ib_quota;

	bus_ab_quota = max(bw_vote, mdata->perf_tune.min_bus_vote);
	bus_ib_quota = __calc_bus_ib_quota(mdata, perf, nrt_client, bw_vote);


	bus_ab_quota = apply_fudge_factor(bus_ab_quota, &mdss_res->ab_factor);
	ATRACE_INT("bus_quota", bus_ib_quota);

	mdss_bus_scale_set_quota(nrt_client ? MDSS_MDP_NRT : MDSS_MDP_RT,
		bus_ab_quota, bus_ib_quota);
	pr_debug("client:%s ab=%llu ib=%llu\n", nrt_client ? "nrt" : "rt",
		bus_ab_quota, bus_ib_quota);
}

static void mdss_mdp_ctl_perf_update_bus(struct mdss_data_type *mdata,
	struct mdss_mdp_ctl *ctl, u32 mdp_clk)
{
	u64 bw_sum_of_rt_intfs = 0, bw_sum_of_nrt_intfs = 0;
	struct mdss_mdp_perf_params perf = {0};

	ATRACE_BEGIN(__func__);

	/*
	 * non-real time client
	 * 1. rotator path
	 * 2. writeback output path
	 */
	if (mdss_mdp_is_nrt_ctl_path(ctl)) {
		bitmap_zero(perf.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);
		bw_sum_of_nrt_intfs = mdss_mdp_ctl_calc_client_vote(mdata,
			&perf, true, mdp_clk);
		mdss_mdp_ctl_update_client_vote(mdata, &perf, true,
			bw_sum_of_nrt_intfs);
	}

	/*
	 * real time client
	 * 1. any realtime interface - primary or secondary interface
	 * 2. writeback input path
	 */
	if (!mdss_mdp_is_nrt_ctl_path(ctl) ||
		(ctl->intf_num ==  MDSS_MDP_NO_INTF)) {
		bitmap_zero(perf.bw_vote_mode, MDSS_MDP_BW_MODE_MAX);
		bw_sum_of_rt_intfs = mdss_mdp_ctl_calc_client_vote(mdata,
			&perf, false, mdp_clk);
		mdss_mdp_ctl_update_client_vote(mdata, &perf, false,
			bw_sum_of_rt_intfs);
	}

	ATRACE_END(__func__);
}

/**
 * @mdss_mdp_ctl_perf_release_bw() - request zero bandwidth
 * @ctl - pointer to a ctl
 *
 * Function checks a state variable for the ctl, if all pending commit
 * requests are done, meaning no more bandwidth is needed, release
 * bandwidth request.
 */
void mdss_mdp_ctl_perf_release_bw(struct mdss_mdp_ctl *ctl)
{
	int transaction_status;
	struct mdss_data_type *mdata;
	int i;

	/* only do this for command panel */
	if (!ctl || !ctl->mdata || !ctl->panel_data ||
		(ctl->panel_data->panel_info.type != MIPI_CMD_PANEL))
		return;

	mutex_lock(&mdss_mdp_ctl_lock);
	mdata = ctl->mdata;
	/*
	 * If video interface present, cmd panel bandwidth cannot be
	 * released.
	 */
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl_local = mdata->ctl_off + i;

		if (mdss_mdp_ctl_is_power_on(ctl_local) &&
			ctl_local->is_video_mode)
			goto exit;
	}

	transaction_status = mdss_mdp_ctl_perf_get_transaction_status(ctl);
	pr_debug("transaction_status=0x%x\n", transaction_status);

	/*Release the bandwidth only if there are no transactions pending*/
	if (!transaction_status && mdata->enable_bw_release) {
		/*
		 * for splitdisplay if release_bw is called using secondary
		 * then find the main ctl and release BW for main ctl because
		 * BW is always calculated/stored using main ctl.
		 */
		struct mdss_mdp_ctl *ctl_local =
			mdss_mdp_get_main_ctl(ctl) ? : ctl;

		trace_mdp_cmd_release_bw(ctl_local->num);
		ctl_local->cur_perf.bw_ctl = 0;
		ctl_local->new_perf.bw_ctl = 0;
		pr_debug("Release BW ctl=%d\n", ctl_local->num);
		mdss_mdp_ctl_perf_update_bus(mdata, ctl, 0);
	}
exit:
	mutex_unlock(&mdss_mdp_ctl_lock);
}

static int mdss_mdp_select_clk_lvl(struct mdss_data_type *mdata,
			u32 clk_rate)
{
	int i;
	for (i = 0; i < mdata->nclk_lvl; i++) {
		if (clk_rate > mdata->clock_levels[i]) {
			continue;
		} else {
			clk_rate = mdata->clock_levels[i];
			break;
		}
	}

	return clk_rate;
}

static void mdss_mdp_perf_release_ctl_bw(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_perf_params *perf)
{
	/* Set to zero controller bandwidth. */
	memset(perf, 0, sizeof(*perf));
	ctl->perf_release_ctl_bw = false;
}

u32 mdss_mdp_get_mdp_clk_rate(struct mdss_data_type *mdata)
{
	u32 clk_rate = 0;
	uint i;
	struct clk *clk = mdss_mdp_get_clk(MDSS_CLK_MDP_CORE);

	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl;
		ctl = mdata->ctl_off + i;
		if (mdss_mdp_ctl_is_power_on(ctl)) {
			clk_rate = max(ctl->cur_perf.mdp_clk_rate,
							clk_rate);
			clk_rate = clk_round_rate(clk, clk_rate);
		}
	}
	clk_rate  = mdss_mdp_select_clk_lvl(mdata, clk_rate);

	pr_debug("clk:%u nctl:%d\n", clk_rate, mdata->nctl);
	return clk_rate;
}

static bool is_traffic_shaper_enabled(struct mdss_data_type *mdata)
{
	uint i;
	for (i = 0; i < mdata->nctl; i++) {
		struct mdss_mdp_ctl *ctl;
		ctl = mdata->ctl_off + i;
		if (mdss_mdp_ctl_is_power_on(ctl))
			if (ctl->traffic_shaper_enabled)
				return true;
	}
	return false;
}

static bool __mdss_mdp_compare_bw(
	struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_perf_params *new_perf,
	struct mdss_mdp_perf_params *old_perf,
	bool params_changed,
	bool stop_req)
{
	struct mdss_data_type *mdata = ctl->mdata;
	bool is_nrt = mdss_mdp_is_nrt_ctl_path(ctl);
	u64 new_ib =
		__calc_bus_ib_quota(mdata, new_perf, is_nrt, new_perf->bw_ctl);
	u64 old_ib =
		__calc_bus_ib_quota(mdata, old_perf, is_nrt, old_perf->bw_ctl);
	u64 max_new_bw = max(new_perf->bw_ctl, new_ib);
	u64 max_old_bw = max(old_perf->bw_ctl, old_ib);
	bool update_bw = false;

	/*
	 * three cases for bus bandwidth update.
	 * 1. new bandwidth vote (ab or ib) or writeback output vote
	 *		are higher than current vote for update request.
	 * 2. new bandwidth vote or writeback output vote are
	 *		lower than current vote at end of commit or stop.
	 * 3. end of writeback/rotator session - last chance to
	 *		non-realtime remove vote.
	 */
	if ((params_changed && ((max_new_bw > max_old_bw) || /* ab and ib bw */
			(new_perf->bw_writeback > old_perf->bw_writeback))) ||
			(!params_changed && ((max_new_bw < max_old_bw) ||
			(new_perf->bw_writeback < old_perf->bw_writeback))) ||
			(stop_req && is_nrt))
		update_bw = true;

	trace_mdp_compare_bw(new_perf->bw_ctl, new_ib, new_perf->bw_writeback,
		max_new_bw, old_perf->bw_ctl, old_ib, old_perf->bw_writeback,
		max_old_bw, params_changed, update_bw);

	return update_bw;
}

static void mdss_mdp_ctl_perf_update(struct mdss_mdp_ctl *ctl,
		int params_changed, bool stop_req)
{
	struct mdss_mdp_perf_params *new, *old;
	int update_bus = 0, update_clk = 0;
	struct mdss_data_type *mdata;
	bool is_bw_released;
	u32 clk_rate = 0;

	if (!ctl || !ctl->mdata)
		return;
	ATRACE_BEGIN(__func__);
	mutex_lock(&mdss_mdp_ctl_lock);

	mdata = ctl->mdata;
	old = &ctl->cur_perf;
	new = &ctl->new_perf;

	/*
	 * We could have released the bandwidth if there were no transactions
	 * pending, so we want to re-calculate the bandwidth in this situation.
	 */
	is_bw_released = !mdss_mdp_ctl_perf_get_transaction_status(ctl);

	if (mdss_mdp_ctl_is_power_on(ctl)) {
		/* Skip perf update if ctl is used for rotation */
		if (ctl->mixer_left && ctl->mixer_left->rotator_mode)
			goto end;

		if (ctl->perf_release_ctl_bw &&
			mdata->enable_rotator_bw_release)
			mdss_mdp_perf_release_ctl_bw(ctl, new);
		else if (is_bw_released || params_changed)
			mdss_mdp_perf_calc_ctl(ctl, new);

		if (__mdss_mdp_compare_bw(ctl, new, old, params_changed,
				stop_req)) {

			pr_debug("c=%d p=%d new_bw=%llu,old_bw=%llu\n",
				ctl->num, params_changed, new->bw_ctl,
				old->bw_ctl);
			if (stop_req) {
				old->bw_writeback = 0;
				old->bw_ctl = 0;
				old->max_per_pipe_ib = 0;
			} else {
				old->bw_ctl = new->bw_ctl;
				old->max_per_pipe_ib = new->max_per_pipe_ib;
				old->bw_writeback = new->bw_writeback;
			}
			bitmap_copy(old->bw_vote_mode, new->bw_vote_mode,
				MDSS_MDP_BW_MODE_MAX);
			update_bus = 1;
		}

		/*
		 * If traffic shaper is enabled, we do not decrease the clock,
		 * otherwise we would increase traffic shaper latency. Clock
		 * would be decreased after traffic shaper is done.
		 */
		if ((params_changed && (new->mdp_clk_rate > old->mdp_clk_rate))
			 || (!params_changed &&
			 (new->mdp_clk_rate < old->mdp_clk_rate) &&
			(false == is_traffic_shaper_enabled(mdata)))) {
			old->mdp_clk_rate = new->mdp_clk_rate;
			update_clk = 1;
		}
	} else {
		memset(old, 0, sizeof(*old));
		memset(new, 0, sizeof(*new));
		update_bus = 1;
		update_clk = 1;
	}

	/*
	 * Calculate mdp clock before bandwidth calculation. If traffic shaper
	 * is enabled and clock increased, the bandwidth calculation can
	 * use the new clock for the rotator bw calculation.
	 */
	if (update_clk)
		clk_rate = mdss_mdp_get_mdp_clk_rate(mdata);

	if (update_bus)
		mdss_mdp_ctl_perf_update_bus(mdata, ctl, clk_rate);

	/*
	 * Update the clock after bandwidth vote to ensure
	 * bandwidth is available before clock rate is increased.
	 */
	if (update_clk) {
		ATRACE_INT("mdp_clk", clk_rate);
		mdss_mdp_set_clk_rate(clk_rate);
		pr_debug("update clk rate = %d HZ\n", clk_rate);
	}

end:
	mutex_unlock(&mdss_mdp_ctl_lock);
	ATRACE_END(__func__);
}

struct mdss_mdp_ctl *mdss_mdp_ctl_alloc(struct mdss_data_type *mdata,
					       u32 off)
{
	struct mdss_mdp_ctl *ctl = NULL;
	u32 cnum;
	u32 nctl = mdata->nctl;

	mutex_lock(&mdss_mdp_ctl_lock);
	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED)
		nctl++;

	for (cnum = off; cnum < nctl; cnum++) {
		ctl = mdata->ctl_off + cnum;
		if (ctl->ref_cnt == 0) {
			ctl->ref_cnt++;
			ctl->mdata = mdata;
			mutex_init(&ctl->lock);
			mutex_init(&ctl->offlock);
			mutex_init(&ctl->flush_lock);
			mutex_init(&ctl->rsrc_lock);
			spin_lock_init(&ctl->spin_lock);
			BLOCKING_INIT_NOTIFIER_HEAD(&ctl->notifier_head);
			pr_debug("alloc ctl_num=%d\n", ctl->num);
			break;
		}
		ctl = NULL;
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return ctl;
}

int mdss_mdp_ctl_free(struct mdss_mdp_ctl *ctl)
{
	if (!ctl)
		return -ENODEV;

	pr_debug("free ctl_num=%d ref_cnt=%d\n", ctl->num, ctl->ref_cnt);

	if (!ctl->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	if (ctl->mixer_left && ctl->mixer_left->ref_cnt)
		mdss_mdp_mixer_free(ctl->mixer_left);

	if (ctl->mixer_right && ctl->mixer_right->ref_cnt)
		mdss_mdp_mixer_free(ctl->mixer_right);

	if (ctl->wb)
		mdss_mdp_wb_free(ctl->wb);

	mutex_lock(&mdss_mdp_ctl_lock);
	ctl->ref_cnt--;
	ctl->intf_num = MDSS_MDP_NO_INTF;
	ctl->intf_type = MDSS_MDP_NO_INTF;
	ctl->is_secure = false;
	ctl->power_state = MDSS_PANEL_POWER_OFF;
	ctl->mixer_left = NULL;
	ctl->mixer_right = NULL;
	ctl->wb = NULL;
	ctl->cdm = NULL;
	memset(&ctl->ops, 0, sizeof(ctl->ops));
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

/**
 * mdss_mdp_mixer_alloc() - allocate mdp mixer.
 * @ctl: mdp controller.
 * @type: specifying type of mixer requested. interface or writeback.
 * @mux: specifies if mixer allocation is for split_fb cases.
 * @rotator: specifies if the mixer requested for rotator operations.
 *
 * This function is called to request allocation of mdp mixer
 * during mdp controller path setup.
 *
 * Return: mdp mixer structure that is allocated.
 *	   NULL if mixer allocation fails.
 */
struct mdss_mdp_mixer *mdss_mdp_mixer_alloc(
		struct mdss_mdp_ctl *ctl, u32 type, int mux, int rotator)
{
	struct mdss_mdp_mixer *mixer = NULL, *alt_mixer = NULL;
	u32 nmixers_intf;
	u32 nmixers_wb;
	u32 i;
	u32 nmixers;
	struct mdss_mdp_mixer *mixer_pool = NULL;

	if (!ctl || !ctl->mdata)
		return NULL;

	mutex_lock(&mdss_mdp_ctl_lock);
	nmixers_intf = ctl->mdata->nmixers_intf;
	nmixers_wb = ctl->mdata->nmixers_wb;

	switch (type) {
	case MDSS_MDP_MIXER_TYPE_INTF:
		mixer_pool = ctl->mdata->mixer_intf;
		nmixers = nmixers_intf;

		/*
		 * try to reserve first layer mixer for write back if
		 * assertive display needs to be supported through wfd
		 */
		if (ctl->mdata->has_wb_ad && ctl->intf_num &&
			((ctl->panel_data->panel_info.type != MIPI_CMD_PANEL) ||
			!mux)) {
			alt_mixer = mixer_pool;
			mixer_pool++;
			nmixers--;
		} else if ((ctl->panel_data->panel_info.type == WRITEBACK_PANEL)
			&& (ctl->mdata->ndspp < nmixers)) {
			mixer_pool += ctl->mdata->ndspp;
			nmixers -= ctl->mdata->ndspp;
		}
		break;

	case MDSS_MDP_MIXER_TYPE_WRITEBACK:
		mixer_pool = ctl->mdata->mixer_wb;
		nmixers = nmixers_wb;
		if ((ctl->mdata->wfd_mode == MDSS_MDP_WFD_DEDICATED) && rotator)
			mixer_pool = mixer_pool + nmixers;
		break;

	default:
		nmixers = 0;
		pr_err("invalid pipe type %d\n", type);
		break;
	}

	/*Allocate virtual wb mixer if no dedicated wfd wb blk is present*/
	if ((ctl->mdata->wfd_mode == MDSS_MDP_WFD_SHARED) &&
			(type == MDSS_MDP_MIXER_TYPE_WRITEBACK))
		nmixers += 1;

	for (i = 0; i < nmixers; i++) {
		mixer = mixer_pool + i;
		if (mixer->ref_cnt == 0)
			break;
		mixer = NULL;
	}

	if (!mixer && alt_mixer && (alt_mixer->ref_cnt == 0))
		mixer = alt_mixer;

	if (mixer) {
		mixer->ref_cnt++;
		mixer->params_changed++;
		mixer->ctl = ctl;
		mixer->next_pipe_map = 0;
		mixer->pipe_mapped = 0;
		pr_debug("alloc mixer num %d for ctl=%d\n",
				mixer->num, ctl->num);
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return mixer;
}

struct mdss_mdp_mixer *mdss_mdp_mixer_assign(u32 id, bool wb, bool rot)
{
	struct mdss_mdp_mixer *mixer = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	mutex_lock(&mdss_mdp_ctl_lock);

	if (rot && (mdata->wfd_mode == MDSS_MDP_WFD_DEDICATED))
		mixer = mdata->mixer_wb + mdata->nmixers_wb;
	else if (wb && id < mdata->nmixers_wb)
		mixer = mdata->mixer_wb + id;
	else if (!wb && id < mdata->nmixers_intf)
		mixer = mdata->mixer_intf + id;

	if (mixer && mixer->ref_cnt == 0) {
		mixer->ref_cnt++;
		mixer->params_changed++;
	} else {
		pr_err("mixer is in use already = %d\n", id);
		mixer = NULL;
	}
	mutex_unlock(&mdss_mdp_ctl_lock);
	return mixer;
}

int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer)
{
	if (!mixer)
		return -ENODEV;

	pr_debug("free mixer_num=%d ref_cnt=%d\n", mixer->num, mixer->ref_cnt);

	if (!mixer->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	mutex_lock(&mdss_mdp_ctl_lock);
	mixer->ref_cnt--;
	mixer->is_right_mixer = false;
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_block_mixer_alloc(void)
{
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_mixer *mixer = NULL;
	struct mdss_mdp_writeback *wb = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 offset = mdss_mdp_get_wb_ctl_support(mdata, true);
	int ret = 0;

	ctl = mdss_mdp_ctl_alloc(mdss_res, offset);
	if (!ctl) {
		pr_debug("unable to allocate wb ctl\n");
		return NULL;
	}

	mixer = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_WRITEBACK,
							false, true);
	if (!mixer) {
		pr_debug("unable to allocate wb mixer\n");
		goto error;
	}

	mixer->rotator_mode = 1;

	switch (mixer->num) {
	case MDSS_MDP_WB_LAYERMIXER0:
		ctl->opmode = MDSS_MDP_CTL_OP_ROT0_MODE;
		break;
	case MDSS_MDP_WB_LAYERMIXER1:
		ctl->opmode = MDSS_MDP_CTL_OP_ROT1_MODE;
		break;
	default:
		pr_err("invalid layer mixer=%d\n", mixer->num);
		goto error;
	}

	wb = mdss_mdp_wb_alloc(MDSS_MDP_WB_ROTATOR, ctl->num);
	if (!wb) {
		pr_err("Unable to allocate writeback block\n");
		goto error;
	}

	ctl->mixer_left = mixer;

	ctl->ops.start_fnc = mdss_mdp_writeback_start;
	ctl->power_state = MDSS_PANEL_POWER_ON;
	ctl->wb_type = MDSS_MDP_WB_CTL_TYPE_BLOCK;
	mixer->ctl = ctl;
	ctl->wb = wb;

	if (ctl->ops.start_fnc)
		ret = ctl->ops.start_fnc(ctl);

	if (!ret)
		return mixer;
error:
	if (wb)
		mdss_mdp_wb_free(wb);
	if (mixer)
		mdss_mdp_mixer_free(mixer);
	if (ctl)
		mdss_mdp_ctl_free(ctl);

	return NULL;
}

int mdss_mdp_block_mixer_destroy(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_ctl *ctl;

	if (!mixer || !mixer->ctl) {
		pr_err("invalid ctl handle\n");
		return -ENODEV;
	}

	ctl = mixer->ctl;
	mixer->rotator_mode = 0;

	pr_debug("destroy ctl=%d mixer=%d\n", ctl->num, mixer->num);

	if (ctl->ops.stop_fnc)
		ctl->ops.stop_fnc(ctl, MDSS_PANEL_POWER_OFF);

	mdss_mdp_ctl_free(ctl);

	mdss_mdp_ctl_perf_update(ctl, 0, true);

	return 0;
}

int mdss_mdp_ctl_cmd_set_autorefresh(struct mdss_mdp_ctl *ctl, int frame_cnt)
{
	int ret = 0;

	if (ctl->panel_data->panel_info.type == MIPI_CMD_PANEL) {
		ret = mdss_mdp_cmd_set_autorefresh_mode(ctl, frame_cnt);
	} else {
		pr_err("Mode not supported for this panel\n");
		ret = -EINVAL;
	}

	return ret;
}

int mdss_mdp_ctl_cmd_get_autorefresh(struct mdss_mdp_ctl *ctl)
{
	if (ctl->panel_data->panel_info.type == MIPI_CMD_PANEL)
		return mdss_mdp_cmd_get_autorefresh_mode(ctl);
	else
		return 0;
}

int mdss_mdp_ctl_splash_finish(struct mdss_mdp_ctl *ctl, bool handoff)
{
	switch (ctl->panel_data->panel_info.type) {
	case MIPI_VIDEO_PANEL:
	case DP_PANEL:
	case DTV_PANEL:
		return mdss_mdp_video_reconfigure_splash_done(ctl, handoff);
	case MIPI_CMD_PANEL:
		return mdss_mdp_cmd_reconfigure_splash_done(ctl, handoff);
	default:
		return 0;
	}
}

static inline int mdss_mdp_set_split_ctl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_ctl *split_ctl)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_panel_info *pinfo;


	if (!ctl || !split_ctl || !mdata)
		return -ENODEV;

	/* setup split ctl mixer as right mixer of original ctl so that
	 * original ctl can work the same way as dual pipe solution */
	ctl->mixer_right = split_ctl->mixer_left;
	pinfo = &ctl->panel_data->panel_info;

	/* add x offset from left ctl's border */
	split_ctl->border_x_off += (pinfo->lcdc.border_left +
					pinfo->lcdc.border_right);

	return 0;
}

static inline void __dsc_enable(struct mdss_mdp_mixer *mixer)
{
	mdss_mdp_pingpong_write(mixer->pingpong_base,
			MDSS_MDP_REG_PP_DSC_MODE, 1);
}

static inline void __dsc_disable(struct mdss_mdp_mixer *mixer)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	char __iomem *offset = mdata->mdp_base;

	mdss_mdp_pingpong_write(mixer->pingpong_base,
			MDSS_MDP_REG_PP_DSC_MODE, 0);

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0) {
		offset += MDSS_MDP_DSC_0_OFFSET;
	} else if (mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		offset += MDSS_MDP_DSC_1_OFFSET;
	} else {
		pr_err("invalid mixer numer=%d\n", mixer->num);
		return;
	}
	writel_relaxed(0, offset + MDSS_MDP_REG_DSC_COMMON_MODE);
}

static void __dsc_config(struct mdss_mdp_mixer *mixer,
	struct dsc_desc *dsc, u32 mode, bool ich_reset_override)
{
	u32 data;
	int bpp, lsb;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	char __iomem *offset = mdata->mdp_base;
	u32 initial_lines = dsc->initial_lines;
	bool is_cmd_mode = !(mode & BIT(2));

	data = mdss_mdp_pingpong_read(mixer->pingpong_base,
			MDSS_MDP_REG_PP_DCE_DATA_OUT_SWAP);
	data |= BIT(18); /* endian flip */
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_DCE_DATA_OUT_SWAP, data);

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0) {
		offset += MDSS_MDP_DSC_0_OFFSET;
	} else if (mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		offset += MDSS_MDP_DSC_1_OFFSET;
	} else {
		pr_err("invalid mixer numer=%d\n", mixer->num);
		return;
	}

	writel_relaxed(mode, offset + MDSS_MDP_REG_DSC_COMMON_MODE);

	data = 0;
	if (ich_reset_override)
		data = 3 << 28;

	if (is_cmd_mode)
		initial_lines += 1;

	data |= (initial_lines << 20);
	data |= ((dsc->slice_last_group_size - 1) << 18);
	/* bpp is 6.4 format, 4 LSBs bits are for fractional part */
	lsb = dsc->bpp % 4;
	bpp = dsc->bpp / 4;
	bpp *= 4;	/* either 8 or 12 */
	bpp <<= 4;
	bpp |= lsb;
	data |= (bpp << 8);
	data |= (dsc->block_pred_enable << 7);
	data |= (dsc->line_buf_depth << 3);
	data |= (dsc->enable_422 << 2);
	data |= (dsc->convert_rgb << 1);
	data |= dsc->input_10_bits;

	pr_debug("%d %d %d %d %d %d %d %d %d, data=%x\n",
		ich_reset_override,
		initial_lines , dsc->slice_last_group_size,
		dsc->bpp, dsc->block_pred_enable, dsc->line_buf_depth,
		dsc->enable_422, dsc->convert_rgb, dsc->input_10_bits, data);

	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_ENC);

	data = dsc->pic_width << 16;
	data |= dsc->pic_height;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_PICTURE);

	data = dsc->slice_width << 16;
	data |= dsc->slice_height;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SLICE);

	data = dsc->chunk_size << 16;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_CHUNK_SIZE);

	pr_debug("mix%d pic_w=%d pic_h=%d, slice_w=%d slice_h=%d, chunk=%d\n",
		mixer->num, dsc->pic_width, dsc->pic_height,
		dsc->slice_width, dsc->slice_height, dsc->chunk_size);
	MDSS_XLOG(mixer->num, dsc->pic_width, dsc->pic_height,
		dsc->slice_width, dsc->slice_height, dsc->chunk_size);

	data = dsc->initial_dec_delay << 16;
	data |= dsc->initial_xmit_delay;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_DELAY);

	data = dsc->initial_scale_value;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SCALE_INITIAL);

	data = dsc->scale_decrement_interval;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SCALE_DEC_INTERVAL);

	data = dsc->scale_increment_interval;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_SCALE_INC_INTERVAL);

	data = dsc->first_line_bpg_offset;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_FIRST_LINE_BPG_OFFSET);

	data = dsc->nfl_bpg_offset << 16;
	data |= dsc->slice_bpg_offset;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_BPG_OFFSET);

	data = dsc->initial_offset << 16;
	data |= dsc->final_offset;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_DSC_OFFSET);

	data = dsc->det_thresh_flatness << 10;
	data |= dsc->max_qp_flatness << 5;
	data |= dsc->min_qp_flatness;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_FLATNESS);
	writel_relaxed(0x983, offset + MDSS_MDP_REG_DSC_FLATNESS);

	data = dsc->rc_model_size;	/* rate_buffer_size */
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_RC_MODEL_SIZE);

	data = dsc->tgt_offset_lo << 18;
	data |= dsc->tgt_offset_hi << 14;
	data |= dsc->quant_incr_limit1 << 9;
	data |= dsc->quant_incr_limit0 << 4;
	data |= dsc->edge_factor;
	writel_relaxed(data, offset + MDSS_MDP_REG_DSC_RC);
}

static void __dsc_config_thresh(struct mdss_mdp_mixer *mixer,
	struct dsc_desc *dsc)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	char __iomem *offset, *off;
	u32 *lp;
	char *cp;
	int i;

	offset = mdata->mdp_base;

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0) {
		offset += MDSS_MDP_DSC_0_OFFSET;
	} else if (mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		offset += MDSS_MDP_DSC_1_OFFSET;
	} else {
		pr_err("invalid mixer numer=%d\n", mixer->num);
		return;
	}

	lp = dsc->buf_thresh;
	off = offset + MDSS_MDP_REG_DSC_RC_BUF_THRESH;
	for (i = 0; i < 14; i++) {
		writel_relaxed(*lp++, off);
		off += 4;
	}

	cp = dsc->range_min_qp;
	off = offset + MDSS_MDP_REG_DSC_RANGE_MIN_QP;
	for (i = 0; i < 15; i++) {
		writel_relaxed(*cp++, off);
		off += 4;
	}

	cp = dsc->range_max_qp;
	off = offset + MDSS_MDP_REG_DSC_RANGE_MAX_QP;
	for (i = 0; i < 15; i++) {
		writel_relaxed(*cp++, off);
		off += 4;
	}

	cp = dsc->range_bpg_offset;
	off = offset + MDSS_MDP_REG_DSC_RANGE_BPG_OFFSET;
	for (i = 0; i < 15; i++) {
		writel_relaxed(*cp++, off);
		off += 4;
	}
}

static bool __is_dsc_merge_enabled(u32 common_mode)
{
	return common_mode & BIT(1);
}

static bool __dsc_is_3d_mux_enabled(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_info *pinfo)
{
	return ctl && is_dual_lm_single_display(ctl->mfd) &&
	       pinfo && (pinfo->dsc_enc_total == 1);
}

/* must be called from master ctl */
static u32 __dsc_get_common_mode(struct mdss_mdp_ctl *ctl, bool mux_3d)
{
	u32 common_mode = 0;

	if (ctl->is_video_mode)
		common_mode = BIT(2);

	if (mdss_mdp_is_both_lm_valid(ctl))
		common_mode |= BIT(0);

	if (is_dual_lm_single_display(ctl->mfd)) {
		if (mux_3d)
			common_mode &= ~BIT(0);
		else if (mdss_mdp_is_both_lm_valid(ctl)) /* dsc_merge */
			common_mode |= BIT(1);
	}

	return common_mode;
}

static void __dsc_get_pic_dim(struct mdss_mdp_mixer *mixer_l,
	struct mdss_mdp_mixer *mixer_r, u32 *pic_w, u32 *pic_h)
{
	struct mdss_data_type *mdata = NULL;
	bool valid_l = mixer_l && mixer_l->valid_roi;
	bool valid_r = mixer_r && mixer_r->valid_roi;

	*pic_w = 0;
	*pic_h = 0;

	if (valid_l) {
		mdata = mixer_l->ctl->mdata;
		if (test_bit(MDSS_CAPS_DEST_SCALER, mdata->mdss_caps_map) &&
				mixer_l->ds &&
				(mixer_l->ds->flags & DS_ENABLE)) {
			*pic_w = mixer_l->ds->scaler.dst_width;
			*pic_h = mixer_l->ds->scaler.dst_height;
		} else {
			*pic_w = mixer_l->roi.w;
			*pic_h = mixer_l->roi.h;
		}
	}

	if (valid_r) {
		mdata = mixer_r->ctl->mdata;
		if (test_bit(MDSS_CAPS_DEST_SCALER, mdata->mdss_caps_map) &&
				mixer_r->ds &&
				(mixer_r->ds->flags & DS_ENABLE)) {
			*pic_w += mixer_r->ds->scaler.dst_width;
			*pic_h = mixer_r->ds->scaler.dst_height;
		} else {
			*pic_w += mixer_r->roi.w;
			*pic_h = mixer_r->roi.h;
		}
	}
}

static bool __is_ich_reset_override_needed(bool pu_en, struct dsc_desc *dsc)
{
	/*
	 * As per the DSC spec, ICH_RESET can be either end of the slice line
	 * or at the end of the slice. HW internally generates ich_reset at
	 * end of the slice line if DSC_MERGE is used or encoder has two
	 * soft slices. However, if encoder has only 1 soft slice and DSC_MERGE
	 * is not used then it will generate ich_reset at the end of slice.
	 *
	 * Now as per the spec, during one PPS session, position where
	 * ich_reset is generated should not change. Now if full-screen frame
	 * has more than 1 soft slice then HW will automatically generate
	 * ich_reset at the end of slice_line. But for the same panel, if
	 * partial frame is enabled and only 1 encoder is used with 1 slice,
	 * then HW will generate ich_reset at end of the slice. This is a
	 * mismatch. Prevent this by overriding HW's decision.
	 */
	return pu_en && dsc && (dsc->full_frame_slices > 1) &&
	       (dsc->slice_width == dsc->pic_width);
}

static void __dsc_setup_dual_lm_single_display(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_info *pinfo)
{
	u32 pic_width = 0, pic_height = 0;
	u32 intf_ip_w, enc_ip_w, common_mode, this_frame_slices;
	bool valid_l, valid_r;
	bool enable_right_dsc;
	bool mux_3d, ich_reset_override;
	struct dsc_desc *dsc;
	struct mdss_mdp_mixer *mixer_l, *mixer_r;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!pinfo || !ctl || !ctl->is_master ||
	    !is_dual_lm_single_display(ctl->mfd))
		return;

	dsc = &pinfo->dsc;
	mixer_l = ctl->mixer_left;
	mixer_r = ctl->mixer_right;

	mux_3d = __dsc_is_3d_mux_enabled(ctl, pinfo);
	common_mode = __dsc_get_common_mode(ctl, mux_3d);
	__dsc_get_pic_dim(mixer_l, mixer_r, &pic_width, &pic_height);

	valid_l = mixer_l->valid_roi;
	valid_r = mixer_r->valid_roi;
	if (mdss_mdp_is_lm_swap_needed(mdata, ctl)) {
		valid_l = true;
		valid_r = false;
	}

	this_frame_slices = pic_width / dsc->slice_width;

	/* enable or disable pp_split + DSC_Merge based on partial update */
	if ((pinfo->partial_update_enabled) && !mux_3d &&
	    (dsc->full_frame_slices == 4) &&
	    (mdss_has_quirk(mdata, MDSS_QUIRK_DSC_2SLICE_PU_THRPUT))) {

		if (valid_l && valid_r) {
			/* left + right */
			pr_debug("full line (4 slices) or middle 2 slice partial update\n");
			writel_relaxed(0x0,
				mdata->mdp_base + mdata->ppb_ctl[0]);
			writel_relaxed(0x0,
				mdata->mdp_base + MDSS_MDP_REG_DCE_SEL);
		} else if (valid_l || valid_r) {
			/* left-only or right-only */
			if (this_frame_slices == 2) {
				pr_debug("2 slice parital update, use merge\n");

				/* tandem + merge */
				common_mode = BIT(1) | BIT(0);

				valid_r = true;
				valid_l = true;

				writel_relaxed(0x2 << 4, mdata->mdp_base +
					mdata->ppb_ctl[0]);
				writel_relaxed(BIT(0),
					mdata->mdp_base + MDSS_MDP_REG_DCE_SEL);
			} else {
				pr_debug("only one slice partial update\n");
				writel_relaxed(0x0, mdata->mdp_base +
					mdata->ppb_ctl[0]);
				writel_relaxed(0x0, mdata->mdp_base +
					MDSS_MDP_REG_DCE_SEL);
			}
		}
	} else {
		writel_relaxed(0x0, mdata->mdp_base + MDSS_MDP_REG_DCE_SEL);
	}

	mdss_panel_dsc_update_pic_dim(dsc, pic_width, pic_height);

	intf_ip_w = this_frame_slices * dsc->slice_width;
	mdss_panel_dsc_pclk_param_calc(dsc, intf_ip_w);

	enc_ip_w = intf_ip_w;
	/* if dsc_merge, both encoders work on same number of slices */
	if (__is_dsc_merge_enabled(common_mode))
		enc_ip_w /= 2;
	mdss_panel_dsc_initial_line_calc(dsc, enc_ip_w);

	/*
	 * __is_ich_reset_override_needed should be called only after
	 * updating pic dimension, mdss_panel_dsc_update_pic_dim.
	 */
	ich_reset_override = __is_ich_reset_override_needed(
					pinfo->partial_update_enabled, dsc);
	if (valid_l) {
		__dsc_config(mixer_l, dsc, common_mode, ich_reset_override);
		__dsc_config_thresh(mixer_l, dsc);
		__dsc_enable(mixer_l);
	} else {
		__dsc_disable(mixer_l);
	}

	enable_right_dsc = valid_r;
	if (mux_3d && valid_l)
		enable_right_dsc = false;

	if (enable_right_dsc) {
		__dsc_config(mixer_r, dsc, common_mode, ich_reset_override);
		__dsc_config_thresh(mixer_r, dsc);
		__dsc_enable(mixer_r);
	} else {
		__dsc_disable(mixer_r);
	}

	pr_debug("mix%d: valid_l=%d mix%d: valid_r=%d mode=%d, pic_dim:%dx%d mux_3d=%d intf_ip_w=%d enc_ip_w=%d ich_ovrd=%d\n",
		mixer_l->num, valid_l, mixer_r->num, valid_r,
		common_mode, pic_width, pic_height,
		mux_3d, intf_ip_w, enc_ip_w, ich_reset_override);

	MDSS_XLOG(mixer_l->num, valid_l, mixer_r->num, valid_r,
		  common_mode, pic_width, pic_height,
		  mux_3d, intf_ip_w, enc_ip_w, ich_reset_override);
}

static void __dsc_setup_dual_lm_dual_display(
	struct mdss_mdp_ctl *ctl, struct mdss_panel_info *pinfo,
	struct mdss_mdp_ctl *sctl, struct mdss_panel_info *spinfo)
{
	u32 pic_width = 0, pic_height = 0;
	u32 intf_ip_w, enc_ip_w, common_mode, this_frame_slices;
	bool valid_l, valid_r;
	bool ich_reset_override;
	struct dsc_desc *dsc_l, *dsc_r;
	struct mdss_mdp_mixer *mixer_l, *mixer_r;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!pinfo || !ctl || !sctl || !spinfo ||
	    !ctl->is_master || !ctl->mfd ||
	    (ctl->mfd->split_mode != MDP_DUAL_LM_DUAL_DISPLAY))
		return;

	dsc_l = &pinfo->dsc;
	dsc_r = &spinfo->dsc;

	mixer_l = ctl->mixer_left;
	mixer_r = ctl->mixer_right;

	common_mode = __dsc_get_common_mode(ctl, false);
	/*
	 * In this topology, both DSC use same pic dimension. So no need to
	 * maintain two separate local copies.
	 */
	__dsc_get_pic_dim(mixer_l, mixer_r, &pic_width, &pic_height);

	valid_l = mixer_l->valid_roi;
	valid_r = mixer_r->valid_roi;
	if (mdss_mdp_is_lm_swap_needed(mdata, ctl)) {
		valid_l = true;
		valid_r = false;
	}

	/*
	 * Since both DSC use same pic dimension, set same pic dimension
	 * to both DSC structures.
	 */
	mdss_panel_dsc_update_pic_dim(dsc_l, pic_width, pic_height);
	mdss_panel_dsc_update_pic_dim(dsc_r, pic_width, pic_height);

	this_frame_slices = pic_width / dsc_l->slice_width;
	intf_ip_w = this_frame_slices * dsc_l->slice_width;
	if (valid_l && valid_r)
		intf_ip_w /= 2;
	/*
	 * In this topology when both interfaces are active, they have same
	 * load so intf_ip_w will be same.
	 */
	mdss_panel_dsc_pclk_param_calc(dsc_l, intf_ip_w);
	mdss_panel_dsc_pclk_param_calc(dsc_r, intf_ip_w);

	/*
	 * In this topology, since there is no dsc_merge, uncompressed input
	 * to encoder and interface is same.
	 */
	enc_ip_w = intf_ip_w;
	mdss_panel_dsc_initial_line_calc(dsc_l, enc_ip_w);
	mdss_panel_dsc_initial_line_calc(dsc_r, enc_ip_w);

	/*
	 * __is_ich_reset_override_needed should be called only after
	 * updating pic dimension, mdss_panel_dsc_update_pic_dim.
	 */
	ich_reset_override = __is_ich_reset_override_needed(
					pinfo->partial_update_enabled, dsc_l);

	if (valid_l) {
		__dsc_config(mixer_l, dsc_l, common_mode, ich_reset_override);
		__dsc_config_thresh(mixer_l, dsc_l);
		__dsc_enable(mixer_l);
	} else {
		__dsc_disable(mixer_l);
	}

	if (valid_r) {
		__dsc_config(mixer_r, dsc_r, common_mode, ich_reset_override);
		__dsc_config_thresh(mixer_r, dsc_r);
		__dsc_enable(mixer_r);
	} else {
		__dsc_disable(mixer_r);
	}

	pr_debug("mix%d: valid_l=%d mix%d: valid_r=%d mode=%d, pic_dim:%dx%d intf_ip_w=%d enc_ip_w=%d ich_ovrd=%d\n",
		mixer_l->num, valid_l, mixer_r->num, valid_r,
		common_mode, pic_width, pic_height,
		intf_ip_w, enc_ip_w, ich_reset_override);

	MDSS_XLOG(mixer_l->num, valid_l, mixer_r->num, valid_r,
		  common_mode, pic_width, pic_height,
		  intf_ip_w, enc_ip_w, ich_reset_override);
}

static void __dsc_setup_single_lm_single_display(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_info *pinfo)
{
	u32 pic_width = 0, pic_height = 0;
	u32 intf_ip_w, enc_ip_w, common_mode, this_frame_slices;
	bool valid;
	bool ich_reset_override;
	struct dsc_desc *dsc;
	struct mdss_mdp_mixer *mixer;

	if (!pinfo || !ctl || !ctl->is_master)
		return;

	dsc = &pinfo->dsc;
	mixer = ctl->mixer_left;
	valid = mixer->valid_roi;

	common_mode = __dsc_get_common_mode(ctl, false);
	__dsc_get_pic_dim(mixer, NULL, &pic_width, &pic_height);

	mdss_panel_dsc_update_pic_dim(dsc, pic_width, pic_height);

	this_frame_slices = pic_width / dsc->slice_width;
	intf_ip_w = this_frame_slices * dsc->slice_width;
	mdss_panel_dsc_pclk_param_calc(dsc, intf_ip_w);

	enc_ip_w = intf_ip_w;
	mdss_panel_dsc_initial_line_calc(dsc, enc_ip_w);

	/*
	 * __is_ich_reset_override_needed should be called only after
	 * updating pic dimension, mdss_panel_dsc_update_pic_dim.
	 */
	ich_reset_override = __is_ich_reset_override_needed(
					pinfo->partial_update_enabled, dsc);
	if (valid) {
		__dsc_config(mixer, dsc, common_mode, ich_reset_override);
		__dsc_config_thresh(mixer, dsc);
		__dsc_enable(mixer);
	} else {
		__dsc_disable(mixer);
	}

	pr_debug("mix%d: valid=%d mode=%d, pic_dim:%dx%d intf_ip_w=%d enc_ip_w=%d ich_ovrd=%d\n",
		mixer->num, valid, common_mode, pic_width, pic_height,
		intf_ip_w, enc_ip_w, ich_reset_override);

	MDSS_XLOG(mixer->num, valid, common_mode, pic_width, pic_height,
		  intf_ip_w, enc_ip_w, ich_reset_override);
}

void mdss_mdp_ctl_dsc_setup(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_info *pinfo)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_panel_info *spinfo;

	if (!is_dsc_compression(pinfo))
		return;

	if (!ctl->is_master) {
		pr_debug("skip slave ctl because master will program for both\n");
		return;
	}

	switch (ctl->mfd->split_mode) {
	case MDP_DUAL_LM_SINGLE_DISPLAY:
		__dsc_setup_dual_lm_single_display(ctl, pinfo);
		break;
	case MDP_DUAL_LM_DUAL_DISPLAY:
		sctl = mdss_mdp_get_split_ctl(ctl);
		if (sctl) {
			spinfo = &sctl->panel_data->panel_info;
			__dsc_setup_dual_lm_dual_display(ctl, pinfo, sctl,
					spinfo);
		}
		break;
	default:
		/* pp_split is not supported yet */
		__dsc_setup_single_lm_single_display(ctl, pinfo);
		break;
	}
}

static int mdss_mdp_ctl_fbc_enable(int enable,
		struct mdss_mdp_mixer *mixer, struct mdss_panel_info *pdata)
{
	struct fbc_panel_info *fbc;
	u32 mode = 0, budget_ctl = 0, lossy_mode = 0, width;

	if (!pdata) {
		pr_err("Invalid pdata\n");
		return -EINVAL;
	}

	fbc = &pdata->fbc;

	if (!fbc->enabled) {
		pr_debug("FBC not enabled\n");
		return -EINVAL;
	}

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0 ||
			mixer->num == MDSS_MDP_INTF_LAYERMIXER1) {
		pr_debug("Mixer supports FBC.\n");
	} else {
		pr_debug("Mixer doesn't support FBC.\n");
		return -EINVAL;
	}

	if (enable) {
		if (fbc->enc_mode && pdata->bpp) {
			/* width is the compressed width */
			width = mult_frac(pdata->xres, fbc->target_bpp,
					pdata->bpp);
		} else {
			/* width is the source width */
			width = pdata->xres;
		}

		mode = ((width) << 16) | ((fbc->slice_height) << 11) |
			((fbc->pred_mode) << 10) | ((fbc->enc_mode) << 9) |
			((fbc->comp_mode) << 8) | ((fbc->qerr_enable) << 7) |
			((fbc->cd_bias) << 4) | ((fbc->pat_enable) << 3) |
			((fbc->vlc_enable) << 2) | ((fbc->bflc_enable) << 1) |
			enable;

		budget_ctl = ((fbc->line_x_budget) << 12) |
			((fbc->block_x_budget) << 8) | fbc->block_budget;

		lossy_mode = ((fbc->max_pred_err) << 28) |
			((fbc->lossless_mode_thd) << 16) |
			((fbc->lossy_mode_thd) << 8) |
			((fbc->lossy_rgb_thd) << 4) | fbc->lossy_mode_idx;
	}

	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_MODE, mode);
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_BUDGET_CTL, budget_ctl);
	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_FBC_LOSSY_MODE, lossy_mode);

	return 0;
}

int mdss_mdp_cwb_setup(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cwb *cwb = NULL;
	struct mdss_mdp_writeback *wb = NULL;
	struct mdss_overlay_private *mdp5_data = NULL;
	struct mdss_mdp_wb_data *cwb_data;
	struct mdss_mdp_writeback_arg wb_args;
	struct mdss_mdp_ctl *sctl = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	u32 opmode, data_point;
	int rc = 0;

	if (!ctl->mfd)
		return -ENODEV;

	mdp5_data = mfd_to_mdp5_data(ctl->mfd);
	cwb = &mdp5_data->cwb;

	if (!cwb->valid)
		return rc;

	/* Wait for previous CWB job to complete */
	if (mdss_mdp_acquire_wb(ctl))
		return -EBUSY;

	wb = mdata->wb + cwb->wb_idx;
	wb->base = mdata->mdss_io.base + mdata->wb_offsets[cwb->wb_idx];
	ctl->wb = wb;

	/* Get new instance of writeback interface context */
	cwb->priv_data = mdss_mdp_writeback_get_ctx_for_cwb(ctl);
	if (cwb->priv_data == NULL) {
		pr_err("fail to get writeback context\n");
		rc = -ENOMEM;
		goto cwb_setup_fail;
	}

	/* reset wb to null to avoid deferencing in ctl free */
	ctl->wb = NULL;

	mutex_lock(&cwb->queue_lock);
	cwb_data = list_first_entry_or_null(&cwb->data_queue,
			struct mdss_mdp_wb_data, next);
	__list_del_entry(&cwb_data->next);
	mutex_unlock(&cwb->queue_lock);
	if (cwb_data == NULL) {
		pr_err("no output buffer for cwb\n");
		rc = -ENOMEM;
		goto cwb_setup_fail;
	}

	rc = mdss_mdp_data_map(&cwb_data->data, true, DMA_FROM_DEVICE);
	if (rc) {
		pr_err("fail to acquire CWB output buffer\n");
		goto cwb_setup_fail;
	}

	memset(&wb_args, 0, sizeof(wb_args));
	wb_args.data = &cwb_data->data;

	rc =  mdss_mdp_writeback_prepare_cwb(ctl, &wb_args);
	if (rc) {
		pr_err("failed to writeback prepare cwb\n");
		goto cwb_setup_fail;
	}

	/* Select MEM_SEL to WB */
	ctl->opmode |= MDSS_MDP_CTL_OP_WFD_MODE;
	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctl->opmode |= MDSS_MDP_CTL_OP_WFD_MODE;

	/* Select CWB data point */
	data_point = (cwb->layer.flags & MDP_COMMIT_CWB_DSPP) ? 0x4 : 0;
	writel_relaxed(data_point, mdata->mdp_base + mdata->ppb_ctl[2]);
	if (sctl)
		writel_relaxed(data_point + 1,
				mdata->mdp_base + mdata->ppb_ctl[3]);

	/* Flush WB and CTL */
	ctl->flush_bits |= BIT(16) | BIT(17);

	opmode = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_TOP) | ctl->opmode;
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, opmode);
	if (sctl) {
		opmode = mdss_mdp_ctl_read(sctl, MDSS_MDP_REG_CTL_TOP) |
			sctl->opmode;
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP, opmode);
	}

	/* Increase commit count to signal CWB release fence */
	atomic_inc(&cwb->cwb_sync_pt_data.commit_cnt);

	goto cwb_setup_done;

cwb_setup_fail:
	atomic_add_unless(&mdp5_data->wb_busy, -1, 0);

cwb_setup_done:
	cwb->valid = 0;
	return 0;
}

int mdss_mdp_ctl_setup(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *split_ctl;
	u32 width, height;
	int split_fb, rc = 0;
	u32 max_mixer_width;
	struct mdss_panel_info *pinfo;

	if (!ctl || !ctl->panel_data) {
		pr_err("invalid ctl handle\n");
		return -ENODEV;
	}

	pinfo = &ctl->panel_data->panel_info;
	if (pinfo->type == WRITEBACK_PANEL) {
		pr_err("writeback panel, ignore\n");
		return 0;
	}

	split_ctl = mdss_mdp_get_split_ctl(ctl);

	if (is_dest_scaling_enable(ctl->mixer_left)) {
		width = get_ds_input_width(ctl->mixer_left);
		height = get_ds_input_height(ctl->mixer_left);
		if (is_dual_lm_single_display(ctl->mfd) ||
				(ctl->panel_data->next &&
				 is_pingpong_split(ctl->mfd)))
			width *= 2;
	} else {
		width = get_panel_width(ctl);
		height = get_panel_yres(pinfo);
	}

	max_mixer_width = ctl->mdata->max_mixer_width;

	split_fb = ((is_dual_lm_single_display(ctl->mfd)) &&
		(ctl->mfd->split_fb_left <= max_mixer_width) &&
		(ctl->mfd->split_fb_right <= max_mixer_width)) ? 1 : 0;
	pr_debug("max=%d xres=%d left=%d right=%d\n", max_mixer_width,
		width, ctl->mfd->split_fb_left, ctl->mfd->split_fb_right);

	if ((split_ctl && (width > max_mixer_width)) ||
			(width > (2 * max_mixer_width))) {
		pr_err("Unsupported panel resolution: %dx%d\n", width, height);
		return -ENOTSUPP;
	}

	ctl->width = width;
	ctl->height = height;
	ctl->roi = (struct mdss_rect) {0, 0, width, height};

	if (!ctl->mixer_left) {
		ctl->mixer_left =
			mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_INTF,
			((width > max_mixer_width) || split_fb), 0);
		if (!ctl->mixer_left) {
			pr_err("unable to allocate layer mixer\n");
			return -ENOMEM;
		} else if (split_fb && ctl->mixer_left->num >= 1 &&
			(ctl->panel_data->panel_info.type == MIPI_CMD_PANEL)) {
			pr_err("use only DSPP0 and DSPP1 with cmd split\n");
			return -EPERM;
		}
	}

	if (split_fb) {
		if (is_dest_scaling_enable(ctl->mixer_left)) {
			width = get_ds_input_width(ctl->mixer_left);
		} else {
			width = ctl->mfd->split_fb_left;
			width += (pinfo->lcdc.border_left +
					pinfo->lcdc.border_right);
		}
	} else if (width > max_mixer_width) {
		width /= 2;
	}

	ctl->mixer_left->width = width;
	ctl->mixer_left->height = height;
	ctl->mixer_left->roi = (struct mdss_rect) {0, 0, width, height};
	ctl->mixer_left->valid_roi = true;
	ctl->mixer_left->roi_changed = true;

	rc = mdss_mdp_pp_default_overlay_config(ctl->mfd, ctl->panel_data);
	/*
	 * Ignore failure of PP config, ctl set-up can succeed.
	 */
	if (rc) {
		pr_err("failed to set the pp config rc %dfb %d\n", rc,
			ctl->mfd->index);
		rc = 0;
	}

	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		pr_debug("dual display detected\n");
		return 0;
	}

	if (split_fb) {
		if (is_dest_scaling_enable(ctl->mixer_left))
			width = get_ds_input_width(ctl->mixer_left);
		else
			width = ctl->mfd->split_fb_right;
	}

	if (width < ctl->width) {
		if (ctl->mixer_right == NULL) {
			ctl->mixer_right = mdss_mdp_mixer_alloc(ctl,
					MDSS_MDP_MIXER_TYPE_INTF, true, 0);
			if (!ctl->mixer_right) {
				pr_err("unable to allocate right mixer\n");
				if (ctl->mixer_left)
					mdss_mdp_mixer_free(ctl->mixer_left);
				return -ENOMEM;
			}
		}
		ctl->mixer_right->is_right_mixer = true;
		ctl->mixer_right->width = width;
		ctl->mixer_right->height = height;
		ctl->mixer_right->roi = (struct mdss_rect)
						{0, 0, width, height};
		ctl->mixer_right->valid_roi = true;
		ctl->mixer_right->roi_changed = true;
	} else if (ctl->mixer_right) {
		ctl->mixer_right->valid_roi = false;
		ctl->mixer_right->roi_changed = false;
		mdss_mdp_mixer_free(ctl->mixer_right);
		ctl->mixer_right = NULL;
	}

	if (ctl->mixer_right) {
		if (!is_dsc_compression(pinfo) ||
		    (pinfo->dsc_enc_total == 1))
			ctl->opmode |= MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
				       MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT;
	} else {
		ctl->opmode &= ~(MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
				  MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT);
	}
	return 0;
}

/**
 * mdss_mdp_ctl_reconfig() - re-configure ctl for new mode
 * @ctl: mdp controller.
 * @pdata: panel data
 *
 * This function is called when we are trying to dynamically change
 * the DSI mode. We need to change various mdp_ctl properties to
 * the new mode of operation.
 */
int mdss_mdp_ctl_reconfig(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata)
{
	void *tmp;
	int ret = 0;

	/*
	 * Switch first to prevent deleting important data in the case
	 * where panel type is not supported in reconfig
	 */
	if ((pdata->panel_info.type != MIPI_VIDEO_PANEL) &&
			(pdata->panel_info.type != MIPI_CMD_PANEL)) {
		pr_err("unsupported panel type (%d)\n", pdata->panel_info.type);
		return -EINVAL;
	}

	/* if only changing resolution there is no need for intf reconfig */
	if (!ctl->is_video_mode == (pdata->panel_info.type == MIPI_CMD_PANEL))
		goto skip_intf_reconfig;

	/*
	 * Intentionally not clearing stop function, as stop will
	 * be called after panel is instructed mode switch is happening
	 */
	tmp = ctl->ops.stop_fnc;
	memset(&ctl->ops, 0, sizeof(ctl->ops));
	ctl->ops.stop_fnc = tmp;

	switch (pdata->panel_info.type) {
	case MIPI_VIDEO_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_CMD_PANEL:
		ctl->is_video_mode = false;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_CMD_MODE;
		ctl->ops.start_fnc = mdss_mdp_cmd_start;
		break;
	}

	ctl->is_secure = false;
	ctl->split_flush_en = false;
	ctl->perf_release_ctl_bw = false;
	ctl->play_cnt = 0;

	ctl->opmode |= (ctl->intf_num << 4);

skip_intf_reconfig:
	if (is_dest_scaling_enable(ctl->mixer_left)) {
		ctl->width  = get_ds_input_width(ctl->mixer_left);
		ctl->height = get_ds_input_height(ctl->mixer_left);
	} else {
		ctl->width  = get_panel_xres(&pdata->panel_info);
		ctl->height = get_panel_yres(&pdata->panel_info);
	}

	if (ctl->mfd->split_mode == MDP_DUAL_LM_SINGLE_DISPLAY) {
		if (ctl->mixer_left) {
			ctl->mixer_left->width = ctl->width / 2;
			ctl->mixer_left->height = ctl->height;
		}
		if (ctl->mixer_right) {
			ctl->mixer_right->width = ctl->width / 2;
			ctl->mixer_right->height = ctl->height;
		}
	} else {
		/*
		 * Handles MDP_SPLIT_MODE_NONE, MDP_DUAL_LM_DUAL_DISPLAY and
		 * MDP_PINGPONG_SPLIT case.
		 */
		if (ctl->mixer_left) {
			ctl->mixer_left->width = ctl->width;
			ctl->mixer_left->height = ctl->height;
		}
	}
	ctl->roi = (struct mdss_rect) {0, 0, ctl->width, ctl->height};

	ctl->border_x_off = pdata->panel_info.lcdc.border_left;
	ctl->border_y_off = pdata->panel_info.lcdc.border_top;

	return ret;
}

struct mdss_mdp_ctl *mdss_mdp_ctl_init(struct mdss_panel_data *pdata,
				       struct msm_fb_data_type *mfd)
{
	int ret = 0, offset;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_panel_info *pinfo;

	if (pdata->panel_info.type == WRITEBACK_PANEL)
		offset = mdss_mdp_get_wb_ctl_support(mdata, false);
	else
		offset = MDSS_MDP_CTL0;

	if (is_pingpong_split(mfd) && !mdata->has_pingpong_split) {
		pr_err("Error: pp_split cannot be enabled on fb%d if HW doesn't support it\n",
			mfd->index);
		return ERR_PTR(-EINVAL);
	}

	ctl = mdss_mdp_ctl_alloc(mdata, offset);
	if (!ctl) {
		pr_err("unable to allocate ctl\n");
		return ERR_PTR(-ENOMEM);
	}

	pinfo = &pdata->panel_info;
	ctl->mfd = mfd;
	ctl->panel_data = pdata;
	ctl->is_video_mode = false;
	ctl->perf_release_ctl_bw = false;
	ctl->border_x_off = pinfo->lcdc.border_left;
	ctl->border_y_off = pinfo->lcdc.border_top;
	ctl->disable_prefill = false;

	switch (pdata->panel_info.type) {
	case DP_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF0;
		ctl->intf_type = MDSS_INTF_EDP;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_VIDEO_PANEL:
		ctl->is_video_mode = true;
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF2 :
				MDSS_MDP_INTF1;
		else
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF1 :
				MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_CMD_PANEL:
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF2 :
				MDSS_MDP_INTF1;
		else
			ctl->intf_num = mdp5_data->mixer_swap ? MDSS_MDP_INTF1 :
				MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_CMD_MODE;
		ctl->ops.start_fnc = mdss_mdp_cmd_start;
		break;
	case DTV_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF3;
		ctl->intf_type = MDSS_INTF_HDMI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->ops.start_fnc = mdss_mdp_video_start;
		break;
	case WRITEBACK_PANEL:
		ctl->intf_num = MDSS_MDP_NO_INTF;
		ctl->ops.start_fnc = mdss_mdp_writeback_start;
		break;
	default:
		pr_err("unsupported panel type (%d)\n", pdata->panel_info.type);
		ret = -EINVAL;
		goto ctl_init_fail;
	}

	ctl->opmode |= (ctl->intf_num << 4);

	if (ctl->intf_num == MDSS_MDP_NO_INTF) {
		ctl->dst_format = pdata->panel_info.out_format;
	} else {
		switch (pdata->panel_info.bpp) {
		case 18:
			if (ctl->intf_type == MDSS_INTF_DSI)
				ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB666 |
					MDSS_MDP_PANEL_FORMAT_PACK_ALIGN_MSB;
			else
				ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB666;
			break;
		case 24:
		default:
			ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB888;
			break;
		}
	}

	return ctl;
ctl_init_fail:
	mdss_mdp_ctl_free(ctl);

	return ERR_PTR(ret);
}

int mdss_mdp_ctl_split_display_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_data *pdata)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_mdp_mixer *mixer;

	if (!ctl || !pdata)
		return -ENODEV;

	if (pdata->panel_info.xres > ctl->mdata->max_mixer_width) {
		pr_err("Unsupported second panel resolution: %dx%d\n",
				pdata->panel_info.xres, pdata->panel_info.yres);
		return -ENOTSUPP;
	}

	if (ctl->mixer_right) {
		pr_err("right mixer already setup for ctl=%d\n", ctl->num);
		return -EPERM;
	}

	sctl = mdss_mdp_ctl_init(pdata, ctl->mfd);
	if (!sctl) {
		pr_err("unable to setup split display\n");
		return -ENODEV;
	}

	if (!ctl->mixer_left) {
		ctl->mixer_left = mdss_mdp_mixer_alloc(ctl,
				MDSS_MDP_MIXER_TYPE_INTF,
				false, 0);
		if (!ctl->mixer_left) {
			pr_err("unable to allocate layer mixer\n");
			mdss_mdp_ctl_destroy(sctl);
			return -ENOMEM;
		}
	}

	mixer = mdss_mdp_mixer_alloc(sctl, MDSS_MDP_MIXER_TYPE_INTF, false, 0);
	if (!mixer) {
		pr_err("unable to allocate layer mixer\n");
		mdss_mdp_ctl_destroy(sctl);
		return -ENOMEM;
	}

	if (is_dest_scaling_enable(mixer)) {
		sctl->width  = get_ds_input_width(mixer);
		sctl->height = get_ds_input_height(mixer);
	} else {
		sctl->width  = get_panel_xres(&pdata->panel_info);
		sctl->height = get_panel_yres(&pdata->panel_info);
	}

	sctl->roi = (struct mdss_rect){0, 0, sctl->width, sctl->height};

	mixer->is_right_mixer = true;
	mixer->width = sctl->width;
	mixer->height = sctl->height;
	mixer->roi = (struct mdss_rect)
				{0, 0, mixer->width, mixer->height};
	mixer->valid_roi = true;
	mixer->roi_changed = true;
	sctl->mixer_left = mixer;

	return mdss_mdp_set_split_ctl(ctl, sctl);
}

static void mdss_mdp_ctl_split_display_enable(int enable,
	struct mdss_mdp_ctl *main_ctl, struct mdss_mdp_ctl *slave_ctl)
{
	u32 upper = 0, lower = 0;

	pr_debug("split main ctl=%d intf=%d\n",
			main_ctl->num, main_ctl->intf_num);

	if (slave_ctl)
		pr_debug("split slave ctl=%d intf=%d\n",
			slave_ctl->num, slave_ctl->intf_num);

	if (enable) {
		if (main_ctl->opmode & MDSS_MDP_CTL_OP_CMD_MODE) {
			/* interface controlling sw trigger (cmd mode) */
			lower |= BIT(1);
			if (main_ctl->intf_num == MDSS_MDP_INTF2)
				lower |= BIT(4);
			else
				lower |= BIT(8);
			/*
			 * Enable SMART_PANEL_FREE_RUN if ping pong split
			 * is enabled.
			 */
			if (is_pingpong_split(main_ctl->mfd))
				lower |= BIT(2);
			upper = lower;
		} else {
			/* interface controlling sw trigger (video mode) */
			if (main_ctl->intf_num == MDSS_MDP_INTF2) {
				lower |= BIT(4);
				upper |= BIT(8);
			} else {
				lower |= BIT(8);
				upper |= BIT(4);
			}
		}
	}
	writel_relaxed(upper, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_UPPER_PIPE_CTRL);
	writel_relaxed(lower, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_LOWER_PIPE_CTRL);
	writel_relaxed(enable, main_ctl->mdata->mdp_base +
		MDSS_MDP_REG_SPLIT_DISPLAY_EN);

	if ((main_ctl->mdata->mdp_rev >= MDSS_MDP_HW_REV_103)
		&& main_ctl->is_video_mode) {
		struct mdss_overlay_private *mdp5_data;
		bool mixer_swap = false;

		if (main_ctl->mfd) {
			mdp5_data = mfd_to_mdp5_data(main_ctl->mfd);
			mixer_swap = mdp5_data->mixer_swap;
		}

		main_ctl->split_flush_en = !mixer_swap;
		if (main_ctl->split_flush_en)
			writel_relaxed(enable ? 0x1 : 0x0,
				main_ctl->mdata->mdp_base +
				MMSS_MDP_MDP_SSPP_SPARE_0);
	}
}

static void mdss_mdp_ctl_pp_split_display_enable(bool enable,
		struct mdss_mdp_ctl *ctl)
{
	u32 cfg = 0, cntl = 0;

	if (!ctl->mdata->nppb_ctl || !ctl->mdata->nppb_cfg) {
		pr_err("No PPB to enable PP split\n");
		BUG();
	}

	mdss_mdp_ctl_split_display_enable(enable, ctl, NULL);

	if (enable) {
		cfg = ctl->slave_intf_num << 20; /* Set slave intf */
		cfg |= BIT(16);			 /* Set horizontal split */
		cntl = BIT(5);			 /* enable dst split */
	}

	writel_relaxed(cfg, ctl->mdata->mdp_base + ctl->mdata->ppb_cfg[0]);
	writel_relaxed(cntl, ctl->mdata->mdp_base + ctl->mdata->ppb_ctl[0]);
}

int mdss_mdp_ctl_destroy(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;
	int rc;

	rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CLOSE, NULL,
				     CTL_INTF_EVENT_FLAG_DEFAULT);
	WARN(rc, "unable to close panel for intf=%d\n", ctl->intf_num);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl) {
		pr_debug("destroying split display ctl=%d\n", sctl->num);
		mdss_mdp_ctl_free(sctl);
	}

	mdss_mdp_ctl_free(ctl);

	return 0;
}

int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg,
	u32 flags)
{
	struct mdss_panel_data *pdata;
	int rc = 0;

	if (!ctl || !ctl->panel_data)
		return -ENODEV;

	pdata = ctl->panel_data;

	if (flags & CTL_INTF_EVENT_FLAG_SLAVE_INTF) {
		pdata = pdata->next;
		if (!pdata) {
			pr_err("Error: event=%d flags=0x%x, ctl%d slave intf is not present\n",
				event, flags, ctl->num);
			return -EINVAL;
		}
	}

	pr_debug("sending ctl=%d event=%d flag=0x%x\n", ctl->num, event, flags);

	do {
		if (pdata->event_handler)
			rc = pdata->event_handler(pdata, event, arg);
		pdata = pdata->next;
	} while (rc == 0 && pdata && pdata->active &&
		!(flags & CTL_INTF_EVENT_FLAG_SKIP_BROADCAST));

	return rc;
}

static void mdss_mdp_ctl_restore_sub(struct mdss_mdp_ctl *ctl)
{
	u32 temp;
	int ret = 0;

	temp = readl_relaxed(ctl->mdata->mdp_base +
			MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	writel_relaxed(temp, ctl->mdata->mdp_base +
			MDSS_MDP_REG_DISP_INTF_SEL);

	if (ctl->mfd && ctl->panel_data) {
		ctl->mfd->ipc_resume = true;
		mdss_mdp_pp_resume(ctl->mfd);
		mdss_mdp_pp_dest_scaler_resume(ctl);

		if (is_dsc_compression(&ctl->panel_data->panel_info)) {
			/*
			 * Avoid redundant call to dsc_setup when mode switch
			 * is in progress. During the switch, dsc_setup is
			 * handled in mdss_mode_switch() function.
			 */
			if (ctl->pending_mode_switch != SWITCH_RESOLUTION)
				mdss_mdp_ctl_dsc_setup(ctl,
					&ctl->panel_data->panel_info);
		} else if (ctl->panel_data->panel_info.compression_mode ==
				COMPRESSION_FBC) {
			ret = mdss_mdp_ctl_fbc_enable(1, ctl->mixer_left,
					&ctl->panel_data->panel_info);
			if (ret)
				pr_err("Failed to restore FBC mode\n");
		}
	}
}

/*
 * mdss_mdp_ctl_restore() - restore mdp ctl path
 * @locked - boolean to signal that clock lock is already acquired
 *
 * This function is called whenever MDP comes out of a power collapse as
 * a result of a screen update. It restores the MDP controller's software
 * state to the hardware registers.
 * Function does not enable the clocks, so caller must make sure
 * clocks are enabled before calling.
 * The locked boolean in the parametrs signals that synchronization
 * with mdp clocks access is not required downstream.
 * Only call this function setting this value to true if the clocks access
 * synchronization is guaranteed by the caller.
 */
void mdss_mdp_ctl_restore(bool locked)
{
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_ctl *sctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 cnum;

	for (cnum = MDSS_MDP_CTL0; cnum < mdata->nctl; cnum++) {
		ctl = mdata->ctl_off + cnum;
		if (!mdss_mdp_ctl_is_power_on(ctl))
			continue;

		pr_debug("restoring ctl%d, intf_type=%d\n", cnum,
			ctl->intf_type);
		ctl->play_cnt = 0;
		sctl = mdss_mdp_get_split_ctl(ctl);
		mdss_mdp_ctl_restore_sub(ctl);
		if (sctl) {
			mdss_mdp_ctl_restore_sub(sctl);
			mdss_mdp_ctl_split_display_enable(1, ctl, sctl);
		} else if (is_pingpong_split(ctl->mfd)) {
			mdss_mdp_ctl_pp_split_display_enable(1, ctl);
		}

		if (ctl->ops.restore_fnc)
			ctl->ops.restore_fnc(ctl, locked);
	}
}

static int mdss_mdp_ctl_start_sub(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_mdp_mixer *mixer;
	u32 outsize, temp;
	int ret = 0;
	int i, nmixers;

	pr_debug("ctl_num=%d\n", ctl->num);

	/*
	 * Need start_fnc in 2 cases:
	 * (1) handoff
	 * (2) continuous splash finished.
	 */
	if (handoff || !ctl->panel_data->panel_info.cont_splash_enabled) {
		if (ctl->ops.start_fnc)
			ret = ctl->ops.start_fnc(ctl);
		else
			pr_warn("no start function for ctl=%d type=%d\n",
					ctl->num,
					ctl->panel_data->panel_info.type);

		if (ret) {
			pr_err("unable to start intf\n");
			return ret;
		}
	}

	if (!ctl->panel_data->panel_info.cont_splash_enabled) {
		nmixers = MDSS_MDP_INTF_MAX_LAYERMIXER +
			MDSS_MDP_WB_MAX_LAYERMIXER;
		for (i = 0; i < nmixers; i++)
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(i), 0);
	}

	temp = readl_relaxed(ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	if (is_pingpong_split(ctl->mfd))
		temp |= (ctl->intf_type << (ctl->intf_num * 8));

	writel_relaxed(temp, ctl->mdata->mdp_base +
		MDSS_MDP_REG_DISP_INTF_SEL);

	mixer = ctl->mixer_left;
	if (mixer) {
		struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

		mixer->params_changed++;

		outsize = (mixer->height << 16) | mixer->width;
		mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, outsize);

		if (is_dsc_compression(pinfo)) {
			mdss_mdp_ctl_dsc_setup(ctl, pinfo);
		} else if (pinfo->compression_mode == COMPRESSION_FBC) {
			ret = mdss_mdp_ctl_fbc_enable(1, ctl->mixer_left,
					pinfo);
		}
	}
	return ret;
}

int mdss_mdp_ctl_start(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int ret = 0;

	pr_debug("ctl_num=%d, power_state=%d\n", ctl->num, ctl->power_state);

	if (mdss_mdp_ctl_is_power_on_interactive(ctl)
			&& !(ctl->pending_mode_switch)) {
		pr_debug("%d: panel already on!\n", __LINE__);
		return 0;
	}

	ret = mdss_mdp_ctl_setup(ctl);
	if (ret)
		return ret;

	sctl = mdss_mdp_get_split_ctl(ctl);

	mutex_lock(&ctl->lock);

	if (mdss_mdp_ctl_is_power_off(ctl))
		memset(&ctl->cur_perf, 0, sizeof(ctl->cur_perf));

	/*
	 * keep power_on false during handoff to avoid unexpected
	 * operations to overlay.
	 */
	if (!handoff || ctl->pending_mode_switch)
		ctl->power_state = MDSS_PANEL_POWER_ON;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	ret = mdss_mdp_ctl_start_sub(ctl, handoff);
	if (ret == 0) {
		if (sctl && ctl->mfd &&
		    ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
			/*split display available */
			ret = mdss_mdp_ctl_start_sub(sctl, handoff);
			if (!ret)
				mdss_mdp_ctl_split_display_enable(1, ctl, sctl);
		} else if (ctl->mixer_right) {
			struct mdss_mdp_mixer *mixer = ctl->mixer_right;
			u32 out;

			mixer->params_changed++;
			out = (mixer->height << 16) | mixer->width;
			mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, out);
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_PACK_3D, 0);
		} else if (is_pingpong_split(ctl->mfd)) {
			ctl->slave_intf_num = (ctl->intf_num + 1);
			mdss_mdp_ctl_pp_split_display_enable(true, ctl);
		}
	}

	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_RESUME);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_ctl_stop(struct mdss_mdp_ctl *ctl, int power_state)
{
	struct mdss_mdp_ctl *sctl;
	int ret = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	pr_debug("ctl_num=%d, power_state=%d\n", ctl->num, ctl->power_state);

	if (!ctl->mfd->panel_reconfig && !mdss_mdp_ctl_is_power_on(ctl)) {
		pr_debug("%s %d already off!\n", __func__, __LINE__);
		return 0;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);

	mutex_lock(&ctl->lock);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_SUSPEND);

	if (ctl->ops.stop_fnc) {
		ret = ctl->ops.stop_fnc(ctl, power_state);
		if (ctl->panel_data->panel_info.compression_mode ==
				COMPRESSION_FBC) {
			mdss_mdp_ctl_fbc_enable(0, ctl->mixer_left,
					&ctl->panel_data->panel_info);
		}
	} else {
		pr_warn("no stop func for ctl=%d\n", ctl->num);
	}

	if (sctl && sctl->ops.stop_fnc) {
		ret = sctl->ops.stop_fnc(sctl, power_state);
		if (sctl->panel_data->panel_info.compression_mode ==
				COMPRESSION_FBC) {
			mdss_mdp_ctl_fbc_enable(0, sctl->mixer_left,
					&sctl->panel_data->panel_info);
		}
	}
	if (ret) {
		pr_warn("error powering off intf ctl=%d\n", ctl->num);
		goto end;
	}

	if (mdss_panel_is_power_on(power_state)) {
		pr_debug("panel is not off, leaving ctl power on\n");
		goto end;
	}

	if (sctl)
		mdss_mdp_ctl_split_display_enable(0, ctl, sctl);

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, 0);
	if (sctl) {
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP, 0);
		mdss_mdp_reset_mixercfg(sctl);
	}

	mdss_mdp_reset_mixercfg(ctl);

	ctl->play_cnt = 0;

end:
	if (!ret) {
		ctl->power_state = power_state;
		if (!ctl->pending_mode_switch)
			mdss_mdp_ctl_perf_update(ctl, 0, true);
	}
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	mutex_unlock(&ctl->lock);

	return ret;
}

/*
 * mdss_mdp_pipe_reset() - Halts all the pipes during ctl reset.
 * @mixer: Mixer from which to reset all pipes.
 * This function called during control path reset and will halt
 * all the pipes staged on the mixer.
 */
static void mdss_mdp_pipe_reset(struct mdss_mdp_mixer *mixer, bool is_recovery)
{
	unsigned long pipe_map = mixer->pipe_mapped;
	u32 bit = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool sw_rst_avail = mdss_mdp_pipe_is_sw_reset_available(mdata);

	pr_debug("pipe_map=0x%lx\n", pipe_map);
	for_each_set_bit_from(bit, &pipe_map, MAX_PIPES_PER_LM) {
		struct mdss_mdp_pipe *pipe;

		/*
		 * this assumes that within lm there can be either rect0+rect1
		 * or rect0 only. Thus to find the hardware pipe to halt only
		 * check for rect 0 is sufficient.
		 */
		pipe = mdss_mdp_pipe_search(mdata, 1 << bit,
				MDSS_MDP_PIPE_RECT0);
		if (pipe) {
			mdss_mdp_pipe_fetch_halt(pipe, is_recovery);
			if (sw_rst_avail)
				mdss_mdp_pipe_clk_force_off(pipe);
		}
	}
}

static u32 mdss_mdp_poll_ctl_reset_status(struct mdss_mdp_ctl *ctl, u32 cnt)
{
	u32 status;
	/*
	 * it takes around 30us to have mdp finish resetting its ctl path
	 * poll every 50us so that reset should be completed at 1st poll
	 */
	do {
		udelay(50);
		status = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_SW_RESET);
		status &= 0x01;
		pr_debug("status=%x, count=%d\n", status, cnt);
		cnt--;
	} while (cnt > 0 && status);

	return status;
}

/*
 * mdss_mdp_check_ctl_reset_status() - checks ctl reset status
 * @ctl: mdp controller
 *
 * This function checks the ctl reset status before every frame update.
 * If the reset bit is set, it keeps polling the status till the hw
 * reset is complete. And does a panic if hw fails to complet the reset
 * with in the max poll interval.
 */
void mdss_mdp_check_ctl_reset_status(struct mdss_mdp_ctl *ctl)
{
	u32 status;

	if (!ctl)
		return;

	status = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_SW_RESET);
	status &= 0x01;
	if (!status)
		return;

	pr_debug("hw ctl reset is set for ctl:%d\n", ctl->num);
	/* poll for at least ~1 frame */
	status = mdss_mdp_poll_ctl_reset_status(ctl, 320);
	if (status) {
		pr_err("hw recovery is not complete for ctl:%d status:0x%x\n",
			ctl->num, status);
		MDSS_XLOG_TOUT_HANDLER("mdp", "vbif", "vbif_nrt", "dbg_bus",
			"vbif_dbg_bus", "panic");
	}
}

/*
 * mdss_mdp_ctl_reset() - reset mdp ctl path.
 * @ctl: mdp controller.
 * this function called when underflow happen,
 * it will reset mdp ctl path and poll for its completion
 *
 * Note: called within atomic context.
 */
int mdss_mdp_ctl_reset(struct mdss_mdp_ctl *ctl, bool is_recovery)
{
	u32 status;
	struct mdss_mdp_mixer *mixer;

	if (!ctl) {
		pr_err("ctl not initialized\n");
		return -EINVAL;
	}

	mixer = ctl->mixer_left;
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_SW_RESET, 1);

	status = mdss_mdp_poll_ctl_reset_status(ctl, 20);
	if (status)
		pr_err("sw ctl:%d reset timedout\n", ctl->num);

	if (mixer) {
		mdss_mdp_pipe_reset(mixer, is_recovery);

		if (is_dual_lm_single_display(ctl->mfd))
			mdss_mdp_pipe_reset(ctl->mixer_right, is_recovery);
	}

	return (status) ? -EAGAIN : 0;
}

/*
 * mdss_mdp_mixer_update_pipe_map() - keep track of pipe configuration in  mixer
 * @master_ctl: mdp controller.
 *
 * This function keeps track of the current mixer configuration in the hardware.
 * It's callers responsibility to call with master control.
 */
void mdss_mdp_mixer_update_pipe_map(struct mdss_mdp_ctl *master_ctl,
		       int mixer_mux)
{
	struct mdss_mdp_mixer *mixer = mdss_mdp_mixer_get(master_ctl,
			mixer_mux);

	if (!mixer)
		return;

	pr_debug("mixer%d pipe_mapped=0x%x next_pipes=0x%x\n",
		mixer->num, mixer->pipe_mapped, mixer->next_pipe_map);

	mixer->pipe_mapped = mixer->next_pipe_map;
}

static void mdss_mdp_set_mixer_roi(struct mdss_mdp_mixer *mixer,
	struct mdss_rect *roi)
{
	mixer->valid_roi = (roi->w && roi->h);
	mixer->roi_changed = false;

	if (!mdss_rect_cmp(roi, &mixer->roi)) {
		mixer->roi = *roi;
		mixer->params_changed++;
		mixer->roi_changed = true;
	}

	pr_debug("mixer%d ROI %s: [%d, %d, %d, %d]\n",
		mixer->num, mixer->roi_changed ? "changed" : "not changed",
		mixer->roi.x, mixer->roi.y, mixer->roi.w, mixer->roi.h);
	MDSS_XLOG(mixer->num, mixer->roi_changed, mixer->valid_roi,
		mixer->roi.x, mixer->roi.y, mixer->roi.w, mixer->roi.h);
}

/* only call from master ctl */
void mdss_mdp_set_roi(struct mdss_mdp_ctl *ctl,
	struct mdss_rect *l_roi, struct mdss_rect *r_roi)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	enum mdss_mdp_pu_type previous_frame_pu_type, current_frame_pu_type;

	/* Reset ROI when we have (1) invalid ROI (2) feature disabled */
	if ((!l_roi->w && l_roi->h) || (l_roi->w && !l_roi->h) ||
	    (!r_roi->w && r_roi->h) || (r_roi->w && !r_roi->h) ||
	    (!l_roi->w && !l_roi->h && !r_roi->w && !r_roi->h) ||
	    !ctl->panel_data->panel_info.partial_update_enabled) {

		if (ctl->mixer_left) {
			*l_roi = (struct mdss_rect) {0, 0,
					ctl->mixer_left->width,
					ctl->mixer_left->height};
		}

		if (ctl->mixer_right) {
			*r_roi = (struct mdss_rect) {0, 0,
					ctl->mixer_right->width,
					ctl->mixer_right->height};
		}
	}

	previous_frame_pu_type = mdss_mdp_get_pu_type(ctl);
	if (ctl->mixer_left) {
		mdss_mdp_set_mixer_roi(ctl->mixer_left, l_roi);
		ctl->roi = ctl->mixer_left->roi;
	}

	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

		if (sctl && sctl->mixer_left) {
			mdss_mdp_set_mixer_roi(sctl->mixer_left, r_roi);
			sctl->roi = sctl->mixer_left->roi;
		}
	} else if (is_dual_lm_single_display(ctl->mfd) && ctl->mixer_right) {

		mdss_mdp_set_mixer_roi(ctl->mixer_right, r_roi);

		/* in this case, CTL_ROI is a union of left+right ROIs. */
		ctl->roi.w += ctl->mixer_right->roi.w;

		/* right_only, update roi.x as per CTL ROI guidelines */
		if (ctl->mixer_left && !ctl->mixer_left->valid_roi) {
			ctl->roi = ctl->mixer_right->roi;
			ctl->roi.x = left_lm_w_from_mfd(ctl->mfd) +
				ctl->mixer_right->roi.x;
		}
	}

	current_frame_pu_type = mdss_mdp_get_pu_type(ctl);

	/*
	 * Force HW programming whenever partial update type changes
	 * between two consecutive frames to avoid incorrect HW programming.
	 */
	if (is_split_lm(ctl->mfd) && mdata->has_src_split &&
	    (previous_frame_pu_type != current_frame_pu_type)) {
		if (ctl->mixer_left)
			ctl->mixer_left->roi_changed = true;
		if (ctl->mixer_right)
			ctl->mixer_right->roi_changed = true;
	}
}

static void __mdss_mdp_mixer_update_cfg_masks(u32 pnum,
		enum mdss_mdp_pipe_rect rect_num,
		u32 stage, struct mdss_mdp_mixer_cfg *cfg)
{
	u32 masks[NUM_MIXERCFG_REGS] = { 0 };
	int i;

	if (pnum >= MDSS_MDP_MAX_SSPP)
		return;

	if (rect_num == MDSS_MDP_PIPE_RECT0) {
		masks[0] = mdss_mdp_hwio_mask(&mdp_pipe_hwio[pnum].base, stage);
		masks[1] = mdss_mdp_hwio_mask(&mdp_pipe_hwio[pnum].ext, stage);
		masks[2] = mdss_mdp_hwio_mask(&mdp_pipe_hwio[pnum].ext2, stage);
	} else { /* RECT1 */
		masks[2] = mdss_mdp_hwio_mask(&mdp_pipe_rec1_hwio[pnum].ext2,
				stage);
	}

	for (i = 0; i < NUM_MIXERCFG_REGS; i++)
		cfg->config_masks[i] |= masks[i];

	pr_debug("pnum=%d stage=%d cfg=0x%08x ext=0x%08x\n",
			pnum, stage, masks[0], masks[1]);
}

static void __mdss_mdp_mixer_get_offsets(u32 mixer_num,
		u32 *offsets, size_t count)
{
	BUG_ON(count < NUM_MIXERCFG_REGS);

	offsets[0] = MDSS_MDP_REG_CTL_LAYER(mixer_num);
	offsets[1] = MDSS_MDP_REG_CTL_LAYER_EXTN(mixer_num);
	offsets[2] = MDSS_MDP_REG_CTL_LAYER_EXTN2(mixer_num);
}

static inline int __mdss_mdp_mixer_get_hw_num(struct mdss_mdp_mixer *mixer)
{
	/*
	 * mapping to hardware expectation of actual mixer programming to
	 * happen on following registers:
	 *  INTF: 0, 1, 2, 5
	 *  WB: 3, 4
	 * With some exceptions on certain revisions
	 */
	if (mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK) {
		u32 wb_offset;

		if (test_bit(MDSS_CAPS_MIXER_1_FOR_WB,
					mixer->ctl->mdata->mdss_caps_map))
			wb_offset = MDSS_MDP_INTF_LAYERMIXER1;
		else
			wb_offset = MDSS_MDP_INTF_LAYERMIXER3;

		return mixer->num + wb_offset;
	} else if (mixer->num == MDSS_MDP_INTF_LAYERMIXER3) {
		return 5;
	} else {
		return mixer->num;
	}
}

static inline void __mdss_mdp_mixer_write_layer(struct mdss_mdp_ctl *ctl,
		u32 mixer_num, u32 *values, size_t count)
{
	u32 off[NUM_MIXERCFG_REGS];
	int i;

	BUG_ON(!values || count < NUM_MIXERCFG_REGS);

	__mdss_mdp_mixer_get_offsets(mixer_num, off, ARRAY_SIZE(off));

	for (i = 0; i < count; i++)
		mdss_mdp_ctl_write(ctl, off[i], values[i]);
}

static void __mdss_mdp_mixer_write_cfg(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_mixer_cfg *cfg)
{
	u32 vals[NUM_MIXERCFG_REGS] = {0};
	int i, mixer_num;

	if (!mixer)
		return;

	mixer_num = __mdss_mdp_mixer_get_hw_num(mixer);

	if (cfg) {
		for (i = 0; i < NUM_MIXERCFG_REGS; i++)
			vals[i] = cfg->config_masks[i];

		if (cfg->border_enabled)
			vals[0] |= MDSS_MDP_LM_BORDER_COLOR;
		if (cfg->cursor_enabled)
			vals[0] |= MDSS_MDP_LM_CURSOR_OUT;
	}

	__mdss_mdp_mixer_write_layer(mixer->ctl, mixer_num,
			vals, ARRAY_SIZE(vals));

	pr_debug("mixer=%d cfg=0%08x cfg_extn=0x%08x cfg_extn2=0x%08x\n",
		mixer->num, vals[0], vals[1], vals[2]);
	MDSS_XLOG(mixer->num, vals[0], vals[1], vals[2]);
}

void mdss_mdp_reset_mixercfg(struct mdss_mdp_ctl *ctl)
{
	u32 vals[NUM_MIXERCFG_REGS] = {0};
	int i, nmixers;

	if (!ctl)
		return;

	nmixers = MDSS_MDP_INTF_MAX_LAYERMIXER + MDSS_MDP_WB_MAX_LAYERMIXER;

	for (i = 0; i < nmixers; i++)
		__mdss_mdp_mixer_write_layer(ctl, i, vals, ARRAY_SIZE(vals));
}

bool mdss_mdp_mixer_reg_has_pipe(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_pipe *pipe)
{
	u32 offs[NUM_MIXERCFG_REGS];
	u32 cfgs[NUM_MIXERCFG_REGS];
	struct mdss_mdp_mixer_cfg mixercfg;
	int i, mixer_num;

	if (!mixer)
		return false;

	memset(&mixercfg, 0, sizeof(mixercfg));

	mixer_num = __mdss_mdp_mixer_get_hw_num(mixer);
	__mdss_mdp_mixer_get_offsets(mixer_num, offs, NUM_MIXERCFG_REGS);

	for (i = 0; i < NUM_MIXERCFG_REGS; i++)
		cfgs[i] = mdss_mdp_ctl_read(mixer->ctl, offs[i]);

	__mdss_mdp_mixer_update_cfg_masks(pipe->num, pipe->multirect.num, -1,
			&mixercfg);
	for (i = 0; i < NUM_MIXERCFG_REGS; i++) {
		if (cfgs[i] & mixercfg.config_masks[i]) {
			MDSS_XLOG(mixer->num, cfgs[0], cfgs[1]);
			return true;
		}
	}

	return false;
}

static void mdss_mdp_mixer_setup(struct mdss_mdp_ctl *master_ctl,
	int mixer_mux, bool lm_swap)
{
	int i, mixer_num;
	int stage, screen_state, outsize;
	u32 off, blend_op, blend_stage;
	u32 mixer_op_mode = 0, bg_alpha_enable = 0;
	struct mdss_mdp_mixer_cfg mixercfg;
	u32 fg_alpha = 0, bg_alpha = 0;
	struct mdss_mdp_pipe *pipe;
	struct mdss_mdp_ctl *ctl, *ctl_hw;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_mixer *mixer_hw = mdss_mdp_mixer_get(master_ctl,
		mixer_mux);
	struct mdss_mdp_mixer *mixer;

	if (!mixer_hw)
		return;

	ctl = mixer_hw->ctl;
	if (!ctl)
		return;

	ctl_hw = ctl;
	mixer_hw->params_changed = 0;

	/* check if mixer setup for rotator is needed */
	if (mixer_hw->rotator_mode) {
		__mdss_mdp_mixer_write_cfg(mixer_hw, NULL);
		return;
	}

	memset(&mixercfg, 0, sizeof(mixercfg));

	if (lm_swap) {
		if (mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT)
			mixer = mdss_mdp_mixer_get(master_ctl,
				MDSS_MDP_MIXER_MUX_LEFT);
		else
			mixer = mdss_mdp_mixer_get(master_ctl,
				MDSS_MDP_MIXER_MUX_RIGHT);
		ctl_hw = mixer->ctl;
	} else {
		mixer = mixer_hw;
	}

	/*
	 * if lm_swap was used on MDP_DUAL_LM_DUAL_DISPLAY then we need to
	 * reset mixercfg every frame because there might be a stale value
	 * in mixerfcfg register.
	 */
	if ((ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) &&
	    is_dsc_compression(&ctl->panel_data->panel_info) &&
	    ctl->panel_data->panel_info.partial_update_enabled &&
	    mdss_has_quirk(mdata, MDSS_QUIRK_DSC_RIGHT_ONLY_PU))
		mdss_mdp_reset_mixercfg(ctl_hw);

	if (!mixer->valid_roi) {
		/*
		 * resetting mixer config is specifically needed when split
		 * mode is MDP_DUAL_LM_SINGLE_DISPLAY but update is only on
		 * one side.
		 */
		__mdss_mdp_mixer_write_cfg(mixer_hw, NULL);

		MDSS_XLOG(mixer->num, mixer_hw->num, XLOG_FUNC_EXIT);
		return;
	}

	trace_mdp_mixer_update(mixer_hw->num);
	pr_debug("setup mixer=%d hw=%d\n", mixer->num, mixer_hw->num);
	screen_state = ctl->force_screen_state;

	outsize = (mixer->roi.h << 16) | mixer->roi.w;
	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_OUT_SIZE, outsize);

	if (screen_state == MDSS_SCREEN_FORCE_BLANK) {
		mixercfg.border_enabled = true;
		goto update_mixer;
	}

	pipe = mixer->stage_pipe[MDSS_MDP_STAGE_BASE * MAX_PIPES_PER_STAGE];
	if (pipe == NULL) {
		mixercfg.border_enabled = true;
	} else {
		__mdss_mdp_mixer_update_cfg_masks(pipe->num,
				pipe->multirect.num, MDSS_MDP_STAGE_BASE,
				&mixercfg);

		if (pipe->src_fmt->alpha_enable)
			bg_alpha_enable = 1;
	}

	i = MDSS_MDP_STAGE_0 * MAX_PIPES_PER_STAGE;
	for (; i < MAX_PIPES_PER_LM; i++) {
		pipe = mixer->stage_pipe[i];
		if (pipe == NULL)
			continue;

		stage = i / MAX_PIPES_PER_STAGE;
		if (stage != pipe->mixer_stage) {
			pr_warn("pipe%d rec%d mixer:%d stage mismatch. pipe->mixer_stage=%d, mixer->stage_pipe=%d multirect_mode=%d. skip staging it\n",
			    pipe->num, pipe->multirect.num, mixer->num,
			    pipe->mixer_stage, stage, pipe->multirect.mode);
			mixer->stage_pipe[i] = NULL;
			continue;
		}

		/*
		 * pipe which is staged on both LMs will be tracked through
		 * left mixer only.
		 */
		if (!pipe->src_split_req || !mixer->is_right_mixer)
			mixer->next_pipe_map |= pipe->ndx;

		blend_stage = stage - MDSS_MDP_STAGE_0;
		off = MDSS_MDP_REG_LM_BLEND_OFFSET(blend_stage);

		/*
		 * Account for additional blending stages
		 * from MDP v1.5 onwards
		 */
		if (blend_stage > 3)
			off += MDSS_MDP_REG_LM_BLEND_STAGE4;
		blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
			    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
		fg_alpha = pipe->alpha;
		bg_alpha = 0xFF - pipe->alpha;
		/* keep fg alpha */
		mixer_op_mode |= 1 << (blend_stage + 1);

		switch (pipe->blend_op) {
		case BLEND_OP_OPAQUE:

			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);

			pr_debug("pnum=%d stg=%d op=OPAQUE\n", pipe->num,
					stage);
			break;

		case BLEND_OP_PREMULTIPLIED:
			if (pipe->src_fmt->alpha_enable) {
				blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
					    MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL);
				if (fg_alpha != 0xff) {
					bg_alpha = fg_alpha;
					blend_op |=
						MDSS_MDP_BLEND_BG_MOD_ALPHA |
						MDSS_MDP_BLEND_BG_INV_MOD_ALPHA;
				} else {
					blend_op |= MDSS_MDP_BLEND_BG_INV_ALPHA;
				}
			}
			pr_debug("pnum=%d stg=%d op=PREMULTIPLIED\n", pipe->num,
					stage);
			break;

		case BLEND_OP_COVERAGE:
			if (pipe->src_fmt->alpha_enable) {
				blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_PIXEL |
					    MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL);
				if (fg_alpha != 0xff) {
					bg_alpha = fg_alpha;
					blend_op |=
					       MDSS_MDP_BLEND_FG_MOD_ALPHA |
					       MDSS_MDP_BLEND_FG_INV_MOD_ALPHA |
					       MDSS_MDP_BLEND_BG_MOD_ALPHA |
					       MDSS_MDP_BLEND_BG_INV_MOD_ALPHA;
				} else {
					blend_op |= MDSS_MDP_BLEND_BG_INV_ALPHA;
				}
			}
			pr_debug("pnum=%d stg=%d op=COVERAGE\n", pipe->num,
					stage);
			break;

		default:
			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
			pr_debug("pnum=%d stg=%d op=NONE\n", pipe->num,
					stage);
			break;
		}

		if (!pipe->src_fmt->alpha_enable && bg_alpha_enable)
			mixer_op_mode = 0;

		__mdss_mdp_mixer_update_cfg_masks(pipe->num,
				pipe->multirect.num, stage, &mixercfg);

		trace_mdp_sspp_change(pipe);

		pr_debug("stg=%d op=%x fg_alpha=%x bg_alpha=%x\n", stage,
					blend_op, fg_alpha, bg_alpha);
		mdp_mixer_write(mixer_hw,
			off + MDSS_MDP_REG_LM_OP_MODE, blend_op);
		mdp_mixer_write(mixer_hw,
			off + MDSS_MDP_REG_LM_BLEND_FG_ALPHA, fg_alpha);
		mdp_mixer_write(mixer_hw,
			off + MDSS_MDP_REG_LM_BLEND_BG_ALPHA, bg_alpha);
	}

	if (mixer->cursor_enabled)
		mixercfg.cursor_enabled = true;

update_mixer:
	mixer_num = __mdss_mdp_mixer_get_hw_num(mixer_hw);
	ctl_hw->flush_bits |= BIT(mixer_num < 5 ? 6 + mixer_num : 20);

	/* Read GC enable/disable status on LM */
	mixer_op_mode |=
		(mdp_mixer_read(mixer_hw, MDSS_MDP_REG_LM_OP_MODE) & BIT(0));

	if (mixer->src_split_req && mixer_mux == MDSS_MDP_MIXER_MUX_RIGHT)
		mixer_op_mode |= BIT(31);

	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_OP_MODE, mixer_op_mode);

	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_BORDER_COLOR_0,
		(mdata->bcolor0 & 0xFFF) | ((mdata->bcolor1 & 0xFFF) << 16));
	mdp_mixer_write(mixer_hw, MDSS_MDP_REG_LM_BORDER_COLOR_1,
		mdata->bcolor2 & 0xFFF);

	__mdss_mdp_mixer_write_cfg(mixer_hw, &mixercfg);

	pr_debug("mixer=%d hw=%d op_mode=0x%08x w=%d h=%d bc0=0x%x bc1=0x%x\n",
		mixer->num, mixer_hw->num,
		mixer_op_mode, mixer->roi.w, mixer->roi.h,
		(mdata->bcolor0 & 0xFFF) | ((mdata->bcolor1 & 0xFFF) << 16),
		mdata->bcolor2 & 0xFFF);
	MDSS_XLOG(mixer->num, mixer_hw->num,
		mixer_op_mode, mixer->roi.h, mixer->roi.w);
}

int mdss_mdp_mixer_addr_setup(struct mdss_data_type *mdata,
	 u32 *mixer_offsets, u32 *dspp_offsets, u32 *pingpong_offsets,
	 u32 type, u32 len)
{
	struct mdss_mdp_mixer *head;
	u32 i;
	int rc = 0;
	u32 size = len;

	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
			(mdata->wfd_mode == MDSS_MDP_WFD_SHARED))
		size++;

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_mixer) *
			size, GFP_KERNEL);

	if (!head) {
		pr_err("unable to setup mixer type=%d :kzalloc fail\n",
			type);
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].type = type;
		head[i].base = mdata->mdss_io.base + mixer_offsets[i];
		head[i].ref_cnt = 0;
		head[i].num = i;
		if (type == MDSS_MDP_MIXER_TYPE_INTF && dspp_offsets
				&& pingpong_offsets) {
			if (mdata->ndspp > i)
				head[i].dspp_base = mdata->mdss_io.base +
						dspp_offsets[i];
			head[i].pingpong_base = mdata->mdss_io.base +
					pingpong_offsets[i];
		}
	}

	/*
	 * Duplicate the last writeback mixer for concurrent line and block mode
	 * operations
	*/
	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) &&
			(mdata->wfd_mode == MDSS_MDP_WFD_SHARED))
		head[len] = head[len - 1];

	switch (type) {

	case MDSS_MDP_MIXER_TYPE_INTF:
		mdata->mixer_intf = head;
		break;

	case MDSS_MDP_MIXER_TYPE_WRITEBACK:
		mdata->mixer_wb = head;
		break;

	default:
		pr_err("Invalid mixer type=%d\n", type);
		rc = -EINVAL;
		break;
	}

	return rc;
}

int mdss_mdp_ctl_addr_setup(struct mdss_data_type *mdata,
	u32 *ctl_offsets,  u32 len)
{
	struct mdss_mdp_ctl *head;
	struct mutex *shared_lock = NULL;
	u32 i;
	u32 size = len;

	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED) {
		size++;
		shared_lock = devm_kzalloc(&mdata->pdev->dev,
					   sizeof(struct mutex),
					   GFP_KERNEL);
		if (!shared_lock) {
			pr_err("unable to allocate mem for mutex\n");
			return -ENOMEM;
		}
		mutex_init(shared_lock);
	}

	head = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_ctl) *
			size, GFP_KERNEL);

	if (!head) {
		pr_err("unable to setup ctl and wb: kzalloc fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < len; i++) {
		head[i].num = i;
		head[i].base = (mdata->mdss_io.base) + ctl_offsets[i];
		head[i].ref_cnt = 0;
	}

	if (mdata->wfd_mode == MDSS_MDP_WFD_SHARED) {
		head[len - 1].shared_lock = shared_lock;
		/*
		 * Allocate a virtual ctl to be able to perform simultaneous
		 * line mode and block mode operations on the same
		 * writeback block
		*/
		head[len] = head[len - 1];
		head[len].num = head[len - 1].num;
	}
	mdata->ctl_off = head;

	return 0;
}

int mdss_mdp_wb_addr_setup(struct mdss_data_type *mdata,
	u32 num_block_wb, u32 num_intf_wb)
{
	struct mdss_mdp_writeback *wb;
	u32 total, i;

	total = num_block_wb + num_intf_wb;
	wb = devm_kzalloc(&mdata->pdev->dev, sizeof(struct mdss_mdp_writeback) *
			total, GFP_KERNEL);
	if (!wb) {
		pr_err("unable to setup wb: kzalloc fail\n");
		return -ENOMEM;
	}

	for (i = 0; i < total; i++) {
		wb[i].num = i;
		if (i < num_block_wb) {
			wb[i].caps = MDSS_MDP_WB_ROTATOR | MDSS_MDP_WB_WFD;
			if (mdss_mdp_is_ubwc_supported(mdata))
				wb[i].caps |= MDSS_MDP_WB_UBWC;
		} else {
			wb[i].caps = MDSS_MDP_WB_WFD | MDSS_MDP_WB_INTF;
		}
	}

	mdata->wb = wb;
	mdata->nwb = total;
	mutex_init(&mdata->wb_lock);

	return 0;
}

int mdss_mdp_ds_addr_setup(struct mdss_data_type *mdata)
{
	struct mdss_mdp_destination_scaler *ds;
	struct mdss_mdp_mixer *mixer = mdata->mixer_intf;
	u32 num_ds_block;
	int i;

	num_ds_block = mdata->scaler_off->ndest_scalers;
	ds = devm_kcalloc(&mdata->pdev->dev, num_ds_block,
			sizeof(struct mdss_mdp_destination_scaler),
			GFP_KERNEL);
	if (!ds) {
		pr_err("unable to setup ds: kzalloc failed\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_ds_block; i++) {
		ds[i].num = i;
		ds[i].ds_base = mdata->scaler_off->dest_base;
		ds[i].scaler_base = mdata->scaler_off->dest_base +
			mdata->scaler_off->dest_scaler_off[i];
		ds[i].lut_base = mdata->scaler_off->dest_base +
			mdata->scaler_off->dest_scaler_lut_off[i];

		/*
		 * Assigning destination scaler to each LM. There is no dynamic
		 * assignment because destination scaler and LM are hard wired.
		 */
		if (i < mdata->nmixers_intf)
			mixer[i].ds = &ds[i];
	}

	mdata->ds = ds;

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux)
{
	struct mdss_mdp_mixer *mixer = NULL;

	if (!ctl) {
		pr_err("ctl not initialized\n");
		return NULL;
	}

	switch (mux) {
	case MDSS_MDP_MIXER_MUX_DEFAULT:
	case MDSS_MDP_MIXER_MUX_LEFT:
		mixer = ctl->mixer_left;
		break;
	case MDSS_MDP_MIXER_MUX_RIGHT:
		mixer = ctl->mixer_right;
		break;
	}

	return mixer;
}

struct mdss_mdp_pipe *mdss_mdp_get_staged_pipe(struct mdss_mdp_ctl *ctl,
	int mux, int stage, bool is_right_blend)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_mdp_mixer *mixer;
	int index = (stage * MAX_PIPES_PER_STAGE) + (int)is_right_blend;

	if (!ctl)
		return NULL;

	BUG_ON(index > MAX_PIPES_PER_LM);

	mixer = mdss_mdp_mixer_get(ctl, mux);
	if (mixer && (index < MAX_PIPES_PER_LM))
		pipe = mixer->stage_pipe[index];

	pr_debug("%pS index=%d pipe%d\n", __builtin_return_address(0),
		index, pipe ? pipe->num : -1);
	return pipe;
}

int mdss_mdp_get_pipe_flush_bits(struct mdss_mdp_pipe *pipe)
{
	if (WARN_ON(!pipe || pipe->num >= MDSS_MDP_MAX_SSPP))
		return 0;

	return BIT(mdp_pipe_hwio[pipe->num].flush_bit);
}

int mdss_mdp_async_ctl_flush(struct msm_fb_data_type *mfd,
		u32 flush_bits)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = mdp5_data->ctl;
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	int ret = 0;

	mutex_lock(&ctl->flush_lock);

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, flush_bits);
	if ((!ctl->split_flush_en) && sctl)
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_FLUSH, flush_bits);

	mutex_unlock(&ctl->flush_lock);
	return ret;
}

int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe,
			 struct mdss_mdp_mixer *mixer, int params_changed)
{
	struct mdss_mdp_ctl *ctl;
	int i, j, k;

	if (!pipe)
		return -EINVAL;
	if (!mixer)
		return -EINVAL;
	ctl = mixer->ctl;
	if (!ctl)
		return -EINVAL;

	if (pipe->mixer_stage >= MDSS_MDP_MAX_STAGE) {
		pr_err("invalid mixer stage\n");
		return -EINVAL;
	}

	pr_debug("pnum=%x mixer=%d stage=%d\n", pipe->num, mixer->num,
			pipe->mixer_stage);

	mutex_lock(&ctl->flush_lock);

	if (params_changed) {
		mixer->params_changed++;
		for (i = MDSS_MDP_STAGE_UNUSED; i < MDSS_MDP_MAX_STAGE; i++) {
			j = i * MAX_PIPES_PER_STAGE;

			/*
			 * this could lead to cases where left blend index is
			 * not populated. For instance, where pipe is spanning
			 * across layer mixers. But this is handled properly
			 * within mixer programming code.
			 */
			if (pipe->is_right_blend)
				j++;

			/* First clear all blend containers for current stage */
			for (k = 0; k < MAX_PIPES_PER_STAGE; k++) {
				u32 ndx = (i * MAX_PIPES_PER_STAGE) + k;

				if (mixer->stage_pipe[ndx] == pipe)
					mixer->stage_pipe[ndx] = NULL;
			}

			/* then stage actual pipe on specific blend container */
			if (i == pipe->mixer_stage)
				mixer->stage_pipe[j] = pipe;
		}
	}

	ctl->flush_bits |= mdss_mdp_get_pipe_flush_bits(pipe);

	mutex_unlock(&ctl->flush_lock);

	return 0;
}

/**
 * mdss_mdp_mixer_unstage_all() - Unstage all pipes from mixer
 * @mixer:	Mixer from which to unstage all pipes
 *
 * Unstage any pipes that are currently attached to mixer.
 *
 * NOTE: this will not update the pipe structure, and thus a full
 * deinitialization or reconfiguration of all pipes is expected after this call.
 */
void mdss_mdp_mixer_unstage_all(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *tmp;
	int i;

	if (!mixer)
		return;

	for (i = 0; i < MAX_PIPES_PER_LM; i++) {
		tmp = mixer->stage_pipe[i];
		if (tmp) {
			mixer->stage_pipe[i] = NULL;
			mixer->params_changed++;
			tmp->params_changed++;
		}
	}
}

int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe,
	struct mdss_mdp_mixer *mixer)
{
	int i, right_blend;

	if (!pipe)
		return -EINVAL;
	if (!mixer)
		return -EINVAL;

	right_blend = pipe->is_right_blend ? 1 : 0;
	i = (pipe->mixer_stage * MAX_PIPES_PER_STAGE) + right_blend;
	if ((i < MAX_PIPES_PER_LM) && (pipe == mixer->stage_pipe[i])) {
		pr_debug("unstage p%d from %s side of stage=%d lm=%d ndx=%d\n",
				pipe->num, right_blend ? "right" : "left",
				pipe->mixer_stage, mixer->num, i);
	} else {
		int stage;

		for (i = 0; i < MAX_PIPES_PER_LM; i++) {
			if (pipe != mixer->stage_pipe[i])
				continue;

			stage = i / MAX_PIPES_PER_STAGE;
			right_blend = i & 1;

			pr_warn("lm=%d pipe #%d stage=%d with %s blend, unstaged from %s side of stage=%d!\n",
				mixer->num, pipe->num, pipe->mixer_stage,
				pipe->is_right_blend ? "right" : "left",
				right_blend ? "right" : "left", stage);
			break;
		}

		/* pipe not found, not a failure */
		if (i == MAX_PIPES_PER_LM)
			return 0;
	}

	mixer->params_changed++;
	mixer->stage_pipe[i] = NULL;

	return 0;
}

int mdss_mdp_ctl_update_fps(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo;
	struct mdss_overlay_private *mdp5_data;
	int ret = 0;
	int new_fps;

	if (!ctl->panel_data || !ctl->mfd)
		return -ENODEV;

	pinfo = &ctl->panel_data->panel_info;

	if (!pinfo->dynamic_fps || !ctl->ops.config_fps_fnc)
		return 0;

	if (!pinfo->default_fps) {
		/* we haven't got any call to update the fps */
		return 0;
	}

	mdp5_data = mfd_to_mdp5_data(ctl->mfd);
	if (!mdp5_data)
		return -ENODEV;

	/*
	 * Panel info is already updated with the new fps info,
	 * so we need to lock the data to make sure the panel info
	 * is not updated while we reconfigure the HW.
	 */
	mutex_lock(&mdp5_data->dfps_lock);

	if ((pinfo->dfps_update == DFPS_IMMEDIATE_PORCH_UPDATE_MODE_VFP) ||
		(pinfo->dfps_update == DFPS_IMMEDIATE_PORCH_UPDATE_MODE_HFP) ||
		(pinfo->dfps_update ==
			DFPS_IMMEDIATE_MULTI_UPDATE_MODE_CLK_HFP) ||
		pinfo->dfps_update == DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
		new_fps = mdss_panel_get_framerate(pinfo);
	} else {
		new_fps = pinfo->new_fps;
	}

	pr_debug("fps new:%d old:%d\n", new_fps,
		pinfo->current_fps);

	if (new_fps == pinfo->current_fps) {
		pr_debug("FPS is already %d\n", new_fps);
		ret = 0;
		goto exit;
	}

	ret = ctl->ops.config_fps_fnc(ctl, new_fps);
	if (!ret)
		pr_debug("fps set to %d\n", new_fps);
	else
		pr_err("Failed to configure %d fps rc=%d\n",
			new_fps, ret);

exit:
	mutex_unlock(&mdp5_data->dfps_lock);
	return ret;
}

int mdss_mdp_display_wakeup_time(struct mdss_mdp_ctl *ctl,
				 ktime_t *wakeup_time)
{
	struct mdss_panel_info *pinfo;
	u64 clk_rate;
	u32 clk_period;
	u32 current_line, total_line;
	u32 time_of_line, time_to_vsync, adjust_line_ns;

	ktime_t current_time = ktime_get();

	if (!ctl->ops.read_line_cnt_fnc)
		return -ENOSYS;

	pinfo = &ctl->panel_data->panel_info;
	if (!pinfo)
		return -ENODEV;

	clk_rate = mdss_mdp_get_pclk_rate(ctl);

	clk_rate = DIV_ROUND_UP_ULL(clk_rate, 1000); /* in kHz */
	if (!clk_rate)
		return -EINVAL;

	/*
	 * calculate clk_period as pico second to maintain good
	 * accuracy with high pclk rate and this number is in 17 bit
	 * range.
	 */
	clk_period = DIV_ROUND_UP_ULL(1000000000, clk_rate);
	if (!clk_period)
		return -EINVAL;

	time_of_line = (pinfo->lcdc.h_back_porch +
		 pinfo->lcdc.h_front_porch +
		 pinfo->lcdc.h_pulse_width +
		 pinfo->xres) * clk_period;

	time_of_line /= 1000;	/* in nano second */
	if (!time_of_line)
		return -EINVAL;

	current_line = ctl->ops.read_line_cnt_fnc(ctl);

	total_line = pinfo->lcdc.v_back_porch +
		pinfo->lcdc.v_front_porch +
		pinfo->lcdc.v_pulse_width +
		pinfo->yres;

	if (current_line > total_line)
		return -EINVAL;

	time_to_vsync = time_of_line * (total_line - current_line);

	if (pinfo->adjust_timer_delay_ms) {
		adjust_line_ns = pinfo->adjust_timer_delay_ms
			* 1000000; /* convert to ns */

		/* Ignore large values of adjust_line_ns\ */
		if (time_to_vsync > adjust_line_ns)
			time_to_vsync -= adjust_line_ns;
	}

	if (!time_to_vsync)
		return -EINVAL;

	*wakeup_time = ktime_add_ns(current_time, time_to_vsync);

	pr_debug("clk_rate=%lldkHz clk_period=%d cur_line=%d tot_line=%d\n",
		clk_rate, clk_period, current_line, total_line);
	pr_debug("time_to_vsync=%d current_time=%d wakeup_time=%d\n",
		time_to_vsync, (int)ktime_to_ms(current_time),
		(int)ktime_to_ms(*wakeup_time));

	return 0;
}

int mdss_mdp_display_wait4comp(struct mdss_mdp_ctl *ctl)
{
	int ret;
	u32 reg_data, flush_data;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctl) {
		pr_err("invalid ctl\n");
		return -ENODEV;
	}

	ret = mutex_lock_interruptible(&ctl->lock);
	if (ret)
		return ret;

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	ATRACE_BEGIN("wait_fnc");
	if (ctl->ops.wait_fnc)
		ret = ctl->ops.wait_fnc(ctl, NULL);
	ATRACE_END("wait_fnc");

	trace_mdp_commit(ctl);

	mdss_mdp_ctl_perf_update(ctl, 0, false);
	mdata->bw_limit_pending = false;

	if (IS_MDSS_MAJOR_MINOR_SAME(mdata->mdp_rev, MDSS_MDP_HW_REV_103)) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		reg_data = mdss_mdp_ctl_read(ctl, MDSS_MDP_REG_CTL_FLUSH);
		flush_data = readl_relaxed(mdata->mdp_base + AHB_CLK_OFFSET);
		if ((flush_data & BIT(28)) &&
		    !(ctl->flush_reg_data & reg_data)) {

			flush_data &= ~(BIT(28));
			writel_relaxed(flush_data,
					 mdata->mdp_base + AHB_CLK_OFFSET);
			ctl->flush_reg_data = 0;
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	}

	mutex_unlock(&ctl->lock);
	return ret;
}

int mdss_mdp_display_wait4pingpong(struct mdss_mdp_ctl *ctl, bool use_lock)
{
	struct mdss_mdp_ctl *sctl = NULL;
	int ret;
	bool recovery_needed = false;

	if (use_lock) {
		ret = mutex_lock_interruptible(&ctl->lock);
		if (ret)
			return ret;
	}

	if (!mdss_mdp_ctl_is_power_on(ctl) || !ctl->ops.wait_pingpong) {
		if (use_lock)
			mutex_unlock(&ctl->lock);
		return 0;
	}

	ATRACE_BEGIN("wait_pingpong");
	ret = ctl->ops.wait_pingpong(ctl, NULL);
	ATRACE_END("wait_pingpong");
	if (ret)
		recovery_needed = true;

	sctl = mdss_mdp_get_split_ctl(ctl);

	if (sctl && sctl->ops.wait_pingpong) {
		ATRACE_BEGIN("wait_pingpong sctl");
		ret = sctl->ops.wait_pingpong(sctl, NULL);
		ATRACE_END("wait_pingpong sctl");
		if (ret)
			recovery_needed = true;
	}

	ctl->mdata->bw_limit_pending = false;
	if (recovery_needed) {
		mdss_mdp_ctl_reset(ctl, true);
		if (sctl)
			mdss_mdp_ctl_reset(sctl, true);

		mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_RESET_WRITE_PTR,
			NULL, CTL_INTF_EVENT_FLAG_DEFAULT);

		pr_debug("pingpong timeout recovery finished\n");
	}

	if (use_lock)
		mutex_unlock(&ctl->lock);

	return ret;
}

static void mdss_mdp_force_border_color(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool lm_swap = mdss_mdp_is_lm_swap_needed(mdata, ctl);

	ctl->force_screen_state = MDSS_SCREEN_FORCE_BLANK;

	if (sctl)
		sctl->force_screen_state = MDSS_SCREEN_FORCE_BLANK;

	mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_LEFT, lm_swap);
	mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_RIGHT, lm_swap);

	ctl->force_screen_state = MDSS_SCREEN_DEFAULT;
	if (sctl)
		sctl->force_screen_state = MDSS_SCREEN_DEFAULT;

	/*
	 * Update the params changed for mixer for the next frame to
	 * configure the mixer setup properly.
	 */
	if (ctl->mixer_left)
		ctl->mixer_left->params_changed++;
	if (ctl->mixer_right)
		ctl->mixer_right->params_changed++;
}

int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg,
	struct mdss_mdp_commit_cb *commit_cb)
{
	struct mdss_mdp_ctl *sctl = NULL;
	int ret = 0;
	bool is_bw_released, split_lm_valid;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 ctl_flush_bits = 0, sctl_flush_bits = 0;

	if (!ctl) {
		pr_err("display function not set\n");
		return -ENODEV;
	}

	mutex_lock(&ctl->lock);
	pr_debug("commit ctl=%d play_cnt=%d\n", ctl->num, ctl->play_cnt);

	if (!mdss_mdp_ctl_is_power_on(ctl)) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	split_lm_valid = mdss_mdp_is_both_lm_valid(ctl);

	sctl = mdss_mdp_get_split_ctl(ctl);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	if (ctl->ops.avr_ctrl_fnc) {
		ret = ctl->ops.avr_ctrl_fnc(ctl);
		if (ret) {
			pr_err("error configuring avr ctrl registers ctl=%d err=%d\n",
				ctl->num, ret);
			mutex_unlock(&ctl->lock);
			return ret;
		}
	}

	if (sctl && sctl->ops.avr_ctrl_fnc) {
		ret = sctl->ops.avr_ctrl_fnc(sctl);
		if (ret) {
			pr_err("error configuring avr ctrl registers sctl=%d err=%d\n",
				sctl->num, ret);
			mutex_unlock(&ctl->lock);
			return ret;
		}
	}

	mutex_lock(&ctl->flush_lock);

	/*
	 * We could have released the bandwidth if there were no transactions
	 * pending, so we want to re-calculate the bandwidth in this situation
	 */
	is_bw_released = !mdss_mdp_ctl_perf_get_transaction_status(ctl);
	if (is_bw_released) {
		if (sctl)
			is_bw_released =
				!mdss_mdp_ctl_perf_get_transaction_status(sctl);
	}

	/*
	 * left update on any topology or
	 * any update on MDP_DUAL_LM_SINGLE_DISPLAY topology.
	 */
	if (ctl->mixer_left->valid_roi ||
	    (is_dual_lm_single_display(ctl->mfd) &&
	     ctl->mixer_right->valid_roi))
		mdss_mdp_ctl_perf_set_transaction_status(ctl,
				PERF_SW_COMMIT_STATE, PERF_STATUS_BUSY);

	/* right update on MDP_DUAL_LM_DUAL_DISPLAY */
	if (sctl && sctl->mixer_left->valid_roi)
		mdss_mdp_ctl_perf_set_transaction_status(sctl,
			PERF_SW_COMMIT_STATE, PERF_STATUS_BUSY);

	if (ctl->mixer_right)
		ctl->mixer_right->src_split_req =
			mdata->has_src_split && split_lm_valid;

	if (is_bw_released || ctl->force_screen_state ||
	    (ctl->mixer_left->params_changed) ||
	    (ctl->mixer_right && ctl->mixer_right->params_changed)) {
		bool lm_swap = mdss_mdp_is_lm_swap_needed(mdata, ctl);

		ATRACE_BEGIN("prepare_fnc");
		if (ctl->ops.prepare_fnc)
			ret = ctl->ops.prepare_fnc(ctl, arg);
		ATRACE_END("prepare_fnc");
		if (ret) {
			pr_err("error preparing display\n");
			mutex_unlock(&ctl->flush_lock);
			goto done;
		}

		ATRACE_BEGIN("mixer_programming");
		mdss_mdp_ctl_perf_update(ctl, 1, false);

		mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_LEFT, lm_swap);
		mdss_mdp_mixer_setup(ctl, MDSS_MDP_MIXER_MUX_RIGHT, lm_swap);

		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, ctl->opmode);
		ctl->flush_bits |= BIT(17);	/* CTL */

		if (sctl) {
			mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP,
					sctl->opmode);
			sctl->flush_bits |= BIT(17);
			sctl_flush_bits = sctl->flush_bits;
		}
		ATRACE_END("mixer_programming");
	}

	/*
	 * With partial frame update, enable split display bit only
	 * when validity of ROI's on both the DSI's are identical.
	 */
	if (sctl)
		mdss_mdp_ctl_split_display_enable(split_lm_valid, ctl, sctl);

	ATRACE_BEGIN("postproc_programming");
	if (ctl->mfd && ctl->mfd->dcm_state != DTM_ENTER)
		/* postprocessing setup, including dspp */
		mdss_mdp_pp_setup_locked(ctl);

	if (sctl) {
		if (ctl->split_flush_en) {
			ctl->flush_bits |= sctl->flush_bits;
			sctl->flush_bits = 0;
			sctl_flush_bits = 0;
		} else {
			sctl_flush_bits = sctl->flush_bits;
		}
	}
	ctl_flush_bits = ctl->flush_bits;

	ATRACE_END("postproc_programming");

	mutex_unlock(&ctl->flush_lock);

	ATRACE_BEGIN("frame_ready");
	mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_CFG_DONE);
	if (commit_cb)
		commit_cb->commit_cb_fnc(
			MDP_COMMIT_STAGE_SETUP_DONE,
			commit_cb->data);
	ret = mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_READY);

	/*
	 * When wait for fence timed out, driver ignores the fences
	 * for signalling. Hardware needs to access only on the buffers
	 * that are valid and driver needs to ensure it. This function
	 * would set the mixer state to border when there is timeout.
	 */
	if (ret == NOTIFY_BAD) {
		mdss_mdp_force_border_color(ctl);
		ctl_flush_bits |= (ctl->flush_bits | BIT(17));
		if (sctl && (!ctl->split_flush_en))
			sctl_flush_bits |= (sctl->flush_bits | BIT(17));
		ret = 0;
	}

	ATRACE_END("frame_ready");

	if (ctl->ops.wait_pingpong && !mdata->serialize_wait4pp)
		mdss_mdp_display_wait4pingpong(ctl, false);

	/*
	 * if serialize_wait4pp is false then roi_bkup used in wait4pingpong
	 * will be of previous frame as expected.
	 */
	ctl->roi_bkup.w = ctl->roi.w;
	ctl->roi_bkup.h = ctl->roi.h;

	/*
	 * update roi of panel_info which will be
	 * used by dsi to set col_page addr of panel.
	 */
	if (ctl->panel_data &&
	    ctl->panel_data->panel_info.partial_update_enabled) {

		if (is_pingpong_split(ctl->mfd)) {
			bool pp_split = false;
			struct mdss_rect l_roi, r_roi, temp = {0};
			u32 opmode = mdss_mdp_ctl_read(ctl,
			     MDSS_MDP_REG_CTL_TOP) & ~0xF0; /* clear OUT_SEL */
			/*
			 * with pp split enabled, it is a requirement that both
			 * panels share equal load, so split-point is center.
			 */
			u32 left_panel_w = left_lm_w_from_mfd(ctl->mfd) / 2;

			mdss_rect_split(&ctl->roi, &l_roi, &r_roi,
				left_panel_w);

			/*
			 * If update is only on left panel then we still send
			 * zeroed out right panel ROIs to DSI driver. Based on
			 * zeroed ROI, DSI driver identifies which panel is not
			 * transmitting.
			 */
			ctl->panel_data->panel_info.roi = l_roi;
			ctl->panel_data->next->panel_info.roi = r_roi;

			/* based on the roi, update ctl topology */
			if (!mdss_rect_cmp(&temp, &l_roi) &&
			    !mdss_rect_cmp(&temp, &r_roi)) {
				/* left + right */
				opmode |= (ctl->intf_num << 4);
				pp_split = true;
			} else if (mdss_rect_cmp(&temp, &l_roi)) {
				/* right only */
				opmode |= (ctl->slave_intf_num << 4);
				pp_split = false;
			} else {
				/* left only */
				opmode |= (ctl->intf_num << 4);
				pp_split = false;
			}

			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, opmode);

			mdss_mdp_ctl_pp_split_display_enable(pp_split, ctl);
		} else {
			/*
			 * if single lm update on 3D mux topology, clear it.
			 */
			if ((is_dual_lm_single_display(ctl->mfd)) &&
			    (ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE) &&
			    (!mdss_mdp_is_both_lm_valid(ctl))) {

				u32 opmode = mdss_mdp_ctl_read(ctl,
					MDSS_MDP_REG_CTL_TOP);
			       opmode &= ~(0xF << 19); /* clear 3D Mux */

				mdss_mdp_ctl_write(ctl,
					MDSS_MDP_REG_CTL_TOP, opmode);
			}

			ctl->panel_data->panel_info.roi = ctl->roi;
			if (sctl && sctl->panel_data)
				sctl->panel_data->panel_info.roi = sctl->roi;
		}
	}

	if (commit_cb)
		commit_cb->commit_cb_fnc(MDP_COMMIT_STAGE_READY_FOR_KICKOFF,
			commit_cb->data);

	if (mdss_has_quirk(mdata, MDSS_QUIRK_BWCPANIC) &&
	    !bitmap_empty(mdata->bwc_enable_map, MAX_DRV_SUP_PIPES))
		mdss_mdp_bwcpanic_ctrl(mdata, true);

	ret = mdss_mdp_cwb_setup(ctl);
	if (ret)
		pr_warn("concurrent setup failed ctl=%d\n", ctl->num);

	ctl_flush_bits |= ctl->flush_bits;

	ATRACE_BEGIN("flush_kickoff");
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, ctl_flush_bits);
	if (sctl && sctl_flush_bits) {
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_FLUSH,
			sctl_flush_bits);
		sctl->flush_bits = 0;
	}
	MDSS_XLOG(ctl->intf_num, ctl_flush_bits, sctl_flush_bits,
		split_lm_valid);
	wmb();
	ctl->flush_reg_data = ctl_flush_bits;
	ctl->flush_bits = 0;

	mdss_mdp_mixer_update_pipe_map(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	mdss_mdp_mixer_update_pipe_map(ctl, MDSS_MDP_MIXER_MUX_RIGHT);

	/* right-only kickoff */
	if (!ctl->mixer_left->valid_roi &&
	    sctl && sctl->mixer_left->valid_roi) {
		/*
		 * Seperate kickoff on DSI1 is needed only when we have
		 * ONLY right half updating on a dual DSI panel
		 */
		if (sctl->ops.display_fnc)
			ret = sctl->ops.display_fnc(sctl, arg);
	} else {
		if (ctl->ops.display_fnc)
			ret = ctl->ops.display_fnc(ctl, arg); /* DSI0 kickoff */
	}

	if (ret)
		pr_warn("ctl %d error displaying frame\n", ctl->num);

	ctl->play_cnt++;
	ATRACE_END("flush_kickoff");

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	mutex_unlock(&ctl->lock);

	return ret;
}

void mdss_mdp_ctl_notifier_register(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier)
{
	struct mdss_mdp_ctl *sctl;

	blocking_notifier_chain_register(&ctl->notifier_head, notifier);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		blocking_notifier_chain_register(&sctl->notifier_head,
						notifier);
}

void mdss_mdp_ctl_notifier_unregister(struct mdss_mdp_ctl *ctl,
	struct notifier_block *notifier)
{
	struct mdss_mdp_ctl *sctl;
	blocking_notifier_chain_unregister(&ctl->notifier_head, notifier);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		blocking_notifier_chain_unregister(&sctl->notifier_head,
						notifier);
}

int mdss_mdp_ctl_notify(struct mdss_mdp_ctl *ctl, int event)
{
	return blocking_notifier_call_chain(&ctl->notifier_head, event, ctl);
}

int mdss_mdp_get_ctl_mixers(u32 fb_num, u32 *mixer_id)
{
	int i;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata;
	u32 mixer_cnt = 0;
	mutex_lock(&mdss_mdp_ctl_lock);
	mdata = mdss_mdp_get_mdata();
	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		if ((mdss_mdp_ctl_is_power_on(ctl)) && (ctl->mfd) &&
			(ctl->mfd->index == fb_num)) {
			if (ctl->mixer_left) {
				mixer_id[mixer_cnt] = ctl->mixer_left->num;
				mixer_cnt++;
			}
			if (mixer_cnt && ctl->mixer_right) {
				mixer_id[mixer_cnt] = ctl->mixer_right->num;
				mixer_cnt++;
			}
			if (mixer_cnt)
				break;
		}
	}
	mutex_unlock(&mdss_mdp_ctl_lock);
	return mixer_cnt;
}

/**
 * @mdss_mdp_ctl_mixer_switch() - return ctl mixer of @return_type
 * @ctl: Pointer to ctl structure to be switched.
 * @return_type: wb_type of the ctl to be switched to.
 *
 * Virtual mixer switch should be performed only when there is no
 * dedicated wfd block and writeback block is shared.
 */
struct mdss_mdp_ctl *mdss_mdp_ctl_mixer_switch(struct mdss_mdp_ctl *ctl,
					       u32 return_type)
{
	int i;
	struct mdss_data_type *mdata = ctl->mdata;

	if (ctl->wb_type == return_type) {
		mdata->mixer_switched = false;
		return ctl;
	}
	for (i = 0; i <= mdata->nctl; i++) {
		if (mdata->ctl_off[i].wb_type == return_type) {
			pr_debug("switching mixer from ctl=%d to ctl=%d\n",
				 ctl->num, mdata->ctl_off[i].num);
			mdata->mixer_switched = true;
			return mdata->ctl_off + i;
		}
	}
	pr_err("unable to switch mixer to type=%d\n", return_type);
	return NULL;
}

static int __mdss_mdp_mixer_handoff_helper(struct mdss_mdp_mixer *mixer,
	struct mdss_mdp_pipe *pipe)
{
	int rc = 0;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 right_blend = 0;

	if (!mixer) {
		rc = -EINVAL;
		goto error;
	}

	/*
	 * It is possible to have more the one pipe staged on a single
	 * layer mixer at same staging level.
	 */
	if (mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED] != NULL) {
		if (mdata->mdp_rev < MDSS_MDP_HW_REV_103) {
			pr_err("More than one pipe staged on mixer num %d\n",
				mixer->num);
			rc = -EINVAL;
			goto error;
		} else if (mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED + 1] !=
			NULL) {
			pr_err("More than two pipe staged on mixer num %d\n",
				mixer->num);
			rc = -EINVAL;
			goto error;
		} else {
			right_blend = 1;
		}
	}

	pr_debug("Staging pipe num %d on mixer num %d\n",
		pipe->num, mixer->num);
	mixer->stage_pipe[MDSS_MDP_STAGE_UNUSED + right_blend] = pipe;
	pipe->mixer_left = mixer;
	pipe->mixer_stage = MDSS_MDP_STAGE_UNUSED;

error:
	return rc;
}

/**
 * mdss_mdp_mixer_handoff() - Stages a given pipe on the appropriate mixer
 * @ctl:  pointer to the control structure associated with the overlay device.
 * @num:  the mixer number on which the pipe needs to be staged.
 * @pipe: pointer to the pipe to be staged.
 *
 * Function stages a given pipe on either the left mixer or the right mixer
 * for the control structre based on the mixer number. If the input mixer
 * number does not match either of the mixers then an error is returned.
 * This function is called during overlay handoff when certain pipes are
 * already staged by the bootloader.
 */
int mdss_mdp_mixer_handoff(struct mdss_mdp_ctl *ctl, u32 num,
	struct mdss_mdp_pipe *pipe)
{
	int rc = 0;
	struct mdss_mdp_mixer *mx_left = ctl->mixer_left;
	struct mdss_mdp_mixer *mx_right = ctl->mixer_right;

	/*
	 * For performance calculations, stage the handed off pipe
	 * as MDSS_MDP_STAGE_UNUSED
	 */
	if (mx_left && (mx_left->num == num)) {
		rc = __mdss_mdp_mixer_handoff_helper(mx_left, pipe);
	} else if (mx_right && (mx_right->num == num)) {
		rc = __mdss_mdp_mixer_handoff_helper(mx_right, pipe);
	} else {
		pr_err("pipe num %d staged on unallocated mixer num %d\n",
			pipe->num, num);
		rc = -EINVAL;
	}

	return rc;
}

struct mdss_mdp_writeback *mdss_mdp_wb_alloc(u32 caps, u32 reg_index)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_writeback *wb = NULL;
	int i;
	bool wb_virtual_on;

	wb_virtual_on = (mdata->nctl == mdata->nwb_offsets);

	if (wb_virtual_on && reg_index >= mdata->nwb_offsets)
		return NULL;

	mutex_lock(&mdata->wb_lock);

	for (i = 0; i < mdata->nwb; i++) {
		wb = mdata->wb + i;
		if ((wb->caps & caps) &&
			(atomic_read(&wb->kref.refcount) == 0)) {
			kref_init(&wb->kref);
			break;
		}
		wb = NULL;
	}
	mutex_unlock(&mdata->wb_lock);

	if (wb) {
		wb->base = mdata->mdss_io.base;
		if (wb_virtual_on)
			wb->base += mdata->wb_offsets[reg_index];
		else
			wb->base += mdata->wb_offsets[i];
	}

	return wb;
}

bool mdss_mdp_is_wb_mdp_intf(u32 num, u32 reg_index)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_writeback *wb = NULL;
	bool wb_virtual_on;

	wb_virtual_on = (mdata->nctl == mdata->nwb_offsets);

	if (num >= mdata->nwb || (wb_virtual_on && reg_index >=
			mdata->nwb_offsets))
		return false;

	wb = mdata->wb + num;
	if (!wb)
		return false;

	return (wb->caps & MDSS_MDP_WB_INTF) ? true : false;
}

struct mdss_mdp_writeback *mdss_mdp_wb_assign(u32 num, u32 reg_index)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_writeback *wb = NULL;
	bool wb_virtual_on;

	wb_virtual_on = (mdata->nctl == mdata->nwb_offsets);

	if (num >= mdata->nwb)
		return NULL;

	if (wb_virtual_on && reg_index >= mdata->nwb_offsets)
		return NULL;

	mutex_lock(&mdata->wb_lock);
	wb = mdata->wb + num;
	if (atomic_read(&wb->kref.refcount) == 0)
		kref_init(&wb->kref);
	else
		wb = NULL;
	mutex_unlock(&mdata->wb_lock);

	if (!wb)
		return NULL;

	wb->base = mdata->mdss_io.base;
	if (wb_virtual_on)
		wb->base += mdata->wb_offsets[reg_index];
	else
		wb->base += mdata->wb_offsets[num];

	return wb;
}

static void mdss_mdp_wb_release(struct kref *kref)
{
	struct mdss_mdp_writeback *wb =
		container_of(kref, struct mdss_mdp_writeback, kref);

	if (!wb)
		return;

	wb->base = NULL;
}

void mdss_mdp_wb_free(struct mdss_mdp_writeback *wb)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (kref_put_mutex(&wb->kref, mdss_mdp_wb_release,
			&mdata->wb_lock))
		mutex_unlock(&mdata->wb_lock);
}
