/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/jiffies.h>

#include "sde_encoder_phys.h"
#include "sde_hw_interrupts.h"
#include "sde_core_irq.h"
#include "sde_formats.h"

#define SDE_DEBUG_VIDENC(e, fmt, ...) SDE_DEBUG("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) && (e)->hw_intf ? \
		(e)->hw_intf->idx - INTF_0 : -1, ##__VA_ARGS__)

#define SDE_ERROR_VIDENC(e, fmt, ...) SDE_ERROR("enc%d intf%d " fmt, \
		(e) && (e)->base.parent ? \
		(e)->base.parent->base.id : -1, \
		(e) && (e)->hw_intf ? \
		(e)->hw_intf->idx - INTF_0 : -1, ##__VA_ARGS__)

#define VBLANK_TIMEOUT msecs_to_jiffies(100)

#define to_sde_encoder_phys_vid(x) \
	container_of(x, struct sde_encoder_phys_vid, base)

#define DEV(phy_enc) (phy_enc->parent->dev)

#define WAIT_TIMEOUT_MSEC 100

static bool sde_encoder_phys_vid_is_master(
		struct sde_encoder_phys *phys_enc)
{
	bool ret = false;

	if (phys_enc->split_role != ENC_ROLE_SLAVE)
		ret = true;

	return ret;
}

static void sde_encoder_phys_vid_wait_for_vblank(
		struct sde_encoder_phys_vid *vid_enc)
{
	int rc = 0;

	if (!vid_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_VIDENC(vid_enc, "\n");
	rc = wait_for_completion_timeout(&vid_enc->vblank_completion,
			VBLANK_TIMEOUT);
	if (rc == 0)
		SDE_ERROR_VIDENC(vid_enc, "timed out waiting for vblank irq\n");
}

static void drm_mode_to_intf_timing_params(
		const struct sde_encoder_phys_vid *vid_enc,
		const struct drm_display_mode *mode,
		struct intf_timing_params *timing)
{
	memset(timing, 0, sizeof(*timing));
	/*
	 * https://www.kernel.org/doc/htmldocs/drm/ch02s05.html
	 *  Active Region      Front Porch   Sync   Back Porch
	 * <-----------------><------------><-----><----------->
	 * <- [hv]display --->
	 * <--------- [hv]sync_start ------>
	 * <----------------- [hv]sync_end ------->
	 * <---------------------------- [hv]total ------------->
	 */
	timing->width = mode->hdisplay;	/* active width */
	timing->height = mode->vdisplay;	/* active height */
	timing->xres = timing->width;
	timing->yres = timing->height;
	timing->h_back_porch = mode->htotal - mode->hsync_end;
	timing->h_front_porch = mode->hsync_start - mode->hdisplay;
	timing->v_back_porch = mode->vtotal - mode->vsync_end;
	timing->v_front_porch = mode->vsync_start - mode->vdisplay;
	timing->hsync_pulse_width = mode->hsync_end - mode->hsync_start;
	timing->vsync_pulse_width = mode->vsync_end - mode->vsync_start;
	timing->hsync_polarity = (mode->flags & DRM_MODE_FLAG_NHSYNC) ? 1 : 0;
	timing->vsync_polarity = (mode->flags & DRM_MODE_FLAG_NVSYNC) ? 1 : 0;
	timing->border_clr = 0;
	timing->underflow_clr = 0xff;
	timing->hsync_skew = mode->hskew;

	/* DSI controller cannot handle active-low sync signals. */
	if (vid_enc->hw_intf->cap->type == INTF_DSI) {
		timing->hsync_polarity = 0;
		timing->vsync_polarity = 0;
	}

	/*
	 * For edp only:
	 * DISPLAY_V_START = (VBP * HCYCLE) + HBP
	 * DISPLAY_V_END = (VBP + VACTIVE) * HCYCLE - 1 - HFP
	 */
	/*
	 * if (vid_enc->hw->cap->type == INTF_EDP) {
	 * display_v_start += mode->htotal - mode->hsync_start;
	 * display_v_end -= mode->hsync_start - mode->hdisplay;
	 * }
	 */
}

static inline u32 get_horizontal_total(const struct intf_timing_params *timing)
{
	u32 active = timing->xres;
	u32 inactive =
	    timing->h_back_porch + timing->h_front_porch +
	    timing->hsync_pulse_width;
	return active + inactive;
}

static inline u32 get_vertical_total(const struct intf_timing_params *timing)
{
	u32 active = timing->yres;
	u32 inactive =
	    timing->v_back_porch + timing->v_front_porch +
	    timing->vsync_pulse_width;
	return active + inactive;
}

/*
 * programmable_fetch_get_num_lines:
 *	Number of fetch lines in vertical front porch
 * @timing: Pointer to the intf timing information for the requested mode
 *
 * Returns the number of fetch lines in vertical front porch at which mdp
 * can start fetching the next frame.
 *
 * Number of needed prefetch lines is anything that cannot be absorbed in the
 * start of frame time (back porch + vsync pulse width).
 *
 * Some panels have very large VFP, however we only need a total number of
 * lines based on the chip worst case latencies.
 */
static u32 programmable_fetch_get_num_lines(
		struct sde_encoder_phys_vid *vid_enc,
		const struct intf_timing_params *timing)
{
	u32 worst_case_needed_lines =
	    vid_enc->hw_intf->cap->prog_fetch_lines_worst_case;
	u32 start_of_frame_lines =
	    timing->v_back_porch + timing->vsync_pulse_width;
	u32 needed_vfp_lines = worst_case_needed_lines - start_of_frame_lines;
	u32 actual_vfp_lines = 0;

	/* Fetch must be outside active lines, otherwise undefined. */
	if (start_of_frame_lines >= worst_case_needed_lines) {
		SDE_DEBUG_VIDENC(vid_enc,
				"prog fetch is not needed, large vbp+vsw\n");
		actual_vfp_lines = 0;
	} else if (timing->v_front_porch < needed_vfp_lines) {
		/* Warn fetch needed, but not enough porch in panel config */
		pr_warn_once
			("low vbp+vfp may lead to perf issues in some cases\n");
		SDE_DEBUG_VIDENC(vid_enc,
				"less vfp than fetch req, using entire vfp\n");
		actual_vfp_lines = timing->v_front_porch;
	} else {
		SDE_DEBUG_VIDENC(vid_enc, "room in vfp for needed prefetch\n");
		actual_vfp_lines = needed_vfp_lines;
	}

	SDE_DEBUG_VIDENC(vid_enc,
		"v_front_porch %u v_back_porch %u vsync_pulse_width %u\n",
		timing->v_front_porch, timing->v_back_porch,
		timing->vsync_pulse_width);
	SDE_DEBUG_VIDENC(vid_enc,
		"wc_lines %u needed_vfp_lines %u actual_vfp_lines %u\n",
		worst_case_needed_lines, needed_vfp_lines, actual_vfp_lines);

	return actual_vfp_lines;
}

/*
 * programmable_fetch_config: Programs HW to prefetch lines by offsetting
 *	the start of fetch into the vertical front porch for cases where the
 *	vsync pulse width and vertical back porch time is insufficient
 *
 *	Gets # of lines to pre-fetch, then calculate VSYNC counter value.
 *	HW layer requires VSYNC counter of first pixel of tgt VFP line.
 *
 * @timing: Pointer to the intf timing information for the requested mode
 */
static void programmable_fetch_config(struct sde_encoder_phys *phys_enc,
				      const struct intf_timing_params *timing)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	struct intf_prog_fetch f = { 0 };
	u32 vfp_fetch_lines = 0;
	u32 horiz_total = 0;
	u32 vert_total = 0;
	u32 vfp_fetch_start_vsync_counter = 0;
	unsigned long lock_flags;

	if (WARN_ON_ONCE(!vid_enc->hw_intf->ops.setup_prg_fetch))
		return;

	vfp_fetch_lines = programmable_fetch_get_num_lines(vid_enc, timing);
	if (vfp_fetch_lines) {
		vert_total = get_vertical_total(timing);
		horiz_total = get_horizontal_total(timing);
		vfp_fetch_start_vsync_counter =
		    (vert_total - vfp_fetch_lines) * horiz_total + 1;
		f.enable = 1;
		f.fetch_start = vfp_fetch_start_vsync_counter;
	}

	SDE_DEBUG_VIDENC(vid_enc,
		"vfp_fetch_lines %u vfp_fetch_start_vsync_counter %u\n",
		vfp_fetch_lines, vfp_fetch_start_vsync_counter);

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	vid_enc->hw_intf->ops.setup_prg_fetch(vid_enc->hw_intf, &f);
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);
}

static bool sde_encoder_phys_vid_mode_fixup(
		struct sde_encoder_phys *phys_enc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	if (phys_enc)
		SDE_DEBUG_VIDENC(to_sde_encoder_phys_vid(phys_enc), "\n");

	/*
	 * Modifying mode has consequences when the mode comes back to us
	 */
	return true;
}

static void sde_encoder_phys_vid_setup_timing_engine(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	struct drm_display_mode mode = phys_enc->cached_mode;
	struct intf_timing_params timing_params = { 0 };
	const struct sde_format *fmt = NULL;
	u32 fmt_fourcc = DRM_FORMAT_RGB888;
	unsigned long lock_flags;
	struct sde_hw_intf_cfg intf_cfg = { 0 };

	if (!phys_enc ||
			!vid_enc->hw_intf->ops.setup_timing_gen ||
			!phys_enc->hw_ctl->ops.setup_intf_cfg) {
		SDE_ERROR("invalid encoder %d\n", phys_enc != 0);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "enabling mode:\n");
	drm_mode_debug_printmodeline(&mode);

	if (phys_enc->split_role != ENC_ROLE_SOLO) {
		mode.hdisplay >>= 1;
		mode.htotal >>= 1;
		mode.hsync_start >>= 1;
		mode.hsync_end >>= 1;

		SDE_DEBUG_VIDENC(vid_enc,
			"split_role %d, halve horizontal %d %d %d %d\n",
			phys_enc->split_role,
			mode.hdisplay, mode.htotal,
			mode.hsync_start, mode.hsync_end);
	}

	drm_mode_to_intf_timing_params(vid_enc, &mode, &timing_params);

	fmt = sde_get_sde_format(fmt_fourcc);
	SDE_DEBUG_VIDENC(vid_enc, "fmt_fourcc 0x%X\n", fmt_fourcc);

	intf_cfg.intf = vid_enc->hw_intf->idx;
	intf_cfg.intf_mode_sel = SDE_CTL_MODE_SEL_VID;
	intf_cfg.stream_sel = 0; /* Don't care value for video mode */
	intf_cfg.mode_3d = sde_encoder_helper_get_3d_blend_mode(phys_enc);

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	vid_enc->hw_intf->ops.setup_timing_gen(vid_enc->hw_intf,
			&timing_params, fmt);
	phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl, &intf_cfg);
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);

	programmable_fetch_config(phys_enc, &timing_params);
}

static void sde_encoder_phys_vid_vblank_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_vid *vid_enc = arg;
	struct sde_encoder_phys *phys_enc;

	if (!vid_enc)
		return;

	phys_enc = &vid_enc->base;
	phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent);

	/* signal VBLANK completion */
	complete_all(&vid_enc->vblank_completion);
}

static bool sde_encoder_phys_vid_needs_split_flush(
		struct sde_encoder_phys *phys_enc)
{
	return phys_enc && phys_enc->split_role != ENC_ROLE_SOLO;
}

static void _sde_encoder_phys_vid_split_config(
		struct sde_encoder_phys *phys_enc, bool enable)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	struct sde_hw_mdp *hw_mdptop = phys_enc->hw_mdptop;
	struct split_pipe_cfg cfg = { 0 };

	SDE_DEBUG_VIDENC(vid_enc, "enable %d\n", enable);

	cfg.en = enable;
	cfg.mode = INTF_MODE_VIDEO;
	cfg.intf = vid_enc->hw_intf->idx;
	cfg.split_flush_en = enable &&
		sde_encoder_phys_vid_needs_split_flush(phys_enc);

	/* Configure split pipe control to handle master/slave triggering */
	if (hw_mdptop && hw_mdptop->ops.setup_split_pipe) {
		unsigned long lock_flags;

		spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
		hw_mdptop->ops.setup_split_pipe(hw_mdptop, &cfg);
		spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);
	}
}

static int sde_encoder_phys_vid_register_irq(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);
	struct sde_irq_callback irq_cb;
	int ret = 0;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	vid_enc->irq_idx = sde_core_irq_idx_lookup(phys_enc->sde_kms,
			SDE_IRQ_TYPE_INTF_VSYNC, vid_enc->hw_intf->idx);
	if (vid_enc->irq_idx < 0) {
		SDE_ERROR_VIDENC(vid_enc,
				"failed to lookup IRQ index for INTF_VSYNC\n");
		return -EINVAL;
	}

	irq_cb.func = sde_encoder_phys_vid_vblank_irq;
	irq_cb.arg = vid_enc;
	ret = sde_core_irq_register_callback(phys_enc->sde_kms,
			vid_enc->irq_idx, &irq_cb);
	if (ret) {
		SDE_ERROR_VIDENC(vid_enc,
				"failed to register IRQ callback INTF_VSYNC");
		return ret;
	}

	ret = sde_core_irq_enable(phys_enc->sde_kms, &vid_enc->irq_idx, 1);
	if (ret) {
		SDE_ERROR_VIDENC(vid_enc,
			"enable IRQ for INTF_VSYNC failed, irq_idx %d\n",
				vid_enc->irq_idx);
		vid_enc->irq_idx = -EINVAL;

		/* Unregister callback on IRQ enable failure */
		sde_core_irq_register_callback(phys_enc->sde_kms,
				vid_enc->irq_idx, NULL);
		return ret;
	}

	SDE_DEBUG_VIDENC(vid_enc, "registered %d\n", vid_enc->irq_idx);

	return ret;
}

static int sde_encoder_phys_vid_unregister_irq(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	sde_core_irq_register_callback(phys_enc->sde_kms, vid_enc->irq_idx,
			NULL);
	sde_core_irq_disable(phys_enc->sde_kms, &vid_enc->irq_idx, 1);

	SDE_DEBUG_VIDENC(vid_enc, "unregistered %d\n", vid_enc->irq_idx);

	return 0;
}

static void sde_encoder_phys_vid_mode_set(
		struct sde_encoder_phys *phys_enc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adj_mode)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	struct sde_rm *rm = &phys_enc->sde_kms->rm;
	struct sde_rm_hw_iter iter;
	int i, instance;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}

	phys_enc->cached_mode = *adj_mode;
	SDE_DEBUG_VIDENC(vid_enc, "caching mode:\n");
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
		SDE_ERROR_VIDENC(vid_enc, "failed to init ctl, %ld\n",
				PTR_ERR(phys_enc->hw_ctl));
		phys_enc->hw_ctl = NULL;
		return;
	}
}

static int sde_encoder_phys_vid_control_vblank_irq(
		struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	int ret = 0;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return -EINVAL;
	}

	/* Slave encoders don't report vblank */
	if (!sde_encoder_phys_vid_is_master(phys_enc))
		return 0;

	SDE_DEBUG_VIDENC(vid_enc, "[%pS] enable=%d/%d\n",
			__builtin_return_address(0),
			enable, atomic_read(&phys_enc->vblank_refcount));

	MSM_EVTMSG(phys_enc->parent->dev, NULL, enable,
			atomic_read(&phys_enc->vblank_refcount));

	if (enable && atomic_inc_return(&phys_enc->vblank_refcount) == 1)
		ret = sde_encoder_phys_vid_register_irq(phys_enc);
	else if (!enable && atomic_dec_return(&phys_enc->vblank_refcount) == 0)
		ret = sde_encoder_phys_vid_unregister_irq(phys_enc);

	if (ret)
		SDE_ERROR_VIDENC(vid_enc,
				"control vblank irq error %d, enable %d\n",
				ret, enable);

	return ret;
}

static void sde_encoder_phys_vid_enable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);
	struct sde_hw_intf *intf = vid_enc->hw_intf;
	struct sde_hw_ctl *ctl = phys_enc->hw_ctl;
	u32 flush_mask = 0;

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	} else if (!vid_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
				vid_enc->hw_intf != 0, phys_enc->hw_ctl != 0);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	if (WARN_ON(!vid_enc->hw_intf->ops.enable_timing))
		return;

	if (phys_enc->split_role == ENC_ROLE_MASTER)
		_sde_encoder_phys_vid_split_config(phys_enc, true);
	else if (phys_enc->split_role == ENC_ROLE_SOLO)
		_sde_encoder_phys_vid_split_config(phys_enc, false);

	sde_encoder_phys_vid_setup_timing_engine(phys_enc);
	sde_encoder_phys_vid_control_vblank_irq(phys_enc, true);

	ctl->ops.get_bitmask_intf(ctl, &flush_mask, intf->idx);
	ctl->ops.update_pending_flush(ctl, flush_mask);

	SDE_DEBUG_VIDENC(vid_enc, "update pending flush ctl %d flush_mask %x\n",
		ctl->idx - CTL_0, flush_mask);

	/* ctl_flush & timing engine enable will be triggered by framework */
	if (phys_enc->enable_state == SDE_ENC_DISABLED)
		phys_enc->enable_state = SDE_ENC_ENABLING;
}

static void sde_encoder_phys_vid_disable(struct sde_encoder_phys *phys_enc)
{
	unsigned long lock_flags;
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	} else if (!vid_enc->hw_intf || !phys_enc->hw_ctl) {
		SDE_ERROR("invalid hw_intf %d hw_ctl %d\n",
				vid_enc->hw_intf != 0, phys_enc->hw_ctl != 0);
		return;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	if (WARN_ON(!vid_enc->hw_intf->ops.enable_timing))
		return;

	if (phys_enc->enable_state == SDE_ENC_DISABLED) {
		SDE_ERROR("already disabled\n");
		return;
	}

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	vid_enc->hw_intf->ops.enable_timing(vid_enc->hw_intf, 0);
	reinit_completion(&vid_enc->vblank_completion);
	phys_enc->enable_state = SDE_ENC_DISABLED;
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);

	/*
	 * Wait for a vsync so we know the ENABLE=0 latched before
	 * the (connector) source of the vsync's gets disabled,
	 * otherwise we end up in a funny state if we re-enable
	 * before the disable latches, which results that some of
	 * the settings changes for the new modeset (like new
	 * scanout buffer) don't latch properly..
	 */
	if (sde_encoder_phys_vid_is_master(phys_enc)) {
		sde_encoder_phys_vid_wait_for_vblank(vid_enc);
		sde_encoder_phys_vid_control_vblank_irq(phys_enc, false);
	}

	if (atomic_read(&phys_enc->vblank_refcount))
		SDE_ERROR("enc:%d role:%d invalid vblank refcount %d\n",
				phys_enc->parent->base.id,
				phys_enc->split_role,
				atomic_read(&phys_enc->vblank_refcount));
}

static void sde_encoder_phys_vid_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
	    to_sde_encoder_phys_vid(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_VIDENC(vid_enc, "\n");
	kfree(vid_enc);
}

static void sde_encoder_phys_vid_get_hw_resources(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state)
{
	struct sde_encoder_phys_vid *vid_enc =
		to_sde_encoder_phys_vid(phys_enc);

	if (!phys_enc || !hw_res || !vid_enc->hw_intf) {
		SDE_ERROR("invalid arg(s), enc %d hw_res %d conn_state %d\n",
				phys_enc != 0, hw_res != 0, conn_state != 0);
		return;
	}
	SDE_DEBUG_VIDENC(vid_enc, "\n");
	hw_res->intfs[vid_enc->hw_intf->idx - INTF_0] = INTF_MODE_VIDEO;
}

static int sde_encoder_phys_vid_wait_for_commit_done(
		struct sde_encoder_phys *phys_enc)
{
	unsigned long ret;
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	if (!sde_encoder_phys_vid_is_master(phys_enc))
		return 0;

	if (phys_enc->enable_state != SDE_ENC_ENABLED) {
		SDE_ERROR("encoder not enabled\n");
		return -EWOULDBLOCK;
	}

	MSM_EVTMSG(DEV(phys_enc), "waiting", 0, 0);

	ret = wait_for_completion_timeout(&vid_enc->vblank_completion,
			msecs_to_jiffies(WAIT_TIMEOUT_MSEC));
	if (!ret) {
		SDE_DEBUG_VIDENC(vid_enc, "wait %u ms timed out\n",
				WAIT_TIMEOUT_MSEC);
		MSM_EVTMSG(DEV(phys_enc), "wait_timeout", 0, 0);
		return -ETIMEDOUT;
	}

	MSM_EVTMSG(DEV(phys_enc), "wait_done", 0, 0);

	return 0;
}

static void sde_encoder_phys_vid_prepare_for_kickoff(
		struct sde_encoder_phys *phys_enc,
		bool *need_to_wait)
{
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	/* Vid encoder is simple, kickoff is immediate */
	*need_to_wait = false;

	/* Reset completion to wait for the next vblank */
	reinit_completion(&vid_enc->vblank_completion);
}

static void sde_encoder_phys_vid_handle_post_kickoff(
		struct sde_encoder_phys *phys_enc)
{
	unsigned long lock_flags;
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	if (!phys_enc) {
		SDE_ERROR("invalid encoder\n");
		return;
	}
	SDE_DEBUG_VIDENC(vid_enc, "enable_state %d\n", phys_enc->enable_state);

	/*
	 * Video mode must flush CTL before enabling timing engine
	 * Video encoders need to turn on their interfaces now
	 */
	if (phys_enc->enable_state == SDE_ENC_ENABLING) {
		MSM_EVT(DEV(phys_enc), 0, 0);
		spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
		vid_enc->hw_intf->ops.enable_timing(vid_enc->hw_intf, 1);
		spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);
		phys_enc->enable_state = SDE_ENC_ENABLED;
	}
}

static void sde_encoder_phys_vid_init_ops(struct sde_encoder_phys_ops *ops)
{
	ops->is_master = sde_encoder_phys_vid_is_master;
	ops->mode_set = sde_encoder_phys_vid_mode_set;
	ops->mode_fixup = sde_encoder_phys_vid_mode_fixup;
	ops->enable = sde_encoder_phys_vid_enable;
	ops->disable = sde_encoder_phys_vid_disable;
	ops->destroy = sde_encoder_phys_vid_destroy;
	ops->get_hw_resources = sde_encoder_phys_vid_get_hw_resources;
	ops->control_vblank_irq = sde_encoder_phys_vid_control_vblank_irq;
	ops->wait_for_commit_done = sde_encoder_phys_vid_wait_for_commit_done;
	ops->prepare_for_kickoff = sde_encoder_phys_vid_prepare_for_kickoff;
	ops->handle_post_kickoff = sde_encoder_phys_vid_handle_post_kickoff;
	ops->needs_split_flush = sde_encoder_phys_vid_needs_split_flush;
}

struct sde_encoder_phys *sde_encoder_phys_vid_init(
		struct sde_enc_phys_init_params *p)
{
	struct sde_encoder_phys *phys_enc = NULL;
	struct sde_encoder_phys_vid *vid_enc = NULL;
	struct sde_rm_hw_iter iter;
	struct sde_hw_mdp *hw_mdp;
	int ret = 0;

	if (!p) {
		ret = -EINVAL;
		goto fail;
	}

	vid_enc = kzalloc(sizeof(*vid_enc), GFP_KERNEL);
	if (!vid_enc) {
		ret = -ENOMEM;
		goto fail;
	}
	vid_enc->irq_idx = -EINVAL;
	init_completion(&vid_enc->vblank_completion);

	phys_enc = &vid_enc->base;

	hw_mdp = sde_rm_get_mdp(&p->sde_kms->rm);
	if (IS_ERR_OR_NULL(hw_mdp)) {
		ret = PTR_ERR(hw_mdp);
		SDE_ERROR("failed to get mdptop\n");
		goto fail;
	}
	phys_enc->hw_mdptop = hw_mdp;

	/**
	 * hw_intf resource permanently assigned to this encoder
	 * Other resources allocated at atomic commit time by use case
	 */
	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_INTF);
	while (sde_rm_get_hw(&p->sde_kms->rm, &iter)) {
		struct sde_hw_intf *hw_intf = (struct sde_hw_intf *)iter.hw;

		if (hw_intf->idx == p->intf_idx) {
			vid_enc->hw_intf = hw_intf;
			break;
		}
	}

	if (!vid_enc->hw_intf) {
		ret = -EINVAL;
		SDE_ERROR("failed to get hw_intf\n");
		goto fail;
	}

	SDE_DEBUG_VIDENC(vid_enc, "\n");

	sde_encoder_phys_vid_init_ops(&phys_enc->ops);
	phys_enc->parent = p->parent;
	phys_enc->parent_ops = p->parent_ops;
	phys_enc->sde_kms = p->sde_kms;
	phys_enc->split_role = p->split_role;
	phys_enc->intf_mode = INTF_MODE_VIDEO;
	spin_lock_init(&phys_enc->spin_lock);
	init_completion(&vid_enc->vblank_completion);
	atomic_set(&phys_enc->vblank_refcount, 0);
	phys_enc->enable_state = SDE_ENC_DISABLED;

	SDE_DEBUG_VIDENC(vid_enc, "created\n");

	return phys_enc;

fail:
	SDE_ERROR("failed to create encoder\n");
	if (vid_enc)
		sde_encoder_phys_vid_destroy(phys_enc);

	return ERR_PTR(ret);
}
