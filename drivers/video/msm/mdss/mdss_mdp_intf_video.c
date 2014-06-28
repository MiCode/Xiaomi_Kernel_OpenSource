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

#include <linux/iopoll.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/memblock.h>

#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"

/* wait for at least 2 vsyncs for lowest refresh rate (24hz) */
#define VSYNC_TIMEOUT_US 100000

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

	atomic_t vsync_ref;
	spinlock_t vsync_lock;
	struct mutex vsync_mtx;
	struct list_head vsync_handlers;
};

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
	if (!ctl || !ctl->priv_data)
		goto line_count_exit;
	ctx = ctl->priv_data;
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	line_cnt = mdp_video_read(ctx, MDSS_MDP_REG_INTF_LINE_COUNT);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
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

static int mdss_mdp_video_timegen_setup(struct mdss_mdp_ctl *ctl,
					struct intf_timing_params *p)
{
	u32 hsync_period, vsync_period;
	u32 hsync_start_x, hsync_end_x, display_v_start, display_v_end;
	u32 active_h_start, active_h_end, active_v_start, active_v_end;
	u32 den_polarity, hsync_polarity, vsync_polarity;
	u32 display_hctl, active_hctl, hsync_ctl, polarity_ctl;
	struct mdss_mdp_video_ctx *ctx;

	ctx = ctl->priv_data;
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


static inline void video_vsync_irq_enable(struct mdss_mdp_ctl *ctl, bool clear)
{
	struct mdss_mdp_video_ctx *ctx = ctl->priv_data;

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
	struct mdss_mdp_video_ctx *ctx = ctl->priv_data;

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

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
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

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
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

static int mdss_mdp_video_stop(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_vsync_handler *tmp, *handle;
	int rc;
	u32 frame_rate = 0;

	pr_debug("stop ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx for ctl=%d\n", ctl->num);
		return -ENODEV;
	}
	MDSS_XLOG(ctl->num, ctl->vsync_cnt);
	if (ctx->timegen_en) {
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

		mdss_iommu_ctrl(0);
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);
		ctx->timegen_en = false;

		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF, NULL);
		WARN(rc, "intf %d timegen off error (%d)\n", ctl->intf_num, rc);

		mdss_mdp_irq_disable(MDSS_MDP_IRQ_INTF_UNDER_RUN,
			ctl->intf_num);
	}

	list_for_each_entry_safe(handle, tmp, &ctx->vsync_handlers, list)
		mdss_mdp_video_remove_vsync_handler(ctl, handle);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num,
				   NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num,
				   NULL, NULL);

	mdss_mdp_ctl_reset(ctl);
	ctx->ref_cnt--;
	ctl->priv_data = NULL;

	return 0;
}

static void mdss_mdp_video_vsync_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_video_ctx *ctx = ctl->priv_data;
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
	struct mdss_mdp_video_ctx *ctx = ctl->priv_data;
	u32 mask, status;
	int rc;

	mask = MDP_INTR_MASK_INTF_VSYNC(ctl->intf_num);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);
	rc = readl_poll_timeout(ctl->mdata->mdp_base + MDSS_MDP_REG_INTR_STATUS,
		status,
		(status & mask) || try_wait_for_completion(&ctx->vsync_comp),
		1000,
		VSYNC_TIMEOUT_US);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF, false);

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

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	WARN(!ctx->wait_pending, "waiting without commit! ctl=%d", ctl->num);

	if (ctx->polling_en) {
		rc = mdss_mdp_video_pollwait(ctl);
	} else {
		rc = wait_for_completion_timeout(&ctx->vsync_comp,
				usecs_to_jiffies(VSYNC_TIMEOUT_US));
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

static void mdss_mdp_video_underrun_intr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	if (unlikely(!ctl))
		return;

	ctl->underrun_cnt++;
	MDSS_XLOG(ctl->num, ctl->underrun_cnt);
	MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0", "dsi1", "edp", "hdmi", "panic");
	pr_debug("display underrun detected for ctl=%d count=%d\n", ctl->num,
			ctl->underrun_cnt);
}

static int mdss_mdp_video_vfp_fps_update(struct mdss_mdp_ctl *ctl, int new_fps)
{
	int curr_fps;
	u32 add_v_lines = 0;
	u32 current_vsync_period_f0, new_vsync_period_f0;
	struct mdss_panel_data *pdata;
	struct mdss_mdp_video_ctx *ctx;
	u32 vsync_period, hsync_period;

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	pdata = ctl->panel_data;
	if (pdata == NULL) {
		pr_err("%s: Invalid panel data\n", __func__);
		return -EINVAL;
	}

	vsync_period = mdss_panel_get_vtotal(&pdata->panel_info);
	hsync_period = mdss_panel_get_htotal(&pdata->panel_info);
	curr_fps = mdss_panel_get_framerate(&pdata->panel_info);

	if (curr_fps > new_fps) {
		add_v_lines = mult_frac(vsync_period,
				(curr_fps - new_fps), new_fps);
		pdata->panel_info.lcdc.v_front_porch += add_v_lines;
	} else {
		add_v_lines = mult_frac(vsync_period,
				(new_fps - curr_fps), new_fps);
		pdata->panel_info.lcdc.v_front_porch -= add_v_lines;
	}

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

static int mdss_mdp_video_config_fps(struct mdss_mdp_ctl *ctl,
					struct mdss_mdp_ctl *sctl, int new_fps)
{
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_panel_data *pdata;
	int rc = 0;
	u32 hsync_period, vsync_period;

	pr_debug("Updating fps for ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
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
	hsync_period = mdss_panel_get_htotal(&pdata->panel_info);

	if (pdata->panel_info.dfps_update
			!= DFPS_SUSPEND_RESUME_MODE) {
		if (pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_CLK_UPDATE_MODE) {
			if (!ctx->timegen_en) {
				pr_err("TG is OFF. DFPS mode invalid\n");
				return -EINVAL;
			}
			ctl->force_screen_state = MDSS_SCREEN_FORCE_BLANK;
			mdss_mdp_display_commit(ctl, NULL);
			mdss_mdp_display_wait4comp(ctl);
			mdp_video_write(ctx,
					MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 0);
			/*
			 * Need to wait for atleast one vsync time for proper
			 * TG OFF before doing changes on interfaces
			 */
			msleep(20);
			rc = mdss_mdp_ctl_intf_event(ctl,
						MDSS_EVENT_PANEL_UPDATE_FPS,
						(void *)new_fps);
			WARN(rc, "intf %d panel fps update error (%d)\n",
							ctl->intf_num, rc);
			mdp_video_write(ctx,
					MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 1);
			/*
			 * Add memory barrier to make sure the MDP Video
			 * mode engine is enabled before next frame is sent
			 */
			mb();
			ctl->force_screen_state = MDSS_SCREEN_DEFAULT;
			mdss_mdp_display_commit(ctl, NULL);
			mdss_mdp_display_wait4comp(ctl);
		} else if (pdata->panel_info.dfps_update
				== DFPS_IMMEDIATE_PORCH_UPDATE_MODE){
			if (!ctx->timegen_en) {
				pr_err("TG is OFF. DFPS mode invalid\n");
				return -EINVAL;
			}

			video_vsync_irq_enable(ctl, true);
			INIT_COMPLETION(ctx->vsync_comp);
			rc = wait_for_completion_timeout(&ctx->vsync_comp,
				usecs_to_jiffies(VSYNC_TIMEOUT_US));
			WARN(rc <= 0, "timeout (%d) vsync interrupt on ctl=%d\n",
				rc, ctl->num);
			rc = 0;
			video_vsync_irq_disable(ctl);

			rc = mdss_mdp_video_vfp_fps_update(ctl, new_fps);
			if (rc < 0) {
				pr_err("%s: Error during DFPS\n", __func__);
				return rc;
			}
			if (sctl) {
				rc = mdss_mdp_video_vfp_fps_update(sctl,
								new_fps);
				if (rc < 0) {
					pr_err("%s: DFPS error\n", __func__);
					return rc;
				}
			}
			rc = mdss_mdp_ctl_intf_event(ctl,
						MDSS_EVENT_PANEL_UPDATE_FPS,
						(void *)new_fps);
			WARN(rc, "intf %d panel fps update error (%d)\n",
							ctl->intf_num, rc);
		} else {
			pr_err("intf %d panel, unknown FPS mode\n",
							ctl->intf_num);
			return -EINVAL;
		}
	} else {
		rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_PANEL_UPDATE_FPS,
					(void *)new_fps);
		WARN(rc, "intf %d panel fps update error (%d)\n",
						ctl->intf_num, rc);
	}

	return rc;
}

static int mdss_mdp_video_display(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_panel_data *pdata = ctl->panel_data;
	int rc;

	pr_debug("kickoff ctl=%d\n", ctl->num);

	ctx = (struct mdss_mdp_video_ctx *) ctl->priv_data;
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
		rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_UNBLANK, NULL);
		if (rc) {
			pr_warn("intf #%d unblank error (%d)\n",
					ctl->intf_num, rc);
			video_vsync_irq_disable(ctl);
			ctx->wait_pending = 0;
			return rc;
		}

		pr_debug("enabling timing gen for intf=%d\n", ctl->intf_num);

		if (pdata->panel_info.cont_splash_enabled) {
			rc = wait_for_completion_timeout(&ctx->vsync_comp,
					usecs_to_jiffies(VSYNC_TIMEOUT_US));
		}

		rc = mdss_iommu_ctrl(1);
		if (IS_ERR_VALUE(rc)) {
			pr_err("IOMMU attach failed\n");
			return rc;
		}

		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON, false);

		mdss_mdp_irq_enable(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num);
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
	struct mdss_panel_data *pdata = ctl->panel_data;
	int i, ret = 0;
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_data_type *mdata = ctl->mdata;

	i = ctl->intf_num - MDSS_MDP_INTF0;
	if (i < mdata->nintf) {
		ctx = ((struct mdss_mdp_video_ctx *) mdata->video_intf) + i;
		pr_debug("video Intf #%d base=%p", ctx->intf_num, ctx->base);
	} else {
		pr_err("Invalid intf number: %d\n", ctl->intf_num);
		ret = -EINVAL;
		goto error;
	}

	if (!handoff) {
		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_CONT_SPLASH_BEGIN,
					      NULL);
		if (ret) {
			pr_err("%s: Failed to handle 'CONT_SPLASH_BEGIN' event\n"
				, __func__);
			return ret;
		}

		mdss_mdp_ctl_write(ctl, 0, MDSS_MDP_LM_BORDER_COLOR);
		mdp_video_write(ctx, MDSS_MDP_REG_INTF_TIMING_ENGINE_EN, 0);

		/* wait for 1 VSYNC for the pipe to be unstaged */
		msleep(20);

		ret = mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_CONT_SPLASH_FINISH, NULL);
	}

error:
	pdata->panel_info.cont_splash_enabled = 0;
	return ret;
}

int mdss_mdp_video_start(struct mdss_mdp_ctl *ctl)
{
	struct mdss_data_type *mdata;
	struct mdss_panel_info *pinfo;
	struct mdss_mdp_video_ctx *ctx;
	struct mdss_mdp_mixer *mixer;
	struct intf_timing_params itp = {0};
	u32 dst_bpp;
	int i;

	mdata = ctl->mdata;
	pinfo = &ctl->panel_data->panel_info;
	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);

	if (!mixer) {
		pr_err("mixer not setup correctly\n");
		return -ENODEV;
	}

	i = ctl->intf_num - MDSS_MDP_INTF0;
	if (i < mdata->nintf) {
		ctx = ((struct mdss_mdp_video_ctx *) mdata->video_intf) + i;
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

	ctl->priv_data = ctx;
	ctx->intf_type = ctl->intf_type;
	init_completion(&ctx->vsync_comp);
	spin_lock_init(&ctx->vsync_lock);
	mutex_init(&ctx->vsync_mtx);
	atomic_set(&ctx->vsync_ref, 0);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_VSYNC, ctl->intf_num,
				   mdss_mdp_video_vsync_intr_done, ctl);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_INTF_UNDER_RUN, ctl->intf_num,
				   mdss_mdp_video_underrun_intr_done, ctl);

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

	if (mdss_mdp_video_timegen_setup(ctl, &itp)) {
		pr_err("unable to get timing parameters\n");
		return -EINVAL;
	}
	mdp_video_write(ctx, MDSS_MDP_REG_INTF_PANEL_FORMAT, ctl->dst_format);

	ctl->stop_fnc = mdss_mdp_video_stop;
	ctl->display_fnc = mdss_mdp_video_display;
	ctl->wait_fnc = mdss_mdp_video_wait4comp;
	ctl->read_line_cnt_fnc = mdss_mdp_video_line_count;
	ctl->add_vsync_handler = mdss_mdp_video_add_vsync_handler;
	ctl->remove_vsync_handler = mdss_mdp_video_remove_vsync_handler;
	ctl->config_fps_fnc = mdss_mdp_video_config_fps;

	return 0;
}
