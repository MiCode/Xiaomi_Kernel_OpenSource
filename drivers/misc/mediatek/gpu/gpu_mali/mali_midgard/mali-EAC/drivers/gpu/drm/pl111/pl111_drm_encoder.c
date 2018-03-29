/*
 *
 * (C) COPYRIGHT ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



/**
 * pl111_drm_encoder.c
 * Implementation of the encoder functions for PL111 DRM
 */
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/version.h>
#include <linux/shmem_fs.h>
#include <linux/dma-buf.h>
#include <linux/module.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>

#include "pl111_drm.h"

bool pl111_encoder_helper_mode_fixup(struct drm_encoder *encoder,
					struct drm_display_mode *mode,
					struct drm_display_mode *adjusted_mode)
{
	DRM_DEBUG_KMS("DRM %s on encoder=%p\n", __func__, encoder);
	return true;
}

void pl111_encoder_helper_prepare(struct drm_encoder *encoder)
{
	DRM_DEBUG_KMS("DRM %s on encoder=%p\n", __func__, encoder);
}

void pl111_encoder_helper_commit(struct drm_encoder *encoder)
{
	DRM_DEBUG_KMS("DRM %s on encoder=%p\n", __func__, encoder);
}

void pl111_encoder_helper_mode_set(struct drm_encoder *encoder,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	DRM_DEBUG_KMS("DRM %s on encoder=%p\n", __func__, encoder);
}

void pl111_encoder_helper_disable(struct drm_encoder *encoder)
{
	DRM_DEBUG_KMS("DRM %s on encoder=%p\n", __func__, encoder);
}

void pl111_encoder_destroy(struct drm_encoder *encoder)
{
	struct pl111_drm_encoder *pl111_encoder =
					PL111_ENCODER_FROM_ENCODER(encoder);

	DRM_DEBUG_KMS("DRM %s on encoder=%p\n", __func__, encoder);

	drm_encoder_cleanup(encoder);
	kfree(pl111_encoder);
}

const struct drm_encoder_funcs encoder_funcs = {
	.destroy = pl111_encoder_destroy,
};

const struct drm_encoder_helper_funcs encoder_helper_funcs = {
	.mode_fixup = pl111_encoder_helper_mode_fixup,
	.prepare = pl111_encoder_helper_prepare,
	.commit = pl111_encoder_helper_commit,
	.mode_set = pl111_encoder_helper_mode_set,
	.disable = pl111_encoder_helper_disable,
};

struct pl111_drm_encoder *pl111_encoder_create(struct drm_device *dev,
						int possible_crtcs)
{
	struct pl111_drm_encoder *pl111_encoder;

	pl111_encoder = kzalloc(sizeof(struct pl111_drm_encoder), GFP_KERNEL);
	if (pl111_encoder == NULL) {
		pr_err("Failed to allocated pl111_drm_encoder\n");
		return NULL;
	}

	drm_encoder_init(dev, &pl111_encoder->encoder, &encoder_funcs,
				DRM_MODE_ENCODER_DAC);

	drm_encoder_helper_add(&pl111_encoder->encoder, &encoder_helper_funcs);

	pl111_encoder->encoder.possible_crtcs = possible_crtcs;

	return pl111_encoder;
}

