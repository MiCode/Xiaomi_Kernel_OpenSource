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

struct sde_encoder {
	struct drm_encoder base;
	int intf;
};
#define to_sde_encoder(x) container_of(x, struct sde_encoder, base)

static void sde_encoder_destroy(struct drm_encoder *encoder)
{
	struct sde_encoder *sde_encoder = to_sde_encoder(encoder);

	drm_encoder_cleanup(encoder);
	kfree(sde_encoder);
}

static const struct drm_encoder_funcs sde_encoder_funcs = {
	.destroy = sde_encoder_destroy,
};

static void sde_encoder_dpms(struct drm_encoder *encoder, int mode)
{
}

static bool sde_encoder_mode_fixup(struct drm_encoder *encoder,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	return true;
}

static void sde_encoder_mode_set(struct drm_encoder *encoder,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
}

static void sde_encoder_prepare(struct drm_encoder *encoder)
{
}

static void sde_encoder_commit(struct drm_encoder *encoder)
{
}

static const struct drm_encoder_helper_funcs sde_encoder_helper_funcs = {
	.dpms = sde_encoder_dpms,
	.mode_fixup = sde_encoder_mode_fixup,
	.mode_set = sde_encoder_mode_set,
	.prepare = sde_encoder_prepare,
	.commit = sde_encoder_commit,
};

/* initialize encoder */
struct drm_encoder *sde_encoder_init(struct drm_device *dev, int intf)
{
	struct drm_encoder *encoder = NULL;
	struct sde_encoder *sde_encoder;
	int ret;

	sde_encoder = kzalloc(sizeof(*sde_encoder), GFP_KERNEL);
	if (!sde_encoder) {
		ret = -ENOMEM;
		goto fail;
	}

	sde_encoder->intf = intf;
	encoder = &sde_encoder->base;

	drm_encoder_init(dev, encoder, &sde_encoder_funcs,
			 DRM_MODE_ENCODER_TMDS);
	drm_encoder_helper_add(encoder, &sde_encoder_helper_funcs);

	return encoder;

fail:
	if (encoder)
		sde_encoder_destroy(encoder);

	return ERR_PTR(ret);
}
