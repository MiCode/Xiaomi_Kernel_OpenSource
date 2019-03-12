/*
 * Copyright (c) 2015-2019 The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include "sde_encoder_phys.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"
#include "sde_formats.h"
#include "sde_trace.h"

#define SDE_DEBUG_CMDENC(e, fmt, ...) SDE_DEBUG("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) ? (e)->base.intf_idx - INTF_0 : -1, ##__VA_ARGS__)

#define SDE_ERROR_CMDENC(e, fmt, ...) SDE_ERROR("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) ? (e)->base.intf_idx - INTF_0 : -1, ##__VA_ARGS__)

#define to_sde_encoder_phys_cmd(x) \
	container_of(x, struct sde_encoder_phys_cmd, base)

#define PP_TIMEOUT_MAX_TRIALS	4

/*
 * Tearcheck sync start and continue thresholds are empirically found
 * based on common panels In the future, may want to allow panels to override
 * these default values
 */
#define DEFAULT_TEARCHECK_SYNC_THRESH_START	4
#define DEFAULT_TEARCHECK_SYNC_THRESH_CONTINUE	4

#define SDE_ENC_WR_PTR_START_TIMEOUT_US 20000

/*
 * Threshold for signalling retire fences in cases where
 * CTL_START_IRQ is received just after RD_PTR_IRQ
 */
#define SDE_ENC_CTL_START_THRESHOLD_US 500

#define SDE_ENC_MAX_POLL_TIMEOUT_US	2000

static inline int _sde_encoder_phys_cmd_get_idle_timeout(
		struct sde_encoder_phys_cmd *cmd_enc)
{
	return cmd_enc->autorefresh.cfg.frame_count ?
			cmd_enc->autorefresh.cfg.frame_count *
			KICKOFF_TIMEOUT_MS : KICKOFF_TIMEOUT_MS;
}

static inline bool sde_encoder_phys_cmd_is_master(
		struct sde_encoder_phys *phys_enc)
{
	return (phys_enc->split_role != ENC_ROLE_SLAVE) ? true : false;
}

static bool sde_encoder_phys_cmd_mode_fixup(
		struct sde_encoder_phys *phys_enc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	if (phys_enc)
		SDE_DEBUG_CMDENC(to_sde_encoder_phys_cmd(phys_enc), "\n");
	return true;
}

static uint64_t _sde_encoder_phys_cmd_get_autorefresh_property(
		struct sde_encoder_phys *phys_enc)
{
	struct drm_connector *conn = phys_enc->connector;

	if (!conn || !conn->state)
		return 0;

	return sde_connector_get_property(conn->state,
				CONNECTOR_PROP_AUTOREFRESH);
}

static void _sde_encoder_phys_cmd_config_autorefresh(
		struct sde_encoder_phys *phys_enc,
		u32 new_frame_count)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_pingpong *hw_pp = phys_enc->hw_pp;
	struct sde_hw_intf *hw_intf = phys_enc->hw_intf;
	struct drm_connector *conn = phys_enc->connector;
	struct sde_hw_autorefresh *cfg_cur, cfg_nxt;

	if (!conn || !conn->state || !hw_pp || !hw_intf)
		return;

	cfg_cur = &cmd_enc->autorefresh.cfg;

	/* autorefresh property value should be validated already */
	memset(&cfg_nxt, 0, sizeof(cfg_nxt));
	cfg_nxt.frame_count = new_frame_count;
	cfg_nxt.enable = (cfg_nxt.frame_count != 0);

	SDE_DEBUG_CMDENC(cmd_enc, "autorefresh state %d->%d framecount %d\n",
			cfg_cur->enable, cfg_nxt.enable, cfg_nxt.frame_count);
	SDE_EVT32(DRMID(phys_enc->parent), hw_pp->idx, hw_intf->idx,
			cfg_cur->enable, cfg_nxt.enable, cfg_nxt.frame_count);

	/* only proceed on state changes */
	if (cfg_nxt.enable == cfg_cur->enable)
		return;

	memcpy(cfg_cur, &cfg_nxt, sizeof(*cfg_cur));

	if (phys_enc->has_intf_te && hw_intf->ops.setup_autorefresh)
		hw_intf->ops.setup_autorefresh(hw_intf, cfg_cur);
	else if (hw_pp->ops.setup_autorefresh)
		hw_pp->ops.setup_autorefresh(hw_pp, cfg_cur);
}

static void _sde_encoder_phys_cmd_update_flush_mask(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc;
	struct sde_hw_ctl *ctl;

	if (!phys_enc || !phys_enc->hw_intf || !phys_enc->hw_pp)
		return;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);
	ctl = phys_enc->hw_ctl;

	if (!ctl)
		return;

	if (!ctl->ops.update_bitmask_intf ||
		(test_bit(SDE_CTL_ACTIVE_CFG, &ctl->caps->features) &&
		!ctl->ops.update_bitmask_merge3d)) {
		SDE_ERROR("invalid hw_ctl ops %d\n", ctl->idx);
		return;
	}

	ctl->ops.update_bitmask_intf(ctl, phys_enc->intf_idx, 1);

	if (ctl->ops.update_bitmask_merge3d && phys_enc->hw_pp->merge_3d)
		ctl->ops.update_bitmask_merge3d(ctl,
			phys_enc->hw_pp->merge_3d->idx, 1);

	SDE_DEBUG_CMDENC(cmd_enc, "update pending flush ctl %d intf_idx %x\n",
			ctl->idx - CTL_0, phys_enc->intf_idx);
}

static void _sde_encoder_phys_cmd_update_intf_cfg(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_ctl *ctl;

	if (!phys_enc)
		return;

	ctl = phys_enc->hw_ctl;
	if (!ctl)
		return;

	if (ctl->ops.setup_intf_cfg) {
		struct sde_hw_intf_cfg intf_cfg = { 0 };

		intf_cfg.intf = phys_enc->intf_idx;
		intf_cfg.intf_mode_sel = SDE_CTL_MODE_SEL_CMD;
		intf_cfg.stream_sel = cmd_enc->stream_sel;
		intf_cfg.mode_3d =
			sde_encoder_helper_get_3d_blend_mode(phys_enc);
		ctl->ops.setup_intf_cfg(ctl, &intf_cfg);
	} else if (test_bit(SDE_CTL_ACTIVE_CFG, &ctl->caps->features)) {
		sde_encoder_helper_update_intf_cfg(phys_enc);
	}
}

static void sde_encoder_phys_cmd_pp_tx_done_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	unsigned long lock_flags;
	int new_cnt;
	u32 event = SDE_ENCODER_FRAME_EVENT_DONE |
			SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE;

	if (!phys_enc || !phys_enc->hw_pp)
		return;

	SDE_ATRACE_BEGIN("pp_done_irq");

	/* handle rare cases where the ctl_start_irq is not received */
	if (sde_encoder_phys_cmd_is_master(phys_enc)) {
		/*
		 * Reduce the refcount for the retire fence as well
		 * as for the ctl_start if the counters are greater
		 * than zero. If there was a retire fence count pending,
		 * then signal the RETIRE FENCE here.
		 */
		if (atomic_add_unless(&phys_enc->pending_retire_fence_cnt,
				-1, 0))
			phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent,
				phys_enc,
				SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE);
		atomic_add_unless(&phys_enc->pending_ctlstart_cnt, -1, 0);
		atomic_set(&phys_enc->ctlstart_timeout, 0);
	}

	/* notify all synchronous clients first, then asynchronous clients */
	if (phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
				phys_enc, event);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	new_cnt = atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0, new_cnt, event);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&phys_enc->pending_kickoff_wq);
	SDE_ATRACE_END("pp_done_irq");
}

static void sde_encoder_phys_cmd_autorefresh_done_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	unsigned long lock_flags;
	int new_cnt;

	if (!cmd_enc)
		return;

	phys_enc = &cmd_enc->base;
	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	new_cnt = atomic_add_unless(&cmd_enc->autorefresh.kickoff_cnt, -1, 0);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			new_cnt);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&cmd_enc->autorefresh.kickoff_wq);
}

static void sde_encoder_phys_cmd_te_rd_ptr_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	struct sde_encoder_phys_cmd *cmd_enc;
	u32 event = 0;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf)
		return;

	SDE_ATRACE_BEGIN("rd_ptr_irq");
	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	/**
	 * signal only for master, when the ctl_start irq is
	 * done and incremented the pending_rd_ptr_cnt.
	 */
	if (sde_encoder_phys_cmd_is_master(phys_enc)
		    && atomic_add_unless(&cmd_enc->pending_rd_ptr_cnt, -1, 0)
		    && atomic_add_unless(
				&phys_enc->pending_retire_fence_cnt, -1, 0)) {

		event = SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE;
		if (phys_enc->parent_ops.handle_frame_done)
			phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc, event);
	}

	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			event, 0xfff);

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
			phys_enc);

	cmd_enc->rd_ptr_timestamp = ktime_get();

	atomic_add_unless(&cmd_enc->pending_vblank_cnt, -1, 0);
	wake_up_all(&cmd_enc->pending_vblank_wq);
	SDE_ATRACE_END("rd_ptr_irq");
}

static void sde_encoder_phys_cmd_ctl_start_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	struct sde_encoder_phys_cmd *cmd_enc;
	struct sde_hw_ctl *ctl;
	u32 event = 0;
	s64 time_diff_us;

	if (!phys_enc || !phys_enc->hw_ctl)
		return;

	SDE_ATRACE_BEGIN("ctl_start_irq");
	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	ctl = phys_enc->hw_ctl;
	atomic_add_unless(&phys_enc->pending_ctlstart_cnt, -1, 0);
	atomic_set(&phys_enc->ctlstart_timeout, 0);

	time_diff_us = ktime_us_delta(ktime_get(), cmd_enc->rd_ptr_timestamp);

	/* handle retire fence based on only master */
	if (sde_encoder_phys_cmd_is_master(phys_enc)
			&& atomic_read(&phys_enc->pending_retire_fence_cnt)) {
		/**
		 * Handle rare cases where the ctl_start_irq is received
		 * after rd_ptr_irq. If it falls within a threshold, it is
		 * guaranteed the frame would be picked up in the current TE.
		 * Signal retire fence immediately in such case. The threshold
		 * timer adds extra line time duration based on lowest panel
		 * fps for qsync enabled case.
		 */
		if ((time_diff_us <= cmd_enc->ctl_start_threshold)
			    && atomic_add_unless(
				&phys_enc->pending_retire_fence_cnt, -1, 0)) {

			event = SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE;

			if (phys_enc->parent_ops.handle_frame_done)
				phys_enc->parent_ops.handle_frame_done(
					phys_enc->parent, phys_enc, event);

		/**
		 * In ideal cases, ctl_start_irq is received before the
		 * rd_ptr_irq, so set the atomic flag to indicate the event
		 * and rd_ptr_irq will handle signalling the retire fence
		 */
		} else {
			atomic_inc(&cmd_enc->pending_rd_ptr_cnt);
		}
	}

	SDE_EVT32_IRQ(DRMID(phys_enc->parent), ctl->idx - CTL_0,
				time_diff_us, event, 0xfff);

	/* Signal any waiting ctl start interrupt */
	wake_up_all(&phys_enc->pending_kickoff_wq);
	SDE_ATRACE_END("ctl_start_irq");
}

static void sde_encoder_phys_cmd_underrun_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;

	if (!phys_enc)
		return;

	if (phys_enc->parent_ops.handle_underrun_virt)
		phys_enc->parent_ops.handle_underrun_virt(phys_enc->parent,
			phys_enc);
}

static void _sde_encoder_phys_cmd_setup_irq_hw_idx(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_irq *irq;
	int ret = 0;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid args %d %d\n", !phys_enc,
			phys_enc ? !phys_enc->hw_pp : 0);
		return;
	}

	if (phys_enc->has_intf_te && !phys_enc->hw_intf) {
		SDE_ERROR("invalid intf configuration\n");
		return;
	}

	mutex_lock(phys_enc->vblank_ctl_lock);

	if (atomic_read(&phys_enc->vblank_refcount)) {
		SDE_ERROR(
		"vblank_refcount mismatch detected, try to reset %d\n",
				atomic_read(&phys_enc->vblank_refcount));
		ret = sde_encoder_helper_unregister_irq(phys_enc,
				INTR_IDX_RDPTR);
		if (ret)
			SDE_ERROR(
			"control vblank irq registration error %d\n",
				ret);
	}

	atomic_set(&phys_enc->vblank_refcount, 0);

	irq = &phys_enc->irq[INTR_IDX_CTL_START];
	irq->hw_idx = phys_enc->hw_ctl->idx;
	irq->irq_idx = -EINVAL;

	irq = &phys_enc->irq[INTR_IDX_PINGPONG];
	irq->hw_idx = phys_enc->hw_pp->idx;
	irq->irq_idx = -EINVAL;

	irq = &phys_enc->irq[INTR_IDX_RDPTR];
	irq->irq_idx = -EINVAL;
	if (phys_enc->has_intf_te)
		irq->hw_idx = phys_enc->hw_intf->idx;
	else
		irq->hw_idx = phys_enc->hw_pp->idx;

	irq = &phys_enc->irq[INTR_IDX_UNDERRUN];
	irq->hw_idx = phys_enc->intf_idx;
	irq->irq_idx = -EINVAL;

	irq = &phys_enc->irq[INTR_IDX_AUTOREFRESH_DONE];
	irq->irq_idx = -EINVAL;
	if (phys_enc->has_intf_te)
		irq->hw_idx = phys_enc->hw_intf->idx;
	else
		irq->hw_idx = phys_enc->hw_pp->idx;

	mutex_unlock(phys_enc->vblank_ctl_lock);
}

static void sde_encoder_phys_cmd_cont_splash_mode_set(
		struct sde_encoder_phys *phys_enc,
		struct drm_display_mode *adj_mode)
{
	struct sde_hw_intf *hw_intf;
	struct sde_hw_pingpong *hw_pp;
	struct sde_encoder_phys_cmd *cmd_enc;

	if (!phys_enc || !adj_mode) {
		SDE_ERROR("invalid args\n");
		return;
	}

	phys_enc->cached_mode = *adj_mode;
	phys_enc->enable_state = SDE_ENC_ENABLED;

	if (!phys_enc->hw_ctl || !phys_enc->hw_pp) {
		SDE_DEBUG("invalid ctl:%d pp:%d\n",
			(phys_enc->hw_ctl == NULL),
			(phys_enc->hw_pp == NULL));
		return;
	}

	if (sde_encoder_phys_cmd_is_master(phys_enc)) {
		cmd_enc = to_sde_encoder_phys_cmd(phys_enc);
		hw_pp = phys_enc->hw_pp;
		hw_intf = phys_enc->hw_intf;

		if (phys_enc->has_intf_te && hw_intf &&
				hw_intf->ops.get_autorefresh) {
			hw_intf->ops.get_autorefresh(hw_intf,
					&cmd_enc->autorefresh.cfg);
		} else if (hw_pp && hw_pp->ops.get_autorefresh) {
			hw_pp->ops.get_autorefresh(hw_pp,
					&cmd_enc->autorefresh.cfg);
		}
	}

	_sde_encoder_phys_cmd_setup_irq_hw_idx(phys_enc);
}

static void sde_encoder_phys_cmd_mode_set(
		struct sde_encoder_phys *phys_enc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	struct sde_rm *rm = &phys_enc->sde_kms->rm;
	struct sde_rm_hw_iter iter;
	int i, instance;

	if (!phys_enc || !mode || !adj_mode) {
		SDE_ERROR("invalid args\n");
		return;
	}
	phys_enc->cached_mode = *adj_mode;
	SDE_DEBUG_CMDENC(cmd_enc, "caching mode:\n");
	drm_mode_debug_printmodeline(adj_mode);

	instance = phys_enc->split_role == ENC_ROLE_SLAVE ? 1 : 0;

	/* Retrieve previously allocated HW Resources. Shouldn't fail */
	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_CTL);
	for (i = 0; i <= instance; i++) {
		if (sde_rm_get_hw(rm, &iter))
			phys_enc->hw_ctl = (struct sde_hw_ctl *)iter.hw;
	}

	if (IS_ERR_OR_NULL(phys_enc->hw_ctl)) {
		SDE_ERROR_CMDENC(cmd_enc, "failed to init ctl: %ld\n",
				PTR_ERR(phys_enc->hw_ctl));
		phys_enc->hw_ctl = NULL;
		return;
	}

	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_INTF);
	for (i = 0; i <= instance; i++) {
		if (sde_rm_get_hw(rm, &iter))
			phys_enc->hw_intf = (struct sde_hw_intf *)iter.hw;
	}

	if (IS_ERR_OR_NULL(phys_enc->hw_intf)) {
		SDE_ERROR_CMDENC(cmd_enc, "failed to init intf: %ld\n",
				PTR_ERR(phys_enc->hw_intf));
		phys_enc->hw_intf = NULL;
		return;
	}

	_sde_encoder_phys_cmd_setup_irq_hw_idx(phys_enc);
}

static int _sde_encoder_phys_cmd_handle_ppdone_timeout(
		struct sde_encoder_phys *phys_enc,
		bool recovery_events)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	u32 frame_event = SDE_ENCODER_FRAME_EVENT_ERROR
				| SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE;
	struct drm_connector *conn;
	struct sde_connector *sde_conn;
	int event;
	u32 pending_kickoff_cnt;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_ctl)
		return -EINVAL;

	conn = phys_enc->connector;
	sde_conn = to_sde_connector(conn);
	cmd_enc->pp_timeout_report_cnt++;
	pending_kickoff_cnt = atomic_read(&phys_enc->pending_kickoff_cnt);

	if (sde_encoder_phys_cmd_is_master(phys_enc)) {
		 /* trigger the retire fence if it was missed */
		if (atomic_add_unless(&phys_enc->pending_retire_fence_cnt,
				-1, 0))
			phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent,
				phys_enc,
				SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE);
		atomic_add_unless(&phys_enc->pending_ctlstart_cnt, -1, 0);
	}

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			cmd_enc->pp_timeout_report_cnt,
			pending_kickoff_cnt,
			frame_event);

	/* decrement the kickoff_cnt before checking for ESD status */
	atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);

	/* check if panel is still sending TE signal or not */
	if (sde_connector_esd_status(phys_enc->connector) ||
	    sde_conn->panel_dead)
		goto exit;

	/* to avoid flooding, only log first time, and "dead" time */
	if (cmd_enc->pp_timeout_report_cnt == 1) {
		SDE_ERROR_CMDENC(cmd_enc,
				"pp:%d kickoff timed out ctl %d koff_cnt %d\n",
				phys_enc->hw_pp->idx - PINGPONG_0,
				phys_enc->hw_ctl->idx - CTL_0,
				pending_kickoff_cnt);

		SDE_EVT32(DRMID(phys_enc->parent), SDE_EVTLOG_FATAL);
		sde_encoder_helper_unregister_irq(phys_enc, INTR_IDX_RDPTR);
		if (sde_kms_is_secure_session_inprogress(phys_enc->sde_kms))
			SDE_DBG_DUMP("secure");
		else
			SDE_DBG_DUMP("all", "dbg_bus", "vbif_dbg_bus");
		sde_encoder_helper_register_irq(phys_enc, INTR_IDX_RDPTR);
	}

	/*
	 * if the recovery event is registered by user, don't panic
	 * trigger panic on first timeout if no listener registered
	 */
	if (recovery_events) {
		event = cmd_enc->pp_timeout_report_cnt > PP_TIMEOUT_MAX_TRIALS ?
			SDE_RECOVERY_HARD_RESET : SDE_RECOVERY_CAPTURE;
		sde_connector_event_notify(conn, DRM_EVENT_SDE_HW_RECOVERY,
				sizeof(uint8_t), event);
	} else if (cmd_enc->pp_timeout_report_cnt) {
		SDE_DBG_DUMP("dsi_dbg_bus", "panic");
	}

	/* request a ctl reset before the next kickoff */
	phys_enc->enable_state = SDE_ENC_ERR_NEEDS_HW_RESET;

exit:
	if (phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc, frame_event);

	return -ETIMEDOUT;
}

static bool _sde_encoder_phys_is_ppsplit_slave(
		struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc)
		return false;

	return _sde_encoder_phys_is_ppsplit(phys_enc) &&
			phys_enc->split_role == ENC_ROLE_SLAVE;
}

static bool _sde_encoder_phys_is_disabling_ppsplit_slave(
		struct sde_encoder_phys *phys_enc)
{
	enum sde_rm_topology_name old_top;

	if (!phys_enc || !phys_enc->connector ||
			phys_enc->split_role != ENC_ROLE_SLAVE)
		return false;

	old_top = sde_connector_get_old_topology_name(
			phys_enc->connector->state);

	return old_top == SDE_RM_TOPOLOGY_PPSPLIT;
}

static int _sde_encoder_phys_cmd_poll_write_pointer_started(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_pingpong *hw_pp = phys_enc->hw_pp;
	struct sde_hw_intf *hw_intf = phys_enc->hw_intf;
	struct sde_hw_pp_vsync_info info;
	u32 timeout_us = SDE_ENC_WR_PTR_START_TIMEOUT_US;
	int ret = 0;

	if (!hw_pp || !hw_intf)
		return 0;

	if (phys_enc->has_intf_te) {
		if (!hw_intf->ops.get_vsync_info ||
				!hw_intf->ops.poll_timeout_wr_ptr)
			goto end;
	} else {
		if (!hw_pp->ops.get_vsync_info ||
				!hw_pp->ops.poll_timeout_wr_ptr)
			goto end;
	}

	if (phys_enc->has_intf_te)
		ret = hw_intf->ops.get_vsync_info(hw_intf, &info);
	else
		ret = hw_pp->ops.get_vsync_info(hw_pp, &info);

	if (ret)
		return ret;

	SDE_DEBUG_CMDENC(cmd_enc,
			"pp:%d intf:%d rd_ptr %d wr_ptr %d\n",
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			info.rd_ptr_line_count,
			info.wr_ptr_line_count);
	SDE_EVT32_VERBOSE(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			info.wr_ptr_line_count);

	if (phys_enc->has_intf_te)
		ret = hw_intf->ops.poll_timeout_wr_ptr(hw_intf, timeout_us);
	else
		ret = hw_pp->ops.poll_timeout_wr_ptr(hw_pp, timeout_us);

	if (ret) {
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_pp->idx - PINGPONG_0,
				phys_enc->hw_intf->idx - INTF_0,
				timeout_us,
				ret);
		SDE_DBG_DUMP("all", "dbg_bus", "vbif_dbg_bus", "panic");
	}

end:
	return ret;
}

static bool _sde_encoder_phys_cmd_is_ongoing_pptx(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_pingpong *hw_pp;
	struct sde_hw_pp_vsync_info info;
	struct sde_hw_intf *hw_intf;

	if (!phys_enc)
		return false;

	if (phys_enc->has_intf_te) {
		hw_intf = phys_enc->hw_intf;
		if (!hw_intf || !hw_intf->ops.get_vsync_info)
			return false;

		hw_intf->ops.get_vsync_info(hw_intf, &info);
	} else {
		hw_pp = phys_enc->hw_pp;
		if (!hw_pp || !hw_pp->ops.get_vsync_info)
			return false;

		hw_pp->ops.get_vsync_info(hw_pp, &info);
	}

	SDE_EVT32(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			atomic_read(&phys_enc->pending_kickoff_cnt),
			info.wr_ptr_line_count,
			phys_enc->cached_mode.vdisplay);

	if (info.wr_ptr_line_count > 0 && info.wr_ptr_line_count <
			phys_enc->cached_mode.vdisplay)
		return true;

	return false;
}

static int _sde_encoder_phys_cmd_wait_for_idle(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_encoder_wait_info wait_info;
	bool recovery_events;
	int ret;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_kickoff_cnt;
	wait_info.timeout_ms = KICKOFF_TIMEOUT_MS;
	recovery_events = sde_encoder_recovery_events_enabled(
			phys_enc->parent);

	/* slave encoder doesn't enable for ppsplit */
	if (_sde_encoder_phys_is_ppsplit_slave(phys_enc))
		return 0;

	ret = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_PINGPONG,
			&wait_info);
	if (ret == -ETIMEDOUT) {
		_sde_encoder_phys_cmd_handle_ppdone_timeout(phys_enc,
				recovery_events);
	} else if (!ret) {
		if (cmd_enc->pp_timeout_report_cnt && recovery_events) {
			struct drm_connector *conn = phys_enc->connector;

			sde_connector_event_notify(conn,
					DRM_EVENT_SDE_HW_RECOVERY,
					sizeof(uint8_t),
					SDE_RECOVERY_SUCCESS);
		}
		cmd_enc->pp_timeout_report_cnt = 0;
	}

	return ret;
}

static int _sde_encoder_phys_cmd_wait_for_autorefresh_done(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_encoder_wait_info wait_info;
	int ret = 0;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	/* only master deals with autorefresh */
	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return 0;

	wait_info.wq = &cmd_enc->autorefresh.kickoff_wq;
	wait_info.atomic_cnt = &cmd_enc->autorefresh.kickoff_cnt;
	wait_info.timeout_ms = _sde_encoder_phys_cmd_get_idle_timeout(cmd_enc);

	/* wait for autorefresh kickoff to start */
	ret = sde_encoder_helper_wait_for_irq(phys_enc,
			INTR_IDX_AUTOREFRESH_DONE, &wait_info);

	/* double check that kickoff has started by reading write ptr reg */
	if (!ret)
		ret = _sde_encoder_phys_cmd_poll_write_pointer_started(
			phys_enc);
	else
		sde_encoder_helper_report_irq_timeout(phys_enc,
				INTR_IDX_AUTOREFRESH_DONE);

	return ret;
}

static int sde_encoder_phys_cmd_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	int ret = 0;
	int refcount;

	if (!phys_enc || !phys_enc->hw_pp) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	mutex_lock(phys_enc->vblank_ctl_lock);
	refcount = atomic_read(&phys_enc->vblank_refcount);

	/* Slave encoders don't report vblank */
	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		goto end;

	/* protect against negative */
	if (!enable && refcount == 0) {
		ret = -EINVAL;
		goto end;
	}

	SDE_DEBUG_CMDENC(cmd_enc, "[%pS] enable=%d/%d\n",
			__builtin_return_address(0), enable, refcount);
	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			enable, refcount);

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1) {
		ret = sde_encoder_helper_register_irq(phys_enc, INTR_IDX_RDPTR);
		if (ret)
			atomic_dec_return(&phys_enc->vblank_refcount);
	} else if (!enable &&
			atomic_dec_return(&phys_enc->vblank_refcount) == 0) {
		ret = sde_encoder_helper_unregister_irq(phys_enc,
				INTR_IDX_RDPTR);
		if (ret)
			atomic_inc_return(&phys_enc->vblank_refcount);
	}

end:
	if (ret) {
		SDE_ERROR_CMDENC(cmd_enc,
				"control vblank irq error %d, enable %d, refcount %d\n",
				ret, enable, refcount);
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_pp->idx - PINGPONG_0,
				enable, refcount, SDE_EVTLOG_ERROR);
	}

	mutex_unlock(phys_enc->vblank_ctl_lock);
	return ret;
}

void sde_encoder_phys_cmd_irq_control(struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct sde_encoder_phys_cmd *cmd_enc;

	if (!phys_enc)
		return;

	/**
	 * pingpong split slaves do not register for IRQs
	 * check old and new topologies
	 */
	if (_sde_encoder_phys_is_ppsplit_slave(phys_enc) ||
			_sde_encoder_phys_is_disabling_ppsplit_slave(phys_enc))
		return;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			enable, atomic_read(&phys_enc->vblank_refcount));

	if (enable) {
		sde_encoder_helper_register_irq(phys_enc, INTR_IDX_PINGPONG);
		sde_encoder_helper_register_irq(phys_enc, INTR_IDX_UNDERRUN);
		sde_encoder_phys_cmd_control_vblank_irq(phys_enc, true);

		if (sde_encoder_phys_cmd_is_master(phys_enc)) {
			sde_encoder_helper_register_irq(phys_enc,
					INTR_IDX_CTL_START);
			sde_encoder_helper_register_irq(phys_enc,
					INTR_IDX_AUTOREFRESH_DONE);
		}

	} else {
		if (sde_encoder_phys_cmd_is_master(phys_enc)) {
			sde_encoder_helper_unregister_irq(phys_enc,
					INTR_IDX_CTL_START);
			sde_encoder_helper_unregister_irq(phys_enc,
					INTR_IDX_AUTOREFRESH_DONE);
		}

		sde_encoder_helper_unregister_irq(phys_enc, INTR_IDX_UNDERRUN);
		sde_encoder_phys_cmd_control_vblank_irq(phys_enc, false);
		sde_encoder_helper_unregister_irq(phys_enc, INTR_IDX_PINGPONG);
	}
}

static int _get_tearcheck_threshold(struct sde_encoder_phys *phys_enc,
	u32 *extra_frame_trigger_time)
{
	struct drm_connector *conn = phys_enc->connector;
	u32 qsync_mode;
	struct drm_display_mode *mode;
	u32 threshold_lines = 0;
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);

	*extra_frame_trigger_time = 0;
	if (!conn || !conn->state)
		return 0;

	mode = &phys_enc->cached_mode;
	qsync_mode = sde_connector_get_qsync_mode(conn);

	if (mode && (qsync_mode == SDE_RM_QSYNC_CONTINUOUS_MODE)) {
		u32 qsync_min_fps = 0;
		u32 default_fps = mode->vrefresh;
		u32 yres = mode->vtotal;
		u32 slow_time_ns;
		u32 default_time_ns;
		u32 extra_time_ns;
		u32 total_extra_lines;
		u32 default_line_time_ns;

		if (phys_enc->parent_ops.get_qsync_fps)
			phys_enc->parent_ops.get_qsync_fps(
				phys_enc->parent, &qsync_min_fps);

		if (!qsync_min_fps || !default_fps || !yres) {
			SDE_ERROR_CMDENC(cmd_enc,
				"wrong qsync params %d %d %d\n",
				qsync_min_fps, default_fps, yres);
			goto exit;
		}

		if (qsync_min_fps >= default_fps) {
			SDE_ERROR_CMDENC(cmd_enc,
				"qsync fps:%d must be less than default:%d\n",
				qsync_min_fps, default_fps);
			goto exit;
		}

		/* Calculate the number of extra lines*/
		slow_time_ns = (1 * 1000000000) / qsync_min_fps;
		default_time_ns = (1 * 1000000000) / default_fps;
		extra_time_ns = slow_time_ns - default_time_ns;
		default_line_time_ns = (1 * 1000000000) / (default_fps * yres);

		total_extra_lines = extra_time_ns / default_line_time_ns;
		threshold_lines += total_extra_lines;

		SDE_DEBUG_CMDENC(cmd_enc, "slow:%d default:%d extra:%d(ns)\n",
			slow_time_ns, default_time_ns, extra_time_ns);
		SDE_DEBUG_CMDENC(cmd_enc, "extra_lines:%d threshold:%d\n",
			total_extra_lines, threshold_lines);
		SDE_DEBUG_CMDENC(cmd_enc, "min_fps:%d fps:%d yres:%d\n",
			qsync_min_fps, default_fps, yres);

		SDE_EVT32(qsync_mode, qsync_min_fps, extra_time_ns, default_fps,
			yres, threshold_lines);

		*extra_frame_trigger_time = extra_time_ns;
	}

exit:
	threshold_lines += DEFAULT_TEARCHECK_SYNC_THRESH_START;

	return threshold_lines;
}

static void sde_encoder_phys_cmd_tearcheck_config(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_tear_check tc_cfg = { 0 };
	struct drm_display_mode *mode;
	bool tc_enable = true;
	u32 vsync_hz, extra_frame_trigger_time;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	mode = &phys_enc->cached_mode;

	SDE_DEBUG_CMDENC(cmd_enc, "pp %d, intf %d\n",
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0);

	if (phys_enc->has_intf_te) {
		if (!phys_enc->hw_intf->ops.setup_tearcheck ||
			!phys_enc->hw_intf->ops.enable_tearcheck) {
			SDE_DEBUG_CMDENC(cmd_enc, "tearcheck not supported\n");
			return;
		}
	} else {
		if (!phys_enc->hw_pp->ops.setup_tearcheck ||
			!phys_enc->hw_pp->ops.enable_tearcheck) {
			SDE_DEBUG_CMDENC(cmd_enc, "tearcheck not supported\n");
			return;
		}
	}

	sde_kms = phys_enc->sde_kms;
	if (!sde_kms || !sde_kms->dev || !sde_kms->dev->dev_private) {
		SDE_ERROR("invalid device\n");
		return;
	}
	priv = sde_kms->dev->dev_private;

	/*
	 * TE default: dsi byte clock calculated base on 70 fps;
	 * around 14 ms to complete a kickoff cycle if te disabled;
	 * vclk_line base on 60 fps; write is faster than read;
	 * init == start == rdptr;
	 *
	 * vsync_count is ratio of MDP VSYNC clock frequency to LCD panel
	 * frequency divided by the no. of rows (lines) in the LCDpanel.
	 */
	vsync_hz = sde_power_clk_get_rate(&priv->phandle, "vsync_clk");
	if (!vsync_hz || !mode->vtotal || !mode->vrefresh) {
		SDE_DEBUG_CMDENC(cmd_enc,
			"invalid params - vsync_hz %u vtot %u vrefresh %u\n",
			vsync_hz, mode->vtotal, mode->vrefresh);
		return;
	}

	tc_cfg.vsync_count = vsync_hz / (mode->vtotal * mode->vrefresh);

	/* enable external TE after kickoff to avoid premature autorefresh */
	tc_cfg.hw_vsync_mode = 0;

	/*
	 * By setting sync_cfg_height to near max register value, we essentially
	 * disable sde hw generated TE signal, since hw TE will arrive first.
	 * Only caveat is if due to error, we hit wrap-around.
	 */
	tc_cfg.sync_cfg_height = 0xFFF0;
	tc_cfg.vsync_init_val = mode->vdisplay;
	tc_cfg.sync_threshold_start = _get_tearcheck_threshold(phys_enc,
			&extra_frame_trigger_time);
	tc_cfg.sync_threshold_continue = DEFAULT_TEARCHECK_SYNC_THRESH_CONTINUE;
	tc_cfg.start_pos = mode->vdisplay;
	tc_cfg.rd_ptr_irq = mode->vdisplay + 1;

	cmd_enc->ctl_start_threshold = (extra_frame_trigger_time / 1000) +
			SDE_ENC_CTL_START_THRESHOLD_US;

	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d intf %d vsync_clk_speed_hz %u vtotal %u vrefresh %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		vsync_hz, mode->vtotal, mode->vrefresh);
	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d intf %d enable %u start_pos %u rd_ptr_irq %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		tc_enable, tc_cfg.start_pos, tc_cfg.rd_ptr_irq);
	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d intf %d hw_vsync_mode %u vsync_count %u vsync_init_val %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		tc_cfg.hw_vsync_mode, tc_cfg.vsync_count,
		tc_cfg.vsync_init_val);
	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d intf %d cfgheight %u thresh_start %u thresh_cont %u ctl_start_threshold:%d\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		tc_cfg.sync_cfg_height,
		tc_cfg.sync_threshold_start, tc_cfg.sync_threshold_continue,
		cmd_enc->ctl_start_threshold);

	if (phys_enc->has_intf_te) {
		phys_enc->hw_intf->ops.setup_tearcheck(phys_enc->hw_intf,
				&tc_cfg);
		phys_enc->hw_intf->ops.enable_tearcheck(phys_enc->hw_intf,
				tc_enable);
	} else {
		phys_enc->hw_pp->ops.setup_tearcheck(phys_enc->hw_pp, &tc_cfg);
		phys_enc->hw_pp->ops.enable_tearcheck(phys_enc->hw_pp,
				tc_enable);
	}
}

static void _sde_encoder_phys_cmd_pingpong_config(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc || !phys_enc->hw_ctl || !phys_enc->hw_pp) {
		SDE_ERROR("invalid arg(s), enc %d\n", phys_enc != 0);
		return;
	}

	SDE_DEBUG_CMDENC(cmd_enc, "pp %d, enabling mode:\n",
			phys_enc->hw_pp->idx - PINGPONG_0);
	drm_mode_debug_printmodeline(&phys_enc->cached_mode);

	if (!_sde_encoder_phys_is_ppsplit_slave(phys_enc))
		_sde_encoder_phys_cmd_update_intf_cfg(phys_enc);
	sde_encoder_phys_cmd_tearcheck_config(phys_enc);
}

static void sde_encoder_phys_cmd_enable_helper(
		struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc || !phys_enc->hw_ctl || !phys_enc->hw_pp) {
		SDE_ERROR("invalid arg(s), encoder %d\n", phys_enc != 0);
		return;
	}

	sde_encoder_helper_split_config(phys_enc, phys_enc->intf_idx);

	_sde_encoder_phys_cmd_pingpong_config(phys_enc);

	/*
	 * For pp-split, skip setting the flush bit for the slave intf, since
	 * both intfs use same ctl and HW will only flush the master.
	 */
	if (_sde_encoder_phys_is_ppsplit(phys_enc) &&
		!sde_encoder_phys_cmd_is_master(phys_enc))
		goto skip_flush;

	_sde_encoder_phys_cmd_update_flush_mask(phys_enc);

skip_flush:
	return;
}

static void sde_encoder_phys_cmd_enable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc || !phys_enc->hw_pp) {
		SDE_ERROR("invalid phys encoder\n");
		return;
	}

	SDE_DEBUG_CMDENC(cmd_enc, "pp %d\n", phys_enc->hw_pp->idx - PINGPONG_0);

	if (phys_enc->enable_state == SDE_ENC_ENABLED) {
		if (!phys_enc->cont_splash_enabled)
			SDE_ERROR("already enabled\n");
		return;
	}

	sde_encoder_phys_cmd_enable_helper(phys_enc);
	phys_enc->enable_state = SDE_ENC_ENABLED;
}

static bool sde_encoder_phys_cmd_is_autorefresh_enabled(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_pingpong *hw_pp;
	struct sde_hw_intf *hw_intf;
	struct sde_hw_autorefresh cfg;
	int ret;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf)
		return 0;

	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return 0;

	if (phys_enc->has_intf_te) {
		hw_intf = phys_enc->hw_intf;
		if (!hw_intf->ops.get_autorefresh)
			return 0;

		ret = hw_intf->ops.get_autorefresh(hw_intf, &cfg);
	} else {
		hw_pp = phys_enc->hw_pp;
		if (!hw_pp->ops.get_autorefresh)
			return 0;

		ret = hw_pp->ops.get_autorefresh(hw_pp, &cfg);
	}

	if (ret)
		return 0;

	return cfg.enable;
}

static void sde_encoder_phys_cmd_connect_te(
		struct sde_encoder_phys *phys_enc, bool enable)
{
	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf)
		return;

	if (phys_enc->has_intf_te &&
			phys_enc->hw_intf->ops.connect_external_te)
		phys_enc->hw_intf->ops.connect_external_te(phys_enc->hw_intf,
				enable);
	else if (phys_enc->hw_pp->ops.connect_external_te)
		phys_enc->hw_pp->ops.connect_external_te(phys_enc->hw_pp,
				enable);
	else
		return;

	SDE_EVT32(DRMID(phys_enc->parent), enable);
}

static int sde_encoder_phys_cmd_te_get_line_count(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_pingpong *hw_pp;
	struct sde_hw_intf *hw_intf;
	u32 line_count;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf)
		return -EINVAL;

	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return -EINVAL;

	if (phys_enc->has_intf_te) {
		hw_intf = phys_enc->hw_intf;
		if (!hw_intf->ops.get_line_count)
			return -EINVAL;

		line_count = hw_intf->ops.get_line_count(hw_intf);
	} else {
		hw_pp = phys_enc->hw_pp;
		if (!hw_pp->ops.get_line_count)
			return -EINVAL;

		line_count = hw_pp->ops.get_line_count(hw_pp);
	}

	return line_count;
}

static int sde_encoder_phys_cmd_get_write_line_count(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_pingpong *hw_pp;
	struct sde_hw_intf *hw_intf;
	struct sde_hw_pp_vsync_info info;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf)
		return -EINVAL;

	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return -EINVAL;

	if (phys_enc->has_intf_te) {
		hw_intf = phys_enc->hw_intf;
		if (!hw_intf->ops.get_vsync_info)
			return -EINVAL;

		if (hw_intf->ops.get_vsync_info(hw_intf, &info))
			return -EINVAL;
	} else {
		hw_pp = phys_enc->hw_pp;
		if (!hw_pp->ops.get_vsync_info)
			return -EINVAL;

		if (hw_pp->ops.get_vsync_info(hw_pp, &info))
			return -EINVAL;
	}

	return (int)info.wr_ptr_line_count;
}

static void sde_encoder_phys_cmd_disable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	atomic_set(&phys_enc->ctlstart_timeout, 0);
	SDE_DEBUG_CMDENC(cmd_enc, "pp %d intf %d state %d\n",
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			phys_enc->enable_state);
	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			phys_enc->enable_state);

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR_CMDENC(cmd_enc, "already disabled\n");
		return;
	}

	if (phys_enc->has_intf_te && phys_enc->hw_intf->ops.enable_tearcheck)
		phys_enc->hw_intf->ops.enable_tearcheck(
				phys_enc->hw_intf,
				false);
	else if (phys_enc->hw_pp->ops.enable_tearcheck)
		phys_enc->hw_pp->ops.enable_tearcheck(phys_enc->hw_pp,
				false);

	phys_enc->enable_state = SDE_ENC_DISABLED;
}

static void sde_encoder_phys_cmd_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	kfree(cmd_enc);
}

static void sde_encoder_phys_cmd_get_hw_resources(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	if ((phys_enc->intf_idx - INTF_0) >= INTF_MAX) {
		SDE_ERROR("invalid intf idx:%d\n", phys_enc->intf_idx);
		return;
	}

	SDE_DEBUG_CMDENC(cmd_enc, "\n");
	hw_res->intfs[phys_enc->intf_idx - INTF_0] = INTF_MODE_CMD;
}

static int sde_encoder_phys_cmd_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct sde_hw_tear_check tc_cfg = {0};
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	int ret;
	u32 extra_frame_trigger_time;

	if (!phys_enc || !phys_enc->hw_pp) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	SDE_DEBUG_CMDENC(cmd_enc, "pp %d\n", phys_enc->hw_pp->idx - PINGPONG_0);

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			atomic_read(&phys_enc->pending_kickoff_cnt),
			atomic_read(&cmd_enc->autorefresh.kickoff_cnt));

	/*
	 * Mark kickoff request as outstanding. If there are more than one,
	 * outstanding, then we have to wait for the previous one to complete
	 */
	ret = _sde_encoder_phys_cmd_wait_for_idle(phys_enc);
	if (ret) {
		/* force pending_kickoff_cnt 0 to discard failed kickoff */
		atomic_set(&phys_enc->pending_kickoff_cnt, 0);
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->hw_pp->idx - PINGPONG_0);
		SDE_ERROR("failed wait_for_idle: %d\n", ret);
	}

	if (sde_connector_is_qsync_updated(phys_enc->connector)) {
		tc_cfg.sync_threshold_start =
			_get_tearcheck_threshold(phys_enc,
				&extra_frame_trigger_time);
		if (phys_enc->has_intf_te &&
				phys_enc->hw_intf->ops.update_tearcheck)
			phys_enc->hw_intf->ops.update_tearcheck(
					phys_enc->hw_intf, &tc_cfg);
		else if (phys_enc->hw_pp->ops.update_tearcheck)
			phys_enc->hw_pp->ops.update_tearcheck(
					phys_enc->hw_pp, &tc_cfg);

		cmd_enc->ctl_start_threshold =
			(extra_frame_trigger_time / 1000) +
				SDE_ENC_CTL_START_THRESHOLD_US;
		SDE_EVT32(DRMID(phys_enc->parent),
		    tc_cfg.sync_threshold_start, cmd_enc->ctl_start_threshold);
	}

	SDE_DEBUG_CMDENC(cmd_enc, "pp:%d pending_cnt %d\n",
			phys_enc->hw_pp->idx - PINGPONG_0,
			atomic_read(&phys_enc->pending_kickoff_cnt));
	return ret;
}

static int _sde_encoder_phys_cmd_wait_for_ctl_start(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_encoder_wait_info wait_info;
	int ret;
	bool frame_pending = true;

	if (!phys_enc || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_ctlstart_cnt;
	wait_info.timeout_ms = KICKOFF_TIMEOUT_MS;

	/* slave encoder doesn't enable for ppsplit */
	if (_sde_encoder_phys_is_ppsplit_slave(phys_enc))
		return 0;

	ret = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_CTL_START,
			&wait_info);
	if (ret == -ETIMEDOUT) {
		struct sde_hw_ctl *ctl = phys_enc->hw_ctl;

		if (ctl && ctl->ops.get_start_state)
			frame_pending = ctl->ops.get_start_state(ctl);

		if (frame_pending)
			SDE_ERROR_CMDENC(cmd_enc,
					"ctl start interrupt wait failed\n");
		else
			ret = 0;

		if (sde_encoder_phys_cmd_is_master(phys_enc)) {
			/*
			 * Signaling the retire fence at ctl start timeout
			 * to allow the next commit and avoid device freeze.
			 * As ctl start timeout can occurs due to no read ptr,
			 * updating pending_rd_ptr_cnt here may not cover all
			 * cases. Hence signaling the retire fence.
			 */
			if (atomic_add_unless(
			 &phys_enc->pending_retire_fence_cnt, -1, 0))
				phys_enc->parent_ops.handle_frame_done(
				 phys_enc->parent,
				 phys_enc,
				 SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE);
			atomic_add_unless(
				&phys_enc->pending_ctlstart_cnt, -1, 0);
			atomic_inc_return(&phys_enc->ctlstart_timeout);
		}

	}

	return ret;
}

static int sde_encoder_phys_cmd_wait_for_tx_complete(
		struct sde_encoder_phys *phys_enc)
{
	int rc;
	struct sde_encoder_phys_cmd *cmd_enc;

	if (!phys_enc)
		return -EINVAL;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	rc = _sde_encoder_phys_cmd_wait_for_idle(phys_enc);
	if (rc) {
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->intf_idx - INTF_0);
		SDE_ERROR("failed wait_for_idle: %d\n", rc);
	}

	return rc;
}

static int sde_encoder_phys_cmd_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc)
{
	int rc = 0;
	struct sde_encoder_phys_cmd *cmd_enc;

	if (!phys_enc)
		return -EINVAL;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	/* only required for master controller */
	if (sde_encoder_phys_cmd_is_master(phys_enc))
		rc = _sde_encoder_phys_cmd_wait_for_ctl_start(phys_enc);

	if (!rc && sde_encoder_phys_cmd_is_master(phys_enc) &&
			cmd_enc->autorefresh.cfg.enable)
		rc = _sde_encoder_phys_cmd_wait_for_autorefresh_done(phys_enc);

	/* required for both controllers */
	if (!rc && cmd_enc->serialize_wait4pp)
		sde_encoder_phys_cmd_prepare_for_kickoff(phys_enc, NULL);

	return rc;
}

static int sde_encoder_phys_cmd_wait_for_vblank(
		struct sde_encoder_phys *phys_enc)
{
	int rc = 0;
	struct sde_encoder_phys_cmd *cmd_enc;
	struct sde_encoder_wait_info wait_info;

	if (!phys_enc)
		return -EINVAL;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	/* only required for master controller */
	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return rc;

	wait_info.wq = &cmd_enc->pending_vblank_wq;
	wait_info.atomic_cnt = &cmd_enc->pending_vblank_cnt;
	wait_info.timeout_ms = _sde_encoder_phys_cmd_get_idle_timeout(cmd_enc);

	atomic_inc(&cmd_enc->pending_vblank_cnt);

	rc = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_RDPTR,
			&wait_info);

	return rc;
}

static void sde_encoder_phys_cmd_update_split_role(
		struct sde_encoder_phys *phys_enc,
		enum sde_enc_split_role role)
{
	struct sde_encoder_phys_cmd *cmd_enc;
	enum sde_enc_split_role old_role;
	bool is_ppsplit;

	if (!phys_enc)
		return;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);
	old_role = phys_enc->split_role;
	is_ppsplit = _sde_encoder_phys_is_ppsplit(phys_enc);

	phys_enc->split_role = role;

	SDE_DEBUG_CMDENC(cmd_enc, "old role %d new role %d\n",
			old_role, role);

	/*
	 * ppsplit solo needs to reprogram because intf may have swapped without
	 * role changing on left-only, right-only back-to-back commits
	 */
	if (!(is_ppsplit && role == ENC_ROLE_SOLO) &&
			(role == old_role || role == ENC_ROLE_SKIP))
		return;

	sde_encoder_helper_split_config(phys_enc, phys_enc->intf_idx);
	_sde_encoder_phys_cmd_pingpong_config(phys_enc);
	_sde_encoder_phys_cmd_update_flush_mask(phys_enc);
}

static void sde_encoder_phys_cmd_prepare_commit(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	int trial = 0;

	if (!phys_enc)
		return;

	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return;

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->intf_idx - INTF_0,
			cmd_enc->autorefresh.cfg.enable);

	if (!sde_encoder_phys_cmd_is_autorefresh_enabled(phys_enc))
		return;

	/*
	 * If autorefresh is enabled, disable it and make sure it is safe to
	 * proceed with current frame commit/push. Sequence fallowed is,
	 * 1. Disable TE
	 * 2. Disable autorefresh config
	 * 4. Poll for frame transfer ongoing to be false
	 * 5. Enable TE back
	 */
	sde_encoder_phys_cmd_connect_te(phys_enc, false);

	_sde_encoder_phys_cmd_config_autorefresh(phys_enc, 0);

	do {
		udelay(SDE_ENC_MAX_POLL_TIMEOUT_US);
		if ((trial * SDE_ENC_MAX_POLL_TIMEOUT_US)
				> (KICKOFF_TIMEOUT_MS * USEC_PER_MSEC)) {
			SDE_ERROR_CMDENC(cmd_enc,
					"disable autorefresh failed\n");
			break;
		}

		trial++;
	} while (_sde_encoder_phys_cmd_is_ongoing_pptx(phys_enc));

	sde_encoder_phys_cmd_connect_te(phys_enc, true);

	SDE_DEBUG_CMDENC(cmd_enc, "disabled autorefresh\n");
}

static void sde_encoder_phys_cmd_trigger_start(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	u32 frame_cnt;

	if (!phys_enc)
		return;

	/* we don't issue CTL_START when using autorefresh */
	frame_cnt = _sde_encoder_phys_cmd_get_autorefresh_property(phys_enc);
	if (frame_cnt) {
		_sde_encoder_phys_cmd_config_autorefresh(phys_enc, frame_cnt);
		atomic_inc(&cmd_enc->autorefresh.kickoff_cnt);
	} else {
		sde_encoder_helper_trigger_start(phys_enc);
	}
}

static void sde_encoder_phys_cmd_setup_vsync_source(
		struct sde_encoder_phys *phys_enc,
		u32 vsync_source, bool is_dummy)
{
	if (!phys_enc || !phys_enc->hw_intf)
		return;

	sde_encoder_helper_vsync_config(phys_enc, vsync_source, is_dummy);

	if (phys_enc->has_intf_te && phys_enc->hw_intf->ops.vsync_sel)
		phys_enc->hw_intf->ops.vsync_sel(phys_enc->hw_intf,
				vsync_source);
}

static void sde_encoder_phys_cmd_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->prepare_commit = sde_encoder_phys_cmd_prepare_commit;
	ops->is_master = sde_encoder_phys_cmd_is_master;
	ops->mode_set = sde_encoder_phys_cmd_mode_set;
	ops->cont_splash_mode_set = sde_encoder_phys_cmd_cont_splash_mode_set;
	ops->mode_fixup = sde_encoder_phys_cmd_mode_fixup;
	ops->enable = sde_encoder_phys_cmd_enable;
	ops->disable = sde_encoder_phys_cmd_disable;
	ops->destroy = sde_encoder_phys_cmd_destroy;
	ops->get_hw_resources = sde_encoder_phys_cmd_get_hw_resources;
	ops->control_vblank_irq = sde_encoder_phys_cmd_control_vblank_irq;
	ops->wait_for_commit_done = sde_encoder_phys_cmd_wait_for_commit_done;
	ops->prepare_for_kickoff = sde_encoder_phys_cmd_prepare_for_kickoff;
	ops->wait_for_tx_complete = sde_encoder_phys_cmd_wait_for_tx_complete;
	ops->wait_for_vblank = sde_encoder_phys_cmd_wait_for_vblank;
	ops->trigger_flush = sde_encoder_helper_trigger_flush;
	ops->trigger_start = sde_encoder_phys_cmd_trigger_start;
	ops->needs_single_flush = sde_encoder_phys_needs_single_flush;
	ops->hw_reset = sde_encoder_helper_hw_reset;
	ops->irq_control = sde_encoder_phys_cmd_irq_control;
	ops->update_split_role = sde_encoder_phys_cmd_update_split_role;
	ops->restore = sde_encoder_phys_cmd_enable_helper;
	ops->control_te = sde_encoder_phys_cmd_connect_te;
	ops->is_autorefresh_enabled =
			sde_encoder_phys_cmd_is_autorefresh_enabled;
	ops->get_line_count = sde_encoder_phys_cmd_te_get_line_count;
	ops->get_wr_line_count = sde_encoder_phys_cmd_get_write_line_count;
	ops->wait_for_active = NULL;
	ops->setup_vsync_source = sde_encoder_phys_cmd_setup_vsync_source;
}

struct sde_encoder_phys *sde_encoder_phys_cmd_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc = NULL;
	struct sde_encoder_phys_cmd *cmd_enc = NULL;
	struct sde_hw_mdp *hw_mdp;
	struct sde_encoder_irq *irq;
	int i, ret = 0;

	SDE_DEBUG("intf %d\n", p->intf_idx - INTF_0);

	cmd_enc = kzalloc(sizeof(*cmd_enc), GFP_KERNEL);
	if (!cmd_enc) {
		ret = -ENOMEM;
		SDE_ERROR("failed to allocate\n");
		goto fail;
	}
	phys_enc = &cmd_enc->base;

	hw_mdp = sde_rm_get_mdp(&p->sde_kms->rm);
	if (IS_ERR_OR_NULL(hw_mdp)) {
		ret = PTR_ERR(hw_mdp);
		SDE_ERROR("failed to get mdptop\n");
		goto fail_mdp_init;
	}
	phys_enc->hw_mdptop = hw_mdp;
	phys_enc->intf_idx = p->intf_idx;

	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_CMD;
	phys_enc->enc_spinlock = p->enc_spinlock;
	phys_enc->vblank_ctl_lock = p->vblank_ctl_lock;
	cmd_enc->stream_sel = 0;
	cmd_enc->ctl_start_threshold = SDE_ENC_CTL_START_THRESHOLD_US;
	phys_enc->enable_state = SDE_ENC_DISABLED;
	sde_encoder_phys_cmd_init_ops(&phys_enc->ops);
	phys_enc->comp_type = p->comp_type;

	if (sde_hw_intf_te_supported(phys_enc->sde_kms->catalog))
		phys_enc->has_intf_te = true;
	else
		phys_enc->has_intf_te = false;

	for (i = 0; i < INTR_IDX_MAX; i++) {
		irq = &phys_enc->irq[i];
		INIT_LIST_HEAD(&irq->cb.list);
		irq->irq_idx = -EINVAL;
		irq->hw_idx = -EINVAL;
		irq->cb.arg = phys_enc;
	}

	irq = &phys_enc->irq[INTR_IDX_CTL_START];
	irq->name = "ctl_start";
	irq->intr_type = SDE_IRQ_TYPE_CTL_START;
	irq->intr_idx = INTR_IDX_CTL_START;
	irq->cb.func = sde_encoder_phys_cmd_ctl_start_irq;

	irq = &phys_enc->irq[INTR_IDX_PINGPONG];
	irq->name = "pp_done";
	irq->intr_type = SDE_IRQ_TYPE_PING_PONG_COMP;
	irq->intr_idx = INTR_IDX_PINGPONG;
	irq->cb.func = sde_encoder_phys_cmd_pp_tx_done_irq;

	irq = &phys_enc->irq[INTR_IDX_RDPTR];
	irq->intr_idx = INTR_IDX_RDPTR;
	irq->name = "te_rd_ptr";

	if (phys_enc->has_intf_te)
		irq->intr_type = SDE_IRQ_TYPE_INTF_TEAR_RD_PTR;
	else
		irq->intr_type = SDE_IRQ_TYPE_PING_PONG_RD_PTR;

	irq->cb.func = sde_encoder_phys_cmd_te_rd_ptr_irq;

	irq = &phys_enc->irq[INTR_IDX_UNDERRUN];
	irq->name = "underrun";
	irq->intr_type = SDE_IRQ_TYPE_INTF_UNDER_RUN;
	irq->intr_idx = INTR_IDX_UNDERRUN;
	irq->cb.func = sde_encoder_phys_cmd_underrun_irq;

	irq = &phys_enc->irq[INTR_IDX_AUTOREFRESH_DONE];
	irq->name = "autorefresh_done";

	if (phys_enc->has_intf_te)
		irq->intr_type = SDE_IRQ_TYPE_INTF_TEAR_AUTO_REF;
	else
		irq->intr_type = SDE_IRQ_TYPE_PING_PONG_AUTO_REF;

	irq->intr_idx = INTR_IDX_AUTOREFRESH_DONE;
	irq->cb.func = sde_encoder_phys_cmd_autorefresh_done_irq;

	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
	atomic_set(&phys_enc->pending_ctlstart_cnt, 0);
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);
	atomic_set(&cmd_enc->pending_rd_ptr_cnt, 0);
	atomic_set(&cmd_enc->pending_vblank_cnt, 0);
	atomic_set(&phys_enc->ctlstart_timeout, 0);
	init_waitqueue_head(&phys_enc->pending_kickoff_wq);
	init_waitqueue_head(&cmd_enc->pending_vblank_wq);
	atomic_set(&cmd_enc->autorefresh.kickoff_cnt, 0);
	init_waitqueue_head(&cmd_enc->autorefresh.kickoff_wq);

	SDE_DEBUG_CMDENC(cmd_enc, "created\n");

	return phys_enc;

fail_mdp_init:
	kfree(cmd_enc);
fail:
	return ERR_PTR(ret);
}
