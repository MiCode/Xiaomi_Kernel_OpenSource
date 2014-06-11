/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include "mdss_mdp.h"

#define SMP_MB_SIZE		(mdss_res->smp_mb_size)
#define SMP_MB_CNT		(mdss_res->smp_mb_cnt)
#define SMP_ENTRIES_PER_MB	(SMP_MB_SIZE / 16)
#define SMP_MB_ENTRY_SIZE	16
#define MAX_BPP 4

#define PIPE_HALT_TIMEOUT_US	0x4000

/* following offsets are relative to ctrl register bit offset */
#define CLK_FORCE_ON_OFFSET	0x0
#define CLK_FORCE_OFF_OFFSET	0x1
/* following offsets are relative to status register bit offset */
#define CLK_STATUS_OFFSET	0x0

static DEFINE_MUTEX(mdss_mdp_sspp_lock);
static DEFINE_MUTEX(mdss_mdp_smp_lock);

static void mdss_mdp_pipe_free(struct kref *kref);
static int mdss_mdp_smp_mmb_set(int client_id, unsigned long *smp);
static void mdss_mdp_smp_mmb_free(unsigned long *smp, bool write);
static struct mdss_mdp_pipe *mdss_mdp_pipe_search_by_client_id(
	struct mdss_data_type *mdata, int client_id);

static inline void mdss_mdp_pipe_write(struct mdss_mdp_pipe *pipe,
				       u32 reg, u32 val)
{
	writel_relaxed(val, pipe->base + reg);
}

static inline u32 mdss_mdp_pipe_read(struct mdss_mdp_pipe *pipe, u32 reg)
{
	return readl_relaxed(pipe->base + reg);
}

static u32 mdss_mdp_smp_mmb_reserve(struct mdss_mdp_pipe_smp_map *smp_map,
	size_t n)
{
	u32 i, mmb;
	u32 fixed_cnt = bitmap_weight(smp_map->fixed, SMP_MB_CNT);
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (n <= fixed_cnt)
		return fixed_cnt;
	else
		n -= fixed_cnt;

	i = bitmap_weight(smp_map->allocated, SMP_MB_CNT);

	/*
	 * SMP programming is not double buffered. Fail the request,
	 * that calls for change in smp configuration (addition/removal
	 * of smp blocks), so that fallback solution happens.
	 */
	if (i != 0 && n != i) {
		pr_debug("Can't change mmb config, num_blks: %d alloc: %d\n",
			n, i);
		return 0;
	}

	/*
	 * Clear previous SMP reservations and reserve according to the
	 * latest configuration
	 */
	mdss_mdp_smp_mmb_free(smp_map->reserved, false);

	/* Reserve mmb blocks*/
	for (; i < n; i++) {
		if (bitmap_full(mdata->mmb_alloc_map, SMP_MB_CNT))
			break;

		mmb = find_first_zero_bit(mdata->mmb_alloc_map, SMP_MB_CNT);
		set_bit(mmb, smp_map->reserved);
		set_bit(mmb, mdata->mmb_alloc_map);
	}

	return i + fixed_cnt;
}

static int mdss_mdp_smp_mmb_set(int client_id, unsigned long *smp)
{
	u32 mmb, off, data, s;
	int cnt = 0;

	for_each_set_bit(mmb, smp, SMP_MB_CNT) {
		off = (mmb / 3) * 4;
		s = (mmb % 3) * 8;
		data = MDSS_MDP_REG_READ(MDSS_MDP_REG_SMP_ALLOC_W0 + off);
		data &= ~(0xFF << s);
		data |= client_id << s;
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_SMP_ALLOC_W0 + off, data);
		MDSS_MDP_REG_WRITE(MDSS_MDP_REG_SMP_ALLOC_R0 + off, data);
		cnt++;
	}
	return cnt;
}

static void mdss_mdp_smp_mmb_amend(unsigned long *smp, unsigned long *extra)
{
	bitmap_or(smp, smp, extra, SMP_MB_CNT);
	bitmap_zero(extra, SMP_MB_CNT);
}

static void mdss_mdp_smp_mmb_free(unsigned long *smp, bool write)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!bitmap_empty(smp, SMP_MB_CNT)) {
		if (write)
			mdss_mdp_smp_mmb_set(0, smp);
		bitmap_andnot(mdata->mmb_alloc_map, mdata->mmb_alloc_map,
			      smp, SMP_MB_CNT);
		bitmap_zero(smp, SMP_MB_CNT);
	}
}

/**
 * @mdss_mdp_smp_get_size - get allocated smp size for a pipe
 * @pipe: pointer to a pipe
 *
 * Function counts number of blocks that are currently allocated for a
 * pipe, then smp buffer size is number of blocks multiplied by block
 * size.
 */
u32 mdss_mdp_smp_get_size(struct mdss_mdp_pipe *pipe)
{
	int i, mb_cnt = 0;

	for (i = 0; i < MAX_PLANES; i++) {
		mb_cnt += bitmap_weight(pipe->smp_map[i].allocated, SMP_MB_CNT);
		mb_cnt += bitmap_weight(pipe->smp_map[i].fixed, SMP_MB_CNT);
	}

	return mb_cnt * SMP_MB_SIZE;
}

static void mdss_mdp_smp_set_wm_levels(struct mdss_mdp_pipe *pipe, int mb_cnt)
{
	u32 fetch_size, val, wm[3];

	fetch_size = mb_cnt * SMP_MB_SIZE;

	/*
	 * when doing hflip, one line is reserved to be consumed down the
	 * pipeline. This line will always be marked as full even if it doesn't
	 * have any data. In order to generate proper priority levels ignore
	 * this region while setting up watermark levels
	 */
	if (pipe->flags & MDP_FLIP_LR) {
		u8 bpp = pipe->src_fmt->is_yuv ? 1 :
			pipe->src_fmt->bpp;
		fetch_size -= (pipe->src.w * bpp);
	}

	/* 1/4 of SMP pool that is being fetched */
	val = (fetch_size / SMP_MB_ENTRY_SIZE) >> 2;

	wm[0] = val;
	wm[1] = wm[0] + val;
	wm[2] = wm[1] + val;

	pr_debug("pnum=%d fetch_size=%u watermarks %u,%u,%u\n", pipe->num,
			fetch_size, wm[0], wm[1], wm[2]);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_REQPRIO_FIFO_WM_0, wm[0]);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_REQPRIO_FIFO_WM_1, wm[1]);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_REQPRIO_FIFO_WM_2, wm[2]);
}

static void mdss_mdp_smp_free(struct mdss_mdp_pipe *pipe)
{
	int i;

	mutex_lock(&mdss_mdp_smp_lock);
	for (i = 0; i < MAX_PLANES; i++) {
		mdss_mdp_smp_mmb_free(pipe->smp_map[i].reserved, false);
		mdss_mdp_smp_mmb_free(pipe->smp_map[i].allocated, true);
	}
	mutex_unlock(&mdss_mdp_smp_lock);
}

void mdss_mdp_smp_unreserve(struct mdss_mdp_pipe *pipe)
{
	int i;

	mutex_lock(&mdss_mdp_smp_lock);
	for (i = 0; i < MAX_PLANES; i++)
		mdss_mdp_smp_mmb_free(pipe->smp_map[i].reserved, false);
	mutex_unlock(&mdss_mdp_smp_lock);
}

int mdss_mdp_smp_reserve(struct mdss_mdp_pipe *pipe)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 num_blks = 0, reserved = 0;
	struct mdss_mdp_plane_sizes ps;
	int i;
	int rc = 0, rot_mode = 0, wb_mixer = 0;
	u32 nlines, format, seg_w;
	u16 width;

	width = pipe->src.w >> pipe->horz_deci;

	if (pipe->bwc_mode) {
		rc = mdss_mdp_get_rau_strides(pipe->src.w, pipe->src.h,
			pipe->src_fmt, &ps);
		if (rc)
			return rc;
		/*
		 * Override fetch strides with SMP buffer size for both the
		 * planes. BWC line buffer needs to be divided into 16
		 * segments and every segment is aligned to format
		 * specific RAU size
		 */
		seg_w = DIV_ROUND_UP(pipe->src.w, 16);
		if (pipe->src_fmt->fetch_planes == MDSS_MDP_PLANE_INTERLEAVED) {
			ps.ystride[0] = ALIGN(seg_w, 32) * 16 * ps.rau_h[0] *
					pipe->src_fmt->bpp;
			ps.ystride[1] = 0;
		} else {
			u32 bwc_width = ALIGN(seg_w, 64) * 16;
			ps.ystride[0] = bwc_width * ps.rau_h[0];
			ps.ystride[1] = bwc_width * ps.rau_h[1];
			/*
			 * Since chroma for H1V2 is not subsampled it needs
			 * to be accounted for with bpp factor
			 */
			if (pipe->src_fmt->chroma_sample ==
				MDSS_MDP_CHROMA_H1V2)
				ps.ystride[1] *= 2;
		}
		pr_debug("BWC SMP strides ystride0=%x ystride1=%x\n",
			ps.ystride[0], ps.ystride[1]);
	} else {
		format = pipe->src_fmt->format;
		/*
		 * when decimation block is present, all chroma planes
		 * are fetched on a single SMP plane for chroma pixels
		 */
		if (mdata->has_decimation) {
			switch (pipe->src_fmt->chroma_sample) {
			case MDSS_MDP_CHROMA_H2V1:
				format = MDP_Y_CRCB_H2V1;
				break;
			case MDSS_MDP_CHROMA_420:
				format = MDP_Y_CBCR_H2V2;
				break;
			default:
				break;
			}
		}
		rc = mdss_mdp_get_plane_sizes(format, width, pipe->src.h,
			&ps, 0);
		if (rc)
			return rc;

		if (pipe->mixer && pipe->mixer->rotator_mode) {
			rot_mode = 1;
		} else if (ps.num_planes == 1) {
			ps.ystride[0] = MAX_BPP *
				max(pipe->mixer->width, width);
		} else if (mdata->has_decimation) {
			/*
			 * To avoid quailty loss, MDP does one less decimation
			 * on chroma components if they are subsampled.
			 * Account for this to have enough SMPs for latency
			 */
			switch (pipe->src_fmt->chroma_sample) {
			case MDSS_MDP_CHROMA_H2V1:
			case MDSS_MDP_CHROMA_420:
				ps.ystride[1] <<= 1;
				break;
			}
		}
	}

	if (pipe->src_fmt->tile)
		nlines = 8;
	else
		nlines = pipe->bwc_mode ? 1 : 2;

	if (pipe->mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK)
		wb_mixer = 1;

	mutex_lock(&mdss_mdp_smp_lock);
	for (i = (MAX_PLANES - 1); i >= ps.num_planes; i--) {
		if (bitmap_weight(pipe->smp_map[i].allocated, SMP_MB_CNT)) {
			pr_debug("Extra mmb identified for pnum=%d plane=%d\n",
				pipe->num, i);
			mutex_unlock(&mdss_mdp_smp_lock);
			return -EAGAIN;
		}
	}

	for (i = 0; i < ps.num_planes; i++) {
		if (rot_mode || wb_mixer) {
			num_blks = 1;
		} else {
			num_blks = DIV_ROUND_UP(ps.ystride[i] * nlines,
					SMP_MB_SIZE);

			if (mdata->mdp_rev == MDSS_MDP_HW_REV_100)
				num_blks = roundup_pow_of_two(num_blks);

			if (mdata->smp_mb_per_pipe &&
				(num_blks > mdata->smp_mb_per_pipe) &&
				!(pipe->flags & MDP_FLIP_LR))
				num_blks = mdata->smp_mb_per_pipe;
		}

		pr_debug("reserving %d mmb for pnum=%d plane=%d\n",
				num_blks, pipe->num, i);
		reserved = mdss_mdp_smp_mmb_reserve(&pipe->smp_map[i],
			num_blks);
		if (reserved < num_blks)
			break;
	}

	if (reserved < num_blks) {
		pr_debug("insufficient MMB blocks\n");
		for (; i >= 0; i--)
			mdss_mdp_smp_mmb_free(pipe->smp_map[i].reserved,
				false);
		rc = -ENOMEM;
	}
	mutex_unlock(&mdss_mdp_smp_lock);

	return rc;
}
/*
 * mdss_mdp_smp_alloc() -- set smp mmb and and wm levels for a staged pipe
 * @pipe: pointer to a pipe
 *
 * Function amends reserved smp mmbs to allocated bitmap and ties respective
 * mmbs to their pipe fetch_ids. Based on the number of total allocated mmbs
 * for a staged pipe, it also sets the watermark levels (wm).
 *
 * This function will be called on every commit where pipe params might not
 * have changed. In such cases, we need to ensure that wm levels are not
 * wiped out. Also in some rare situations hw might have reset and wiped out
 * smp mmb programming but new smp reservation is not done. In such cases we
 * need to ensure that for a staged pipes, mmbs are set properly based on
 * allocated bitmap.
 */
static int mdss_mdp_smp_alloc(struct mdss_mdp_pipe *pipe)
{
	int i;
	int cnt = 0;

	mutex_lock(&mdss_mdp_smp_lock);
	for (i = 0; i < MAX_PLANES; i++) {
		cnt += bitmap_weight(pipe->smp_map[i].fixed, SMP_MB_CNT);

		if (bitmap_empty(pipe->smp_map[i].reserved, SMP_MB_CNT)) {
			cnt += mdss_mdp_smp_mmb_set(pipe->ftch_id + i,
				pipe->smp_map[i].allocated);
			continue;
		}

		mdss_mdp_smp_mmb_amend(pipe->smp_map[i].allocated,
			pipe->smp_map[i].reserved);
		cnt += mdss_mdp_smp_mmb_set(pipe->ftch_id + i,
			pipe->smp_map[i].allocated);
	}
	mdss_mdp_smp_set_wm_levels(pipe, cnt);
	mutex_unlock(&mdss_mdp_smp_lock);
	return 0;
}

void mdss_mdp_smp_release(struct mdss_mdp_pipe *pipe)
{
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	mdss_mdp_smp_free(pipe);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
}

int mdss_mdp_smp_setup(struct mdss_data_type *mdata, u32 cnt, u32 size)
{
	if (!mdata)
		return -EINVAL;

	mdata->smp_mb_cnt = cnt;
	mdata->smp_mb_size = size;

	return 0;
}

/**
 * mdss_mdp_smp_handoff() - Handoff SMP MMBs in use by staged pipes
 * @mdata: pointer to the global mdss data structure.
 *
 * Iterate through the list of all SMP MMBs and check to see if any
 * of them are assigned to a pipe being marked as being handed-off.
 * If so, update the corresponding software allocation map to reflect
 * this.
 *
 * This function would typically be called during MDP probe for the case
 * when certain pipes might be programmed in the bootloader to display
 * the splash screen.
 */
int mdss_mdp_smp_handoff(struct mdss_data_type *mdata)
{
	int rc = 0;
	int i, client_id, prev_id = 0;
	u32 off, s, data;
	struct mdss_mdp_pipe *pipe = NULL;

	/*
	 * figure out what SMP MMBs are allocated for each of the pipes
	 * that need to be handed off.
	 */
	for (i = 0; i < SMP_MB_CNT; i++) {
		off = (i / 3) * 4;
		s = (i % 3) * 8;
		data = MDSS_MDP_REG_READ(MDSS_MDP_REG_SMP_ALLOC_W0 + off);
		client_id = (data >> s) & 0xFF;
		if (test_bit(i, mdata->mmb_alloc_map)) {
			/*
			 * Certain pipes may have a dedicated set of
			 * SMP MMBs statically allocated to them. In
			 * such cases, we do not need to do anything
			 * here.
			 */
			pr_debug("smp mmb %d already assigned to pipe %d (client_id %d)"
				, i, pipe->num, client_id);
			continue;
		}

		if (client_id) {
			if (client_id != prev_id) {
				pipe = mdss_mdp_pipe_search_by_client_id(mdata,
					client_id);
				prev_id = client_id;
			}

			if (!pipe) {
				pr_warn("Invalid client id %d for SMP MMB %d\n",
					client_id, i);
				continue;
			}

			if (!pipe->is_handed_off) {
				pr_warn("SMP MMB %d assigned to a pipe not marked for handoff (client id %d)"
					, i, client_id);
				continue;
			}

			/*
			 * Assume that the source format only has
			 * one plane
			 */
			pr_debug("Assigning smp mmb %d to pipe %d (client_id %d)\n"
				, i, pipe->num, client_id);
			set_bit(i, pipe->smp_map[0].allocated);
			set_bit(i, mdata->mmb_alloc_map);
		}
	}

	return rc;
}

void mdss_mdp_pipe_unmap(struct mdss_mdp_pipe *pipe)
{
	if (kref_put_mutex(&pipe->kref, mdss_mdp_pipe_free,
			&mdss_mdp_sspp_lock)) {
		WARN(1, "Unexpected free pipe during unmap");
		mutex_unlock(&mdss_mdp_sspp_lock);
	}
}

int mdss_mdp_pipe_map(struct mdss_mdp_pipe *pipe)
{
	if (!kref_get_unless_zero(&pipe->kref))
		return -EINVAL;
	return 0;
}

static struct mdss_mdp_pipe *mdss_mdp_pipe_init(struct mdss_mdp_mixer *mixer,
						u32 type, u32 off)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_data_type *mdata;
	struct mdss_mdp_pipe *pipe_pool = NULL;
	u32 npipes;
	bool pipe_share = false;
	u32 i;

	if (!mixer || !mixer->ctl || !mixer->ctl->mdata)
		return NULL;

	mdata = mixer->ctl->mdata;

	switch (type) {
	case MDSS_MDP_PIPE_TYPE_VIG:
		pipe_pool = mdata->vig_pipes;
		npipes = mdata->nvig_pipes;
		break;

	case MDSS_MDP_PIPE_TYPE_RGB:
		pipe_pool = mdata->rgb_pipes;
		npipes = mdata->nrgb_pipes;
		break;

	case MDSS_MDP_PIPE_TYPE_DMA:
		pipe_pool = mdata->dma_pipes;
		npipes = mdata->ndma_pipes;
		if (!mdata->has_wfd_blk &&
		   (mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK))
			pipe_share = true;
		break;

	default:
		npipes = 0;
		pr_err("invalid pipe type %d\n", type);
		break;
	}

	for (i = off; i < npipes; i++) {
		pipe = pipe_pool + i;
		if (atomic_read(&pipe->kref.refcount) == 0) {
			pipe->mixer = mixer;
			break;
		}
		pipe = NULL;
	}

	if (pipe && mdss_mdp_pipe_fetch_halt(pipe)) {
		pr_err("%d failed because pipe is in bad state\n",
			pipe->num);
		return NULL;
	}

	if (pipe) {
		pr_debug("type=%x   pnum=%d\n", pipe->type, pipe->num);
		mutex_init(&pipe->pp_res.hist.hist_mutex);
		spin_lock_init(&pipe->pp_res.hist.hist_lock);
		kref_init(&pipe->kref);
	} else if (pipe_share) {
		/*
		 * when there is no dedicated wfd blk, DMA pipe can be
		 * shared as long as its attached to a writeback mixer
		 */
		pipe = mdata->dma_pipes + mixer->num;
		kref_get(&pipe->kref);
		pr_debug("pipe sharing for pipe=%d\n", pipe->num);
	} else {
		pr_err("no %d type pipes available\n", type);
	}

	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_alloc_dma(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_data_type *mdata;

	mutex_lock(&mdss_mdp_sspp_lock);
	mdata = mixer->ctl->mdata;
	pipe = mdss_mdp_pipe_init(mixer, MDSS_MDP_PIPE_TYPE_DMA, mixer->num);
	if (!pipe) {
		pr_err("DMA pipes not available for mixer=%d\n", mixer->num);
	} else if (pipe != &mdata->dma_pipes[mixer->num]) {
		pr_err("Requested DMA pnum=%d not available\n",
			mdata->dma_pipes[mixer->num].num);
		kref_put(&pipe->kref, mdss_mdp_pipe_free);
		pipe = NULL;
	} else {
		pipe->mixer = mixer;
	}
	mutex_unlock(&mdss_mdp_sspp_lock);
	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_alloc(struct mdss_mdp_mixer *mixer,
						 u32 type)
{
	struct mdss_mdp_pipe *pipe;
	mutex_lock(&mdss_mdp_sspp_lock);
	pipe = mdss_mdp_pipe_init(mixer, type, 0);
	mutex_unlock(&mdss_mdp_sspp_lock);
	return pipe;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_get(struct mdss_data_type *mdata, u32 ndx)
{
	struct mdss_mdp_pipe *pipe = NULL;

	if (!ndx)
		return ERR_PTR(-EINVAL);

	mutex_lock(&mdss_mdp_sspp_lock);

	pipe = mdss_mdp_pipe_search(mdata, ndx);
	if (!pipe) {
		pipe = ERR_PTR(-EINVAL);
		goto error;
	}

	if (mdss_mdp_pipe_map(pipe))
		pipe = ERR_PTR(-EACCES);

error:
	mutex_unlock(&mdss_mdp_sspp_lock);
	return pipe;
}

static struct mdss_mdp_pipe *mdss_mdp_pipe_search_by_client_id(
	struct mdss_data_type *mdata, int client_id)
{
	u32 i;

	for (i = 0; i < mdata->nrgb_pipes; i++) {
		if (mdata->rgb_pipes[i].ftch_id == client_id)
			return &mdata->rgb_pipes[i];
	}

	for (i = 0; i < mdata->nvig_pipes; i++) {
		if (mdata->vig_pipes[i].ftch_id == client_id)
			return &mdata->vig_pipes[i];
	}

	for (i = 0; i < mdata->ndma_pipes; i++) {
		if (mdata->dma_pipes[i].ftch_id == client_id)
			return &mdata->dma_pipes[i];
	}

	return NULL;
}

struct mdss_mdp_pipe *mdss_mdp_pipe_search(struct mdss_data_type *mdata,
						  u32 ndx)
{
	u32 i;
	for (i = 0; i < mdata->nvig_pipes; i++) {
		if (mdata->vig_pipes[i].ndx == ndx)
			return &mdata->vig_pipes[i];
	}

	for (i = 0; i < mdata->nrgb_pipes; i++) {
		if (mdata->rgb_pipes[i].ndx == ndx)
			return &mdata->rgb_pipes[i];
	}

	for (i = 0; i < mdata->ndma_pipes; i++) {
		if (mdata->dma_pipes[i].ndx == ndx)
			return &mdata->dma_pipes[i];
	}

	return NULL;
}

static void mdss_mdp_pipe_free(struct kref *kref)
{
	struct mdss_mdp_pipe *pipe;

	pipe = container_of(kref, struct mdss_mdp_pipe, kref);

	pr_debug("ndx=%x pnum=%d\n", pipe->ndx, pipe->num);

	if (pipe->play_cnt) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		mdss_mdp_pipe_fetch_halt(pipe);
		mdss_mdp_pipe_sspp_term(pipe);
		mdss_mdp_smp_free(pipe);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	} else {
		mdss_mdp_smp_unreserve(pipe);
	}

	pipe->flags = 0;
	pipe->bwc_mode = 0;
	pipe->mfd = NULL;
	memset(&pipe->scale, 0, sizeof(struct mdp_scale_data));
}

static int mdss_mdp_is_pipe_idle(struct mdss_mdp_pipe *pipe,
	bool ignore_force_on)
{
	u32 reg_val;
	u32 vbif_idle_mask, forced_on_mask, clk_status_idle_mask;
	bool is_idle = false, is_forced_on;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	forced_on_mask = BIT(pipe->clk_ctrl.bit_off + CLK_FORCE_ON_OFFSET);
	reg_val = readl_relaxed(mdata->mdp_base + pipe->clk_ctrl.reg_off);
	is_forced_on = (reg_val & forced_on_mask) ? true : false;

	pr_debug("pipe#:%d clk_ctrl: 0x%x forced_on_mask: 0x%x\n", pipe->num,
		reg_val, forced_on_mask);
	/* if forced on then no need to check status */
	if (!is_forced_on) {
		clk_status_idle_mask =
			BIT(pipe->clk_status.bit_off + CLK_STATUS_OFFSET);
		reg_val = readl_relaxed(mdata->mdp_base +
			pipe->clk_status.reg_off);

		if (reg_val & clk_status_idle_mask)
			is_idle = false;

		pr_debug("pipe#:%d clk_status:0x%x clk_status_idle_mask:0x%x\n",
			pipe->num, reg_val, clk_status_idle_mask);
	}

	if (!ignore_force_on && (is_forced_on || !is_idle))
		goto exit;

	vbif_idle_mask = BIT(pipe->xin_id + 16);
	reg_val = readl_relaxed(mdata->vbif_base + MMSS_VBIF_XIN_HALT_CTRL1);

	if (reg_val & vbif_idle_mask)
		is_idle = true;

	pr_debug("pipe#:%d XIN_HALT_CTRL1: 0x%x\n", pipe->num, reg_val);

exit:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	return is_idle;
}

/**
 * mdss_mdp_pipe_fetch_halt() - Halt VBIF client corresponding to specified pipe
 * @pipe: pointer to the pipe data structure which needs to be halted.
 *
 * Check if VBIF client corresponding to specified pipe is idle or not. If not
 * send a halt request for the client in question and wait for it be idle.
 *
 * This function would typically be called after pipe is unstaged or before it
 * is initialized. On success it should be assumed that pipe is in idle state
 * and would not fetch any more data. This function cannot be called from
 * interrupt context.
 */
int mdss_mdp_pipe_fetch_halt(struct mdss_mdp_pipe *pipe)
{
	bool is_idle;
	int rc = 0;
	u32 reg_val, idle_mask, status;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	is_idle = mdss_mdp_is_pipe_idle(pipe, true);
	if (!is_idle) {
		pr_err("%pS: pipe%d is not idle. xin_id=%d\n",
			__builtin_return_address(0), pipe->num, pipe->xin_id);

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		mutex_lock(&mdata->reg_lock);
		idle_mask = BIT(pipe->xin_id + 16);

		reg_val = readl_relaxed(mdata->vbif_base +
			MMSS_VBIF_XIN_HALT_CTRL0);
		writel_relaxed(reg_val | BIT(pipe->xin_id),
			mdata->vbif_base + MMSS_VBIF_XIN_HALT_CTRL0);
		mutex_unlock(&mdata->reg_lock);

		rc = readl_poll_timeout(mdata->vbif_base +
			MMSS_VBIF_XIN_HALT_CTRL1, status, (status & idle_mask),
			1000, PIPE_HALT_TIMEOUT_US);
		if (rc == -ETIMEDOUT)
			pr_err("VBIF client %d not halting. TIMEDOUT.\n",
				pipe->xin_id);
		else
			pr_debug("VBIF client %d is halted\n", pipe->xin_id);

		mutex_lock(&mdata->reg_lock);
		reg_val = readl_relaxed(mdata->vbif_base +
			MMSS_VBIF_XIN_HALT_CTRL0);
		writel_relaxed(reg_val & ~BIT(pipe->xin_id),
			mdata->vbif_base + MMSS_VBIF_XIN_HALT_CTRL0);

		mutex_unlock(&mdata->reg_lock);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	}

	return rc;
}

int mdss_mdp_pipe_destroy(struct mdss_mdp_pipe *pipe)
{
	if (!kref_put_mutex(&pipe->kref, mdss_mdp_pipe_free,
			&mdss_mdp_sspp_lock)) {
		pr_err("unable to free pipe %d while still in use\n",
				pipe->num);
		return -EBUSY;
	}

	mutex_unlock(&mdss_mdp_sspp_lock);

	return 0;
}

/**
 * mdss_mdp_pipe_handoff() - Handoff staged pipes during bootup
 * @pipe: pointer to the pipe to be handed-off
 *
 * Populate the software structures for the pipe based on the current
 * configuration of the hardware pipe by the reading the appropriate MDP
 * registers.
 *
 * This function would typically be called during MDP probe for the case
 * when certain pipes might be programmed in the bootloader to display
 * the splash screen.
 */
int mdss_mdp_pipe_handoff(struct mdss_mdp_pipe *pipe)
{
	int rc = 0;
	u32 src_fmt, reg = 0, bpp = 0;

	/*
	 * todo: for now, only reading pipe src and dest size details
	 * from the registers. This is needed for appropriately
	 * calculating perf metrics for the handed off pipes.
	 * We may need to parse some more details at a later date.
	 */
	reg = mdss_mdp_pipe_read(pipe, MDSS_MDP_REG_SSPP_SRC_SIZE);
	pipe->src.h = reg >> 16;
	pipe->src.w = reg & 0xFFFF;
	reg = mdss_mdp_pipe_read(pipe, MDSS_MDP_REG_SSPP_OUT_SIZE);
	pipe->dst.h = reg >> 16;
	pipe->dst.w = reg & 0xFFFF;

	/* Assume that the source format is RGB */
	reg = mdss_mdp_pipe_read(pipe, MDSS_MDP_REG_SSPP_SRC_FORMAT);
	bpp = ((reg >> 9) & 0x3) + 1;
	switch (bpp) {
	case 4:
		src_fmt = MDP_RGBA_8888;
		break;
	case 3:
		src_fmt = MDP_RGB_888;
		break;
	case 2:
		src_fmt = MDP_RGB_565;
		break;
	default:
		pr_err("Invalid bpp=%d found\n", bpp);
		rc = -EINVAL;
		goto error;
	}
	pipe->src_fmt = mdss_mdp_get_format_params(src_fmt);

	pr_debug("Pipe settings: src.h=%d src.w=%d dst.h=%d dst.w=%d bpp=%d\n"
		, pipe->src.h, pipe->src.w, pipe->dst.h, pipe->dst.w,
		pipe->src_fmt->bpp);

	pipe->is_handed_off = true;
	pipe->play_cnt = 1;
	kref_init(&pipe->kref);

error:
	return rc;
}



static int mdss_mdp_image_setup(struct mdss_mdp_pipe *pipe,
					struct mdss_mdp_data *data)
{
	u32 img_size, src_size, src_xy, dst_size, dst_xy, ystride0, ystride1;
	u32 width, height;
	u32 decimation;
	struct mdss_mdp_img_rect sci, dst, src;
	int ret = 0;

	pr_debug("pnum=%d wh=%dx%d src={%d,%d,%d,%d} dst={%d,%d,%d,%d}\n",
			pipe->num, pipe->img_width, pipe->img_height,
			pipe->src.x, pipe->src.y, pipe->src.w, pipe->src.h,
			pipe->dst.x, pipe->dst.y, pipe->dst.w, pipe->dst.h);

	width = pipe->img_width;
	height = pipe->img_height;
	mdss_mdp_get_plane_sizes(pipe->src_fmt->format, width, height,
			&pipe->src_planes, pipe->bwc_mode);

	if (data != NULL) {
		ret = mdss_mdp_data_check(data, &pipe->src_planes);
		if (ret)
			return ret;
	}

	if ((pipe->flags & MDP_DEINTERLACE) &&
			!(pipe->flags & MDP_SOURCE_ROTATED_90)) {
		int i;
		for (i = 0; i < pipe->src_planes.num_planes; i++)
			pipe->src_planes.ystride[i] *= 2;
		width *= 2;
		height /= 2;
	}

	decimation = ((1 << pipe->horz_deci) - 1) << 8;
	decimation |= ((1 << pipe->vert_deci) - 1);
	if (decimation)
		pr_debug("Image decimation h=%d v=%d\n",
				pipe->horz_deci, pipe->vert_deci);

	sci = pipe->mixer->ctl->roi;
	dst = pipe->dst;
	src = pipe->src;

	if (pipe->mixer->type == MDSS_MDP_MIXER_TYPE_INTF)
		mdss_mdp_crop_rect(&src, &dst, &sci);

	src_size = (src.h << 16) | src.w;
	src_xy = (src.y << 16) | src.x;
	dst_size = (dst.h << 16) | dst.w;
	dst_xy = (dst.y << 16) | dst.x;

	ystride0 =  (pipe->src_planes.ystride[0]) |
			(pipe->src_planes.ystride[1] << 16);
	ystride1 =  (pipe->src_planes.ystride[2]) |
			(pipe->src_planes.ystride[3] << 16);

	/*
	 * Software overfetch is used when scalar pixel extension is
	 * not enabled
	 */
	if (pipe->overfetch_disable && !pipe->scale.enable_pxl_ext) {
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_BOTTOM) {
			height = pipe->src.h;
			if (!(pipe->overfetch_disable & OVERFETCH_DISABLE_TOP))
				height += pipe->src.y;
		}
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_RIGHT) {
			width = pipe->src.w;
			if (!(pipe->overfetch_disable & OVERFETCH_DISABLE_LEFT))
				width += pipe->src.x;
		}
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_LEFT)
			src_xy &= ~0xFFFF;
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_TOP)
			src_xy &= ~(0xFFFF << 16);

		pr_debug("overfetch w=%d/%d h=%d/%d src_xy=0x%08x\n", width,
			pipe->img_width, height, pipe->img_height, src_xy);
	}
	img_size = (height << 16) | width;

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_IMG_SIZE, img_size);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_SIZE, src_size);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_XY, src_xy);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_SIZE, dst_size);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_OUT_XY, dst_xy);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_YSTRIDE0, ystride0);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_YSTRIDE1, ystride1);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_DECIMATION_CONFIG,
		decimation);

	return 0;
}

static int mdss_mdp_format_setup(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_format_params *fmt;
	u32 chroma_samp, unpack, src_format;
	u32 secure = 0;
	u32 opmode;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	fmt = pipe->src_fmt;

	if (pipe->flags & MDP_SECURE_OVERLAY_SESSION)
		secure = 0xF;

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

	if (fmt->tile)
		src_format |= BIT(30);

	if (pipe->flags & MDP_ROT_90)
		src_format |= BIT(11); /* ROT90 */

	if (fmt->alpha_enable &&
			fmt->fetch_planes != MDSS_MDP_PLANE_INTERLEAVED)
		src_format |= BIT(8); /* SRCC3_EN */

	unpack = (fmt->element[3] << 24) | (fmt->element[2] << 16) |
			(fmt->element[1] << 8) | (fmt->element[0] << 0);
	src_format |= ((fmt->unpack_count - 1) << 12) |
			(fmt->unpack_tight << 17) |
			(fmt->unpack_align_msb << 18) |
			((fmt->bpp - 1) << 9);

	mdss_mdp_pipe_sspp_setup(pipe, &opmode);

	if (pipe->scale.enable_pxl_ext)
		opmode |= (1 << 31);

	if (fmt->tile && mdata->highest_bank_bit) {
		mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_FETCH_CONFIG,
			MDSS_MDP_FETCH_CONFIG_RESET_VALUE |
				 mdata->highest_bank_bit << 18);
	}
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_FORMAT, src_format);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_UNPACK_PATTERN, unpack);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_OP_MODE, opmode);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_ADDR_SW_STATUS, secure);

	return 0;
}

int mdss_mdp_pipe_addr_setup(struct mdss_data_type *mdata,
	struct mdss_mdp_pipe *head, u32 *offsets, u32 *ftch_id, u32 *xin_id,
	u32 type, u32 num_base, u32 len)
{
	u32 i;

	if (!head || !mdata) {
		pr_err("unable to setup pipe type=%d: invalid input\n", type);
		return -EINVAL;
	}

	for (i = 0; i < len; i++) {
		head[i].type = type;
		head[i].ftch_id  = ftch_id[i];
		head[i].xin_id = xin_id[i];
		head[i].num = i + num_base;
		head[i].ndx = BIT(i + num_base);
		head[i].base = mdata->mdp_base + offsets[i];
	}

	return 0;
}

static int mdss_mdp_src_addr_setup(struct mdss_mdp_pipe *pipe,
				   struct mdss_mdp_data *src_data)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_data data = *src_data;
	int ret = 0;

	pr_debug("pnum=%d\n", pipe->num);

	data.bwc_enabled = pipe->bwc_mode;

	ret = mdss_mdp_data_check(&data, &pipe->src_planes);
	if (ret)
		return ret;

	if (pipe->overfetch_disable && !pipe->scale.enable_pxl_ext) {
		u32 x = 0, y = 0;

		if (pipe->overfetch_disable & OVERFETCH_DISABLE_LEFT)
			x = pipe->src.x;
		if (pipe->overfetch_disable & OVERFETCH_DISABLE_TOP)
			y = pipe->src.y;

		mdss_mdp_data_calc_offset(&data, x, y,
			&pipe->src_planes, pipe->src_fmt);
	}

	/* planar format expects YCbCr, swap chroma planes if YCrCb */
	if (mdata->mdp_rev < MDSS_MDP_HW_REV_102 &&
			(pipe->src_fmt->fetch_planes == MDSS_MDP_PLANE_PLANAR)
				&& (pipe->src_fmt->element[0] == C1_B_Cb))
		swap(data.p[1].addr, data.p[2].addr);

	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC0_ADDR, data.p[0].addr);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC1_ADDR, data.p[1].addr);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC2_ADDR, data.p[2].addr);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC3_ADDR, data.p[3].addr);

	return 0;
}

static int mdss_mdp_pipe_solidfill_setup(struct mdss_mdp_pipe *pipe)
{
	int ret;
	u32 secure, format, unpack;

	pr_debug("solid fill setup on pnum=%d\n", pipe->num);

	ret = mdss_mdp_image_setup(pipe, NULL);
	if (ret) {
		pr_err("image setup error for pnum=%d\n", pipe->num);
		return ret;
	}

	format = MDSS_MDP_FMT_SOLID_FILL;
	secure = (pipe->flags & MDP_SECURE_OVERLAY_SESSION ? 0xF : 0x0);

	/* support ARGB color format only */
	unpack = (C3_ALPHA << 24) | (C2_R_Cr << 16) |
		(C1_B_Cb << 8) | (C0_G_Y << 0);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_FORMAT, format);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_CONSTANT_COLOR,
		pipe->bg_color);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_UNPACK_PATTERN, unpack);
	mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_SSPP_SRC_ADDR_SW_STATUS, secure);

	return 0;
}

int mdss_mdp_pipe_queue_data(struct mdss_mdp_pipe *pipe,
			     struct mdss_mdp_data *src_data)
{
	int ret = 0;
	struct mdss_mdp_ctl *ctl;
	u32 params_changed;
	u32 opmode = 0;

	if (!pipe) {
		pr_err("pipe not setup properly for queue\n");
		return -ENODEV;
	}

	if (!pipe->mixer || !pipe->mixer->ctl) {
		pr_err("pipe mixer not setup properly for queue\n");
		return -ENODEV;
	}

	pr_debug("pnum=%x mixer=%d play_cnt=%u\n", pipe->num,
		 pipe->mixer->num, pipe->play_cnt);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	ctl = pipe->mixer->ctl;
	/*
	 * Reprogram the pipe when there is no dedicated wfd blk and
	 * virtual mixer is allocated for the DMA pipe during concurrent
	 * line and block mode operations
	 */
	params_changed = (pipe->params_changed) ||
			 ((pipe->type == MDSS_MDP_PIPE_TYPE_DMA) &&
			 (pipe->mixer->type == MDSS_MDP_MIXER_TYPE_WRITEBACK)
			 && (ctl->mdata->mixer_switched)) ||
			 ctl->roi_changed;
	if (src_data == NULL || (pipe->flags & MDP_SOLID_FILL)) {
		pipe->params_changed = 0;
		mdss_mdp_pipe_solidfill_setup(pipe);
		goto update_nobuf;
	}

	if (params_changed) {
		pipe->params_changed = 0;

		ret = mdss_mdp_pipe_pp_setup(pipe, &opmode);
		if (ret) {
			pr_err("pipe pp setup error for pnum=%d\n", pipe->num);
			goto done;
		}

		ret = mdss_mdp_image_setup(pipe, src_data);
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
			mdss_mdp_pipe_write(pipe, MDSS_MDP_REG_VIG_OP_MODE,
			opmode);
	}

	mdss_mdp_smp_alloc(pipe);
	ret = mdss_mdp_src_addr_setup(pipe, src_data);
	if (ret) {
		pr_err("addr setup error for pnum=%d\n", pipe->num);
		goto done;
	}

update_nobuf:
	mdss_mdp_mixer_pipe_update(pipe, params_changed);

	pipe->play_cnt++;

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	return ret;
}

int mdss_mdp_pipe_is_staged(struct mdss_mdp_pipe *pipe)
{
	return (pipe == pipe->mixer->stage_pipe[pipe->mixer_stage]);
}

static inline void __mdss_mdp_pipe_program_pixel_extn_helper(
	struct mdss_mdp_pipe *pipe, u32 plane, u32 off)
{
	u32 src_h = pipe->src.h >> pipe->vert_deci;
	u32 mask = 0xFF;

	/*
	 * CB CR plane required pxls need to be accounted
	 * for chroma decimation.
	 */
	if (plane == 1)
		src_h >>= pipe->chroma_sample_v;
	writel_relaxed(((pipe->scale.right_ftch[plane] & mask) << 24)|
		((pipe->scale.right_rpt[plane] & mask) << 16)|
		((pipe->scale.left_ftch[plane] & mask) << 8)|
		(pipe->scale.left_rpt[plane] & mask), pipe->base +
			MDSS_MDP_REG_SSPP_SW_PIX_EXT_C0_LR + off);
	writel_relaxed(((pipe->scale.btm_ftch[plane] & mask) << 24)|
		((pipe->scale.btm_rpt[plane] & mask) << 16)|
		((pipe->scale.top_ftch[plane] & mask) << 8)|
		(pipe->scale.top_rpt[plane] & mask), pipe->base +
			MDSS_MDP_REG_SSPP_SW_PIX_EXT_C0_TB + off);
	mask = 0xFFFF;
	writel_relaxed((((src_h + pipe->scale.num_ext_pxls_top[plane] +
		pipe->scale.num_ext_pxls_btm[plane]) & mask) << 16) |
		((pipe->scale.roi_w[plane] +
		pipe->scale.num_ext_pxls_left[plane] +
		pipe->scale.num_ext_pxls_right[plane]) & mask), pipe->base +
			MDSS_MDP_REG_SSPP_SW_PIX_EXT_C0_REQ_PIXELS + off);
}

/**
 * mdss_mdp_pipe_program_pixel_extn - Program the source pipe's
 *				      sw pixel extension
 * @pipe:	Source pipe struct containing pixel extn values
 *
 * Function programs the pixel extn values calculated during
 * scale setup.
 */
int mdss_mdp_pipe_program_pixel_extn(struct mdss_mdp_pipe *pipe)
{
	/* Y plane pixel extn */
	__mdss_mdp_pipe_program_pixel_extn_helper(pipe, 0, 0);
	/* CB CR plane pixel extn */
	__mdss_mdp_pipe_program_pixel_extn_helper(pipe, 1, 16);
	/* Alpha plane pixel extn */
	__mdss_mdp_pipe_program_pixel_extn_helper(pipe, 3, 32);
	return 0;
}
