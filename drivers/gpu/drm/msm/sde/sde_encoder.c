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

#include "sde_kms.h"
#include "drm_crtc.h"
#include "drm_crtc_helper.h"

#include "sde_hwio.h"
#include "sde_hw_catalog.h"
#include "sde_hw_intf.h"
#include "sde_hw_mdp_ctl.h"
#include "sde_mdp_formats.h"

#include "../dsi-staging/dsi_display.h"

#define DBG(fmt, ...) DRM_DEBUG(fmt"\n", ##__VA_ARGS__)

struct sde_encoder {
	struct drm_encoder base;
	spinlock_t intf_lock;
	bool enabled;
	uint32_t bus_scaling_client;
	struct sde_hw_intf *hw_intf;
	struct sde_hw_ctl *hw_ctl;
	int drm_mode_enc;

	void (*vblank_callback)(void *);
	void *vblank_callback_data;

	struct mdp_irq vblank_irq;
};
#define to_sde_encoder(x) container_of(x, struct sde_encoder, base)

static struct sde_kms *get_kms(struct drm_encoder *drm_enc)
{
	struct msm_drm_private *priv = drm_enc->dev->dev_private;

	return to_sde_kms(to_mdp_kms(priv->kms));
}

#ifdef CONFIG_QCOM_BUS_SCALING
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#define MDP_BUS_VECTOR_ENTRY(ab_val, ib_val)		\
	{						\
		.src = MSM_BUS_MASTER_MDP_PORT0,	\
		.dst = MSM_BUS_SLAVE_EBI_CH0,		\
		.ab = (ab_val),				\
		.ib = (ib_val),				\
	}

static struct msm_bus_vectors mdp_bus_vectors[] = {
	MDP_BUS_VECTOR_ENTRY(0, 0),
	MDP_BUS_VECTOR_ENTRY(2000000000, 2000000000),
};
static struct msm_bus_paths mdp_bus_usecases[] = { {
						    .num_paths = 1,
						    .vectors =
						    &mdp_bus_vectors[0],
						    }, {
							.num_paths = 1,
							.vectors =
							&mdp_bus_vectors[1],
							}
};

static struct msm_bus_scale_pdata mdp_bus_scale_table = {
	.usecase = mdp_bus_usecases,
	.num_usecases = ARRAY_SIZE(mdp_bus_usecases),
	.name = "mdss_mdp",
};

static void bs_init(struct sde_encoder *sde_enc)
{
	sde_enc->bus_scaling_client =
	    msm_bus_scale_register_client(&mdp_bus_scale_table);
	DBG("bus scale client: %08x", sde_enc->bus_scaling_client);
}

static void bs_fini(struct sde_encoder *sde_enc)
{
	if (sde_enc->bus_scaling_client) {
		msm_bus_scale_unregister_client(sde_enc->bus_scaling_client);
		sde_enc->bus_scaling_client = 0;
	}
}

static void bs_set(struct sde_encoder *sde_enc, int idx)
{
	if (sde_enc->bus_scaling_client) {
		DBG("set bus scaling: %d", idx);
		idx = 1;
		msm_bus_scale_client_update_request(sde_enc->bus_scaling_client,
						    idx);
	}
}
#else
static void bs_init(struct sde_encoder *sde_enc)
{
}

static void bs_fini(struct sde_encoder *sde_enc)
{
}

static void bs_set(struct sde_encoder *sde_enc, int idx)
{
}
#endif

static bool sde_encoder_mode_fixup(struct drm_encoder *drm_enc,
				   const struct drm_display_mode *mode,
				   struct drm_display_mode *adjusted_mode)
{
	DBG("");
	return true;
}

static void sde_encoder_mode_set(struct drm_encoder *drm_enc,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{

	struct sde_encoder *sde_enc = to_sde_encoder(drm_enc);
	struct intf_timing_params p = {0};
	uint32_t hsync_polarity = 0, vsync_polarity = 0;
	struct sde_mdp_format_params *sde_fmt_params = NULL;
	u32 fmt_fourcc = DRM_FORMAT_RGB888, fmt_mod = 0;
	unsigned long lock_flags;
	struct sde_hw_intf_cfg intf_cfg = {0};

	mode = adjusted_mode;

	DBG("set mode: %d:\"%s\" %d %d %d %d %d %d %d %d %d %d 0x%x 0x%x",
	    mode->base.id, mode->name, mode->vrefresh, mode->clock,
	    mode->hdisplay, mode->hsync_start, mode->hsync_end, mode->htotal,
	    mode->vdisplay, mode->vsync_start, mode->vsync_end, mode->vtotal,
	    mode->type, mode->flags);

	/* DSI controller cannot handle active-low sync signals. */
	if (sde_enc->hw_intf->cap->type != INTF_DSI) {
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
	 * if (sde_enc->hw->cap->type == INTF_EDP) {
	 *	display_v_start += mode->htotal - mode->hsync_start;
	 *	display_v_end -= mode->hsync_start - mode->hdisplay;
	 * }
	 */

	/*
	 * https://www.kernel.org/doc/htmldocs/drm/ch02s05.html
	 *  Active Region            Front Porch       Sync       Back Porch
	 * <---------------------><----------------><---------><-------------->
	 * <--- [hv]display ----->
	 * <----------- [hv]sync_start ------------>
	 * <------------------- [hv]sync_end ----------------->
	 * <------------------------------ [hv]total ------------------------->
	 */

	sde_fmt_params = sde_mdp_get_format_params(fmt_fourcc, fmt_mod);

	p.width = mode->hdisplay;	/* active width */
	p.height = mode->vdisplay;	/* active height */
	p.xres = p.width;	/* Display panel width */
	p.yres = p.height;	/* Display panel height */
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

	intf_cfg.intf = sde_enc->hw_intf->idx;
	intf_cfg.wb = SDE_NONE;

	spin_lock_irqsave(&sde_enc->intf_lock, lock_flags);
	sde_enc->hw_intf->ops.setup_timing_gen(sde_enc->hw_intf, &p,
					       sde_fmt_params);
	sde_enc->hw_ctl->ops.setup_intf_cfg(sde_enc->hw_ctl, &intf_cfg);
	spin_unlock_irqrestore(&sde_enc->intf_lock, lock_flags);
}

static void sde_encoder_wait_for_vblank(struct sde_encoder *sde_enc)
{
	struct sde_kms *sde_kms = get_kms(&sde_enc->base);
	struct mdp_kms *mdp_kms = &sde_kms->base;

	DBG("");
	mdp_irq_wait(mdp_kms, sde_enc->vblank_irq.irqmask);
}

static void sde_encoder_vblank_irq(struct mdp_irq *irq, uint32_t irqstatus)
{
	struct sde_encoder *sde_enc = container_of(irq, struct sde_encoder,
						   vblank_irq);
	struct intf_status status = { 0 };
	unsigned long lock_flags;

	spin_lock_irqsave(&sde_enc->intf_lock, lock_flags);
	if (sde_enc->vblank_callback)
		sde_enc->vblank_callback(sde_enc->vblank_callback_data);
	spin_unlock_irqrestore(&sde_enc->intf_lock, lock_flags);

	sde_enc->hw_intf->ops.get_status(sde_enc->hw_intf, &status);
}

static void sde_encoder_disable(struct drm_encoder *drm_enc)
{
	struct sde_encoder *sde_enc = to_sde_encoder(drm_enc);
	struct sde_kms *sde_kms = get_kms(drm_enc);
	struct mdp_kms *mdp_kms = &(sde_kms->base);
	unsigned long lock_flags;

	DBG("");

	if (WARN_ON(!sde_enc->enabled))
		return;

	spin_lock_irqsave(&sde_enc->intf_lock, lock_flags);
	sde_enc->hw_intf->ops.enable_timing(sde_enc->hw_intf, 0);
	spin_unlock_irqrestore(&sde_enc->intf_lock, lock_flags);

	/*
	 * Wait for a vsync so we know the ENABLE=0 latched before
	 * the (connector) source of the vsync's gets disabled,
	 * otherwise we end up in a funny state if we re-enable
	 * before the disable latches, which results that some of
	 * the settings changes for the new modeset (like new
	 * scanout buffer) don't latch properly..
	 */
	sde_encoder_wait_for_vblank(sde_enc);

	mdp_irq_unregister(mdp_kms, &sde_enc->vblank_irq);
	bs_set(sde_enc, 0);
	sde_enc->enabled = false;
}

static void sde_encoder_enable(struct drm_encoder *drm_enc)
{
	struct sde_encoder *sde_enc = to_sde_encoder(drm_enc);
	struct mdp_kms *mdp_kms = &(get_kms(drm_enc)->base);
	unsigned long lock_flags;

	DBG("");

	if (WARN_ON(sde_enc->enabled))
		return;

	bs_set(sde_enc, 1);
	spin_lock_irqsave(&sde_enc->intf_lock, lock_flags);
	sde_enc->hw_intf->ops.enable_timing(sde_enc->hw_intf, 1);
	spin_unlock_irqrestore(&sde_enc->intf_lock, lock_flags);
	sde_enc->enabled = true;

	mdp_irq_register(mdp_kms, &sde_enc->vblank_irq);
	DBG("Registered IRQ for intf %d mask 0x%X", sde_enc->hw_intf->idx,
	    sde_enc->vblank_irq.irqmask);
}

void sde_encoder_get_hw_resources(struct drm_encoder *drm_enc,
				  struct sde_encoder_hw_resources *hw_res)
{
	struct sde_encoder *sde_enc = to_sde_encoder(drm_enc);

	DBG("");

	if (WARN_ON(!hw_res))
		return;

	memset(hw_res, 0, sizeof(*hw_res));
	hw_res->intfs[sde_enc->hw_intf->idx] = true;
}

static void sde_encoder_destroy(struct drm_encoder *drm_enc)
{
	struct sde_encoder *sde_enc = to_sde_encoder(drm_enc);

	DBG("");
	drm_encoder_cleanup(drm_enc);
	bs_fini(sde_enc);
	kfree(sde_enc->hw_intf);
	kfree(sde_enc);
}

static const struct drm_encoder_helper_funcs sde_encoder_helper_funcs = {
	.mode_fixup = sde_encoder_mode_fixup,
	.mode_set = sde_encoder_mode_set,
	.disable = sde_encoder_disable,
	.enable = sde_encoder_enable,
};

static const struct drm_encoder_funcs sde_encoder_funcs = {.destroy =
	    sde_encoder_destroy,
};

static int sde_encoder_setup_hw(struct sde_encoder *sde_enc,
				struct sde_kms *sde_kms,
				enum sde_intf intf_idx,
				enum sde_ctl ctl_idx)
{
	int ret = 0;

	DBG("");

	sde_enc->hw_intf = sde_hw_intf_init(intf_idx, sde_kms->mmio,
					    sde_kms->catalog);
	if (!sde_enc->hw_intf)
		return -EINVAL;

	sde_enc->hw_ctl = sde_hw_ctl_init(ctl_idx, sde_kms->mmio,
					    sde_kms->catalog);
	if (!sde_enc->hw_ctl)
		return -EINVAL;

	return ret;
}

static int sde_encoder_virt_add_phys_vid_enc(struct sde_encoder *sde_enc,
					     struct sde_kms *sde_kms,
					     enum sde_intf intf_idx,
					     enum sde_ctl ctl_idx)
{
	int ret = 0;

	DBG("");

	ret = sde_encoder_setup_hw(sde_enc, sde_kms, intf_idx, ctl_idx);
	if (!ret) {
		sde_enc->vblank_irq.irq = sde_encoder_vblank_irq;
		sde_enc->vblank_irq.irqmask = 0x8000000;
	}
	return ret;
}

static int sde_encoder_setup_hdmi(struct sde_encoder *sde_enc,
				  struct sde_kms *sde_kms, int *hdmi_info)
{
	int ret = 0;
	enum sde_intf intf_idx = INTF_MAX;

	DBG("");

	sde_enc->drm_mode_enc = DRM_MODE_ENCODER_TMDS;

	intf_idx = INTF_3;
	if (intf_idx == INTF_MAX)
		ret = -EINVAL;

	if (!ret)
		ret =
		    sde_encoder_virt_add_phys_vid_enc(sde_enc, sde_kms,
						      intf_idx,
						      CTL_2);

	return ret;
}

static int sde_encoder_setup_dsi(struct sde_encoder *sde_enc,
				 struct sde_kms *sde_kms,
				 struct dsi_display_info *dsi_info)
{
	int ret = 0;
	int i = 0;

	DBG("");

	sde_enc->drm_mode_enc = DRM_MODE_ENCODER_DSI;

	if (WARN_ON(dsi_info->num_of_h_tiles > 1)) {
		DBG("Dual DSI mode not yet supported");
		ret = -EINVAL;
	}

	WARN_ON(dsi_info->num_of_h_tiles != 1);
	dsi_info->num_of_h_tiles = 1;

	DBG("dsi_info->num_of_h_tiles %d h_tiled %d dsi_info->h_tile_ids %d ",
			dsi_info->num_of_h_tiles, dsi_info->h_tiled,
			dsi_info->h_tile_ids[0]);

	for (i = 0; i < !ret && dsi_info->num_of_h_tiles; i++) {
		enum sde_intf intf_idx = INTF_1;
		enum sde_ctl ctl_idx = CTL_0;

		if (intf_idx == INTF_MAX) {
			DBG("Error: could not get the interface id");
			ret = -EINVAL;
		}

		/*  Get DSI modes, create both VID & CMD Phys Encoders */
		if (!ret)
			ret =
			    sde_encoder_virt_add_phys_vid_enc(sde_enc, sde_kms,
							      intf_idx,
							      ctl_idx);
	}

	return ret;
}

struct display_probe_info {
	enum sde_intf_type type;
	struct dsi_display_info dsi_info;
	int hdmi_info;
};

static struct drm_encoder *sde_encoder_virt_init(struct drm_device *dev,
		struct display_probe_info
		*display)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(to_mdp_kms(priv->kms));
	struct drm_encoder *drm_enc = NULL;
	struct sde_encoder *sde_enc = NULL;
	int ret = 0;

	DBG("");

	sde_enc = kzalloc(sizeof(*sde_enc), GFP_KERNEL);
	if (!sde_enc) {
		ret = -ENOMEM;
		goto fail;
	}

	if (display->type == INTF_DSI) {
		ret =
		    sde_encoder_setup_dsi(sde_enc, sde_kms, &display->dsi_info);
	} else if (display->type == INTF_HDMI) {
		ret =
		    sde_encoder_setup_hdmi(sde_enc, sde_kms,
					   &display->hdmi_info);
	} else {
		DBG("No valid displays found");
		ret = -EINVAL;
	}
	if (ret)
		goto fail;

	spin_lock_init(&sde_enc->intf_lock);
	drm_enc = &sde_enc->base;
	drm_encoder_init(dev, drm_enc, &sde_encoder_funcs,
			 sde_enc->drm_mode_enc);
	drm_encoder_helper_add(drm_enc, &sde_encoder_helper_funcs);
	bs_init(sde_enc);

	DBG("Created sde_encoder for intf %d", sde_enc->hw_intf->idx);

	return drm_enc;

fail:
	if (drm_enc)
		sde_encoder_destroy(drm_enc);

	return ERR_PTR(ret);
}

static int sde_encoder_probe_hdmi(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct drm_encoder *enc = NULL;
	struct display_probe_info probe_info = { 0 };
	int ret = 0;

	DBG("");

	probe_info.type = INTF_HDMI;

	enc = sde_encoder_virt_init(dev, &probe_info);
	if (IS_ERR(enc))
		ret = PTR_ERR(enc);
	else {
		/*  Register new encoder with the upper layer */
		priv->encoders[priv->num_encoders++] = enc;
	}
	return ret;
}

static int sde_encoder_probe_dsi(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	u32 ret = 0;
	u32 i = 0;
	u32 num_displays = 0;

	DBG("");

	num_displays = dsi_display_get_num_of_displays();
	DBG("num_displays %d", num_displays);
	for (i = 0; i < num_displays; i++) {
		struct dsi_display *dsi = dsi_display_get_display_by_index(i);

		if (dsi_display_is_active(dsi)) {
			struct display_probe_info probe_info = { 0 };

			DBG("display %d/%d is active", i, num_displays);
			probe_info.type = INTF_DSI;

			ret = dsi_display_get_info(dsi, &probe_info.dsi_info);
			if (WARN_ON(ret))
				DBG("Failed to retrieve dsi panel info");
			else {
				struct drm_encoder *enc =
				    sde_encoder_virt_init(dev,
							  &probe_info);
				if (IS_ERR(enc))
					return PTR_ERR(enc);

				ret = dsi_display_drm_init(dsi, enc);
				if (ret)
					return ret;

				/*  Register new encoder with the upper layer */
				priv->encoders[priv->num_encoders++] = enc;
			}
		} else
			DBG("display %d/%d is not active", i, num_displays);
	}

	return ret;
}

void sde_encoder_register_vblank_callback(struct drm_encoder *drm_enc,
		void (*cb)(void *), void *data) {
	struct sde_encoder *sde_enc = to_sde_encoder(drm_enc);
	unsigned long lock_flags;

	DBG("");

	spin_lock_irqsave(&sde_enc->intf_lock, lock_flags);
	sde_enc->vblank_callback = cb;
	sde_enc->vblank_callback_data = data;
	spin_unlock_irqrestore(&sde_enc->intf_lock, lock_flags);
}

/* encoders init,
 * initialize encoder based on displays
 */
void sde_encoders_init(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	int ret = 0;

	DBG("");

	/* Start num_encoders at 0, probe functions will increment */
	priv->num_encoders = 0;
	ret = sde_encoder_probe_dsi(dev);
	if (ret)
		DBG("Error probing DSI, %d", ret);
	else {
		ret = sde_encoder_probe_hdmi(dev);
		if (ret)
			DBG("Error probing HDMI, %d", ret);
	}
}
