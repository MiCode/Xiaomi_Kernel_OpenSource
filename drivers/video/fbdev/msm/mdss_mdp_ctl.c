/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include "mdss_fb.h"
#include "mdss_mdp.h"

/* truncate at 1k */
#define MDSS_MDP_BUS_FACTOR_SHIFT 10
/* 1.5 bus fudge factor */
#define MDSS_MDP_BUS_FUDGE_FACTOR_IB(val) (((val) / 2) * 3)
#define MDSS_MDP_BUS_FUDGE_FACTOR_HIGH_IB(val) (val << 1)
#define MDSS_MDP_BUS_FUDGE_FACTOR_AB(val) (val << 1)
#define MDSS_MDP_BUS_FLOOR_BW (1600000000ULL >> MDSS_MDP_BUS_FACTOR_SHIFT)

/* 1.25 clock fudge factor */
#define MDSS_MDP_CLK_FUDGE_FACTOR(val) (((val) * 5) / 4)

enum {
	MDSS_MDP_PERF_UPDATE_SKIP,
	MDSS_MDP_PERF_UPDATE_EARLY,
	MDSS_MDP_PERF_UPDATE_LATE,
};

#define MDSS_MDP_PERF_UPDATE_CLK BIT(0)
#define MDSS_MDP_PERF_UPDATE_BUS BIT(1)
#define MDSS_MDP_PERF_UPDATE_ALL -1

static DEFINE_MUTEX(mdss_mdp_ctl_lock);

static int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer);
static inline int __mdss_mdp_ctl_get_mixer_off(struct mdss_mdp_mixer *mixer);

static inline void mdp_mixer_write(struct mdss_mdp_mixer *mixer,
				   u32 reg, u32 val)
{
	writel_relaxed(val, mixer->base + reg);
}

static inline u32 mdss_mdp_get_pclk_rate(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

	return (ctl->intf_type == MDSS_INTF_DSI) ?
		pinfo->mipi.dsi_pclk_rate :
		pinfo->clk_rate;
}

static u32 __mdss_mdp_ctrl_perf_ovrd_helper(struct mdss_mdp_mixer *mixer,
		u32 *npipe)
{
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_pipe *pipe;
	u32 mnum, ovrd = 0;

	if (!mixer || !mixer->ctl->panel_data)
		return 0;

	pinfo = &mixer->ctl->panel_data->panel_info;
	for (mnum = 0; mnum < MDSS_MDP_MAX_STAGE; mnum++) {
		pipe = mixer->stage_pipe[mnum];
		if (pipe && pinfo) {
			*npipe = *npipe + 1;
			if ((pipe->src.w >= pipe->src.h) &&
					(pipe->src.w >= pinfo->xres))
				ovrd = 1;
		}
	}

	return ovrd;
}

/**
 * mdss_mdp_ctrl_perf_ovrd() - Determines if performance override is needed
 * @mdata:	Struct containing references to all MDP5 hardware structures
 *		and status info such as interupts, target caps etc.
 * @ab_quota:	Arbitrated bandwidth quota
 * @ib_quota:	Instantaneous bandwidth quota
 *
 * Function calculates the minimum required MDP and BIMC clocks to avoid MDP
 * underflow during portrait video playback. The calculations are based on the
 * way MDP fetches (bandwidth requirement) and processes data through
 * MDP pipeline (MDP clock requirement) based on frame size and scaling
 * requirements.
 */
static void __mdss_mdp_ctrl_perf_ovrd(struct mdss_data_type *mdata,
	u64 *ab_quota, u64 *ib_quota)
{
	struct mdss_mdp_ctl *ctl;
	u32 i, npipe = 0, ovrd = 0;

	for (i = 0; i < mdata->nctl; i++) {
		ctl = mdata->ctl_off + i;
		if (!ctl->power_on)
			continue;
		ovrd |= __mdss_mdp_ctrl_perf_ovrd_helper(
				ctl->mixer_left, &npipe);
		ovrd |= __mdss_mdp_ctrl_perf_ovrd_helper(
				ctl->mixer_right, &npipe);
	}

	*ab_quota = MDSS_MDP_BUS_FUDGE_FACTOR_AB(*ab_quota);
	if (npipe > 1)
		*ib_quota = MDSS_MDP_BUS_FUDGE_FACTOR_HIGH_IB(*ib_quota);
	else
		*ib_quota = MDSS_MDP_BUS_FUDGE_FACTOR_IB(*ib_quota);

	if (ovrd && (*ib_quota < MDSS_MDP_BUS_FLOOR_BW)) {
		*ib_quota = MDSS_MDP_BUS_FLOOR_BW;
		pr_debug("forcing the BIMC clock to 200 MHz : %llu bytes",
			*ib_quota);
	} else {
		pr_debug("ib quota : %llu bytes", *ib_quota);
	}
}

static int mdss_mdp_ctl_perf_commit(struct mdss_data_type *mdata, u32 flags)
{
	struct mdss_mdp_ctl *ctl;
	int cnum;
	unsigned long clk_rate = 0;
	u64 bus_ab_quota = 0, bus_ib_quota = 0;

	if (!flags) {
		pr_err("nothing to update\n");
		return -EINVAL;
	}

	mutex_lock(&mdss_mdp_ctl_lock);
	for (cnum = 0; cnum < mdata->nctl; cnum++) {
		ctl = mdata->ctl_off + cnum;
		if (ctl->power_on) {
			bus_ab_quota += ctl->bus_ab_quota;
			bus_ib_quota += ctl->bus_ib_quota;

			if (ctl->clk_rate > clk_rate)
				clk_rate = ctl->clk_rate;
		}
	}
	if (flags & MDSS_MDP_PERF_UPDATE_BUS) {
		bus_ab_quota = bus_ib_quota;
		__mdss_mdp_ctrl_perf_ovrd(mdata, &bus_ab_quota, &bus_ib_quota);
		bus_ib_quota <<= MDSS_MDP_BUS_FACTOR_SHIFT;
		bus_ab_quota <<= MDSS_MDP_BUS_FACTOR_SHIFT;
		mdss_mdp_bus_scale_set_quota(bus_ab_quota, bus_ib_quota);
	}
	if (flags & MDSS_MDP_PERF_UPDATE_CLK) {
		clk_rate = MDSS_MDP_CLK_FUDGE_FACTOR(clk_rate);
		pr_debug("update clk rate = %lu HZ\n", clk_rate);
		mdss_mdp_set_clk_rate(clk_rate);
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

/**
 * mdss_mdp_perf_calc_pipe() - calculate performance numbers required by pipe
 * @pipe:	Source pipe struct containing updated pipe params
 * @perf:	Structure containing values that should be updated for
 *		performance tuning
 *
 * Function calculates the minimum required performance calculations in order
 * to avoid MDP underflow. The calculations are based on the way MDP
 * fetches (bandwidth requirement) and processes data through MDP pipeline
 * (MDP clock requirement) based on frame size and scaling requirements.
 */
int mdss_mdp_perf_calc_pipe(struct mdss_mdp_pipe *pipe,
		struct mdss_mdp_perf_params *perf)
{
	struct mdss_mdp_mixer *mixer;
	int fps = DEFAULT_FRAME_RATE;
	u32 quota, rate, v_total, src_h;

	if (!pipe || !perf || !pipe->mixer)
		return -EINVAL;

	mixer = pipe->mixer;
	if (mixer->rotator_mode) {
		v_total = pipe->flags & MDP_ROT_90 ? pipe->dst.w : pipe->dst.h;
	} else if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		struct mdss_panel_info *pinfo;

		pinfo = &mixer->ctl->panel_data->panel_info;
		fps = mdss_panel_get_framerate(pinfo);
		v_total = mdss_panel_get_vtotal(pinfo);
	} else {
		v_total = mixer->height;
	}

	/*
	 * when doing vertical decimation lines will be skipped, hence there is
	 * no need to account for these lines in MDP clock or request bus
	 * bandwidth to fetch them.
	 */
	src_h = pipe->src.h >> pipe->vert_deci;

	quota = fps * pipe->src.w * src_h;
	if (pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_420)
		/*
		 * with decimation, chroma is not downsampled, this means we
		 * need to allocate bw for extra lines that will be fetched
		 */
		if (pipe->vert_deci)
			quota *= 2;
		else
			quota = (quota * 3) / 2;
	else
		quota *= pipe->src_fmt->bpp;

	rate = pipe->dst.w;
	if (src_h > pipe->dst.h)
		rate = (rate * src_h) / pipe->dst.h;

	rate *= v_total * fps;
	if (mixer->rotator_mode) {
		rate /= 4; /* block mode fetch at 4 pix/clk */
		quota *= 2; /* bus read + write */
		perf->ib_quota = quota;
	} else {
		perf->ib_quota = (quota / pipe->dst.h) * v_total;
	}
	perf->ab_quota = quota;
	perf->mdp_clk_rate = rate;

	pr_debug("mixer=%d pnum=%d clk_rate=%u bus ab=%u ib=%u\n",
		 mixer->num, pipe->num, rate, perf->ab_quota, perf->ib_quota);

	return 0;
}

static void mdss_mdp_perf_mixer_update(struct mdss_mdp_mixer *mixer,
				       u32 *bus_ab_quota, u32 *bus_ib_quota,
				       u32 *clk_rate)
{
	struct mdss_mdp_pipe *pipe;
	struct mdss_panel_info *pinfo = NULL;
	int fps = DEFAULT_FRAME_RATE;
	u32 v_total;
	int i;
	u32 max_clk_rate = 0, ab_total = 0, ib_total = 0;

	*bus_ab_quota = 0;
	*bus_ib_quota = 0;
	*clk_rate = 0;

	if (!mixer->rotator_mode) {
		if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
			pinfo = &mixer->ctl->panel_data->panel_info;
			fps = mdss_panel_get_framerate(pinfo);
			v_total = mdss_panel_get_vtotal(pinfo);

			if (pinfo->type == WRITEBACK_PANEL)
				pinfo = NULL;
		} else {
			v_total = mixer->height;
		}
		*clk_rate = mixer->width * v_total * fps;
		if (pinfo && pinfo->lcdc.v_back_porch < MDP_MIN_VBP)
			*clk_rate = MDSS_MDP_CLK_FUDGE_FACTOR(*clk_rate);

		if (!pinfo) {
			/* perf for bus writeback */
			*bus_ab_quota = fps * mixer->width * mixer->height * 3;
			*bus_ab_quota >>= MDSS_MDP_BUS_FACTOR_SHIFT;
			*bus_ib_quota = *bus_ab_quota;
		}
	}

	for (i = 0; i < MDSS_MDP_MAX_STAGE; i++) {
		struct mdss_mdp_perf_params perf;
		pipe = mixer->stage_pipe[i];
		if (pipe == NULL)
			continue;

		if (mdss_mdp_perf_calc_pipe(pipe, &perf))
			continue;

		ab_total += perf.ab_quota >> MDSS_MDP_BUS_FACTOR_SHIFT;
		ib_total += perf.ib_quota >> MDSS_MDP_BUS_FACTOR_SHIFT;
		if (perf.mdp_clk_rate > max_clk_rate)
			max_clk_rate = perf.mdp_clk_rate;
	}

	*bus_ab_quota += ab_total;
	*bus_ib_quota += ib_total;
	if (max_clk_rate > *clk_rate)
		*clk_rate = max_clk_rate;

	pr_debug("final mixer=%d clk_rate=%u bus ab=%u ib=%u\n", mixer->num,
		 *clk_rate, *bus_ab_quota, *bus_ib_quota);
}

static int mdss_mdp_ctl_perf_update(struct mdss_mdp_ctl *ctl)
{
	int ret = MDSS_MDP_PERF_UPDATE_SKIP;
	u32 clk_rate, ab_quota, ib_quota;
	u32 max_clk_rate = 0, total_ab_quota = 0, total_ib_quota = 0;

	if (ctl->mixer_left) {
		mdss_mdp_perf_mixer_update(ctl->mixer_left, &ab_quota,
					   &ib_quota, &clk_rate);
		total_ab_quota += ab_quota;
		total_ib_quota += ib_quota;
		max_clk_rate = clk_rate;
	}

	if (ctl->mixer_right) {
		mdss_mdp_perf_mixer_update(ctl->mixer_right, &ab_quota,
					   &ib_quota, &clk_rate);
		total_ab_quota += ab_quota;
		total_ib_quota += ib_quota;
		if (clk_rate > max_clk_rate)
			max_clk_rate = clk_rate;

		if (ctl->intf_type) {
			clk_rate = mdss_mdp_get_pclk_rate(ctl);
			/* minimum clock rate due to inefficiency in 3dmux */
			clk_rate = mult_frac(clk_rate >> 1, 9, 8);
			if (clk_rate > max_clk_rate)
				max_clk_rate = clk_rate;
		}
	}

	/* request minimum bandwidth to have bus clock on when display is on */
	if (total_ib_quota == 0)
		total_ib_quota = SZ_16M >> MDSS_MDP_BUS_FACTOR_SHIFT;

	if (max_clk_rate != ctl->clk_rate) {
		if (max_clk_rate > ctl->clk_rate)
			ret = MDSS_MDP_PERF_UPDATE_EARLY;
		else
			ret = MDSS_MDP_PERF_UPDATE_LATE;
		ctl->clk_rate = max_clk_rate;
		ctl->perf_changed |= MDSS_MDP_PERF_UPDATE_CLK;
	}

	if ((total_ab_quota != ctl->bus_ab_quota) ||
			(total_ib_quota != ctl->bus_ib_quota)) {
		if (ret == MDSS_MDP_PERF_UPDATE_SKIP) {
			if (total_ib_quota >= ctl->bus_ib_quota)
				ret = MDSS_MDP_PERF_UPDATE_EARLY;
			else
				ret = MDSS_MDP_PERF_UPDATE_LATE;
		}
		ctl->bus_ab_quota = total_ab_quota;
		ctl->bus_ib_quota = total_ib_quota;
		ctl->perf_changed |= MDSS_MDP_PERF_UPDATE_BUS;
	}

	return ret;
}

static struct mdss_mdp_ctl *mdss_mdp_ctl_alloc(struct mdss_data_type *mdata,
					       u32 off)
{
	struct mdss_mdp_ctl *ctl = NULL;
	u32 cnum;
	u32 nctl = mdata->nctl;

	mutex_lock(&mdss_mdp_ctl_lock);
	if (!mdata->has_wfd_blk)
		nctl++;

	for (cnum = off; cnum < nctl; cnum++) {
		ctl = mdata->ctl_off + cnum;
		if (ctl->ref_cnt == 0) {
			ctl->ref_cnt++;
			ctl->mdata = mdata;
			mutex_init(&ctl->lock);
			pr_debug("alloc ctl_num=%d\n", ctl->num);
			break;
		}
		ctl = NULL;
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return ctl;
}

static int mdss_mdp_ctl_free(struct mdss_mdp_ctl *ctl)
{
	if (!ctl)
		return -ENODEV;

	pr_debug("free ctl_num=%d ref_cnt=%d\n", ctl->num, ctl->ref_cnt);

	if (!ctl->ref_cnt) {
		pr_err("called with ref_cnt=0\n");
		return -EINVAL;
	}

	if (ctl->mixer_left) {
		mdss_mdp_mixer_free(ctl->mixer_left);
		ctl->mixer_left = NULL;
	}
	if (ctl->mixer_right) {
		mdss_mdp_mixer_free(ctl->mixer_right);
		ctl->mixer_right = NULL;
	}
	mutex_lock(&mdss_mdp_ctl_lock);
	ctl->ref_cnt--;
	ctl->intf_num = MDSS_MDP_NO_INTF;
	ctl->intf_type = MDSS_MDP_NO_INTF;
	ctl->is_secure = false;
	ctl->power_on = false;
	ctl->start_fnc = NULL;
	ctl->stop_fnc = NULL;
	ctl->prepare_fnc = NULL;
	ctl->display_fnc = NULL;
	ctl->wait_fnc = NULL;
	ctl->read_line_cnt_fnc = NULL;
	ctl->add_vsync_handler = NULL;
	ctl->remove_vsync_handler = NULL;
	ctl->config_fps_fnc = NULL;
	ctl->panel_data = NULL;
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

static struct mdss_mdp_mixer *mdss_mdp_mixer_alloc(
		struct mdss_mdp_ctl *ctl, u32 type, int mux)
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
		if (ctl->mdata->has_wb_ad && ctl->intf_num) {
			alt_mixer = mixer_pool;
			mixer_pool++;
			nmixers--;
		}
		break;

	case MDSS_MDP_MIXER_TYPE_WRITEBACK:
		mixer_pool = ctl->mdata->mixer_wb;
		nmixers = nmixers_wb;
		break;

	default:
		nmixers = 0;
		pr_err("invalid pipe type %d\n", type);
		break;
	}

	/* early mdp revision only supports mux of dual pipe on mixers 0 and 1,
	 * need to ensure that these pipes are readily available by using
	 * mixer 2 if available and mux is not required */
	if (!mux && (ctl->mdata->mdp_rev == MDSS_MDP_HW_REV_100) &&
			(type == MDSS_MDP_MIXER_TYPE_INTF) &&
			(nmixers >= MDSS_MDP_INTF_LAYERMIXER2) &&
			(mixer_pool[MDSS_MDP_INTF_LAYERMIXER2].ref_cnt == 0))
		mixer_pool += MDSS_MDP_INTF_LAYERMIXER2;

	/*Allocate virtual wb mixer if no dedicated wfd wb blk is present*/
	if (!ctl->mdata->has_wfd_blk && (type == MDSS_MDP_MIXER_TYPE_WRITEBACK))
		nmixers += 1;

	for (i = 0; i < nmixers; i++) {
		mixer = mixer_pool + i;
		if (mixer->ref_cnt == 0) {
			mixer->ref_cnt++;
			mixer->params_changed++;
			mixer->ctl = ctl;
			pr_debug("alloc mixer num %d for ctl=%d\n",
				 mixer->num, ctl->num);
			break;
		}
		mixer = NULL;
	}

	if (!mixer && alt_mixer && (alt_mixer->ref_cnt == 0))
		mixer = alt_mixer;
	mutex_unlock(&mdss_mdp_ctl_lock);

	return mixer;
}

static int mdss_mdp_mixer_free(struct mdss_mdp_mixer *mixer)
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
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_wb_mixer_alloc(int rotator)
{
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_mixer *mixer = NULL;

	ctl = mdss_mdp_ctl_alloc(mdss_res, mdss_res->nmixers_intf);
	if (!ctl) {
		pr_debug("unable to allocate wb ctl\n");
		return NULL;
	}

	mixer = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_WRITEBACK, false);
	if (!mixer) {
		pr_debug("unable to allocate wb mixer\n");
		goto error;
	}

	mixer->rotator_mode = rotator;

	switch (mixer->num) {
	case MDSS_MDP_WB_LAYERMIXER0:
		ctl->opmode = (rotator ? MDSS_MDP_CTL_OP_ROT0_MODE :
			       MDSS_MDP_CTL_OP_WB0_MODE);
		break;
	case MDSS_MDP_WB_LAYERMIXER1:
		ctl->opmode = (rotator ? MDSS_MDP_CTL_OP_ROT1_MODE :
			       MDSS_MDP_CTL_OP_WB1_MODE);
		break;
	default:
		pr_err("invalid layer mixer=%d\n", mixer->num);
		goto error;
	}

	ctl->mixer_left = mixer;

	ctl->start_fnc = mdss_mdp_writeback_start;
	ctl->power_on = true;
	ctl->wb_type = (rotator ? MDSS_MDP_WB_CTL_TYPE_BLOCK :
			MDSS_MDP_WB_CTL_TYPE_LINE);
	mixer->ctl = ctl;

	if (ctl->start_fnc)
		ctl->start_fnc(ctl);

	return mixer;
error:
	if (mixer)
		mdss_mdp_mixer_free(mixer);
	if (ctl)
		mdss_mdp_ctl_free(ctl);

	return NULL;
}

int mdss_mdp_wb_mixer_destroy(struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_ctl *ctl;

	ctl = mixer->ctl;
	mixer->rotator_mode = 0;

	pr_debug("destroy ctl=%d mixer=%d\n", ctl->num, mixer->num);

	if (ctl->stop_fnc)
		ctl->stop_fnc(ctl);

	mdss_mdp_ctl_free(ctl);

	mdss_mdp_ctl_perf_commit(ctl->mdata, MDSS_MDP_PERF_UPDATE_ALL);

	return 0;
}

int mdss_mdp_ctl_splash_finish(struct mdss_mdp_ctl *ctl)
{
	switch (ctl->panel_data->panel_info.type) {
	case MIPI_VIDEO_PANEL:
	case EDP_PANEL:
		return mdss_mdp_video_reconfigure_splash_done(ctl);
	case MIPI_CMD_PANEL:
		return mdss_mdp_cmd_reconfigure_splash_done(ctl);
	default:
		return 0;
	}
}

static inline int mdss_mdp_set_split_ctl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_ctl *split_ctl)
{
	if (!ctl || !split_ctl)
		return -ENODEV;

	/* setup split ctl mixer as right mixer of original ctl so that
	 * original ctl can work the same way as dual pipe solution */
	ctl->mixer_right = split_ctl->mixer_left;

	return 0;
}

static inline struct mdss_mdp_ctl *mdss_mdp_get_split_ctl(
		struct mdss_mdp_ctl *ctl)
{
	if (ctl && ctl->mixer_right && (ctl->mixer_right->ctl != ctl))
		return ctl->mixer_right->ctl;

	return NULL;
}

static int mdss_mdp_ctl_fbc_enable(int enable,
		struct mdss_mdp_mixer *mixer, struct mdss_panel_info *pdata)
{
	struct fbc_panel_info *fbc;
	u32 mode = 0, budget_ctl = 0, lossy_mode = 0;

	if (!pdata) {
		pr_err("Invalid pdata\n");
		return -EINVAL;
	}

	fbc = &pdata->fbc;

	if (!fbc || !fbc->enabled) {
		pr_err("Invalid FBC structure\n");
		return -EINVAL;
	}

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER0)
		pr_debug("Mixer supports FBC.\n");
	else {
		pr_debug("Mixer doesn't support FBC.\n");
		return -EINVAL;
	}

	if (enable) {
		mode = ((pdata->xres) << 16) | ((fbc->comp_mode) << 8) |
			((fbc->qerr_enable) << 7) | ((fbc->cd_bias) << 4) |
			((fbc->pat_enable) << 3) | ((fbc->vlc_enable) << 2) |
			((fbc->bflc_enable) << 1) | enable;

		budget_ctl = ((fbc->line_x_budget) << 12) |
			((fbc->block_x_budget) << 8) | fbc->block_budget;

		lossy_mode = ((fbc->lossless_mode_thd) << 16) |
			((fbc->lossy_mode_thd) << 8) |
			((fbc->lossy_rgb_thd) << 3) | fbc->lossy_mode_idx;
	}

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_FBC_MODE, mode);
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_FBC_BUDGET_CTL,
			budget_ctl);
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_FBC_LOSSY_MODE,
			lossy_mode);

	return 0;
}

int mdss_mdp_ctl_setup(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *split_ctl;
	u32 width, height;
	int split_fb;

	if (!ctl || !ctl->panel_data) {
		pr_err("invalid ctl handle\n");
		return -ENODEV;
	}

	split_ctl = mdss_mdp_get_split_ctl(ctl);

	width = ctl->panel_data->panel_info.xres;
	height = ctl->panel_data->panel_info.yres;

	split_fb = (ctl->mfd->split_fb_left &&
		    ctl->mfd->split_fb_right &&
		    (ctl->mfd->split_fb_left <= MAX_MIXER_WIDTH) &&
		    (ctl->mfd->split_fb_right <= MAX_MIXER_WIDTH)) ? 1 : 0;
	pr_debug("max=%d xres=%d left=%d right=%d\n", MAX_MIXER_WIDTH,
		 width, ctl->mfd->split_fb_left, ctl->mfd->split_fb_right);

	if ((split_ctl && (width > MAX_MIXER_WIDTH)) ||
			(width > (2 * MAX_MIXER_WIDTH))) {
		pr_err("Unsupported panel resolution: %dx%d\n", width, height);
		return -ENOTSUPP;
	}

	ctl->width = width;
	ctl->height = height;

	if (!ctl->mixer_left) {
		ctl->mixer_left =
			mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_INTF,
			 ((width > MAX_MIXER_WIDTH) || split_fb));
		if (!ctl->mixer_left) {
			pr_err("unable to allocate layer mixer\n");
			return -ENOMEM;
		}
	}

	if (split_fb)
		width = ctl->mfd->split_fb_left;
	else if (width > MAX_MIXER_WIDTH)
		width /= 2;

	ctl->mixer_left->width = width;
	ctl->mixer_left->height = height;

	if (split_ctl) {
		pr_debug("split display detected\n");
		return 0;
	}

	if (split_fb)
		width = ctl->mfd->split_fb_right;

	if (width < ctl->width) {
		if (ctl->mixer_right == NULL) {
			ctl->mixer_right = mdss_mdp_mixer_alloc(ctl,
					MDSS_MDP_MIXER_TYPE_INTF, true);
			if (!ctl->mixer_right) {
				pr_err("unable to allocate right mixer\n");
				if (ctl->mixer_left)
					mdss_mdp_mixer_free(ctl->mixer_left);
				return -ENOMEM;
			}
		}
		ctl->mixer_right->width = width;
		ctl->mixer_right->height = height;
	} else if (ctl->mixer_right) {
		mdss_mdp_mixer_free(ctl->mixer_right);
		ctl->mixer_right = NULL;
	}

	if (ctl->mixer_right) {
		ctl->opmode |= MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
			       MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT;
	} else {
		ctl->opmode &= ~(MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
				  MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT);
	}

	return 0;
}

static int mdss_mdp_ctl_setup_wfd(struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_mixer *mixer;
	int mixer_type;

	/* if WB2 is supported, try to allocate it first */
	if (mdata->nmixers_intf >= MDSS_MDP_INTF_LAYERMIXER2)
		mixer_type = MDSS_MDP_MIXER_TYPE_INTF;
	else
		mixer_type = MDSS_MDP_MIXER_TYPE_WRITEBACK;

	mixer = mdss_mdp_mixer_alloc(ctl, mixer_type, false);
	if (!mixer && mixer_type == MDSS_MDP_MIXER_TYPE_INTF)
		mixer = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_WRITEBACK,
				false);

	if (!mixer) {
		pr_err("Unable to allocate writeback mixer\n");
		return -ENOMEM;
	}

	if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		ctl->opmode = MDSS_MDP_CTL_OP_WFD_MODE;
	} else {
		switch (mixer->num) {
		case MDSS_MDP_WB_LAYERMIXER0:
			ctl->opmode = MDSS_MDP_CTL_OP_WB0_MODE;
			break;
		case MDSS_MDP_WB_LAYERMIXER1:
			ctl->opmode = MDSS_MDP_CTL_OP_WB1_MODE;
			break;
		default:
			pr_err("Incorrect writeback config num=%d\n",
					mixer->num);
			mdss_mdp_mixer_free(mixer);
			return -EINVAL;
		}
		ctl->wb_type = MDSS_MDP_WB_CTL_TYPE_LINE;
	}
	ctl->mixer_left = mixer;

	return 0;
}

struct mdss_mdp_ctl *mdss_mdp_ctl_init(struct mdss_panel_data *pdata,
				       struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;
	int ret = 0;

	struct mdss_data_type *mdata = mfd_to_mdata(mfd);
	ctl = mdss_mdp_ctl_alloc(mdata, MDSS_MDP_CTL0);
	if (!ctl) {
		pr_err("unable to allocate ctl\n");
		return ERR_PTR(-ENOMEM);
	}
	ctl->mfd = mfd;
	ctl->panel_data = pdata;
	ctl->is_video_mode = false;

	switch (pdata->panel_info.type) {
	case EDP_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF0;
		ctl->intf_type = MDSS_INTF_EDP;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_VIDEO_PANEL:
		ctl->is_video_mode = true;
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = MDSS_MDP_INTF1;
		else
			ctl->intf_num = MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_CMD_PANEL:
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = MDSS_MDP_INTF1;
		else
			ctl->intf_num = MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_CMD_MODE;
		ctl->start_fnc = mdss_mdp_cmd_start;
		break;
	case DTV_PANEL:
		ctl->is_video_mode = true;
		ctl->intf_num = MDSS_MDP_INTF3;
		ctl->intf_type = MDSS_INTF_HDMI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		ret = mdss_mdp_limited_lut_igc_config(ctl);
		if (ret)
			pr_err("Unable to config IGC LUT data");
		break;
	case WRITEBACK_PANEL:
		ctl->intf_num = MDSS_MDP_NO_INTF;
		ctl->start_fnc = mdss_mdp_writeback_start;
		ret = mdss_mdp_ctl_setup_wfd(ctl);
		if (ret)
			goto ctl_init_fail;
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
		struct mdp_dither_cfg_data dither = {
			.block = mfd->index + MDP_LOGICAL_BLOCK_DISP_0,
			.flags = MDP_PP_OPS_DISABLE,
		};

		switch (pdata->panel_info.bpp) {
		case 18:
			ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB666;
			dither.flags = MDP_PP_OPS_ENABLE | MDP_PP_OPS_WRITE;
			dither.g_y_depth = 2;
			dither.r_cr_depth = 2;
			dither.b_cb_depth = 2;
			break;
		case 24:
		default:
			ctl->dst_format = MDSS_MDP_PANEL_FORMAT_RGB888;
			break;
		}
		mdss_mdp_dither_config(ctl, &dither, NULL);
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

	if (pdata->panel_info.xres > MAX_MIXER_WIDTH) {
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

	sctl->width = pdata->panel_info.xres;
	sctl->height = pdata->panel_info.yres;

	ctl->mixer_left = mdss_mdp_mixer_alloc(ctl, MDSS_MDP_MIXER_TYPE_INTF,
			false);
	if (!ctl->mixer_left) {
		pr_err("unable to allocate layer mixer\n");
		mdss_mdp_ctl_destroy(sctl);
		return -ENOMEM;
	}

	mixer = mdss_mdp_mixer_alloc(sctl, MDSS_MDP_MIXER_TYPE_INTF, false);
	if (!mixer) {
		pr_err("unable to allocate layer mixer\n");
		mdss_mdp_ctl_destroy(sctl);
		return -ENOMEM;
	}

	mixer->width = sctl->width;
	mixer->height = sctl->height;
	sctl->mixer_left = mixer;

	return mdss_mdp_set_split_ctl(ctl, sctl);
}

static void mdss_mdp_ctl_split_display_enable(int enable,
	struct mdss_mdp_ctl *main_ctl, struct mdss_mdp_ctl *slave_ctl)
{
	u32 upper = 0, lower = 0;

	pr_debug("split main ctl=%d intf=%d slave ctl=%d intf=%d\n",
			main_ctl->num, main_ctl->intf_num,
			slave_ctl->num, slave_ctl->intf_num);
	if (enable) {
		if (main_ctl->opmode & MDSS_MDP_CTL_OP_CMD_MODE) {
			upper |= BIT(1);
			lower |= BIT(1);

			/* interface controlling sw trigger */
			if (main_ctl->intf_num == MDSS_MDP_INTF2)
				upper |= BIT(4);
			else
				upper |= BIT(8);
		} else { /* video mode */
			if (main_ctl->intf_num == MDSS_MDP_INTF2)
				lower |= BIT(4);
			else
				lower |= BIT(8);
		}
	}
	MDSS_MDP_REG_WRITE(MDSS_MDP_REG_SPLIT_DISPLAY_UPPER_PIPE_CTRL, upper);
	MDSS_MDP_REG_WRITE(MDSS_MDP_REG_SPLIT_DISPLAY_LOWER_PIPE_CTRL, lower);
	MDSS_MDP_REG_WRITE(MDSS_MDP_REG_SPLIT_DISPLAY_EN, enable);
}


int mdss_mdp_ctl_destroy(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;
	int rc;

	rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CLOSE, NULL);
	WARN(rc, "unable to close panel for intf=%d\n", ctl->intf_num);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl) {
		pr_debug("destroying split display ctl=%d\n", sctl->num);
		if (sctl->mixer_left)
			mdss_mdp_mixer_free(sctl->mixer_left);
		mdss_mdp_ctl_free(sctl);
	} else if (ctl->mixer_right) {
		mdss_mdp_mixer_free(ctl->mixer_right);
		ctl->mixer_right = NULL;
	}

	if (ctl->mixer_left) {
		mdss_mdp_mixer_free(ctl->mixer_left);
		ctl->mixer_left = NULL;
	}
	mdss_mdp_ctl_free(ctl);

	return 0;
}

int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg)
{
	struct mdss_panel_data *pdata;
	int rc = 0;

	if (!ctl || !ctl->panel_data)
		return -ENODEV;

	pdata = ctl->panel_data;

	pr_debug("sending ctl=%d event=%d\n", ctl->num, event);

	do {
		if (pdata->event_handler)
			rc = pdata->event_handler(pdata, event, arg);
		pdata = pdata->next;
	} while (rc == 0 && pdata);

	return rc;
}

static int mdss_mdp_ctl_start_sub(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_mixer *mixer;
	u32 outsize, temp;
	int ret = 0;
	int i, nmixers;

	if (ctl->start_fnc)
		ret = ctl->start_fnc(ctl);
	else
		pr_warn("no start function for ctl=%d type=%d\n", ctl->num,
				ctl->panel_data->panel_info.type);

	if (ret) {
		pr_err("unable to start intf\n");
		return ret;
	}

	pr_debug("ctl_num=%d\n", ctl->num);

	nmixers = MDSS_MDP_INTF_MAX_LAYERMIXER + MDSS_MDP_WB_MAX_LAYERMIXER;
	for (i = 0; i < nmixers; i++)
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(i), 0);

	mixer = ctl->mixer_left;
	mdss_mdp_pp_resume(ctl, mixer->num);
	mixer->params_changed++;

	temp = MDSS_MDP_REG_READ(MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	MDSS_MDP_REG_WRITE(MDSS_MDP_REG_DISP_INTF_SEL, temp);

	outsize = (mixer->height << 16) | mixer->width;
	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OUT_SIZE, outsize);

	if (ctl->panel_data->panel_info.fbc.enabled) {
		ret = mdss_mdp_ctl_fbc_enable(1, ctl->mixer_left,
				&ctl->panel_data->panel_info);
	}

	return ret;
}

int mdss_mdp_ctl_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;
	int ret = 0;

	if (ctl->power_on) {
		pr_debug("%d: panel already on!\n", __LINE__);
		return 0;
	}

	ret = mdss_mdp_ctl_setup(ctl);
	if (ret)
		return ret;


	sctl = mdss_mdp_get_split_ctl(ctl);

	mutex_lock(&ctl->lock);

	ctl->power_on = true;
	ctl->bus_ab_quota = 0;
	ctl->bus_ib_quota = 0;
	ctl->clk_rate = 0;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_RESET, NULL);
	if (ret) {
		pr_err("panel power on failed ctl=%d\n", ctl->num);
		goto error;
	}

	ret = mdss_mdp_ctl_start_sub(ctl);
	if (ret == 0) {
		if (sctl) { /* split display is available */
			ret = mdss_mdp_ctl_start_sub(sctl);
			if (!ret)
				mdss_mdp_ctl_split_display_enable(1, ctl, sctl);
		} else if (ctl->mixer_right) {
			struct mdss_mdp_mixer *mixer = ctl->mixer_right;
			u32 out, off;

			mdss_mdp_pp_resume(ctl, mixer->num);
			mixer->params_changed++;
			out = (mixer->height << 16) | mixer->width;
			off = MDSS_MDP_REG_LM_OFFSET(mixer->num);
			MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OUT_SIZE, out);
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_PACK_3D, 0);
		}
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
error:
	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_ctl_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl;
	int ret = 0;
	u32 off;

	if (!ctl->power_on) {
		pr_debug("%s %d already off!\n", __func__, __LINE__);
		return 0;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);

	pr_debug("ctl_num=%d\n", ctl->num);

	mutex_lock(&ctl->lock);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	if (ctl->stop_fnc)
		ret = ctl->stop_fnc(ctl);
	else
		pr_warn("no stop func for ctl=%d\n", ctl->num);

	if (sctl && sctl->stop_fnc) {
		ret = sctl->stop_fnc(sctl);

		mdss_mdp_ctl_split_display_enable(0, ctl, sctl);
	}

	if (ret) {
		pr_warn("error powering off intf ctl=%d\n", ctl->num);
	} else {
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, 0);
		if (sctl)
			mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP, 0);

		if (ctl->mixer_left) {
			off = __mdss_mdp_ctl_get_mixer_off(ctl->mixer_left);
			mdss_mdp_ctl_write(ctl, off, 0);
		}

		if (ctl->mixer_right) {
			off = __mdss_mdp_ctl_get_mixer_off(ctl->mixer_right);
			mdss_mdp_ctl_write(ctl, off, 0);
		}

		ctl->power_on = false;
		ctl->play_cnt = 0;
		ctl->clk_rate = 0;
		mdss_mdp_ctl_perf_commit(ctl->mdata, MDSS_MDP_PERF_UPDATE_ALL);
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&ctl->lock);

	return ret;
}

static int mdss_mdp_mixer_setup(struct mdss_mdp_ctl *ctl,
				struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *pipe;
	u32 off, blend_op, blend_stage;
	u32 mixercfg = 0, blend_color_out = 0, bg_alpha_enable = 0;
	u32 fg_alpha = 0, bg_alpha = 0;
	int stage, secure = 0;
	int screen_state;

	screen_state = ctl->force_screen_state;

	if (!mixer)
		return -ENODEV;

	pr_debug("setup mixer=%d\n", mixer->num);

	if (screen_state == MDSS_SCREEN_FORCE_BLANK) {
		mixercfg = MDSS_MDP_LM_BORDER_COLOR;
		goto update_mixer;
	}

	pipe = mixer->stage_pipe[MDSS_MDP_STAGE_BASE];
	if (pipe == NULL) {
		mixercfg = MDSS_MDP_LM_BORDER_COLOR;
	} else {
		if (pipe->num == MDSS_MDP_SSPP_VIG3 ||
			pipe->num == MDSS_MDP_SSPP_RGB3) {
			/* Add 2 to account for Cursor & Border bits */
			mixercfg = 1 << ((3 * pipe->num)+2);
		} else {
			mixercfg = 1 << (3 * pipe->num);
		}
		if (pipe->src_fmt->alpha_enable)
			bg_alpha_enable = 1;
		secure = pipe->flags & MDP_SECURE_OVERLAY_SESSION;
	}

	for (stage = MDSS_MDP_STAGE_0; stage < MDSS_MDP_MAX_STAGE; stage++) {
		pipe = mixer->stage_pipe[stage];
		if (pipe == NULL)
			continue;

		if (stage != pipe->mixer_stage) {
			mixer->stage_pipe[stage] = NULL;
			continue;
		}

		blend_stage = stage - MDSS_MDP_STAGE_0;
		off = MDSS_MDP_REG_LM_BLEND_OFFSET(blend_stage);

		blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
			    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
		fg_alpha = pipe->alpha;
		bg_alpha = 0xFF - pipe->alpha;
		/* keep fg alpha */
		blend_color_out |= 1 << (blend_stage + 1);

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
			blend_color_out = 0;

		if (pipe->num == MDSS_MDP_SSPP_VIG3 ||
			pipe->num == MDSS_MDP_SSPP_RGB3) {
			/* Add 2 to account for Cursor & Border bits */
			mixercfg |= stage << ((3 * pipe->num)+2);
		} else {
			mixercfg |= stage << (3 * pipe->num);
		}

		pr_debug("stg=%d op=%x fg_alpha=%x bg_alpha=%x\n", stage,
					blend_op, fg_alpha, bg_alpha);
		mdp_mixer_write(mixer, off + MDSS_MDP_REG_LM_OP_MODE, blend_op);
		mdp_mixer_write(mixer, off + MDSS_MDP_REG_LM_BLEND_FG_ALPHA,
				   fg_alpha);
		mdp_mixer_write(mixer, off + MDSS_MDP_REG_LM_BLEND_BG_ALPHA,
				   bg_alpha);
	}

	if (mixer->cursor_enabled)
		mixercfg |= MDSS_MDP_LM_CURSOR_OUT;

update_mixer:
	pr_debug("mixer=%d mixer_cfg=%x\n", mixer->num, mixercfg);

	if (mixer->num == MDSS_MDP_INTF_LAYERMIXER3)
		ctl->flush_bits |= BIT(20);
	else
		ctl->flush_bits |= BIT(6) << mixer->num;

	mdp_mixer_write(mixer, MDSS_MDP_REG_LM_OP_MODE, blend_color_out);
	off = __mdss_mdp_ctl_get_mixer_off(mixer);
	mdss_mdp_ctl_write(ctl, off, mixercfg);

	return 0;
}

int mdss_mdp_mixer_addr_setup(struct mdss_data_type *mdata,
	 u32 *mixer_offsets, u32 *dspp_offsets, u32 *pingpong_offsets,
	 u32 type, u32 len)
{
	struct mdss_mdp_mixer *head;
	u32 i;
	int rc = 0;
	u32 size = len;

	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) && !mdata->has_wfd_blk)
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
		head[i].base = mdata->mdp_base + mixer_offsets[i];
		head[i].ref_cnt = 0;
		head[i].num = i;
		if (type == MDSS_MDP_MIXER_TYPE_INTF) {
			head[i].dspp_base = mdata->mdp_base + dspp_offsets[i];
			head[i].pingpong_base = mdata->mdp_base +
				pingpong_offsets[i];
		}
	}

	/*
	 * Duplicate the last writeback mixer for concurrent line and block mode
	 * operations
	*/
	if ((type == MDSS_MDP_MIXER_TYPE_WRITEBACK) && !mdata->has_wfd_blk)
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
	u32 *ctl_offsets, u32 *wb_offsets, u32 len)
{
	struct mdss_mdp_ctl *head;
	struct mutex *shared_lock = NULL;
	u32 i;
	u32 size = len;

	if (!mdata->has_wfd_blk) {
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
		head[i].base = (mdata->mdp_base) + ctl_offsets[i];
		head[i].wb_base = (mdata->mdp_base) + wb_offsets[i];
		head[i].ref_cnt = 0;
	}

	if (!mdata->has_wfd_blk) {
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

struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux)
{
	struct mdss_mdp_mixer *mixer = NULL;
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(ctl->mfd);
	if (!ctl)
		return NULL;

	switch (mux) {
	case MDSS_MDP_MIXER_MUX_DEFAULT:
	case MDSS_MDP_MIXER_MUX_LEFT:
		mixer = mdp5_data->mixer_swap ?
			ctl->mixer_right : ctl->mixer_left;
		break;
	case MDSS_MDP_MIXER_MUX_RIGHT:
		mixer = mdp5_data->mixer_swap ?
			ctl->mixer_left : ctl->mixer_right;
		break;
	}

	return mixer;
}

struct mdss_mdp_pipe *mdss_mdp_mixer_stage_pipe(struct mdss_mdp_ctl *ctl,
						int mux, int stage)
{
	struct mdss_mdp_pipe *pipe = NULL;
	struct mdss_mdp_mixer *mixer;
	if (!ctl)
		return NULL;

	if (mutex_lock_interruptible(&ctl->lock))
		return NULL;

	mixer = mdss_mdp_mixer_get(ctl, mux);
	if (mixer)
		pipe = mixer->stage_pipe[stage];
	mutex_unlock(&ctl->lock);

	return pipe;
}

int mdss_mdp_mixer_pipe_update(struct mdss_mdp_pipe *pipe, int params_changed)
{
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;
	int i;

	if (!pipe)
		return -EINVAL;
	mixer = pipe->mixer;
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

	if (mutex_lock_interruptible(&ctl->lock))
		return -EINTR;

	if (params_changed) {
		mixer->params_changed++;
		for (i = 0; i < MDSS_MDP_MAX_STAGE; i++) {
			if (i == pipe->mixer_stage)
				mixer->stage_pipe[i] = pipe;
			else if (mixer->stage_pipe[i] == pipe)
				mixer->stage_pipe[i] = NULL;
		}
	}

	if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA)
		ctl->flush_bits |= BIT(pipe->num) << 5;
	else if (pipe->num == MDSS_MDP_SSPP_VIG3 ||
			pipe->num == MDSS_MDP_SSPP_RGB3)
		ctl->flush_bits |= BIT(pipe->num) << 10;
	else /* RGB/VIG 0-2 pipes */
		ctl->flush_bits |= BIT(pipe->num);

	mutex_unlock(&ctl->lock);

	return 0;
}

int mdss_mdp_mixer_pipe_unstage(struct mdss_mdp_pipe *pipe)
{
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;

	if (!pipe)
		return -EINVAL;
	mixer = pipe->mixer;
	if (!mixer)
		return -EINVAL;
	ctl = mixer->ctl;
	if (!ctl)
		return -EINVAL;

	pr_debug("unstage pnum=%d stage=%d mixer=%d\n", pipe->num,
			pipe->mixer_stage, mixer->num);

	if (mutex_lock_interruptible(&ctl->lock))
		return -EINTR;

	if (pipe == mixer->stage_pipe[pipe->mixer_stage]) {
		mixer->params_changed++;
		mixer->stage_pipe[pipe->mixer_stage] = NULL;
	}
	mutex_unlock(&ctl->lock);

	return 0;
}

static int mdss_mdp_mixer_update(struct mdss_mdp_mixer *mixer)
{
	u32 off = 0;
	if (!mixer)
		return -EINVAL;

	mixer->params_changed = 0;

	/* skip mixer setup for rotator */
	if (!mixer->rotator_mode) {
		mdss_mdp_mixer_setup(mixer->ctl, mixer);
	} else {
		off = __mdss_mdp_ctl_get_mixer_off(mixer);
		mdss_mdp_ctl_write(mixer->ctl, off, 0);
	}

	return 0;
}

int mdss_mdp_ctl_update_fps(struct mdss_mdp_ctl *ctl, int fps)
{
	int ret = 0;

	if (ctl->config_fps_fnc)
		ret = ctl->config_fps_fnc(ctl, fps);

	return ret;
}

int mdss_mdp_display_wakeup_time(struct mdss_mdp_ctl *ctl,
				 ktime_t *wakeup_time)
{
	struct mdss_panel_info *pinfo;
	u32 clk_rate, clk_period;
	u32 current_line, total_line;
	u32 time_of_line, time_to_vsync;
	ktime_t current_time = ktime_get();

	if (!ctl->read_line_cnt_fnc)
		return -ENOSYS;

	pinfo = &ctl->panel_data->panel_info;
	if (!pinfo)
		return -ENODEV;

	clk_rate = mdss_mdp_get_pclk_rate(ctl);

	clk_rate /= 1000;	/* in kHz */
	if (!clk_rate)
		return -EINVAL;

	/*
	 * calculate clk_period as pico second to maintain good
	 * accuracy with high pclk rate and this number is in 17 bit
	 * range.
	 */
	clk_period = 1000000000 / clk_rate;
	if (!clk_period)
		return -EINVAL;

	time_of_line = (pinfo->lcdc.h_back_porch +
		 pinfo->lcdc.h_front_porch +
		 pinfo->lcdc.h_pulse_width +
		 pinfo->xres) * clk_period;

	time_of_line /= 1000;	/* in nano second */
	if (!time_of_line)
		return -EINVAL;

	current_line = ctl->read_line_cnt_fnc(ctl);

	total_line = pinfo->lcdc.v_back_porch +
		pinfo->lcdc.v_front_porch +
		pinfo->lcdc.v_pulse_width +
		pinfo->yres;

	if (current_line > total_line)
		return -EINVAL;

	time_to_vsync = time_of_line * (total_line - current_line);
	if (!time_to_vsync)
		return -EINVAL;

	*wakeup_time = ktime_add_ns(current_time, time_to_vsync);

	pr_debug("clk_rate=%dkHz clk_period=%d cur_line=%d tot_line=%d\n",
		clk_rate, clk_period, current_line, total_line);
	pr_debug("time_to_vsync=%d current_time=%d wakeup_time=%d\n",
		time_to_vsync, (int)ktime_to_ms(current_time),
		(int)ktime_to_ms(*wakeup_time));

	return 0;
}

int mdss_mdp_display_wait4comp(struct mdss_mdp_ctl *ctl)
{
	int ret;

	ret = mutex_lock_interruptible(&ctl->lock);
	if (ret)
		return ret;

	if (!ctl->power_on) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	if (ctl->wait_fnc)
		ret = ctl->wait_fnc(ctl, NULL);

	if (ctl->perf_changed) {
		mdss_mdp_ctl_perf_commit(ctl->mdata, ctl->perf_changed);
		ctl->perf_changed = 0;
	}

	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_display_wait4pingpong(struct mdss_mdp_ctl *ctl)
{
	int ret;

	ret = mutex_lock_interruptible(&ctl->lock);
	if (ret)
		return ret;

	if (!ctl->power_on) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	if (ctl->wait_pingpong)
		ret = ctl->wait_pingpong(ctl, NULL);

	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_ctl *sctl = NULL;
	int mixer1_changed, mixer2_changed;
	int ret = 0;
	int perf_update = MDSS_MDP_PERF_UPDATE_SKIP;

	if (!ctl) {
		pr_err("display function not set\n");
		return -ENODEV;
	}

	mutex_lock(&ctl->lock);
	pr_debug("commit ctl=%d play_cnt=%d\n", ctl->num, ctl->play_cnt);

	if (!ctl->power_on) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);

	mixer1_changed = (ctl->mixer_left && ctl->mixer_left->params_changed);
	mixer2_changed = (ctl->mixer_right && ctl->mixer_right->params_changed);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	if (mixer1_changed || mixer2_changed
			|| ctl->force_screen_state) {
		perf_update = mdss_mdp_ctl_perf_update(ctl);

		if (ctl->prepare_fnc)
			ret = ctl->prepare_fnc(ctl, arg);
		if (ret) {
			pr_err("error preparing display\n");
			goto done;
		}

		if (perf_update == MDSS_MDP_PERF_UPDATE_EARLY) {
			mdss_mdp_ctl_perf_commit(ctl->mdata, ctl->perf_changed);
			ctl->perf_changed = 0;
		}

		if (mixer1_changed)
			mdss_mdp_mixer_update(ctl->mixer_left);
		if (mixer2_changed)
			mdss_mdp_mixer_update(ctl->mixer_right);

		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, ctl->opmode);
		ctl->flush_bits |= BIT(17);	/* CTL */

		if (sctl) {
			mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_TOP,
					sctl->opmode);
			sctl->flush_bits |= BIT(17);
		}
	}

	/* postprocessing setup, including dspp */
	mdss_mdp_pp_setup_locked(ctl);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, ctl->flush_bits);
	if (sctl) {
		mdss_mdp_ctl_write(sctl, MDSS_MDP_REG_CTL_FLUSH,
			sctl->flush_bits);
	}
	wmb();
	ctl->flush_bits = 0;

	if (ctl->display_fnc)
		ret = ctl->display_fnc(ctl, arg); /* kickoff */
	if (ret)
		pr_warn("error displaying frame\n");

	ctl->play_cnt++;

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&ctl->lock);

	return ret;
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
		if ((ctl->power_on) && (ctl->mfd) &&
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

static inline int __mdss_mdp_ctl_get_mixer_off(struct mdss_mdp_mixer *mixer)
{
	if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
		if (mixer->num == MDSS_MDP_INTF_LAYERMIXER3)
			return MDSS_MDP_CTL_X_LAYER_5;
		else
			return MDSS_MDP_REG_CTL_LAYER(mixer->num);
	} else {
		return MDSS_MDP_REG_CTL_LAYER(mixer->num +
				MDSS_MDP_INTF_LAYERMIXER3);
	}
}
