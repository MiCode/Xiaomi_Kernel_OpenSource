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

#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_trace.h"

/* wait for at least 2 vsyncs for lowest refresh rate (24hz) */
#define VSYNC_TIMEOUT_US 100000

/* Poll time to do recovery during active region */
#define POLL_TIME_USEC_FOR_LN_CNT 500

#define MDP_INTR_MASK_INTF_VSYNC(intf_num) \
	(1 << (2 * (intf_num - MDSS_MDP_INTF0) + MDSS_MDP_IRQ_INTF_VSYNC))

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
	u32 h_polarity;
	u32 v_polarity;

	u32 border_clr;
	u32 underflow_clr;
	u32 hsync_skew;
};

struct mdss_mdp_video_ctx {
	u32 intf_num;
	char __iomem *base;
	u32 intf_type;
	u8 ref_cnt;

	u8 timegen_en;
	bool polling_en;
	u32 poll_cnt;
	struct completion vsync_comp;
	int wait_pending;

	u32 default_fps;
	u32 saved_vtotal;
	u32 saved_vfporch;

	atomic_t vsync_ref;
	spinlock_t vsync_lock;
	spinlock_t dfps_lock;
	struct mutex vsync_mtx;
	struct list_head vsync_handlers;
	struct mdss_intf_recovery intf_recovery;
};

static void mdss_mdp_fetch_start_config(struct mdss_mdp_video_ctx *ctx,
		struct mdss_mdp_ctl *ctl);

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
	}

	mdata->video_intf = head;
	mdata->nintf = count;
	return 0;
}

static void mdss_mdp_video_intf_recovery(void *data, int event)
{
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_ctl *ctl = data;
	struct mdss_panel_info *pinfo;
	u32 line_cnt, min_ln_cnt, active_lns_cnt;
	u32 clk_rate, clk_period, time_of_line;
	u32 delay;

	if (!data) {
		pr_err("%s: invalid ctl\n", __func__);
		return;
	}

	/*
	 * Currently, only intf_fifo_overflow is
	 * supported for recovery sequence for video
	 * mode DSI interface
	 */
	if (event != MDP_INTF_DSI_VIDEO_FIFO_OVERFLOW) {
		pr_warn("%s: unsupported recovery event:%d\n",
					__func__, event);
		return;
	}

	ctx = ctl->intf_ctx[MASTER_CTX];
	pr_debug("%s: ctl num = %d, event = %d\n",
				__func__, ctl->num, event);

	pinfo = &ctl->panel_data->panel_info;
	clk_rate = ((ctl->intf_type == MDSS_INTF_DSI) ?
			pinfo->mipi.dsi_pclk_rate :
			pinfo->clk_rate);

	clk_rate /= 1000;	/* in kHz */
	if (!clk_rate) {
		pr_err("Unable to get proper clk_rate\n");
		return;
	}
	/*
	 * calculate clk_period as pico second to maintain good
	 * accuracy with high pclk rate and this number is in 17 bit
	 * range.
	 */
	clk_period = 1000000000 / clk_rate;
	if (!clk_period) {
		pr_err("Unable to calculate clock period\n");
		return;
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

	while (1) {
		if (!ctl || !ctx || !ctx->timegen_en) {
			pr_warn("Target is in suspend state\n");
			return;
		}

		line_cnt = mdss_mdp_video_line_count(ctl);

		if ((line_cnt >= min_ln_cnt) && (line_cnt <
			(active_lns_cnt + min_ln_cnt))) {
			pr_debug("%s, Needed lines left line_cnt=%d\n",
						__func__, line_cnt);
			return;
		} else {
			pr_warn("line count is less. line_cnt = %d\n",
								line_cnt);
			/* Add delay so that line count is in active region */
			udelay(delay);
		}
	}
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
	hsync_polarity = p->h_polarity;
	vsync_polarity = p->v_polarity;
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


static inline void video_vsync_irq_enable(struct mdss_mdp_ctl *ctl, bool clear)
{
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

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
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	mutex_lock(&ctx->vsync_mtx);
	if (atomic_dec_return(&ctx->vsync_ref) == 0)
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num);
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
	if (irq_en)
		video_vsync_irq_enable(ctl, false);
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
	if (irq_dis)
		video_vsync_irq_disable(ctl);
	return 0;
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

	mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num);

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_UNDER_RUN,
			sctl->intf_num);
}

static int mdss_mdp_video_ctx_stop(struct mdss_mdp_ctl *ctl,
		struct mdss_panel_info *pinfo, struct mdss_mdp_video_ctx *ctx)
{
	int rc = 0;
	u32 frame_rate = 0;

	if (ctx->timegen_en) {
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK, NULL);
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

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF, NULL);
		WARN(rc, "intf %d timegen off error (%d)\n", ctl->intf_num, rc);

		mdss_bus_bandwidth_ctrl(false);
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC,
		ctx->intf_num, NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_UNDER_RUN,
		ctx->intf_num, NULL, NULL);

	ctx->ref_cnt--;
end:
	return rc;
}

static int mdss_mdp_video_intfs_stop(struct mdss_mdp_ctl *ctl,
	struct mdss_panel_data *pdata, int inum)
{
	struct mdss_data_type *mdata;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_video_ctx *ctx;
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

		ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[SLAVE_CTX];
		if (!ctx->ref_cnt) {
			pr_err("Intf %d not in use\n", (inum + MDSS_MDP_INTF0));
			return -ENODEV;
		}
		pr_debug("stop ctl=%d video Intf #%d base=%pK", ctl->num,
				ctx->intf_num, ctx->base);

		ret = mdss_mdp_video_ctx_stop(ctl, pinfo, ctx);
		if (ret) {
			pr_err("mdss_mdp_video_ctx_stop failed for intf: %d",
					ctx->intf_num);
			return -EPERM;
		}
	}

	list_for_each_entry_safe(handle, tmp, &ctx->vsync_handlers, list)
		mdss_mdp_video_remove_vsync_handler(ctl, handle);

	return 0;
}


static int mdss_mdp_video_stop(struct mdss_mdp_ctl *ctl, int panel_power_state)
{
	int intfs_num, ret = 0;

	mutex_lock(&ctl->offlock);
	intfs_num = ctl->intf_num - MDSS_MDP_INTF0;
	ret = mdss_mdp_video_intfs_stop(ctl, ctl->panel_data, intfs_num);
	if (IS_ERR_VALUE(ret)) {
		pr_err("unable to stop video interface: %d\n", ret);
		return ret;
	}

	MDSS_XLOG(ctl->num, ctl->vsync_cnt);

	mdss_mdp_ctl_reset(ctl);
	ctl->intf_ctx[MASTER_CTX] = NULL;
	mutex_unlock(&ctl->offlock);

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

static int mdss_mdp_video_pollwait(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_video_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	u32 mask, status;
	int rc;

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

	if (ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE)
		schedule_work(&ctl->recover_work);
}

static int mdss_mdp_video_timegen_update(struct mdss_mdp_video_ctx *ctx,
					struct mdss_panel_info *pinfo)
{
	u32 hsync_period, vsync_period;
	u32 hsync_start_x, hsync_end_x, display_v_start, display_v_end;
	u32 display_hctl, hsync_ctl;

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

	return 0;
}

static int mdss_mdp_video_hfp_fps_update(struct mdss_mdp_video_ctx *ctx,
			struct mdss_panel_data *pdata, int new_fps)
{
	int curr_fps;
	int add_h_pixels = 0;
	int hsync_period;
	int diff;

	hsync_period = mdss_panel_get_htotal(&pdata->panel_info, true);
	curr_fps = mdss_panel_get_framerate(&pdata->panel_info);

	diff = curr_fps - new_fps;
	add_h_pixels = mult_frac(hsync_period, diff, new_fps);
	pdata->panel_info.lcdc.h_front_porch += add_h_pixels;

	mdss_mdp_video_timegen_update(ctx, &pdata->panel_info);
	return 0;
}

static int mdss_mdp_video_vfp_fps_update(struct mdss_mdp_video_ctx *ctx,
				 struct mdss_panel_data *pdata, int new_fps)
{
	int add_v_lines = 0;
	u32 current_vsync_period_f0, new_vsync_period_f0;
	int vsync_period, hsync_period;
	int diff;

	vsync_period = mdss_panel_get_vtotal(&pdata->panel_info);
	hsync_period = mdss_panel_get_htotal(&pdata->panel_info, true);

	if (!ctx->default_fps) {
		ctx->default_fps = mdss_panel_get_framerate(&pdata->panel_info);
		ctx->saved_vtotal = vsync_period;
		ctx->saved_vfporch = pdata->panel_info.lcdc.v_front_porch;
	}

	diff = ctx->default_fps - new_fps;
	add_v_lines = mult_frac(ctx->saved_vtotal, diff, new_fps);
	pdata->panel_info.lcdc.v_front_porch = ctx->saved_vfporch +
			add_v_lines;

	vsync_period = mdss_panel_get_vtotal(&pdata->panel_info);
	current_vsync_period_f0 = mdp_video_read(ctx,
		MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0);
	new_vsync_period_f0 = (vsync_period * hsync_period);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
			current_vsync_period_f0 | 0x800000);
	if (new_vsync_period_f0 & 0x800000) {
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
			new_vsync_period_f0);
	} else {
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
			new_vsync_period_f0 | 0x800000);
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0,
			new_vsync_period_f0 & 0x7fffff);
	}

	return 0;
}

static int mdss_mdp_video_fps_update(struct mdss_mdp_video_ctx *ctx,
				 struct mdss_panel_data *pdata, int new_fps)
{
	int rc;

	if (pdata->panel_info.dfps_update ==
				DFPS_IMMEDIATE_PORCH_UPDATE_MODE_HFP)
		rc = mdss_mdp_video_hfp_fps_update(ctx, pdata, new_fps);
	else
		rc = mdss_mdp_video_vfp_fps_update(ctx, pdata, new_fps);

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
	INIT_COMPLETION(ctx->vsync_comp);
	rc = wait_for_completion_timeout(&ctx->vsync_comp,
		usecs_to_jiffies(VSYNC_TIMEOUT_US));
	WARN(rc <= 0, "timeout (%d) vsync interrupt on ctl=%d\n",
		rc, ctl->num);

	video_vsync_irq_disable(ctl);
	if (rc <= 0)
		return -EPERM;

	return 0;
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
}

static int mdss_mdp_video_config_fps(struct mdss_mdp_ctl *ctl,
					struct mdss_mdp_ctl *sctl, int new_fps)
{
	struct mdss_mdp_video_ctx *ctx, *sctx = NULL;
	struct mdss_panel_data *pdata;
	int rc = 0;
	u32 hsync_period, vsync_period;
	struct mdss_data_type *mdata;

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	mdata = ctl->mdata;
	if (sctl) {
		sctx = (struct mdss_mdp_video_ctx *) sctl->intf_ctx[MASTER_CTX];
		if (!sctx) {
			pr_err("invalid ctx\n");
			return -ENODEV;
		}
	} else if (is_pingpong_split(ctl->mfd)) {
		sctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[SLAVE_CTX];
		if (!sctx) {
			pr_err("invalid sctx\n");
			return -ENODEV;
		}
	}

	pdata = ctl->panel_data;
	if (pdata == NULL) {
		pr_err("%s: Invalid panel data\n", __func__);
		return -EINVAL;
	}

	if (!pdata->panel_info.dynamic_fps) {
		pr_err("%s: Dynamic fps not enabled for this panel\n",
						__func__);
		return -EINVAL;
	}

	vsync_period = mdss_panel_get_vtotal(&pdata->panel_info);
	hsync_period = mdss_panel_get_htotal(&pdata->panel_info, true);

	if (pdata->panel_info.dfps_update
			!= DFPS_SUSPEND_RESUME_MODE) {
		if (!ctx->timegen_en) {
			pr_err("TG is OFF. DFPS mode invalid\n");
			return -EINVAL;
		}

		/*
		 * there is possibility that the time of mdp flush
		 * bit set and the time of dsi flush bit are cross
		 * vsync boundary. therefore wait4vsync is needed
		 * to guarantee both flush bits are set within same
		 * vsync period regardless of mdp revision.
		 */
		rc = mdss_mdp_video_dfps_wait4vsync(ctl);
		if (rc < 0) {
			pr_err("Error during wait4vsync\n");
			return rc;
		}

		if (pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
			rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_PANEL_UPDATE_FPS,
					(void *) (unsigned long) new_fps);
			WARN(rc, "intf %d panel fps update error (%d)\n",
							ctl->intf_num, rc);
		} else if (pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_PORCH_UPDATE_MODE_VFP ||
				pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_PORCH_UPDATE_MODE_HFP) {
			unsigned long flags;

			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			spin_lock_irqsave(&ctx->dfps_lock, flags);

			rc = mdss_mdp_video_dfps_check_line_cnt(ctl);
			if (rc < 0)
				goto exit_dfps;

			rc = mdss_mdp_video_fps_update(ctx, pdata, new_fps);
			if (rc < 0) {
				pr_err("%s: Error during DFPS\n", __func__);
				goto exit_dfps;
			}
			if (sctx) {
				rc = mdss_mdp_video_fps_update(sctx,
							pdata->next, new_fps);
				if (rc < 0) {
					pr_err("%s: DFPS error\n", __func__);
					goto exit_dfps;
				}
			}
			rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_PANEL_UPDATE_FPS,
					(void *) (unsigned long) new_fps);
			WARN(rc, "intf %d panel fps update error (%d)\n",
							ctl->intf_num, rc);

			mdss_mdp_fetch_start_config(ctx, ctl);
			if (sctx)
				mdss_mdp_fetch_start_config(sctx, ctl);

			/*
			 * MDP INTF registers support DB on targets
			 * starting from MDP v1.5.
			 */
			if (mdata->mdp_rev >= MDSS_MDP_HW_REV_105)
				mdss_mdp_video_timegen_flush(ctl, sctx);

exit_dfps:
			spin_unlock_irqrestore(&ctx->dfps_lock, flags);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		} else {
			pr_err("intf %d panel, unknown FPS mode\n",
							ctl->intf_num);
			return -EINVAL;
		}
	} else {
		rc = mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_PANEL_UPDATE_FPS,
				(void *) (unsigned long) new_fps);
		WARN(rc, "intf %d panel fps update error (%d)\n",
						ctl->intf_num, rc);
	}

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
		INIT_COMPLETION(ctx->vsync_comp);
	} else {
		WARN(1, "commit without wait! ctl=%d", ctl->num);
	}

	MDSS_XLOG(ctl->num, ctl->underrun_cnt);

	if (!ctx->timegen_en) {
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_LINK_READY, NULL);
		if (rc) {
			pr_warn("intf #%d link ready error (%d)\n",
					ctl->intf_num, rc);
			video_vsync_irq_disable(ctl);
			ctx->wait_pending = 0;
			return rc;
		}

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_UNBLANK, NULL);
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

		mdss_mdp_irq_enable(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num);
		sctl = mdss_mdp_get_split_ctl(ctl);
		if (sctl)
			mdss_mdp_irq_enable(MDSS_MDP_IRQ_INTF_UNDER_RUN,
				sctl->intf_num);

		mdss_bus_bandwidth_ctrl(true);

		mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 1);
		wmb();

		rc = wait_for_completion_timeout(&ctx->vsync_comp,
				usecs_to_jiffies(VSYNC_TIMEOUT_US));
		WARN(rc == 0, "timeout (%d) enabling timegen on ctl=%d\n",
				rc, ctl->num);

		ctx->timegen_en = true;
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_ON, NULL);
		WARN(rc, "intf %d panel on error (%d)\n", ctl->intf_num, rc);
	}

	return 0;
}

int mdss_mdp_video_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
	bool handoff)
{
	struct mdss_panel_data *pdata;
	int i, ret = 0, off;
	u32 data, flush;
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

	off = 0;
	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}

	pdata = ctl->panel_data;

	pdata->panel_info.cont_splash_enabled = 0;
	if (sctl)
		sctl->panel_data->panel_info.cont_splash_enabled = 0;
	else if (ctl->panel_data->next && is_pingpong_split(ctl->mfd))
		ctl->panel_data->next->panel_info.cont_splash_enabled = 0;

	if (!handoff) {
		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CONT_SPLASH_BEGIN,
					      NULL);
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
		/* wait for 1 VSYNC for the pipe to be unstaged */
		msleep(20);

		ret = mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_CONT_SPLASH_FINISH, NULL);
	}

	return ret;
}

static void mdss_mdp_disable_prefill(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;
	struct mdss_data_type *mdata = ctl->mdata;

	if ((ctl->prg_fet + pinfo->lcdc.v_back_porch +
			pinfo->lcdc.v_pulse_width) > mdata->min_prefill_lines) {
		ctl->disable_prefill = true;
		pr_debug("disable prefill vbp:%d vpw:%d prg_fet:%d\n",
			pinfo->lcdc.v_back_porch, pinfo->lcdc.v_pulse_width,
			ctl->prg_fet);
	}
}

static void mdss_mdp_fetch_start_config(struct mdss_mdp_video_ctx *ctx,
		struct mdss_mdp_ctl *ctl)
{
	int fetch_start, fetch_enable, v_total, h_total;
	struct mdss_data_type *mdata;
	struct mdss_panel_info *pinfo = &ctl->panel_data->panel_info;

	mdata = ctl->mdata;

	ctl->prg_fet = mdss_mdp_get_prefetch_lines(ctl);
	if (!ctl->prg_fet) {
		pr_debug("programmable fetch is not needed/supported\n");
		return;
	}

	/*
	 * Fetch should always be outside the active lines. If the fetching
	 * is programmed within active region, hardware behavior is unknown.
	 */
	v_total = mdss_panel_get_vtotal(pinfo);
	h_total = mdss_panel_get_htotal(pinfo, true);

	fetch_start = (v_total - ctl->prg_fet) * h_total + 1;
	fetch_enable = BIT(31);

	if (pinfo->dynamic_fps && (pinfo->dfps_update ==
			DFPS_IMMEDIATE_CLK_UPDATE_MODE))
		fetch_enable |= BIT(23);

	pr_debug("ctl:%d fetch_start:%d lines:%d\n",
		ctl->num, fetch_start, ctl->prg_fet);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_PROG_FETCH_START, fetch_start);
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_CONFIG, fetch_enable);
}

static void mdss_mdp_handoff_programmable_fetch(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_video_ctx *ctx)
{
	u32 fetch_start_handoff, v_total_handoff, h_total_handoff;
	ctl->prg_fet = 0;
	if (mdp_video_read(ctx, MDSS_MDP_REG_INTF_CONFIG) & BIT(31)) {
		fetch_start_handoff = mdp_video_read(ctx,
			MDSS_MDP_REG_INTF_PROG_FETCH_START);
		h_total_handoff = mdp_video_read(ctx,
			MDSS_MDP_REG_INTF_HSYNC_CTL) >> 16;
		v_total_handoff = mdp_video_read(ctx,
			MDSS_MDP_REG_INTF_VSYNC_PERIOD_F0)/h_total_handoff;
		ctl->prg_fet = v_total_handoff -
			((fetch_start_handoff - 1)/h_total_handoff);
		pr_debug("programmable fetch lines %d start:%d\n",
			ctl->prg_fet, fetch_start_handoff);
	}
}

static int mdss_mdp_video_ctx_setup(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_video_ctx *ctx, struct mdss_panel_info *pinfo)
{
	struct intf_timing_params itp = {0};
	u32 dst_bpp;

	ctx->intf_type = ctl->intf_type;
	init_completion(&ctx->vsync_comp);
	spin_lock_init(&ctx->vsync_lock);
	spin_lock_init(&ctx->dfps_lock);
	mutex_init(&ctx->vsync_mtx);
	atomic_set(&ctx->vsync_ref, 0);
	INIT_WORK(&ctl->recover_work, recover_underrun_work);

	if (ctl->intf_type == MDSS_INTF_DSI) {
		ctx->intf_recovery.fxn = mdss_mdp_video_intf_recovery;
		ctx->intf_recovery.data = ctl;
		if (mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_REGISTER_RECOVERY_HANDLER,
					(void *)&ctx->intf_recovery)) {
			pr_err("Failed to register intf recovery handler\n");
			return -EINVAL;
		}
	} else {
		ctx->intf_recovery.fxn = NULL;
		ctx->intf_recovery.data = NULL;
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC,
			ctx->intf_num, mdss_mdp_video_vsync_intr_done,
			ctl);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_UNDER_RUN,
				ctx->intf_num,
				mdss_mdp_video_underrun_intr_done, ctl);

	dst_bpp = pinfo->fbc.enabled ? (pinfo->fbc.target_bpp) : (pinfo->bpp);

	itp.width = mult_frac((pinfo->xres + pinfo->lcdc.border_left +
			pinfo->lcdc.border_right), dst_bpp, pinfo->bpp);
	itp.height = pinfo->yres + pinfo->lcdc.border_top +
					pinfo->lcdc.border_bottom;
	itp.border_clr = pinfo->lcdc.border_clr;
	itp.underflow_clr = pinfo->lcdc.underflow_clr;
	itp.hsync_skew = pinfo->lcdc.hsync_skew;

	/* tg active area is not work, hence yres should equal to height */
	itp.xres = mult_frac((pinfo->xres + pinfo->lcdc.border_left +
			pinfo->lcdc.border_right), dst_bpp, pinfo->bpp);

	itp.yres = pinfo->yres + pinfo->lcdc.border_top +
				pinfo->lcdc.border_bottom;

	itp.h_back_porch = pinfo->lcdc.h_back_porch;
	itp.h_front_porch = pinfo->lcdc.h_front_porch;
	itp.v_back_porch = pinfo->lcdc.v_back_porch;
	itp.v_front_porch = pinfo->lcdc.v_front_porch;
	itp.hsync_pulse_width = pinfo->lcdc.h_pulse_width;
	itp.vsync_pulse_width = pinfo->lcdc.v_pulse_width;
	itp.h_polarity = pinfo->lcdc.h_polarity;
	itp.v_polarity = pinfo->lcdc.v_polarity;

	if (!ctl->panel_data->panel_info.cont_splash_enabled) {
		if (mdss_mdp_video_timegen_setup(ctl, &itp, ctx)) {
			pr_err("unable to set timing parameters intfs: %d\n",
				ctx->intf_num);
			return -EINVAL;
		}
		mdss_mdp_fetch_start_config(ctx, ctl);
	} else {
		mdss_mdp_handoff_programmable_fetch(ctl, ctx);
	}

	mdss_mdp_disable_prefill(ctl);

	mdp_video_write(ctx, MDSS_MDP_REG_INTF_PANEL_FORMAT, ctl->dst_format);
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
			(void *) mode);
		return;
	}

	ctx = (struct mdss_mdp_video_ctx *) ctl->intf_ctx[MASTER_CTX];

	if (!ctx->timegen_en) {
		pr_err("Time engine not enabled, cannot switch from vid\n");
		return;
	}

	/* Start off by sending command to initial cmd mode */
	rc = mdss_mdp_ctl_intf_event(ctl,
		MDSS_EVENT_DSI_DYNAMIC_SWITCH, (void *) mode);
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

	return 0;
}

void *mdss_mdp_get_intf_base_addr(struct mdss_data_type *mdata,
		u32 interface_id)
{
	struct mdss_mdp_video_ctx *ctx;
	ctx = ((struct mdss_mdp_video_ctx *) mdata->video_intf) + interface_id;
	return (void *)(ctx->base);
}
