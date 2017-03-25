/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>
#include <video/msm_hdmi_modes.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_trace.h"

/* wait for at least 2 vsyncs for lowest refresh rate (24hz) */
#define VSYNC_TIMEOUT_US 100000

/* Poll time to do recovery during active region */
#define POLL_TIME_USEC_FOR_LN_CNT 500

/* Filter out input events for 1 vsync time after receiving an input event*/
#define INPUT_EVENT_HANDLER_DELAY_USECS 16000

enum {
	MDP_INTF_INTR_PROG_LINE,
	MDP_INTF_INTR_MAX,
};

struct intr_callback {
	void (*func)(void *);
	void *arg;
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

struct mdss_mdp_video_ctx {
	struct mdss_mdp_ctl *ctl;
	u32 intf_num;
	char __iomem *base;
	u32 intf_type;
	u8 ref_cnt;

	u8 timegen_en;
	bool polling_en;
	u32 poll_cnt;
	struct completion vsync_comp;
	int wait_pending;

	atomic_t vsync_ref;
	spinlock_t vsync_lock;
	spinlock_t dfps_lock;
	struct mutex vsync_mtx;
	struct list_head vsync_handlers;
	struct mdss_intf_recovery intf_recovery;
	struct mdss_intf_recovery intf_mdp_callback;
	struct mdss_intf_ulp_clamp intf_clamp_handler;
	struct work_struct early_wakeup_dfps_work;

	atomic_t lineptr_ref;
	spinlock_t lineptr_lock;
	struct mutex lineptr_mtx;
	struct list_head lineptr_handlers;

	struct intf_timing_params itp;
	bool lineptr_enabled;
	u32 prev_wr_ptr_irq;

	struct intr_callback mdp_intf_intr_cb[MDP_INTF_INTR_MAX];
	u32 intf_irq_mask;
	spinlock_t mdss_mdp_video_lock;
	spinlock_t mdss_mdp_intf_intr_lock;
};

static void mdss_mdp_fetch_start_config(struct mdss_mdp_video_ctx *ctx,
		struct mdss_mdp_ctl *ctl);

static void mdss_mdp_fetch_end_config(struct mdss_mdp_video_ctx *ctx,
		struct mdss_mdp_ctl *ctl);

static void mdss_mdp_video_timegen_flush(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_video_ctx *sctx);

static void early_wakeup_dfps_update_work(struct work_struct *work);

static int mdss_mdp_video_avr_ctrl(struct mdss_mdp_ctl *ctl, bool enable);

static inline void mdp_video_write(struct mdss_mdp_video_ctx *ctx,
				   u32 reg, u32 val)
{
	writel_relaxed(val, ctx->base + reg);
}

static inline u32 mdp_video_read(struct mdss_mdp_video_ctx *ctx,
				   u32 reg)
{
	return readl_relaxed(ctx->base + reg);
}

static inline u32 mdss_mdp_video_line_count(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_video_ctx *ctx;
	u32 line_cnt = 0;
	if (!ctl || !ctl->intf_ctx[MASTER_CTX])
		goto line_count_exit;
	ctx = ctl->intf_ctx[MASTER_CTX];
	line_cnt = mdp_video_read(ctx, MDSS_MDP_REG_INTF_LINE_COUNT);
line_count_exit:
	return line_cnt;
}

static int mdss_mdp_intf_intr2index(u32 intr_type)
{
	int index = -1;

	switch (intr_type) {
	case MDSS_MDP_INTF_IRQ_PROG_LINE:
		index = MDP_INTF_INTR_PROG_LINE;
		break;
	}
	return index;
}

void *mdss_mdp_intf_get_ctx_base(struct mdss_mdp_ctl *ctl, int intf_num)
{
	struct mdss_mdp_video_ctx *head;
	int i = 0;

	if (!ctl)
		return NULL;

	head = ctl->mdata->video_intf;
	for (i = 0; i < ctl->mdata->nintf; i++) {
		if (head[i].intf_num == intf_num)
			return (void *)head[i].base;
	}

	return NULL;
}

int mdss_mdp_set_intf_intr_callback(struct mdss_mdp_video_ctx *ctx,
		u32 intr_type, void (*fnc_ptr)(void *), void *arg)
{
	unsigned long flags;
	int index;

	index = mdss_mdp_intf_intr2index(intr_type);
	if (index < 0) {
		pr_warn("invalid intr type=%u\n", intr_type);
		return -EINVAL;
	}

	spin_lock_irqsave(&ctx->mdss_mdp_intf_intr_lock, flags);
	WARN(ctx->mdp_intf_intr_cb[index].func && fnc_ptr,
		"replacing current intr callback for ndx=%d\n", index);
	ctx->mdp_intf_intr_cb[index].func = fnc_ptr;
	ctx->mdp_intf_intr_cb[index].arg = arg;
	spin_unlock_irqrestore(&ctx->mdss_mdp_intf_intr_lock, flags);

	return 0;
}

static inline void mdss_mdp_intf_intr_done(struct mdss_mdp_video_ctx *ctx,
	int index)
{
	void (*fnc)(void *);
	void *arg;

	spin_lock(&ctx->mdss_mdp_intf_intr_lock);
	fnc = ctx->mdp_intf_intr_cb[index].func;
	arg = ctx->mdp_intf_intr_cb[index].arg;
	spin_unlock(&ctx->mdss_mdp_intf_intr_lock);
	if (fnc)
		fnc(arg);
}

/*
 * mdss_mdp_video_isr() - ISR handler for video mode interfaces
 *
 * @ptr: pointer to all the video ctx
 * @count: number of interfaces which should match ctx
 *
 * The video isr is meant to handle all the interrupts in video interface,
 * in MDSS_MDP_REG_INTF_INTR_EN register. Currently it handles only the
 * programmable lineptr interrupt.
 */
void mdss_mdp_video_isr(void *ptr, u32 count)
{
	struct mdss_mdp_video_ctx *head = (struct mdss_mdp_video_ctx *) ptr;
	int i;

	for (i = 0; i < count; i++) {
		struct mdss_mdp_video_ctx *ctx = &head[i];
		u32 intr, mask;

		if (!ctx->intf_irq_mask)
			continue;

		intr = mdp_video_read(ctx, MDSS_MDP_REG_INTF_INTR_STATUS);
		mask = mdp_video_read(ctx, MDSS_MDP_REG_INTF_INTR_EN);
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_INTR_CLEAR, intr);

		pr_debug("%s: intf=%d intr=%x mask=%x\n", __func__,
				i, intr, mask);

		if (!(intr & mask))
			continue;

		if (intr & MDSS_MDP_INTF_INTR_PROG_LINE)
			mdss_mdp_intf_intr_done(ctx, MDP_INTF_INTR_PROG_LINE);
	}
}

static int mdss_mdp_video_intf_irq_enable(struct mdss_mdp_ctl *ctl,
		u32 intr_type)
{
	struct mdss_mdp_video_ctx *ctx;
	unsigned long irq_flags;
	int ret = 0;
	u32 irq;

	if (!ctl || !ctl->intf_ctx[MASTER_CTX])
		return -ENODEV;

	ctx = ctl->intf_ctx[MASTER_CTX];

	irq = 1 << intr_type;

	spin_lock_irqsave(&ctx->mdss_mdp_video_lock, irq_flags);
	if (ctx->intf_irq_mask & irq) {
		pr_warn("MDSS MDP Intf IRQ-0x%x is already set, mask=%x\n",
				irq, ctx->intf_irq_mask);
		ret = -EBUSY;
	} else {
		pr_debug("MDSS MDP Intf IRQ mask old=%x new=%x\n",
				ctx->intf_irq_mask, irq);
		ctx->intf_irq_mask |= irq;
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_INTR_CLEAR, irq);
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_INTR_EN,
				ctx->intf_irq_mask);
		ctl->mdata->mdp_intf_irq_mask |=
				(1 << (ctx->intf_num - MDSS_MDP_INTF0));
		mdss_mdp_enable_hw_irq(ctl->mdata);
	}
	spin_unlock_irqrestore(&ctx->mdss_mdp_video_lock, irq_flags);

	return ret;
}

void mdss_mdp_video_intf_irq_disable(struct mdss_mdp_ctl *ctl, u32 intr_type)
{
	struct mdss_mdp_video_ctx *ctx;
	unsigned long irq_flags;
	u32 irq;

	if (!ctl || !ctl->intf_ctx[MASTER_CTX])
		return;

	ctx = ctl->intf_ctx[MASTER_CTX];

	irq = 1 << intr_type;

	spin_lock_irqsave(&ctx->mdss_mdp_video_lock, irq_flags);
	if (!(ctx->intf_irq_mask & irq)) {
		pr_warn("MDSS MDP Intf IRQ-%x is NOT set, mask=%x\n",
				irq, ctx->intf_irq_mask);
	} else {
		ctx->intf_irq_mask &= ~irq;
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_INTR_CLEAR, irq);
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_INTR_EN,
				ctx->intf_irq_mask);
		if (ctx->intf_irq_mask == 0) {
			ctl->mdata->mdp_intf_irq_mask &=
				~(1 << (ctx->intf_num - MDSS_MDP_INTF0));
			mdss_mdp_disable_hw_irq(ctl->mdata);
		}
	}
	spin_unlock_irqrestore(&ctx->mdss_mdp_video_lock, irq_flags);
}

int mdss_mdp_video_addr_setup(struct mdss_data_type *mdata,
				u32 *offsets,  u32 count)
{
	struct mdss_mdp_video_ctx *head;
	u32 i;

	head = devm_kzalloc(&mdata->pdev->dev,
			sizeof(struct mdss_mdp_video_ctx) * count, GFP_KERNEL);
	if (!head)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		head[i].base = mdata->mdss_io.base + offsets[i];
		pr_debug("adding Video Intf #%d offset=0x%x virt=%pK\n", i,
				offsets[i], head[i].base);
		head[i].ref_cnt = 0;
		head[i].intf_num = i + MDSS_MDP_INTF0;
		INIT_LIST_HEAD(&head[i].vsync_handlers);
		INIT_LIST_HEAD(&head[i].lineptr_handlers);
	}

	mdata->video_intf = head;
	mdata->nintf = count;
	return 0;
}

static int mdss_mdp_video_intf_clamp_ctrl(void *data, int intf_num, bool enable)
{
	struct mdss_mdp_video_ctx *ctx = data;

	if (!data) {
		pr_err("%s: invalid ctl\n", __func__);
		return -EINVAL;
	}

	if (intf_num != ctx->intf_num) {
		pr_err("%s: invalid intf num\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: ctx intf num = %d, enable = %d\n",
				__func__, ctx->intf_num, enable);

	mdp_video_write(ctx, MDSS_MDP_REG_DSI_ULP_CLAMP_VALUE, enable);
	wmb(); /* ensure clamp is enabled */

	return 0;
}

static int mdss_mdp_video_intf_recovery(void *data, int event)
{
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_ctl *ctl = data;
	struct mdss_panel_info *pinfo;
	u32 line_cnt, min_ln_cnt, active_lns_cnt;
	u64 clk_rate;
	u32 clk_period, time_of_line;
	u32 delay;

	if (!data) {
		pr_err("%s: invalid ctl\n", __func__);
		return -EINVAL;
	}

	/*
	 * Currently, only intf_fifo_overflow is
	 * supported for recovery sequence for video
	 * mode DSI interface
	 */
	if (event != MDP_INTF_DSI_VIDEO_FIFO_OVERFLOW) {
		pr_warn("%s: unsupported recovery event:%d\n",
					__func__, event);
		return -EPERM;
	}

	ctx = ctl->intf_ctx[MASTER_CTX];
	pr_debug("%s: ctl num = %d, event = %d\n",
				__func__, ctl->num, event);

	pinfo = &ctl->panel_data->panel_info;
	clk_rate = ((ctl->intf_type == MDSS_INTF_DSI) ?
			pinfo->mipi.dsi_pclk_rate :
			pinfo->clk_rate);

	clk_rate = DIV_ROUND_UP_ULL(clk_rate, 1000); /* in kHz */
	if (!clk_rate) {
		pr_err("Unable to get proper clk_rate\n");
		return -EINVAL;
	}
	/*
	 * calculate clk_period as pico second to maintain good
	 * accuracy with high pclk rate and this number is in 17 bit
	 * range.
	 */
	clk_period = DIV_ROUND_UP_ULL(1000000000, clk_rate);
	if (!clk_period) {
		pr_err("Unable to calculate clock period\n");
		return -EINVAL;
	}
	min_ln_cnt = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width;
	active_lns_cnt = pinfo->yres;
	time_of_line = (pinfo->lcdc.h_back_porch +
		 pinfo->lcdc.h_front_porch +
		 pinfo->lcdc.h_pulse_width +
		 pinfo->xres) * clk_period;

	/* delay in micro seconds */
	delay = (time_of_line * (min_ln_cnt +
			pinfo->lcdc.v_front_porch)) / 1000000;

	/*
	 * Wait for max delay before
	 * polling to check active region
	 */
	if (delay > POLL_TIME_USEC_FOR_LN_CNT)
		delay = POLL_TIME_USEC_FOR_LN_CNT;

	mutex_lock(&ctl->offlock);
	while (1) {
		if (!ctl || ctl->mfd->shutdown_pending || !ctx ||
				!ctx->timegen_en) {
			pr_warn("Target is in suspend or shutdown pending\n");
			mutex_unlock(&ctl->offlock);
			return -EPERM;
		}

		line_cnt = mdss_mdp_video_line_count(ctl);

		if ((line_cnt >= min_ln_cnt) && (line_cnt <
			(active_lns_cnt + min_ln_cnt))) {
			pr_debug("%s, Needed lines left line_cnt=%d\n",
						__func__, line_cnt);
			mutex_unlock(&ctl->offlock);
			return 0;
		} else {
			pr_warn("line count is less. line_cnt = %d\n",
								line_cnt);
			/* Add delay so that line count is in active region */
			udelay(delay);
		}
	}
}

static void mdss_mdp_video_avr_vtotal_setup(struct mdss_mdp_ctl *ctl,
					struct intf_timing_params *p,
					struct mdss_mdp_video_ctx *ctx)
{
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_ctl *sctl = NULL;
	struct mdss_mdp_video_ctx *sctx = NULL;

	if (test_bit(MDSS_CAPS_AVR_SUPPORTED, mdata->mdss_caps_map)) {
		struct mdss_panel_data *pdata = ctl->panel_data;
		struct mdss_panel_info *pinfo = &pdata->panel_info;
		u32 avr_vtotal = pinfo->saved_avr_vtotal;

		if (!pinfo->saved_avr_vtotal) {
			u32 hsync_period = p->hsync_pulse_width +
				p->h_back_porch + p->width + p->h_front_porch;
			u32 vsync_period = p->vsync_pulse_width +
				p->v_back_porch + p->height + p->v_front_porch;
			u32 min_fps = pinfo->min_fps;
			u32 default_fps = mdss_panel_get_framerate(pinfo);
			u32 diff_fps = abs(default_fps - min_fps);
			u32 vtotal = mdss_panel_get_vtotal(pinfo);
			int add_porches = mult_frac(vtotal, diff_fps, min_fps);
			u32 vsync_period_slow = vsync_period + add_porches;

			avr_vtotal = vsync_period_slow * hsync_period;
			pinfo->saved_avr_vtotal = avr_vtotal;
		}

		mdp_video_write(ctx, MDSS_MDP_REG_INTF_AVR_VTOTAL, avr_vtotal);

		/*
		 * Make sure config goes through
		 */
		wmb();

		sctl = mdss_mdp_get_split_ctl(ctl);
		if (sctl)
			sctx = (struct mdss_mdp_video_ctx *)
				sctl->intf_ctx[MASTER_CTX];
		mdss_mdp_video_timegen_flush(ctl, sctx);

		MDSS_XLOG(pinfo->min_fps, pinfo->default_fps, avr_vtotal);
	}
}

static int mdss_mdp_video_avr_trigger_setup(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_video_ctx *ctx = NULL;
	struct mdss_data_type *mdata = ctl->mdata;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctx->ref_cnt) {
		pr_err("invalid master ctx\n");
		return -EINVAL;
	}

	if (!ctl->is_master)
		return 0;

	if (ctl->avr_info.avr_enabled &&
		test_bit(MDSS_CAPS_AVR_SUPPORTED, mdata->mdss_caps_map))
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_AVR_TRIGGER, 1);

	return 0;
}

static void mdss_mdp_video_avr_ctrl_setup(struct mdss_mdp_video_ctx *ctx,
		struct mdss_mdp_ctl *ctl, bool is_master, bool enable)
{
	struct mdss_mdp_avr_info *avr_info = &ctl->avr_info;
	u32 avr_ctrl = 0;
	u32 avr_mode = 0;

	if (enable) {
		avr_ctrl = avr_info->avr_enabled;
		avr_mode = avr_info->avr_mode;
	}

	/* Enable avr_vsync_clear_en bit to clear avr in next vsync */
	if (avr_mode == MDSS_MDP_AVR_ONE_SHOT)
		avr_mode |= (1 << 8);

	if (is_master) {
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_AVR_CONTROL, avr_ctrl);

		/*
		 * When AVR is enabled, need to setup DSI Video mode control
		 */
		mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_AVR_MODE,
				(void *)(unsigned long) avr_ctrl,
				CTL_INTF_EVENT_FLAG_DEFAULT);
	}

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_AVR_MODE, avr_mode);

	pr_debug("intf:%d avr_mode:%x avr_ctrl:%x\n",
		ctx->intf_num, avr_mode, avr_ctrl);
}

static int mdss_mdp_video_timegen_setup(struct mdss_mdp_ctl *ctl,
					struct intf_timing_params *p,
					struct mdss_mdp_video_ctx *ctx)
{
	u32 hsync_period, vsync_period;
	u32 hsync_start_x, hsync_end_x, display_v_start, display_v_end;
	u32 active_h_start, active_h_end, active_v_start, active_v_end;
	u32 den_polarity, hsync_polarity, vsync_polarity;
	u32 display_hctl, active_hctl, hsync_ctl, polarity_ctl;
	struct mdss_data_type *mdata;

	mdata = ctl->mdata;
	hsync_period = p->hsync_pulse_width + p->h_back_porch +
			p->width + p->h_front_porch;
	vsync_period = p->vsync_pulse_width + p->v_back_porch +
			p->height + p->v_front_porch;

	MDSS_XLOG(p->vsync_pulse_width, p->v_back_porch,
			p->height, p->v_front_porch);

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
	MDSS_XLOG(hsync_period, vsync_period);

	/*
	 * If CDM is present Interface should have destination
	 * format set to RGB
	 */
	if (ctl->cdm) {
		u32 reg = mdp_video_read(ctx, MDSS_MDP_REG_INTF_CONFIG);

		reg &= ~BIT(18); /* CSC_DST_DATA_FORMAT = RGB */
		reg &= ~BIT(17); /* CSC_SRC_DATA_FROMAT = RGB */
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_CONFIG, reg);
	}
	return 0;
}

static void mdss_mdp_video_timegen_flush(struct mdss_mdp_ctl *ctl,
					struct mdss_mdp_video_ctx *sctx)
{
	u32 ctl_flush;
	struct mdss_data_type *mdata;

	mdata = ctl->mdata;
	ctl_flush = (BIT(31) >> (ctl->intf_num - MDSS_MDP_INTF0));
	if (sctx) {
		/* For 8939, sctx is always INTF2 and the flush bit is BIT 31 */
		if (mdata->mdp_rev == MDSS_MDP_HW_REV_108)
			ctl_flush |= BIT(31);
		else
			ctl_flush |= (BIT(31) >>
					(sctx->intf_num - MDSS_MDP_INTF0));
	}
	mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_FLUSH, ctl_flush);
	MDSS_XLOG(ctl->intf_num, sctx?sctx->intf_num:0xf00, ctl_flush);
}

static inline void video_vsync_irq_enable(struct mdss_mdp_ctl *ctl, bool clear)
{
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	mutex_lock(&ctx->vsync_mtx);
	if (atomic_inc_return(&ctx->vsync_ref) == 1)
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_INTF_VSYNC,
				ctl->intf_num);
	else if (clear)
		mdss_mdp_irq_clear(ctl->mdata, MDSS_MDP_IRQ_TYPE_INTF_VSYNC,
				ctl->intf_num);
	mutex_unlock(&ctx->vsync_mtx);
}

static inline void video_vsync_irq_disable(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	mutex_lock(&ctx->vsync_mtx);
	if (atomic_dec_return(&ctx->vsync_ref) == 0)
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_TYPE_INTF_VSYNC,
				ctl->intf_num);
	mutex_unlock(&ctx->vsync_mtx);
}

static int mdss_mdp_video_add_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_video_ctx *ctx;
	unsigned long flags;
	int ret = 0;
	bool irq_en = false;

	if (!handle || !(handle->vsync_handler)) {
		ret = -EINVAL;
		goto exit;
	}

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		ret = -ENODEV;
		goto exit;
	}

	MDSS_XLOG(ctl->num, ctl->vsync_cnt, handle->enabled);

	spin_lock_irqsave(&ctx->vsync_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->vsync_handlers);
		irq_en = true;
	}
	spin_unlock_irqrestore(&ctx->vsync_lock, flags);
	if (irq_en) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		video_vsync_irq_enable(ctl, false);
	}
exit:
	return ret;
}

static int mdss_mdp_video_remove_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_video_ctx *ctx;
	unsigned long flags;
	bool irq_dis = false;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	MDSS_XLOG(ctl->num, ctl->vsync_cnt, handle->enabled);

	spin_lock_irqsave(&ctx->vsync_lock, flags);
	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);
		irq_dis = true;
	}
	spin_unlock_irqrestore(&ctx->vsync_lock, flags);
	if (irq_dis) {
		video_vsync_irq_disable(ctl);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	}
	return 0;
}

static int mdss_mdp_video_add_lineptr_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_lineptr_handler *handle)
{
	struct mdss_mdp_video_ctx *ctx;
	unsigned long flags;
	int ret = 0;
	bool irq_en = false;

	if (!handle || !(handle->lineptr_handler)) {
		ret = -EINVAL;
		goto exit;
	}

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		ret = -ENODEV;
		goto exit;
	}

	spin_lock_irqsave(&ctx->lineptr_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->lineptr_handlers);
		irq_en = true;
	}
	spin_unlock_irqrestore(&ctx->lineptr_lock, flags);

	if (irq_en) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
		mutex_lock(&ctx->lineptr_mtx);
		if (atomic_inc_return(&ctx->lineptr_ref) == 1)
			mdss_mdp_video_intf_irq_enable(ctl,
				MDSS_MDP_INTF_IRQ_PROG_LINE);
		mutex_unlock(&ctx->lineptr_mtx);
	}
	ctx->lineptr_enabled = true;

exit:
	return ret;
}

static int mdss_mdp_video_remove_lineptr_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_lineptr_handler *handle)
{
	struct mdss_mdp_video_ctx *ctx;
	unsigned long flags;
	bool irq_dis = false;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctx->lineptr_enabled)
		return -EINVAL;

	spin_lock_irqsave(&ctx->lineptr_lock, flags);
	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);
		irq_dis = true;
	}
	spin_unlock_irqrestore(&ctx->lineptr_lock, flags);

	if (irq_dis) {
		mutex_lock(&ctx->lineptr_mtx);
		if (atomic_dec_return(&ctx->lineptr_ref) == 0)
			mdss_mdp_video_intf_irq_disable(ctl,
				MDSS_MDP_INTF_IRQ_PROG_LINE);
		mutex_unlock(&ctx->lineptr_mtx);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	}
	ctx->lineptr_enabled = false;
	ctx->prev_wr_ptr_irq = 0;

	return 0;
}

static int mdss_mdp_video_set_lineptr(struct mdss_mdp_ctl *ctl,
	u32 new_lineptr)
{
	struct mdss_mdp_video_ctx *ctx;
	u32 pixel_start, offset, hsync_period;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	if (0 == new_lineptr) {
		mdp_video_write(ctx,
			MDSS_MDP_REG_INTF_PROG_LINE_INTR_CONF, UINT_MAX);
	} else if (new_lineptr <= ctx->itp.yres) {
		hsync_period = ctx->itp.hsync_pulse_width
			+ ctx->itp.h_back_porch + ctx->itp.width
			+ ctx->itp.h_front_porch;

		offset = ((ctx->itp.vsync_pulse_width + ctx->itp.v_back_porch)
				* hsync_period) + ctx->itp.hsync_skew;

		/* convert from line to pixel */
		pixel_start = offset + (hsync_period * (new_lineptr - 1));
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_PROG_LINE_INTR_CONF,
			pixel_start);

		mdss_mdp_video_timegen_flush(ctl, ctx);
	} else {
		pr_err("invalid new lineptr_value: new=%d yres=%d\n",
				new_lineptr, ctx->itp.yres);
		return -EINVAL;
	}

	return 0;
}

static int mdss_mdp_video_lineptr_ctrl(struct mdss_mdp_ctl *ctl, bool enable)
{
	struct mdss_mdp_pp_tear_check *te;
	struct mdss_mdp_video_ctx *ctx;
	int rc = 0;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctl->is_master)
		return -EINVAL;

	te = &ctl->panel_data->panel_info.te;
	pr_debug("%pS->%s: ctl=%d en=%d, prev_lineptr=%d, lineptr=%d\n",
			__builtin_return_address(0), __func__, ctl->num,
			enable, ctx->prev_wr_ptr_irq, te->wr_ptr_irq);

	if (enable) {
		/* update reg only if the value has changed */
		if (ctx->prev_wr_ptr_irq != te->wr_ptr_irq) {
			if (mdss_mdp_video_set_lineptr(ctl,
						te->wr_ptr_irq) < 0) {
				/* invalid new value, so restore the previous */
				te->wr_ptr_irq = ctx->prev_wr_ptr_irq;
				goto end;
			}
			ctx->prev_wr_ptr_irq = te->wr_ptr_irq;
		}

		/*
		 * add handler only when lineptr is not enabled
		 * and wr ptr is non zero
		 */
		if (!ctx->lineptr_enabled && te->wr_ptr_irq)
			rc = mdss_mdp_video_add_lineptr_handler(ctl,
				&ctl->lineptr_handler);
		/* Disable handler when the value is zero */
		else if (ctx->lineptr_enabled && !te->wr_ptr_irq)
			rc = mdss_mdp_video_remove_lineptr_handler(ctl,
				&ctl->lineptr_handler);
	} else {
		if (ctx->lineptr_enabled)
			rc = mdss_mdp_video_remove_lineptr_handler(ctl,
				&ctl->lineptr_handler);
	}

end:
	return rc;
}

void mdss_mdp_turn_off_time_engine(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_video_ctx *ctx, u32 sleep_time)
{
	struct mdss_mdp_ctl *sctl;

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 0);
	/* wait for at least one VSYNC for proper TG OFF */
	msleep(sleep_time);

	mdss_iommu_ctrl(0);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
	ctx->timegen_en = false;

	mdss_mdp_irq_disable(MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN, ctl->intf_num);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN,
			sctl->intf_num);
}

static int mdss_mdp_video_ctx_stop(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_info *pinfo, struct mdss_mdp_video_ctx *ctx)
{
	int rc = 0;
	u32 frame_rate = 0;

	mutex_lock(&ctl->offlock);
	if (ctx->timegen_en) {
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK, NULL,
			CTL_INTF_EVENT_FLAG_DEFAULT);
		if (rc == -EBUSY) {
			pr_debug("intf #%d busy don't turn off\n",
				 ctl->intf_num);
			goto end;
		}
		WARN(rc, "intf %d blank error (%d)\n", ctl->intf_num, rc);

		frame_rate = mdss_panel_get_framerate(pinfo);
		if (!(frame_rate >= 24 && frame_rate <= 240))
			frame_rate = 24;

		frame_rate = (1000/frame_rate) + 1;
		mdss_mdp_turn_off_time_engine(ctl, ctx, frame_rate);

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF, NULL,
			CTL_INTF_EVENT_FLAG_DEFAULT);
		WARN(rc, "intf %d timegen off error (%d)\n", ctl->intf_num, rc);

		mdss_bus_bandwidth_ctrl(false);
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_INTF_VSYNC,
		ctx->intf_num, NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN,
		ctx->intf_num, NULL, NULL);
	mdss_mdp_set_intf_intr_callback(ctx, MDSS_MDP_INTF_IRQ_PROG_LINE,
		NULL, NULL);

	ctx->ref_cnt--;
end:
	mutex_unlock(&ctl->offlock);
	return rc;
}

static int mdss_mdp_video_intfs_stop(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_data *pdata, int inum)
{
	struct mdss_data_type *mdata;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_video_ctx *ctx, *sctx = NULL;
	struct mdss_mdp_vsync_handler *tmp, *handle;
	int ret = 0;

	if (pdata == NULL)
		return 0;

	mdata = ctl->mdata;
	pinfo = &pdata->panel_info;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx->ref_cnt) {
		pr_err("Intf %d not in use\n", (inum + MDSS_MDP_INTF0));
		return -ENODEV;
	}
	pr_debug("stop ctl=%d video Intf #%d base=%pK", ctl->num, ctx->intf_num,
			ctx->base);

	ret = mdss_mdp_video_ctx_stop(ctl, pinfo, ctx);
	if (ret) {
		pr_err("mdss_mdp_video_ctx_stop failed for intf: %d",
				ctx->intf_num);
		return -EPERM;
	}

	if (is_pingpong_split(ctl->mfd)) {
		pinfo = &pdata->next->panel_info;

		sctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[SLAVE_CTX];
		if (!sctx->ref_cnt) {
			pr_err("Intf %d not in use\n", (inum + MDSS_MDP_INTF0));
			return -ENODEV;
		}
		pr_debug("stop ctl=%d video Intf #%d base=%pK", ctl->num,
				sctx->intf_num, sctx->base);

		ret = mdss_mdp_video_ctx_stop(ctl, pinfo, sctx);
		if (ret) {
			pr_err("mdss_mdp_video_ctx_stop failed for intf: %d",
					sctx->intf_num);
			return -EPERM;
		}
	}

	list_for_each_entry_safe(handle, tmp, &ctx->vsync_handlers, list)
		mdss_mdp_video_remove_vsync_handler(ctl, handle);

	if (mdss_mdp_is_lineptr_supported(ctl))
		mdss_mdp_video_lineptr_ctrl(ctl, false);

	return 0;
}


static int mdss_mdp_video_stop(struct mdss_mdp_ctl *ctl, int panel_power_state)
{
	int intfs_num, ret = 0;

	intfs_num = ctl->intf_num - MDSS_MDP_INTF0;
	ret = mdss_mdp_video_intfs_stop(ctl, ctl->panel_data, intfs_num);
	if (IS_ERR_VALUE(ret)) {
		pr_err("unable to stop video interface: %d\n", ret);
		return ret;
	}

	MDSS_XLOG(ctl->num, ctl->vsync_cnt);

	mdss_mdp_ctl_reset(ctl, false);
	ctl->intf_ctx[MASTER_CTX] = NULL;

	if (ctl->cdm) {
		mdss_mdp_cdm_destroy(ctl->cdm);
		ctl->cdm = NULL;
	}
	return 0;
}

static void mdss_mdp_video_vsync_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_vsync_handler *tmp;
	ktime_t vsync_time;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	vsync_time = ktime_get();
	ctl->vsync_cnt++;

	MDSS_XLOG(ctl->num, ctl->vsync_cnt, ctl->vsync_cnt);

	pr_debug("intr ctl=%d vsync cnt=%u vsync_time=%d\n",
		 ctl->num, ctl->vsync_cnt, (int)ktime_to_ms(vsync_time));

	ctx->polling_en = false;
	complete_all(&ctx->vsync_comp);
	spin_lock(&ctx->vsync_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		tmp->vsync_handler(ctl, vsync_time);
	}
	spin_unlock(&ctx->vsync_lock);
}

static void mdss_mdp_video_lineptr_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_lineptr_handler *tmp;
	ktime_t lineptr_time;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	lineptr_time = ktime_get();
	pr_debug("intr lineptr_time=%lld\n", ktime_to_ms(lineptr_time));

	spin_lock(&ctx->lineptr_lock);
	list_for_each_entry(tmp, &ctx->lineptr_handlers, list) {
		tmp->lineptr_handler(ctl, lineptr_time);
	}
	spin_unlock(&ctx->lineptr_lock);
}

static int mdss_mdp_video_pollwait(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	u32 mask, status;
	int rc;

	mask = mdss_mdp_get_irq_mask(MDSS_MDP_IRQ_TYPE_INTF_VSYNC,
			ctl->intf_num);

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
			unsigned long flags;
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

static int mdss_mdp_video_wait4comp(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_video_ctx *ctx;
	int rc;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	WARN(!ctx->wait_pending, "waiting without commit! ctl=%d", ctl->num);

	if (ctx->polling_en) {
		rc = mdss_mdp_video_pollwait(ctl);
	} else {
		mutex_unlock(&ctl->lock);
		rc = wait_for_completion_timeout(&ctx->vsync_comp,
				usecs_to_jiffies(VSYNC_TIMEOUT_US));
		mutex_lock(&ctl->lock);
		if (rc == 0) {
			pr_warn("vsync wait timeout %d, fallback to poll mode\n",
					ctl->num);
			ctx->polling_en++;
			rc = mdss_mdp_video_pollwait(ctl);
		} else {
			rc = 0;
		}
	}
	mdss_mdp_ctl_notify(ctl,
			rc ? MDP_NOTIFY_FRAME_TIMEOUT : MDP_NOTIFY_FRAME_DONE);

	if (ctx->wait_pending) {
		ctx->wait_pending = 0;
		video_vsync_irq_disable(ctl);
	}

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

static void mdss_mdp_video_underrun_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	if (unlikely(!ctl))
		return;

	ctl->underrun_cnt++;
	MDSS_XLOG(ctl->num, ctl->underrun_cnt);
	trace_mdp_video_underrun_done(ctl->num, ctl->underrun_cnt);
	pr_debug("display underrun detected for ctl=%d count=%d\n", ctl->num,
			ctl->underrun_cnt);

	if (!test_bit(MDSS_CAPS_3D_MUX_UNDERRUN_RECOVERY_SUPPORTED,
		ctl->mdata->mdss_caps_map) &&
		(ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE))
		schedule_work(&ctl->recover_work);
}

/**
 * mdss_mdp_video_hfp_fps_update() - configure mdp with new fps.
 * @ctx: pointer to the master context.
 * @pdata: panel information data.
 *
 * This function configures the hardware to modify the fps.
 * within mdp for the hfp method.
 * Function assumes that timings for the new fps configuration
 * are already updated in the panel data passed as parameter.
 *
 * Return: 0 - succeed, otherwise - fail
 */
static int mdss_mdp_video_hfp_fps_update(struct mdss_mdp_video_ctx *ctx,
					struct mdss_panel_data *pdata)
{
	u32 hsync_period, vsync_period;
	u32 hsync_start_x, hsync_end_x, display_v_start, display_v_end;
	u32 display_hctl, hsync_ctl;
	struct mdss_panel_info *pinfo = &pdata->panel_info;

	hsync_period = mdss_panel_get_htotal(pinfo, true);
	vsync_period = mdss_panel_get_vtotal(pinfo);

	display_v_start = ((pinfo->lcdc.v_pulse_width +
			pinfo->lcdc.v_back_porch) * hsync_period) +
					pinfo->lcdc.hsync_skew;
	display_v_end = ((vsync_period - pinfo->lcdc.v_front_porch) *
				hsync_period) + pinfo->lcdc.hsync_skew - 1;

	hsync_start_x = pinfo->lcdc.h_back_porch + pinfo->lcdc.h_pulse_width;
	hsync_end_x = hsync_period - pinfo->lcdc.h_front_porch - 1;

	hsync_ctl = (hsync_period << 16) | pinfo->lcdc.h_pulse_width;
	display_hctl = (hsync_end_x << 16) | hsync_start_x;

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_HSYNC_CTL, hsync_ctl);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
				vsync_period * hsync_period);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PULSE_WIDTH_F0,
			pinfo->lcdc.v_pulse_width * hsync_period);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_DISPLAY_HCTL, display_hctl);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_DISPLAY_V_START_F0,
						display_v_start);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_DISPLAY_V_END_F0, display_v_end);
	MDSS_XLOG(ctx->intf_num, hsync_ctl, vsync_period, hsync_period);

	return 0;
}

/**
 * mdss_mdp_video_vfp_fps_update() - configure mdp with new fps.
 * @ctx: pointer to the master context.
 * @pdata: panel information data.
 *
 * This function configures the hardware to modify the fps.
 * within mdp for the vfp method.
 * Function assumes that timings for the new fps configuration
 * are already updated in the panel data passed as parameter.
 *
 * Return: 0 - succeed, otherwise - fail
 */
static int mdss_mdp_video_vfp_fps_update(struct mdss_mdp_video_ctx *ctx,
				 struct mdss_panel_data *pdata)
{
	u32 current_vsync_period_f0, new_vsync_period_f0;
	int vsync_period, hsync_period;

	/*
	 * Change in the blanking times are already in the
	 * panel info, so just get the vtotal and htotal expected
	 * for this panel to configure those in hw.
	 */
	vsync_period = mdss_panel_get_vtotal(&pdata->panel_info);
	hsync_period = mdss_panel_get_htotal(&pdata->panel_info, true);

	current_vsync_period_f0 = mdp_video_read(ctx,
		MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0);
	new_vsync_period_f0 = (vsync_period * hsync_period);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
			new_vsync_period_f0);

	pr_debug("if:%d vtotal:%d htotal:%d f0:0x%x nw_f0:0x%x\n",
		ctx->intf_num, vsync_period, hsync_period,
		current_vsync_period_f0, new_vsync_period_f0);

	MDSS_XLOG(ctx->intf_num, current_vsync_period_f0,
		hsync_period, vsync_period, new_vsync_period_f0);

	return 0;
}

static int mdss_mdp_video_fps_update(struct mdss_mdp_video_ctx *ctx,
				 struct mdss_panel_data *pdata, int new_fps)
{
	int rc;

	if (pdata->panel_info.dfps_update ==
				DFPS_IMMEDIATE_PORCH_UPDATE_MODE_VFP)
		rc = mdss_mdp_video_vfp_fps_update(ctx, pdata);
	else
		rc = mdss_mdp_video_hfp_fps_update(ctx, pdata);

	return rc;
}

static int mdss_mdp_video_dfps_wait4vsync(struct mdss_mdp_ctl *ctl)
{
	int rc = 0;
	struct mdss_mdp_video_ctx *ctx;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	video_vsync_irq_enable(ctl, true);
	reinit_completion(&ctx->vsync_comp);
	rc = wait_for_completion_timeout(&ctx->vsync_comp,
		usecs_to_jiffies(VSYNC_TIMEOUT_US));

	if (rc <= 0) {
		pr_warn("vsync timeout %d fallback to poll mode\n",
			ctl->num);
		rc = mdss_mdp_video_pollwait(ctl);
		if (rc) {
			pr_err("error polling for vsync\n");
			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl", "dsi0_phy",
				"dsi1_ctrl", "dsi1_phy", "vbif", "dbg_bus",
				"vbif_dbg_bus", "panic");
		}
	} else {
		rc = 0;
	}
	video_vsync_irq_disable(ctl);

	return rc;
}

static int mdss_mdp_video_dfps_check_line_cnt(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_data *pdata;
	u32 line_cnt;
	pdata = ctl->panel_data;
	if (pdata == NULL) {
		pr_err("%s: Invalid panel data\n", __func__);
		return -EINVAL;
	}

	line_cnt = mdss_mdp_video_line_count(ctl);
	if (line_cnt >=	pdata->panel_info.yres/2) {
		pr_debug("Too few lines left line_cnt=%d yres/2=%d\n",
			line_cnt,
			pdata->panel_info.yres/2);
		return -EPERM;
	}
	return 0;
}

/**
 * mdss_mdp_video_config_fps() - modify the fps.
 * @ctl: pointer to the master controller.
 * @new_fps: new fps to be set.
 *
 * This function configures the hardware to modify the fps.
 * Note that this function will flush the DSI and MDP
 * to reconfigure the fps in VFP and HFP methods.
 * Given above statement, is callers responsibility to call
 * this function at the beginning of the frame, so it can be
 * guaranteed that flush of both (DSI and MDP) happen within
 * the same frame.
 *
 * Return: 0 - succeed, otherwise - fail
 */
static int mdss_mdp_video_config_fps(struct mdss_mdp_ctl *ctl, int new_fps)
{
	struct mdss_mdp_video_ctx *ctx, *sctx = NULL;
	struct mdss_panel_data *pdata;
	int rc = 0;
	struct mdss_data_type *mdata = ctl->mdata;
	struct mdss_mdp_ctl *sctl = NULL;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctx->timegen_en || !ctx->ref_cnt) {
		pr_err("invalid ctx or interface is powered off\n");
		return -EINVAL;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl) {
		sctx = (struct mdss_mdp_video_ctx *) sctl->intf_ctx[MASTER_CTX];
		if (!sctx) {
			pr_err("invalid ctx\n");
			return -ENODEV;
		}
	} else if (is_pingpong_split(ctl->mfd)) {
		sctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[SLAVE_CTX];
		if (!sctx || !sctx->ref_cnt) {
			pr_err("invalid sctx or interface is powered off\n");
			return -EINVAL;
		}
	}

	/* add HW recommended delay to handle panel_vsync */
	udelay(2000);
	mutex_lock(&ctl->offlock);
	pdata = ctl->panel_data;
	if (pdata == NULL) {
		pr_err("%s: Invalid panel data\n", __func__);
		rc = -EINVAL;
		goto end;
	}

	pr_debug("ctl:%d dfps_update:%d fps:%d\n",
		ctl->num, pdata->panel_info.dfps_update, new_fps);
	MDSS_XLOG(ctl->num, pdata->panel_info.dfps_update,
		new_fps, XLOG_FUNC_ENTRY);

	if (pdata->panel_info.dfps_update
			!= DFPS_SUSPEND_RESUME_MODE) {
		if (pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
			if (!ctx->timegen_en) {
				pr_err("TG is OFF. DFPS mode invalid\n");
				rc = -EINVAL;
				goto end;
			}
			rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_PANEL_UPDATE_FPS,
					(void *) (unsigned long) new_fps,
					CTL_INTF_EVENT_FLAG_DEFAULT);

			WARN(rc, "intf %d panel fps update error (%d)\n",
							ctl->intf_num, rc);
		} else if (pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_PORCH_UPDATE_MODE_VFP ||
				pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_PORCH_UPDATE_MODE_HFP ||
				pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_MULTI_UPDATE_MODE_CLK_HFP ||
				pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_MULTI_MODE_HFP_CALC_CLK) {
			unsigned long flags;
			if (!ctx->timegen_en) {
				pr_err("TG is OFF. DFPS mode invalid\n");
				rc = -EINVAL;
				goto end;
			}

			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			/*
			 * Need to disable AVR during DFPS update period.
			 * Next commit will restore the AVR settings.
			 */
			if (test_bit(MDSS_CAPS_AVR_SUPPORTED,
						mdata->mdss_caps_map) &&
					ctl->avr_info.avr_enabled) {
				mdss_mdp_video_avr_ctrl(ctl, false);
				rc = mdss_mdp_video_dfps_wait4vsync(ctl);
				if (rc < 0)
					pr_err("Error in dfps_wait: %d\n", rc);
			}

			spin_lock_irqsave(&ctx->dfps_lock, flags);

			if (mdata->mdp_rev < MDSS_MDP_HW_REV_105) {
				rc = mdss_mdp_video_dfps_check_line_cnt(ctl);
				if (rc < 0)
					goto exit_dfps;
			}

			rc = mdss_mdp_video_fps_update(ctx, pdata, new_fps);
			if (rc < 0) {
				pr_err("%s: Error during DFPS: %d\n", __func__,
					new_fps);
				goto exit_dfps;
			}
			if (sctx) {
				rc = mdss_mdp_video_fps_update(sctx,
							pdata->next, new_fps);
				if (rc < 0) {
					pr_err("%s: DFPS error fps:%d\n",
						__func__, new_fps);
					goto exit_dfps;
				}
			}
			rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_PANEL_UPDATE_FPS,
					(void *) (unsigned long) new_fps,
					CTL_INTF_EVENT_FLAG_DEFAULT);
			WARN(rc, "intf %d panel fps update error (%d)\n",
							ctl->intf_num, rc);

			rc = 0;
			mdss_mdp_fetch_start_config(ctx, ctl);
			if (sctx)
				mdss_mdp_fetch_start_config(sctx, ctl);

			if (test_bit(MDSS_QOS_VBLANK_PANIC_CTRL,
					mdata->mdss_qos_map)) {
				mdss_mdp_fetch_end_config(ctx, ctl);
				if (sctx)
					mdss_mdp_fetch_end_config(sctx, ctl);
			}

			/*
			 * Make sure controller setting committed
			 */
			wmb();

			/*
			 * MDP INTF registers support DB on targets
			 * starting from MDP v1.5.
			 */
			if (mdata->mdp_rev >= MDSS_MDP_HW_REV_105)
				mdss_mdp_video_timegen_flush(ctl, sctx);

exit_dfps:
			spin_unlock_irqrestore(&ctx->dfps_lock, flags);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

			/*
			 * Wait for one vsync to make sure these changes
			 * are applied as part of one single frame and
			 * no mixer changes happen at the same time.
			 * A potential optimization would be not to wait
			 * here, but next mixer programming would need
			 * to wait before programming the flush bits.
			 */
			if (!rc) {
				rc = mdss_mdp_video_dfps_wait4vsync(ctl);
				if (rc < 0)
					pr_err("Error in dfps_wait: %d\n", rc);
			}
			/* add HW recommended delay to handle panel_vsync */
			udelay(2000);
			/* Disable interface timing double buffer */
			rc = mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_DSI_TIMING_DB_CTRL,
				(void *) (unsigned long) 0,
				CTL_INTF_EVENT_FLAG_DEFAULT);
		} else {
			pr_err("intf %d panel, unknown FPS mode\n",
							ctl->intf_num);
			rc = -EINVAL;
			goto end;
		}
	} else {
		rc = mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_PANEL_UPDATE_FPS,
				(void *) (unsigned long) new_fps,
				CTL_INTF_EVENT_FLAG_DEFAULT);
		WARN(rc, "intf %d panel fps update error (%d)\n",
						ctl->intf_num, rc);
	}

end:
	MDSS_XLOG(ctl->num, new_fps, XLOG_FUNC_EXIT);
	mutex_unlock(&ctl->offlock);
	return rc;
}

static int mdss_mdp_video_display(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_ctl *sctl;
	struct mdss_panel_data *pdata = ctl->panel_data;
	int rc;

	pr_debug("kickoff ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (!ctx->wait_pending) {
		ctx->wait_pending++;
		video_vsync_irq_enable(ctl, true);
		reinit_completion(&ctx->vsync_comp);
	} else {
		WARN(1, "commit without wait! ctl=%d", ctl->num);
	}

	MDSS_XLOG(ctl->num, ctl->underrun_cnt);

	if (!ctx->timegen_en) {
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_LINK_READY, NULL,
			CTL_INTF_EVENT_FLAG_DEFAULT);
		if (rc) {
			pr_warn("intf #%d link ready error (%d)\n",
					ctl->intf_num, rc);
			video_vsync_irq_disable(ctl);
			ctx->wait_pending = 0;
			return rc;
		}

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_UNBLANK, NULL,
			CTL_INTF_EVENT_FLAG_DEFAULT);
		WARN(rc, "intf %d unblank error (%d)\n", ctl->intf_num, rc);

		pr_debug("enabling timing gen for intf=%d\n", ctl->intf_num);

		if (pdata->panel_info.cont_splash_enabled &&
			!ctl->mfd->splash_info.splash_logo_enabled) {
			rc = wait_for_completion_timeout(&ctx->vsync_comp,
					usecs_to_jiffies(VSYNC_TIMEOUT_US));
		}

		rc = mdss_iommu_ctrl(1);
		if (IS_ERR_VALUE(rc)) {
			pr_err("IOMMU attach failed\n");
			return rc;
		}

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

		mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN,
				ctl->intf_num);
		sctl = mdss_mdp_get_split_ctl(ctl);
		if (sctl)
			mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN,
				sctl->intf_num);

		mdss_bus_bandwidth_ctrl(true);

		mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 1);
		wmb();

		rc = wait_for_completion_timeout(&ctx->vsync_comp,
				usecs_to_jiffies(VSYNC_TIMEOUT_US));
		WARN(rc == 0, "timeout (%d) enabling timegen on ctl=%d\n",
				rc, ctl->num);

		ctx->timegen_en = true;
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_ON, NULL,
			CTL_INTF_EVENT_FLAG_DEFAULT);
		WARN(rc, "intf %d panel on error (%d)\n", ctl->intf_num, rc);
		mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_POST_PANEL_ON, NULL,
			CTL_INTF_EVENT_FLAG_DEFAULT);
	}

	rc = mdss_mdp_video_avr_trigger_setup(ctl);
	if (rc) {
		pr_err("avr trigger setup failed\n");
		return rc;
	}

	if (mdss_mdp_is_lineptr_supported(ctl))
		mdss_mdp_video_lineptr_ctrl(ctl, true);

	return 0;
}

int mdss_mdp_video_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
	bool handoff)
{
	struct mdss_panel_data *pdata;
	int i, ret = 0, off;
	u32 data, flush;
	struct mdss_mdp_video_ctx *ctx, *sctx = NULL;
	struct mdss_mdp_ctl *sctl;

	if (!ctl) {
		pr_err("invalid ctl\n");
		return -ENODEV;
	}

	off = 0;
	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	pdata = ctl->panel_data;
	if (!pdata) {
		pr_err("invalid pdata\n");
		return -ENODEV;
	}

	pdata->panel_info.cont_splash_enabled = 0;
	sctl = mdss_mdp_get_split_ctl(ctl);

	if (sctl) {
		sctl->panel_data->panel_info.cont_splash_enabled = 0;
		sctx = (struct mdss_mdp_video_ctx *) sctl->intf_ctx[MASTER_CTX];
	} else if (ctl->panel_data->next && is_pingpong_split(ctl->mfd)) {
		ctl->panel_data->next->panel_info.cont_splash_enabled = 0;
		sctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[SLAVE_CTX];
	}

	if (!handoff) {
		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CONT_SPLASH_BEGIN,
				      NULL, CTL_INTF_EVENT_FLAG_DEFAULT);
		if (ret) {
			pr_err("%s: Failed to handle 'CONT_SPLASH_BEGIN' event\n"
				, __func__);
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
		mdss_mdp_video_timegen_flush(ctl, sctx);

		/* wait for 1 VSYNC for the pipe to be unstaged */
		msleep(20);

		ret = mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_CONT_SPLASH_FINISH, NULL,
			CTL_INTF_EVENT_FLAG_DEFAULT);
	}

	return ret;
}

static void mdss_mdp_disable_prefill(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;
	struct mdss_data_type *mdata = ctl->mdata;

	if ((pinfo->prg_fet + pinfo->lcdc.v_back_porch +
			pinfo->lcdc.v_pulse_width) > mdata->min_prefill_lines) {
		ctl->disable_prefill = true;
		pr_debug("disable prefill vbp:%d vpw:%d prg_fet:%d\n",
			pinfo->lcdc.v_back_porch, pinfo->lcdc.v_pulse_width,
			pinfo->prg_fet);
	}
}

static void mdss_mdp_fetch_end_config(struct mdss_mdp_video_ctx *ctx,
		struct mdss_mdp_ctl *ctl)
{
	int fetch_stop, h_total;
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;
	u32 lines_before_active = ctl->mdata->lines_before_active ? : 2;
	u32 vblank_lines = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width;
	u32 vblank_end_enable;

	if (vblank_lines <= lines_before_active) {
		pr_debug("cannot support fetch end vblank:%d lines:%d\n",
			vblank_lines, lines_before_active);
		return;
	}

	/* Fetch should always be stopped before the active start */
	h_total = mdss_panel_get_htotal(pinfo, true);
	fetch_stop = (vblank_lines - lines_before_active) * h_total;

	vblank_end_enable = mdp_video_read(ctx, MDSS_MDP_REG_INTF_CONFIG);
	vblank_end_enable |= BIT(22);

	pr_debug("ctl:%d fetch_stop:%d lines:%d\n",
		ctl->num, fetch_stop, lines_before_active);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_VBLANK_END_CONF, fetch_stop);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_CONFIG, vblank_end_enable);
	MDSS_XLOG(ctx->intf_num, fetch_stop, vblank_end_enable);
}

static void mdss_mdp_fetch_start_config(struct mdss_mdp_video_ctx *ctx,
		struct mdss_mdp_ctl *ctl)
{
	int fetch_start = 0, fetch_enable, v_total, h_total;
	struct mdss_data_type *mdata;
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

	mdata = ctl->mdata;

	pinfo->prg_fet = mdss_mdp_get_prefetch_lines(pinfo);
	if (!pinfo->prg_fet) {
		pr_debug("programmable fetch is not needed/supported\n");

		fetch_enable = mdp_video_read(ctx, MDSS_MDP_REG_INTF_CONFIG);
		fetch_enable &= ~BIT(31);

		mdp_video_write(ctx, MDSS_MDP_REG_INTF_CONFIG, fetch_enable);
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_PROG_FETCH_START,
						fetch_start);

		MDSS_XLOG(ctx->intf_num, fetch_enable, fetch_start);
		return;
	}

	/*
	 * Fetch should always be outside the active lines. If the fetching
	 * is programmed within active region, hardware behavior is unknown.
	 */
	v_total = mdss_panel_get_vtotal(pinfo);
	h_total = mdss_panel_get_htotal(pinfo, true);

	fetch_start = (v_total - pinfo->prg_fet) * h_total + 1;

	fetch_enable = mdp_video_read(ctx, MDSS_MDP_REG_INTF_CONFIG);
	fetch_enable |= BIT(31);

	if (pinfo->dynamic_fps && (pinfo->dfps_update ==
			DFPS_IMMEDIATE_CLK_UPDATE_MODE))
		fetch_enable |= BIT(23);

	pr_debug("ctl:%d fetch_start:%d lines:%d\n",
		ctl->num, fetch_start, pinfo->prg_fet);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_PROG_FETCH_START, fetch_start);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_CONFIG, fetch_enable);
	MDSS_XLOG(ctx->intf_num, fetch_enable, fetch_start);
}

static inline bool mdss_mdp_video_need_pixel_drop(u32 vic)
{
	return vic == HDMI_VFRMT_4096x2160p50_256_135 ||
		vic == HDMI_VFRMT_4096x2160p60_256_135;
}

static int mdss_mdp_video_cdm_setup(struct mdss_mdp_cdm *cdm,
	struct mdss_panel_info *pinfo, struct mdss_mdp_format_params *fmt)
{
	struct mdp_cdm_cfg setup;

	if (fmt->is_yuv)
		setup.csc_type = MDSS_MDP_CSC_RGB2YUV_601FR;
	else
		setup.csc_type = MDSS_MDP_CSC_RGB2RGB;

	switch (fmt->chroma_sample) {
	case MDSS_MDP_CHROMA_RGB:
		setup.horz_downsampling_type = MDP_CDM_CDWN_DISABLE;
		setup.vert_downsampling_type = MDP_CDM_CDWN_DISABLE;
		break;
	case MDSS_MDP_CHROMA_H2V1:
		setup.horz_downsampling_type = MDP_CDM_CDWN_COSITE;
		setup.vert_downsampling_type = MDP_CDM_CDWN_DISABLE;
		break;
	case MDSS_MDP_CHROMA_420:
		if (mdss_mdp_video_need_pixel_drop(pinfo->vic)) {
			setup.horz_downsampling_type = MDP_CDM_CDWN_PIXEL_DROP;
			setup.vert_downsampling_type = MDP_CDM_CDWN_PIXEL_DROP;
		} else {
			setup.horz_downsampling_type = MDP_CDM_CDWN_COSITE;
			setup.vert_downsampling_type = MDP_CDM_CDWN_OFFSITE;
		}
		break;
	case MDSS_MDP_CHROMA_H1V2:
	default:
		pr_err("%s: unsupported chroma sampling type\n", __func__);
		return -EINVAL;
	}

	setup.out_format = pinfo->out_format;
	setup.mdp_csc_bit_depth = MDP_CDM_CSC_8BIT;
	setup.output_width = pinfo->xres + pinfo->lcdc.xres_pad;
	setup.output_height = pinfo->yres + pinfo->lcdc.yres_pad;
	return mdss_mdp_cdm_setup(cdm, &setup);
}

static void mdss_mdp_handoff_programmable_fetch(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_video_ctx *ctx)
{
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

	u32 fetch_start_handoff, v_total_handoff, h_total_handoff;
	pinfo->prg_fet = 0;
	if (mdp_video_read(ctx, MDSS_MDP_REG_INTF_CONFIG) & BIT(31)) {
		fetch_start_handoff = mdp_video_read(ctx,
			MDSS_MDP_REG_INTF_PROG_FETCH_START);
		h_total_handoff = mdp_video_read(ctx,
			MDSS_MDP_REG_INTF_HSYNC_CTL) >> 16;
		v_total_handoff = mdp_video_read(ctx,
			MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0)/h_total_handoff;
		pinfo->prg_fet = v_total_handoff -
			((fetch_start_handoff - 1)/h_total_handoff);
		pr_debug("programmable fetch lines %d start:%d\n",
			pinfo->prg_fet, fetch_start_handoff);
		MDSS_XLOG(pinfo->prg_fet, fetch_start_handoff,
			h_total_handoff, v_total_handoff);
	}
}

static int mdss_mdp_video_intf_callback(void *data, int event)
{
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_ctl *ctl = data;
	struct mdss_panel_info *pinfo;
	u32 line_cnt, min_ln_cnt, active_lns_cnt, line_buff = 50;

	if (!data) {
		pr_err("%s: invalid ctl\n", __func__);
		return -EINVAL;
	}

	ctx = ctl->intf_ctx[MASTER_CTX];
	pr_debug("%s: ctl num = %d, event = %d\n",
				__func__, ctl->num, event);

	if (!ctl->is_video_mode)
		return 0;

	pinfo = &ctl->panel_data->panel_info;
	min_ln_cnt = pinfo->lcdc.v_back_porch + pinfo->lcdc.v_pulse_width;
	active_lns_cnt = pinfo->yres;

	switch (event) {
	case MDP_INTF_CALLBACK_CHECK_LINE_COUNT:
		if (!ctl || !ctx || !ctx->timegen_en) {
			pr_debug("%s: no need to check for active line\n",
							__func__);
			goto end;
		}

		line_cnt = mdss_mdp_video_line_count(ctl);

		if ((line_cnt >= min_ln_cnt) && (line_cnt <
			(min_ln_cnt + active_lns_cnt - line_buff))) {
			pr_debug("%s: line count is within active range=%d\n",
						__func__, line_cnt);
			goto end;
		} else {
			pr_debug("line count is less. line_cnt = %d\n",
								line_cnt);
			return -EPERM;
		}
		break;
	default:
		pr_debug("%s: unhandled event!\n", __func__);
		break;
	}
end:
	return 0;
}

static int mdss_mdp_video_ctx_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_video_ctx *ctx, struct mdss_panel_info *pinfo)
{
	struct intf_timing_params *itp = &ctx->itp;
	u32 dst_bpp;
	struct mdss_mdp_format_params *fmt;
	struct mdss_data_type *mdata = ctl->mdata;
	struct dsc_desc *dsc = NULL;

	ctx->ctl = ctl;
	ctx->intf_type = ctl->intf_type;
	init_completion(&ctx->vsync_comp);
	spin_lock_init(&ctx->vsync_lock);
	spin_lock_init(&ctx->dfps_lock);
	mutex_init(&ctx->vsync_mtx);
	atomic_set(&ctx->vsync_ref, 0);
	spin_lock_init(&ctx->lineptr_lock);
	spin_lock_init(&ctx->mdss_mdp_video_lock);
	spin_lock_init(&ctx->mdss_mdp_intf_intr_lock);
	mutex_init(&ctx->lineptr_mtx);
	atomic_set(&ctx->lineptr_ref, 0);
	INIT_WORK(&ctl->recover_work, recover_underrun_work);

	if (ctl->intf_type == MDSS_INTF_DSI) {
		ctx->intf_recovery.fxn = mdss_mdp_video_intf_recovery;
		ctx->intf_recovery.data = ctl;
		if (mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_REGISTER_RECOVERY_HANDLER,
					(void *)&ctx->intf_recovery,
					CTL_INTF_EVENT_FLAG_DEFAULT)) {
			pr_err("Failed to register intf recovery handler\n");
			return -EINVAL;
		}

		ctx->intf_mdp_callback.fxn = mdss_mdp_video_intf_callback;
		ctx->intf_mdp_callback.data = ctl;
		mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_REGISTER_MDP_CALLBACK,
				(void *)&ctx->intf_mdp_callback,
				CTL_INTF_EVENT_FLAG_DEFAULT);

		ctx->intf_clamp_handler.fxn = mdss_mdp_video_intf_clamp_ctrl;
		ctx->intf_clamp_handler.data = ctx;
		mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_REGISTER_CLAMP_HANDLER,
				(void *)&ctx->intf_clamp_handler,
				CTL_INTF_EVENT_FLAG_DEFAULT);
	} else {
		ctx->intf_recovery.fxn = NULL;
		ctx->intf_recovery.data = NULL;
	}

	if (mdss_mdp_is_cdm_supported(mdata, ctl->intf_type, 0)) {

		fmt = mdss_mdp_get_format_params(pinfo->out_format);
		if (!fmt) {
			pr_err("%s: format %d not supported\n", __func__,
			       pinfo->out_format);
			return -EINVAL;
		}
		if (fmt->is_yuv) {
			ctl->cdm =
			mdss_mdp_cdm_init(ctl, MDP_CDM_CDWN_OUTPUT_HDMI);
			if (!IS_ERR_OR_NULL(ctl->cdm)) {
				if (mdss_mdp_video_cdm_setup(ctl->cdm,
					pinfo, fmt)) {
					pr_err("%s: setting up cdm failed\n",
					       __func__);
					return -EINVAL;
				}
				ctl->flush_bits |= BIT(26);
			} else {
				pr_err("%s: failed to initialize cdm\n",
					__func__);
				return -EINVAL;
			}
		} else {
			pr_debug("%s: Format is not YUV,cdm not required\n",
				 __func__);
		}
	} else {
		pr_debug("%s: cdm not supported\n", __func__);
	}

	if (pinfo->compression_mode == COMPRESSION_DSC)
		dsc = &pinfo->dsc;

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_INTF_VSYNC,
			ctx->intf_num, mdss_mdp_video_vsync_intr_done,
			ctl);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_INTF_UNDER_RUN,
				ctx->intf_num,
				mdss_mdp_video_underrun_intr_done, ctl);
	mdss_mdp_set_intf_intr_callback(ctx, MDSS_MDP_INTF_IRQ_PROG_LINE,
			mdss_mdp_video_lineptr_intr_done, ctl);

	dst_bpp = pinfo->fbc.enabled ? (pinfo->fbc.target_bpp) : (pinfo->bpp);

	memset(itp, 0, sizeof(struct intf_timing_params));
	itp->width = mult_frac((pinfo->xres + pinfo->lcdc.border_left +
			pinfo->lcdc.border_right), dst_bpp, pinfo->bpp);
	itp->height = pinfo->yres + pinfo->lcdc.border_top +
					pinfo->lcdc.border_bottom;
	itp->border_clr = pinfo->lcdc.border_clr;
	itp->underflow_clr = pinfo->lcdc.underflow_clr;
	itp->hsync_skew = pinfo->lcdc.hsync_skew;

	/* tg active area is not work, hence yres should equal to height */
	itp->xres = mult_frac((pinfo->xres + pinfo->lcdc.border_left +
			pinfo->lcdc.border_right), dst_bpp, pinfo->bpp);

	itp->yres = pinfo->yres + pinfo->lcdc.border_top +
				pinfo->lcdc.border_bottom;

	if (dsc) {	/* compressed */
		itp->width = dsc->pclk_per_line;
		itp->xres = dsc->pclk_per_line;
	}

	itp->h_back_porch = pinfo->lcdc.h_back_porch;
	itp->h_front_porch = pinfo->lcdc.h_front_porch;
	itp->v_back_porch = pinfo->lcdc.v_back_porch;
	itp->v_front_porch = pinfo->lcdc.v_front_porch;
	itp->hsync_pulse_width = pinfo->lcdc.h_pulse_width;
	itp->vsync_pulse_width = pinfo->lcdc.v_pulse_width;
	/*
	 * In case of YUV420 output, MDP outputs data at half the rate. So
	 * reduce all horizontal parameters by half
	 */
	if (ctl->cdm && pinfo->out_format == MDP_Y_CBCR_H2V2) {
		itp->width >>= 1;
		itp->hsync_skew >>= 1;
		itp->xres >>= 1;
		itp->h_back_porch >>= 1;
		itp->h_front_porch >>= 1;
		itp->hsync_pulse_width >>= 1;
	}
	if (!ctl->panel_data->panel_info.cont_splash_enabled) {
		if (mdss_mdp_video_timegen_setup(ctl, itp, ctx)) {
			pr_err("unable to set timing parameters intfs: %d\n",
				ctx->intf_num);
			return -EINVAL;
		}
		mdss_mdp_fetch_start_config(ctx, ctl);

		if (test_bit(MDSS_QOS_VBLANK_PANIC_CTRL, mdata->mdss_qos_map))
			mdss_mdp_fetch_end_config(ctx, ctl);

	} else {
		mdss_mdp_handoff_programmable_fetch(ctl, ctx);
	}

	mdss_mdp_video_avr_vtotal_setup(ctl, itp, ctx);

	mdss_mdp_disable_prefill(ctl);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_PANEL_FORMAT, ctl->dst_format);

	/* select HDMI or DP core usage */
	switch (ctx->intf_type) {
	case MDSS_INTF_EDP:
		writel_relaxed(0x1, mdata->mdp_base +
			MDSS_MDP_HDMI_DP_CORE_SELECT);
		break;
	case MDSS_INTF_HDMI:
		writel_relaxed(0x0, mdata->mdp_base +
			MDSS_MDP_HDMI_DP_CORE_SELECT);
		break;
	default:
		break;
	}

	return 0;
}

static int mdss_mdp_video_intfs_setup(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_data *pdata, int inum)
{
	struct mdss_data_type *mdata;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_video_ctx *ctx;
	int ret = 0;

	if (pdata == NULL)
		return 0;

	mdata = ctl->mdata;
	pinfo = &pdata->panel_info;

	if (inum < mdata->nintf) {
		ctx = ((struct mdss_mdp_video_ctx *) mdata->video_intf) + inum;
		if (ctx->ref_cnt) {
			pr_err("Intf %d already in use\n",
					(inum + MDSS_MDP_INTF0));
			return -EBUSY;
		}
		pr_debug("video Intf #%d base=%pK", ctx->intf_num, ctx->base);
		ctx->ref_cnt++;
	} else {
		pr_err("Invalid intf number: %d\n", (inum + MDSS_MDP_INTF0));
		return -EINVAL;
	}

	ctl->intf_ctx[MASTER_CTX] = ctx;
	ret = mdss_mdp_video_ctx_setup(ctl, ctx, pinfo);
	if (ret) {
		pr_err("Video context setup failed for interface: %d\n",
				ctx->intf_num);
		ctx->ref_cnt--;
		return -EPERM;
	}

	/* Initialize early wakeup for the master ctx */
	INIT_WORK(&ctx->early_wakeup_dfps_work, early_wakeup_dfps_update_work);

	if (is_pingpong_split(ctl->mfd)) {
		if ((inum + 1) >= mdata->nintf) {
			pr_err("Intf not available for ping pong split: (%d)\n",
					(inum + 1 + MDSS_MDP_INTF0));
			return -EINVAL;
		}

		ctx = ((struct mdss_mdp_video_ctx *) mdata->video_intf) +
			inum + 1;
		if (ctx->ref_cnt) {
			pr_err("Intf %d already in use\n",
					(inum + MDSS_MDP_INTF0));
			return -EBUSY;
		}
		pr_debug("video Intf #%d base=%pK", ctx->intf_num, ctx->base);
		ctx->ref_cnt++;

		ctl->intf_ctx[SLAVE_CTX] = ctx;
		pinfo = &pdata->next->panel_info;
		ret = mdss_mdp_video_ctx_setup(ctl, ctx, pinfo);
		if (ret) {
			pr_err("Video context setup failed for interface: %d\n",
					ctx->intf_num);
			ctx->ref_cnt--;
			return -EPERM;
		}
	}
	return 0;
}

void mdss_mdp_switch_to_cmd_mode(struct mdss_mdp_ctl *ctl, int prep)
{
	struct mdss_mdp_video_ctx *ctx;
	long int mode = MIPI_CMD_PANEL;
	u32 frame_rate = 0;
	int rc;

	pr_debug("start, prep = %d\n", prep);

	if (!prep) {
		mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_RECONFIG_CMD,
			(void *) mode, CTL_INTF_EVENT_FLAG_DEFAULT);
		return;
	}

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];

	if (!ctx->timegen_en) {
		pr_err("Time engine not enabled, cannot switch from vid\n");
		return;
	}

	/* Start off by sending command to initial cmd mode */
	rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_DYNAMIC_SWITCH,
			     (void *) mode, CTL_INTF_EVENT_FLAG_DEFAULT);
	if (rc) {
		pr_err("intf #%d busy don't turn off, rc=%d\n",
			 ctl->intf_num, rc);
		return;
	}

	if (ctx->wait_pending) {
		/* wait for at least commit to commplete */
		wait_for_completion_interruptible_timeout(&ctx->vsync_comp,
			  usecs_to_jiffies(VSYNC_TIMEOUT_US));
	}
	frame_rate = mdss_panel_get_framerate
			(&(ctl->panel_data->panel_info));
	if (!(frame_rate >= 24 && frame_rate <= 240))
		frame_rate = 24;
	frame_rate = ((1000/frame_rate) + 1);
	/*
	 * In order for panel to switch to cmd mode, we need
	 * to wait for one more video frame to be sent after
	 * issuing the switch command. We do this before
	 * turning off the timeing engine.
	 */
	msleep(frame_rate);
	mdss_mdp_turn_off_time_engine(ctl, ctx, frame_rate);
	mdss_bus_bandwidth_ctrl(false);
}

static void early_wakeup_dfps_update_work(struct work_struct *work)
{
	struct mdss_mdp_video_ctx *ctx =
		container_of(work, typeof(*ctx), early_wakeup_dfps_work);
	struct mdss_panel_data *pdata;
	struct mdss_panel_info *pinfo;
	struct msm_fb_data_type *mfd;
	struct mdss_mdp_ctl *ctl;
	struct mdss_data_type *mdata;
	struct dynamic_fps_data data = {0};
	int ret = 0;
	int dfps;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	ctl = ctx->ctl;

	if (!ctl || !ctl->panel_data || !ctl->mfd || !ctl->mfd->fbi ||
			!ctl->mdata) {
		pr_err("%s: invalid ctl\n", __func__);
		return;
	}

	pdata = ctl->panel_data;
	pinfo = &ctl->panel_data->panel_info;
	mfd =	ctl->mfd;
	mdata = ctl->mdata;

	if (!pinfo->dynamic_fps || !ctl->ops.config_fps_fnc ||
		!pdata->panel_info.default_fps) {
		pr_debug("%s: dfps not enabled on this panel\n", __func__);
		return;
	}

	/*
	 * Bypass DFPS update when AVR is enabled because
	 * AVR will take control of the programmable fetch
	 */
	if (test_bit(MDSS_CAPS_AVR_SUPPORTED,
				mdata->mdss_caps_map) &&
			ctl->avr_info.avr_enabled) {
		pr_debug("Bypass DFPS update when AVR is enabled\n");
		return;
	}

	/* get the default fps that was cached before any dfps update */
	dfps = pdata->panel_info.default_fps;

	ATRACE_BEGIN(__func__);

	if (dfps == pinfo->mipi.frame_rate) {
		pr_debug("%s: FPS is already %d\n",
			__func__, dfps);
		goto exit;
	}

	data.fps = dfps;
	if (mdss_mdp_dfps_update_params(mfd, pdata, &data))
		pr_err("failed to set dfps params!\n");

	/* update the HW with the new fps */
	ATRACE_BEGIN("fps_update_wq");
	ret = mdss_mdp_ctl_update_fps(ctl);
	ATRACE_END("fps_update_wq");
	if (ret)
		pr_err("early wakeup failed to set %d fps ret=%d\n",
			dfps, ret);

exit:
	ATRACE_END(__func__);
}

static int mdss_mdp_video_early_wake_up(struct mdss_mdp_ctl *ctl)
{
	u64 curr_time;

	curr_time = ktime_to_us(ktime_get());

	if ((curr_time - ctl->last_input_time) <
			INPUT_EVENT_HANDLER_DELAY_USECS)
		return 0;
	ctl->last_input_time = curr_time;

	/*
	 * If the idle timer is running when input event happens, the timeout
	 * will be delayed by idle_time again to ensure user space does not get
	 * an idle event when new frames are expected.
	 *
	 * It would be nice to have this logic in mdss_fb.c itself by
	 * implementing a new frame notification event. But input event handler
	 * is called from interrupt context and scheduling a work item adds a
	 * lot of latency rendering the input events useless in preventing the
	 * idle time out.
	 */
	if (ctl->mfd->idle_state == MDSS_FB_IDLE_TIMER_RUNNING) {
		if (ctl->mfd->idle_time)
			mod_delayed_work(system_wq, &ctl->mfd->idle_notify_work,
					 msecs_to_jiffies(ctl->mfd->idle_time));
		pr_debug("Delayed idle time\n");
	} else {
		pr_debug("Nothing to done for this state (%d)\n",
			 ctl->mfd->idle_state);
	}

	/*
	 * Schedule an fps update, so we can go to default fps before
	 * commit. Early wake up event is called from an interrupt
	 * context, so do this from work queue
	 */
	if (ctl->panel_data && ctl->panel_data->panel_info.dynamic_fps) {
		struct mdss_mdp_video_ctx *ctx;

		ctx = ctl->intf_ctx[MASTER_CTX];
		if (ctx)
			schedule_work(&ctx->early_wakeup_dfps_work);
	}

	return 0;
}

static int mdss_mdp_video_avr_ctrl(struct mdss_mdp_ctl *ctl, bool enable)
{
	struct mdss_mdp_video_ctx *ctx = NULL, *sctx = NULL;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctx->ref_cnt) {
		pr_err("invalid master ctx\n");
		return -EINVAL;
	}
	mdss_mdp_video_avr_ctrl_setup(ctx, ctl, ctl->is_master,
			enable);

	if (is_pingpong_split(ctl->mfd)) {
		sctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[SLAVE_CTX];
		if (!sctx || !sctx->ref_cnt) {
			pr_err("invalid slave ctx\n");
			return -EINVAL;
		}
		mdss_mdp_video_avr_ctrl_setup(sctx, ctl, false,
				enable);
	}

	return 0;
}

int mdss_mdp_video_start(struct mdss_mdp_ctl *ctl)
{
	int intfs_num, ret = 0;

	intfs_num = ctl->intf_num - MDSS_MDP_INTF0;
	ret = mdss_mdp_video_intfs_setup(ctl, ctl->panel_data, intfs_num);
	if (IS_ERR_VALUE(ret)) {
		pr_err("unable to set video interface: %d\n", ret);
		return ret;
	}

	ctl->ops.stop_fnc = mdss_mdp_video_stop;
	ctl->ops.display_fnc = mdss_mdp_video_display;
	ctl->ops.wait_fnc = mdss_mdp_video_wait4comp;
	ctl->ops.read_line_cnt_fnc = mdss_mdp_video_line_count;
	ctl->ops.add_vsync_handler = mdss_mdp_video_add_vsync_handler;
	ctl->ops.remove_vsync_handler = mdss_mdp_video_remove_vsync_handler;
	ctl->ops.config_fps_fnc = mdss_mdp_video_config_fps;
	ctl->ops.early_wake_up_fnc = mdss_mdp_video_early_wake_up;
	ctl->ops.update_lineptr = mdss_mdp_video_lineptr_ctrl;
	ctl->ops.avr_ctrl_fnc = mdss_mdp_video_avr_ctrl;
	ctl->ops.wait_for_vsync_fnc = NULL;

	return 0;
}

void *mdss_mdp_get_intf_base_addr(struct mdss_data_type *mdata,
		u32 interface_id)
{
	struct mdss_mdp_video_ctx *ctx;
	ctx = ((struct mdss_mdp_video_ctx *) mdata->video_intf) + interface_id;
	return (void *)(ctx->base);
}
