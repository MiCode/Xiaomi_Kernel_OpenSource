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

#include "edrm_crtc.h"
#include "edrm_plane.h"
#include "edrm_encoder.h"
#include "sde_kms.h"

/* display control path Flush register offset */
#define FLUSH_OFFSET   0x18
#define SSPP_SRC_FORMAT                    0x30
#define SSPP_SRC_UNPACK_PATTERN            0x34
#define SSPP_SRC_OP_MODE                   0x38
#define SSPP_CONSTANT_COLOR                0x3c
#define LAYER_BLEND5_OP                    0x260
#define FLUST_CTL_BIT                      17
#define LAYER_OP_ENABLE_ALPHA_BLEND        0x600

static void edrm_crtc_plane_attach(struct drm_crtc *crtc,
	struct drm_plane *plane)
{
	struct drm_device *dev = crtc->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	struct msm_edrm_kms *edrm_kms = to_edrm_kms(kms);
	struct msm_drm_private *master_priv =
			edrm_kms->master_dev->dev_private;
	struct sde_kms *master_kms = to_sde_kms(master_priv->kms);
	u32 layer_val, ctl_off, lm_idx;
	struct edrm_plane *edrm_plane = to_edrm_plane(plane);
	struct edrm_crtc *edrm_crtc = to_edrm_crtc(crtc);
	struct msm_edrm_display *display;

	display = &edrm_kms->display[edrm_crtc->display_id];
	ctl_off = display->ctl_off;
	lm_idx = (display->ctl_id - 1) * 0x4;

	layer_val = readl_relaxed(master_kms->mmio + ctl_off + lm_idx);
	switch (edrm_plane->sspp_cfg_id) {
	case 1: /* vig 0 */
		layer_val |= edrm_plane->lm_stage + 2;
		break;
	case 2: /* vig 1 */
		layer_val |= (edrm_plane->lm_stage + 2) << 3;
		break;
	case 3: /* vig 2 */
		layer_val |= (edrm_plane->lm_stage + 2) << 6;
		break;
	case 4: /* vig 3 */
		layer_val |= (edrm_plane->lm_stage + 2) << 26;
		break;
	case 5: /* rgb 0 */
		layer_val |= (edrm_plane->lm_stage + 2) << 9;
		break;
	case 6: /* rgb 1 */
		layer_val |= (edrm_plane->lm_stage + 2) << 12;
		break;
	case 7: /* rgb 2 */
		layer_val |= (edrm_plane->lm_stage + 2) << 15;
		break;
	case 8: /* rgb 3 */
		layer_val |= (edrm_plane->lm_stage + 2) << 29;
		break;
	case 9: /* dma 0 */
		layer_val |= (edrm_plane->lm_stage + 2) << 18;
		break;
	case 10: /* dma 1 */
		layer_val |= (edrm_plane->lm_stage + 2) << 21;
		break;
	}
	writel_relaxed(layer_val, master_kms->mmio + ctl_off + lm_idx);
	plane->crtc = crtc;
}

void edrm_crtc_postinit(struct drm_crtc *crtc)
{
	struct drm_device *dev = crtc->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	struct msm_edrm_kms *edrm_kms;
	struct sde_kms *master_kms;
	struct msm_drm_private *master_priv;
	struct msm_edrm_display *display;
	struct edrm_crtc *edrm_crtc;
	struct edrm_plane *edrm_plane;
	u32 lm_off, flush_val;
	const struct drm_plane_helper_funcs *funcs;
	u32 sspp_flush_mask_bit[10] = {
				0, 1, 2, 18, 3, 4, 5, 19, 11, 12};

	edrm_kms = to_edrm_kms(kms);
	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);
	edrm_plane = to_edrm_plane(crtc->primary);
	edrm_crtc = to_edrm_crtc(crtc);
	funcs = crtc->primary->helper_private;
	funcs->atomic_disable(crtc->primary, crtc->primary->state);
	display = &edrm_kms->display[edrm_crtc->display_id];
	lm_off = display->lm_off;

	edrm_crtc_plane_attach(crtc, crtc->primary);

	/* Update CTL bit, layer mixer flush bit and sspp flush bit */
	flush_val = BIT(FLUST_CTL_BIT);
	flush_val |= BIT(display->ctl_id + 5);
	flush_val |= BIT(sspp_flush_mask_bit[edrm_plane->sspp_cfg_id - 1]);

	/* setup alpha blending for mixer stage 5 */
	writel_relaxed(LAYER_OP_ENABLE_ALPHA_BLEND, master_kms->mmio + lm_off +
		LAYER_BLEND5_OP);
	edrm_crtc->sspp_flush_mask |= flush_val;

	edrm_crtc_commit_kickoff(crtc);
}

static void edrm_crtc_atomic_flush(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state)
{
	struct drm_plane *plane = NULL;

	if (!crtc) {
		pr_err("invalid crtc\n");
		return;
	}

	/* TODO: wait for acquire fences before anything else is done */
	drm_atomic_crtc_for_each_plane(plane, crtc) {
		/* update SSPP bit in sspp_flush_mask */
		edrm_plane_flush(plane);
	}
}

static void edrm_crtc_enable(struct drm_crtc *crtc)
{
	crtc->state->enable = true;
}

static void edrm_crtc_disable(struct drm_crtc *crtc)
{
	struct edrm_plane *edrm_plane;
	struct edrm_crtc *edrm_crtc = to_edrm_crtc(crtc);
	const struct drm_plane_helper_funcs *funcs;
	u32 sspp_flush_mask_bit[10] = {
				0, 1, 2, 18, 3, 4, 5, 19, 11, 12};
	struct drm_encoder *encoder;

	edrm_plane = to_edrm_plane(crtc->primary);
	funcs = crtc->primary->helper_private;
	funcs->atomic_disable(crtc->primary, crtc->primary->state);

	edrm_crtc->sspp_flush_mask |=
		BIT(sspp_flush_mask_bit[edrm_plane->sspp_cfg_id - 1]);
	edrm_crtc_commit_kickoff(crtc);

	drm_for_each_encoder(encoder, crtc->dev) {
		if (encoder->crtc != crtc)
			continue;
		edrm_encoder_wait_for_commit_done(encoder);
	}
}

void edrm_crtc_destroy(struct drm_crtc *crtc)
{
	struct edrm_crtc *edrm_crtc = to_edrm_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(edrm_crtc);
}

static const struct drm_crtc_funcs edrm_crtc_funcs = {
	.reset =  drm_atomic_helper_crtc_reset,
	.set_config = drm_atomic_helper_set_config,
	.destroy = edrm_crtc_destroy,
	.page_flip = drm_atomic_helper_page_flip,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
};

static const struct drm_crtc_helper_funcs edrm_crtc_helper_funcs = {
	.disable = edrm_crtc_disable,
	.enable = edrm_crtc_enable,
	.atomic_flush = edrm_crtc_atomic_flush,
};

struct drm_crtc *edrm_crtc_init(struct drm_device *dev,
					struct msm_edrm_display *display,
					struct drm_plane *primary_plane)
{
	struct edrm_crtc *edrm_crtc;
	struct drm_crtc *crtc;
	int ret;

	edrm_crtc = kzalloc(sizeof(*edrm_crtc), GFP_KERNEL);
	if (!edrm_crtc) {
		ret = -ENOMEM;
		goto fail_no_mem;
	}

	crtc = &edrm_crtc->base;
	ret = drm_crtc_init_with_planes(dev, crtc, primary_plane, NULL,
		&edrm_crtc_funcs);
	if (ret)
		goto fail;

	drm_crtc_helper_add(crtc, &edrm_crtc_helper_funcs);
	edrm_crtc->display_id = display->display_id;

	return crtc;
fail:
	kfree(edrm_crtc);
fail_no_mem:
	return ERR_PTR(ret);
}

void edrm_crtc_commit_kickoff(struct drm_crtc *crtc)
{
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct msm_edrm_kms *edrm_kms;
	struct msm_edrm_display *display;
	struct edrm_crtc *edrm_crtc;
	struct sde_kms *master_kms;
	struct msm_drm_private *master_priv;
	u32 ctl_off;

	dev = crtc->dev;
	priv = dev->dev_private;
	edrm_kms = to_edrm_kms(priv->kms);
	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);
	edrm_crtc = to_edrm_crtc(crtc);

	display = &edrm_kms->display[edrm_crtc->display_id];
	ctl_off = display->ctl_off;

	/* Trigger the flush */
	writel_relaxed(edrm_crtc->sspp_flush_mask, master_kms->mmio + ctl_off +
		FLUSH_OFFSET);
}

void edrm_crtc_complete_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state)
{
	struct drm_device *dev;
	struct msm_drm_private *priv;
	struct drm_encoder *encoder;

	dev = crtc->dev;
	priv = dev->dev_private;
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		if (encoder->crtc != crtc)
			continue;

		edrm_encoder_wait_for_commit_done(encoder);
	}
}

void edrm_crtc_prepare_commit(struct drm_crtc *crtc,
				struct drm_crtc_state *old_state)
{
}
