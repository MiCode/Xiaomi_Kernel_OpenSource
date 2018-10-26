/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include "edrm_encoder.h"
#include "edrm_crtc.h"
#include "sde_kms.h"

static void edrm_encoder_enable(struct drm_encoder *drm_enc)
{
	pr_err("eDRM Encoder enable\n");
}

static void edrm_encoder_disable(struct drm_encoder *drm_enc)
{
	pr_err("eDRM Encoder disable\n");
}

void edrm_encoder_destroy(struct drm_encoder *encoder)
{
	struct edrm_encoder *edrm_enc = to_edrm_encoder(encoder);

	drm_encoder_cleanup(encoder);
	kfree(edrm_enc);
}

static const struct drm_encoder_helper_funcs edrm_encoder_helper_funcs = {
	.disable = edrm_encoder_disable,
	.enable = edrm_encoder_enable,
};

static const struct drm_encoder_funcs edrm_encoder_funcs = {
	.destroy = edrm_encoder_destroy,
};

int edrm_encoder_wait_for_commit_done(struct drm_encoder *drm_enc)
{
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct msm_edrm_kms *edrm_kms;
	struct msm_edrm_display *display;
	struct edrm_crtc *edrm_crtc;
	struct sde_kms *master_kms;
	struct msm_drm_private *master_priv;
	struct sde_mdss_cfg *cfg;
	u32 ctl_off;
	u32 flush_register = 0;
	int i;

	dev = drm_enc->dev;
	priv = dev->dev_private;
	edrm_kms = to_edrm_kms(priv->kms);
	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);
	cfg = master_kms->catalog;
	edrm_crtc = to_edrm_crtc(drm_enc->crtc);
	display = &edrm_kms->display[edrm_crtc->display_id];
	ctl_off = display->ctl_off;

	/* poll edrm_crtc->sspp_flush_mask until cleared */
	for (i = 0; i < 20; i++) {
		flush_register = readl_relaxed(master_kms->mmio +
				ctl_off + 0x18);
		if ((flush_register & edrm_crtc->sspp_flush_mask) != 0)
			usleep_range(1000, 2000);
		else
			break;
	}

	/* reset sspp_flush_mask */
	edrm_crtc->sspp_flush_mask = 0;

	return 0;
}


struct drm_encoder *edrm_encoder_init(struct drm_device *dev,
					struct msm_edrm_display *display)
{
	struct edrm_encoder *edrm_encoder;
	struct drm_encoder *encoder;
	int ret;

	edrm_encoder = kzalloc(sizeof(*edrm_encoder), GFP_KERNEL);
	if (!edrm_encoder)
		return ERR_PTR(-ENOMEM);

	encoder = &edrm_encoder->base;

	ret = drm_encoder_init(dev, encoder,
			&edrm_encoder_funcs,
			display->encoder_type);
	if (ret)
		goto fail;

	drm_encoder_helper_add(encoder, &edrm_encoder_helper_funcs);

	edrm_encoder->intf_idx = display->intf_id;

	return encoder;
fail:
	kfree(edrm_encoder);
	return ERR_PTR(ret);
}
