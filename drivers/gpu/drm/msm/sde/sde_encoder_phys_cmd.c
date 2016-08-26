/*
 * Copyright (c) 2015-2016 The Linux Foundation. All rights reserved.
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

#include <linux/jiffies.h>

#include "sde_encoder_phys.h"
#include "sde_hw_interrupts.h"
#include "sde_formats.h"

#define to_sde_encoder_phys_cmd(x) \
	container_of(x, struct sde_encoder_phys_cmd, base)

#define DEV(phy_enc) (phy_enc->parent->dev)

#define WAIT_TIMEOUT_MSEC			100

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
	return true;
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

	phys_enc->cached_mode = *adj_mode;
	SDE_DEBUG("intf %d, caching mode:\n", cmd_enc->intf_idx);
	drm_mode_debug_printmodeline(adj_mode);

	instance = phys_enc->split_role == ENC_ROLE_SLAVE ? 1 : 0;

	/* Retrieve previously allocated HW Resources. Shouldn't fail */
	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id, SDE_HW_BLK_CTL);
	for (i = 0; i <= instance; i++) {
		sde_rm_get_hw(rm, &iter);
		if (i == instance)
			phys_enc->hw_ctl = (struct sde_hw_ctl *) iter.hw;
	}

	if (IS_ERR_OR_NULL(phys_enc->hw_ctl)) {
		SDE_ERROR("failed init ctl: %ld\n", PTR_ERR(phys_enc->hw_ctl));
		phys_enc->hw_ctl = NULL;
		return;
	}

	sde_rm_init_hw_iter(&iter, phys_enc->parent->base.id,
			SDE_HW_BLK_PINGPONG);
	for (i = 0; i <= instance; i++) {
		sde_rm_get_hw(rm, &iter);
		if (i == instance)
			cmd_enc->hw_pp = (struct sde_hw_pingpong *) iter.hw;
	}

	if (IS_ERR_OR_NULL(cmd_enc->hw_pp)) {
		SDE_ERROR("failed init pingpong: %ld\n",
				PTR_ERR(cmd_enc->hw_pp));
		cmd_enc->hw_pp = NULL;
		phys_enc->hw_ctl = NULL;
		return;
	}

}

static void sde_encoder_phys_cmd_pp_tx_done_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_cmd *cmd_enc = arg;
	struct sde_encoder_phys *phys_enc = &cmd_enc->base;
	int new_pending_cnt;

	new_pending_cnt = atomic_dec_return(&cmd_enc->pending_cnt);
	MSM_EVT(DEV(phys_enc), cmd_enc->hw_pp->idx, new_pending_cnt);

	/* Signal any waiting atomic commit thread */
	wake_up_all(&cmd_enc->pp_tx_done_wq);

	/* Trigger a pending flush */
	phys_enc->parent_ops.handle_ready_for_kickoff(phys_enc->parent,
			phys_enc);
}

static void sde_encoder_phys_cmd_pp_rd_ptr_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_cmd *cmd_enc = arg;
	struct sde_encoder_phys *phys_enc = &cmd_enc->base;

	phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent);
}

static int sde_encoder_phys_cmd_register_pp_irq(
		struct sde_encoder_phys *phys_enc,
		enum sde_intr_type intr_type,
		int *irq_idx,
		void (*irq_func)(void *, int),
		const char *irq_name)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	struct sde_irq_callback irq_cb;
	int ret = 0;

	*irq_idx = sde_irq_idx_lookup(phys_enc->sde_kms, intr_type,
			cmd_enc->hw_pp->idx);
	if (*irq_idx < 0) {
		DRM_ERROR(
			"Failed to lookup IRQ index for %s with pp=%d",
			irq_name,
			cmd_enc->hw_pp->idx);
		return -EINVAL;
	}

	irq_cb.func = irq_func;
	irq_cb.arg = cmd_enc;
	ret = sde_register_irq_callback(phys_enc->sde_kms, *irq_idx, &irq_cb);
	if (ret) {
		DRM_ERROR("Failed to register IRQ callback %s", irq_name);
		return ret;
	}

	ret = sde_enable_irq(phys_enc->sde_kms, irq_idx, 1);
	if (ret) {
		DRM_ERROR(
			"Failed to enable IRQ for %s, pp %d, irq_idx=%d",
			irq_name,
			cmd_enc->hw_pp->idx,
			*irq_idx);
		*irq_idx = -EINVAL;

		/* Unregister callback on IRQ enable failure */
		sde_register_irq_callback(phys_enc->sde_kms, *irq_idx, NULL);
		return ret;
	}

	DBG("registered IRQ %s for pp %d, irq_idx=%d",
			irq_name,
			cmd_enc->hw_pp->idx,
			*irq_idx);

	return ret;
}

static int sde_encoder_phys_cmd_unregister_pp_irq(
		struct sde_encoder_phys *phys_enc,
		int irq_idx)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);

	sde_disable_irq(phys_enc->sde_kms, &irq_idx, 1);
	sde_register_irq_callback(phys_enc->sde_kms, irq_idx, NULL);

	DBG("unregister IRQ for pp %d, irq_idx=%d\n",
			cmd_enc->hw_pp->idx,
			irq_idx);

	return 0;
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

	DBG("intf %d, pp %d", cmd_enc->intf_idx, cmd_enc->hw_pp->idx);

	if (!cmd_enc->hw_pp->ops.setup_tearcheck ||
		!cmd_enc->hw_pp->ops.enable_tearcheck) {
		DBG("tearcheck unsupported");
		return;
	}

	/*
	 * TE default: dsi byte clock calculated base on 70 fps;
	 * around 14 ms to complete a kickoff cycle if te disabled;
	 * vclk_line base on 60 fps; write is faster than read;
	 * init == start == rdptr;
	 *
	 * vsync_count is ratio of MDP VSYNC clock frequency to LCD panel
	 * frequency divided by the no. of rows (lines) in the LCDpanel.
	 */
	vsync_hz = clk_get_rate(phys_enc->sde_kms->vsync_clk);
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

	DBG("tc %d vsync_clk_speed_hz %u mode->vtotal %u mode->vrefresh %u",
		cmd_enc->hw_pp->idx, vsync_hz, mode->vtotal, mode->vrefresh);
	DBG("tc %d enable %u start_pos %u rd_ptr_irq %u",
		tc_enable, cmd_enc->hw_pp->idx, tc_cfg.start_pos,
		tc_cfg.rd_ptr_irq);
	DBG("tc %d hw_vsync_mode %u vsync_count %u vsync_init_val %u",
		cmd_enc->hw_pp->idx, tc_cfg.hw_vsync_mode, tc_cfg.vsync_count,
		tc_cfg.vsync_init_val);
	DBG("tc %d sync_cfgheight %u sync_thresh_start %u sync_thresh_cont %u",
		cmd_enc->hw_pp->idx, tc_cfg.sync_cfg_height,
		tc_cfg.sync_threshold_start, tc_cfg.sync_threshold_continue);

	cmd_enc->hw_pp->ops.setup_tearcheck(cmd_enc->hw_pp, &tc_cfg);
	cmd_enc->hw_pp->ops.enable_tearcheck(cmd_enc->hw_pp, tc_enable);
}

static void sde_encoder_phys_cmd_pingpong_config(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_intf_cfg intf_cfg = { 0 };

	if (!phys_enc->hw_ctl->ops.setup_intf_cfg)
		return;

	DBG("intf %d pp %d, enabling mode:", cmd_enc->intf_idx,
			cmd_enc->hw_pp->idx);
	drm_mode_debug_printmodeline(&phys_enc->cached_mode);

	intf_cfg.intf = cmd_enc->intf_idx;
	intf_cfg.mode_3d = phys_enc->mode_3d;
	intf_cfg.intf_mode_sel = SDE_CTL_MODE_SEL_CMD;
	intf_cfg.stream_sel = cmd_enc->stream_sel;
	phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl, &intf_cfg);

	sde_encoder_phys_cmd_tearcheck_config(phys_enc);
}

static void sde_encoder_phys_cmd_split_config(
		struct sde_encoder_phys *phys_enc, bool enable)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_mdp *hw_mdptop = phys_enc->hw_mdptop;
	struct split_pipe_cfg cfg = { 0 };

	DBG("enable %d", enable);

	cfg.en = enable;
	cfg.mode = INTF_MODE_CMD;
	cfg.intf = cmd_enc->intf_idx;
	cfg.split_flush_en = enable;

	if (hw_mdptop && hw_mdptop->ops.setup_split_pipe)
		hw_mdptop->ops.setup_split_pipe(hw_mdptop, &cfg);
}

static int sde_encoder_phys_cmd_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	int ret = 0;

	DBG("enable %d", enable);

	/* Slave encoders don't report vblank */
	if (sde_encoder_phys_cmd_is_master(phys_enc)) {
		if (enable)
			ret = sde_encoder_phys_cmd_register_pp_irq(phys_enc,
					SDE_IRQ_TYPE_PING_PONG_RD_PTR,
					&cmd_enc->pp_rd_ptr_irq_idx,
					sde_encoder_phys_cmd_pp_rd_ptr_irq,
					"pp_rd_ptr");
		else
			ret = sde_encoder_phys_cmd_unregister_pp_irq(phys_enc,
					cmd_enc->pp_rd_ptr_irq_idx);
	}

	if (ret)
		DRM_ERROR("control vblank irq error %d, enable %d\n", ret,
				enable);

	return ret;
}

static void sde_encoder_phys_cmd_enable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);
	struct sde_hw_ctl *ctl = phys_enc->hw_ctl;
	u32 flush_mask;
	int ret = 0;

	if (WARN_ON(phys_enc->enable_state == SDE_ENC_ENABLED))
		return;

	DBG("intf %d, pp %d", cmd_enc->intf_idx, cmd_enc->hw_pp->idx);

	/*
	 * Only master configures master/slave configuration, so no slave check
	 * In solo configuration, solo encoder needs to program no-split
	 */
	if (phys_enc->split_role == ENC_ROLE_MASTER)
		sde_encoder_phys_cmd_split_config(phys_enc, true);
	else if (phys_enc->split_role == ENC_ROLE_SOLO)
		sde_encoder_phys_cmd_split_config(phys_enc, false);

	sde_encoder_phys_cmd_pingpong_config(phys_enc);

	/* Both master and slave need to register for pp_tx_done */
	ret = sde_encoder_phys_cmd_register_pp_irq(phys_enc,
			SDE_IRQ_TYPE_PING_PONG_COMP,
			&cmd_enc->pp_tx_done_irq_idx,
			sde_encoder_phys_cmd_pp_tx_done_irq,
			"pp_tx_done");

	if (ret)
		return;

	ret = sde_encoder_phys_cmd_control_vblank_irq(phys_enc, true);
	if (ret) {
		sde_encoder_phys_cmd_unregister_pp_irq(phys_enc,
				cmd_enc->pp_tx_done_irq_idx);
		return;
	}

	ctl->ops.get_bitmask_intf(ctl, &flush_mask, cmd_enc->intf_idx);
	ctl->ops.update_pending_flush(ctl, flush_mask);
	phys_enc->enable_state = SDE_ENC_ENABLED;

	DBG("Update pending flush CTL_ID %d flush_mask %x, INTF %d",
			ctl->idx, flush_mask, cmd_enc->intf_idx);
}

static void sde_encoder_phys_cmd_disable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	DBG("intf %d, pp %d", cmd_enc->intf_idx, cmd_enc->hw_pp->idx);

	if (WARN_ON(phys_enc->enable_state == SDE_ENC_DISABLED))
		return;

	sde_encoder_phys_cmd_unregister_pp_irq(phys_enc,
			cmd_enc->pp_tx_done_irq_idx);
	sde_encoder_phys_cmd_control_vblank_irq(phys_enc, false);

	atomic_set(&cmd_enc->pending_cnt, 0);
	wake_up_all(&cmd_enc->pp_tx_done_wq);
	phys_enc->enable_state = SDE_ENC_DISABLED;
}

static void sde_encoder_phys_cmd_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	kfree(cmd_enc);
}

static void sde_encoder_phys_cmd_get_hw_resources(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_phys_cmd *cmd_enc =
		to_sde_encoder_phys_cmd(phys_enc);

	DBG("intf %d", cmd_enc->intf_idx);
	hw_res->intfs[cmd_enc->intf_idx - INTF_0] = INTF_MODE_CMD;
}

static int sde_encoder_phys_cmd_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc)
{
	/*
	 * Since ctl_start "commits" the transaction to hardware, and the
	 * tearcheck block takes it from there, there is no need to have a
	 * separate wait for committed, a la wait-for-vsync in video mode
	 */

	return 0;
}

static void sde_encoder_phys_cmd_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		bool *need_to_wait)
{
	struct sde_encoder_phys_cmd *cmd_enc =
			to_sde_encoder_phys_cmd(phys_enc);
	int new_pending_cnt;

	DBG("intf %d, pp %d", cmd_enc->intf_idx, cmd_enc->hw_pp->idx);

	/*
	 * Mark kickoff request as outstanding. If there are more than one,
	 * outstanding, then we have to wait for the previous one to complete
	 */
	new_pending_cnt = atomic_inc_return(&cmd_enc->pending_cnt);
	*need_to_wait = new_pending_cnt != 1;

	if (*need_to_wait)
		SDE_DEBUG("intf %d pp %d needs to wait, new_pending_cnt %d",
				cmd_enc->intf_idx, cmd_enc->hw_pp->idx,
				new_pending_cnt);
	MSM_EVT(DEV(phys_enc), cmd_enc->hw_pp->idx, new_pending_cnt);
}

static bool sde_encoder_phys_cmd_needs_ctl_start(
		struct sde_encoder_phys *phys_enc)
{
	return true;
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
	ops->needs_ctl_start = sde_encoder_phys_cmd_needs_ctl_start;
}

struct sde_encoder_phys *sde_encoder_phys_cmd_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc = NULL;
	struct sde_encoder_phys_cmd *cmd_enc = NULL;
	struct sde_hw_mdp *hw_mdp;
	int ret = 0;

	DBG("intf %d", p->intf_idx);

	cmd_enc = kzalloc(sizeof(*cmd_enc), GFP_KERNEL);
	if (!cmd_enc) {
		ret = -ENOMEM;
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

	cmd_enc->intf_idx = p->intf_idx;

	sde_encoder_phys_cmd_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_CMD;
	spin_lock_init(&phys_enc->spin_lock);
	phys_enc->mode_3d = BLEND_3D_NONE;
	cmd_enc->stream_sel = 0;
	phys_enc->enable_state = SDE_ENC_DISABLED;
	atomic_set(&cmd_enc->pending_cnt, 0);

	init_waitqueue_head(&cmd_enc->pp_tx_done_wq);

	DBG("Created sde_encoder_phys_cmd for intf %d", cmd_enc->intf_idx);

	return phys_enc;

fail_mdp_init:
	kfree(cmd_enc);
fail:
	return ERR_PTR(ret);
}
