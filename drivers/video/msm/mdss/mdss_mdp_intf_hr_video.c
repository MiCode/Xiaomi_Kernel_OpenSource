/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>
#include <linux/hrtimer.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_trace.h"

#define VSYNC_TIMER		16666666
#define INACTIVITY_TIMER	10
#define VSYNC_TIMEOUT_US	100000

/*
 * The number of ticks to wait for before starting the process of entering
 * hr_video mode. i.e blank blank blank repeat repeat
 */
#define INACTIVITY_CNT 0

/* The number of blank frames to be inserted before sending the repeat frames.
 */
#define BLANK_NUM 3

/*
 * The number of repeat frames to be sent.
 */
#define REPEAT_CNT 2

/*
 * The fps for the minimum refresh rate. This is the number of ticks before we
 * send the minimum update frame.
 * e.g if hr_video refresh rate is 1fps, we need to send frames every 60 ticks
 * if 2fps, this needs to be 60/2 = 30
 */
#define HR_VIDEO_REFRESH_CNT 60

/*
 * This is the delay in msecs that the DSI pll takes to lock
 */
#define DSI_CLK_DELAY 10 /* 10 ms */
#define SCHEDULE_DSI_CLK_ON_TIME	nsecs_to_jiffies(\
		(VSYNC_TIMER*HR_VIDEO_REFRESH_CNT) - (DSI_CLK_DELAY))

#define MDP_INTR_MASK_INTF_VSYNC(intf_num) \
	(1 << (2 * ((intf_num) - MDSS_MDP_INTF0) + MDSS_MDP_IRQ_INTF_VSYNC))

/*
 * Possible states for hr_video state machine i.e.ctx->hr_video_state
 * @POWER_ON: Interface ON
 * @NEW_UPDATE: New video update
 * @FIRST_UPDATE: First video frame after turning Interface on
 * @BLANK_FRAME: Blank frame needs to be sent before the repetition frame
 * @REPEAT_FRAME: Previous frame needs to be sent to the panel at the minimum
 *			 fps specified (HR_VIDEO_REFRESH_CNT).
 * @HR_VIDEO_MODE: Minimum refresh period
 * @SOFT_HR_VIDEO_MODE: Period where interface transitioning to Minimum refresh
 *			 mode
 * @POWER_OFF: Interface is off.
 */
enum mdss_mdp_hr_video_mode {
	POWER_ON,
	NEW_UPDATE,
	FIRST_UPDATE,
	BLANK_FRAME,
	REPEAT_FRAME,
	HR_VIDEO_MODE,
	SOFT_HR_VIDEO_MODE,
	POWER_OFF,
};

/*
 * Possible states for timing generator i.e. ctx->tg_state
 * @HW_TG_ON: Timing generator is turned on in h/w
 * @SW_TG_OFF: Timing generator turned off in s/w but h/w will turn off on the
 *		next vsync
 * @HW_TG_OFF: Timing generator is off in s/w and h/w both.
 */
enum mdss_mdp_hr_video_tg_state {
	HW_TG_ON,
	SW_TG_OFF,
	HW_TG_OFF,
};

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

struct mdss_mdp_hr_video_ctx {
	u32 intf_num;
	char __iomem *base;
	u32 intf_type;
	u8 ref_cnt;
	bool power_on;
	int vsync_pending;
	int tg_toggle_pending;
	ktime_t last_hrtick_time;

	u8 timegen_en;
	bool polling_en;
	u32 poll_cnt;
	struct completion vsync_comp;
	atomic_t vsync_handler_cnt;
	s32 hrtimer_next_wait_ns;
	int wait_pending;
	enum mdss_mdp_hr_video_tg_state tg_state;
	enum mdss_mdp_hr_video_mode hr_video_state;
	u32 tick_cnt;
	struct mutex clk_mtx;
	int clk_enabled;
	struct work_struct clk_work;
	struct delayed_work hr_video_clk_work;

	spinlock_t hrtimer_lock;
	struct hrtimer vsync_hrtimer;
	bool hrtimer_init;
	bool repeat_frame_running;

	atomic_t vsync_ref;
	spinlock_t vsync_lock;
	struct mutex vsync_mtx;
	struct list_head vsync_handlers;
	struct mdss_mdp_ctl *ctl;
};

static inline void video_vsync_irq_enable(struct mdss_mdp_ctl *ctl, bool clear)
{
	struct mdss_mdp_hr_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	mutex_lock(&ctx->vsync_mtx);
	if (atomic_inc_return(&ctx->vsync_ref) == 1)
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num);
	else if (clear)
		mdss_mdp_irq_clear(ctl->mdata, MDSS_MDP_IRQ_INTF_VSYNC,
				ctl->intf_num);
	mutex_unlock(&ctx->vsync_mtx);
}

static inline void video_vsync_irq_disable(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_hr_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	mutex_lock(&ctx->vsync_mtx);
	if (atomic_dec_return(&ctx->vsync_ref) == 0)
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num);
	mutex_unlock(&ctx->vsync_mtx);
}

static inline void mdss_mdp_hr_video_clk_on(struct mdss_mdp_hr_video_ctx *ctx)
{
	/* Hook to enable POWER related call like DSI CLK/MDP Clocks */
	mutex_lock(&ctx->clk_mtx);
	if (!ctx->clk_enabled) {
		ctx->clk_enabled = 1;
		pr_debug("Sending CLK ON\n");
	}
	mutex_unlock(&ctx->clk_mtx);
}

static inline void mdss_mdp_hr_video_clk_off(struct mdss_mdp_hr_video_ctx *ctx)
{

	/* Hook to disable POWER related call like DSI CLK/MDP Clocks */
	mutex_lock(&ctx->clk_mtx);
	if (ctx->clk_enabled) {
		ctx->clk_enabled = 0;
		pr_debug("Sending CLK OFF\n");
	}
	mutex_unlock(&ctx->clk_mtx);
}

static void clk_ctrl_hr_video_work(struct work_struct *work)
{
	struct delayed_work *dw = to_delayed_work(work);
	struct mdss_mdp_hr_video_ctx *ctx = container_of(dw, typeof(*ctx),
			hr_video_clk_work);
	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}
	mdss_mdp_hr_video_clk_on(ctx);
}

static void clk_ctrl_work(struct work_struct *work)
{

	struct mdss_mdp_hr_video_ctx *ctx = container_of(work, typeof(*ctx),
			clk_work);
	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	mdss_mdp_hr_video_clk_off(ctx);
}

static inline void mdp_video_write(struct mdss_mdp_hr_video_ctx *ctx,
				   u32 reg, u32 val)
{
	writel_relaxed(val, ctx->base + reg);
}

static inline u32 mdp_video_read(struct mdss_mdp_hr_video_ctx *ctx,
				   u32 reg)
{
	return readl_relaxed(ctx->base + reg);
}

static inline u32 mdss_mdp_hr_video_line_count(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	u32 line_cnt = 0;

	if (!ctl || !ctl->intf_ctx[MASTER_CTX])
		goto line_count_exit;

	ctx = ctl->intf_ctx[MASTER_CTX];

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	line_cnt = mdp_video_read(ctx, MDSS_MDP_REG_INTF_LINE_COUNT);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

line_count_exit:
	return line_cnt;
}

static int mdss_mdp_cancel_hrtimer(struct mdss_mdp_hr_video_ctx *ctx)
{
	int ret = 0;
	if (ctx->hrtimer_init) {
		pr_debug("Cancel timer\n");
		ret = hrtimer_cancel(&ctx->vsync_hrtimer);
	}
	ctx->hrtimer_init = false;
	return 0;
}

static bool mdss_mdp_hr_video_start_timer(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_hr_video_ctx *ctx;

	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (!ctx->hrtimer_init) {
		pr_debug("Hrtimer start\n");
		hrtimer_start(&ctx->vsync_hrtimer, ns_to_ktime(VSYNC_TIMER),
			HRTIMER_MODE_REL);
		ctx->hrtimer_init = true;
	}

	return ctx->hrtimer_init;
}

static int mdss_mdp_timegen_enable(struct mdss_mdp_ctl *ctl, bool enable)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	pr_debug("Setting tg=%d\n", enable);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, enable ? 1 :
			0);
	/* Need to wait for the timing generator to be disabled. */
	wmb();
	return 0;

}

static int mdss_mdp_hr_video_timer_reprogram(struct mdss_mdp_hr_video_ctx *ctx)
{
	int ret = 0;

	ret = hrtimer_try_to_cancel(&ctx->vsync_hrtimer);
	pr_debug("Reprogramming the timer to the Vsync Timer.\n");
	if (ret != -1) {
		/*
		 * Timer was cancelled, start the timer again.
		 */
		hrtimer_start(&ctx->vsync_hrtimer, ns_to_ktime(
			ctx->hrtimer_next_wait_ns), HRTIMER_MODE_REL);
	}
	return 0;
}

static void mdss_mdp_end_full_hr_video_mode(struct mdss_mdp_hr_video_ctx *ctx)
{
	u32 rem = ktime_to_ns(hrtimer_expires_remaining(&ctx->vsync_hrtimer));
	u64 tick_remaining_ns = 0;

	ctx->hrtimer_next_wait_ns = (u32)rem;

	if (rem > VSYNC_TIMER) {
		tick_remaining_ns =
			ktime_to_ns(hrtimer_expires_remaining(
						&ctx->vsync_hrtimer));
		div_u64_rem(tick_remaining_ns, VSYNC_TIMER,
			&ctx->hrtimer_next_wait_ns);
		pr_debug(" new timer tick scheduled after %d\n",
			ctx->hrtimer_next_wait_ns);
		mdss_mdp_hr_video_timer_reprogram(ctx);
	}
	ctx->hrtimer_next_wait_ns = VSYNC_TIMER;
	ctx->tg_toggle_pending++;
	ctx->hr_video_state = NEW_UPDATE;
}

static int mdss_mdp_queue_commit(struct mdss_mdp_hr_video_ctx *ctx)
{
	unsigned long flags;
	enum mdss_mdp_hr_video_mode mode;
	enum mdss_mdp_hr_video_tg_state tstate;

	spin_lock_irqsave(&ctx->hrtimer_lock, flags);

	mode = ctx->hr_video_state;
	tstate = ctx->tg_state;
	ctx->tick_cnt = 0;

	if (mode == HR_VIDEO_MODE) {
		mdss_mdp_end_full_hr_video_mode(ctx);
		goto commit_done;
	} else if (mode == FIRST_UPDATE) {
		/* There will be an extra vsync generated when tg=HW_TG_OFF */
		ctx->vsync_pending += 2;
		mdss_mdp_hr_video_start_timer(ctx->ctl);
		ctx->tg_state = HW_TG_ON;
		mdss_mdp_timegen_enable(ctx->ctl, true);
		ctx->last_hrtick_time = ktime_get();
	} else {
		u32 cur_line, vtot;
		switch (tstate) {
		case HW_TG_ON:
			ctx->vsync_pending++;
			break;
		case SW_TG_OFF:
			cur_line = mdp_video_read(ctx,
					MDSS_MDP_REG_INTF_LINE_COUNT);
			vtot =
				mdss_panel_get_vtotal(
					&ctx->ctl->panel_data->panel_info);
			if (cur_line < (vtot - 1)) {
				ctx->vsync_pending++;
				mdss_mdp_timegen_enable(ctx->ctl, true);
				ctx->tg_state = HW_TG_ON;
			} else {
				ctx->tg_toggle_pending++;
			}
			ctx->hr_video_state = NEW_UPDATE;
			break;
		case HW_TG_OFF:
			ctx->tg_toggle_pending++;
			break;
		default:
			pr_err("Timing Generator state invalid\n");
			BUG();
			break;

		}
	}
	ctx->hr_video_state = NEW_UPDATE;

commit_done:
	spin_unlock_irqrestore(&ctx->hrtimer_lock, flags);

	return 0;
}

static int mdss_mdp_push_one_frame(struct mdss_mdp_hr_video_ctx *ctx)
{
	enum mdss_mdp_hr_video_tg_state tstate;
	u32 cur_line, vtot;

	tstate = ctx->tg_state;
	switch (tstate) {
	case HW_TG_ON:
		ctx->last_hrtick_time = ktime_get();
		ctx->vsync_pending++;
		break;
	case  SW_TG_OFF:
		cur_line = mdp_video_read(ctx,
				MDSS_MDP_REG_INTF_LINE_COUNT);
		vtot =
			mdss_panel_get_vtotal(
					&ctx->ctl->panel_data->panel_info);

		if (cur_line < (vtot - 1)) {
			ctx->vsync_pending++;
			ctx->tg_state = HW_TG_ON;
			mdss_mdp_timegen_enable(ctx->ctl, true);
		} else {
			/*
			 * There will be an extra vsync generated when
			 * tg=HW_TG_OFF
			 */
			ctx->vsync_pending += 2;
			ctx->tg_state = HW_TG_ON;
			mdss_mdp_timegen_enable(ctx->ctl, true);
		}
		break;
	case HW_TG_OFF:
		/*
		 * There will be an extra vsync generated when
		 * tg=HW_TG_OFF
		 */
		ctx->vsync_pending += 2;
		ctx->tg_state = HW_TG_ON;
		mdss_mdp_timegen_enable(ctx->ctl, true);
		break;
	default:
		break;
	}
	return 0;
}

static void mdss_mdp_start_full_hr_video_mode(struct mdss_mdp_hr_video_ctx *ctx)
{
	ctx->hr_video_state = HR_VIDEO_MODE;
	pr_debug("Going to hr_video mode\n");
	schedule_delayed_work(&ctx->hr_video_clk_work,
			SCHEDULE_DSI_CLK_ON_TIME);
	ctx->hrtimer_next_wait_ns = VSYNC_TIMER * HR_VIDEO_REFRESH_CNT;
}

static int mdss_mdp_toggle_timegen(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	enum mdss_mdp_hr_video_tg_state tstate;
	enum mdss_mdp_hr_video_mode mode;
	int tmp_cnt;

	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	pr_debug("enabling timing gen for intf=%d\n", ctl->intf_num);
	tstate = ctx->tg_state;
	mode = ctx->hr_video_state;
	ctx->tick_cnt++;
	tmp_cnt = ctx->tick_cnt;

	if (ctx->tg_toggle_pending > 0) {
		mdss_mdp_push_one_frame(ctx);
		ctx->last_hrtick_time = ktime_get();
		ctx->tick_cnt = 0;
		ctx->tg_toggle_pending--;
	} else if (mode == SOFT_HR_VIDEO_MODE) {
		if (tmp_cnt == HR_VIDEO_REFRESH_CNT) {
			if (atomic_read(&ctx->vsync_handler_cnt) == 0) {
				/* Vsync disabled, switch to full hr_video */
				mdss_mdp_start_full_hr_video_mode(ctx);
				mdss_mdp_push_one_frame(ctx);
			} else {
				/* Update every 60ticks. */
				ctx->hrtimer_next_wait_ns = VSYNC_TIMER;
				mdss_mdp_push_one_frame(ctx);
				ctx->tick_cnt = 0;
			}
		}
	} else if (mode == HR_VIDEO_MODE) {
		mdss_mdp_push_one_frame(ctx);
		mdss_mdp_start_full_hr_video_mode(ctx);
	} else if (mode == NEW_UPDATE && (ctx->vsync_pending > 1)) {
		ctx->tick_cnt = 0;
	} else if ((tmp_cnt >= (INACTIVITY_CNT + 1))  &&
			(tmp_cnt < (INACTIVITY_CNT + BLANK_NUM + 1))) {
		/* Blank frames. Do nothing */
		ATRACE_BEGIN("BLANK");
		pr_debug("Sending %d BLANK frames\n", BLANK_NUM);
		ctx->hr_video_state = BLANK_FRAME;
		ATRACE_END("BLANK");
	} else if (tmp_cnt == (INACTIVITY_CNT + BLANK_NUM + 1)) {
		ATRACE_BEGIN("REPEAT");
		if (REPEAT_CNT > 0) {
			pr_debug("Sending %d REPEAT Frames\n", REPEAT_CNT);
			ctx->hr_video_state = REPEAT_FRAME;
			mdss_mdp_push_one_frame(ctx);
			ctx->vsync_pending += (REPEAT_CNT - 1);
			ctx->last_hrtick_time = ktime_get();
		}
		ATRACE_END("REPEAT");
	} else if (tmp_cnt == (INACTIVITY_CNT + BLANK_NUM + REPEAT_CNT)) {
		if (atomic_read(&ctx->vsync_handler_cnt) == 0) {
			mdss_mdp_start_full_hr_video_mode(ctx);
		} else {
			pr_debug("Going to SOFT hr_video\n");
			ctx->hr_video_state = SOFT_HR_VIDEO_MODE;
			ctx->hrtimer_next_wait_ns = VSYNC_TIMER;
		}
	}

	return 0;
}

static int mod_hrtimer(struct hrtimer *vsync_timer, u32 duration)
{
	return hrtimer_forward_now(vsync_timer, ns_to_ktime(duration));
}

static enum hrtimer_restart hrt_vsync_cb(struct hrtimer *vsync_timer)
{
	int ret = HRTIMER_NORESTART;
	struct mdss_mdp_vsync_handler *tmp;
	unsigned long flags;
	ktime_t curr = ktime_get();
	struct mdss_mdp_hr_video_ctx *ctx =
		container_of(vsync_timer, typeof(*ctx), vsync_hrtimer);

	if (!ctx) {
		pr_err("invalid ctx\n");
		ret = HRTIMER_NORESTART;
		return ret;
	}
	ATRACE_BEGIN("hrtick");

	if (atomic_read(&ctx->vsync_handler_cnt) > 0) {
		spin_lock(&ctx->vsync_lock);
		list_for_each_entry(tmp, &ctx->vsync_handlers, list)
			tmp->vsync_handler(ctx->ctl, curr);
		spin_unlock(&ctx->vsync_lock);
	}

	spin_lock_irqsave(&ctx->hrtimer_lock, flags);
	mdss_mdp_toggle_timegen(ctx->ctl);

	mod_hrtimer(&ctx->vsync_hrtimer, ctx->hrtimer_next_wait_ns);
	spin_unlock_irqrestore(&ctx->hrtimer_lock, flags);

	ret = HRTIMER_RESTART;
	ATRACE_END("hrtick");

	return ret;
}

static int mdss_mdp_register_hrtimer(struct mdss_mdp_hr_video_ctx *ctx)
{
	hrtimer_init(&ctx->vsync_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ctx->vsync_hrtimer.function = hrt_vsync_cb;
	pr_debug("Register the callback for the hrtimer\n");
	return 0;
}

int mdss_mdp_hr_video_addr_setup(struct mdss_data_type *mdata,
				u32 *offsets,  u32 count)
{
	struct mdss_mdp_hr_video_ctx *head;
	u32 i;

	head = devm_kzalloc(&mdata->pdev->dev,
			sizeof(struct mdss_mdp_hr_video_ctx) * count,
			GFP_KERNEL);
	if (!head)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		head[i].base = mdata->mdp_base + offsets[i];
		pr_debug("adding Video Intf #%d offset=0x%x virt=%p\n", i,
				offsets[i], head[i].base);
		head[i].ref_cnt = 0;
		head[i].intf_num = i + MDSS_MDP_INTF0;
		INIT_LIST_HEAD(&head[i].vsync_handlers);
	}

	mdata->video_intf = head;
	mdata->nintf = count;
	return 0;
}

static int mdss_mdp_hr_video_timegen_setup(struct mdss_mdp_ctl *ctl,
					struct intf_timing_params *p)
{
	u32 hsync_period, vsync_period;
	u32 hsync_start_x, hsync_end_x, display_v_start, display_v_end;
	u32 active_h_start, active_h_end, active_v_start, active_v_end;
	u32 den_polarity, hsync_polarity, vsync_polarity;
	u32 display_hctl, active_hctl, hsync_ctl, polarity_ctl;
	struct mdss_mdp_hr_video_ctx *ctx;
	struct mdss_data_type *mdata;

	mdata = ctl->mdata;
	ctx = ctl->intf_ctx[MASTER_CTX];
	hsync_period = p->hsync_pulse_width + p->h_back_porch +
			p->width + p->h_front_porch;
	vsync_period = p->vsync_pulse_width + p->v_back_porch +
			p->height + p->v_front_porch;

	display_v_start = ((p->vsync_pulse_width + p->v_back_porch) *
			hsync_period) + p->hsync_skew;
	display_v_end = ((vsync_period - p->v_front_porch) * hsync_period) +
			p->hsync_skew - 1;

	if (ctx->intf_type == MDSS_INTF_EDP) {
		display_v_start += p->hsync_pulse_width + p->h_back_porch;
		display_v_end -= p->h_front_porch;
	}

	/* TIMING_2 flush bit on 8939 is BIT 31 */
	if (mdata->mdp_rev == MDSS_MDP_HW_REV_108 &&
				ctx->intf_num == MDSS_MDP_INTF2)
		ctl->flush_bits |= BIT(31);
	else
		ctl->flush_bits |= BIT(31) >>
			(ctx->intf_num - MDSS_MDP_INTF0);

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
	if (MDSS_INTF_HDMI == ctx->intf_type) {
		hsync_polarity = p->yres >= 720 ? 0 : 1;
		vsync_polarity = p->yres >= 720 ? 0 : 1;
	} else {
		hsync_polarity = 0;
		vsync_polarity = 0;
	}
	polarity_ctl = (den_polarity << 2)   | /*  DEN Polarity  */
		       (vsync_polarity << 1) | /* VSYNC Polarity */
		       (hsync_polarity << 0);  /* HSYNC Polarity */

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_HSYNC_CTL, hsync_ctl);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
			vsync_period * hsync_period);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PULSE_WIDTH_F0,
			   p->vsync_pulse_width * hsync_period);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_DISPLAY_HCTL, display_hctl);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_DISPLAY_V_START_F0,
			   display_v_start);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_DISPLAY_V_END_F0, display_v_end);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_ACTIVE_HCTL, active_hctl);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_ACTIVE_V_START_F0,
			   active_v_start);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_ACTIVE_V_END_F0, active_v_end);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_BORDER_COLOR, p->border_clr);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_UNDERFLOW_COLOR,
			   p->underflow_clr);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_HSYNC_SKEW, p->hsync_skew);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_POLARITY_CTL, polarity_ctl);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_FRAME_LINE_COUNT_EN, 0x3);

	return 0;
}


static int mdss_mdp_hr_video_add_hrt_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	unsigned long flags;
	int ret = 0;
	bool irq_en = false;

	if (!handle || !(handle->vsync_handler)) {
		ret = -EINVAL;
		goto exit;
	}

	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		ret = -ENODEV;
		goto exit;
	}
	atomic_inc(&ctx->vsync_handler_cnt);

	MDSS_XLOG(ctl->num, ctl->vsync_cnt, handle->enabled);

	spin_lock_irqsave(&ctx->vsync_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->vsync_handlers);
		irq_en = true;
	}
	spin_unlock_irqrestore(&ctx->vsync_lock, flags);
	if (irq_en)
		video_vsync_irq_enable(ctl, false);
exit:
	return ret;
}

static int mdss_mdp_hr_video_remove_hrt_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	unsigned long flags;
	bool irq_dis = false;

	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	MDSS_XLOG(ctl->num, ctl->vsync_cnt, handle->enabled);
	atomic_dec(&ctx->vsync_handler_cnt);

	spin_lock_irqsave(&ctx->vsync_lock, flags);
	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);
		irq_dis = true;
	}
	spin_unlock_irqrestore(&ctx->vsync_lock, flags);
	if (irq_dis)
		video_vsync_irq_disable(ctl);
	return 0;
}

static int mdss_mdp_hr_video_stop(struct mdss_mdp_ctl *ctl,
		int panel_power_state)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	struct mdss_mdp_vsync_handler *tmp, *handle;
	struct mdss_mdp_ctl *sctl;
	unsigned long flags;
	int rc;
	u32 frame_rate = 0;

	pr_debug("stop ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}
	MDSS_XLOG(ctl->num, ctl->vsync_cnt);
	list_for_each_entry_safe(handle, tmp, &ctx->vsync_handlers, list)
		mdss_mdp_hr_video_remove_hrt_vsync_handler(ctl, handle);

	mdss_mdp_cancel_hrtimer(ctx);

	spin_lock_irqsave(&ctx->hrtimer_lock, flags);
	ctx->hr_video_state = POWER_OFF;
	ctx->tg_state = HW_TG_OFF;
	ctx->vsync_pending = 0;
	ctx->tg_toggle_pending = 0;

	spin_unlock_irqrestore(&ctx->hrtimer_lock, flags);

	video_vsync_irq_disable(ctl);

	if (cancel_work_sync(&ctx->clk_work))
		pr_debug("Cancelling clk_ok\n");

	if (cancel_delayed_work_sync(&ctx->hr_video_clk_work))
		pr_debug("Cancelling scheduled clk on\n");

	if (ctx->power_on) {
		ctx->power_on = false;
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 1);
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK, NULL);
		if (rc == -EBUSY) {
			pr_debug("intf #%d busy don't turn off\n",
					ctl->intf_num);
			return rc;
		}
		WARN(rc, "intf %d blank error (%d)\n", ctl->intf_num, rc);

		mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 0);
		/* wait for at least one VSYNC on HDMI intf for proper TG OFF */
		if (MDSS_INTF_HDMI == ctx->intf_type) {
			frame_rate = mdss_panel_get_framerate
				(&(ctl->panel_data->panel_info));
			if (!(frame_rate >= 24 && frame_rate <= 240))
				frame_rate = 24;
			msleep((1000/frame_rate) + 1);
		}
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		ctx->timegen_en = false;

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF, NULL);
		WARN(rc, "intf %d timegen off error (%d)\n", ctl->intf_num, rc);

		mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_UNDER_RUN,
				ctl->intf_num);
		sctl = mdss_mdp_get_split_ctl(ctl);
		if (sctl)
			mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_UNDER_RUN,
					sctl->intf_num);
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num,
			NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num,
			NULL, NULL);

	mdss_mdp_ctl_reset(ctl);
	ctx->ref_cnt--;
	ctl->intf_ctx[MASTER_CTX] = NULL;

	return 0;
}

void mdss_mdp_hr_video_vsync_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_hr_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	ktime_t vsync_time;
	s32 cnt;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	ATRACE_BEGIN("vsync");

	vsync_time = ktime_get();
	ctl->vsync_cnt++;

	MDSS_XLOG(ctl->num, ctl->vsync_cnt, ctl->vsync_cnt);

	pr_debug("intr ctl=%d vsync cnt=%u vsync_time=%d\n",
		 ctl->num, ctl->vsync_cnt, (int)ktime_to_ms(vsync_time));

	ctx->polling_en = false;
	complete_all(&ctx->vsync_comp);
	spin_lock(&ctx->hrtimer_lock);

	ctx->vsync_pending--;
	cnt = ctx->vsync_pending;
	if (cnt > 1) {
		ctx->tg_state = HW_TG_ON;
	} else if (cnt == 1) {
		/* No new updates came in stop the timegen */
		mdss_mdp_timegen_enable(ctl, false);
		ctx->tg_state = SW_TG_OFF;
	} else if (cnt == 0) {
		ctx->tg_state = HW_TG_OFF;
		if (ctx->hr_video_state == HR_VIDEO_MODE)
			schedule_work(&ctx->clk_work);
	} else if (cnt < 0) {
		ctx->vsync_pending = 0;
		ctx->tg_state = HW_TG_OFF;
	}

	spin_unlock(&ctx->hrtimer_lock);
	ATRACE_END("vsync");
}

static int mdss_mdp_hr_video_pollwait(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_hr_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	u32 mask, status;
	int rc;
	unsigned long flags;

	mask = MDP_INTR_MASK_INTF_VSYNC(ctl->intf_num);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	rc = readl_poll_timeout(ctl->mdata->mdp_base + MDSS_MDP_REG_INTR_STATUS,
		status,
		(status & mask) || try_wait_for_completion(&ctx->vsync_comp),
		1000,
		VSYNC_TIMEOUT_US);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	if (rc == 0) {
		MDSS_XLOG(ctl->num, ctl->vsync_cnt);
		pr_debug("vsync poll successful! rc=%d status=0x%x\n",
				rc, status);
		ctx->poll_cnt++;
		if (status) {
			struct mdss_mdp_vsync_handler *tmp;
			ktime_t vsync_time = ktime_get();

			spin_lock_irqsave(&ctx->vsync_lock, flags);
			list_for_each_entry(tmp, &ctx->vsync_handlers, list)
				tmp->vsync_handler(ctl, vsync_time);
			spin_unlock_irqrestore(&ctx->vsync_lock, flags);
		}
	} else {
		pr_warn("vsync poll timed out! rc=%d status=0x%x mask=0x%x\n",
				rc, status, mask);
	}

	return rc;
}

static int mdss_mdp_hr_video_wait4comp(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	int rc;

	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	WARN(!ctx->wait_pending, "waiting without commit! ctl=%d", ctl->num);

	if (ctx->polling_en) {
		rc = mdss_mdp_hr_video_pollwait(ctl);
	} else {
		mutex_unlock(&ctl->lock);
		rc = wait_for_completion_timeout(&ctx->vsync_comp,
				usecs_to_jiffies(VSYNC_TIMEOUT_US));
		mutex_lock(&ctl->lock);
		if (rc == 0) {
			pr_warn("vsync wait timeout %d, fallback to poll mode\n",
					ctl->num);
			ctx->polling_en++;
			rc = mdss_mdp_hr_video_pollwait(ctl);
		} else {
			rc = 0;
		}
	}
	mdss_mdp_ctl_notify(ctl,
			rc ? MDP_NOTIFY_FRAME_TIMEOUT : MDP_NOTIFY_FRAME_DONE);

	if (ctx->wait_pending)
		ctx->wait_pending = 0;

	return rc;
}

static void recover_underrun_work(struct work_struct *work)
{
	struct mdss_mdp_ctl *ctl =
		container_of(work, typeof(*ctl), recover_work);

	if (!ctl || !ctl->ops.add_vsync_handler) {
		pr_err("ctl or vsync handler is NULL\n");
		return;
	}

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	ctl->ops.add_vsync_handler(ctl, &ctl->recover_underrun_handler);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

static void mdss_mdp_hr_video_underrun_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	if (unlikely(!ctl))
		return;

	ctl->underrun_cnt++;
	MDSS_XLOG(ctl->num, ctl->underrun_cnt);
	trace_mdp_video_underrun_done(ctl->num, ctl->underrun_cnt);
	pr_debug("display underrun detected for ctl=%d count=%d\n", ctl->num,
			ctl->underrun_cnt);

	if (ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE)
		schedule_work(&ctl->recover_work);
}

static int mdss_mdp_hr_video_display(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_hr_video_ctx *ctx;
	int rc;

	pr_debug("kickoff ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (!ctx->wait_pending) {
		ctx->wait_pending++;
		INIT_COMPLETION(ctx->vsync_comp);
	} else {
		WARN(1, "commit without wait! ctl=%d", ctl->num);
	}

	MDSS_XLOG(ctl->num, ctl->underrun_cnt);

	if (cancel_delayed_work_sync(&ctx->hr_video_clk_work))
		pr_debug("Cancelling Pending  CLK ON at next tick\n");

	if (!ctx->power_on)  {
		/* First on, need to turn on the panel */
		ctx->hr_video_state = FIRST_UPDATE;
		ctx->power_on = true;

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_UNBLANK, NULL);
		if (rc) {
			pr_warn("intf #%d unblank error (%d)\n",
					ctl->intf_num, rc);
			video_vsync_irq_disable(ctl);
			ctx->wait_pending = 0;
			return rc;
		}

		pr_err("enabling timing gen for intf=%d\n", ctl->intf_num);

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

		mdss_mdp_irq_enable(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num);
		mdss_mdp_hr_video_clk_on(ctx);
		mdss_mdp_queue_commit(ctx);

		ctx->timegen_en = true;
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_ON, NULL);
		WARN(rc, "intf %d panel on error (%d)\n", ctl->intf_num, rc);
	} else {
		mdss_mdp_hr_video_clk_on(ctx);
		mdss_mdp_queue_commit(ctx);
	}

	return 0;
}

int mdss_mdp_hr_video_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
	bool handoff)
{
	struct mdss_panel_data *pdata;
	int i, ret = 0, off;
	u32 data, flush;
	struct mdss_mdp_hr_video_ctx *ctx;

	off = 0;
	ctx = (struct mdss_mdp_hr_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	pdata = ctl->panel_data;

	pdata->panel_info.cont_splash_enabled = 0;

	if (!handoff) {
		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CONT_SPLASH_BEGIN,
					      NULL);
		if (ret) {
			pr_err("Failed to handle 'CONT_SPLASH_BEGIN' event\n");
			return ret;
		}

		/* clear up mixer0 and mixer1 */
		flush = 0;
		for (i = 0; i < 2; i++) {
			data = mdss_mdp_ctl_read(ctl,
				MDSS_MDP_REG_CTL_LAYER(i));
			if (data) {
				mdss_mdp_ctl_write(ctl,
					MDSS_MDP_REG_CTL_LAYER(i),
					MDSS_MDP_LM_BORDER_COLOR);
				flush |= (0x40 << i);
			}
		}
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, flush);

		mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 0);
		/* wait for 1 VSYNC for the pipe to be unstaged */
		msleep(20);

		ret = mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_CONT_SPLASH_FINISH, NULL);
	}

	return ret;
}


int mdss_mdp_hr_video_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_hr_video_ctx *ctx;
	struct intf_timing_params itp = {0};
	u32 dst_bpp;
	int i;

	mdata = ctl->mdata;
	pinfo = &ctl->panel_data->panel_info;

	i = ctl->intf_num - MDSS_MDP_INTF0;
	if (i < mdata->nintf) {
		ctx = ((struct mdss_mdp_hr_video_ctx *) mdata->video_intf) + i;
		if (ctx->ref_cnt) {
			pr_err("Intf %d already in use\n", ctl->intf_num);
			return -EBUSY;
		}
		pr_debug("video Intf #%d base=%p", ctx->intf_num, ctx->base);
		ctx->ref_cnt++;
	} else {
		pr_err("Invalid intf number: %d\n", ctl->intf_num);
		return -EINVAL;
	}

	MDSS_XLOG(ctl->num, ctl->vsync_cnt);
	pr_debug("start ctl=%u\n", ctl->num);

	ctl->intf_ctx[MASTER_CTX] = ctx;
	ctx->intf_type = ctl->intf_type;
	ctx->power_on = false;
	ctx->hr_video_state = POWER_OFF;
	ctx->tg_state = HW_TG_OFF;
	ctx->ctl = ctl;

	init_completion(&ctx->vsync_comp);
	spin_lock_init(&ctx->vsync_lock);
	spin_lock_init(&ctx->hrtimer_lock);
	mutex_init(&ctx->vsync_mtx);
	atomic_set(&ctx->vsync_ref, 0);
	INIT_WORK(&ctl->recover_work, recover_underrun_work);
	mutex_init(&ctx->clk_mtx);
	INIT_WORK(&ctx->clk_work, clk_ctrl_work);
	INIT_DELAYED_WORK(&ctx->hr_video_clk_work, clk_ctrl_hr_video_work);
	ctx->hrtimer_next_wait_ns = VSYNC_TIMER;

	video_vsync_irq_enable(ctl, true);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num,
				   mdss_mdp_hr_video_vsync_intr_done, ctl);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num,
				   mdss_mdp_hr_video_underrun_intr_done, ctl);

	dst_bpp = pinfo->fbc.enabled ? (pinfo->fbc.target_bpp) : (pinfo->bpp);

	itp.width = mult_frac((pinfo->xres + pinfo->lcdc.xres_pad),
				dst_bpp, pinfo->bpp);
	itp.height = pinfo->yres + pinfo->lcdc.yres_pad;
	itp.border_clr = pinfo->lcdc.border_clr;
	itp.underflow_clr = pinfo->lcdc.underflow_clr;
	itp.hsync_skew = pinfo->lcdc.hsync_skew;

	itp.xres =  mult_frac(pinfo->xres, dst_bpp, pinfo->bpp);
	itp.yres = pinfo->yres;
	itp.h_back_porch =  mult_frac(pinfo->lcdc.h_back_porch, dst_bpp,
			pinfo->bpp);
	itp.h_front_porch = mult_frac(pinfo->lcdc.h_front_porch, dst_bpp,
			pinfo->bpp);
	itp.v_back_porch =  mult_frac(pinfo->lcdc.v_back_porch, dst_bpp,
			pinfo->bpp);
	itp.v_front_porch = mult_frac(pinfo->lcdc.v_front_porch, dst_bpp,
			pinfo->bpp);
	itp.hsync_pulse_width = mult_frac(pinfo->lcdc.h_pulse_width, dst_bpp,
			pinfo->bpp);
	itp.vsync_pulse_width = pinfo->lcdc.v_pulse_width;

	mdss_mdp_register_hrtimer(ctx);

	if (mdss_mdp_hr_video_timegen_setup(ctl, &itp)) {
		pr_err("unable to get timing parameters\n");
		return -EINVAL;
	}
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_PANEL_FORMAT, ctl->dst_format);

	ctl->ops.stop_fnc = mdss_mdp_hr_video_stop;
	ctl->ops.display_fnc = mdss_mdp_hr_video_display;
	ctl->ops.wait_fnc = mdss_mdp_hr_video_wait4comp;
	ctl->ops.read_line_cnt_fnc = mdss_mdp_hr_video_line_count;
	ctl->ops.add_vsync_handler = mdss_mdp_hr_video_add_hrt_vsync_handler;
	ctl->ops.remove_vsync_handler =
		mdss_mdp_hr_video_remove_hrt_vsync_handler;

	return 0;
}

