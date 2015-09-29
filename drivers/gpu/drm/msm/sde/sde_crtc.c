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

#include <drm/drm_mode.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_flip_work.h>

#include "sde_kms.h"
#include "sde_hw_lm.h"
#include "sde_hw_mdss.h"

struct sde_crtc {
	struct drm_crtc base;
	char name[8];
	struct drm_plane *plane;
	struct drm_plane *planes[8];
	int id;
	bool enabled;
	enum sde_lm mixer;
	enum sde_ctl ctl_path;
};

#define to_sde_crtc(x) container_of(x, struct sde_crtc, base)

static void sde_crtc_destroy(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	drm_crtc_cleanup(crtc);
	kfree(sde_crtc);
}

static void sde_crtc_dpms(struct drm_crtc *crtc, int mode)
{
}

static bool sde_crtc_mode_fixup(struct drm_crtc *crtc,
		const struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode)
{
	return true;
}

static int sde_crtc_mode_set(struct drm_crtc *crtc,
		struct drm_display_mode *mode,
		struct drm_display_mode *adjusted_mode,
		int x, int y,
		struct drm_framebuffer *old_fb)
{
	return 0;
}

static void sde_crtc_prepare(struct drm_crtc *crtc)
{
}

static void sde_crtc_commit(struct drm_crtc *crtc)
{
}

static int sde_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
		struct drm_framebuffer *old_fb)
{
	return 0;
}

static void sde_crtc_load_lut(struct drm_crtc *crtc)
{
}

static int sde_crtc_page_flip(struct drm_crtc *crtc,
		struct drm_framebuffer *new_fb,
		struct drm_pending_vblank_event *event,
		uint32_t page_flip_flags)
{
	return 0;
}

static int sde_crtc_set_property(struct drm_crtc *crtc,
		struct drm_property *property, uint64_t val)
{
	return -EINVAL;
}

static const struct drm_crtc_funcs sde_crtc_funcs = {
	.set_config = drm_crtc_helper_set_config,
	.destroy = sde_crtc_destroy,
	.page_flip = sde_crtc_page_flip,
	.set_property = sde_crtc_set_property,
};

static const struct drm_crtc_helper_funcs sde_crtc_helper_funcs = {
	.dpms = sde_crtc_dpms,
	.mode_fixup = sde_crtc_mode_fixup,
	.mode_set = sde_crtc_mode_set,
	.prepare = sde_crtc_prepare,
	.commit = sde_crtc_commit,
	.mode_set_base = sde_crtc_mode_set_base,
	.load_lut = sde_crtc_load_lut,
};

uint32_t sde_crtc_vblank(struct drm_crtc *crtc)
{
	return 0;
}

void sde_crtc_cancel_pending_flip(struct drm_crtc *crtc, struct drm_file *file)
{
}

void sde_crtc_attach(struct drm_crtc *crtc, struct drm_plane *plane)
{
}

void sde_crtc_detach(struct drm_crtc *crtc, struct drm_plane *plane)
{
}

struct drm_crtc *sde_crtc_init(struct drm_device *dev,
		struct drm_encoder *encoder,
		struct drm_plane *plane, int id)
{
	struct drm_crtc *crtc = NULL;
	struct sde_crtc *sde_crtc;

	sde_crtc = kzalloc(sizeof(*sde_crtc), GFP_KERNEL);
	if (!sde_crtc)
		return ERR_PTR(-ENOMEM);

	crtc = &sde_crtc->base;

	sde_crtc->id = id;

	/* find out if we need one or two lms */

	drm_crtc_helper_add(crtc, &sde_crtc_helper_funcs);
	return crtc;
}
