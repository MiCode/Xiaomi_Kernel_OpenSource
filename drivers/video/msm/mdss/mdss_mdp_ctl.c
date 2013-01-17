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

#include "mdss_fb.h"
#include "mdss_mdp.h"

/* truncate at 1k */
#define MDSS_MDP_BUS_FACTOR_SHIFT 10
/* 1.5 bus fudge factor */
#define MDSS_MDP_BUS_FUDGE_FACTOR(val) (((val) / 2) * 3)
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
static struct mdss_mdp_ctl mdss_mdp_ctl_list[MDSS_MDP_MAX_CTL];
static struct mdss_mdp_mixer mdss_mdp_mixer_list[MDSS_MDP_MAX_LAYERMIXER];

static int mdss_mdp_ctl_perf_commit(u32 flags)
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
	for (cnum = 0; cnum < MDSS_MDP_MAX_CTL; cnum++) {
		ctl = &mdss_mdp_ctl_list[cnum];
		if (ctl->power_on) {
			bus_ab_quota += ctl->bus_ab_quota;
			bus_ib_quota += ctl->bus_ib_quota;

			if (ctl->clk_rate > clk_rate)
				clk_rate = ctl->clk_rate;
		}
	}
	if (flags & MDSS_MDP_PERF_UPDATE_BUS) {
		bus_ab_quota = bus_ab_quota << MDSS_MDP_BUS_FACTOR_SHIFT;
		bus_ib_quota = MDSS_MDP_BUS_FUDGE_FACTOR(bus_ib_quota);
		bus_ib_quota <<= MDSS_MDP_BUS_FACTOR_SHIFT;

		if ((bus_ib_quota == 0) && (clk_rate > 0)) {
			/* allocate min bw for panel cmds if mdp is active */
			bus_ib_quota = SZ_16M;
		}
		mdss_mdp_bus_scale_set_quota(bus_ab_quota, bus_ib_quota);
	}
	if (flags & MDSS_MDP_PERF_UPDATE_CLK) {
		clk_rate = MDSS_MDP_CLK_FUDGE_FACTOR(clk_rate);
		pr_debug("update clk rate = %lu\n", clk_rate);
		mdss_mdp_set_clk_rate(clk_rate);
	}
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

static void mdss_mdp_perf_mixer_update(struct mdss_mdp_mixer *mixer,
				       u32 *bus_ab_quota, u32 *bus_ib_quota,
				       u32 *clk_rate)
{
	struct mdss_mdp_pipe *pipe;
	const int fps = 60;
	u32 quota, rate;
	u32 v_total, v_active;
	int i;
	u32 max_clk_rate = 0, ab_total = 0, ib_total = 0;

	*bus_ab_quota = 0;
	*bus_ib_quota = 0;
	*clk_rate = 0;

	if (mixer->rotator_mode) {
		pipe = mixer->stage_pipe[0]; /* rotator pipe */
		v_total = pipe->flags & MDP_ROT_90 ? pipe->dst.w : pipe->dst.h;
		v_active = v_total;
	} else {
		int is_writeback = false;
		if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF) {
			struct mdss_panel_info *pinfo;
			pinfo = &mixer->ctl->panel_data->panel_info;
			v_total = (pinfo->yres + pinfo->lcdc.v_back_porch +
				   pinfo->lcdc.v_front_porch +
				   pinfo->lcdc.v_pulse_width);
			v_active = pinfo->yres;

			if (pinfo->type == WRITEBACK_PANEL)
				is_writeback = true;
		} else {
			v_total = mixer->height;
			v_active = v_total;

			is_writeback = true;
		}
		*clk_rate = mixer->width * v_total * fps;
		if (is_writeback) {
			/* perf for bus writeback */
			*bus_ab_quota = fps * mixer->width * mixer->height * 3;
			*bus_ab_quota >>= MDSS_MDP_BUS_FACTOR_SHIFT;
			*bus_ib_quota = *bus_ab_quota;
		}
	}

	for (i = 0; i < MDSS_MDP_MAX_STAGE; i++) {
		u32 ib_quota;
		pipe = mixer->stage_pipe[i];
		if (pipe == NULL)
			continue;
		if (pipe->is_fg) {
			ab_total = 0;
			ib_total = 0;
			max_clk_rate = 0;
		}

		quota = fps * pipe->src.w * pipe->src.h;
		if (pipe->src_fmt->chroma_sample == MDSS_MDP_CHROMA_420)
			quota = (quota * 3) / 2;
		else
			quota *= pipe->src_fmt->bpp;

		if (mixer->type == MDSS_MDP_MIXER_TYPE_INTF)
			quota = (quota / v_active) * v_total;
		else if (mixer->rotator_mode)
			quota *= 2; /* bus read + write */

		rate = pipe->dst.w;
		if (pipe->src.h > pipe->dst.h) {
			rate = (rate * pipe->src.h) / pipe->dst.h;
			ib_quota = (quota / pipe->dst.h) * pipe->src.h;
		} else {
			ib_quota = quota;
		}
		rate *= v_total * fps;
		if (mixer->rotator_mode)
			rate /= 4; /* block mode fetch at 4 pix/clk */

		pr_debug("mixer=%d pnum=%d clk_rate=%u bus ab=%u ib=%u\n",
			 mixer->num, pipe->num, rate, quota, ib_quota);

		ab_total += quota >> MDSS_MDP_BUS_FACTOR_SHIFT;
		ib_total += ib_quota >> MDSS_MDP_BUS_FACTOR_SHIFT;
		if (rate > max_clk_rate)
			max_clk_rate = rate;
	}

	*bus_ab_quota += ab_total;
	*bus_ib_quota += ib_total;
	if (max_clk_rate > *clk_rate)
		*clk_rate = max_clk_rate;

	pr_debug("final mixer=%d clk_rate=%u bus ab=%u ib=%u\n", mixer->num,
		 *clk_rate, *bus_ab_quota, *bus_ib_quota);
}

static int mdss_mdp_ctl_perf_update(struct mdss_mdp_ctl *ctl, u32 *flags)
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
	}

	*flags = 0;

	if (max_clk_rate != ctl->clk_rate) {
		if (max_clk_rate > ctl->clk_rate)
			ret = MDSS_MDP_PERF_UPDATE_EARLY;
		else
			ret = MDSS_MDP_PERF_UPDATE_LATE;
		ctl->clk_rate = max_clk_rate;
		*flags |= MDSS_MDP_PERF_UPDATE_CLK;
	}

	if ((total_ab_quota != ctl->bus_ab_quota) ||
			(total_ib_quota != ctl->bus_ib_quota)) {
		if (ret == MDSS_MDP_PERF_UPDATE_SKIP) {
			if (total_ib_quota > ctl->bus_ib_quota)
				ret = MDSS_MDP_PERF_UPDATE_EARLY;
			else
				ret = MDSS_MDP_PERF_UPDATE_LATE;
		}
		ctl->bus_ab_quota = total_ab_quota;
		ctl->bus_ib_quota = total_ib_quota;
		*flags |= MDSS_MDP_PERF_UPDATE_BUS;
	}

	return ret;
}

static struct mdss_mdp_ctl *mdss_mdp_ctl_alloc(void)
{
	struct mdss_mdp_ctl *ctl = NULL;
	int cnum;

	mutex_lock(&mdss_mdp_ctl_lock);
	for (cnum = 0; cnum < MDSS_MDP_MAX_CTL; cnum++) {
		if (mdss_mdp_ctl_list[cnum].ref_cnt == 0) {
			ctl = &mdss_mdp_ctl_list[cnum];
			ctl->num = cnum;
			ctl->ref_cnt++;
			mutex_init(&ctl->lock);

			pr_debug("alloc ctl_num=%d\n", ctl->num);
			break;
		}
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

	mutex_lock(&mdss_mdp_ctl_lock);
	if (--ctl->ref_cnt == 0)
		memset(ctl, 0, sizeof(*ctl));
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

static struct mdss_mdp_mixer *mdss_mdp_mixer_alloc(u32 type)
{
	struct mdss_mdp_mixer *mixer = NULL;
	int mnum;

	mutex_lock(&mdss_mdp_ctl_lock);
	for (mnum = 0; mnum < MDSS_MDP_MAX_LAYERMIXER; mnum++) {
		if (type == mdss_res->mixer_type_map[mnum] &&
		    mdss_mdp_mixer_list[mnum].ref_cnt == 0) {
			mixer = &mdss_mdp_mixer_list[mnum];
			mixer->num = mnum;
			mixer->ref_cnt++;
			mixer->params_changed++;
			mixer->type = type;

			pr_debug("mixer_num=%d\n", mixer->num);
			break;
		}
	}
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
	if (--mixer->ref_cnt == 0)
		memset(mixer, 0, sizeof(*mixer));
	mutex_unlock(&mdss_mdp_ctl_lock);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_wb_mixer_alloc(int rotator)
{
	struct mdss_mdp_ctl *ctl = NULL;
	struct mdss_mdp_mixer *mixer = NULL;

	ctl = mdss_mdp_ctl_alloc();

	if (!ctl)
		return NULL;

	mixer = mdss_mdp_mixer_alloc(MDSS_MDP_MIXER_TYPE_WRITEBACK);
	if (!mixer)
		goto error;

	mixer->rotator_mode = rotator;

	switch (mixer->num) {
	case MDSS_MDP_LAYERMIXER3:
		ctl->opmode = (rotator ? MDSS_MDP_CTL_OP_ROT0_MODE :
			       MDSS_MDP_CTL_OP_WB0_MODE);
		break;
	case MDSS_MDP_LAYERMIXER4:
		ctl->opmode = (rotator ? MDSS_MDP_CTL_OP_ROT1_MODE :
			       MDSS_MDP_CTL_OP_WB1_MODE);
		break;
	default:
		pr_err("invalid layer mixer=%d\n", mixer->num);
		goto error;
	}

	ctl->mixer_left = mixer;
	mixer->ctl = ctl;

	ctl->start_fnc = mdss_mdp_writeback_start;
	ctl->power_on = true;

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

	pr_debug("destroy ctl=%d mixer=%d\n", ctl->num, mixer->num);

	if (ctl->stop_fnc)
		ctl->stop_fnc(ctl);

	mdss_mdp_mixer_free(mixer);
	mdss_mdp_ctl_free(ctl);

	mdss_mdp_ctl_perf_commit(MDSS_MDP_PERF_UPDATE_ALL);

	return 0;
}

static int mdss_mdp_ctl_init(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;
	struct mdss_panel_data *pdata;
	u32 width, height;
	int ret = 0;

	if (!mfd)
		return -ENODEV;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	if (!pdata) {
		pr_err("no panel connected for fb%d\n", mfd->index);
		return -ENODEV;
	}

	width = pdata->panel_info.xres;
	height = pdata->panel_info.yres;

	if (width > (2 * MAX_MIXER_WIDTH)) {
		pr_err("unsupported resolution\n");
		return -EINVAL;
	}

	if (!mfd->ctl) {
		ctl = mdss_mdp_ctl_alloc();
		if (!ctl) {
			pr_err("unable to allocate ctl\n");
			return -ENOMEM;
		}
		ctl->mfd = mfd;
		mfd->ctl = ctl;
		ctl->panel_data = pdata;
	} else {
		ctl = mfd->ctl;
	}

	ctl->width = width;
	ctl->height = height;

	if (!ctl->mixer_left) {
		ctl->mixer_left =
			mdss_mdp_mixer_alloc(MDSS_MDP_MIXER_TYPE_INTF);
		if (!ctl->mixer_left) {
			pr_err("unable to allocate layer mixer\n");
			ret = -ENOMEM;
			goto ctl_init_fail;
		}
	}

	if (width > MAX_MIXER_WIDTH)
		width /= 2;

	ctl->mixer_left->width = width;
	ctl->mixer_left->height = height;
	ctl->mixer_left->ctl = ctl;

	if (width < ctl->width) {
		if (ctl->mixer_right == NULL) {
			ctl->mixer_right =
				mdss_mdp_mixer_alloc(MDSS_MDP_MIXER_TYPE_INTF);
			if (!ctl->mixer_right) {
				pr_err("unable to allocate right mixer\n");
				ret = -ENOMEM;
				goto ctl_init_fail;
			}
		}
		ctl->mixer_right->width = width;
		ctl->mixer_right->height = height;
		ctl->mixer_right->ctl = ctl;
	} else if (ctl->mixer_right) {
		mdss_mdp_mixer_free(ctl->mixer_right);
	}

	switch (pdata->panel_info.type) {
	case EDP_PANEL:
		ctl->intf_num = MDSS_MDP_INTF0;
		ctl->intf_type = MDSS_INTF_EDP;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		break;
	case MIPI_VIDEO_PANEL:
		if (pdata->panel_info.pdest == DISPLAY_1)
			ctl->intf_num = MDSS_MDP_INTF1;
		else
			ctl->intf_num = MDSS_MDP_INTF2;
		ctl->intf_type = MDSS_INTF_DSI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		break;
	case DTV_PANEL:
		ctl->intf_num = MDSS_MDP_INTF3;
		ctl->intf_type = MDSS_INTF_HDMI;
		ctl->opmode = MDSS_MDP_CTL_OP_VIDEO_MODE;
		ctl->start_fnc = mdss_mdp_video_start;
		break;
	case WRITEBACK_PANEL:
		ctl->intf_num = MDSS_MDP_NO_INTF;
		ctl->opmode = MDSS_MDP_CTL_OP_WFD_MODE;
		ctl->start_fnc = mdss_mdp_writeback_start;
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
		mdss_mdp_dither_config(&dither, NULL);
	}

	if (ctl->mixer_right) {
		ctl->opmode |= MDSS_MDP_CTL_OP_PACK_3D_ENABLE |
			       MDSS_MDP_CTL_OP_PACK_3D_H_ROW_INT;
	}

ctl_init_fail:
	if (IS_ERR_VALUE(ret)) {
		if (ctl->mixer_left)
			mdss_mdp_mixer_free(ctl->mixer_left);
		if (ctl->mixer_right)
			mdss_mdp_mixer_free(ctl->mixer_right);
		mdss_mdp_ctl_free(ctl);
		mfd->ctl = NULL;
	}

	return ret;
}

static int mdss_mdp_ctl_destroy(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;
	if (!mfd || !mfd->ctl)
		return -ENODEV;

	ctl = mfd->ctl;
	mfd->ctl = NULL;

	if (ctl->mixer_left)
		mdss_mdp_mixer_free(ctl->mixer_left);
	if (ctl->mixer_right)
		mdss_mdp_mixer_free(ctl->mixer_right);
	mdss_mdp_ctl_free(ctl);

	return 0;
}

int mdss_mdp_ctl_intf_event(struct mdss_mdp_ctl *ctl, int event, void *arg)
{
	struct mdss_panel_data *pdata;
	if (!ctl || !ctl->panel_data)
		return -ENODEV;

	pdata = ctl->panel_data;

	pr_debug("sending ctl=%d event=%d\n", ctl->num, event);

	if (pdata->event_handler)
		return pdata->event_handler(pdata, event, arg);

	return 0;
}

int mdss_mdp_ctl_on(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;
	struct mdss_mdp_mixer *mixer;
	u32 outsize, temp, off;
	int ret = 0;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (mdss_mdp_ctl_init(mfd)) {
		pr_err("unable to initialize ctl\n");
		return -ENODEV;
	}

	ctl = mfd->ctl;

	if (ctl->power_on) {
		WARN(1, "already on!\n");
		return 0;
	}

	mutex_lock(&ctl->lock);

	ctl->power_on = true;
	ctl->bus_ab_quota = 0;
	ctl->bus_ib_quota = 0;
	ctl->clk_rate = 0;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_RESET, NULL);
	if (ret) {
		pr_err("panel power on failed ctl=%d\n", ctl->num);
		goto start_fail;
	}

	if (ctl->start_fnc)
		ret = ctl->start_fnc(ctl);
	else
		pr_warn("no start function for ctl=%d type=%d\n", ctl->num,
				ctl->panel_data->panel_info.type);

	if (ret) {
		pr_err("unable to start intf\n");
		goto start_fail;
	}

	pr_debug("ctl_num=%d\n", ctl->num);

	mixer = ctl->mixer_left;
	mdss_mdp_pp_resume(mixer->num);
	mixer->params_changed++;

	temp = MDSS_MDP_REG_READ(MDSS_MDP_REG_DISP_INTF_SEL);
	temp |= (ctl->intf_type << ((ctl->intf_num - MDSS_MDP_INTF0) * 8));
	MDSS_MDP_REG_WRITE(MDSS_MDP_REG_DISP_INTF_SEL, temp);

	if (ctl->intf_num != MDSS_MDP_NO_INTF) {
		off = MDSS_MDP_REG_INTF_OFFSET(ctl->intf_num);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_PANEL_FORMAT,
				   ctl->dst_format);
	}

	outsize = (mixer->height << 16) | mixer->width;
	off = MDSS_MDP_REG_LM_OFFSET(mixer->num);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OUT_SIZE, outsize);

	if (ctl->mixer_right) {
		mixer = ctl->mixer_right;
		mdss_mdp_pp_resume(mixer->num);
		mixer->params_changed++;
		outsize = (mixer->height << 16) | mixer->width;
		off = MDSS_MDP_REG_LM_OFFSET(mixer->num);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OUT_SIZE, outsize);
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_PACK_3D, 0);
	}

start_fail:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
	mutex_unlock(&ctl->lock);
	if (ret)
		mdss_mdp_ctl_destroy(mfd);

	return ret;
}

int mdss_mdp_ctl_off(struct msm_fb_data_type *mfd)
{
	struct mdss_mdp_ctl *ctl;
	int ret = 0;

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (!mfd->ctl) {
		pr_err("ctl not initialized\n");
		return -ENODEV;
	}

	ctl = mfd->ctl;

	if (!ctl->power_on) {
		WARN(1, "already off!\n");
		return 0;
	}

	pr_debug("ctl_num=%d\n", mfd->ctl->num);

	mutex_lock(&ctl->lock);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	if (ctl->stop_fnc)
		ret = ctl->stop_fnc(ctl);
	else
		pr_warn("no stop func for ctl=%d\n", ctl->num);

	if (ret) {
		pr_warn("error powering off intf ctl=%d\n", ctl->num);
	} else {
		ctl->power_on = false;
		ctl->play_cnt = 0;
		ctl->clk_rate = 0;
		if (ctl->mixer_left) {
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(
					ctl->mixer_left->num), 0);
		}
		if (ctl->mixer_right) {
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(
					ctl->mixer_right->num), 0);
		}
		mdss_mdp_ctl_perf_commit(MDSS_MDP_PERF_UPDATE_ALL);
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&ctl->lock);

	if (!ret && !mfd->ref_cnt) {
		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CLOSE, NULL);
		WARN(ret, "unable to close intf %d\n", ctl->intf_num);
		mdss_mdp_ctl_destroy(mfd);
	}

	return ret;
}

static int mdss_mdp_mixer_setup(struct mdss_mdp_ctl *ctl,
				struct mdss_mdp_mixer *mixer)
{
	struct mdss_mdp_pipe *pipe;
	u32 off, blend_op, blend_stage;
	u32 mixercfg = 0, blend_color_out = 0, bgalpha = 0;
	int stage;

	if (!mixer)
		return -ENODEV;

	pr_debug("setup mixer=%d\n", mixer->num);

	pipe = mixer->stage_pipe[MDSS_MDP_STAGE_BASE];
	if (pipe == NULL) {
		mixercfg = MDSS_MDP_LM_BORDER_COLOR;
	} else {
		mixercfg = 1 << (3 * pipe->num);
		if (pipe->src_fmt->alpha_enable)
			bgalpha = 1;
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
		off = MDSS_MDP_REG_LM_OFFSET(mixer->num) +
		      MDSS_MDP_REG_LM_BLEND_OFFSET(blend_stage);

		if (pipe->is_fg) {
			bgalpha = 0;
			mixercfg = MDSS_MDP_LM_BORDER_COLOR;

			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
			/* keep fg alpha */
			blend_color_out |= 1 << (blend_stage + 1);

			pr_debug("pnum=%d stg=%d alpha=IS_FG\n", pipe->num,
					stage);
		} else if (pipe->src_fmt->alpha_enable) {
			bgalpha = 0;
			blend_op = (MDSS_MDP_BLEND_BG_ALPHA_FG_PIXEL |
				    MDSS_MDP_BLEND_BG_INV_ALPHA);
			/* keep fg alpha */
			blend_color_out |= 1 << (blend_stage + 1);

			pr_debug("pnum=%d stg=%d alpha=FG PIXEL\n", pipe->num,
					stage);
		} else if (bgalpha) {
			blend_op = (MDSS_MDP_BLEND_BG_ALPHA_BG_PIXEL |
				    MDSS_MDP_BLEND_FG_ALPHA_BG_PIXEL |
				    MDSS_MDP_BLEND_FG_INV_ALPHA);
			/* keep bg alpha */
			pr_debug("pnum=%d stg=%d alpha=BG_PIXEL\n", pipe->num,
					stage);
		} else {
			blend_op = (MDSS_MDP_BLEND_FG_ALPHA_FG_CONST |
				    MDSS_MDP_BLEND_BG_ALPHA_BG_CONST);
			pr_debug("pnum=%d stg=%d alpha=CONST\n", pipe->num,
					stage);
		}

		if (mixercfg == MDSS_MDP_LM_BORDER_COLOR &&
				pipe->src_fmt->alpha_enable &&
				pipe->dst.w == mixer->width &&
				pipe->dst.h == mixer->height) {
			pr_debug("setting pipe=%d as BG_PIPE\n", pipe->num);
			bgalpha = 1;
		}

		mixercfg |= stage << (3 * pipe->num);

		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OP_MODE, blend_op);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_BLEND_FG_ALPHA,
				   pipe->alpha);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_BLEND_BG_ALPHA,
				   0xFF - pipe->alpha);
	}

	if (mixer->cursor_enabled)
		mixercfg |= MDSS_MDP_LM_CURSOR_OUT;

	pr_debug("mixer=%d mixer_cfg=%x\n", mixer->num, mixercfg);

	ctl->flush_bits |= BIT(6) << mixer->num;	/* LAYER_MIXER */

	off = MDSS_MDP_REG_LM_OFFSET(mixer->num);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_LM_OP_MODE, blend_color_out);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_LAYER(mixer->num), mixercfg);

	return 0;
}

struct mdss_mdp_mixer *mdss_mdp_mixer_get(struct mdss_mdp_ctl *ctl, int mux)
{
	struct mdss_mdp_mixer *mixer = NULL;
	if (!ctl)
		return NULL;

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
		mixer->stage_pipe[pipe->mixer_stage] = pipe;
	}

	if (pipe->type == MDSS_MDP_PIPE_TYPE_DMA)
		ctl->flush_bits |= BIT(pipe->num) << 5;
	else /* RGB/VIG pipe */
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

	mixer->params_changed++;
	mixer->stage_pipe[pipe->mixer_stage] = NULL;

	mutex_unlock(&ctl->lock);

	return 0;
}

static int mdss_mdp_mixer_update(struct mdss_mdp_mixer *mixer)
{
	mixer->params_changed = 0;

	/* skip mixer setup for rotator */
	if (!mixer->rotator_mode)
		mdss_mdp_mixer_setup(mixer->ctl, mixer);

	return 0;
}

int mdss_mdp_display_commit(struct mdss_mdp_ctl *ctl, void *arg)
{
	int mixer1_changed, mixer2_changed;
	int ret = 0;
	int perf_update = MDSS_MDP_PERF_UPDATE_SKIP;
	u32 update_flags = 0;

	if (!ctl) {
		pr_err("display function not set\n");
		return -ENODEV;
	}

	pr_debug("commit ctl=%d play_cnt=%d\n", ctl->num, ctl->play_cnt);

	ret = mutex_lock_interruptible(&ctl->lock);
	if (ret)
		return ret;

	if (!ctl->power_on) {
		mutex_unlock(&ctl->lock);
		return 0;
	}

	mixer1_changed = (ctl->mixer_left && ctl->mixer_left->params_changed);
	mixer2_changed = (ctl->mixer_right && ctl->mixer_right->params_changed);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	if (mixer1_changed || mixer2_changed) {
		perf_update = mdss_mdp_ctl_perf_update(ctl, &update_flags);

		if (ctl->prepare_fnc)
			ret = ctl->prepare_fnc(ctl, arg);
		if (ret) {
			pr_err("error preparing display\n");
			goto done;
		}

		if (perf_update == MDSS_MDP_PERF_UPDATE_EARLY)
			mdss_mdp_ctl_perf_commit(update_flags);

		if (mixer1_changed)
			mdss_mdp_mixer_update(ctl->mixer_left);
		if (mixer2_changed)
			mdss_mdp_mixer_update(ctl->mixer_right);

		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_TOP, ctl->opmode);
		ctl->flush_bits |= BIT(17);	/* CTL */
	}

	/* postprocessing setup, including dspp */
	mdss_mdp_pp_setup(ctl);
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, ctl->flush_bits);
	wmb();
	ctl->flush_bits = 0;

	if (ctl->display_fnc)
		ret = ctl->display_fnc(ctl, arg); /* kickoff */
	if (ret)
		pr_warn("error displaying frame\n");

	ctl->play_cnt++;

	if (perf_update == MDSS_MDP_PERF_UPDATE_LATE)
		mdss_mdp_ctl_perf_commit(update_flags);

done:
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	mutex_unlock(&ctl->lock);

	return ret;
}

int mdss_mdp_get_ctl_mixers(u32 fb_num, u32 *mixer_id)
{
	int i;
	struct mdss_mdp_ctl *ctl;
	u32 mixer_cnt = 0;
	mutex_lock(&mdss_mdp_ctl_lock);
	for (i = 0; i < MDSS_MDP_MAX_CTL; i++) {
		ctl = &mdss_mdp_ctl_list[i];
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

