/*
 * Copyright (c) 2015-2017 The Linux Foundation. All rights reserved.
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

#define PP_TIMEOUT_MAX_TRIALS	10

/* wait for 2 vyncs only */
#define CTL_START_TIMEOUT_MS	32

/*
 * Tearcheck sync start and continue thresholds are empirically found
 * based on common panels In the future, may want to allow panels to override
 * these default values
 */
#define DEFAULT_TEARCHECK_SYNC_THRESH_START	4
#define DEFAULT_TEARCHECK_SYNC_THRESH_CONTINUE	4

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

static void _sde_encoder_phys_cmd_update_flush_mask(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_ctl *ctl;
	u32 flush_mask = 0;

	ctl = phys_enc->hw_ctl;
	if (!ctl || !ctl->ops.get_bitmask_intf ||
			!ctl->ops.update_pending_flush)
		return;

	ctl->ops.get_bitmask_intf(ctl, &flush_mask, phys_enc->intf_idx);
	ctl->ops.update_pending_flush(ctl, flush_mask);

	SDE_DEBUG_CMDENC(cmd_enc, "update pending flush ctl %d flush_mask %x\n",
			ctl->idx - CTL_0, flush_mask);
}

static void _sde_encoder_phys_cmd_update_intf_cfg(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_ctl *ctl;
	struct sde_hw_intf_cfg intf_cfg = { 0 };

	ctl = phys_enc->hw_ctl;
	if (!ctl || !ctl->ops.setup_intf_cfg)
		return;

	intf_cfg.intf = phys_enc->intf_idx;
	intf_cfg.intf_mode_sel = SDE_CTL_MODE_SEL_CMD;
	intf_cfg.stream_sel = cmd_enc->stream_sel;
	intf_cfg.mode_3d = sde_encoder_helper_get_3d_blend_mode(phys_enc);
	ctl->ops.setup_intf_cfg(ctl, &intf_cfg);
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
		SDE_ERROR("invalid arg(s), enc %d mode %d adj_mode %d\n",
				phys_enc != 0, mode != 0, adj_mode != 0);
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
}

static void sde_encoder_phys_cmd_pp_tx_done_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_cmd *cmd_enc = arg;
	struct sde_encoder_phys *phys_enc;
	unsigned long lock_flags;
	int new_cnt;

	if (!cmd_enc)
		return;

	phys_enc = &cmd_enc->base;

	/* notify all synchronous clients first, then asynchronous clients */
	if (phys_enc->parent_ops.handle_frame_done)
		phys_enc->parent_ops.handle_frame_done(phys_enc->parent,
				phys_enc, SDE_ENCODER_FRAME_EVENT_DONE);

	spin_lock_irqsave(phys_enc->enc_spinlock, lock_flags);
	new_cnt = atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);
	spin_unlock_irqrestore(phys_enc->enc_spinlock, lock_flags);

	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0, new_cnt);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&phys_enc->pending_kickoff_wq);
}

static void sde_encoder_phys_cmd_pp_rd_ptr_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_cmd *cmd_enc = arg;
	struct sde_encoder_phys *phys_enc = &cmd_enc->base;

	if (!cmd_enc)
		return;

	SDE_EVT32_IRQ(DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0, 0xfff);

	if (phys_enc->parent_ops.handle_vblank_virt)
		phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent,
			phys_enc);
}

static void sde_encoder_phys_cmd_ctl_start_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_cmd *cmd_enc = arg;
	struct sde_encoder_phys *phys_enc;
	struct sde_hw_ctl *ctl;

	if (!cmd_enc)
		return;

	phys_enc = &cmd_enc->base;
	if (!phys_enc->hw_ctl)
		return;

	ctl = phys_enc->hw_ctl;
	SDE_EVT32_IRQ(DRMID(phys_enc->parent), ctl->idx - CTL_0, 0xfff);
	atomic_add_unless(&phys_enc->pending_ctlstart_cnt, -1, 0);

	/* Signal any waiting ctl start interrupt */
	wake_up_all(&phys_enc->pending_kickoff_wq);
}

static bool _sde_encoder_phys_is_ppsplit(struct sde_encoder_phys *phys_enc)
{
	enum sde_rm_topology_name topology;

	if (!phys_enc)
		return false;

	topology = sde_connector_get_topology_name(phys_enc->connector);
	if (topology == SDE_RM_TOPOLOGY_PPSPLIT)
		return true;

	return false;
}

static int _sde_encoder_phys_cmd_handle_ppdone_timeout(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	u32 frame_event = SDE_ENCODER_FRAME_EVENT_ERROR;
	bool do_log = false;

	cmd_enc->pp_timeout_report_cnt++;
	if (cmd_enc->pp_timeout_report_cnt == PP_TIMEOUT_MAX_TRIALS) {
		frame_event |= SDE_ENCODER_FRAME_EVENT_PANEL_DEAD;
		do_log = true;
	} else if (cmd_enc->pp_timeout_report_cnt == 1) {
		do_log = true;
	}

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			cmd_enc->pp_timeout_report_cnt,
			atomic_read(&phys_enc->pending_kickoff_cnt));

	/* to avoid flooding, only log first time, and "dead" time */
	if (do_log) {
		SDE_ERROR_CMDENC(cmd_enc,
				"pp:%d kickoff timed out ctl %d cnt %d koff_cnt %d\n",
				phys_enc->hw_pp->idx - PINGPONG_0,
				phys_enc->hw_ctl->idx - CTL_0,
				cmd_enc->pp_timeout_report_cnt,
				atomic_read(&phys_enc->pending_kickoff_cnt));

		SDE_EVT32(DRMID(phys_enc->parent), SDE_EVTLOG_FATAL);

		SDE_DBG_DUMP("sde", "dsi0_ctrl", "dsi0_phy", "dsi1_ctrl",
				"dsi1_phy", "vbif", "dbg_bus",
				"vbif_dbg_bus", "panic");
	}

	atomic_add_unless(&phys_enc->pending_kickoff_cnt, -1, 0);

	/* request a ctl reset before the next kickoff */
	phys_enc->enable_state = SDE_ENC_ERR_NEEDS_HW_RESET;

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

static int _sde_encoder_phys_cmd_wait_for_idle(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	u32 irq_status;
	int ret;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	/* slave encoder doesn't enable for ppsplit */
	if (_sde_encoder_phys_is_ppsplit_slave(phys_enc))
		return 0;

	/* return EWOULDBLOCK since we know the wait isn't necessary */
	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR_CMDENC(cmd_enc, "encoder is disabled\n");
		return -EWOULDBLOCK;
	}

	/* wait for previous kickoff to complete */
	ret = sde_encoder_helper_wait_event_timeout(
			DRMID(phys_enc->parent),
			phys_enc->hw_pp->idx - PINGPONG_0,
			&phys_enc->pending_kickoff_wq,
			&phys_enc->pending_kickoff_cnt,
			KICKOFF_TIMEOUT_MS);
	if (ret <= 0) {
		/* read and clear interrupt */
		irq_status = sde_core_irq_read(phys_enc->sde_kms,
				cmd_enc->irq_idx[INTR_IDX_PINGPONG], true);
		if (irq_status) {
			unsigned long flags;
			SDE_EVT32(DRMID(phys_enc->parent),
					phys_enc->hw_pp->idx - PINGPONG_0);
			SDE_DEBUG_CMDENC(cmd_enc,
					"pp:%d done but irq not triggered\n",
					phys_enc->hw_pp->idx - PINGPONG_0);
			local_irq_save(flags);
			sde_encoder_phys_cmd_pp_tx_done_irq(cmd_enc,
					INTR_IDX_PINGPONG);
			local_irq_restore(flags);
			ret = 0;
		} else {
			ret = _sde_encoder_phys_cmd_handle_ppdone_timeout(
					phys_enc);
		}
	} else {
		ret = 0;
	}

	if (!ret)
		cmd_enc->pp_timeout_report_cnt = 0;

	return ret;
}

static void sde_encoder_phys_cmd_underrun_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_cmd *cmd_enc = arg;
	struct sde_encoder_phys *phys_enc;

	if (!cmd_enc)
		return;

	phys_enc = &cmd_enc->base;
	if (phys_enc->parent_ops.handle_underrun_virt)
		phys_enc->parent_ops.handle_underrun_virt(phys_enc->parent,
			phys_enc);
}

static int sde_encoder_phys_cmd_register_irq(struct sde_encoder_phys *phys_enc,
	enum sde_intr_type intr_type, int idx,
	void (*irq_func)(void *, int), const char *irq_name)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	int idx_lookup = 0;
	int ret = 0;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	if (intr_type == SDE_IRQ_TYPE_INTF_UNDER_RUN)
		idx_lookup = phys_enc->intf_idx;
	else if (intr_type == SDE_IRQ_TYPE_CTL_START)
		idx_lookup = phys_enc->hw_ctl ? phys_enc->hw_ctl->idx : -1;
	else
		idx_lookup = phys_enc->hw_pp->idx;

	cmd_enc->irq_idx[idx] = sde_core_irq_idx_lookup(phys_enc->sde_kms,
			intr_type, idx_lookup);
	if (cmd_enc->irq_idx[idx] < 0) {
		SDE_ERROR_CMDENC(cmd_enc,
			"failed to lookup IRQ index for %s with pp=%d\n",
			irq_name,
			phys_enc->hw_pp->idx - PINGPONG_0);
		return -EINVAL;
	}

	cmd_enc->irq_cb[idx].func = irq_func;
	cmd_enc->irq_cb[idx].arg = cmd_enc;
	ret = sde_core_irq_register_callback(phys_enc->sde_kms,
			cmd_enc->irq_idx[idx], &cmd_enc->irq_cb[idx]);
	if (ret) {
		SDE_ERROR_CMDENC(cmd_enc,
				"failed to register IRQ callback %s\n",
				irq_name);
		return ret;
	}

	ret = sde_core_irq_enable(phys_enc->sde_kms, &cmd_enc->irq_idx[idx], 1);
	if (ret) {
		SDE_ERROR_CMDENC(cmd_enc,
			"failed to enable IRQ for %s, pp %d, irq_idx %d\n",
			irq_name,
			phys_enc->hw_pp->idx - PINGPONG_0,
			cmd_enc->irq_idx[idx]);
		cmd_enc->irq_idx[idx] = -EINVAL;

		/* Unregister callback on IRQ enable failure */
		sde_core_irq_unregister_callback(phys_enc->sde_kms,
				cmd_enc->irq_idx[idx], &cmd_enc->irq_cb[idx]);
		return ret;
	}

	SDE_DEBUG_CMDENC(cmd_enc, "registered IRQ %s for pp %d, irq_idx %d\n",
			irq_name,
			phys_enc->hw_pp->idx - PINGPONG_0,
			cmd_enc->irq_idx[idx]);

	return ret;
}

static int sde_encoder_phys_cmd_unregister_irq(
		struct sde_encoder_phys *phys_enc, int idx)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	sde_core_irq_disable(phys_enc->sde_kms, &cmd_enc->irq_idx[idx], 1);
	sde_core_irq_unregister_callback(phys_enc->sde_kms,
			cmd_enc->irq_idx[idx], &cmd_enc->irq_cb[idx]);

	SDE_DEBUG_CMDENC(cmd_enc, "unregistered IRQ for pp %d, irq_idx %d\n",
			phys_enc->hw_pp->idx - PINGPONG_0,
			cmd_enc->irq_idx[idx]);

	return 0;
}

static int sde_encoder_phys_cmd_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	int ret = 0;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	/* Slave encoders don't report vblank */
	if (!sde_encoder_phys_cmd_is_master(phys_enc))
		goto end;

	SDE_DEBUG_CMDENC(cmd_enc, "[%pS] enable=%d/%d\n",
			__builtin_return_address(0),
			enable, atomic_read(&phys_enc->vblank_refcount));

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0,
			enable, atomic_read(&phys_enc->vblank_refcount));

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1)
		ret = sde_encoder_phys_cmd_register_irq(phys_enc,
				SDE_IRQ_TYPE_PING_PONG_RD_PTR,
				INTR_IDX_RDPTR,
				sde_encoder_phys_cmd_pp_rd_ptr_irq,
				"pp_rd_ptr");
	else if (!enable && atomic_dec_return(&phys_enc->vblank_refcount) == 0)
		ret = sde_encoder_phys_cmd_unregister_irq(phys_enc,
				INTR_IDX_RDPTR);

end:
	if (ret)
		SDE_ERROR_CMDENC(cmd_enc,
				"control vblank irq error %d, enable %d\n",
				ret, enable);

	return ret;
}

void sde_encoder_phys_cmd_irq_control(struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct sde_encoder_phys_cmd *cmd_enc;

	if (!phys_enc || _sde_encoder_phys_is_ppsplit_slave(phys_enc))
		return;

	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);

	if (enable) {
		sde_encoder_phys_cmd_register_irq(phys_enc,
				SDE_IRQ_TYPE_PING_PONG_COMP,
				INTR_IDX_PINGPONG,
				sde_encoder_phys_cmd_pp_tx_done_irq,
				"pp_tx_done");

		sde_encoder_phys_cmd_control_vblank_irq(phys_enc, true);

		sde_encoder_phys_cmd_register_irq(phys_enc,
				SDE_IRQ_TYPE_INTF_UNDER_RUN,
				INTR_IDX_UNDERRUN,
				sde_encoder_phys_cmd_underrun_irq,
				"underrun");

		if (sde_encoder_phys_cmd_is_master(phys_enc))
			sde_encoder_phys_cmd_register_irq(phys_enc,
				SDE_IRQ_TYPE_CTL_START,
				INTR_IDX_CTL_START,
				sde_encoder_phys_cmd_ctl_start_irq,
				"ctl_start");
	} else {
		if (sde_encoder_phys_cmd_is_master(phys_enc))
			sde_encoder_phys_cmd_unregister_irq(
				phys_enc, INTR_IDX_CTL_START);
		sde_encoder_phys_cmd_unregister_irq(
				phys_enc, INTR_IDX_UNDERRUN);
		sde_encoder_phys_cmd_control_vblank_irq(phys_enc, false);
		sde_encoder_phys_cmd_unregister_irq(
				phys_enc, INTR_IDX_PINGPONG);
	}
}

static void sde_encoder_phys_cmd_tearcheck_config(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_tear_check tc_cfg = { 0 };
	struct drm_display_mode *mode = &phys_enc->cached_mode;
	bool tc_enable = true;
	u32 vsync_hz;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	SDE_DEBUG_CMDENC(cmd_enc, "pp %d\n", phys_enc->hw_pp->idx - PINGPONG_0);

	if (!phys_enc->hw_pp->ops.setup_tearcheck ||
		!phys_enc->hw_pp->ops.enable_tearcheck) {
		SDE_DEBUG_CMDENC(cmd_enc, "tearcheck not supported\n");
		return;
	}

	sde_kms = phys_enc->sde_kms;
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
	if (!vsync_hz) {
		SDE_DEBUG_CMDENC(cmd_enc, "invalid vsync clock rate\n");
		return;
	}

	tc_cfg.vsync_count = vsync_hz / (mode->vtotal * mode->vrefresh);
	tc_cfg.hw_vsync_mode = 1;

	/*
	 * By setting sync_cfg_height to near max register value, we essentially
	 * disable sde hw generated TE signal, since hw TE will arrive first.
	 * Only caveat is if due to error, we hit wrap-around.
	 */
	tc_cfg.sync_cfg_height = 0xFFF0;
	tc_cfg.vsync_init_val = mode->vdisplay;
	tc_cfg.sync_threshold_start = DEFAULT_TEARCHECK_SYNC_THRESH_START;
	tc_cfg.sync_threshold_continue = DEFAULT_TEARCHECK_SYNC_THRESH_CONTINUE;
	tc_cfg.start_pos = mode->vdisplay;
	tc_cfg.rd_ptr_irq = mode->vdisplay + 1;

	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d vsync_clk_speed_hz %u vtotal %u vrefresh %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0, vsync_hz,
		mode->vtotal, mode->vrefresh);
	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d enable %u start_pos %u rd_ptr_irq %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0, tc_enable, tc_cfg.start_pos,
		tc_cfg.rd_ptr_irq);
	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d hw_vsync_mode %u vsync_count %u vsync_init_val %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0, tc_cfg.hw_vsync_mode,
		tc_cfg.vsync_count, tc_cfg.vsync_init_val);
	SDE_DEBUG_CMDENC(cmd_enc,
		"tc %d cfgheight %u thresh_start %u thresh_cont %u\n",
		phys_enc->hw_pp->idx - PINGPONG_0, tc_cfg.sync_cfg_height,
		tc_cfg.sync_threshold_start, tc_cfg.sync_threshold_continue);

	phys_enc->hw_pp->ops.setup_tearcheck(phys_enc->hw_pp, &tc_cfg);
	phys_enc->hw_pp->ops.enable_tearcheck(phys_enc->hw_pp, tc_enable);
}

static void _sde_encoder_phys_cmd_pingpong_config(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc || !phys_enc->hw_ctl ||
			!phys_enc->hw_ctl->ops.setup_intf_cfg) {
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

static bool sde_encoder_phys_cmd_needs_single_flush(
		struct sde_encoder_phys *phys_enc)
{
	if (!phys_enc)
		return false;

	return _sde_encoder_phys_is_ppsplit(phys_enc);
}

static void sde_encoder_phys_cmd_enable_helper(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_ctl *ctl;
	u32 flush_mask = 0;

	if (!phys_enc || !phys_enc->hw_ctl) {
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

	ctl = phys_enc->hw_ctl;
	ctl->ops.get_bitmask_intf(ctl, &flush_mask, phys_enc->intf_idx);
	ctl->ops.update_pending_flush(ctl, flush_mask);

skip_flush:
	return;
}

static void sde_encoder_phys_cmd_enable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid phys encoder\n");
		return;
	}
	SDE_DEBUG_CMDENC(cmd_enc, "pp %d\n", phys_enc->hw_pp->idx - PINGPONG_0);

	if (phys_enc->enable_state == SDE_ENC_ENABLED) {
		SDE_ERROR("already enabled\n");
		return;
	}

	sde_encoder_phys_cmd_enable_helper(phys_enc);
	phys_enc->enable_state = SDE_ENC_ENABLED;
}

static void sde_encoder_phys_cmd_disable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	int ret;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_CMDENC(cmd_enc, "pp %d\n", phys_enc->hw_pp->idx - PINGPONG_0);

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR_CMDENC(cmd_enc, "already disabled\n");
		return;
	}

	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0);

	if (!_sde_encoder_phys_is_ppsplit_slave(phys_enc)) {
		ret = _sde_encoder_phys_cmd_wait_for_idle(phys_enc);
		if (ret) {
			atomic_set(&phys_enc->pending_kickoff_cnt, 0);
			SDE_ERROR_CMDENC(cmd_enc,
					"pp %d failed wait for idle, %d\n",
					phys_enc->hw_pp->idx - PINGPONG_0, ret);
			SDE_EVT32(DRMID(phys_enc->parent),
					phys_enc->hw_pp->idx - PINGPONG_0, ret);
		}
	}

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
	SDE_DEBUG_CMDENC(cmd_enc, "\n");
	hw_res->intfs[phys_enc->intf_idx - INTF_0] = INTF_MODE_CMD;
}

static void sde_encoder_phys_cmd_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_kickoff_params *params)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	int ret;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_CMDENC(cmd_enc, "pp %d\n", phys_enc->hw_pp->idx - PINGPONG_0);
	SDE_EVT32(DRMID(phys_enc->parent), phys_enc->hw_pp->idx - PINGPONG_0);

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
}

static int _sde_encoder_phys_cmd_wait_for_ctl_start(
		struct sde_encoder_phys *phys_enc)
{
	int rc = 0;
	struct sde_hw_ctl *ctl;
	u32 irq_status;
	struct sde_encoder_phys_cmd *cmd_enc;

	if (!phys_enc->hw_ctl) {
		SDE_ERROR("invalid ctl\n");
		return -EINVAL;
	}

	ctl = phys_enc->hw_ctl;
	cmd_enc = to_sde_encoder_phys_cmd(phys_enc);
	rc = sde_encoder_helper_wait_event_timeout(DRMID(phys_enc->parent),
			ctl->idx - CTL_0,
			&phys_enc->pending_kickoff_wq,
			&phys_enc->pending_ctlstart_cnt,
			CTL_START_TIMEOUT_MS);
	if (rc <= 0) {
		/* read and clear interrupt */
		irq_status = sde_core_irq_read(phys_enc->sde_kms,
				cmd_enc->irq_idx[INTR_IDX_CTL_START], true);
		if (irq_status) {
			unsigned long flags;

			SDE_EVT32(DRMID(phys_enc->parent), ctl->idx - CTL_0);
			SDE_DEBUG_CMDENC(cmd_enc,
					"ctl:%d start done but irq not triggered\n",
					ctl->idx - CTL_0);
			local_irq_save(flags);
			sde_encoder_phys_cmd_ctl_start_irq(cmd_enc,
					INTR_IDX_CTL_START);
			local_irq_restore(flags);
			rc = 0;
		} else {
			SDE_ERROR("ctl start interrupt wait failed\n");
			rc = -EINVAL;
		}
	} else {
		rc = 0;
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

	/* required for both controllers */
	if (!rc && cmd_enc->serialize_wait4pp)
		sde_encoder_phys_cmd_prepare_for_kickoff(phys_enc, NULL);

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

static void sde_encoder_phys_cmd_init_ops(
		struct sde_encoder_phys_ops *ops)
{
	ops->is_master = sde_encoder_phys_cmd_is_master;
	ops->mode_set = sde_encoder_phys_cmd_mode_set;
	ops->mode_fixup = sde_encoder_phys_cmd_mode_fixup;
	ops->enable = sde_encoder_phys_cmd_enable;
	ops->disable = sde_encoder_phys_cmd_disable;
	ops->destroy = sde_encoder_phys_cmd_destroy;
	ops->get_hw_resources = sde_encoder_phys_cmd_get_hw_resources;
	ops->control_vblank_irq = sde_encoder_phys_cmd_control_vblank_irq;
	ops->wait_for_commit_done = sde_encoder_phys_cmd_wait_for_commit_done;
	ops->prepare_for_kickoff = sde_encoder_phys_cmd_prepare_for_kickoff;
	ops->trigger_start = sde_encoder_helper_trigger_start;
	ops->needs_single_flush = sde_encoder_phys_cmd_needs_single_flush;
	ops->hw_reset = sde_encoder_helper_hw_reset;
	ops->irq_control = sde_encoder_phys_cmd_irq_control;
	ops->update_split_role = sde_encoder_phys_cmd_update_split_role;
	ops->restore = sde_encoder_phys_cmd_enable_helper;
}

struct sde_encoder_phys *sde_encoder_phys_cmd_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc = NULL;
	struct sde_encoder_phys_cmd *cmd_enc = NULL;
	struct sde_hw_mdp *hw_mdp;
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

	sde_encoder_phys_cmd_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_CMD;
	phys_enc->enc_spinlock = p->enc_spinlock;
	cmd_enc->stream_sel = 0;
	phys_enc->enable_state = SDE_ENC_DISABLED;
	phys_enc->comp_type = p->comp_type;
	for (i = 0; i < INTR_IDX_MAX; i++)
		INIT_LIST_HEAD(&cmd_enc->irq_cb[i].list);
	atomic_set(&phys_enc->vblank_refcount, 0);
	atomic_set(&phys_enc->pending_kickoff_cnt, 0);
	atomic_set(&phys_enc->pending_ctlstart_cnt, 0);
	init_waitqueue_head(&phys_enc->pending_kickoff_wq);

	SDE_DEBUG_CMDENC(cmd_enc, "created\n");

	return phys_enc;

fail_mdp_init:
	kfree(cmd_enc);
fail:
	return ERR_PTR(ret);
}
