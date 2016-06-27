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

#include "msm_drv.h"
#include "sde_kms.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "sde_encoder_phys.h"
#include "sde_mdp_formats.h"
#include "sde_hw_mdp_top.h"

#define VBLANK_TIMEOUT msecs_to_jiffies(100)

#define to_sde_encoder_phys_vid(x) \
	container_of(x, struct sde_encoder_phys_vid, base)

static bool sde_encoder_phys_vid_is_master(
		struct sde_encoder_phys *phys_enc)
{
	bool ret = true;

	return ret;
}

static void sde_encoder_phys_vid_wait_for_vblank(
		struct sde_encoder_phys_vid *vid_enc)
{
	int rc = 0;

	DBG("");
	rc = wait_for_completion_timeout(&vid_enc->vblank_complete,
			VBLANK_TIMEOUT);
	if (rc == 0)
		DRM_ERROR("Timed out waiting for vblank irq\n");
}

static void drm_mode_to_intf_timing_params(
		const struct sde_encoder_phys *phys_enc,
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
	if (phys_enc->hw_intf->cap->type == INTF_DSI) {
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
		struct sde_encoder_phys *phys_enc,
		const struct intf_timing_params *timing)
{
	u32 worst_case_needed_lines =
	    phys_enc->hw_intf->cap->prog_fetch_lines_worst_case;
	u32 start_of_frame_lines =
	    timing->v_back_porch + timing->vsync_pulse_width;
	u32 needed_vfp_lines = worst_case_needed_lines - start_of_frame_lines;
	u32 actual_vfp_lines = 0;

	/* Fetch must be outside active lines, otherwise undefined. */

	if (start_of_frame_lines >= worst_case_needed_lines) {
		DBG("Programmable fetch is not needed due to large vbp+vsw");
		actual_vfp_lines = 0;
	} else if (timing->v_front_porch < needed_vfp_lines) {
		/* Warn fetch needed, but not enough porch in panel config */
		pr_warn_once
		    ("low vbp+vfp may lead to perf issues in some cases\n");
		DBG("Less vfp than fetch requires, using entire vfp");
		actual_vfp_lines = timing->v_front_porch;
	} else {
		DBG("Room in vfp for needed prefetch");
		actual_vfp_lines = needed_vfp_lines;
	}

	DBG("v_front_porch %u v_back_porch %u vsync_pulse_width %u",
	    timing->v_front_porch, timing->v_back_porch,
	    timing->vsync_pulse_width);
	DBG("wc_lines %u needed_vfp_lines %u actual_vfp_lines %u",
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
	struct intf_prog_fetch f = { 0 };
	u32 vfp_fetch_lines = 0;
	u32 horiz_total = 0;
	u32 vert_total = 0;
	u32 vfp_fetch_start_vsync_counter = 0;
	unsigned long lock_flags;

	if (WARN_ON_ONCE(!phys_enc->hw_intf->ops.setup_prg_fetch))
		return;

	vfp_fetch_lines = programmable_fetch_get_num_lines(phys_enc, timing);
	if (vfp_fetch_lines) {
		vert_total = get_vertical_total(timing);
		horiz_total = get_horizontal_total(timing);
		vfp_fetch_start_vsync_counter =
		    (vert_total - vfp_fetch_lines) * horiz_total + 1;
		f.enable = 1;
		f.fetch_start = vfp_fetch_start_vsync_counter;
	}

	DBG("vfp_fetch_lines %u vfp_fetch_start_vsync_counter %u",
	    vfp_fetch_lines, vfp_fetch_start_vsync_counter);

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	phys_enc->hw_intf->ops.setup_prg_fetch(phys_enc->hw_intf, &f);
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);
}

static bool sde_encoder_phys_vid_mode_fixup(
		struct sde_encoder_phys *phys_enc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	DBG("");

	/*
	 * Modifying mode has consequences when the mode comes back to us
	 */
	return true;
}

static void sde_encoder_phys_vid_flush_intf(struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_intf *intf = phys_enc->hw_intf;
	struct sde_hw_ctl *ctl = phys_enc->hw_ctl;
	u32 flush_mask = 0;

	DBG("");

	ctl->ops.get_bitmask_intf(ctl, &flush_mask, intf->idx);
	ctl->ops.setup_flush(ctl, flush_mask);

	DBG("Flushing CTL_ID %d, flush_mask %x, INTF %d",
			ctl->idx, flush_mask, intf->idx);
}

static void sde_encoder_phys_vid_mode_set(struct sde_encoder_phys *phys_enc,
					  struct drm_display_mode *mode,
					  struct drm_display_mode
					  *adjusted_mode,
					  bool splitmode)
{
	mode = adjusted_mode;
	phys_enc->cached_mode = *adjusted_mode;
	if (splitmode) {
		phys_enc->cached_mode.hdisplay >>= 1;
		phys_enc->cached_mode.htotal >>= 1;
		phys_enc->cached_mode.hsync_start >>= 1;
		phys_enc->cached_mode.hsync_end >>= 1;
	}

	DBG("set mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
	    mode->base.id, mode->name, mode->vrefresh, mode->clock,
	    mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
	    mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
	    mode->type, mode->flags);
}

static void sde_encoder_phys_vid_setup_timing_engine(
		struct sde_encoder_phys *phys_enc)
{
	struct drm_display_mode *mode = &phys_enc->cached_mode;
	struct intf_timing_params p = { 0 };
	struct sde_mdp_format_params *sde_fmt_params = NULL;
	u32 fmt_fourcc = DRM_FORMAT_RGB888;
	u32 fmt_mod = 0;
	unsigned long lock_flags;
	struct sde_hw_intf_cfg intf_cfg = { 0 };

	if (WARN_ON(!phys_enc->hw_intf->ops.setup_timing_gen))
		return;

	if (WARN_ON(!phys_enc->hw_ctl->ops.setup_intf_cfg))
		return;

	DBG("enable mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
	    mode->base.id, mode->name, mode->vrefresh, mode->clock,
	    mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
	    mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
	    mode->type, mode->flags);

	drm_mode_to_intf_timing_params(phys_enc, mode, &p);

	sde_fmt_params = sde_mdp_get_format_params(fmt_fourcc, fmt_mod);

	intf_cfg.intf = phys_enc->hw_intf->idx;
	intf_cfg.wb = SDE_NONE;

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	phys_enc->hw_intf->ops.setup_timing_gen(phys_enc->hw_intf, &p,
			sde_fmt_params);
	phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl, &intf_cfg);
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);

	programmable_fetch_config(phys_enc, &p);
}

static void sde_encoder_phys_vid_vblank_irq(void *arg, int irq_idx)
{
	struct sde_encoder_phys_vid *vid_enc = arg;
	struct sde_encoder_phys *phys_enc = &vid_enc->base;

	phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent);

	/* signal VBLANK completion */
	complete_all(&vid_enc->vblank_complete);
}

static int sde_encoder_phys_vid_register_irq(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
	    to_sde_encoder_phys_vid(phys_enc);
	struct sde_irq_callback irq_cb;
	int ret = 0;

	vid_enc->irq_idx = sde_irq_idx_lookup(phys_enc->sde_kms,
			SDE_IRQ_TYPE_INTF_VSYNC, phys_enc->hw_intf->idx);
	if (vid_enc->irq_idx < 0) {
		DRM_ERROR(
			"Failed to lookup IRQ index for INTF_VSYNC with intf=%d\n",
			phys_enc->hw_intf->idx);
		return -EINVAL;
	}

	irq_cb.func = sde_encoder_phys_vid_vblank_irq;
	irq_cb.arg = vid_enc;
	ret = sde_register_irq_callback(phys_enc->sde_kms, vid_enc->irq_idx,
			&irq_cb);
	if (ret) {
		DRM_ERROR("Failed to register IRQ callback INTF_VSYNC\n");
		return ret;
	}

	ret = sde_enable_irq(phys_enc->sde_kms, &vid_enc->irq_idx, 1);
	if (ret) {
		DRM_ERROR(
			"Failed to enable IRQ for INTF_VSYNC, intf %d, irq_idx=%d\n",
				phys_enc->hw_intf->idx,
				vid_enc->irq_idx);
		vid_enc->irq_idx = -EINVAL;

		/* Unregister callback on IRQ enable failure */
		sde_register_irq_callback(phys_enc->sde_kms, vid_enc->irq_idx,
				NULL);
		return ret;
	}

	DBG("Registered IRQ for intf %d, irq_idx=%d\n",
			phys_enc->hw_intf->idx,
			vid_enc->irq_idx);

	return ret;
}

static int sde_encoder_phys_vid_unregister_irq(
		struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	sde_register_irq_callback(phys_enc->sde_kms, vid_enc->irq_idx, NULL);
	sde_disable_irq(phys_enc->sde_kms, &vid_enc->irq_idx, 1);

	DBG("Un-Register IRQ for intf %d, irq_idx=%d\n",
			phys_enc->hw_intf->idx,
			vid_enc->irq_idx);

	return 0;
}

static void sde_encoder_phys_vid_enable(struct sde_encoder_phys *phys_enc)
{
	int ret = 0;

	DBG("");

	if (WARN_ON(!phys_enc->hw_intf->ops.enable_timing))
		return;

	sde_encoder_phys_vid_setup_timing_engine(phys_enc);

	sde_encoder_phys_vid_flush_intf(phys_enc);

	/* Register for interrupt unless we're the slave encoder */
	if (sde_encoder_phys_vid_is_master(phys_enc))
		ret = sde_encoder_phys_vid_register_irq(phys_enc);

	if (!ret && !phys_enc->enabled) {
		unsigned long lock_flags = 0;

		/* Now enable timing engine */
		spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
		phys_enc->hw_intf->ops.enable_timing(phys_enc->hw_intf, 1);
		spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);

		phys_enc->enabled = true;
	}
}

static void sde_encoder_phys_vid_disable(struct sde_encoder_phys *phys_enc)
{
	unsigned long lock_flags;
	struct sde_encoder_phys_vid *vid_enc =
			to_sde_encoder_phys_vid(phys_enc);

	DBG("");

	if (WARN_ON(!phys_enc->enabled))
		return;

	if (WARN_ON(!phys_enc->hw_intf->ops.enable_timing))
		return;

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	phys_enc->hw_intf->ops.enable_timing(phys_enc->hw_intf, 0);
	reinit_completion(&vid_enc->vblank_complete);
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);

	/*
	 * Wait for a vsync so we know the ENABLE=0 latched before
	 * the (connector) source of the vsync's gets disabled,
	 * otherwise we end up in a funny state if we re-enable
	 * before the disable latches, which results that some of
	 * the settings changes for the new modeset (like new
	 * scanout buffer) don't latch properly..
	 */
	sde_encoder_phys_vid_wait_for_vblank(vid_enc);
	sde_encoder_phys_vid_unregister_irq(phys_enc);
	phys_enc->enabled = false;
}

static void sde_encoder_phys_vid_destroy(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
	    to_sde_encoder_phys_vid(phys_enc);
	DBG("");
	kfree(phys_enc->hw_intf);
	kfree(vid_enc);
}

static void sde_encoder_phys_vid_get_hw_resources(
		struct sde_encoder_phys *phys_enc,
		struct sde_encoder_hw_resources *hw_res)
{
	struct msm_drm_private *priv = phys_enc->parent->dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	const struct sde_hw_res_map *hw_res_map;

	DBG("Intf %d\n", phys_enc->hw_intf->idx);

	hw_res->intfs[phys_enc->hw_intf->idx] = INTF_MODE_VIDEO;
	/*
	 * defaults should not be in use,
	 * otherwise signal/return failure
	 */
	hw_res_map = sde_rm_get_res_map(sde_kms, phys_enc->hw_intf->idx);

	/* This is video mode panel so PINGPONG will be in by-pass mode
	 * only assign ctl path.For cmd panel check if pp_split is
	 * enabled, override default map
	 */
	hw_res->ctls[hw_res_map->ctl] = true;
}

/**
  * video mode will use the intf (get_status)
  * cmd mode will use the pingpong (get_vsync_info)
  * to get this information
  */
static void sde_encoder_intf_get_vsync_info(struct sde_encoder_phys *phys_enc,
		struct vsync_info *vsync)
{
	struct intf_status status;

	DBG("");
	phys_enc->hw_intf->ops.get_status(phys_enc->hw_intf, &status);
	vsync->frame_count = status.frame_count;
	vsync->line_count = status.line_count;
	DBG(" sde_encoder_intf_get_vsync_info, count  %d", vsync->frame_count);
}

static void sde_encoder_intf_split_config(struct sde_encoder_phys *phys_enc,
		bool enable)
{
	struct msm_drm_private *priv = phys_enc->parent->dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct sde_hw_mdp *mdp = sde_hw_mdptop_init(MDP_TOP, sde_kms->mmio,
			sde_kms->catalog);
	struct split_pipe_cfg cfg;

	DBG("%p", mdp);
	cfg.en = true;
	cfg.mode = INTF_MODE_VIDEO;
	if (!IS_ERR_OR_NULL(mdp))
		mdp->ops.setup_split_pipe(mdp, &cfg);
}

static void sde_encoder_phys_vid_init_cbs(struct sde_encoder_phys_ops *ops)
{
	ops->mode_set = sde_encoder_phys_vid_mode_set;
	ops->mode_fixup = sde_encoder_phys_vid_mode_fixup;
	ops->enable = sde_encoder_phys_vid_enable;
	ops->disable = sde_encoder_phys_vid_disable;
	ops->destroy = sde_encoder_phys_vid_destroy;
	ops->get_hw_resources = sde_encoder_phys_vid_get_hw_resources;
	ops->get_vsync_info = sde_encoder_intf_get_vsync_info;
	ops->enable_split_config = sde_encoder_intf_split_config;
}

struct sde_encoder_phys *sde_encoder_phys_vid_init(
		struct sde_kms *sde_kms,
		enum sde_intf intf_idx,
		enum sde_ctl ctl_idx,
		struct drm_encoder *parent,
		struct sde_encoder_virt_ops parent_ops)
{
	struct sde_encoder_phys *phys_enc = NULL;
	struct sde_encoder_phys_vid *vid_enc = NULL;
	int ret = 0;

	DBG("");

	vid_enc = kzalloc(sizeof(*vid_enc), GFP_KERNEL);
	if (!vid_enc) {
		ret = -ENOMEM;
		goto fail;
	}
	vid_enc->irq_idx = -EINVAL;
	init_completion(&vid_enc->vblank_complete);

	phys_enc = &vid_enc->base;

	phys_enc->hw_intf =
	    sde_hw_intf_init(intf_idx, sde_kms->mmio, sde_kms->catalog);
	if (!phys_enc->hw_intf) {
		ret = -ENOMEM;
		goto fail;
	}

	phys_enc->hw_ctl = sde_rm_acquire_ctl_path(sde_kms, ctl_idx);
	if (!phys_enc->hw_ctl) {
		ret = -ENOMEM;
		goto fail;
	}

	sde_encoder_phys_vid_init_cbs(&phys_enc->phys_ops);
	phys_enc->parent = parent;
	phys_enc->parent_ops = parent_ops;
	phys_enc->sde_kms = sde_kms;
	spin_lock_init(&phys_enc->spin_lock);

	DBG("Created sde_encoder_phys_vid for intf %d", phys_enc->hw_intf->idx);

	return phys_enc;

fail:
	DRM_ERROR("Failed to create encoder\n");
	if (vid_enc)
		sde_encoder_phys_vid_destroy(phys_enc);

	return ERR_PTR(ret);
}
