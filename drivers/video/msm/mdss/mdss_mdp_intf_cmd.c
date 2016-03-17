/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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
#include <linux/pm_runtime.h>

#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_trace.h"

#define VSYNC_EXPIRE_TICK 6
#define MAX_RECOVERY_TRIALS 10
#define MAX_SESSIONS 2

#define SPLIT_MIXER_OFFSET 0x800

#define STOP_TIMEOUT(hz) msecs_to_jiffies((1000 / hz) * (VSYNC_EXPIRE_TICK + 2))
#define POWER_COLLAPSE_TIME msecs_to_jiffies(100)

static DEFINE_MUTEX(cmd_clk_mtx);

struct mdss_mdp_cmd_ctx {
	struct mdss_mdp_ctl *ctl;
	u32 pp_num;
	u8 ref_cnt;
	struct completion stop_comp;
	struct completion readptr_done;
	wait_queue_head_t pp_waitq;
	struct list_head vsync_handlers;
	int panel_power_state;
	atomic_t koff_cnt;
	u32 intf_stopped;
	int clk_enabled;
	int vsync_enabled;
	int rdptr_enabled;
	struct mutex clk_mtx;
	spinlock_t clk_lock;
	spinlock_t koff_lock;
	struct work_struct clk_work;
	struct work_struct pp_done_work;
	struct mutex autorefresh_mtx;
	atomic_t pp_done_cnt;

	int autorefresh_pending_frame_cnt;
	bool autorefresh_off_pending;
	bool autorefresh_init;

	struct mdss_intf_recovery intf_recovery;
	struct mdss_mdp_cmd_ctx *sync_ctx; /* for partial update */
	u32 pp_timeout_report_cnt;
	int pingpong_split_slave;
};

struct mdss_mdp_cmd_ctx mdss_mdp_cmd_ctx_list[MAX_SESSIONS];

static int mdss_mdp_cmd_do_notifier(struct mdss_mdp_cmd_ctx *ctx);

static bool __mdss_mdp_cmd_is_panel_power_off(struct mdss_mdp_cmd_ctx *ctx)
{
	return mdss_panel_is_power_off(ctx->panel_power_state);
}

static bool __mdss_mdp_cmd_is_panel_power_on_interactive(
		struct mdss_mdp_cmd_ctx *ctx)
{
	return mdss_panel_is_power_on_interactive(ctx->panel_power_state);
}

static inline u32 mdss_mdp_cmd_line_count(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_mixer *mixer;
	u32 cnt = 0xffff;	/* init it to an invalid value */
	u32 init;
	u32 height;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (!mixer) {
		mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
		if (!mixer) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
			goto exit;
		}
	}

	init = mdss_mdp_pingpong_read(mixer->pingpong_base,
		MDSS_MDP_REG_PP_VSYNC_INIT_VAL) & 0xffff;
	height = mdss_mdp_pingpong_read(mixer->pingpong_base,
		MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT) & 0xffff;

	if (height < init) {
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		goto exit;
	}

	cnt = mdss_mdp_pingpong_read(mixer->pingpong_base,
		MDSS_MDP_REG_PP_INT_COUNT_VAL) & 0xffff;

	if (cnt < init)		/* wrap around happened at height */
		cnt += (height - init);
	else
		cnt -= init;

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	pr_debug("cnt=%d init=%d height=%d\n", cnt, init, height);
exit:
	return cnt;
}

static int mdss_mdp_tearcheck_enable(struct mdss_mdp_ctl *ctl, bool enable)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_ctl *sctl;
	struct mdss_mdp_pp_tear_check *te;
	struct mdss_mdp_mixer *mixer =
		mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);

	if (IS_ERR_OR_NULL(ctl->panel_data)) {
		pr_err("no panel data\n");
		return -ENODEV;
	}

	if (IS_ERR_OR_NULL(mixer)) {
		pr_err("mixer not configured\n");
		return -ENODEV;
	}

	sctl = mdss_mdp_get_split_ctl(ctl);
	te = &ctl->panel_data->panel_info.te;

	pr_debug("%s: enable=%d\n", __func__, enable);

	mdss_mdp_pingpong_write(mixer->pingpong_base,
		MDSS_MDP_REG_PP_TEAR_CHECK_EN,
		(te ? te->tear_check_en : 0) && enable);

	/*
	 * When there are two controls, driver needs to enable
	 * tear check configuration for both.
	 */
	if (sctl) {
		mixer = mdss_mdp_mixer_get(sctl, MDSS_MDP_MIXER_MUX_LEFT);
		te = &sctl->panel_data->panel_info.te;
		mdss_mdp_pingpong_write(mixer->pingpong_base,
				MDSS_MDP_REG_PP_TEAR_CHECK_EN,
				(te ? te->tear_check_en : 0) && enable);
	}

	/*
	 * In the case of pingpong split, there is no second
	 * control and enables only slave tear check block as
	 * defined in slave_pingpong_base.
	 */
	if (is_pingpong_split(ctl->mfd))
		mdss_mdp_pingpong_write(mdata->slave_pingpong_base,
				MDSS_MDP_REG_PP_TEAR_CHECK_EN,
				(te ? te->tear_check_en : 0) && enable);
	return 0;
}

static int mdss_mdp_cmd_tearcheck_cfg(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_cmd_ctx *ctx)
{
	struct mdss_mdp_pp_tear_check *te = NULL;
	struct mdss_panel_info *pinfo;
	u32 vsync_clk_speed_hz, total_lines, vclks_line, cfg = 0;
	char __iomem *pingpong_base;
	struct mdss_mdp_ctl *ctl = ctx->ctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (IS_ERR_OR_NULL(ctl->panel_data)) {
		pr_err("no panel data\n");
		return -ENODEV;
	}

	pinfo = &ctl->panel_data->panel_info;
	te = &ctl->panel_data->panel_info.te;

	mdss_mdp_vsync_clk_enable(1);

	vsync_clk_speed_hz =
		mdss_mdp_get_clk_rate(MDSS_CLK_MDP_VSYNC);

	total_lines = mdss_panel_get_vtotal(pinfo);

	total_lines *= pinfo->mipi.frame_rate;

	vclks_line = (total_lines) ? vsync_clk_speed_hz/total_lines : 0;

	cfg = BIT(19);
	if (pinfo->mipi.hw_vsync_mode)
		cfg |= BIT(20);

	if (te->refx100) {
		vclks_line = vclks_line * pinfo->mipi.frame_rate *
			100 / te->refx100;
	} else {
		pr_warn("refx100 cannot be zero! Use 6000 as default\n");
		vclks_line = vclks_line * pinfo->mipi.frame_rate *
			100 / 6000;
	}

	cfg |= vclks_line;

	pr_debug("%s: yres=%d vclks=%x height=%d init=%d rd=%d start=%d\n",
		__func__, pinfo->yres, vclks_line, te->sync_cfg_height,
			te->vsync_init_val, te->rd_ptr_irq, te->start_pos);
	pr_debug("thrd_start =%d thrd_cont=%d\n",
		te->sync_threshold_start, te->sync_threshold_continue);

	pingpong_base = mixer->pingpong_base;

	if (ctx->pingpong_split_slave)
		pingpong_base = mdata->slave_pingpong_base;

	mdss_mdp_pingpong_write(pingpong_base,
		MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);
	mdss_mdp_pingpong_write(pingpong_base,
		MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT,
		te ? te->sync_cfg_height : 0);
	mdss_mdp_pingpong_write(pingpong_base,
		MDSS_MDP_REG_PP_VSYNC_INIT_VAL,
		te ? te->vsync_init_val : 0);
	mdss_mdp_pingpong_write(pingpong_base,
		MDSS_MDP_REG_PP_RD_PTR_IRQ,
		te ? te->rd_ptr_irq : 0);
	mdss_mdp_pingpong_write(pingpong_base,
		MDSS_MDP_REG_PP_START_POS,
		te ? te->start_pos : 0);
	mdss_mdp_pingpong_write(pingpong_base,
		MDSS_MDP_REG_PP_SYNC_THRESH,
		te ? ((te->sync_threshold_continue << 16) |
		 te->sync_threshold_start) : 0);
	mdss_mdp_pingpong_write(pingpong_base,
		MDSS_MDP_REG_PP_SYNC_WRCOUNT,
		te ? (te->start_pos + te->sync_threshold_start + 1) : 0);

	return 0;
}

static int mdss_mdp_cmd_tearcheck_setup(struct mdss_mdp_cmd_ctx *ctx)
{
	int rc = 0;
	struct mdss_mdp_mixer *mixer;
	struct mdss_mdp_ctl *ctl = ctx->ctl;

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (mixer) {
		rc = mdss_mdp_cmd_tearcheck_cfg(mixer, ctx);
		if (rc)
			goto err;
	}

	if (!(ctl->opmode & MDSS_MDP_CTL_OP_PACK_3D_ENABLE)) {
		mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
		if (mixer)
			rc = mdss_mdp_cmd_tearcheck_cfg(mixer, ctx);
	}
err:
	return rc;
}

static void mdss_mdp_cmd_clocks_enable(struct mdss_mdp_ctl *ctl)
{
	int rc;

	pr_debug("%s: %pS\n", __func__, __builtin_return_address(0));

	mdss_bus_bandwidth_ctrl(true);

	rc = mdss_iommu_ctrl(1);
	if (IS_ERR_VALUE(rc))
		pr_err("IOMMU attach failed\n");

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_CLK_CTRL, (void *)1);
}

static void mdss_mdp_cmd_clocks_disable(struct mdss_mdp_ctl *ctl)
{
	pr_debug("%s: %pS\n", __func__, __builtin_return_address(0));

	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_CLK_CTRL, (void *)0);
	mdss_iommu_ctrl(0);
	mdss_bus_bandwidth_ctrl(false);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
}

static inline void mdss_mdp_cmd_clk_on(struct mdss_mdp_cmd_ctx *ctx)
{
	unsigned long flags;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int irq_en;

	if (__mdss_mdp_cmd_is_panel_power_off(ctx))
		return;

	mutex_lock(&ctx->clk_mtx);
	MDSS_XLOG(ctx->pp_num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
						ctx->rdptr_enabled);
	if (!ctx->clk_enabled) {
		mdss_mdp_cmd_clocks_enable(ctx->ctl);
		ctx->clk_enabled = 1;

		mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_RESUME);
	}
	spin_lock_irqsave(&ctx->clk_lock, flags);
	irq_en =  !ctx->rdptr_enabled;
	ctx->rdptr_enabled = VSYNC_EXPIRE_TICK;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (irq_en)
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num);

	mutex_unlock(&ctx->clk_mtx);
}

static inline void mdss_mdp_cmd_clk_off(struct mdss_mdp_cmd_ctx *ctx)
{
	unsigned long flags;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	int set_clk_off = 0;

	if (ctx->autorefresh_init) {
		/* Do not turn off clocks if aurtorefresh is on. */
		return;
	}

	mutex_lock(&ctx->clk_mtx);
	MDSS_XLOG(ctx->pp_num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
						ctx->rdptr_enabled);
	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (!ctx->rdptr_enabled)
		set_clk_off = 1;
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	pr_debug("clk_enabled =%d set_clk_off=%d\n", ctx->clk_enabled,
			set_clk_off);
	if (ctx->clk_enabled && set_clk_off) {
		ctx->clk_enabled = 0;
		mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_SUSPEND);
		mdss_mdp_cmd_clocks_disable(ctx->ctl);
	}
	mutex_unlock(&ctx->clk_mtx);
}

static void mdss_mdp_cmd_readptr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_vsync_handler *tmp;
	ktime_t vsync_time;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	if (ctx->autorefresh_init) {
		pr_debug("Completing read pointer done\n");
		complete_all(&ctx->readptr_done);
	}

	if (ctx->autorefresh_off_pending)
		ctx->autorefresh_off_pending = false;

	vsync_time = ktime_get();
	ctl->vsync_cnt++;
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
				ctx->rdptr_enabled);

	spin_lock(&ctx->clk_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		if (tmp->enabled && !tmp->cmd_post_flush)
			tmp->vsync_handler(ctl, vsync_time);
	}

	if (!ctx->vsync_enabled) {
		if (ctx->rdptr_enabled && !ctx->autorefresh_init)
			ctx->rdptr_enabled--;

		/* keep clk on during kickoff */
		if (ctx->rdptr_enabled == 0 && atomic_read(&ctx->koff_cnt))
			ctx->rdptr_enabled++;
	}

	if (ctx->rdptr_enabled == 0) {
		mdss_mdp_irq_disable_nosync
			(MDSS_MDP_IRQ_PING_PONG_RD_PTR, ctx->pp_num);
		complete(&ctx->stop_comp);
		schedule_work(&ctx->clk_work);
	}

	spin_unlock(&ctx->clk_lock);
}

static void mdss_mdp_cmd_intf_recovery(void *data, int event)
{
	struct mdss_mdp_cmd_ctx *ctx = data;
	unsigned long flags;
	bool reset_done = false;

	if (!data) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	if (!ctx->ctl)
		return;

	/*
	 * Currently, only intf_fifo_underflow is
	 * supported for recovery sequence for command
	 * mode DSI interface
	 */
	if (event != MDP_INTF_DSI_CMD_FIFO_UNDERFLOW) {
		pr_warn("%s: unsupported recovery event:%d\n",
					__func__, event);
		return;
	}

	if (atomic_read(&ctx->koff_cnt)) {
		mdss_mdp_ctl_reset(ctx->ctl);
		reset_done = true;
	}

	spin_lock_irqsave(&ctx->koff_lock, flags);
	if (reset_done && atomic_read(&ctx->koff_cnt)) {
		pr_debug("%s: intf_num=%d\n", __func__,
					ctx->ctl->intf_num);
		atomic_dec(&ctx->koff_cnt);
		mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_PING_PONG_COMP,
						ctx->pp_num);
	}
	spin_unlock_irqrestore(&ctx->koff_lock, flags);
}

static void mdss_mdp_cmd_pingpong_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_vsync_handler *tmp;
	ktime_t vsync_time;
	u32 status;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	mdss_mdp_ctl_perf_set_transaction_status(ctl,
		PERF_HW_MDP_STATE, PERF_STATUS_DONE);

	spin_lock(&ctx->clk_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		if (tmp->enabled && tmp->cmd_post_flush)
			tmp->vsync_handler(ctl, vsync_time);
	}
	spin_unlock(&ctx->clk_lock);

	spin_lock(&ctx->koff_lock);
	mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
					ctx->rdptr_enabled);

	if (atomic_add_unless(&ctx->koff_cnt, -1, 0)) {
		if (atomic_read(&ctx->koff_cnt))
			pr_err("%s: too many kickoffs=%d!\n", __func__,
			       atomic_read(&ctx->koff_cnt));
		if (mdss_mdp_cmd_do_notifier(ctx)) {
			atomic_inc(&ctx->pp_done_cnt);
			status = mdss_mdp_ctl_perf_get_transaction_status(ctl);
			if (status == 0)
				schedule_work(&ctx->pp_done_work);
		}
		wake_up_all(&ctx->pp_waitq);
	} else if (!ctl->cmd_autorefresh_en) {
		pr_err("%s: should not have pingpong interrupt!\n", __func__);
	}

	trace_mdp_cmd_pingpong_done(ctl, ctx->pp_num,
					atomic_read(&ctx->koff_cnt));
	pr_debug("%s: ctl_num=%d intf_num=%d ctx=%d kcnt=%d\n", __func__,
			ctl->num, ctl->intf_num, ctx->pp_num,
				atomic_read(&ctx->koff_cnt));

	spin_unlock(&ctx->koff_lock);
}

static void pingpong_done_work(struct work_struct *work)
{
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), pp_done_work);

	if (ctx->ctl) {
		while (atomic_add_unless(&ctx->pp_done_cnt, -1, 0))
			mdss_mdp_ctl_notify(ctx->ctl, MDP_NOTIFY_FRAME_DONE);

		mdss_mdp_ctl_perf_release_bw(ctx->ctl);
	}
}

static void clk_ctrl_work(struct work_struct *work)
{
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), clk_work);
	struct mdss_mdp_ctl *ctl, *sctl;
	struct mdss_mdp_cmd_ctx *sctx = NULL;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	ctl = ctx->ctl;

	if (ctl->panel_data->panel_info.is_split_display) {
		mutex_lock(&cmd_clk_mtx);

		sctl = mdss_mdp_get_split_ctl(ctl);
		if (sctl) {
			sctx =
			(struct mdss_mdp_cmd_ctx *)sctl->intf_ctx[MASTER_CTX];
		} else {
			/* slave ctl, let master ctl do clk control */
			mutex_unlock(&cmd_clk_mtx);
			return;
		}
	}

	mdss_mdp_cmd_clk_off(ctx);

	if (ctl->panel_data->panel_info.is_split_display) {
		if (sctx)
			mdss_mdp_cmd_clk_off(sctx);
		mutex_unlock(&cmd_clk_mtx);
	}
}

static int mdss_mdp_cmd_add_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_ctl *sctl = NULL;
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;
	unsigned long flags;
	bool enable_rdptr = false;
	int ret = 0;

	mutex_lock(&ctl->offlock);
	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		ret = -ENODEV;
		goto done;
	}

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
					ctx->rdptr_enabled);
	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->vsync_handlers);

		enable_rdptr = !handle->cmd_post_flush;
		if (enable_rdptr) {
			ctx->vsync_enabled++;
			if (sctx)
				sctx->vsync_enabled++;
		}
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (enable_rdptr) {
		if (ctl->panel_data->panel_info.is_split_display)
			mutex_lock(&cmd_clk_mtx);

		mdss_mdp_cmd_clk_on(ctx);
		if (sctx)
			mdss_mdp_cmd_clk_on(sctx);

		if (ctl->panel_data->panel_info.is_split_display)
			mutex_unlock(&cmd_clk_mtx);
	}

done:
	mutex_unlock(&ctl->offlock);

	return ret;
}

static int mdss_mdp_cmd_remove_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;
	unsigned long flags;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -ENODEV;
	}

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
				ctx->rdptr_enabled, 0x88888);
	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);

		if (!handle->cmd_post_flush) {
			if (ctx->vsync_enabled) {
				ctx->vsync_enabled--;
				if (sctx)
					sctx->vsync_enabled--;
			}
			else
				WARN(1, "unbalanced vsync disable");
		}
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);
	return 0;
}

int mdss_mdp_cmd_reconfigure_splash_done(struct mdss_mdp_ctl *ctl, bool handoff)
{
	struct mdss_panel_data *pdata;
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	int ret = 0;

	pdata = ctl->panel_data;

	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_CLK_CTRL, (void *)0);

	pdata->panel_info.cont_splash_enabled = 0;
	if (sctl)
		sctl->panel_data->panel_info.cont_splash_enabled = 0;
	else if (pdata->next && is_pingpong_split(ctl->mfd))
		pdata->next->panel_info.cont_splash_enabled = 0;

	return ret;
}

static int mdss_mdp_cmd_wait4pingpong(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_panel_data *pdata;
	unsigned long flags;
	int rc = 0;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	pdata = ctl->panel_data;

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
			ctx->rdptr_enabled, ctl->roi_bkup.w,
			ctl->roi_bkup.h);

	pr_debug("%s: intf_num=%d ctx=%p koff_cnt=%d\n", __func__,
			ctl->intf_num, ctx, atomic_read(&ctx->koff_cnt));

	rc = wait_event_timeout(ctx->pp_waitq,
			atomic_read(&ctx->koff_cnt) == 0,
			KOFF_TIMEOUT);

	trace_mdp_cmd_wait_pingpong(ctl->num,
				atomic_read(&ctx->koff_cnt));

	if (rc <= 0) {
		u32 status, mask;

		mask = BIT(MDSS_MDP_IRQ_PING_PONG_COMP + ctx->pp_num);
		status = mask & readl_relaxed(ctl->mdata->mdp_base +
				MDSS_MDP_REG_INTR_STATUS);
		if (status) {
			pr_warn("pp done but irq not triggered\n");
			mdss_mdp_irq_clear(ctl->mdata,
					MDSS_MDP_IRQ_PING_PONG_COMP,
					ctx->pp_num);
			local_irq_save(flags);
			mdss_mdp_cmd_pingpong_done(ctl);
			local_irq_restore(flags);
			rc = 1;
		}

		rc = atomic_read(&ctx->koff_cnt) == 0;
	}

	if (rc <= 0) {
		pr_err("cmd kickoff timed out (%d) ctl=%d, cnt=%d\n",
			rc, ctl->num, ctx->pp_timeout_report_cnt);
		if (ctx->pp_timeout_report_cnt == 0) {
			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl", "dsi0_phy",
				"dsi1_ctrl", "dsi1_phy", "panic");
		} else if (ctx->pp_timeout_report_cnt == MAX_RECOVERY_TRIALS) {
			pr_err("timeout recovery seq failed(%d) ctl=%d, cnt=%d\n",
				rc, ctl->num, ctx->pp_timeout_report_cnt);
			mdss_fb_report_panel_dead(ctl->mfd);
		}
		ctx->pp_timeout_report_cnt++;
		rc = -EPERM;
		mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_TIMEOUT);
		atomic_add_unless(&ctx->koff_cnt, -1, 0);
	} else {
		rc = 0;
		ctx->pp_timeout_report_cnt = 0;
	}

	cancel_work_sync(&ctx->pp_done_work);

	/* signal any pending ping pong done events */
	while (atomic_add_unless(&ctx->pp_done_cnt, -1, 0))
		mdss_mdp_ctl_notify(ctx->ctl, MDP_NOTIFY_FRAME_DONE);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
			ctx->rdptr_enabled, rc);

	return rc;
}

static int mdss_mdp_cmd_do_notifier(struct mdss_mdp_cmd_ctx *ctx)
{
	struct mdss_mdp_cmd_ctx *sctx;

	sctx = ctx->sync_ctx;
	if (!sctx || atomic_read(&sctx->koff_cnt) == 0)
		return 1;

	return 0;
}

static void mdss_mdp_cmd_set_sync_ctx(
		struct mdss_mdp_ctl *ctl, struct mdss_mdp_ctl *sctl)
{
	struct mdss_mdp_cmd_ctx *ctx, *sctx;

	ctx = (struct mdss_mdp_cmd_ctx *)ctl->intf_ctx[MASTER_CTX];

	if (!sctl) {
		ctx->sync_ctx = NULL;
		return;
	}

	sctx = (struct mdss_mdp_cmd_ctx *)sctl->intf_ctx[MASTER_CTX];

	if (!sctl->roi.w && !sctl->roi.h) {
		/* left only */
		ctx->sync_ctx = NULL;
		sctx->sync_ctx = NULL;
	} else  {
		/* left + right */
		ctx->sync_ctx = sctx;
		sctx->sync_ctx = ctx;
	}
}

static int mdss_mdp_cmd_set_partial_roi(struct mdss_mdp_ctl *ctl)
{
	int rc = 0;

	if (!ctl->panel_data->panel_info.partial_update_supported)
		return rc;

	/* set panel col and page addr */
	rc = mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_ENABLE_PARTIAL_ROI, NULL);
	return rc;
}

static int mdss_mdp_cmd_set_stream_size(struct mdss_mdp_ctl *ctl)
{
	int rc = 0;

	if (!ctl->panel_data->panel_info.partial_update_supported)
		return rc;

	/* set dsi controller stream size */
	rc = mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_DSI_STREAM_SIZE, NULL);
	return rc;
}

static int mdss_mdp_cmd_panel_on(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_ctl *sctl)
{
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;
	int rc = 0;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	if (!__mdss_mdp_cmd_is_panel_power_on_interactive(ctx)) {
		if (ctl->pending_mode_switch != SWITCH_RESOLUTION) {
			rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_LINK_READY, NULL);
			WARN(rc, "intf %d link ready error (%d)\n",
					ctl->intf_num, rc);

			rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_UNBLANK, NULL);
			WARN(rc, "intf %d unblank error (%d)\n",
					ctl->intf_num, rc);

			rc = mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_PANEL_ON, NULL);
			WARN(rc, "intf %d panel on error (%d)\n",
					ctl->intf_num, rc);

		}

		rc = mdss_mdp_tearcheck_enable(ctl, true);
		WARN(rc, "intf %d tearcheck enable error (%d)\n",
				ctl->intf_num, rc);

		ctx->panel_power_state = MDSS_PANEL_POWER_ON;
		if (sctx)
			sctx->panel_power_state = MDSS_PANEL_POWER_ON;

		mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_REGISTER_RECOVERY_HANDLER,
			(void *)&ctx->intf_recovery);

		ctx->intf_stopped = 0;
	} else {
		pr_err("%s: Panel already on\n", __func__);
	}

	return rc;
}

static int __mdss_mdp_cmd_configure_autorefresh(struct mdss_mdp_ctl *ctl, int
		frame_cnt, bool delayed)
{
	struct mdss_mdp_cmd_ctx *ctx;
	bool enable = frame_cnt ? true : false;

	if (!ctl || !ctl->mixer_left) {
		pr_err("invalid ctl structure\n");
		return -ENODEV;
	}
	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (frame_cnt == ctl->autorefresh_frame_cnt) {
		pr_debug("No change to the refresh count\n");
		return 0;
	}
	pr_debug("%s enable = %d frame_cnt = %d init=%d\n", __func__,
			enable, frame_cnt, ctx->autorefresh_init);

	mutex_lock(&ctx->autorefresh_mtx);

	if (enable) {
		if (delayed) {
			ctx->autorefresh_pending_frame_cnt = frame_cnt;
		} else {
			if (!ctx->autorefresh_init) {
				ctx->autorefresh_init = true;
				mdss_mdp_cmd_clk_on(ctx);
			}
			mdss_mdp_pingpong_write(ctl->mixer_left->pingpong_base,
					MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG,
					BIT(31) | frame_cnt);
			/*
			 * This manual kickoff is needed to actually start the
			 * autorefresh feature. The h/w relies on one commit
			 * before it starts counting the read ptrs to trigger
			 * the frames.
			 */
			mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
			ctl->autorefresh_frame_cnt = frame_cnt;
			ctl->cmd_autorefresh_en = true;
		}
	} else {
		if (ctx->autorefresh_init) {
			/*
			 * Safe to turn off the feature. The clocks will be on
			 * at this time since the feature was enabled.
			 */

			mdss_mdp_pingpong_write(ctl->mixer_left->pingpong_base,
					MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG, 0);
		}

		ctx->autorefresh_init = false;
		ctx->autorefresh_pending_frame_cnt = 0;
		ctx->autorefresh_off_pending = true;

		ctl->autorefresh_frame_cnt = 0;
		ctl->cmd_autorefresh_en = false;
	}

	mutex_unlock(&ctx->autorefresh_mtx);

	return 0;
}

/*
 * This function will be called from the sysfs node to enable and disable the
 * feature.
 */
int mdss_mdp_cmd_set_autorefresh_mode(struct mdss_mdp_ctl *ctl, int frame_cnt)
{
	return __mdss_mdp_cmd_configure_autorefresh(ctl, frame_cnt, true);
}

/*
 * This function is called from the commit thread. This function will check if
 * there was are any pending requests from the sys fs node for the feature and
 * if so then it will enable in the h/w.
 */
static int mdss_mdp_cmd_enable_cmd_autorefresh(struct mdss_mdp_ctl *ctl,
	int frame_cnt)
{
	return __mdss_mdp_cmd_configure_autorefresh(ctl, frame_cnt, false);
}

/*
 * There are 3 partial update possibilities
 * left only ==> enable left pingpong_done
 * left + right ==> enable both pingpong_done
 * right only ==> enable right pingpong_done
 *
 * notification is triggered at pingpong_done which will
 * signal timeline to release source buffer
 *
 * for left+right case, pingpong_done is enabled for both and
 * only the last pingpong_done should trigger the notification
 */
int mdss_mdp_cmd_kickoff(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_ctl *sctl = NULL;
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (ctx->intf_stopped) {
		pr_err("ctx=%d stopped already\n", ctx->pp_num);
		return -EPERM;
	}

	INIT_COMPLETION(ctx->readptr_done);
	/* sctl will be null for right only in the case of Partial update */
	sctl = mdss_mdp_get_split_ctl(ctl);

	if (sctl && (sctl->roi.w == 0 || sctl->roi.h == 0)) {
		/* left update only */
		sctl = NULL;
	}

	mdss_mdp_ctl_perf_set_transaction_status(ctl,
		PERF_HW_MDP_STATE, PERF_STATUS_BUSY);

	if (sctl) {
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];
		mdss_mdp_ctl_perf_set_transaction_status(sctl,
			PERF_HW_MDP_STATE, PERF_STATUS_BUSY);
	}

	/*
	 * Turn on the panel, if not already. This is because the panel is
	 * turned on only when we send the first frame and not during cmd
	 * start. This is to ensure that no artifacts are seen on the panel.
	 */
	if (__mdss_mdp_cmd_is_panel_power_off(ctx))
		mdss_mdp_cmd_panel_on(ctl, sctl);

	MDSS_XLOG(ctl->num, ctl->roi.x, ctl->roi.y, ctl->roi.w,
						ctl->roi.h);

	atomic_inc(&ctx->koff_cnt);
	if (sctx)
		atomic_inc(&sctx->koff_cnt);

	trace_mdp_cmd_kickoff(ctl->num, atomic_read(&ctx->koff_cnt));

	mdss_mdp_cmd_clk_on(ctx);

	mdss_mdp_cmd_set_partial_roi(ctl);

	/*
	 * tx dcs command if had any
	 */
	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_CMDLIST_KOFF, NULL);

	mdss_mdp_cmd_set_stream_size(ctl);

	mdss_mdp_cmd_set_sync_ctx(ctl, sctl);

	if (ctx->autorefresh_init || ctx->autorefresh_off_pending) {
		/*
		 * If autorefresh is enabled then do not queue the frame till
		 * the next read ptr is done otherwise we might get a pp done
		 * immediately for the past autorefresh frame instead.
		 */
		pr_debug("Wait for read pointer done before enabling PP irq\n");
		wait_for_completion(&ctx->readptr_done);
		mdss_mdp_cmd_clk_on(ctx);
	}

	mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num);
	if (sctx)
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_PING_PONG_COMP, sctx->pp_num);

	if (!ctx->autorefresh_pending_frame_cnt && !ctl->cmd_autorefresh_en) {
		/* Kickoff */
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
	} else {
		pr_debug("Enabling autorefresh in hardware.\n");
		mdss_mdp_cmd_enable_cmd_autorefresh(ctl,
				ctx->autorefresh_pending_frame_cnt);
	}

	mdss_mdp_ctl_perf_set_transaction_status(ctl,
		PERF_SW_COMMIT_STATE, PERF_STATUS_DONE);

	if (sctl) {
		mdss_mdp_ctl_perf_set_transaction_status(sctl,
			PERF_SW_COMMIT_STATE, PERF_STATUS_DONE);
	}

	mb();
	MDSS_XLOG(ctl->num,  atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
						ctx->rdptr_enabled);
	return 0;
}

int mdss_mdp_cmd_restore(struct mdss_mdp_ctl *ctl)
{
	pr_debug("%s: called for ctl%d\n", __func__, ctl->num);
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
	if (mdss_mdp_cmd_tearcheck_setup(ctl->intf_ctx[MASTER_CTX]))
		pr_warn("%s: tearcheck setup failed\n", __func__);
	else
		mdss_mdp_tearcheck_enable(ctl, true);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	return 0;
}

int mdss_mdp_cmd_ctx_stop(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_cmd_ctx *ctx, int panel_power_state)
{
	struct mdss_mdp_cmd_ctx *sctx = NULL;
	struct mdss_mdp_ctl *sctl = NULL;
	unsigned long flags;
	unsigned long sflags;
	int need_wait = 0;
	int hz;

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	/* intf stopped,  no more kickoff */
	ctx->intf_stopped = 1;
	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (ctx->rdptr_enabled) {
		INIT_COMPLETION(ctx->stop_comp);
		need_wait = 1;
		/*
		 * clk off at next vsync after pp_done  OR
		 * next vsync if there has no kickoff pending
		 */
		ctx->rdptr_enabled = 1;
		if (sctx) {
			spin_lock_irqsave(&sctx->clk_lock, sflags);
			if (sctx->rdptr_enabled)
				sctx->rdptr_enabled = 1;
			spin_unlock_irqrestore(&sctx->clk_lock, sflags);
		}
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (need_wait) {
		hz = mdss_panel_get_framerate(&ctl->panel_data->panel_info);
		if (wait_for_completion_timeout(&ctx->stop_comp,
			STOP_TIMEOUT(hz)) <= 0) {
			WARN(1, "stop cmd time out\n");
			mdss_mdp_irq_disable(MDSS_MDP_IRQ_PING_PONG_RD_PTR,
				ctx->pp_num);
			ctx->rdptr_enabled = 0;
			atomic_set(&ctx->koff_cnt, 0);
		}
	}

	if (cancel_work_sync(&ctx->clk_work))
		pr_debug("no pending clk work\n");

	if (!ctl->pending_mode_switch) {
		mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_REGISTER_RECOVERY_HANDLER,
			NULL);
	}

	mdss_mdp_cmd_clk_off(ctx);
	flush_work(&ctx->pp_done_work);
	mdss_mdp_tearcheck_enable(ctl, false);

	if (mdss_panel_is_power_on(panel_power_state)) {
		pr_debug("%s: intf stopped with panel on\n", __func__);
		return 0;
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_RD_PTR,
		ctx->pp_num, NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP,
		ctx->pp_num, NULL, NULL);

	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

int mdss_mdp_cmd_intfs_stop(struct mdss_mdp_ctl *ctl, int session,
	int panel_power_state)
{
	struct mdss_mdp_cmd_ctx *ctx;

	if (session >= MAX_SESSIONS)
		return 0;

	ctx = ctl->intf_ctx[MASTER_CTX];
	if (!ctx->ref_cnt) {
		pr_err("invalid ctx session: %d\n", session);
		return -ENODEV;
	}

	mdss_mdp_cmd_ctx_stop(ctl, ctx, panel_power_state);

	if (is_pingpong_split(ctl->mfd)) {
		session += 1;

		if (session >= MAX_SESSIONS)
			return 0;

		ctx = ctl->intf_ctx[SLAVE_CTX];
		if (!ctx->ref_cnt) {
			pr_err("invalid ctx session: %d\n", session);
			return -ENODEV;
		}
		mdss_mdp_cmd_ctx_stop(ctl, ctx, panel_power_state);
	}
	pr_debug("%s:-\n", __func__);
	return 0;
}

static int mdss_mdp_cmd_stop_sub(struct mdss_mdp_ctl *ctl,
		int panel_power_state)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_mdp_vsync_handler *tmp, *handle;
	int session;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	list_for_each_entry_safe(handle, tmp, &ctx->vsync_handlers, list)
		mdss_mdp_cmd_remove_vsync_handler(ctl, handle);
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
				ctx->rdptr_enabled, XLOG_FUNC_ENTRY);

	/* Command mode is supported only starting at INTF1 */
	session = ctl->intf_num - MDSS_MDP_INTF1;
	return mdss_mdp_cmd_intfs_stop(ctl, session, panel_power_state);
}

int mdss_mdp_cmd_stop(struct mdss_mdp_ctl *ctl, int panel_power_state)
{
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	bool panel_off = false;
	bool turn_off_clocks = false;
	bool send_panel_events = false;
	int ret = 0;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (__mdss_mdp_cmd_is_panel_power_off(ctx)) {
		pr_debug("%s: panel already off\n", __func__);
		return 0;
	}

	if (ctx->panel_power_state == panel_power_state) {
		pr_debug("%s: no transition needed %d --> %d\n", __func__,
			ctx->panel_power_state, panel_power_state);
		return 0;
	}

	pr_debug("%s: transition from %d --> %d\n", __func__,
		ctx->panel_power_state, panel_power_state);

	if (ctl->cmd_autorefresh_en) {
		int pre_suspend = ctx->autorefresh_pending_frame_cnt;
		mdss_mdp_cmd_enable_cmd_autorefresh(ctl, 0);
		ctx->autorefresh_pending_frame_cnt = pre_suspend;
	}

	mutex_lock(&ctl->offlock);
	if (mdss_panel_is_power_off(panel_power_state)) {
		/* Transition to display off */
		send_panel_events = true;
		turn_off_clocks = true;
		panel_off = true;
	} else if (__mdss_mdp_cmd_is_panel_power_on_interactive(ctx)) {
		/*
		 * If we are transitioning from interactive to low
		 * power, then we need to send events to the interface
		 * so that the panel can be configured in low power
		 * mode.
		 */
		send_panel_events = true;
		if (mdss_panel_is_power_on_ulp(panel_power_state))
			turn_off_clocks = true;
	} else {
		/* Transitions between low power and ultra low power */
		if (mdss_panel_is_power_on_ulp(panel_power_state)) {
			/*
			 * If we are transitioning from low power to ultra low
			 * power mode, no more display updates are expected.
			 * Turn off the interface clocks.
			 */
			pr_debug("%s: turn off clocks\n", __func__);
			turn_off_clocks = true;
		} else {
			/*
			 * Transition from ultra low power to low power does
			 * not require any special handling. Just rest the
			 * intf_stopped flag so that the clocks would
			 * get turned on when the first update comes.
			 */
			pr_debug("%s: reset intf_stopped flag.\n", __func__);
			ctx->intf_stopped = 0;
			goto end;
		}
	}

	if (!turn_off_clocks)
		goto panel_events;

	if (ctl->pending_mode_switch)
		send_panel_events = false;

	pr_debug("%s: turn off interface clocks\n", __func__);
	ret = mdss_mdp_cmd_stop_sub(ctl, panel_power_state);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s: unable to stop interface: %d\n",
				__func__, ret);
		goto end;
	}

	if (sctl) {
		mdss_mdp_cmd_stop_sub(sctl, panel_power_state);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s: unable to stop slave intf: %d\n",
					__func__, ret);
			goto end;
		}
	}

panel_events:
	if ((ctl->num == 0) && send_panel_events) {
		pr_debug("%s: send panel events\n", __func__);
		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK,
				(void *) (long int) panel_power_state);
		WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);

		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF,
				(void *) (long int) panel_power_state);
		WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);
	}


	if (!panel_off) {
		pr_debug("%s: cmd_stop with panel always on\n", __func__);
		goto end;
	}

	pr_debug("%s: turn off panel\n", __func__);
	ctl->intf_ctx[MASTER_CTX] = NULL;
	ctl->intf_ctx[SLAVE_CTX] = NULL;
	ctl->ops.stop_fnc = NULL;
	ctl->ops.display_fnc = NULL;
	ctl->ops.wait_pingpong = NULL;
	ctl->ops.add_vsync_handler = NULL;
	ctl->ops.remove_vsync_handler = NULL;
	ctl->ops.reconfigure = NULL;

end:
	if (!IS_ERR_VALUE(ret))
		ctx->panel_power_state = panel_power_state;
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
				ctx->rdptr_enabled, XLOG_FUNC_EXIT);
	mutex_unlock(&ctl->offlock);
	pr_debug("%s:-\n", __func__);

	return ret;
}

static int mdss_mdp_cmd_ctx_setup(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_cmd_ctx *ctx, int pp_num,
	int pingpong_split_slave)
{
	int ret = 0;

	ctx->ctl = ctl;
	ctx->pp_num = pp_num;
	ctx->pingpong_split_slave = pingpong_split_slave;
	ctx->pp_timeout_report_cnt = 0;
	init_waitqueue_head(&ctx->pp_waitq);
	init_completion(&ctx->stop_comp);
	init_completion(&ctx->readptr_done);
	spin_lock_init(&ctx->clk_lock);
	spin_lock_init(&ctx->koff_lock);
	mutex_init(&ctx->clk_mtx);
	mutex_init(&ctx->autorefresh_mtx);
	INIT_WORK(&ctx->clk_work, clk_ctrl_work);
	INIT_WORK(&ctx->pp_done_work, pingpong_done_work);
	atomic_set(&ctx->pp_done_cnt, 0);
	ctx->autorefresh_off_pending = false;
	ctx->autorefresh_init = false;
	INIT_LIST_HEAD(&ctx->vsync_handlers);

	ctx->intf_recovery.fxn = mdss_mdp_cmd_intf_recovery;
	ctx->intf_recovery.data = ctx;

	ctx->intf_stopped = 0;

	pr_debug("%s: ctx=%p num=%d\n", __func__, ctx, ctx->pp_num);
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->clk_enabled,
					ctx->rdptr_enabled);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_RD_PTR,
		ctx->pp_num, mdss_mdp_cmd_readptr_done, ctl);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_PING_PONG_COMP, ctx->pp_num,
				   mdss_mdp_cmd_pingpong_done, ctl);

	ret = mdss_mdp_cmd_tearcheck_setup(ctx);
	if (ret)
		pr_err("tearcheck setup failed\n");

	return ret;
}

static int mdss_mdp_cmd_intfs_setup(struct mdss_mdp_ctl *ctl,
			int session)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_mdp_ctl *sctl = NULL;
	struct mdss_mdp_mixer *mixer;
	int ret;

	if (session >= MAX_SESSIONS)
		return 0;

	sctl = mdss_mdp_get_split_ctl(ctl);
	ctx = &mdss_mdp_cmd_ctx_list[session];
	if (ctx->ref_cnt) {
		if (mdss_panel_is_power_on(ctx->panel_power_state)) {
			pr_debug("%s: cmd_start with panel always on\n",
				__func__);
			/*
			 * It is possible that the resume was called from the
			 * panel always on state without MDSS every
			 * power-collapsed (such as a case with any other
			 * interfaces connected). In such cases, we need to
			 * explictly call the restore function to enable
			 * tearcheck logic.
			 */
			mdss_mdp_cmd_restore(ctl);

			/* Turn on panel so that it can exit low power mode */
			return mdss_mdp_cmd_panel_on(ctl, sctl);
		} else {
			pr_err("Intf %d already in use\n", session);
			return -EBUSY;
		}
	}
	ctx->ref_cnt++;

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (!mixer) {
		pr_err("mixer not setup correctly\n");
		return -ENODEV;
	}

	ctl->intf_ctx[MASTER_CTX] = ctx;

	ret = mdss_mdp_cmd_ctx_setup(ctl, ctx, is_pingpong_split(ctl->mfd) ? session : mixer->num, false);
	if (ret) {
		pr_err("mdss_mdp_cmd_ctx_setup failed for ping ping: %d\n",
				mixer->num);
		ctx->ref_cnt--;
		return -ENODEV;
	}

	if (is_pingpong_split(ctl->mfd)) {
		session += 1;
		if (session >= MAX_SESSIONS)
			return 0;
		ctx = &mdss_mdp_cmd_ctx_list[session];
		if (ctx->ref_cnt) {
			if (mdss_panel_is_power_on(ctx->panel_power_state)) {
				pr_debug("%s: cmd_start with panel always on\n",
						__func__);
				mdss_mdp_cmd_restore(ctl);
				return mdss_mdp_cmd_panel_on(ctl, sctl);
			} else {
				pr_err("Intf %d already in use\n", session);
				return -EBUSY;
			}
		}
		ctx->ref_cnt++;

		ctl->intf_ctx[SLAVE_CTX] = ctx;

		ret = mdss_mdp_cmd_ctx_setup(ctl, ctx, session, true);
		if (ret) {
			pr_err("mdss_mdp_cmd_ctx_setup failed for slave ping pong block");
			ctx->ref_cnt--;
			return -EPERM;
		}
	}
	return 0;
}

void mdss_mdp_switch_roi_reset(struct mdss_mdp_ctl *ctl)
{
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

	if (!ctl->panel_data ||
	  !ctl->panel_data->panel_info.partial_update_supported)
		return;

	ctl->panel_data->panel_info.roi = ctl->roi;
	if (sctl && sctl->panel_data)
		sctl->panel_data->panel_info.roi = sctl->roi;

	mdss_mdp_cmd_set_partial_roi(ctl);
}

void mdss_mdp_switch_to_vid_mode(struct mdss_mdp_ctl *ctl, int prep)
{
	long int mode = MIPI_VIDEO_PANEL;
	int rc = 0;

	pr_debug("%s start, prep = %d\n", __func__, prep);

	if (prep) {
		/*
		 * In dsi_on there is an explicit decrement to dsi clk refcount
		 * if we are in cmd mode. We need to rebalance clock in order
		 * to properly enable vid mode compnents
		 */
		rc = mdss_mdp_ctl_intf_event
			(ctl, MDSS_EVENT_PANEL_CLK_CTRL, (void *)1);

		return;
	}

	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_RECONFIG_CMD,
			(void *) mode);
}

static int mdss_mdp_cmd_reconfigure(struct mdss_mdp_ctl *ctl,
		enum dynamic_switch_modes mode, bool prep)
{
	int ret;

	if (mdss_mdp_ctl_is_power_off(ctl))
		return 0;

	pr_debug("%s: ctl=%d mode=%d prep=%d\n", __func__,
			ctl->num, mode, prep);

	if (mode == SWITCH_TO_VIDEO_MODE) {
		mdss_mdp_switch_to_vid_mode(ctl, prep);
	} else if (mode == SWITCH_RESOLUTION) {
		if (prep) {
			/* make sure any pending transfer is finished */
			ret = mdss_mdp_cmd_wait4pingpong(ctl, NULL);
			if (ret)
				return ret;

			/*
			 * keep a ref count on clocks to prevent them from
			 * being disabled while switch happens
			 */
			mdss_mdp_cmd_clocks_enable(ctl);
			mdss_mdp_ctl_stop(ctl, MDSS_PANEL_POWER_OFF);
			mdss_mdp_ctl_intf_event(ctl,
					MDSS_EVENT_DSI_DYNAMIC_SWITCH,
					(void *) mode);
		} else {
			/* release ref count after switch is complete */
			mdss_mdp_cmd_clocks_disable(ctl);
		}
	}

	return 0;
}

int mdss_mdp_cmd_start(struct mdss_mdp_ctl *ctl)
{
	int ret, session = 0;

	pr_debug("%s:+\n", __func__);

	/* Command mode is supported only starting at INTF1 */
	session = ctl->intf_num - MDSS_MDP_INTF1;
	ret = mdss_mdp_cmd_intfs_setup(ctl, session);
	if (IS_ERR_VALUE(ret)) {
		pr_err("unable to set cmd interface: %d\n", ret);
		return ret;
	}

	ctl->ops.stop_fnc = mdss_mdp_cmd_stop;
	ctl->ops.display_fnc = mdss_mdp_cmd_kickoff;
	ctl->ops.wait_pingpong = mdss_mdp_cmd_wait4pingpong;
	ctl->ops.add_vsync_handler = mdss_mdp_cmd_add_vsync_handler;
	ctl->ops.remove_vsync_handler = mdss_mdp_cmd_remove_vsync_handler;
	ctl->ops.read_line_cnt_fnc = mdss_mdp_cmd_line_count;
	ctl->ops.restore_fnc = mdss_mdp_cmd_restore;
	ctl->ops.reconfigure = mdss_mdp_cmd_reconfigure;
	pr_debug("%s:-\n", __func__);

	return 0;
}

