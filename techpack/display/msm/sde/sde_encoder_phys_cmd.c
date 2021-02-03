// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
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
#define AUTOREFRESH_SEQ1_POLL_TIME	2000
#define AUTOREFRESH_SEQ2_POLL_TIME	25000
#define AUTOREFRESH_SEQ2_POLL_TIMEOUT	1000000

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
	u32 event = 0;

	if (!phys_enc || !phys_enc->hw_pp)
		return;

	SDE_ATRACE_BEGIN("pp_done_irq");

	/* notify all synchronous clients first, then asynchronous clients */
	if (phys_enc->parent_ops.handle_frame_done &&
	    atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0)) {
		event = SDE_ENCODER_FRAME_EVENT_DONE |
			SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE;
		spin_lock(phys_enc->enc_spinlock);
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
				phys_enc, event);
		spin_unlock(phys_enc->enc_spinlock);
	}

	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0, event);

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
	u32 scheduler_status = INVALID_CTL_STATUS;
	struct sde_hw_ctl *ctl;
	struct sde_hw_pp_vsync_info info[MAX_CHANNELS_PER_ENC] = {{0}};
	struct sde_encoder_phys_cmd_te_timestamp *te_timestamp;
	unsigned long lock_flags;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_intf)
		return;

	SDE_ATRACE_BEGIN("rd_ptr_irq");
	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);
	ctl = phys_enc->hw_ctl;

	if (ctl && ctl->ops.get_scheduler_status)
		scheduler_status = ctl->ops.get_scheduler_status(ctl);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	te_timestamp = list_first_entry_or_null(&cmd_enc->te_timestamp_list,
				struct sde_encoder_phys_cmd_te_timestamp, list);
	if (te_timestamp) {
		list_del_init(&te_timestamp->list);
		te_timestamp->timestamp = ktime_get();
		list_add_tail(&te_timestamp->list, &cmd_enc->te_timestamp_list);
	}
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	sde_encoder_helper_get_pp_line_count(phys_enc->parent, info);
	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
		info[0].pp_idx, info[0].intf_idx,
		info[0].wr_ptr_line_count, info[0].intf_frame_count,
		info[1].pp_idx, info[1].intf_idx,
		info[1].wr_ptr_line_count, info[1].intf_frame_count,
		scheduler_status);

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
			phys_enc);

	atomic_add_unless(&cmd_enc->pending_vblank_cnt, -1, 0);
	wake_up_all(&cmd_enc->pending_vblank_wq);
	SDE_ATRACE_END("rd_ptr_irq");
}

static void sde_encoder_phys_cmd_wr_ptr_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys *phys_enc = arg;
	struct sde_hw_ctl *ctl;
	u32 event = 0;
	struct sde_hw_pp_vsync_info info[MAX_CHANNELS_PER_ENC] = {{0}};

	if (!phys_enc || !phys_enc->hw_ctl)
		return;

	SDE_ATRACE_BEGIN("wr_ptr_irq");
	ctl = phys_enc->hw_ctl;

	if (atomic_add_unless(&phys_enc->pending_retire_fence_cnt, -1, 0)) {
		event = SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE;
		if (phys_enc->parent_ops.handle_frame_done) {
			spin_lock(phys_enc->enc_spinlock);
			phys_enc->parent_ops.handle_frame_done(
					phys_enc->parent, phys_enc, event);
			spin_unlock(phys_enc->enc_spinlock);
		}
	}

	sde_encoder_helper_get_pp_line_count(phys_enc->parent, info);
	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
		ctl->idx - CTL_0, event,
		info[0].pp_idx, info[0].intf_idx, info[0].wr_ptr_line_count,
		info[1].pp_idx, info[1].intf_idx, info[1].wr_ptr_line_count);

	/* Signal any waiting wr_ptr start interrupt */
	wake_up_all(&phys_enc->pending_kickoff_wq);
	SDE_ATRACE_END("wr_ptr_irq");
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
	struct sde_kms *sde_kms;
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

	sde_kms = phys_enc->sde_kms;
	mutex_lock(&sde_kms->vblank_ctl_global_lock);

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

	irq = &phys_enc->irq[INTR_IDX_WRPTR];
	irq->irq_idx = -EINVAL;
	if (phys_enc->has_intf_te)
		irq->hw_idx = phys_enc->hw_intf->idx;
	else
		irq->hw_idx = phys_enc->hw_pp->idx;

	mutex_unlock(&sde_kms->vblank_ctl_global_lock);

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
	int event;
	u32 pending_kickoff_cnt;
	unsigned long lock_flags;

	if (!phys_enc || !phys_enc->hw_pp || !phys_enc->hw_ctl)
		return -EINVAL;

	conn = phys_enc->connector;

	/* decrement the kickoff_cnt before checking for ESD status */
	if (!atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0))
		return 0;

	cmd_enc->pp_timeout_report_cnt++;
	pending_kickoff_cnt = atomic_read(&phys_enc->pending_kickoff_cnt) + 1;

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			cmd_enc->pp_timeout_report_cnt,
			pending_kickoff_cnt,
			frame_event);

	/* check if panel is still sending TE signal or not */
	if (sde_connector_esd_status(phys_enc->connector))
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
			SDE_DBG_DUMP("secure", "all", "dbg_bus");
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
	if (phys_enc->parent_ops.handle_frame_done) {
		spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
		phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc, frame_event);
		spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);
	}

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

static bool _sde_encoder_phys_cmd_is_scheduler_idle(
		struct sde_encoder_phys *phys_enc)
{
	bool wr_ptr_wait_success = true;
	unsigned long lock_flags;
	bool ret = false;
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_ctl *ctl = phys_enc->hw_ctl;

	if (sde_encoder_phys_cmd_is_master(phys_enc))
		wr_ptr_wait_success = cmd_enc->wr_ptr_wait_success;

	/*
	 * Handle cases where a pp-done interrupt is missed
	 * due to irq latency with POSTED start
	 */
	if (wr_ptr_wait_success &&
	    (phys_enc->frame_trigger_mode == FRAME_DONE_WAIT_POSTED_START) &&
	    ctl->ops.get_scheduler_status &&
	    (ctl->ops.get_scheduler_status(ctl) & BIT(0)) &&
	    atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0) &&
	    phys_enc->parent_ops.handle_frame_done) {

		spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
		phys_enc->parent_ops.handle_frame_done(
			phys_enc->parent, phys_enc,
			SDE_ENCODER_FRAME_EVENT_DONE |
			SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE);
		spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

		SDE_EVT32(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->hw_intf->idx - INTF_0,
			atomic_read(&phys_enc->pending_kickoff_cnt));

		ret = true;
	}

	return ret;
}

static int _sde_encoder_phys_cmd_wait_for_idle(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_encoder_wait_info wait_info = {0};
	bool recovery_events;
	int ret;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	if (atomic_read(&phys_enc->pending_kickoff_cnt) > 1)
		wait_info.count_check = 1;

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_kickoff_cnt;
	wait_info.timeout_ms = KICKOFF_TIMEOUT_MS;
	recovery_events = sde_encoder_recovery_events_enabled(
			phys_enc->parent);

	/* slave encoder doesn't enable for ppsplit */
	if (_sde_encoder_phys_is_ppsplit_slave(phys_enc))
		return 0;

	if (_sde_encoder_phys_cmd_is_scheduler_idle(phys_enc))
		return 0;

	ret = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_PINGPONG,
			&wait_info);
	if (ret == -ETIMEDOUT) {
		if (_sde_encoder_phys_cmd_is_scheduler_idle(phys_enc))
			return 0;

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
	struct sde_encoder_wait_info wait_info = {0};
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
	struct sde_kms *sde_kms;

	if (!phys_enc || !phys_enc->hw_pp) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	sde_kms = phys_enc->sde_kms;

	mutex_lock(&sde_kms->vblank_ctl_global_lock);
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

	mutex_unlock(&sde_kms->vblank_ctl_global_lock);
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
		sde_encoder_phys_cmd_control_vblank_irq(phys_enc, true);

		if (sde_encoder_phys_cmd_is_master(phys_enc)) {
			sde_encoder_helper_register_irq(phys_enc,
					INTR_IDX_WRPTR);
			sde_encoder_helper_register_irq(phys_enc,
					INTR_IDX_AUTOREFRESH_DONE);
		}

	} else {
		if (sde_encoder_phys_cmd_is_master(phys_enc)) {
			sde_encoder_helper_unregister_irq(phys_enc,
					INTR_IDX_WRPTR);
			sde_encoder_helper_unregister_irq(phys_enc,
					INTR_IDX_AUTOREFRESH_DONE);
		}

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
	tc_cfg.wr_ptr_irq = 1;

	SDE_DEBUG_CMDENC(cmd_enc,
	  "tc %d intf %d vsync_clk_speed_hz %u vtotal %u vrefresh %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		vsync_hz, mode->vtotal, mode->vrefresh);
	SDE_DEBUG_CMDENC(cmd_enc,
	  "tc %d intf %d enable %u start_pos %u rd_ptr_irq %u wr_ptr_irq %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		tc_enable, tc_cfg.start_pos, tc_cfg.rd_ptr_irq,
		tc_cfg.wr_ptr_irq);
	SDE_DEBUG_CMDENC(cmd_enc,
	  "tc %d intf %d hw_vsync_mode %u vsync_count %u vsync_init_val %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		tc_cfg.hw_vsync_mode, tc_cfg.vsync_count,
		tc_cfg.vsync_init_val);
	SDE_DEBUG_CMDENC(cmd_enc,
	  "tc %d intf %d cfgheight %u thresh_start %u thresh_cont %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0,
		phys_enc->hw_intf->idx - INTF_0,
		tc_cfg.sync_cfg_height,
		tc_cfg.sync_threshold_start, tc_cfg.sync_threshold_continue);

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
		SDE_ERROR("invalid arg(s), enc %d\n", !phys_enc);
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
		SDE_ERROR("invalid arg(s), encoder %d\n", !phys_enc);
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
		return false;

	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return false;

	if (phys_enc->has_intf_te) {
		hw_intf = phys_enc->hw_intf;
		if (!hw_intf->ops.get_autorefresh)
			return false;

		ret = hw_intf->ops.get_autorefresh(hw_intf, &cfg);
	} else {
		hw_pp = phys_enc->hw_pp;
		if (!hw_pp->ops.get_autorefresh)
			return false;

		ret = hw_pp->ops.get_autorefresh(hw_pp, &cfg);
	}

	if (ret)
		return false;

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
	int ret = 0;
	u32 extra_frame_trigger_time;

	if (!phys_enc || !phys_enc->hw_pp) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}
	SDE_DEBUG_CMDENC(cmd_enc, "pp %d\n", phys_enc->hw_pp->idx - PINGPONG_0);

	phys_enc->frame_trigger_mode = params->frame_trigger_mode;
	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			atomic_read(&phys_enc->pending_kickoff_cnt),
			atomic_read(&cmd_enc->autorefresh.kickoff_cnt),
			phys_enc->frame_trigger_mode);

	if (phys_enc->frame_trigger_mode == FRAME_DONE_WAIT_DEFAULT) {
		/*
		 * Mark kickoff request as outstanding. If there are more
		 * than one outstanding frame, then we have to wait for the
		 * previous frame to complete
		 */
		ret = _sde_encoder_phys_cmd_wait_for_idle(phys_enc);
		if (ret) {
			atomic_set(&phys_enc->pending_kickoff_cnt, 0);
			SDE_EVT32(DRMID(phys_enc->parent),
					phys_enc->hw_pp->idx - PINGPONG_0);
			SDE_ERROR("failed wait_for_idle: %d\n", ret);
		}
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
		SDE_EVT32(DRMID(phys_enc->parent), tc_cfg.sync_threshold_start);
	}

	SDE_DEBUG_CMDENC(cmd_enc, "pp:%d pending_cnt %d\n",
			phys_enc->hw_pp->idx - PINGPONG_0,
			atomic_read(&phys_enc->pending_kickoff_cnt));
	return ret;
}

static bool _sde_encoder_phys_cmd_needs_vsync_change(
		struct sde_encoder_phys *phys_enc, ktime_t profile_timestamp)
{
	struct sde_encoder_phys_cmd *cmd_enc;
	struct sde_encoder_phys_cmd_te_timestamp *cur;
	struct sde_encoder_phys_cmd_te_timestamp *prev = NULL;
	ktime_t time_diff;
	u64 l_bound = 0, u_bound = 0;
	bool ret = false;
	unsigned long lock_flags;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);
	sde_encoder_helper_get_jitter_bounds_ns(phys_enc->parent,
							&l_bound, &u_bound);
	if (!l_bound || !u_bound) {
		SDE_ERROR_CMDENC(cmd_enc, "invalid vsync jitter bounds\n");
		return false;
	}

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	list_for_each_entry_reverse(cur, &cmd_enc->te_timestamp_list, list) {
		if (prev && ktime_after(cur->timestamp, profile_timestamp)) {
			time_diff = ktime_sub(prev->timestamp, cur->timestamp);
			if ((time_diff < l_bound) || (time_diff > u_bound)) {
				ret = true;
				break;
			}
		}
		prev = cur;
	}
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	if (ret) {
		SDE_DEBUG_CMDENC(cmd_enc,
		    "time_diff:%llu, prev:%llu, cur:%llu, jitter:%llu/%llu\n",
			time_diff, prev->timestamp, cur->timestamp,
			l_bound, u_bound);
		time_diff = div_s64(time_diff, 1000);

		SDE_EVT32(DRMID(phys_enc->parent),
			(u32) (do_div(l_bound, 1000)),
			(u32) (do_div(u_bound, 1000)),
			(u32) (time_diff), SDE_EVTLOG_ERROR);
	}

	return ret;
}

static int _sde_encoder_phys_cmd_wait_for_wr_ptr(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_encoder_wait_info wait_info = {0};
	int ret;
	bool frame_pending = true;
	struct sde_hw_ctl *ctl;
	unsigned long lock_flags;

	if (!phys_enc || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}
	ctl = phys_enc->hw_ctl;

	wait_info.wq = &phys_enc->pending_kickoff_wq;
	wait_info.atomic_cnt = &phys_enc->pending_retire_fence_cnt;
	wait_info.timeout_ms = KICKOFF_TIMEOUT_MS;

	/* slave encoder doesn't enable for ppsplit */
	if (_sde_encoder_phys_is_ppsplit_slave(phys_enc))
		return 0;

	ret = sde_encoder_helper_wait_for_irq(phys_enc, INTR_IDX_WRPTR,
			&wait_info);
	if (ret == -ETIMEDOUT) {
		struct sde_hw_ctl *ctl = phys_enc->hw_ctl;

		if (ctl && ctl->ops.get_start_state)
			frame_pending = ctl->ops.get_start_state(ctl);

		ret = frame_pending ? ret : 0;

		/*
		 * There can be few cases of ESD where CTL_START is cleared but
		 * wr_ptr irq doesn't come. Signaling retire fence in these
		 * cases to avoid freeze and dangling pending_retire_fence_cnt
		 */
		if (!ret) {
			SDE_EVT32(DRMID(phys_enc->parent),
				SDE_EVTLOG_FUNC_CASE1);

			if (sde_encoder_phys_cmd_is_master(phys_enc) &&
				atomic_add_unless(
				&phys_enc->pending_retire_fence_cnt, -1, 0)) {
				spin_lock_irqsave(phys_enc->enc_spinlock,
					lock_flags);
				phys_enc->parent_ops.handle_frame_done(
				 phys_enc->parent, phys_enc,
				 SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE);
				spin_unlock_irqrestore(phys_enc->enc_spinlock,
					lock_flags);
			}
		}
	}

	cmd_enc->wr_ptr_wait_success = (ret == 0) ? true : false;
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

	if (!atomic_read(&phys_enc->pending_kickoff_cnt)) {
		SDE_EVT32(DRMID(phys_enc->parent),
			phys_enc->intf_idx - INTF_0,
			phys_enc->enable_state);
		return 0;
	}

	rc = _sde_encoder_phys_cmd_wait_for_idle(phys_enc);
	if (rc) {
		SDE_EVT32(DRMID(phys_enc->parent),
				phys_enc->intf_idx - INTF_0);
		SDE_ERROR("failed wait_for_idle: %d\n", rc);
	}

	return rc;
}

static int _sde_encoder_phys_cmd_handle_wr_ptr_timeout(
		struct sde_encoder_phys *phys_enc,
		ktime_t profile_timestamp)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	bool switch_te;
	int ret = -ETIMEDOUT;
	unsigned long lock_flags;

	switch_te = _sde_encoder_phys_cmd_needs_vsync_change(
				phys_enc, profile_timestamp);

	SDE_EVT32(DRMID(phys_enc->parent), switch_te, SDE_EVTLOG_FUNC_ENTRY);

	if (switch_te) {
		SDE_DEBUG_CMDENC(cmd_enc,
				"wr_ptr_irq wait failed, retry with WD TE\n");

		/* switch to watchdog TE and wait again */
		sde_encoder_helper_switch_vsync(phys_enc->parent, true);

		ret = _sde_encoder_phys_cmd_wait_for_wr_ptr(phys_enc);

		/* switch back to default TE */
		sde_encoder_helper_switch_vsync(phys_enc->parent, false);
	}

	/*
	 * Signaling the retire fence at wr_ptr timeout
	 * to allow the next commit and avoid device freeze.
	 */
	if (ret == -ETIMEDOUT) {
		SDE_ERROR_CMDENC(cmd_enc,
			"wr_ptr_irq wait failed, switch_te:%d\n", switch_te);
		SDE_EVT32(DRMID(phys_enc->parent), switch_te, SDE_EVTLOG_ERROR);

		if (sde_encoder_phys_cmd_is_master(phys_enc) &&
			atomic_add_unless(
			&phys_enc->pending_retire_fence_cnt, -1, 0)) {
			spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
			phys_enc->parent_ops.handle_frame_done(
				phys_enc->parent, phys_enc,
				SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE);
			spin_unlock_irqrestore(phys_enc->enc_spinlock,
				lock_flags);
		}
	}

	cmd_enc->wr_ptr_wait_success = (ret == 0) ? true : false;

	return ret;
}

static int sde_encoder_phys_cmd_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc)
{
	int rc = 0, i, pending_cnt;
	struct sde_encoder_phys_cmd *cmd_enc;
	ktime_t profile_timestamp = ktime_get();
	u32 scheduler_status = INVALID_CTL_STATUS;
	struct sde_hw_ctl *ctl;

	if (!phys_enc)
		return -EINVAL;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	/* only required for master controller */
	if (sde_encoder_phys_cmd_is_master(phys_enc)) {
		rc = _sde_encoder_phys_cmd_wait_for_wr_ptr(phys_enc);
		if (rc == -ETIMEDOUT) {
			/*
			 * Profile all the TE received after profile_timestamp
			 * and if the jitter is more, switch to watchdog TE
			 * and wait for wr_ptr again. Finally move back to
			 * default TE.
			 */
			rc = _sde_encoder_phys_cmd_handle_wr_ptr_timeout(
					phys_enc, profile_timestamp);
			if (rc == -ETIMEDOUT)
				goto wait_for_idle;
		}

		if (cmd_enc->autorefresh.cfg.enable)
			rc = _sde_encoder_phys_cmd_wait_for_autorefresh_done(
								phys_enc);

		ctl = phys_enc->hw_ctl;
		if (ctl && ctl->ops.get_scheduler_status)
			scheduler_status = ctl->ops.get_scheduler_status(ctl);
	}

	/* wait for posted start or serialize trigger */
	pending_cnt = atomic_read(&phys_enc->pending_kickoff_cnt);
	if ((pending_cnt > 1) ||
	    (pending_cnt && (scheduler_status & BIT(0))) ||
	    (!rc && phys_enc->frame_trigger_mode == FRAME_DONE_WAIT_SERIALIZE))
		goto wait_for_idle;

	return rc;

wait_for_idle:
	pending_cnt = atomic_read(&phys_enc->pending_kickoff_cnt);
	for (i = 0; i < pending_cnt; i++)
		rc |= sde_encoder_wait_for_event(phys_enc->parent,
				MSM_ENC_TX_COMPLETE);
	if (rc) {
		SDE_EVT32(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0,
			phys_enc->frame_trigger_mode,
			atomic_read(&phys_enc->pending_kickoff_cnt),
			phys_enc->enable_state,
			cmd_enc->wr_ptr_wait_success, scheduler_status, rc);
		SDE_ERROR("pp:%d failed wait_for_idle: %d\n",
				phys_enc->hw_pp->idx - PINGPONG_0, rc);
		if (phys_enc->enable_state == SDE_ENC_ERR_NEEDS_HW_RESET)
			sde_encoder_needs_hw_reset(phys_enc->parent);
	}

	return rc;
}

static int sde_encoder_phys_cmd_wait_for_vblank(
		struct sde_encoder_phys *phys_enc)
{
	int rc = 0;
	struct sde_encoder_phys_cmd *cmd_enc;
	struct sde_encoder_wait_info wait_info = {0};

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

static void _sde_encoder_autorefresh_disable_seq1(
		struct sde_encoder_phys *phys_enc)
{
	int trial = 0;
	struct sde_encoder_phys_cmd *cmd_enc =
				to_sde_encoder_phys_cmd(phys_enc);

	/*
	 * If autorefresh is enabled, disable it and make sure it is safe to
	 * proceed with current frame commit/push. Sequence fallowed is,
	 * 1. Disable TE - caller will take care of it
	 * 2. Disable autorefresh config
	 * 4. Poll for frame transfer ongoing to be false
	 * 5. Enable TE back - caller will take care of it
	 */
	_sde_encoder_phys_cmd_config_autorefresh(phys_enc, 0);

	do {
		udelay(AUTOREFRESH_SEQ1_POLL_TIME);
		if ((trial * AUTOREFRESH_SEQ1_POLL_TIME)
				> (KICKOFF_TIMEOUT_MS * USEC_PER_MSEC)) {
			SDE_ERROR_CMDENC(cmd_enc,
					"disable autorefresh failed\n");

			phys_enc->enable_state = SDE_ENC_ERR_NEEDS_HW_RESET;
			break;
		}

		trial++;
	} while (_sde_encoder_phys_cmd_is_ongoing_pptx(phys_enc));
}

static void _sde_encoder_autorefresh_disable_seq2(
		struct sde_encoder_phys *phys_enc)
{
	int trial = 0;
	struct sde_hw_mdp *hw_mdp = phys_enc->hw_mdptop;
	u32 autorefresh_status = 0;
	struct sde_encoder_phys_cmd *cmd_enc =
				to_sde_encoder_phys_cmd(phys_enc);
	struct intf_tear_status tear_status;
	struct sde_hw_intf *hw_intf = phys_enc->hw_intf;

	if (!hw_mdp->ops.get_autorefresh_status ||
			!hw_intf->ops.check_and_reset_tearcheck) {
		SDE_DEBUG_CMDENC(cmd_enc,
			"autofresh disable seq2 not supported\n");
		return;
	}

	/*
	 * If autorefresh is still enabled after sequence-1, proceed with
	 * below sequence-2.
	 * 1. Disable autorefresh config
	 * 2. Run in loop:
	 *    2.1 Poll for autorefresh to be disabled
	 *    2.2 Log read and write count status
	 *    2.3 Replace te write count with start_pos to meet trigger window
	 */
	autorefresh_status = hw_mdp->ops.get_autorefresh_status(hw_mdp,
					phys_enc->intf_idx);
	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->intf_idx - INTF_0,
				autorefresh_status, SDE_EVTLOG_FUNC_CASE1);

	if (!(autorefresh_status & BIT(7))) {
		usleep_range(AUTOREFRESH_SEQ2_POLL_TIME,
			AUTOREFRESH_SEQ2_POLL_TIME + 1);

		autorefresh_status = hw_mdp->ops.get_autorefresh_status(hw_mdp,
					phys_enc->intf_idx);
		SDE_EVT32(DRMID(phys_enc->parent), phys_enc->intf_idx - INTF_0,
				autorefresh_status, SDE_EVTLOG_FUNC_CASE2);
	}

	while (autorefresh_status & BIT(7)) {
		if (!trial) {
			SDE_ERROR_CMDENC(cmd_enc,
			  "autofresh status:0x%x intf:%d\n", autorefresh_status,
			  phys_enc->intf_idx - INTF_0);

			_sde_encoder_phys_cmd_config_autorefresh(phys_enc, 0);
		}

		usleep_range(AUTOREFRESH_SEQ2_POLL_TIME,
				AUTOREFRESH_SEQ2_POLL_TIME + 1);
		if ((trial * AUTOREFRESH_SEQ2_POLL_TIME)
			> AUTOREFRESH_SEQ2_POLL_TIMEOUT) {
			SDE_ERROR_CMDENC(cmd_enc,
					"disable autorefresh failed\n");
			SDE_DBG_DUMP("all", "dbg_bus", "vbif_dbg_bus", "panic");
			break;
		}

		trial++;
		autorefresh_status = hw_mdp->ops.get_autorefresh_status(hw_mdp,
					phys_enc->intf_idx);
		hw_intf->ops.check_and_reset_tearcheck(hw_intf, &tear_status);
		SDE_ERROR_CMDENC(cmd_enc,
			"autofresh status:0x%x intf:%d tear_read:0x%x tear_write:0x%x\n",
			autorefresh_status, phys_enc->intf_idx - INTF_0,
			tear_status.read_count, tear_status.write_count);
		SDE_EVT32(DRMID(phys_enc->parent), phys_enc->intf_idx - INTF_0,
			autorefresh_status, tear_status.read_count,
			tear_status.write_count);
	}
}

static void sde_encoder_phys_cmd_prepare_commit(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc)
		return;

	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		return;

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->intf_idx - INTF_0,
			cmd_enc->autorefresh.cfg.enable);

	if (!sde_encoder_phys_cmd_is_autorefresh_enabled(phys_enc))
		return;

	sde_encoder_phys_cmd_connect_te(phys_enc, false);
	_sde_encoder_autorefresh_disable_seq1(phys_enc);
	_sde_encoder_autorefresh_disable_seq2(phys_enc);
	sde_encoder_phys_cmd_connect_te(phys_enc, true);

	SDE_DEBUG_CMDENC(cmd_enc, "autorefresh disabled successfully\n");
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

	/* wr_ptr_wait_success is set true when wr_ptr arrives */
	cmd_enc->wr_ptr_wait_success = false;
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
	ops->setup_misr = sde_encoder_helper_setup_misr;
	ops->collect_misr = sde_encoder_helper_collect_misr;
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
	irq->cb.func = NULL;

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

	irq = &phys_enc->irq[INTR_IDX_WRPTR];
	irq->intr_idx = INTR_IDX_WRPTR;
	irq->name = "wr_ptr";

	if (phys_enc->has_intf_te)
		irq->intr_type = SDE_IRQ_TYPE_INTF_TEAR_WR_PTR;
	else
		irq->intr_type = SDE_IRQ_TYPE_PING_PONG_WR_PTR;
	irq->cb.func = sde_encoder_phys_cmd_wr_ptr_irq;

	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
	atomic_set(&phys_enc->pending_retire_fence_cnt, 0);
	atomic_set(&cmd_enc->pending_vblank_cnt, 0);
	init_waitqueue_head(&phys_enc->pending_kickoff_wq);
	init_waitqueue_head(&cmd_enc->pending_vblank_wq);
	atomic_set(&cmd_enc->autorefresh.kickoff_cnt, 0);
	init_waitqueue_head(&cmd_enc->autorefresh.kickoff_wq);
	INIT_LIST_HEAD(&cmd_enc->te_timestamp_list);
	for (i = 0; i < MAX_TE_PROFILE_COUNT; i++)
		list_add(&cmd_enc->te_timestamp[i].list,
				&cmd_enc->te_timestamp_list);

	SDE_DEBUG_CMDENC(cmd_enc, "created\n");

	return phys_enc;

fail_mdp_init:
	kfree(cmd_enc);
fail:
	return ERR_PTR(ret);
}
