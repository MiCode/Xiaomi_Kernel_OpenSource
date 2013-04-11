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

#define START_THRESHOLD 4
#define CONTINUE_TRESHOLD 4

#define MAX_SESSIONS 2

struct mdss_mdp_cmd_ctx {
	u32 pp_num;
	u8 ref_cnt;

	struct completion pp_comp;
	atomic_t vsync_ref;
	spinlock_t vsync_lock;
	mdp_vsync_handler_t vsync_handler;
	int panel_on;

	/* te config */
	u8 tear_check;
	u16 total_lcd_lines;
	u16 v_porch;	/* vertical porches */
	u32 vsync_cnt;
};

struct mdss_mdp_cmd_ctx mdss_mdp_cmd_ctx_list[MAX_SESSIONS];

static int mdss_mdp_cmd_tearcheck_cfg(struct mdss_mdp_mixer *mixer,
			struct mdss_mdp_cmd_ctx *ctx, int enable)
{
	u32 cfg;

	cfg = BIT(19); /* VSYNC_COUNTER_EN */
	if (ctx->tear_check)
		cfg |= BIT(20);	/* VSYNC_IN_EN */
	cfg |= ctx->vsync_cnt;

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT,
				0xfff0); /* set to verh height */
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_VSYNC_INIT_VAL, 0);
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_RD_PTR_IRQ, 0);

	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_START_POS, ctx->v_porch);
	mdss_mdp_pingpong_write(mixer, MDSS_MDP_REG_PP_SYNC_THRESH,
			   (CONTINUE_TRESHOLD << 16) | (START_THRESHOLD));

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
		u32 vsync_cnt_cfg_dem;

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

		total_lines = pinfo->lcdc.v_back_porch +
				    pinfo->lcdc.v_front_porch +
				    pinfo->lcdc.v_pulse_width + pinfo->yres;

		vsync_cnt_cfg_dem =
			mult_frac(pinfo->mipi.frame_rate * total_lines,
						1, 100);

		ctx->vsync_cnt = mdp_vsync_clk_speed_hz / vsync_cnt_cfg_dem;

		ctx->v_porch = pinfo->lcdc.v_back_porch +
				    pinfo->lcdc.v_front_porch +
				    pinfo->lcdc.v_pulse_width;
		ctx->total_lcd_lines = total_lines;
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

static inline void cmd_readptr_irq_enable(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;

	if (atomic_inc_return(&ctx->vsync_ref) == 1) {
		pr_debug("%s:\n", __func__);
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num);
	}
}

static inline void cmd_readptr_irq_disable(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;

	if (atomic_dec_return(&ctx->vsync_ref) == 0) {
		pr_debug("%s:\n", __func__);
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_PING_PONG_RD_PTR,
							ctx->pp_num);
	}
}

int mdss_mdp_cmd_set_vsync_handler(struct mdss_mdp_ctl *ctl,
		mdp_vsync_handler_t vsync_handler)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	spin_lock_irqsave(&ctx->vsync_lock, flags);

	if (!ctx->vsync_handler && vsync_handler) {
		ctx->vsync_handler = vsync_handler;
		cmd_readptr_irq_enable(ctl);
	} else if (ctx->vsync_handler && !vsync_handler) {
		cmd_readptr_irq_disable(ctl);
		ctx->vsync_handler = vsync_handler;
	}

	spin_unlock_irqrestore(&ctx->vsync_lock, flags);

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

	pr_debug("%s: ctl=%d intf_num=%d\n", __func__, ctl->num, ctl->intf_num);

	vsync_time = ktime_get();
	ctl->vsync_cnt++;

	spin_lock(&ctx->vsync_lock);
	if (ctx->vsync_handler)
		ctx->vsync_handler(ctl, vsync_time);
	spin_unlock(&ctx->vsync_lock);
}

static void mdss_mdp_cmd_pingpong_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->priv_data;

	pr_debug("%s: intf_num=%d ctx=%p\n", __func__, ctl->intf_num, ctx);

	mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);

	if (ctx)
		complete(&ctx->pp_comp);
}

int mdss_mdp_cmd_kickoff(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_cmd_ctx *ctx;
	int rc;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	pr_debug("%s: kickoff intf_num=%d ctx=%p\n", __func__,
					ctl->intf_num, ctx);

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

	INIT_COMPLETION(ctx->pp_comp);
	mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);

	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);

	wait_for_completion_interruptible(&ctx->pp_comp);

	return 0;
}

int mdss_mdp_cmd_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx;
	int ret;

	pr_debug("%s: +\n", __func__);

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	ctx->panel_on = 0;

	mdss_mdp_cmd_set_vsync_handler(ctl, NULL);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctl->intf_num,
				   NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   NULL, NULL);

	memset(ctx, 0, sizeof(*ctx));
	ctl->priv_data = NULL;

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK, NULL);
	WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);

	ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF, NULL);
	WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

	pr_debug("%s:-\n", __func__);

	return 0;
}

int mdss_mdp_cmd_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_mdp_mixer *mixer;
	int i, ret;

	pr_debug("%s:+\n", __func__);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

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

	ctx->pp_num = mixer->num;
	init_completion(&ctx->pp_comp);
	spin_lock_init(&ctx->vsync_lock);
	atomic_set(&ctx->vsync_ref, 0);

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
	ctl->set_vsync_handler = mdss_mdp_cmd_set_vsync_handler;

	pr_debug("%s:-\n", __func__);

	return 0;
}

