/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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
#include <linux/iopoll.h>
#include <linux/delay.h>

#include "mdss_mdp.h"
#include "mdss_panel.h"
#include "mdss_debug.h"
#include "mdss_mdp_trace.h"
#include "mdss_dsi_clk.h"
#include <linux/interrupt.h>

#define MAX_RECOVERY_TRIALS 10
#define MAX_SESSIONS 2

#define SPLIT_MIXER_OFFSET 0x800

#define STOP_TIMEOUT(hz) msecs_to_jiffies((1000 / hz) * (6 + 2))
#define POWER_COLLAPSE_TIME msecs_to_jiffies(100)
#define CMD_MODE_IDLE_TIMEOUT msecs_to_jiffies(16 * 4)
#define INPUT_EVENT_HANDLER_DELAY_USECS (16000 * 4)
#define AUTOREFRESH_MAX_FRAME_CNT 6

static DEFINE_MUTEX(cmd_clk_mtx);

static DEFINE_MUTEX(cmd_off_mtx);

enum mdss_mdp_cmd_autorefresh_state {
	MDP_AUTOREFRESH_OFF,
	MDP_AUTOREFRESH_ON_REQUESTED,
	MDP_AUTOREFRESH_ON,
	MDP_AUTOREFRESH_OFF_REQUESTED
};

struct mdss_mdp_cmd_ctx {
	struct mdss_mdp_ctl *ctl;

	u32 default_pp_num;
	u32 current_pp_num;
	/*
	 * aux_pp_num will be set only when topology is using split-lm.
	 * aux_pp_num will be used only when MDSS_QUIRK_DSC_RIGHT_ONLY_PU
	 * quirk is set and on following partial updates.
	 *
	 * right-only update on DUAL_LM_SINGLE_DISPLAY with DSC_MERGE
	 * right-only update on DUAL_LM_DUAL_DISPLAY with DSC
	 */
	u32 aux_pp_num;

	u8 ref_cnt;
	struct completion stop_comp;
	atomic_t rdptr_cnt;
	wait_queue_head_t rdptr_waitq;
	struct completion pp_done;
	wait_queue_head_t pp_waitq;
	struct list_head vsync_handlers;
	struct list_head lineptr_handlers;
	int panel_power_state;
	atomic_t koff_cnt;
	u32 intf_stopped;
	struct mutex mdp_rdptr_lock;
	struct mutex mdp_wrptr_lock;
	struct mutex clk_mtx;
	spinlock_t clk_lock;
	spinlock_t koff_lock;
	struct work_struct gate_clk_work;
	struct delayed_work delayed_off_clk_work;
	struct work_struct pp_done_work;
	struct work_struct early_wakeup_clk_work;
	atomic_t pp_done_cnt;
	struct completion rdptr_done;

	/*
	 * While autorefresh is on, partial update is not supported. So
	 * autorefresh state machine is always maintained through master ctx.
	 */
	struct mutex autorefresh_lock;
	struct completion autorefresh_ppdone;
	enum mdss_mdp_cmd_autorefresh_state autorefresh_state;
	int autorefresh_frame_cnt;
	bool ignore_external_te;
	struct completion autorefresh_done;

	int vsync_irq_cnt;
	int lineptr_irq_cnt;
	bool lineptr_enabled;
	u32 prev_wr_ptr_irq;

	struct mdss_intf_recovery intf_recovery;
	struct mdss_intf_recovery intf_mdp_callback;
	struct mdss_mdp_cmd_ctx *sync_ctx; /* for partial update */
	u32 pp_timeout_report_cnt;
	bool pingpong_split_slave;
};

struct mdss_mdp_cmd_ctx mdss_mdp_cmd_ctx_list[MAX_SESSIONS];

static int mdss_mdp_cmd_do_notifier(struct mdss_mdp_cmd_ctx *ctx);
static inline void mdss_mdp_cmd_clk_on(struct mdss_mdp_cmd_ctx *ctx);
static inline void mdss_mdp_cmd_clk_off(struct mdss_mdp_cmd_ctx *ctx);
static int mdss_mdp_cmd_wait4pingpong(struct mdss_mdp_ctl *ctl, void *arg);
static int mdss_mdp_disable_autorefresh(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_ctl *sctl);
static int mdss_mdp_setup_vsync(struct mdss_mdp_cmd_ctx *ctx, bool enable);

static bool __mdss_mdp_cmd_is_aux_pp_needed(struct mdss_data_type *mdata,
	struct mdss_mdp_ctl *mctl)
{
	return (mdata && mctl && mctl->is_master &&
		mdss_has_quirk(mdata, MDSS_QUIRK_DSC_RIGHT_ONLY_PU) &&
		is_dsc_compression(&mctl->panel_data->panel_info) &&
		((mctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) ||
		 ((mctl->mfd->split_mode == MDP_DUAL_LM_SINGLE_DISPLAY) &&
		  (mctl->panel_data->panel_info.dsc_enc_total == 1))) &&
		!mctl->mixer_left->valid_roi &&
		mctl->mixer_right->valid_roi);
}

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

	/*
	 * In case of DUAL_LM_SINGLE_DISPLAY, always keep right PP enabled
	 * if partial update is enabled. So when right-only update comes then
	 * by changing CTL topology, HW switches directly to right PP.
	 */
	if (ctl->panel_data->panel_info.partial_update_enabled &&
	    is_dual_lm_single_display(ctl->mfd)) {

		mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
		mdss_mdp_pingpong_write(mixer->pingpong_base,
				MDSS_MDP_REG_PP_TEAR_CHECK_EN,
				(te ? te->tear_check_en : 0) && enable);

	}

	return 0;
}

static int mdss_mdp_cmd_tearcheck_cfg(struct mdss_mdp_mixer *mixer,
		struct mdss_mdp_cmd_ctx *ctx, bool locked)
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

	mdss_mdp_vsync_clk_enable(1, locked);

	vsync_clk_speed_hz =
		mdss_mdp_get_clk_rate(MDSS_CLK_MDP_VSYNC, locked);

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

	pr_debug("%s: yres=%d vclks=%x height=%d init=%d rd=%d start=%d wr=%d\n",
		__func__, pinfo->yres, vclks_line, te->sync_cfg_height,
		te->vsync_init_val, te->rd_ptr_irq, te->start_pos,
		te->wr_ptr_irq);
	pr_debug("thrd_start =%d thrd_cont=%d pp_split=%d\n",
		te->sync_threshold_start, te->sync_threshold_continue,
		ctx->pingpong_split_slave);

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
		MDSS_MDP_REG_PP_WR_PTR_IRQ,
		te ? te->wr_ptr_irq : 0);
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

static int mdss_mdp_cmd_tearcheck_setup(struct mdss_mdp_cmd_ctx *ctx,
		bool locked)
{
	int rc = 0;
	struct mdss_mdp_mixer *mixer = NULL, *mixer_right = NULL;
	struct mdss_mdp_ctl *ctl = ctx->ctl;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	u32 offset = 0;

	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (mixer) {
		/*
		 * Disable auto refresh mode, if enabled in splash to
		 * avoid corruption.
		 */
		if (mdss_mdp_pingpong_read(mixer->pingpong_base,
			MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG) & BIT(31)) {
			offset = MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG;
			if (is_pingpong_split(ctl->mfd))
				writel_relaxed(0x0,
					(mdata->slave_pingpong_base + offset));
			if (is_split_lm(ctl->mfd)) {
				mixer_right =
					mdss_mdp_mixer_get(ctl,
						MDSS_MDP_MIXER_MUX_RIGHT);
				if (mixer_right)
					writel_relaxed(0x0,
					(mixer_right->pingpong_base + offset));
			}
			mdss_mdp_pingpong_write(mixer->pingpong_base,
				MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG, 0x0);
			pr_debug("%s: disabling auto refresh\n", __func__);
		}
		rc = mdss_mdp_cmd_tearcheck_cfg(mixer, ctx, locked);
		if (rc)
			goto err;
	}

	/*
	 * In case of DUAL_LM_SINGLE_DISPLAY, always keep right PP enabled
	 * if partial update is enabled. So when right-only update comes then
	 * by changing CTL topology, HW switches directly to right PP.
	 */
	if (ctl->panel_data->panel_info.partial_update_enabled &&
	    is_dual_lm_single_display(ctl->mfd)) {

		mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_RIGHT);
		if (mixer)
			rc = mdss_mdp_cmd_tearcheck_cfg(mixer, ctx, locked);
	}
err:
	return rc;
}

/**
 * enum mdp_rsrc_ctl_events - events for the resource control state machine
 * @MDP_RSRC_CTL_EVENT_KICKOFF:
 *	This event happens at NORMAL priority.
 *	Event that signals the start of the transfer, regardless of the
 *	state at which we enter this state (ON/OFF or GATE),
 *	we must ensure that power state is ON when we return from this
 *	event.
 *
 * @MDP_RSRC_CTL_EVENT_PP_DONE:
 *	This event happens at INTERRUPT level.
 *	Event signals the end of the data transfer, when getting this
 *	event we should have been in ON state, since a transfer was
 *	ongoing (if this is not the case, then
 *	there is a bug).
 *	Since this event is received at interrupt ievel, by the end of
 *	the event we haven't changed the power state, but scheduled
 *	work items to do the transition, so by the end of this event:
 *	1. A work item is scheduled to go to gate state as soon as
 *		possible (as soon as scheduler give us the chance)
 *	2. A delayed work is scheduled to go to OFF after
 *		CMD_MODE_IDLE_TIMEOUT time. Power State will be updated
 *		at the end of each work item, to make sure we update
 *		the status once the transition is fully done.
 *
 * @MDP_RSRC_CTL_EVENT_STOP:
 *	This event happens at NORMAL priority.
 *	When we get this event, we are expected to wait to finish any
 *	pending data transfer and turn off all the clocks/resources,
 *	so after return from this event we must be in off
 *	state.
 *
 * @MDP_RSRC_CTL_EVENT_EARLY_WAKE_UP:
 *	This event happens at NORMAL priority from a work item.
 *	Event signals that there will be a frame update soon and mdp should wake
 *	up early to update the frame with little latency.
 */
enum mdp_rsrc_ctl_events {
	MDP_RSRC_CTL_EVENT_KICKOFF = 1,
	MDP_RSRC_CTL_EVENT_PP_DONE,
	MDP_RSRC_CTL_EVENT_STOP,
	MDP_RSRC_CTL_EVENT_EARLY_WAKE_UP
};

enum {
	MDP_RSRC_CTL_STATE_OFF,
	MDP_RSRC_CTL_STATE_ON,
	MDP_RSRC_CTL_STATE_GATE,
};

/* helper functions for debugging */
static char *get_sw_event_name(u32 sw_event)
{
	switch (sw_event) {
	case MDP_RSRC_CTL_EVENT_KICKOFF:
		return "KICKOFF";
	case MDP_RSRC_CTL_EVENT_PP_DONE:
		return "PP_DONE";
	case MDP_RSRC_CTL_EVENT_STOP:
		return "STOP";
	case MDP_RSRC_CTL_EVENT_EARLY_WAKE_UP:
		return "EARLY_WAKE_UP";
	default:
		return "UNKNOWN";
	}
}

static char *get_clk_pwr_state_name(u32 pwr_state)
{
	switch (pwr_state) {
	case MDP_RSRC_CTL_STATE_ON:
		return "STATE_ON";
	case MDP_RSRC_CTL_STATE_OFF:
		return "STATE_OFF";
	case MDP_RSRC_CTL_STATE_GATE:
		return "STATE_GATE";
	default:
		return "UNKNOWN";
	}
}

/**
 * mdss_mdp_get_split_display_ctls() - get the display controllers
 * @ctl: Pointer to pointer to the controller used to do the operation.
 *	This can be the pointer to the master or slave of a display with
 *	the MDP_DUAL_LM_DUAL_DISPLAY split mode.
 * @sctl: Pointer to pointer where it is expected to be set the slave
 *	controller. Function does not expect any input parameter here.
 *
 * This function will populate the pointers to pointers with the controllers of
 * the split display ordered such way that the first input parameter will be
 * populated with the master controller and second parameter will be populated
 * with the slave controller, so the caller can assume both controllers are set
 * in the right order after return.
 *
 * This function can only be called for split configuration that uses two
 * controllers, it expects that first pointer is the one passed to do the
 * operation and it can be either the pointer of the master or slave,
 * since is the job of this function to find and accommodate the master/slave
 * controllers accordingly.
 *
 * Return: 0 - succeed, otherwise - fail
 */
int mdss_mdp_get_split_display_ctls(struct mdss_mdp_ctl **ctl,
	struct mdss_mdp_ctl **sctl)
{
	int rc = 0;
	*sctl = NULL;

	if (*ctl == NULL) {
		pr_err("%s invalid ctl\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if ((*ctl)->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		*sctl = mdss_mdp_get_split_ctl(*ctl);
		if (*sctl) {
			/* pointers are in the correct order */
			pr_debug("%s ctls in correct order ctl:%d sctl:%d\n",
				__func__, (*ctl)->num, (*sctl)->num);
			goto exit;
		} else {
			/*
			 * If we have a split display and we didn't find the
			 * Slave controller from the Master, this means that
			 * ctl is the slave controller, so look for the Master
			 */
			*sctl = mdss_mdp_get_main_ctl(*ctl);
			if (!(*sctl)) {
				/*
				 * Bad state, this shouldn't happen, we should
				 * be having both controllers since we are in
				 * dual-lm, dual-display.
				 */
				pr_err("%s cannot find master ctl\n",
					__func__);
				BUG();
			}
			/*
			 * We have both controllers but sctl has the Master,
			 * swap the pointers so we can keep the master in the
			 * ctl pointer and control the order in the power
			 * sequence.
			 */
			pr_debug("ctl is not the master, swap pointers\n");
			swap(*ctl, *sctl);
		}
	} else {
		pr_debug("%s no split mode:%d\n", __func__,
			(*ctl)->mfd->split_mode);
	}
exit:
	return rc;
}

/**
 * mdss_mdp_resource_control() - control the state of mdp resources
 * @ctl: pointer to controller to notify the event.
 * @sw_event: software event to modify the state of the resources.
 *
 * This function implements an state machine to control the state of
 * the mdp resources (clocks, bw, mmu), the states can be ON, OFF and GATE,
 * transition between each state is controlled through the MDP_RSRC_CTL_EVENT_
 * events.
 *
 * Return: 0 - succeed, otherwise - fail
 */
int mdss_mdp_resource_control(struct mdss_mdp_ctl *ctl, u32 sw_event)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(ctl->mfd);
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct mdss_mdp_ctl *sctl = NULL;
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;
	struct dsi_panel_clk_ctrl clk_ctrl;
	u32 status;
	int rc = 0;
	bool schedule_off = false;

	/* Get both controllers in the correct order for dual displays */
	mdss_mdp_get_split_display_ctls(&ctl, &sctl);

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("%s invalid ctx\n", __func__);
		rc = -EINVAL;
		goto exit;
	}

	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	/* In pingpong split we have single controller, dual context */
	if (is_pingpong_split(ctl->mfd))
		sctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[SLAVE_CTX];

	pr_debug("%pS-->%s: task:%s ctl:%d pwr_state:%s event:%s\n",
		__builtin_return_address(0), __func__,
		current->group_leader->comm, ctl->num,
		get_clk_pwr_state_name(mdp5_data->resources_state),
		get_sw_event_name(sw_event));

	MDSS_XLOG(ctl->num, mdp5_data->resources_state, sw_event,
		XLOG_FUNC_ENTRY);

	switch (sw_event) {
	case MDP_RSRC_CTL_EVENT_KICKOFF:
		/*
		 * Cancel any work item pending:
		 * If POWER-OFF was cancel:
		 *	Only UNGATE the clocks (resources should be ON)
		 * If GATE && POWER-OFF were cancel:
		 *	UNGATE and POWER-ON
		 * If only GATE was cancel:
		 *	something can be wrong, OFF should have been
		 *	cancel as well.
		 */

		/* update the active only vote */
		mdata->ao_bw_uc_idx = mdata->curr_bw_uc_idx;

		/* Cancel GATE Work Item */
		if (cancel_work_sync(&ctx->gate_clk_work)) {
			pr_debug("%s gate work canceled\n", __func__);

			if (mdp5_data->resources_state !=
					MDP_RSRC_CTL_STATE_ON)
				pr_debug("%s unexpected power state\n",
					__func__);
		}

		/* Cancel OFF Work Item  */
		if (cancel_delayed_work_sync(&ctx->delayed_off_clk_work)) {
			pr_debug("%s off work canceled\n", __func__);

			if (mdp5_data->resources_state ==
					MDP_RSRC_CTL_STATE_OFF)
				pr_debug("%s unexpected OFF state\n",
					__func__);
		}

		mutex_lock(&ctl->rsrc_lock);
		MDSS_XLOG(ctl->num, mdp5_data->resources_state, sw_event, 0x11);
		/* Transition OFF->ON || GATE->ON (enable clocks) */
		if ((mdp5_data->resources_state == MDP_RSRC_CTL_STATE_OFF) ||
			(mdp5_data->resources_state ==
			MDP_RSRC_CTL_STATE_GATE)) {
			u32 flags = CTL_INTF_EVENT_FLAG_SKIP_BROADCAST;

			/* Enable/Ungate DSI clocks and resources */
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

			clk_ctrl.state = MDSS_DSI_CLK_ON;
			clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
			mdss_mdp_ctl_intf_event /* enable master */
				(ctx->ctl, MDSS_EVENT_PANEL_CLK_CTRL,
				(void *)&clk_ctrl, flags);

			if (sctx) { /* then slave */
				if (sctx->pingpong_split_slave)
					flags |= CTL_INTF_EVENT_FLAG_SLAVE_INTF;

				mdss_mdp_ctl_intf_event(sctx->ctl,
					MDSS_EVENT_PANEL_CLK_CTRL,
					 (void *)&clk_ctrl, flags);
			}

			if (mdp5_data->resources_state ==
					MDP_RSRC_CTL_STATE_GATE)
				mdp5_data->resources_state =
					MDP_RSRC_CTL_STATE_ON;
		}

		/* Transition OFF->ON (enable resources)*/
		if (mdp5_data->resources_state ==
				MDP_RSRC_CTL_STATE_OFF) {
			/* Add an extra vote for the ahb bus */
			mdss_update_reg_bus_vote(mdata->reg_bus_clt,
				VOTE_INDEX_LOW);

			/* Enable MDP resources */
			mdss_mdp_cmd_clk_on(ctx);
			if (sctx)
				mdss_mdp_cmd_clk_on(sctx);

			mdp5_data->resources_state = MDP_RSRC_CTL_STATE_ON;
		}

		if (mdp5_data->resources_state != MDP_RSRC_CTL_STATE_ON) {
			/* we must be ON by the end of kickoff */
			pr_err("%s unexpected power state during:%s\n",
				__func__, get_sw_event_name(sw_event));
			BUG();
		}
		mutex_unlock(&ctl->rsrc_lock);
		break;
	case MDP_RSRC_CTL_EVENT_PP_DONE:
		if (mdp5_data->resources_state != MDP_RSRC_CTL_STATE_ON) {
			pr_err("%s unexpected power state during:%s\n",
				__func__, get_sw_event_name(sw_event));
			BUG();
		}

		/* Check that no pending kickoff is on-going */
		 status = mdss_mdp_ctl_perf_get_transaction_status(ctl);

		/*
		 * Same for the slave controller. for cases where
		 * transaction is only pending in the slave controller.
		 */
		if (sctl)
			status |= mdss_mdp_ctl_perf_get_transaction_status(
				sctl);

		/*
		 * Schedule the work items to shut down only if
		 * 1. no kickoff has been scheduled
		 * 2. no stop command has been started
		 * 3. no autorefresh is enabled
		 * 4. no commit is pending
		 */
		if ((PERF_STATUS_DONE == status) &&
			!ctx->intf_stopped &&
			(ctx->autorefresh_state == MDP_AUTOREFRESH_OFF) &&
			!ctl->mfd->atomic_commit_pending) {
			pr_debug("schedule release after:%d ms\n",
				jiffies_to_msecs
				(CMD_MODE_IDLE_TIMEOUT));

			MDSS_XLOG(ctl->num, mdp5_data->resources_state,
				sw_event, 0x22);

			/* start work item to gate */
			if (mdata->enable_gate)
				schedule_work(&ctx->gate_clk_work);

			/* start work item to shut down after delay */
			schedule_delayed_work(
					&ctx->delayed_off_clk_work,
					CMD_MODE_IDLE_TIMEOUT);
		}

		break;
	case MDP_RSRC_CTL_EVENT_STOP:

		/* Cancel early wakeup Work Item */
		if (cancel_work_sync(&ctx->early_wakeup_clk_work))
			pr_debug("early wakeup work canceled\n");

		/* If we are already OFF, just return */
		if (mdp5_data->resources_state ==
				MDP_RSRC_CTL_STATE_OFF) {
			pr_debug("resources already off\n");
			goto exit;
		}

		/* If pp_done is on-going, wait for it to finish */
		mdss_mdp_cmd_wait4pingpong(ctl, NULL);
		if (sctl)
			mdss_mdp_cmd_wait4pingpong(sctl, NULL);

		mutex_lock(&ctx->autorefresh_lock);
		if (ctx->autorefresh_state != MDP_AUTOREFRESH_OFF) {
			pr_debug("move autorefresh to disable state\n");
			mdss_mdp_disable_autorefresh(ctl, sctl);
		}
		mutex_unlock(&ctx->autorefresh_lock);

		/*
		 * If a pp_done happened just before the stop,
		 * we can still have some work items running;
		 * cancel any pending works.
		 */

		/* Cancel GATE Work Item */
		if (cancel_work_sync(&ctx->gate_clk_work)) {
			pr_debug("gate work canceled\n");

			if (mdp5_data->resources_state !=
				MDP_RSRC_CTL_STATE_ON)
				pr_debug("%s power state is not ON\n",
					__func__);
		}

		/* Cancel OFF Work Item  */
		if (cancel_delayed_work_sync(&ctx->delayed_off_clk_work)) {
			pr_debug("off work canceled\n");


			if (mdp5_data->resources_state ==
					MDP_RSRC_CTL_STATE_OFF)
				pr_debug("%s unexpected OFF state\n",
					__func__);
		}

		mutex_lock(&ctl->rsrc_lock);
		MDSS_XLOG(ctl->num, mdp5_data->resources_state, sw_event, 0x33);
		if ((mdp5_data->resources_state == MDP_RSRC_CTL_STATE_ON) ||
				(mdp5_data->resources_state
				== MDP_RSRC_CTL_STATE_GATE)) {

			/* Enable MDP clocks if gated */
			if (mdp5_data->resources_state ==
					MDP_RSRC_CTL_STATE_GATE)
				mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

			/* First Power off slave DSI (if present) */
			if (sctx)
				mdss_mdp_cmd_clk_off(sctx);

			/* Now Power off master DSI */
			mdss_mdp_cmd_clk_off(ctx);

			/* Remove extra vote for the ahb bus */
			mdss_update_reg_bus_vote(mdata->reg_bus_clt,
				VOTE_INDEX_DISABLE);


			/* we are done accessing the resources */
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

			/* update the state, now we are in off */
			mdp5_data->resources_state = MDP_RSRC_CTL_STATE_OFF;
		}
		mutex_unlock(&ctl->rsrc_lock);
		break;
	case MDP_RSRC_CTL_EVENT_EARLY_WAKE_UP:
		/*
		 * Cancel any work item pending and:
		 * 1. If the current state is ON, stay in ON.
		 * 2. If the current state is GATED, stay at GATED.
		 * 3. If the current state is POWER-OFF, POWER-ON and
		 *	schedule a work item to POWER-OFF if no
		 *	kickoffs get scheduled.
		 */

		/* if panels are off, do not process early wake up */
		if ((ctx && __mdss_mdp_cmd_is_panel_power_off(ctx)) ||
			(sctx && __mdss_mdp_cmd_is_panel_power_off(sctx)))
			break;

		/* Cancel GATE Work Item */
		if (cancel_work_sync(&ctx->gate_clk_work)) {
			pr_debug("%s: %s - gate_work cancelled\n",
				 __func__, get_sw_event_name(sw_event));
			schedule_off = true;
		}

		/* Cancel OFF Work Item */
		if (cancel_delayed_work_sync(
				&ctx->delayed_off_clk_work)) {
			pr_debug("%s: %s - off work cancelled\n",
				 __func__, get_sw_event_name(sw_event));
			schedule_off = true;
		}

		mutex_lock(&ctl->rsrc_lock);
		MDSS_XLOG(ctl->num, mdp5_data->resources_state, sw_event,
			schedule_off, 0x44);
		if (mdp5_data->resources_state == MDP_RSRC_CTL_STATE_OFF) {
			u32 flags = CTL_INTF_EVENT_FLAG_SKIP_BROADCAST;

			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			clk_ctrl.state = MDSS_DSI_CLK_ON;
			clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
			mdss_mdp_ctl_intf_event(ctx->ctl,
				MDSS_EVENT_PANEL_CLK_CTRL,
				(void *)&clk_ctrl, flags);

			if (sctx) { /* then slave */
				if (sctx->pingpong_split_slave)
					flags |= CTL_INTF_EVENT_FLAG_SLAVE_INTF;

				mdss_mdp_ctl_intf_event(sctx->ctl,
					MDSS_EVENT_PANEL_CLK_CTRL,
					(void *)&clk_ctrl, flags);
			}

			mdss_mdp_cmd_clk_on(ctx);
			if (sctx)
				mdss_mdp_cmd_clk_on(sctx);

			mdp5_data->resources_state = MDP_RSRC_CTL_STATE_ON;
			schedule_off = true;
		}

		/*
		 * Driver will schedule off work under three cases:
		 * 1. Early wakeup cancelled the gate work.
		 * 2. Early wakeup cancelled the off work.
		 * 3. Early wakeup changed the state to ON.
		 *
		 * Driver will not allow off work under one condition:
		 * 1. Kickoff is pending.
		 */
		if (schedule_off && !ctl->mfd->atomic_commit_pending) {
			/*
			 * Schedule off work after cmd mode idle timeout is
			 * reached. This is to prevent the case where early wake
			 * up is called but no frame update is sent.
			 */
			schedule_delayed_work(&ctx->delayed_off_clk_work,
				      CMD_MODE_IDLE_TIMEOUT);
			pr_debug("off work scheduled\n");
		}
		mutex_unlock(&ctl->rsrc_lock);
		break;
	default:
		pr_warn("%s unexpected event (%d)\n", __func__, sw_event);
		break;
	}
	MDSS_XLOG(sw_event, mdp5_data->resources_state, XLOG_FUNC_EXIT);

exit:
	return rc;
}

static bool mdss_mdp_cmd_is_autorefresh_enabled(struct mdss_mdp_ctl *mctl)
{
	struct mdss_mdp_cmd_ctx *ctx = mctl->intf_ctx[MASTER_CTX];
	bool enabled = false;

	/* check the ctl to make sure the lock was initialized */
	if (!ctx || !ctx->ctl)
		return 0;

	mutex_lock(&ctx->autorefresh_lock);
	if (ctx->autorefresh_state == MDP_AUTOREFRESH_ON)
		enabled = true;
	mutex_unlock(&ctx->autorefresh_lock);

	return enabled;
}

static inline void mdss_mdp_cmd_clk_on(struct mdss_mdp_cmd_ctx *ctx)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	pr_debug("%pS-->%s: task:%s ctx%d\n", __builtin_return_address(0),
		__func__, current->group_leader->comm, ctx->current_pp_num);

	mutex_lock(&ctx->clk_mtx);
	MDSS_XLOG(ctx->current_pp_num, atomic_read(&ctx->koff_cnt),
		mdata->bus_ref_cnt);

	mdss_bus_bandwidth_ctrl(true);

	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_RESUME);

	mutex_unlock(&ctx->clk_mtx);
}

static inline void mdss_mdp_cmd_clk_off(struct mdss_mdp_cmd_ctx *ctx)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	struct dsi_panel_clk_ctrl clk_ctrl;

	pr_debug("%pS-->%s: task:%s ctx%d\n", __builtin_return_address(0),
		__func__, current->group_leader->comm, ctx->current_pp_num);

	mutex_lock(&ctx->clk_mtx);
	MDSS_XLOG(ctx->current_pp_num, atomic_read(&ctx->koff_cnt),
		mdata->bus_ref_cnt);

	mdss_mdp_hist_intr_setup(&mdata->hist_intr, MDSS_IRQ_SUSPEND);

	/* Power off DSI, is caller responsibility to do slave then master  */
	if (ctx->ctl) {
		u32 flags = CTL_INTF_EVENT_FLAG_SKIP_BROADCAST;

		if (ctx->pingpong_split_slave)
			flags |= CTL_INTF_EVENT_FLAG_SLAVE_INTF;

		clk_ctrl.state = MDSS_DSI_CLK_OFF;
		clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
		mdss_mdp_ctl_intf_event
			(ctx->ctl, MDSS_EVENT_PANEL_CLK_CTRL,
			(void *)&clk_ctrl, flags);
	} else {
		pr_err("OFF with ctl:NULL\n");
	}

	mdss_bus_bandwidth_ctrl(false);

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

	vsync_time = ktime_get();
	ctl->vsync_cnt++;
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt));
	complete_all(&ctx->rdptr_done);

	/* If caller is waiting for the read pointer, notify. */
	if (atomic_read(&ctx->rdptr_cnt)) {
		if (atomic_add_unless(&ctx->rdptr_cnt, -1, 0)) {
			MDSS_XLOG(atomic_read(&ctx->rdptr_cnt));
			if (atomic_read(&ctx->rdptr_cnt))
				pr_warn("%s: too many rdptrs=%d!\n",
				  __func__, atomic_read(&ctx->rdptr_cnt));
		}
		wake_up_all(&ctx->rdptr_waitq);
	}

	spin_lock(&ctx->clk_lock);
	list_for_each_entry(tmp, &ctx->vsync_handlers, list) {
		if (tmp->enabled && !tmp->cmd_post_flush)
			tmp->vsync_handler(ctl, vsync_time);
	}
	spin_unlock(&ctx->clk_lock);
}

static int mdss_mdp_cmd_wait4readptr(struct mdss_mdp_cmd_ctx *ctx)
{
	int rc = 0;

	rc = wait_event_timeout(ctx->rdptr_waitq,
			atomic_read(&ctx->rdptr_cnt) == 0,
			KOFF_TIMEOUT);
	if (rc <= 0) {
		if (atomic_read(&ctx->rdptr_cnt))
			pr_err("timed out waiting for rdptr irq\n");
		else
			rc = 1;
	}
	return rc;
}

static int mdss_mdp_cmd_intf_callback(void *data, int event)
{
	struct mdss_mdp_cmd_ctx *ctx = data;
	struct mdss_mdp_pp_tear_check *te = NULL;
	u32 timeout_us = 3000, val = 0;
	struct mdss_mdp_mixer *mixer;

	if (!data) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!ctx->ctl)
		return -EINVAL;

	switch (event) {
	case MDP_INTF_CALLBACK_DSI_WAIT:
		pr_debug("%s: wait for frame cnt:%d event:%d\n",
			__func__, atomic_read(&ctx->rdptr_cnt), event);

		/*
		 * if we are going to suspended or pp split is not enabled,
		 * just return
		 */
		if (ctx->intf_stopped || !is_pingpong_split(ctx->ctl->mfd))
			return -EINVAL;
		atomic_inc(&ctx->rdptr_cnt);

		/* enable clks and rd_ptr interrupt */
		mdss_mdp_setup_vsync(ctx, true);

		mixer = mdss_mdp_mixer_get(ctx->ctl, MDSS_MDP_MIXER_MUX_LEFT);
		if (!mixer) {
			pr_err("%s: null mixer\n", __func__);
			return -EINVAL;
		}

		/* wait for read pointer */
		MDSS_XLOG(atomic_read(&ctx->rdptr_cnt));
		pr_debug("%s: wait for frame cnt:%d\n",
			__func__, atomic_read(&ctx->rdptr_cnt));
		mdss_mdp_cmd_wait4readptr(ctx);

		/* wait for 3ms to make sure we are within the frame */
		te = &ctx->ctl->panel_data->panel_info.te;
		readl_poll_timeout(mixer->pingpong_base +
			MDSS_MDP_REG_PP_INT_COUNT_VAL, val,
			(val & 0xffff) > (te->start_pos +
			te->sync_threshold_start), 10, timeout_us);

		/* disable rd_ptr interrupt */
		mdss_mdp_setup_vsync(ctx, false);

		break;
	default:
		pr_debug("%s: unhandled event=%d\n", __func__, event);
		break;
	}
	return 0;
}

static void mdss_mdp_cmd_lineptr_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_lineptr_handler *tmp;
	ktime_t lineptr_time;

	if (!ctx) {
		pr_err("invalid ctx\n");
		return;
	}

	lineptr_time = ktime_get();
	pr_debug("intr lineptr_time=%lld\n", ktime_to_ms(lineptr_time));

	spin_lock(&ctx->clk_lock);
	list_for_each_entry(tmp, &ctx->lineptr_handlers, list) {
		if (tmp->enabled)
			tmp->lineptr_handler(ctl, lineptr_time);
	}
	spin_unlock(&ctx->clk_lock);
}

static int mdss_mdp_cmd_intf_recovery(void *data, int event)
{
	struct mdss_mdp_cmd_ctx *ctx = data;
	unsigned long flags;
	bool reset_done = false, notify_frame_timeout = false;

	if (!data) {
		pr_err("%s: invalid ctx\n", __func__);
		return -EINVAL;
	}

	if (!ctx->ctl)
		return -EINVAL;

	/*
	 * Currently, only intf_fifo_underflow is
	 * supported for recovery sequence for command
	 * mode DSI interface
	 */
	if (event != MDP_INTF_DSI_CMD_FIFO_UNDERFLOW) {
		pr_warn("%s: unsupported recovery event:%d\n",
					__func__, event);
		return -EPERM;
	}

	if (atomic_read(&ctx->koff_cnt)) {
		mdss_mdp_ctl_reset(ctx->ctl, true);
		reset_done = true;
	}

	spin_lock_irqsave(&ctx->koff_lock, flags);
	if (reset_done && atomic_add_unless(&ctx->koff_cnt, -1, 0)) {
		pr_debug("%s: intf_num=%d\n", __func__, ctx->ctl->intf_num);
		mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
			ctx->current_pp_num);
		mdss_mdp_set_intr_callback_nosync(
				MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
				ctx->current_pp_num, NULL, NULL);
		if (mdss_mdp_cmd_do_notifier(ctx))
			notify_frame_timeout = true;
	}
	spin_unlock_irqrestore(&ctx->koff_lock, flags);

	if (notify_frame_timeout)
		mdss_mdp_ctl_notify(ctx->ctl, MDP_NOTIFY_FRAME_TIMEOUT);

	return 0;
}

static void mdss_mdp_cmd_pingpong_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_vsync_handler *tmp;
	ktime_t vsync_time;
	bool sync_ppdone;

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

	mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
		ctx->current_pp_num);
	mdss_mdp_set_intr_callback_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
		ctx->current_pp_num, NULL, NULL);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->current_pp_num);

	/*
	 * check state of sync ctx before decrementing koff_cnt to avoid race
	 * condition. That is, once both koff_cnt have been served and new koff
	 * can be triggered (sctx->koff_cnt could change)
	 */
	sync_ppdone = mdss_mdp_cmd_do_notifier(ctx);

	if (atomic_add_unless(&ctx->koff_cnt, -1, 0)) {
		if (atomic_read(&ctx->koff_cnt))
			pr_err("%s: too many kickoffs=%d!\n", __func__,
			       atomic_read(&ctx->koff_cnt));
		if (sync_ppdone) {
			atomic_inc(&ctx->pp_done_cnt);
			if (!ctl->commit_in_progress)
				schedule_work(&ctx->pp_done_work);

			mdss_mdp_resource_control(ctl,
				MDP_RSRC_CTL_EVENT_PP_DONE);
		}
		wake_up_all(&ctx->pp_waitq);
	} else {
		pr_err("%s: should not have pingpong interrupt!\n", __func__);
	}

	pr_debug("%s: ctl_num=%d intf_num=%d ctx=%d cnt=%d\n", __func__,
			ctl->num, ctl->intf_num, ctx->current_pp_num,
			atomic_read(&ctx->koff_cnt));

	trace_mdp_cmd_pingpong_done(ctl, ctx->current_pp_num,
		atomic_read(&ctx->koff_cnt));

	spin_unlock(&ctx->koff_lock);
}

static int mdss_mdp_setup_lineptr(struct mdss_mdp_cmd_ctx *ctx,
	bool enable)
{
	int changed = 0;

	mutex_lock(&ctx->mdp_wrptr_lock);

	if (enable) {
		if (ctx->lineptr_irq_cnt == 0)
			changed++;
		ctx->lineptr_irq_cnt++;
	} else {
		if (ctx->lineptr_irq_cnt) {
			ctx->lineptr_irq_cnt--;
			if (ctx->lineptr_irq_cnt == 0)
				changed++;
		} else {
			pr_warn("%pS->%s: wr_ptr can not be turned off\n",
				__builtin_return_address(0), __func__);
		}
	}

	if (changed)
		MDSS_XLOG(ctx->lineptr_irq_cnt, enable, current->pid);

	pr_debug("%pS->%s: lineptr_irq_cnt=%d changed=%d enable=%d ctl:%d pp:%d\n",
			__builtin_return_address(0), __func__,
			ctx->lineptr_irq_cnt, changed, enable,
			ctx->ctl->num, ctx->default_pp_num);

	if (changed) {
		if (enable) {
			/* enable clocks and irq */
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR,
				ctx->default_pp_num);
		} else {
			/* disable clocks and irq */
			mdss_mdp_irq_disable(MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR,
				ctx->default_pp_num);
			/*
			 * check the intr status and clear the irq before
			 * disabling the clocks
			 */
			mdss_mdp_intr_check_and_clear(
				MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR,
				ctx->default_pp_num);

			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		}
	}

	mutex_unlock(&ctx->mdp_wrptr_lock);
	return ctx->lineptr_irq_cnt;
}

static int mdss_mdp_cmd_add_lineptr_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_lineptr_handler *handle)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;
	int ret = 0;

	mutex_lock(&cmd_off_mtx);
	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctl->is_master) {
		ret = -EINVAL;
		goto done;
	}

	pr_debug("%pS->%s: ctl=%d\n",
		__builtin_return_address(0), __func__, ctl->num);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt));

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->lineptr_handlers);
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY)
		mutex_lock(&cmd_clk_mtx);

	mdss_mdp_setup_lineptr(ctx, true);
	ctx->lineptr_enabled = true;

	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY)
		mutex_unlock(&cmd_clk_mtx);
done:
	mutex_unlock(&cmd_off_mtx);

	return ret;
}

static int mdss_mdp_cmd_remove_lineptr_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_lineptr_handler *handle)
{
	struct mdss_mdp_cmd_ctx *ctx;
	unsigned long flags;
	bool disabled = true;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctl->is_master || !ctx->lineptr_enabled)
		return -EINVAL;

	pr_debug("%pS->%s: ctl=%d\n",
		__builtin_return_address(0), __func__, ctl->num);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt));

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);
	} else {
		disabled = false;
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (disabled)
		mdss_mdp_setup_lineptr(ctx, false);
	ctx->lineptr_enabled = false;
	ctx->prev_wr_ptr_irq = 0;

	return 0;
}

static int mdss_mdp_cmd_lineptr_ctrl(struct mdss_mdp_ctl *ctl, bool enable)
{
	struct mdss_mdp_pp_tear_check *te;
	struct mdss_mdp_cmd_ctx *ctx;
	int rc = 0;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx || !ctl->is_master)
		return -EINVAL;

	te = &ctl->panel_data->panel_info.te;
	pr_debug("%pS->%s: ctl=%d en=%d, prev_lineptr=%d, lineptr=%d\n",
			__builtin_return_address(0), __func__, ctl->num,
			enable, ctx->prev_wr_ptr_irq, te->wr_ptr_irq);

	if (enable) {
		/* update reg only if the value has changed */
		if (ctx->prev_wr_ptr_irq != te->wr_ptr_irq) {
			ctx->prev_wr_ptr_irq = te->wr_ptr_irq;
			mdss_mdp_pingpong_write(ctl->mixer_left->pingpong_base,
				MDSS_MDP_REG_PP_WR_PTR_IRQ, te->wr_ptr_irq);
		}

		/*
		 * add handler only when lineptr is not enabled
		 * and wr ptr is non zero
		 */
		if (!ctx->lineptr_enabled && te->wr_ptr_irq)
			rc = mdss_mdp_cmd_add_lineptr_handler(ctl,
				&ctl->lineptr_handler);
		/* Disable handler when the value is zero */
		else if (ctx->lineptr_enabled && !te->wr_ptr_irq)
			rc = mdss_mdp_cmd_remove_lineptr_handler(ctl,
				&ctl->lineptr_handler);
	} else {
		if (ctx->lineptr_enabled)
			rc = mdss_mdp_cmd_remove_lineptr_handler(ctl,
				&ctl->lineptr_handler);
	}

	return rc;
}

/*
 * Interface used to update the new lineptr value set by the sysfs node.
 * Value is instantly updated only when autorefresh is enabled, else
 * new value would be set in the next kickoff.
 */
static int mdss_mdp_cmd_update_lineptr(struct mdss_mdp_ctl *ctl, bool enable)
{
	if (mdss_mdp_cmd_is_autorefresh_enabled(ctl))
		return mdss_mdp_cmd_lineptr_ctrl(ctl, enable);

	return 0;
}

/**
 * mdss_mdp_cmd_autorefresh_pp_done() - pp done irq callback for autorefresh
 * @arg: void pointer to the controller context.
 *
 * This function is the pp_done interrupt callback while disabling
 * autorefresh. This function does not modify the kickoff count (koff_cnt).
 */
static void mdss_mdp_cmd_autorefresh_pp_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
		ctx->current_pp_num);
	mdss_mdp_set_intr_callback_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
		ctx->current_pp_num, NULL, NULL);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->current_pp_num);
	complete_all(&ctx->autorefresh_ppdone);

	pr_debug("%s: ctl_num=%d intf_num=%d ctx=%d cnt=%d\n", __func__,
		ctl->num, ctl->intf_num, ctx->current_pp_num,
		atomic_read(&ctx->koff_cnt));
}

static void pingpong_done_work(struct work_struct *work)
{
	u32 status;
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), pp_done_work);
	struct mdss_mdp_ctl *ctl = ctx->ctl;

	if (ctl) {
		while (atomic_add_unless(&ctx->pp_done_cnt, -1, 0))
			mdss_mdp_ctl_notify(ctx->ctl, MDP_NOTIFY_FRAME_DONE);

		status = mdss_mdp_ctl_perf_get_transaction_status(ctx->ctl);
		if (status == 0)
			mdss_mdp_ctl_perf_release_bw(ctx->ctl);

		if (!ctl->is_master)
			ctl = mdss_mdp_get_main_ctl(ctl);

		/* do not disable lineptr when autorefresh is enabled */
		if (mdss_mdp_is_lineptr_supported(ctl)
			&& !mdss_mdp_cmd_is_autorefresh_enabled(ctl))
			mdss_mdp_cmd_lineptr_ctrl(ctl, false);
	}
}

static void clk_ctrl_delayed_off_work(struct work_struct *work)
{
	struct mdss_overlay_private *mdp5_data;
	struct delayed_work *dw = to_delayed_work(work);
	struct mdss_mdp_cmd_ctx *ctx = container_of(dw,
		struct mdss_mdp_cmd_ctx, delayed_off_clk_work);
	struct mdss_mdp_ctl *ctl, *sctl;
	struct mdss_mdp_cmd_ctx *sctx = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	ctl = ctx->ctl;
	if (!ctl || !ctl->panel_data) {
		pr_err("NULL ctl||panel_data\n");
		return;
	}

	if (ctl->mfd->atomic_commit_pending) {
		pr_debug("leave clocks on for queued kickoff\n");
		return;
	}

	mdp5_data = mfd_to_mdp5_data(ctl->mfd);
	ATRACE_BEGIN(__func__);

	/*
	 * Ideally we should not wait for the gate work item to finish, since
	 * this work happens CMD_MODE_IDLE_TIMEOUT time after,
	 * but if the system is laggy, prevent from a race condition
	 * between both work items by waiting for the gate to finish.
	 */
	if (mdata->enable_gate)
		flush_work(&ctx->gate_clk_work);

	pr_debug("ctl:%d pwr_state:%s\n", ctl->num,
		get_clk_pwr_state_name
		(mdp5_data->resources_state));

	mutex_lock(&ctl->rsrc_lock);
	MDSS_XLOG(ctl->num, mdp5_data->resources_state, XLOG_FUNC_ENTRY);

	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		mutex_lock(&cmd_clk_mtx);

		if (mdss_mdp_get_split_display_ctls(&ctl, &sctl)) {
			/* error when getting both controllers, just returnr */
			pr_err("cannot get both controllers for the split display\n");
			goto exit;
		}

		/* re-assign to have the correct order in the context */
		ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];
		if (!ctx || !sctx) {
			pr_err("invalid %s %s\n",
				ctx?"":"ctx", sctx?"":"sctx");
			goto exit;
		}
	} else if (is_pingpong_split(ctl->mfd)) {
		mutex_lock(&cmd_clk_mtx);
		sctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[SLAVE_CTX];
		if (!sctx) {
			pr_err("invalid sctx\n");
			goto exit;
		}
	}

	if (ctx->autorefresh_state != MDP_AUTOREFRESH_OFF) {
		/*
		 * Driver shouldn't have scheduled this work item if
		 * autorefresh was enabled, but if any race
		 * condition happens between this work queue and
		 * the enable of the feature, make sure we do not
		 * process this request and mark this error.
		 */
		pr_err("cannot disable clks while autorefresh is not off\n");
		goto exit;
	}

	/* Enable clocks if Gate feature is enabled and we are in this state */
	if (mdata->enable_gate && (mdp5_data->resources_state
			== MDP_RSRC_CTL_STATE_GATE))
		mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);

	/* first power off the slave DSI (if present) */
	if (sctx)
		mdss_mdp_cmd_clk_off(sctx);

	/* now power off the master DSI */
	mdss_mdp_cmd_clk_off(ctx);

	/* Remove extra vote for the ahb bus */
	mdss_update_reg_bus_vote(mdata->reg_bus_clt,
		VOTE_INDEX_DISABLE);

	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	/* update state machine that power off transition is done */
	mdp5_data->resources_state = MDP_RSRC_CTL_STATE_OFF;

exit:
	/* do this at the end, so we can also protect the global power state*/
	if ((ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) ||
	    is_pingpong_split(ctl->mfd))
		mutex_unlock(&cmd_clk_mtx);

	MDSS_XLOG(ctl->num, mdp5_data->resources_state, XLOG_FUNC_EXIT);
	mutex_unlock(&ctl->rsrc_lock);

	ATRACE_END(__func__);
}

static void clk_ctrl_gate_work(struct work_struct *work)
{
	struct mdss_overlay_private *mdp5_data;
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), gate_clk_work);
	struct mdss_mdp_ctl *ctl, *sctl;
	struct mdss_mdp_cmd_ctx *sctx = NULL;
	struct dsi_panel_clk_ctrl clk_ctrl;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	ATRACE_BEGIN(__func__);
	ctl = ctx->ctl;
	if (!ctl) {
		pr_err("%s: invalid ctl\n", __func__);
		return;
	}

	mdp5_data = mfd_to_mdp5_data(ctl->mfd);
	if (!mdp5_data) {
		pr_err("%s: invalid mdp data\n", __func__);
		return;
	}

	pr_debug("%s ctl:%d pwr_state:%s\n", __func__,
		ctl->num, get_clk_pwr_state_name
		(mdp5_data->resources_state));

	mutex_lock(&ctl->rsrc_lock);
	MDSS_XLOG(ctl->num, mdp5_data->resources_state, XLOG_FUNC_ENTRY);


	if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		mutex_lock(&cmd_clk_mtx);

		if (mdss_mdp_get_split_display_ctls(&ctl, &sctl)) {
			/* error when getting both controllers, just return */
			pr_err("%s cannot get both cts for the split display\n",
				__func__);
			goto exit;
		}

		/* re-assign to have the correct order in the context */
		ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];
		if (!ctx || !sctx) {
			pr_err("%s ERROR invalid %s %s\n", __func__,
				ctx?"":"ctx", sctx?"":"sctx");
			goto exit;
		}
	} else if (is_pingpong_split(ctl->mfd)) {
		mutex_lock(&cmd_clk_mtx);
		sctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[SLAVE_CTX];
		if (!sctx) {
			pr_err("invalid sctx\n");
			goto exit;
		}
	}

	if (ctx->autorefresh_state != MDP_AUTOREFRESH_OFF) {
		/*
		 * Driver shouldn't have scheduled this work item if
		 * autorefresh was enabled, but if any race
		 * condition happens between this work queue and
		 * the enable of the feature, make sure we do not
		 * process this request and mark this error.
		 */
		pr_err("cannot gate clocks with autorefresh\n");
		goto exit;
	}

	clk_ctrl.state = MDSS_DSI_CLK_EARLY_GATE;
	clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
	/* First gate the DSI clocks for the slave controller (if present) */
	if (sctx) {
		u32 flags = CTL_INTF_EVENT_FLAG_SKIP_BROADCAST;

		if (sctx->pingpong_split_slave)
			flags |= CTL_INTF_EVENT_FLAG_SLAVE_INTF;

		mdss_mdp_ctl_intf_event(sctx->ctl,
			MDSS_EVENT_PANEL_CLK_CTRL,
			(void *)&clk_ctrl, flags);
	}

	/* Now gate DSI clocks for the master */
	mdss_mdp_ctl_intf_event
		(ctx->ctl, MDSS_EVENT_PANEL_CLK_CTRL,
		(void *)&clk_ctrl, CTL_INTF_EVENT_FLAG_SKIP_BROADCAST);

	/* Gate mdp clocks */
	mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

	/* update state machine that gate transition is done */
	mdp5_data->resources_state = MDP_RSRC_CTL_STATE_GATE;

exit:
	/* unlock mutex needed for split display */
	if ((ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) ||
	    is_pingpong_split(ctl->mfd))
		mutex_unlock(&cmd_clk_mtx);

	MDSS_XLOG(ctl->num, mdp5_data->resources_state, XLOG_FUNC_EXIT);
	mutex_unlock(&ctl->rsrc_lock);

	ATRACE_END(__func__);
}

static int mdss_mdp_setup_vsync(struct mdss_mdp_cmd_ctx *ctx,
	bool enable)
{
	int changed = 0;

	mutex_lock(&ctx->mdp_rdptr_lock);

	if (enable) {
		if (ctx->vsync_irq_cnt == 0)
			changed++;
		ctx->vsync_irq_cnt++;
	} else {
		if (ctx->vsync_irq_cnt) {
			ctx->vsync_irq_cnt--;
			if (ctx->vsync_irq_cnt == 0)
				changed++;
		} else {
			pr_warn("%pS->%s: rd_ptr can not be turned off\n",
				__builtin_return_address(0), __func__);
		}
	}

	if (changed)
		MDSS_XLOG(ctx->vsync_irq_cnt, enable, current->pid);

	pr_debug("%pS->%s: vsync_cnt=%d changed=%d enable=%d ctl:%d pp:%d\n",
			__builtin_return_address(0), __func__,
			ctx->vsync_irq_cnt, changed, enable,
			ctx->ctl->num, ctx->default_pp_num);

	if (changed) {
		if (enable) {
			/* enable clocks and irq */
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR,
				ctx->default_pp_num);
		} else {
			/* disable clocks and irq */
			mdss_mdp_irq_disable(MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR,
				ctx->default_pp_num);
			/*
			 * check the intr status and clear the irq before
			 * disabling the clocks
			 */
			mdss_mdp_intr_check_and_clear(
				MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR,
				ctx->default_pp_num);

			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
		}
	}

	mutex_unlock(&ctx->mdp_rdptr_lock);
	return ctx->vsync_irq_cnt;
}

static int mdss_mdp_cmd_add_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_ctl *sctl = NULL;
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;
	unsigned long flags;
	bool enable_rdptr = false;
	int ret = 0;

	mutex_lock(&cmd_off_mtx);
	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		ret = -ENODEV;
		goto done;
	}

	pr_debug("%pS->%s ctl:%d\n",
		__builtin_return_address(0), __func__, ctl->num);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt));
	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (!handle->enabled) {
		handle->enabled = true;
		list_add(&handle->list, &ctx->vsync_handlers);

		enable_rdptr = !handle->cmd_post_flush;
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (enable_rdptr) {
		if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY)
			mutex_lock(&cmd_clk_mtx);

		/* enable rd_ptr interrupt and clocks */
		mdss_mdp_setup_vsync(ctx, true);

		if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY)
			mutex_unlock(&cmd_clk_mtx);
	}

done:
	mutex_unlock(&cmd_off_mtx);

	return ret;
}

static int mdss_mdp_cmd_remove_vsync_handler(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_vsync_handler *handle)
{
	struct mdss_mdp_ctl *sctl;
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;
	unsigned long flags;
	bool disable_vsync_irq = false;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return -ENODEV;
	}

	pr_debug("%pS->%s ctl:%d\n",
		__builtin_return_address(0), __func__, ctl->num);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), 0x88888);
	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	spin_lock_irqsave(&ctx->clk_lock, flags);
	if (handle->enabled) {
		handle->enabled = false;
		list_del_init(&handle->list);
		disable_vsync_irq = !handle->cmd_post_flush;
	}
	spin_unlock_irqrestore(&ctx->clk_lock, flags);

	if (disable_vsync_irq) {
		/* disable rd_ptr interrupt and clocks */
		mdss_mdp_setup_vsync(ctx, false);
		complete(&ctx->stop_comp);
	}

	return 0;
}

int mdss_mdp_cmd_reconfigure_splash_done(struct mdss_mdp_ctl *ctl,
	bool handoff)
{
	struct mdss_panel_data *pdata;
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	struct dsi_panel_clk_ctrl clk_ctrl;
	int ret = 0;

	pdata = ctl->panel_data;

	clk_ctrl.state = MDSS_DSI_CLK_OFF;
	clk_ctrl.client = DSI_CLK_REQ_MDP_CLIENT;
	if (sctl) {
		u32 flags = CTL_INTF_EVENT_FLAG_SKIP_BROADCAST;

		if (is_pingpong_split(sctl->mfd))
			flags |= CTL_INTF_EVENT_FLAG_SLAVE_INTF;

		mdss_mdp_ctl_intf_event(sctl, MDSS_EVENT_PANEL_CLK_CTRL,
			(void *)&clk_ctrl, flags);
	}

	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_CLK_CTRL,
		(void *)&clk_ctrl, CTL_INTF_EVENT_FLAG_SKIP_BROADCAST);

	pdata->panel_info.cont_splash_enabled = 0;
	if (sctl)
		sctl->panel_data->panel_info.cont_splash_enabled = 0;
	else if (pdata->next && is_pingpong_split(ctl->mfd))
		pdata->next->panel_info.cont_splash_enabled = 0;

	return ret;
}

static int __mdss_mdp_wait4pingpong(struct mdss_mdp_cmd_ctx *ctx)
{
	int rc = 0;
	s64 expected_time = ktime_to_ms(ktime_get()) + KOFF_TIMEOUT_MS;
	s64 time;

	do {
		rc = wait_event_timeout(ctx->pp_waitq,
				atomic_read(&ctx->koff_cnt) == 0,
				KOFF_TIMEOUT);
		time = ktime_to_ms(ktime_get());

		MDSS_XLOG(rc, time, expected_time, atomic_read(&ctx->koff_cnt));
		/*
		 * If we time out, counter is valid and time is less,
		 * wait again.
		 */
	} while (atomic_read(&ctx->koff_cnt) && (rc == 0) &&
			(time < expected_time));

	return rc;
}

static int mdss_mdp_cmd_wait4pingpong(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_panel_data *pdata;
	unsigned long flags;
	int rc = 0, te_irq;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	pdata = ctl->panel_data;

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctl->roi_bkup.w,
			ctl->roi_bkup.h);

	pr_debug("%s: intf_num=%d ctx=%pK koff_cnt=%d\n", __func__,
			ctl->intf_num, ctx, atomic_read(&ctx->koff_cnt));

	rc = __mdss_mdp_wait4pingpong(ctx);

	trace_mdp_cmd_wait_pingpong(ctl->num,
				atomic_read(&ctx->koff_cnt));

	if (rc <= 0) {
		u32 status, mask;
		mask = mdss_mdp_get_irq_mask(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
				ctx->current_pp_num);
		status = mask & readl_relaxed(ctl->mdata->mdp_base +
				MDSS_MDP_REG_INTR_STATUS);
		MDSS_XLOG(status, rc, atomic_read(&ctx->koff_cnt));
		if (status) {
			pr_warn("pp done but irq not triggered\n");
			mdss_mdp_irq_clear(ctl->mdata,
				MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
				ctx->current_pp_num);
			local_irq_save(flags);
			mdss_mdp_cmd_pingpong_done(ctl);
			local_irq_restore(flags);
			rc = 1;
		}

		rc = atomic_read(&ctx->koff_cnt) == 0;
	}

	if (rc <= 0) {
		pr_err("%s:wait4pingpong timed out ctl=%d rc=%d cnt=%d koff_cnt=%d\n",
				__func__,
				ctl->num, rc, ctx->pp_timeout_report_cnt,
				atomic_read(&ctx->koff_cnt));

		/* enable TE irq to check if it is coming from the panel */
		te_irq = gpio_to_irq(pdata->panel_te_gpio);
		enable_irq(te_irq);

		/* wait for 20ms to ensure we are getting the next TE */
		usleep_range(20000, 20010);

		reinit_completion(&pdata->te_done);
		rc = wait_for_completion_timeout(&pdata->te_done, KOFF_TIMEOUT);

		if (!rc) {
			MDSS_XLOG(0xbac);
			mdss_fb_report_panel_dead(ctl->mfd);
		} else if (ctx->pp_timeout_report_cnt == 0) {
			MDSS_XLOG(0xbad);
			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl", "dsi0_phy",
				"dsi1_ctrl", "dsi1_phy", "vbif", "vbif_nrt",
				"dbg_bus", "vbif_dbg_bus", "panic");
		} else if (ctx->pp_timeout_report_cnt == MAX_RECOVERY_TRIALS) {
			MDSS_XLOG(0xbad2);
			MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl", "dsi0_phy",
				"dsi1_ctrl", "dsi1_phy", "vbif", "vbif_nrt",
				"dbg_bus", "vbif_dbg_bus", "panic");
			mdss_fb_report_panel_dead(ctl->mfd);
		}

		/* disable te irq */
		disable_irq_nosync(te_irq);

		ctx->pp_timeout_report_cnt++;
		rc = -EPERM;

		mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
			ctx->current_pp_num);
		mdss_mdp_set_intr_callback_nosync(
				MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
				ctx->current_pp_num, NULL, NULL);
		if (atomic_add_unless(&ctx->koff_cnt, -1, 0)
			&& mdss_mdp_cmd_do_notifier(ctx))
			mdss_mdp_ctl_notify(ctl, MDP_NOTIFY_FRAME_TIMEOUT);

	} else {
		rc = 0;
		ctx->pp_timeout_report_cnt = 0;
	}

	cancel_work_sync(&ctx->pp_done_work);

	/* signal any pending ping pong done events */
	while (atomic_add_unless(&ctx->pp_done_cnt, -1, 0))
		mdss_mdp_ctl_notify(ctx->ctl, MDP_NOTIFY_FRAME_DONE);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), rc);

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

/* only master ctl is valid and pingpong split with DSC is pending */
static void mdss_mdp_cmd_dsc_reconfig(struct mdss_mdp_ctl *ctl)
{
	struct mdss_panel_info *pinfo;
	bool changed = false;

	if (!ctl || !ctl->is_master)
		return;

	pinfo = &ctl->panel_data->panel_info;
	if (pinfo->compression_mode != COMPRESSION_DSC)
		return;

	changed = ctl->mixer_left->roi_changed;
	if (is_split_lm(ctl->mfd))
		changed |= ctl->mixer_right->roi_changed;

	if (changed)
		mdss_mdp_ctl_dsc_setup(ctl, pinfo);
}

static int mdss_mdp_cmd_set_partial_roi(struct mdss_mdp_ctl *ctl)
{
	int rc = -EINVAL;

	if (!ctl->panel_data->panel_info.partial_update_enabled)
		return rc;

	/* set panel col and page addr */
	rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_ENABLE_PARTIAL_ROI,
				     NULL, CTL_INTF_EVENT_FLAG_DEFAULT);
	return rc;
}

static int mdss_mdp_cmd_set_stream_size(struct mdss_mdp_ctl *ctl)
{
	int rc = -EINVAL;

	if (!ctl->panel_data->panel_info.partial_update_enabled)
		return rc;

	/* set dsi controller stream size */
	rc = mdss_mdp_ctl_intf_event(ctl,
		MDSS_EVENT_DSI_STREAM_SIZE, NULL, CTL_INTF_EVENT_FLAG_DEFAULT);
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

	/* In pingpong split we have single controller, dual context */
	if (is_pingpong_split(ctl->mfd))
		sctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[SLAVE_CTX];

	if (!__mdss_mdp_cmd_is_panel_power_on_interactive(ctx)) {
		if (ctl->pending_mode_switch != SWITCH_RESOLUTION) {
			rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_LINK_READY,
					NULL, CTL_INTF_EVENT_FLAG_DEFAULT);
			WARN(rc, "intf %d link ready error (%d)\n",
					ctl->intf_num, rc);

			rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_UNBLANK,
					NULL, CTL_INTF_EVENT_FLAG_DEFAULT);
			WARN(rc, "intf %d unblank error (%d)\n",
					ctl->intf_num, rc);

			rc = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_ON,
					NULL, CTL_INTF_EVENT_FLAG_DEFAULT);
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
			(void *)&ctx->intf_recovery,
			CTL_INTF_EVENT_FLAG_DEFAULT);

		mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_REGISTER_MDP_CALLBACK,
			(void *)&ctx->intf_mdp_callback,
			CTL_INTF_EVENT_FLAG_DEFAULT);

		ctx->intf_stopped = 0;
		if (sctx)
			sctx->intf_stopped = 0;
	} else {
		pr_err("%s: Panel already on\n", __func__);
	}

	return rc;
}

/*
 * This function will be called from the sysfs node to enable and disable the
 * feature with master ctl only.
 */
int mdss_mdp_cmd_set_autorefresh_mode(struct mdss_mdp_ctl *mctl, int frame_cnt)
{
	int rc = 0;
	struct mdss_mdp_cmd_ctx *ctx;
	struct mdss_panel_info *pinfo;

	if (!mctl || !mctl->is_master || !mctl->panel_data) {
		pr_err("invalid ctl mctl:%pK pdata:%pK\n",
			mctl, mctl ? mctl->panel_data : 0);
		return -ENODEV;
	}

	ctx = mctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	pinfo = &mctl->panel_data->panel_info;
	if (!pinfo->mipi.hw_vsync_mode) {
		pr_err("hw vsync disabled, cannot handle autorefresh\n");
		return -ENODEV;
	}

	if (frame_cnt < 0 || frame_cnt >= AUTOREFRESH_MAX_FRAME_CNT) {
		pr_err("frame cnt %d is out of range (16 bits).\n", frame_cnt);
		return -EINVAL;
	}

	if (ctx->intf_stopped) {
		pr_debug("autorefresh cannot be changed when display is off\n");
		return -EPERM;
	}

	mutex_lock(&ctx->autorefresh_lock);

	if (frame_cnt == ctx->autorefresh_frame_cnt) {
		pr_debug("No change to the refresh count\n");
		goto exit;
	}

	MDSS_XLOG(ctx->autorefresh_state,
		  ctx->autorefresh_frame_cnt, frame_cnt);

	pr_debug("curent autorfresh state=%d, frmae_cnt: old=%d new=%d\n",
			ctx->autorefresh_state,
			ctx->autorefresh_frame_cnt, frame_cnt);

	switch (ctx->autorefresh_state) {
	case MDP_AUTOREFRESH_OFF:
		if (frame_cnt == 0) {
			pr_debug("oops autorefresh is already disabled. We shouldn't get here\n");
			rc = -EINVAL;
			goto exit;
		}

		/*
		 * actual enable will happen in commit context when
		 * next update is kicked off.
		 */
		ctx->autorefresh_state = MDP_AUTOREFRESH_ON_REQUESTED;
		ctx->autorefresh_frame_cnt = frame_cnt;
		mctl->mdata->serialize_wait4pp = true;

		/* Cancel GATE Work Item */
		if (cancel_work_sync(&ctx->gate_clk_work))
			pr_debug("%s: gate work canceled\n", __func__);

		/* Cancel OFF Work Item  */
		if (cancel_delayed_work_sync(&ctx->delayed_off_clk_work))
			pr_debug("%s: off work canceled\n", __func__);
		break;
	case MDP_AUTOREFRESH_ON_REQUESTED:
		if (frame_cnt == 0) {
			ctx->autorefresh_state = MDP_AUTOREFRESH_OFF;
			ctx->autorefresh_frame_cnt = 0;
			mctl->mdata->serialize_wait4pp = false;
		} else {
			ctx->autorefresh_frame_cnt = frame_cnt;
			mctl->mdata->serialize_wait4pp = true;
		}
		break;
	case MDP_AUTOREFRESH_ON:
		if (frame_cnt == 0) {
			/*
			 * actual disable will happen in commit context when
			 * next update is kicked off.
			 */
			ctx->autorefresh_state = MDP_AUTOREFRESH_OFF_REQUESTED;
		} else {
			ctx->autorefresh_frame_cnt = frame_cnt;
			mctl->mdata->serialize_wait4pp = true;
		}
		break;
	case MDP_AUTOREFRESH_OFF_REQUESTED:
		if (frame_cnt == 0) {
			pr_debug("autorefresh off is already requested\n");
		} else {
			pr_debug("cancelling autorefresh off request\n");
			ctx->autorefresh_state = MDP_AUTOREFRESH_ON;
			ctx->autorefresh_frame_cnt = frame_cnt;
			mctl->mdata->serialize_wait4pp = true;
		}
		break;
	default:
		pr_err("invalid autorefresh state\n");
	}

	MDSS_XLOG(ctx->autorefresh_state,
		ctx->autorefresh_frame_cnt);

exit:
	mutex_unlock(&ctx->autorefresh_lock);
	return rc;
}

int mdss_mdp_cmd_get_autorefresh_mode(struct mdss_mdp_ctl *mctl)
{
	struct mdss_mdp_cmd_ctx *ctx = mctl->intf_ctx[MASTER_CTX];
	int autorefresh_frame_cnt;

	/* check the ctl to make sure the lock was initialized */
	if (!ctx || !ctx->ctl)
		return 0;

	mutex_lock(&ctx->autorefresh_lock);
	autorefresh_frame_cnt = ctx->autorefresh_frame_cnt;
	mutex_unlock(&ctx->autorefresh_lock);

	return ctx->autorefresh_frame_cnt;
}

static void mdss_mdp_cmd_pre_programming(struct mdss_mdp_ctl *mctl)
{
	struct mdss_mdp_cmd_ctx *ctx = mctl->intf_ctx[MASTER_CTX];
	char __iomem *pp_base;
	u32 autorefresh_state;
	u32 cfg;

	if (!mctl->is_master)
		return;

	mutex_lock(&ctx->autorefresh_lock);

	autorefresh_state = ctx->autorefresh_state;
	MDSS_XLOG(autorefresh_state);
	pr_debug("pre_programming state: %d\n", autorefresh_state);

	if ((autorefresh_state == MDP_AUTOREFRESH_ON) ||
		(autorefresh_state == MDP_AUTOREFRESH_OFF_REQUESTED)) {

		pp_base = mctl->mixer_left->pingpong_base;

		/*
		 * instruct MDP to ignore the panel TE so the next auto-refresh
		 * is delayed until flush bits are set.
		 */
		cfg = mdss_mdp_pingpong_read(pp_base,
			MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC);
		cfg &= ~BIT(20);
		mdss_mdp_pingpong_write(pp_base,
			MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);
		ctx->ignore_external_te = true;

	}
	mutex_unlock(&ctx->autorefresh_lock);
}

/* this function assumes that autorefresh_lock is held by the caller  */
static void mdss_mdp_cmd_post_programming(struct mdss_mdp_ctl *mctl)
{
	struct mdss_mdp_cmd_ctx *ctx = mctl->intf_ctx[MASTER_CTX];
	char __iomem *pp_base;
	u32 cfg;

	if (!mctl->is_master)
		return;

	/*
	 * If listening to the external panel TE was disabled
	 * (this happens when we get a kickoff with
	 * autorefresh enabled), enable the panel TE back.
	 */
	if (ctx->ignore_external_te) {

		MDSS_XLOG(ctx->ignore_external_te);
		pr_debug("post_programming TE status: %d\n",
			ctx->ignore_external_te);

		pp_base = mctl->mixer_left->pingpong_base;

		/* enable MDP to listen to the TE */
		cfg = mdss_mdp_pingpong_read(pp_base,
			MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC);
		cfg |= BIT(20);
		mdss_mdp_pingpong_write(pp_base,
			MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);
		ctx->ignore_external_te = false;
	}
}

static void mdss_mdp_cmd_wait4_autorefresh_pp(struct mdss_mdp_ctl *ctl)
{
	int rc;
	u32 val, line_out, intr_type = MDSS_MDP_IRQ_TYPE_PING_PONG_COMP;
	char __iomem *pp_base = ctl->mixer_left->pingpong_base;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	line_out = mdss_mdp_pingpong_read(pp_base, MDSS_MDP_REG_PP_LINE_COUNT);

	MDSS_XLOG(ctl->num, line_out, ctl->mixer_left->roi.h);

	if ((line_out < ctl->mixer_left->roi.h) && line_out) {
		reinit_completion(&ctx->autorefresh_ppdone);

		/* enable ping pong done */
		mdss_mdp_set_intr_callback(intr_type, ctx->current_pp_num,
					mdss_mdp_cmd_autorefresh_pp_done, ctl);
		mdss_mdp_irq_enable(intr_type, ctx->current_pp_num);

		/* wait for ping pong done */
		rc = wait_for_completion_timeout(&ctx->autorefresh_ppdone,
				KOFF_TIMEOUT);
		if (rc <= 0) {
			val = mdss_mdp_pingpong_read(pp_base,
				MDSS_MDP_REG_PP_LINE_COUNT);
			if (val == ctl->mixer_left->roi.h) {
				mdss_mdp_irq_clear(ctl->mdata,
					MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
					ctx->current_pp_num);
				mdss_mdp_irq_disable_nosync(intr_type,
					ctx->current_pp_num);
				mdss_mdp_set_intr_callback(intr_type,
					ctx->current_pp_num, NULL, NULL);
			} else {
				pr_err("timedout waiting for ctl%d autorefresh pp done\n",
					ctl->num);
				MDSS_XLOG(0xbad3);
				MDSS_XLOG_TOUT_HANDLER("mdp",
					"vbif", "dbg_bus", "vbif_dbg_bus",
					"panic");
			}
		}
	}
}

static void mdss_mdp_cmd_autorefresh_done(void *arg)
{
	struct mdss_mdp_ctl *ctl = arg;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	mdss_mdp_irq_disable_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
		ctx->current_pp_num);
	mdss_mdp_set_intr_callback_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
		ctx->current_pp_num, NULL, NULL);

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), ctx->current_pp_num);
	complete_all(&ctx->autorefresh_done);
}

static u32 get_autorefresh_timeout(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_cmd_ctx *ctx, u32 frame_cnt)
{
	struct mdss_mdp_mixer *mixer;
	struct mdss_panel_info *pinfo;
	u32 line_count;
	u32 fps, v_total;
	unsigned long autorefresh_timeout;

	pinfo = &ctl->panel_data->panel_info;
	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);

	if (!mixer || !pinfo)
		return -EINVAL;

	if (!ctx->ignore_external_te)
		line_count = ctl->mixer_left->roi.h;
	else
		line_count = mdss_mdp_pingpong_read(mixer->pingpong_base,
			MDSS_MDP_REG_PP_SYNC_CONFIG_HEIGHT) & 0xffff;

	fps = mdss_panel_get_framerate(pinfo, FPS_RESOLUTION_HZ);
	v_total = mdss_panel_get_vtotal(pinfo);

	/*
	 * calculate the expected delay for the autorefresh to happen,
	 * this should be:
	 * autorefresh_done = line_count * frame_cnt * line_time
	 */
	frame_cnt *= 1000; /* to use mS */
	autorefresh_timeout = mult_frac(line_count, frame_cnt,
		(fps * v_total));

	/* multiply by two to consider worst case scenario */
	autorefresh_timeout *= 2;
	autorefresh_timeout = msecs_to_jiffies(autorefresh_timeout);

	pr_debug("lines:%d fps:%d v_total:%d frames:%d timeout=%lu\n",
		line_count, fps, v_total, frame_cnt, autorefresh_timeout);

	autorefresh_timeout = (autorefresh_timeout > CMD_MODE_IDLE_TIMEOUT) ?
		autorefresh_timeout : CMD_MODE_IDLE_TIMEOUT;

	return autorefresh_timeout;
}

static void mdss_mdp_cmd_wait4_autorefresh_done(struct mdss_mdp_ctl *ctl)
{
	int rc;
	u32 val, line_out;
	char __iomem *pp_base = ctl->mixer_left->pingpong_base;
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	unsigned long flags;
	unsigned long autorefresh_timeout;

	line_out = mdss_mdp_pingpong_read(pp_base, MDSS_MDP_REG_PP_LINE_COUNT);

	MDSS_XLOG(ctl->num, line_out, ctl->mixer_left->roi.h);

	reinit_completion(&ctx->autorefresh_done);

	/* enable autorefresh done */
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
		ctx->current_pp_num, mdss_mdp_cmd_autorefresh_done, ctl);
	mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
		ctx->current_pp_num);

	/*
	 * Wait for autorefresh done, note that this interrupt would happen
	 * once the RD_PTR is reset to init value for the number of frames
	 * programmed with "autorefresh_frame_cnt", so this wait would take
	 * one RD_PTR reset, if autorefresh_frame_cnt = 1
	 * or the number of RD_PTR resets set by "autorefresh_frame_cnt".
	 */
	autorefresh_timeout = get_autorefresh_timeout(ctl,
		ctx, ctx->autorefresh_frame_cnt);
	rc = wait_for_completion_timeout(&ctx->autorefresh_done,
			autorefresh_timeout);

	if (rc <= 0) {
		u32 status, mask;

		mask = mdss_mdp_get_irq_mask(
				MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
				ctx->current_pp_num);
		status = mask & readl_relaxed(ctl->mdata->mdp_base +
				MDSS_MDP_REG_INTR_STATUS);

		if (status) {
			pr_warn("autorefresh done but irq not triggered\n");
			mdss_mdp_irq_clear(ctl->mdata,
				MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
				ctx->current_pp_num);
			local_irq_save(flags);
			mdss_mdp_irq_disable_nosync(
				MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
				ctx->current_pp_num);
			mdss_mdp_set_intr_callback_nosync(
				MDSS_MDP_IRQ_TYPE_PING_PONG_AUTO_REF,
				ctx->current_pp_num, NULL, NULL);
			local_irq_restore(flags);
			rc = 1;
		}
	}

	if (rc <= 0) {
		val = mdss_mdp_pingpong_read(pp_base,
			MDSS_MDP_REG_PP_LINE_COUNT);

		pr_err("timedout waiting for ctl%d autorefresh done line_cnt:%d frames:%d\n",
			ctl->num, val, ctx->autorefresh_frame_cnt);
		MDSS_XLOG(0xbad4, val);
		MDSS_XLOG_TOUT_HANDLER("mdp", "dsi0_ctrl", "dsi0_phy",
			"dsi1_ctrl", "dsi1_phy", "vbif", "vbif_nrt",
			"dbg_bus", "vbif_dbg_bus", "panic");
	}
}

/* caller needs to hold autorefresh_lock before calling this function */
static int mdss_mdp_disable_autorefresh(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_ctl *sctl)
{
	u32 cfg;
	struct mdss_mdp_cmd_ctx *ctx;
	char __iomem *pp_base = ctl->mixer_left->pingpong_base;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	MDSS_XLOG(ctx->autorefresh_state, ctx->autorefresh_frame_cnt);

	/*
	 * This can happen if driver gets sysfs request to enable autorefresh,
	 * and a CMD_STOP is received before autorefresh is turned on by
	 * the atomic commit.
	 */
	if (ctx->autorefresh_state == MDP_AUTOREFRESH_ON_REQUESTED) {
		ctx->autorefresh_state = MDP_AUTOREFRESH_OFF;
		ctx->autorefresh_frame_cnt = 0;
		return 0;
	}

	pr_debug("%pS->%s: disabling autorefresh\n",
		__builtin_return_address(0), __func__);

	/*
	 * Wait for autorefresh done before disabling it.
	 * This is intended for debug only; if enabled it would cause a large
	 * delay during disable due RD_PTR is program to wait for
	 * wrapping around, which can take hundreds of ms
	 */
	if (mdata->wait4autorefresh)
		mdss_mdp_cmd_wait4_autorefresh_done(ctl);

	/*
	 * To disable auto-refresh we need to make sure that no transfer
	 * is on-going when we write the bit to disable it.
	 * But since when autorefresh is enabled the HW automatically
	 * will trigger a transfer whenever external TE is received and
	 * the hw frame_cnt matches the programmed autorefresh frame_cnt,
	 * in order to have enough time to disable the feature we will instruct
	 * MDP to ignore the panel TE first; when doing this, the hw frame_cnt
	 * will be increased only when the internal counter wraps-around
	 * (instead of each time that the external panel TE is genarated),
	 * this gives us enough margin to disable autorefresh.
	 */
	cfg = mdss_mdp_pingpong_read(pp_base,
				     MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC);
	cfg &= ~BIT(20);
	mdss_mdp_pingpong_write(pp_base,
				MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);
	MDSS_XLOG(cfg);

	/* wait for previous transfer to finish */
	mdss_mdp_cmd_wait4_autorefresh_pp(ctl);
	if (sctl)
		mdss_mdp_cmd_wait4_autorefresh_pp(sctl);

	/* disable autorefresh */
	mdss_mdp_pingpong_write(pp_base, MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG, 0);

	if (is_pingpong_split(ctl->mfd))
		mdss_mdp_pingpong_write(mdata->slave_pingpong_base,
				MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG, 0);

	ctx->autorefresh_state = MDP_AUTOREFRESH_OFF;
	ctx->autorefresh_frame_cnt = 0;

	/* enable MDP to listen to the TE */
	cfg = mdss_mdp_pingpong_read(pp_base,
				     MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC);
	cfg |= BIT(20);
	mdss_mdp_pingpong_write(pp_base,
				MDSS_MDP_REG_PP_SYNC_CONFIG_VSYNC, cfg);

	ctl->mdata->serialize_wait4pp = false;
	return 0;
}


static void __mdss_mdp_kickoff(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_cmd_ctx *ctx)
{
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();
	bool is_pp_split = is_pingpong_split(ctl->mfd);

	MDSS_XLOG(ctx->autorefresh_state);

	if ((ctx->autorefresh_state == MDP_AUTOREFRESH_ON_REQUESTED) ||
		(ctx->autorefresh_state == MDP_AUTOREFRESH_ON)) {

		pr_debug("enabling autorefresh for every %d frames state %d\n",
			ctx->autorefresh_frame_cnt, ctx->autorefresh_state);

		/* Program HW to take care of Kickoff */
		mdss_mdp_pingpong_write(ctl->mixer_left->pingpong_base,
			MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG,
			BIT(31) | ctx->autorefresh_frame_cnt);

		if (is_pp_split)
			mdss_mdp_pingpong_write(mdata->slave_pingpong_base,
				MDSS_MDP_REG_PP_AUTOREFRESH_CONFIG,
				BIT(31) | ctx->autorefresh_frame_cnt);

		MDSS_XLOG(0x11, ctx->autorefresh_frame_cnt,
			ctx->autorefresh_state, is_pp_split);
		ctx->autorefresh_state = MDP_AUTOREFRESH_ON;

	} else {
		/* SW Kickoff */
		mdss_mdp_ctl_write(ctl, MDSS_MDP_REG_CTL_START, 1);
		MDSS_XLOG(0x11, ctx->autorefresh_state);
	}
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
static int mdss_mdp_cmd_kickoff(struct mdss_mdp_ctl *ctl, void *arg)
{
	struct mdss_mdp_ctl *sctl = NULL, *mctl = ctl;
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;
	struct mdss_data_type *mdata = mdss_mdp_get_mdata();

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	if (!ctx) {
		pr_err("invalid ctx\n");
		return -ENODEV;
	}

	if (ctx->intf_stopped) {
		pr_err("ctx=%d stopped already\n", ctx->current_pp_num);
		return -EPERM;
	}

	if (!ctl->is_master) {
		mctl = mdss_mdp_get_main_ctl(ctl);
	} else {
		sctl = mdss_mdp_get_split_ctl(ctl);
		if (sctl && (sctl->roi.w == 0 || sctl->roi.h == 0)) {
			/* left update only */
			sctl = NULL;
		}
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

	ctx->current_pp_num = ctx->default_pp_num;
	if (sctx)
		sctx->current_pp_num = sctx->default_pp_num;

	if (__mdss_mdp_cmd_is_aux_pp_needed(mdata, mctl))
		ctx->current_pp_num = ctx->aux_pp_num;

	MDSS_XLOG(ctl->num, ctx->current_pp_num,
		ctl->roi.x, ctl->roi.y, ctl->roi.w, ctl->roi.h);

	atomic_inc(&ctx->koff_cnt);
	if (sctx)
		atomic_inc(&sctx->koff_cnt);

	trace_mdp_cmd_kickoff(ctl->num, atomic_read(&ctx->koff_cnt));

	/*
	 * Call state machine with kickoff event, we just do it for
	 * current CTL, but internally state machine will check and
	 * if this is a dual dsi, it will enable the power resources
	 * for both DSIs
	 */
	mdss_mdp_resource_control(ctl, MDP_RSRC_CTL_EVENT_KICKOFF);

	if (!ctl->is_master)
		mctl = mdss_mdp_get_main_ctl(ctl);
	mdss_mdp_cmd_dsc_reconfig(mctl);

	mdss_mdp_cmd_set_partial_roi(ctl);

	/*
	 * tx dcs command if had any
	 */
	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_CMDLIST_KOFF, NULL,
		CTL_INTF_EVENT_FLAG_DEFAULT);

	mdss_mdp_cmd_set_stream_size(ctl);

	mdss_mdp_cmd_set_sync_ctx(ctl, sctl);

	mutex_lock(&ctx->autorefresh_lock);
	if (ctx->autorefresh_state == MDP_AUTOREFRESH_OFF_REQUESTED) {
		pr_debug("%s: disable autorefresh ctl%d\n", __func__, ctl->num);
		mdss_mdp_disable_autorefresh(ctl, sctl);
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
		ctx->current_pp_num, mdss_mdp_cmd_pingpong_done, ctl);
	mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
			ctx->current_pp_num);
	if (sctx) {
		mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
			sctx->current_pp_num, mdss_mdp_cmd_pingpong_done, sctl);
		mdss_mdp_irq_enable(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
			sctx->current_pp_num);
	}

	mdss_mdp_ctl_perf_set_transaction_status(ctl,
		PERF_SW_COMMIT_STATE, PERF_STATUS_DONE);
	if (sctl) {
		mdss_mdp_ctl_perf_set_transaction_status(sctl,
			PERF_SW_COMMIT_STATE, PERF_STATUS_DONE);
	}

	if (mdss_mdp_is_lineptr_supported(ctl)) {
		if (mdss_mdp_is_full_frame_update(ctl))
			mdss_mdp_cmd_lineptr_ctrl(ctl, true);
		else if (ctx->lineptr_enabled)
			mdss_mdp_cmd_lineptr_ctrl(ctl, false);
	}

	/* Kickoff */
	__mdss_mdp_kickoff(ctl, ctx);

	mdss_mdp_cmd_post_programming(ctl);

	/*
	 * If auto-refresh is enabled, wait for an autorefresh done,
	 * to make sure configuration has taken effect.
	 * Do this after post-programming, so TE is enabled.
	 */
	if (ctx->autorefresh_state == MDP_AUTOREFRESH_ON)
		mdss_mdp_cmd_wait4_autorefresh_done(ctl);

	mb();
	mutex_unlock(&ctx->autorefresh_lock);

	MDSS_XLOG(ctl->num, ctx->current_pp_num,
		sctx ? sctx->current_pp_num : -1, atomic_read(&ctx->koff_cnt));
	return 0;
}

int mdss_mdp_cmd_restore(struct mdss_mdp_ctl *ctl, bool locked)
{
	struct mdss_mdp_cmd_ctx *ctx, *sctx = NULL;

	if (!ctl)
		return -EINVAL;

	pr_debug("%s: called for ctl%d\n", __func__, ctl->num);

	ctx = (struct mdss_mdp_cmd_ctx *)ctl->intf_ctx[MASTER_CTX];
	if (is_pingpong_split(ctl->mfd)) {
		sctx = (struct mdss_mdp_cmd_ctx *)ctl->intf_ctx[SLAVE_CTX];
	} else if (ctl->mfd->split_mode == MDP_DUAL_LM_DUAL_DISPLAY) {
		struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

		if (sctl)
			sctx = (struct mdss_mdp_cmd_ctx *)
					sctl->intf_ctx[MASTER_CTX];
	}

	if (mdss_mdp_cmd_tearcheck_setup(ctx, locked)) {
		pr_warn("%s: ctx%d tearcheck setup failed\n", __func__,
			ctx->current_pp_num);
	} else {
		if (sctx && mdss_mdp_cmd_tearcheck_setup(sctx, locked))
			pr_warn("%s: ctx%d tearcheck setup failed\n", __func__,
				sctx->current_pp_num);
		else
			mdss_mdp_tearcheck_enable(ctl, true);
	}

	return 0;
}

int mdss_mdp_cmd_ctx_stop(struct mdss_mdp_ctl *ctl,
		struct mdss_mdp_cmd_ctx *ctx, int panel_power_state)
{
	struct mdss_mdp_cmd_ctx *sctx = NULL;
	struct mdss_mdp_ctl *sctl = NULL;

	sctl = mdss_mdp_get_split_ctl(ctl);
	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	/* intf stopped,  no more kickoff */
	ctx->intf_stopped = 1;

	/* Make sure any rd ptr for dsi callback is done before disable vsync */
	if (is_pingpong_split(ctl->mfd)) {
		pr_debug("%s will wait for rd ptr:%d\n", __func__,
			atomic_read(&ctx->rdptr_cnt));
		MDSS_XLOG(atomic_read(&ctx->rdptr_cnt));
		mdss_mdp_cmd_wait4readptr(ctx);
	}

	/*
	 * if any vsyncs are still enabled, loop until the refcount
	 * goes to zero, so the rd ptr interrupt is disabled.
	 * Ideally this shouldn't be the case since vsync handlers
	 * has been flushed by now, so issue a warning in case
	 * that we hit this condition.
	 */
	if (ctx->vsync_irq_cnt) {
		WARN(1, "vsync still enabled\n");
		while (mdss_mdp_setup_vsync(ctx, false))
			;
	}
	if (ctx->lineptr_irq_cnt) {
		WARN(1, "lineptr irq still enabled\n");
		while (mdss_mdp_setup_lineptr(ctx, false))
			;
	}

	if (!ctl->pending_mode_switch) {
		mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_REGISTER_RECOVERY_HANDLER,
			NULL, CTL_INTF_EVENT_FLAG_DEFAULT);

		mdss_mdp_ctl_intf_event(ctl,
			MDSS_EVENT_REGISTER_MDP_CALLBACK,
			NULL, CTL_INTF_EVENT_FLAG_DEFAULT);
	}

	/* shut down the MDP/DSI resources if still enabled */
	mdss_mdp_resource_control(ctl, MDP_RSRC_CTL_EVENT_STOP);

	flush_work(&ctx->pp_done_work);

	if (mdss_panel_is_power_off(panel_power_state) ||
	    mdss_panel_is_power_on_ulp(panel_power_state))
		mdss_mdp_tearcheck_enable(ctl, false);

	if (mdss_panel_is_power_on(panel_power_state)) {
		pr_debug("%s: intf stopped with panel on\n", __func__);
		return 0;
	}

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR,
		ctx->default_pp_num, NULL, NULL);
	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR,
		ctx->default_pp_num, NULL, NULL);
	mdss_mdp_set_intr_callback_nosync(MDSS_MDP_IRQ_TYPE_PING_PONG_COMP,
		ctx->default_pp_num, NULL, NULL);

	memset(ctx, 0, sizeof(*ctx));
	/* intf stopped,  no more kickoff */
	ctx->intf_stopped = 1;

	return 0;
}

static int mdss_mdp_cmd_intfs_stop(struct mdss_mdp_ctl *ctl, int session,
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
	if (mdss_mdp_is_lineptr_supported(ctl))
		mdss_mdp_cmd_lineptr_ctrl(ctl, false);
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), XLOG_FUNC_ENTRY);

	/* Command mode is supported only starting at INTF1 */
	session = ctl->intf_num - MDSS_MDP_INTF1;
	return mdss_mdp_cmd_intfs_stop(ctl, session, panel_power_state);
}

int mdss_mdp_cmd_stop(struct mdss_mdp_ctl *ctl, int panel_power_state)
{
	struct mdss_mdp_cmd_ctx *ctx = ctl->intf_ctx[MASTER_CTX];
	struct mdss_mdp_cmd_ctx *sctx = NULL;
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

	if (sctl)
		sctx = (struct mdss_mdp_cmd_ctx *) sctl->intf_ctx[MASTER_CTX];

	MDSS_XLOG(ctx->panel_power_state, panel_power_state);

	mutex_lock(&ctl->offlock);
	mutex_lock(&cmd_off_mtx);
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
			mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_REGISTER_RECOVERY_HANDLER,
				(void *)&ctx->intf_recovery,
				CTL_INTF_EVENT_FLAG_DEFAULT);

			mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_REGISTER_MDP_CALLBACK,
				(void *)&ctx->intf_mdp_callback,
				CTL_INTF_EVENT_FLAG_DEFAULT);

			ctx->intf_stopped = 0;
			if (sctx)
				sctx->intf_stopped = 0;
			/*
			 * Tearcheck was disabled while entering LP2 state.
			 * Enable it back to allow updates in LP1 state.
			 */
			mdss_mdp_tearcheck_enable(ctl, true);
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
	if ((!is_panel_split(ctl->mfd) || is_pingpong_split(ctl->mfd) ||
	    (is_panel_split(ctl->mfd) && sctl)) && send_panel_events) {
		pr_debug("%s: send panel events\n", __func__);
		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_BLANK,
				(void *) (long int) panel_power_state,
				CTL_INTF_EVENT_FLAG_DEFAULT);
		WARN(ret, "intf %d unblank error (%d)\n", ctl->intf_num, ret);

		ret = mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_OFF,
				(void *) (long int) panel_power_state,
				CTL_INTF_EVENT_FLAG_DEFAULT);
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
	if (!IS_ERR_VALUE(ret)) {
		struct mdss_mdp_cmd_ctx *sctx = NULL;

		ctx->panel_power_state = panel_power_state;
		/* In pingpong split we have single controller, dual context */
		if (is_pingpong_split(ctl->mfd))
			sctx = (struct mdss_mdp_cmd_ctx *)
					ctl->intf_ctx[SLAVE_CTX];
		if (sctx)
			sctx->panel_power_state = panel_power_state;
	}

	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt), XLOG_FUNC_EXIT);
	mutex_unlock(&cmd_off_mtx);
	mutex_unlock(&ctl->offlock);
	pr_debug("%s:-\n", __func__);

	return ret;
}

static void early_wakeup_work(struct work_struct *work)
{
	int rc = 0;
	struct mdss_mdp_cmd_ctx *ctx =
		container_of(work, typeof(*ctx), early_wakeup_clk_work);
	struct mdss_mdp_ctl *ctl;

	if (!ctx) {
		pr_err("%s: invalid ctx\n", __func__);
		return;
	}

	ATRACE_BEGIN(__func__);
	ctl = ctx->ctl;

	if (!ctl) {
		pr_err("%s: invalid ctl\n", __func__);
		goto fail;
	}

	rc = mdss_mdp_resource_control(ctl, MDP_RSRC_CTL_EVENT_EARLY_WAKE_UP);
	if (rc)
		pr_err("%s: failed to control resources\n", __func__);

fail:
	ATRACE_END(__func__);
}

static int mdss_mdp_cmd_early_wake_up(struct mdss_mdp_ctl *ctl)
{
	u64 curr_time;
	struct mdss_mdp_cmd_ctx *ctx;

	curr_time = ktime_to_us(ktime_get());

	if ((curr_time - ctl->last_input_time) <
			INPUT_EVENT_HANDLER_DELAY_USECS)
		return 0;
	ctl->last_input_time = curr_time;

	ctx = (struct mdss_mdp_cmd_ctx *) ctl->intf_ctx[MASTER_CTX];
	/*
	 * Early wake up event is called from an interrupt context and
	 * involves cancelling queued work items. So this will be
	 * scheduled in a work item.
	 * Only schedule if the interface has not been stopped.
	 */
	if (ctx && !ctx->intf_stopped)
		schedule_work(&ctx->early_wakeup_clk_work);
	return 0;
}

static int mdss_mdp_cmd_ctx_setup(struct mdss_mdp_ctl *ctl,
	struct mdss_mdp_cmd_ctx *ctx, int default_pp_num, int aux_pp_num,
	bool pingpong_split_slave)
{
	int ret = 0;

	/*
	 * Initialize the mutex before the ctl is assigned,
	 * so we can prevent any race condition with the
	 * initialization of the the mutex and the autorefresh
	 * sysfs.
	 */
	mutex_init(&ctx->autorefresh_lock);

	ctx->ctl = ctl;
	ctx->default_pp_num = default_pp_num;
	ctx->aux_pp_num = aux_pp_num;
	ctx->pingpong_split_slave = pingpong_split_slave;
	ctx->pp_timeout_report_cnt = 0;
	init_waitqueue_head(&ctx->pp_waitq);
	init_waitqueue_head(&ctx->rdptr_waitq);
	init_completion(&ctx->stop_comp);
	init_completion(&ctx->autorefresh_ppdone);
	init_completion(&ctx->rdptr_done);
	init_completion(&ctx->pp_done);
	init_completion(&ctx->autorefresh_done);
	spin_lock_init(&ctx->clk_lock);
	spin_lock_init(&ctx->koff_lock);
	mutex_init(&ctx->clk_mtx);
	mutex_init(&ctx->mdp_rdptr_lock);
	mutex_init(&ctx->mdp_wrptr_lock);
	INIT_WORK(&ctx->gate_clk_work, clk_ctrl_gate_work);
	INIT_DELAYED_WORK(&ctx->delayed_off_clk_work,
		clk_ctrl_delayed_off_work);
	INIT_WORK(&ctx->pp_done_work, pingpong_done_work);
	INIT_WORK(&ctx->early_wakeup_clk_work, early_wakeup_work);
	atomic_set(&ctx->pp_done_cnt, 0);
	ctx->autorefresh_state = MDP_AUTOREFRESH_OFF;
	ctx->autorefresh_frame_cnt = 0;
	INIT_LIST_HEAD(&ctx->vsync_handlers);
	INIT_LIST_HEAD(&ctx->lineptr_handlers);

	ctx->intf_recovery.fxn = mdss_mdp_cmd_intf_recovery;
	ctx->intf_recovery.data = ctx;

	ctx->intf_mdp_callback.fxn = mdss_mdp_cmd_intf_callback;
	ctx->intf_mdp_callback.data = ctx;

	ctx->intf_stopped = 0;

	pr_debug("%s: ctx=%pK num=%d aux=%d\n", __func__, ctx,
		default_pp_num, aux_pp_num);
	MDSS_XLOG(ctl->num, atomic_read(&ctx->koff_cnt));

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_PING_PONG_RD_PTR,
		ctx->default_pp_num, mdss_mdp_cmd_readptr_done, ctl);

	mdss_mdp_set_intr_callback(MDSS_MDP_IRQ_TYPE_PING_PONG_WR_PTR,
		ctx->default_pp_num, mdss_mdp_cmd_lineptr_done, ctl);

	ret = mdss_mdp_cmd_tearcheck_setup(ctx, false);
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
	u32 default_pp_num, aux_pp_num;

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
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			mdss_mdp_cmd_restore(ctl, false);
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);

			/* Turn on panel so that it can exit low power mode */
			return mdss_mdp_cmd_panel_on(ctl, sctl);
		} else {
			pr_err("Intf %d already in use\n", session);
			return -EBUSY;
		}
	}
	ctx->ref_cnt++;
	ctl->intf_ctx[MASTER_CTX] = ctx;


	mixer = mdss_mdp_mixer_get(ctl, MDSS_MDP_MIXER_MUX_LEFT);
	if (!mixer) {
		pr_err("mixer not setup correctly\n");
		return -ENODEV;
	}
	default_pp_num = mixer->num;

	if (is_split_lm(ctl->mfd)) {
		if (is_dual_lm_single_display(ctl->mfd)) {
			mixer = mdss_mdp_mixer_get(ctl,
				MDSS_MDP_MIXER_MUX_RIGHT);
			if (!mixer) {
				pr_err("right mixer not setup correctly for dual_lm_single_display\n");
				return -ENODEV;
			}
			aux_pp_num = mixer->num;
		} else { /* DUAL_LM_DUAL_DISPLAY */
			struct mdss_mdp_ctl *mctl = ctl;

			if (!mctl->is_master) {
				mctl = mdss_mdp_get_main_ctl(ctl);
				if (!mctl) {
					pr_err("%s master ctl cannot be NULL\n",
						__func__);
					return -EINVAL;
				}
			}

			if (ctl->is_master) /* setup is called for master */
				mixer = mdss_mdp_mixer_get(mctl,
					MDSS_MDP_MIXER_MUX_RIGHT);
			else
				mixer = mdss_mdp_mixer_get(mctl,
					MDSS_MDP_MIXER_MUX_LEFT);

			if (!mixer) {
				pr_err("right mixer not setup correctly for dual_lm_dual_display\n");
				return -ENODEV;
			}
			aux_pp_num = mixer->num;
		}
	} else {
		aux_pp_num = default_pp_num;
	}

	ret = mdss_mdp_cmd_ctx_setup(ctl, ctx,
		default_pp_num, aux_pp_num, false);
	if (ret) {
		pr_err("mdss_mdp_cmd_ctx_setup failed for default_pp:%d aux_pp:%d\n",
			default_pp_num, aux_pp_num);
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
				mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
				mdss_mdp_cmd_restore(ctl, false);
				mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
				return mdss_mdp_cmd_panel_on(ctl, sctl);
			} else {
				pr_err("Intf %d already in use\n", session);
				return -EBUSY;
			}
		}
		ctx->ref_cnt++;

		ctl->intf_ctx[SLAVE_CTX] = ctx;

		ret = mdss_mdp_cmd_ctx_setup(ctl, ctx, session, session, true);
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
	struct mdss_mdp_ctl *mctl = ctl;
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);

	if (!ctl->panel_data ||
	  !ctl->panel_data->panel_info.partial_update_supported)
		return;

	ctl->panel_data->panel_info.roi = ctl->roi;
	if (sctl && sctl->panel_data)
		sctl->panel_data->panel_info.roi = sctl->roi;

	if (!ctl->is_master)
		mctl = mdss_mdp_get_main_ctl(ctl);
	mdss_mdp_cmd_dsc_reconfig(mctl);

	mdss_mdp_cmd_set_partial_roi(ctl);
}

void mdss_mdp_switch_to_vid_mode(struct mdss_mdp_ctl *ctl, int prep)
{
	struct mdss_mdp_ctl *sctl = mdss_mdp_get_split_ctl(ctl);
	struct dsi_panel_clk_ctrl clk_ctrl;
	long int mode = MIPI_VIDEO_PANEL;

	pr_debug("%s start, prep = %d\n", __func__, prep);

	if (prep) {
		/*
		 * In dsi_on there is an explicit decrement to dsi clk refcount
		 * if we are in cmd mode, using the dsi client handle. We need
		 * to rebalance clock in order to properly enable vid mode
		 * compnents.
		 */
		clk_ctrl.state = MDSS_DSI_CLK_ON;
		clk_ctrl.client = DSI_CLK_REQ_DSI_CLIENT;
		if (sctl)
			mdss_mdp_ctl_intf_event(sctl,
				MDSS_EVENT_PANEL_CLK_CTRL, (void *)&clk_ctrl,
				CTL_INTF_EVENT_FLAG_SKIP_BROADCAST);

		mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_PANEL_CLK_CTRL,
			(void *)&clk_ctrl, CTL_INTF_EVENT_FLAG_SKIP_BROADCAST);

		return;
	}

	mdss_mdp_ctl_intf_event(ctl, MDSS_EVENT_DSI_RECONFIG_CMD,
			(void *) mode, CTL_INTF_EVENT_FLAG_DEFAULT);
}

static int mdss_mdp_cmd_reconfigure(struct mdss_mdp_ctl *ctl,
		enum dynamic_switch_modes mode, bool prep)
{
	if (mdss_mdp_ctl_is_power_off(ctl))
		return 0;

	pr_debug("%s: ctl=%d mode=%d prep=%d\n", __func__,
			ctl->num, mode, prep);

	if (mode == SWITCH_TO_VIDEO_MODE) {
		mdss_mdp_switch_to_vid_mode(ctl, prep);
	} else if (mode == SWITCH_RESOLUTION) {
		if (prep) {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_ON);
			/*
			 * Setup DSC conifg early, as DSI configuration during
			 * resolution switch would rely on DSC params for
			 * stream configs.
			 */
			mdss_mdp_cmd_dsc_reconfig(ctl);

			/*
			 * Make explicit cmd_panel_on call, when dynamic
			 * resolution switch request comes before cont-splash
			 * handoff, to match the ctl_stop/ctl_start done
			 * during the reconfiguration.
			 */
			if (ctl->switch_with_handoff) {
				struct mdss_mdp_cmd_ctx *ctx;
				struct mdss_mdp_ctl *sctl;

				ctx = (struct mdss_mdp_cmd_ctx *)
					ctl->intf_ctx[MASTER_CTX];
				if (ctx &&
				     __mdss_mdp_cmd_is_panel_power_off(ctx)) {
					sctl = mdss_mdp_get_split_ctl(ctl);
					mdss_mdp_cmd_panel_on(ctl, sctl);
				}
				ctl->switch_with_handoff = false;
			}

			mdss_mdp_ctl_stop(ctl, MDSS_PANEL_POWER_OFF);
			mdss_mdp_ctl_intf_event(ctl,
				MDSS_EVENT_DSI_DYNAMIC_SWITCH,
				(void *) mode, CTL_INTF_EVENT_FLAG_DEFAULT);
		} else {
			mdss_mdp_clk_ctrl(MDP_BLOCK_POWER_OFF);
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
	ctl->ops.early_wake_up_fnc = mdss_mdp_cmd_early_wake_up;
	ctl->ops.reconfigure = mdss_mdp_cmd_reconfigure;
	ctl->ops.pre_programming = mdss_mdp_cmd_pre_programming;
	ctl->ops.update_lineptr = mdss_mdp_cmd_update_lineptr;
	pr_debug("%s:-\n", __func__);

	return 0;
}

