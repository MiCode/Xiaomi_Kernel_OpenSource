/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/workqueue.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"

/* intf timing settings */
struct intf_timing_params {
	u32 width;
	u32 height;
	u32 xres;
	u32 yres;

	u32 h_back_porch;
	u32 h_front_porch;
	u32 v_back_porch;
	u32 v_front_porch;
	u32 hsync_pulse_width;
	u32 vsync_pulse_width;

	u32 border_clr;
	u32 underflow_clr;
	u32 hsync_skew;
};

#define MAX_SESSIONS 3
struct mdss_mdp_video_ctx {
	u32 ctl_num;
	u32 pp_num;
	u8 ref_cnt;

	u8 timegen_en;
	struct completion pp_comp;
	struct completion vsync_comp;

	struct mutex vsync_lock;
	struct work_struct vsync_work;
	mdp_vsync_handler_t vsync_handler;
	void *vsync_ptr;
	ktime_t vsync_time;
};

struct mdss_mdp_video_ctx mdss_mdp_video_ctx_list[MAX_SESSIONS];

static int mdss_mdp_video_timegen_setup(struct mdss_mdp_ctl *ctl,
					struct intf_timing_params *p)
{
	u32 hsync_period, vsync_period;
	u32 hsync_start_x, hsync_end_x, display_v_start, display_v_end;
	u32 active_h_start, active_h_end, active_v_start, active_v_end;
	u32 den_polarity, hsync_polarity, vsync_polarity;
	u32 display_hctl, active_hctl, hsync_ctl, polarity_ctl;
	int off;

	off = MDSS_MDP_REG_INTF_OFFSET(ctl->intf_num);

	hsync_period = p->hsync_pulse_width + p->h_back_porch +
			p->width + p->h_front_porch;
	vsync_period = p->vsync_pulse_width + p->v_back_porch +
			p->height + p->v_front_porch;

	display_v_start = ((p->vsync_pulse_width + p->v_back_porch) *
			hsync_period) + p->hsync_skew;
	display_v_end = ((vsync_period - p->v_front_porch) * hsync_period) +
			p->hsync_skew - 1;

	if (ctl->intf_type == MDSS_INTF_EDP) {
		display_v_start += p->hsync_pulse_width + p->h_back_porch;
		display_v_end -= p->h_front_porch;
	}

	hsync_start_x = p->h_back_porch + p->hsync_pulse_width;
	hsync_end_x = hsync_period - p->h_front_porch - 1;

	if (p->width != p->xres) {
		active_h_start = hsync_start_x;
		active_h_end = active_h_start + p->xres - 1;
	} else {
		active_h_start = 0;
		active_h_end = 0;
	}

	if (p->height != p->yres) {
		active_v_start = display_v_start;
		active_v_end = active_v_start + (p->yres * hsync_period) - 1;
	} else {
		active_v_start = 0;
		active_v_end = 0;
	}


	if (active_h_end) {
		active_hctl = (active_h_end << 16) | active_h_start;
		active_hctl |= BIT(31);	/* ACTIVE_H_ENABLE */
	} else {
		active_hctl = 0;
	}

	if (active_v_end)
		active_v_start |= BIT(31); /* ACTIVE_V_ENABLE */

	hsync_ctl = (hsync_period << 16) | p->hsync_pulse_width;
	display_hctl = (hsync_end_x << 16) | hsync_start_x;

	den_polarity = 0;
	if (MDSS_INTF_HDMI ==  ctl->intf_type) {
		hsync_polarity = p->yres >= 720 ? 0 : 1;
		vsync_polarity = p->yres >= 720 ? 0 : 1;
	} else {
		hsync_polarity = 0;
		vsync_polarity = 0;
	}
	polarity_ctl = (den_polarity << 2)   | /*  DEN Polarity  */
		       (vsync_polarity << 1) | /* VSYNC Polarity */
		       (hsync_polarity << 0);  /* HSYNC Polarity */

	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_HSYNC_CTL, hsync_ctl);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
			   vsync_period * hsync_period);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_VSYNC_PULSE_WIDTH_F0,
			   p->vsync_pulse_width * hsync_period);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_DISPLAY_HCTL,
			   display_hctl);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_DISPLAY_V_START_F0,
			   display_v_start);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_DISPLAY_V_END_F0,
			   display_v_end);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_ACTIVE_HCTL, active_hctl);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_ACTIVE_V_START_F0,
			   active_v_start);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_ACTIVE_V_END_F0,
			   active_v_end);

	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_BORDER_COLOR,
			   p->border_clr);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_UNDERFLOW_COLOR,
			   p->underflow_clr);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_HSYNC_SKEW,
			   p->hsync_skew);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_POLARITY_CTL,
			   polarity_ctl);
	MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_PANEL_FORMAT,
			   MDSS_MDP_PANEL_FORMAT_RGB888);

	return 0;
}

static void send_vsync_work(struct work_struct *work)
{
	struct mdss_mdp_video_ctx *ctx;

	ctx = container_of(work, typeof(*ctx), vsync_work);
	mutex_lock(&ctx->vsync_lock);
	if (ctx->vsync_handler)
		ctx->vsync_handler(ctx->vsync_ptr, ctx->vsync_time);
	mutex_unlock(&ctx->vsync_lock);
}

static int mdss_mdp_video_set_vsync_handler(struct mdss_mdp_ctl *ctl,
		mdp_vsync_handler_t vsync_handler)
{
	struct mdss_mdp_video_ctx *ctx;

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}
	if (mutex_lock_interruptible(&ctx->vsync_lock))
		return -EINTR;

	if (!ctx->timegen_en) {
		ctx->vsync_time = ktime_get();
		schedule_work(&ctx->vsync_work);
	}

	if (!ctx->vsync_handler && vsync_handler)
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num);
	else if (ctx->vsync_handler && !vsync_handler)
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num);

	ctx->vsync_handler = vsync_handler;
	ctx->vsync_ptr = ctl;
	mutex_unlock(&ctx->vsync_lock);

	return 0;
}

static int mdss_mdp_video_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_video_ctx *ctx;
	int off;

	pr_debug("stop ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	if (ctx->timegen_en) {
		off = MDSS_MDP_REG_INTF_OFFSET(ctl->intf_num);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 0);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
		ctx->timegen_en = false;
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num,
				   NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   NULL, NULL);

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

static void mdss_mdp_video_pp_intr_done(void *arg)
{
	struct mdss_mdp_video_ctx *ctx;

	ctx = (struct mdss_mdp_video_ctx *) arg;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	pr_debug("intr mixer=%d\n", ctx->pp_num);

	complete(&ctx->pp_comp);
}

static void mdss_mdp_video_vsync_intr_done(void *arg)
{
	struct mdss_mdp_video_ctx *ctx;

	ctx = (struct mdss_mdp_video_ctx *) arg;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}
	ctx->vsync_time = ktime_get();

	pr_debug("intr ctl=%d\n", ctx->ctl_num);

	complete(&ctx->vsync_comp);
	if (ctx->vsync_handler)
		schedule_work(&ctx->vsync_work);
}

static int mdss_mdp_video_prepare(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_video_ctx *ctx;

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (ctx->timegen_en) {
		pr_debug("waiting for ping pong %d done\n", ctx->pp_num);
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);
		wait_for_completion_interruptible(&ctx->pp_comp);
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);
	}

	return 0;
}

static int mdss_mdp_video_display(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_video_ctx *ctx;

	pr_debug("kickoff ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}
	INIT_COMPLETION(ctx->vsync_comp);

	if (mutex_lock_interruptible(&ctx->vsync_lock))
		return -EINTR;
	if (!ctx->vsync_handler)
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num);

	if (!ctx->timegen_en) {
		int off = MDSS_MDP_REG_INTF_OFFSET(ctl->intf_num);

		pr_debug("enabling timing gen for intf=%d\n", ctl->intf_num);

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		MDSS_MDP_REG_WRITE(off + MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 1);
		ctx->timegen_en = true;
		wmb();
	}

	wait_for_completion_interruptible(&ctx->vsync_comp);
	if (!ctx->vsync_handler)
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num);
	mutex_unlock(&ctx->vsync_lock);

	return 0;
}

int mdss_mdp_video_start(struct mdss_mdp_ctl *ctl)
{
	struct msm_fb_data_type *mfd;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_mixer *mixer;
	struct intf_timing_params itp = {0};
	struct fb_info *fbi;
	int i;

	mfd = ctl->mfd;
	fbi = mfd->fbi;
	pinfo = &mfd->panel_info;
	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);

	if (!mixer) {
		pr_err("mixer not setup correctly\n");
		return -ENODEV;
	}

	pr_debug("start ctl=%u\n", ctl->num);

	for (i = 0; i < MAX_SESSIONS; i++) {
		ctx = &mdss_mdp_video_ctx_list[i];
		if (ctx->ref_cnt == 0) {
			ctx->ref_cnt++;
			break;
		}
	}
	if (i == MAX_SESSIONS) {
		pr_err("too many sessions\n");
		return -ENOMEM;
	}
	ctl->priv_data = ctx;
	ctx->ctl_num = ctl->num;
	ctx->pp_num = mixer->num;
	init_completion(&ctx->pp_comp);
	init_completion(&ctx->vsync_comp);

	INIT_WORK(&ctx->vsync_work, send_vsync_work);
	mutex_init(&ctx->vsync_lock);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num,
				   mdss_mdp_video_vsync_intr_done, ctx);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   mdss_mdp_video_pp_intr_done, ctx);

	itp.width = pinfo->xres + pinfo->lcdc.xres_pad;
	itp.height = pinfo->yres + pinfo->lcdc.yres_pad;
	itp.border_clr = pinfo->lcdc.border_clr;
	itp.underflow_clr = pinfo->lcdc.underflow_clr;
	itp.hsync_skew = pinfo->lcdc.hsync_skew;

	itp.xres =  pinfo->xres;
	itp.yres = pinfo->yres;
	itp.h_back_porch =  pinfo->lcdc.h_back_porch;
	itp.h_front_porch =  pinfo->lcdc.h_front_porch;
	itp.v_back_porch =  pinfo->lcdc.v_back_porch;
	itp.v_front_porch = pinfo->lcdc.v_front_porch;
	itp.hsync_pulse_width = pinfo->lcdc.h_pulse_width;
	itp.vsync_pulse_width = pinfo->lcdc.v_pulse_width;

	if (mdss_mdp_video_timegen_setup(ctl, &itp)) {
		pr_err("unable to get timing parameters\n");
		return -EINVAL;
	}

	ctl->stop_fnc = mdss_mdp_video_stop;
	ctl->prepare_fnc = mdss_mdp_video_prepare;
	ctl->display_fnc = mdss_mdp_video_display;
	ctl->set_vsync_handler = mdss_mdp_video_set_vsync_handler;

	return 0;
}
