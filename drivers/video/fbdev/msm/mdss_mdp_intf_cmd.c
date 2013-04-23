/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include "mdss_panel.h"
#include "mdss_mdp.h"
#include "mdss_dsi.h"

#define VSYNC_EXPIRE_TICK 4

#define START_THRESHOLD 4
#define CONTINUE_THRESHOLD 4

#define MAX_SESSIONS 2

/* wait for at most 2 vsync for lowest refresh rate (24hz) */
#define KOFF_TIMEOUT msecs_to_jiffies(84)

struct mdss_mdp_cmd_ctx {
	struct mdss_mdp_ctl *ctl;
	u32 pp_num;
	u8 ref_cnt;
	struct completion vsync_comp;
	struct completion pp_comp;
	struct completion stop_comp;
	mdp_vsync_handler_t send_vsync;
	int panel_on;
	int koff_cnt;
	int clk_enabled;
	int clk_control;
	int vsync_enabled;
	int expire;
	struct mutex clk_mtx;
	spinlock_t clk_lock;
	struct work_struct clk_work;

	/* te config */
	u8 tear_check;
	u16 height;	/* panel height */
	u16 vporch;	/* vertical porches */
	u16 start_threshold;
	u32 vclk_line;	/* vsync clock per line */
};

struct mdss_mdp_cmd_ctx mdss_mdp_cmd_ctx_list[MAX_SESSIONS];

static inline u32 mdss_mdp_cmd_line_count(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_mixer *mixer;
	u32 cnt = 0xffff;	/* init it to an invalid value */
	u32 init;
	u32 height;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (!mixer) {
		mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
		if (!mixer) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
			goto exit;
		}
	}

	init = mdss_mdp_pingpong_read
		(mixer, MDSS_MDP_REG_PP_VSYNC_INIT_VAL) & 0xffff;

	height = mdss_mdp_pingpong_read
		(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT) & 0xffff;

	if (height < init) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
		goto exit;
	}

	cnt = mdss_mdp_pingpong_read
		(mixer, MDSS_MDP_REG_PP_INT_COUNT_VAL) & 0xffff;

	if (cnt < init)		/* wrap around happened at height */
		cnt += (height - init);
	else
		cnt -= init;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	pr_debug("cnt=%d init=%d height=%d\n", cnt, init, height);
exit:
	return cnt;
}

/*
 * TE configuration:
 * dsi byte clock calculated base on 70 fps
 * around 14 ms to complete a kickoff cycle if te disabled
 * vclk_line base on 60 fps
 * write is faster than read
 * init == start == rdptr
 */
static int mdss_mdp_cmd_tearcheck_cfg(struct mdss_mdp_mixer *mixer,
			struct mdss_mdp_cmd_ctx *ctx, int enable)
{
	u32 cfg;

	cfg = BIT(19); /* VSYNC_COUNTER_EN */
	if (ctx->tear_check)
		cfg |= BIT(20);	/* VSYNC_IN_EN */
	cfg |= ctx->vclk_line;

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT,
				0xfff0); /* set to verh height */

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_VSYNC_INIT_VAL,
						ctx->height);

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_RD_PTR_IRQ,
						ctx->height + 1);

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_START_POS,
						ctx->height);

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_THRESH,
		   (CONTINUE_THRESHOLD << 16) | (ctx->start_threshold));

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_TEAR_CHECK_EN, enable);
	return 0;
}

static int mdss_mdp_cmd_tearcheck_setup(struct mdss_mdp_ctl *ctl, int enable)
{
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_mixer *mixer;

	pinfo = &ctl->panel_data->panel_info;

	if (pinfo->mipi.vsync_enable && enable) {
		u32 mdp_vsync_clk_speed_hz, total_lines;

		mdss_mdp_vsync_clk_enable(1);

		mdp_vsync_clk_speed_hz =
		mdss_mdp_get_clk_rate(MDSS_CLK_MDP_VSYNC);
		pr_debug("%s: vsync_clk_rate=%d\n", __func__,
					mdp_vsync_clk_speed_hz);

		if (mdp_vsync_clk_speed_hz == 0) {
			pr_err("can't get clk speed\n");
			return -EINVAL;
		}

		ctx->tear_check = pinfo->mipi.hw_vsync_mode;
		ctx->height = pinfo->yres;
		ctx->vporch = pinfo->lcdc.v_back_porch +
				    pinfo->lcdc.v_front_porch +
				    pinfo->lcdc.v_pulse_width;

		ctx->start_threshold = START_THRESHOLD;

		total_lines = ctx->height + ctx->vporch;
		total_lines *= pinfo->mipi.frame_rate;
		ctx->vclk_line = mdp_vsync_clk_speed_hz / total_lines;

		pr_debug("%s: fr=%d tline=%d vcnt=%d thold=%d vrate=%d\n",
			__func__, pinfo->mipi.frame_rate, total_lines,
				ctx->vclk_line, ctx->start_threshold,
				mdp_vsync_clk_speed_hz);
	} else {
		enable = 0;
	}

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (mixer)
		mdss_mdp_cmd_tearcheck_cfg(mixer, ctx, enable);

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
	if (mixer)
		mdss_mdp_cmd_tearcheck_cfg(mixer, ctx, enable);

	return 0;
}

static void mdss_mdp_cmd_readptr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;
	ktime_t vsync_time;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	complete_all(&ctx->vsync_comp);

	vsync_time = ktime_get();
	ctl->vsync_cnt++;

	pr_debug("%s: num=%d ctx=%d expire=%d koff=%d vsync_time=%d\n",
		 __func__, ctl->num, ctx->pp_num, ctx->expire, ctx->koff_cnt,
		 (int)ktime_to_ms(vsync_time));

	spin_lock(&ctx->clk_lock);
	if (ctx->send_vsync)
		ctx->send_vsync(ctl, vsync_time);

	if (ctx->expire) {
		ctx->expire--;
		if (ctx->expire == 0) {
			if (ctx->koff_cnt <= 0) {
				ctx->clk_control = 1;
				schedule_work(&ctx->clk_work);
			} else {
				/* put off one vsync */
				ctx->expire += 1;
			}
		}
	}
	spin_unlock(&ctx->clk_lock);
}

static void mdss_mdp_cmd_pingpong_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	spin_lock(&ctx->clk_lock);
	mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);

	complete_all(&ctx->pp_comp);

	if (ctx->koff_cnt)
		ctx->koff_cnt--;

	pr_debug("%s: ctl_num=%d intf_num=%d ctx=%d kcnt=%d\n", __func__,
		ctl->num, ctl->intf_num, ctx->pp_num, ctx->koff_cnt);

	spin_unlock(&ctx->clk_lock);
}

static void clk_ctrl_work(struct work_struct *work)
{
	unsigned long flags;
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), clk_work);

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	pr_debug("%s:ctx=%p num=%d\n", __func__, ctx, ctx->pp_num);

	mutex_lock(&ctx->clk_mtx);
	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (ctx->clk_control && ctx->clk_enabled) {
		ctx->clk_enabled = 0;
		ctx->clk_control = 0;
		spin_unlock_irqrestore(&ctx->clk_lock, flags);
		/*
		 * make sure dsi link is idle  here
		 */
		ctx->vsync_enabled = 0;
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_PING_PONG_RD_PTR,
						ctx->pp_num);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
		/* disable dsi clock */
		mdss_mdp_ctl_intf_event(ctx->ctl, MDSS_EVENT_PANEL_CLK_CTRL,
							(void *)0);
		complete(&ctx->stop_comp);
		pr_debug("%s: SET_CLK_OFF, pid=%d\n", __func__, current->pid);
	} else {
		spin_unlock_irqrestore(&ctx->clk_lock, flags);
	}
	mutex_unlock(&ctx->clk_mtx);
}

static int mdss_mdp_cmd_vsync_ctrl(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handler)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;
	int enable;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -ENODEV;
	}

	enable = (handler->vsync_handler != NULL);

	mutex_lock(&ctx->clk_mtx);

	pr_debug("%s: ctx=%p ctx=%d enabled=%d %d clk_enabled=%d clk_ctrl=%d\n",
			__func__, ctx, ctx->pp_num, ctx->vsync_enabled, enable,
					ctx->clk_enabled, ctx->clk_control);

	if (ctx->vsync_enabled == enable) {
		mutex_unlock(&ctx->clk_mtx);
		return 0;
	}

	if (enable) {
		spin_lock_irqsave(&ctx->clk_lock, flags);
		ctx->clk_control = 0;
		ctx->expire = 0;
		ctx->send_vsync = handler->vsync_handler;
		spin_unlock_irqrestore(&ctx->clk_lock, flags);
		if (ctx->clk_enabled == 0) {
			mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_CLK_CTRL,
							(void *)1);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
			mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_RD_PTR,
							ctx->pp_num);
			ctx->vsync_enabled = 1;
			ctx->clk_enabled = 1;
			pr_debug("%s: SET_CLK_ON, pid=%d\n", __func__,
						current->pid);
		}
	} else {
		spin_lock_irqsave(&ctx->clk_lock, flags);
		ctx->expire = VSYNC_EXPIRE_TICK;
		spin_unlock_irqrestore(&ctx->clk_lock, flags);
	}
	mutex_unlock(&ctx->clk_mtx);

	return 0;
}

static void mdss_mdp_cmd_chk_clock(struct mdss_mdp_cmd_ctx *ctx)
{
	unsigned long flags;
	int set_clk_on = 0;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	mutex_lock(&ctx->clk_mtx);

	pr_debug("%s: ctx=%p num=%d clk_enabled=%d\n", __func__,
				ctx, ctx->pp_num, ctx->clk_enabled);

	spin_lock_irqsave(&ctx->clk_lock, flags);
	ctx->koff_cnt++;
	ctx->clk_control = 0;
	ctx->expire = VSYNC_EXPIRE_TICK;
	if (ctx->clk_enabled == 0) {
		set_clk_on++;
		ctx->clk_enabled = 1;
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (set_clk_on) {
		mdss_mdp_ctl_intf_event(ctx->ctl, MDSS_EVENT_PANEL_CLK_CTRL,
							(void *)1);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
		ctx->vsync_enabled = 1;
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num);
		pr_debug("%s: ctx=%p num=%d SET_CLK_ON\n", __func__,
						ctx, ctx->pp_num);
	}
	mutex_unlock(&ctx->clk_mtx);
}

static int mdss_mdp_cmd_wait4pingpong(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;
	int need_wait = 0;
	int rc = 0;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (ctx->koff_cnt > 0)
		need_wait = 1;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	pr_debug("%s: need_wait=%d  intf_num=%d ctx=%p\n",
			__func__, need_wait, ctl->intf_num, ctx);

	if (need_wait) {
		rc = wait_for_completion_timeout(
				&ctx->pp_comp, KOFF_TIMEOUT);

		if (rc <= 0) {
			WARN(1, "cmd kickoff timed out (%d) ctl=%d\n",
						rc, ctl->num);
			rc = -EPERM;
		} else
			rc = 0;
	}

	return rc;
}

int mdss_mdp_cmd_kickoff(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_cmd_ctx *ctx;
	int rc;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (ctx->panel_on == 0) {
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_UNBLANK, NULL);
		WARN(rc, "intf %d unblank error (%d)\n", ctl->intf_num, rc);

		ctx->panel_on++;

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_ON, NULL);
		WARN(rc, "intf %d panel on error (%d)\n", ctl->intf_num, rc);
	}

	mdss_mdp_cmd_chk_clock(ctx);

	/*
	 * tx dcs command if had any
	 */
	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_CMDLIST_KOFF, NULL);

	INIT_COMPLETION(ctx->vsync_comp);
	INIT_COMPLETION(ctx->pp_comp);
	mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
	mb();

	return 0;
}

int mdss_mdp_cmd_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx;
	int need_wait = 0;
	struct mdss_mdp_vsync_handler null_handle;
	int ret;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	pr_debug("%s:+ vaync_enable=%d expire=%d\n", __func__,
		ctx->vsync_enabled, ctx->expire);

	mutex_lock(&ctx->clk_mtx);
	if (ctx->vsync_enabled) {
		INIT_COMPLETION(ctx->stop_comp);
		need_wait = 1;
	}
	mutex_unlock(&ctx->clk_mtx);

	if (need_wait)
		wait_for_completion_interruptible(&ctx->stop_comp);

	ctx->panel_on = 0;

	null_handle.vsync_handler = NULL;
	mdss_mdp_cmd_vsync_ctrl(ctl, &null_handle);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num,
				   NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   NULL, NULL);

	memset(ctx, 0, sizeof(*ctx));
	ctl->priv_data = NULL;

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK, NULL);
	WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF, NULL);
	WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);

	ctl->stop_fnc = NULL;
	ctl->display_fnc = NULL;
	ctl->wait_pingpong = NULL;
	ctl->add_vsync_handler = NULL;
	ctl->remove_vsync_handler = NULL;

	pr_debug("%s:-\n", __func__);

	return 0;
}

int mdss_mdp_cmd_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_mdp_mixer *mixer;
	int i, ret;

	pr_debug("%s:+\n", __func__);

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (!mixer) {
		pr_err("mixer not setup correctly\n");
		return -ENODEV;
	}

	for (i = 0; i < MAX_SESSIONS; i++) {
		ctx = &mdss_mdp_cmd_ctx_list[i];
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
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	ctx->ctl = ctl;
	ctx->pp_num = mixer->num;
	init_completion(&ctx->vsync_comp);
	init_completion(&ctx->pp_comp);
	init_completion(&ctx->stop_comp);
	spin_lock_init(&ctx->clk_lock);
	mutex_init(&ctx->clk_mtx);
	INIT_WORK(&ctx->clk_work, clk_ctrl_work);

	pr_debug("%s: ctx=%p num=%d mixer=%d\n", __func__,
				ctx, ctx->pp_num, mixer->num);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num,
				   mdss_mdp_cmd_readptr_done, ctl);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   mdss_mdp_cmd_pingpong_done, ctl);

	ret = mdss_mdp_cmd_tearcheck_setup(ctl, 1);
	if (ret) {
		pr_err("tearcheck setup failed\n");
		return ret;
	}

	ctl->stop_fnc = mdss_mdp_cmd_stop;
	ctl->display_fnc = mdss_mdp_cmd_kickoff;
	ctl->wait_pingpong = mdss_mdp_cmd_wait4pingpong;
	ctl->add_vsync_handler = mdss_mdp_cmd_vsync_ctrl;
	ctl->remove_vsync_handler = mdss_mdp_cmd_vsync_ctrl;
	ctl->read_line_cnt_fnc = mdss_mdp_cmd_line_count;

	pr_debug("%s:-\n", __func__);

	return 0;
}

