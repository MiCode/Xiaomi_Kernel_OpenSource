/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

#include "msm_drv.h"
#include "sde_kms.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "sde_encoder_phys.h"
#include "sde_mdp_formats.h"


#define to_sde_encoder_phys_vid(x) \
	container_of(x, struct sde_encoder_phys_vid, base)

static bool sde_encoder_phys_vid_mode_fixup(struct sde_encoder_phys *drm_enc,
					    const struct drm_display_mode *mode,
					    struct drm_display_mode
					    *adjusted_mode)
{
	DBG("");
	return true;
}

static void sde_encoder_phys_vid_mode_set(struct sde_encoder_phys *phys_enc,
					  struct drm_display_mode *mode,
					  struct drm_display_mode
					  *adjusted_mode)
{
	mode = adjusted_mode;
	phys_enc->cached_mode = *adjusted_mode;

	DBG("set mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
	    mode->base.id, mode->name, mode->vrefresh, mode->clock,
	    mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
	    mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
	    mode->type, mode->flags);
}

static void sde_encoder_phys_vid_setup_timing_engine(struct sde_encoder_phys
						     *phys_enc)
{
	struct drm_display_mode *mode = &phys_enc->cached_mode;
	struct intf_timing_params p = { 0 };
	uint32_t hsync_polarity = 0;
	uint32_t vsync_polarity = 0;
	struct sde_mdp_format_params *sde_fmt_params = NULL;
	u32 fmt_fourcc = DRM_FORMAT_RGB888;
	u32 fmt_mod = 0;
	unsigned long lock_flags;
	struct sde_hw_intf_cfg intf_cfg = {0};

	DBG("enable mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
	    mode->base.id, mode->name, mode->vrefresh, mode->clock,
	    mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
	    mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
	    mode->type, mode->flags);

	/* DSI controller cannot handle active-low sync signals. */
	if (phys_enc->hw_intf->cap->type != INTF_DSI) {
		if (mode->flags & DRM_MODE_FLAG_NHSYNC)
			hsync_polarity = 1;
		if (mode->flags & DRM_MODE_FLAG_NVSYNC)
			vsync_polarity = 1;
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

	/*
	 * https://www.kernel.org/doc/htmldocs/drm/ch02s05.html
	 *  Active Region            Front Porch       Sync        Back Porch
	 * <---------------------><----------------><---------><-------------->
	 * <--- [hv]display ----->
	 * <----------- [hv]sync_start ------------>
	 * <------------------- [hv]sync_end ----------------->
	 * <------------------------------ [hv]total ------------------------->
	 */

	sde_fmt_params = sde_mdp_get_format_params(fmt_fourcc, fmt_mod);

	p.width = mode->hdisplay;	/* active width */
	p.height = mode->vdisplay;	/* active height */
	p.xres = p.width;		/* Display panel width */
	p.yres = p.height;		/* Display panel height */
	p.h_back_porch = mode->htotal - mode->hsync_end;
	p.h_front_porch = mode->hsync_start - mode->hdisplay;
	p.v_back_porch = mode->vtotal - mode->vsync_end;
	p.v_front_porch = mode->vsync_start - mode->vdisplay;
	p.hsync_pulse_width = mode->hsync_end - mode->hsync_start;
	p.vsync_pulse_width = mode->vsync_end - mode->vsync_start;
	p.hsync_polarity = hsync_polarity;
	p.vsync_polarity = vsync_polarity;
	p.border_clr = 0;
	p.underflow_clr = 0xff;
	p.hsync_skew = mode->hskew;

	intf_cfg.intf = phys_enc->hw_intf->idx;
	intf_cfg.wb = SDE_NONE;

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	phys_enc->hw_intf->ops.setup_timing_gen(phys_enc->hw_intf, &p,
						sde_fmt_params);
	phys_enc->hw_ctl->ops.setup_intf_cfg(phys_enc->hw_ctl, &intf_cfg);
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);
}

static void sde_encoder_phys_vid_wait_for_vblank(struct sde_encoder_phys_vid
						 *vid_enc)
{
	DBG("");
	mdp_irq_wait(vid_enc->base.mdp_kms, vid_enc->vblank_irq.irqmask);
}

static void sde_encoder_phys_vid_vblank_irq(struct mdp_irq *irq,
					    uint32_t irqstatus)
{
	struct sde_encoder_phys_vid *vid_enc =
	    container_of(irq, struct sde_encoder_phys_vid,
			 vblank_irq);
	struct sde_encoder_phys *phys_enc = &vid_enc->base;
	struct intf_status status = { 0 };

	phys_enc->hw_intf->ops.get_status(phys_enc->hw_intf, &status);
	phys_enc->parent_ops.handle_vblank_virt(phys_enc->parent);
}

static void sde_encoder_phys_vid_enable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
	    to_sde_encoder_phys_vid(phys_enc);
	unsigned long lock_flags;

	DBG("");

	if (WARN_ON(phys_enc->enabled))
		return;

	sde_encoder_phys_vid_setup_timing_engine(phys_enc);

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	phys_enc->hw_intf->ops.enable_timing(phys_enc->hw_intf, 1);
	spin_unlock_irqrestore(&phys_enc->spin_lock, lock_flags);

	phys_enc->enabled = true;

	mdp_irq_register(phys_enc->mdp_kms, &vid_enc->vblank_irq);
	DBG("Registered IRQ for intf %d mask 0x%X", phys_enc->hw_intf->idx,
	    vid_enc->vblank_irq.irqmask);
}

static void sde_encoder_phys_vid_disable(struct sde_encoder_phys *phys_enc)
{
	struct sde_encoder_phys_vid *vid_enc =
	    to_sde_encoder_phys_vid(phys_enc);
	unsigned long lock_flags;

	DBG("");

	if (WARN_ON(!phys_enc->enabled))
		return;

	spin_lock_irqsave(&phys_enc->spin_lock, lock_flags);
	phys_enc->hw_intf->ops.enable_timing(phys_enc->hw_intf, 0);
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
	mdp_irq_unregister(phys_enc->mdp_kms, &vid_enc->vblank_irq);
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

static void sde_encoder_phys_vid_get_hw_resources(struct sde_encoder_phys
						  *phys_enc, struct
						  sde_encoder_hw_resources
						  *hw_res)
{
	DBG("");
	hw_res->intfs[phys_enc->hw_intf->idx] = true;
}

static void sde_encoder_phys_vid_init_cbs(struct sde_encoder_phys_ops *ops)
{
	ops->mode_set = sde_encoder_phys_vid_mode_set;
	ops->mode_fixup = sde_encoder_phys_vid_mode_fixup;
	ops->enable = sde_encoder_phys_vid_enable;
	ops->disable = sde_encoder_phys_vid_disable;
	ops->destroy = sde_encoder_phys_vid_destroy;
	ops->get_hw_resources = sde_encoder_phys_vid_get_hw_resources;
}

struct sde_encoder_phys *sde_encoder_phys_vid_init(struct sde_kms *sde_kms,
					      enum sde_intf intf_idx,
						  enum sde_ctl ctl_idx,
					      struct drm_encoder *parent,
					      struct sde_encoder_virt_ops
					      parent_ops)
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
	phys_enc = &vid_enc->base;

	phys_enc->hw_intf =
	    sde_hw_intf_init(intf_idx, sde_kms->mmio, sde_kms->catalog);
	if (!phys_enc->hw_intf) {
		ret = -ENOMEM;
		goto fail;
	}

	phys_enc->hw_ctl = sde_hw_ctl_init(ctl_idx, sde_kms->mmio,
					    sde_kms->catalog);
	if (!phys_enc->hw_ctl) {
		ret = -ENOMEM;
		goto fail;
	}

	sde_encoder_phys_vid_init_cbs(&phys_enc->phys_ops);
	phys_enc->parent = parent;
	phys_enc->parent_ops = parent_ops;
	phys_enc->mdp_kms = &sde_kms->base;
	vid_enc->vblank_irq.irq = sde_encoder_phys_vid_vblank_irq;
	vid_enc->vblank_irq.irqmask = 0x8000000;
	spin_lock_init(&phys_enc->spin_lock);

	DBG("Created sde_encoder_phys_vid for intf %d", phys_enc->hw_intf->idx);

	return phys_enc;

fail:
	DRM_ERROR("Failed to create encoder\n");
	if (vid_enc)
		sde_encoder_phys_vid_destroy(phys_enc);

	return ERR_PTR(ret);
}
