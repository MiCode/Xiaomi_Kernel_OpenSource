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

#include "edrm_plane.h"
#include "edrm_crtc.h"
#include "sde_kms.h"
#include "edrm_kms.h"

/* SDE_SSPP_SRC */
#define SSPP_SRC_SIZE                      0x00
#define SSPP_SRC_XY                        0x08
#define SSPP_OUT_SIZE                      0x0c
#define SSPP_OUT_XY                        0x10
#define SSPP_SRC0_ADDR                     0x14
#define SSPP_SRC1_ADDR                     0x18
#define SSPP_SRC2_ADDR                     0x1C
#define SSPP_SRC3_ADDR                     0x20
#define SSPP_SRC_YSTRIDE0                  0x24
#define SSPP_SRC_YSTRIDE1                  0x28
#define SSPP_SRC_FORMAT                    0x30
#define SSPP_SRC_UNPACK_PATTERN            0x34
#define SSPP_SRC_OP_MODE                   0x38
#define SSPP_CONSTANT_COLOR                0x3c
#define PIPE_SW_PIXEL_EXT_C0_REQ           0x108
#define PIPE_SW_PIXEL_EXT_C1C2_REQ         0x118
#define PIPE_SW_PIXEL_EXT_C3_REQ           0x128
#define FLUSH_OFFSET                       0x18
#define SSPP_SOLID_FILL_FORMAT             0x004237FF
#define SSPP_RGB888_FORMAT                 0x000237FF
#define SSPP_RGB_PATTERN                   0x03020001

void edrm_plane_destroy(struct drm_plane *plane)
{
	struct edrm_plane *edrm_plane = to_edrm_plane(plane);

	drm_plane_helper_disable(plane);
	drm_plane_cleanup(plane);
	kfree(edrm_plane);
}

int edrm_plane_flush(struct drm_plane *plane)
{
	struct edrm_plane *edrm_plane = to_edrm_plane(plane);
	struct edrm_crtc *edrm_crtc = to_edrm_crtc(plane->state->crtc);
	u32 sspp_flush_mask_bit[10] = {
				0, 1, 2, 18, 3, 4, 5, 19, 11, 12};

	edrm_crtc->sspp_flush_mask |=
		BIT(sspp_flush_mask_bit[edrm_plane->sspp_cfg_id - 1]);
	return 0;
}

static int edrm_plane_atomic_check(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	/* TODO: check plane setting */
	return 0;
}

static void edrm_plane_atomic_update(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct drm_device *dev = plane->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	struct msm_edrm_kms *edrm_kms;
	struct msm_drm_private *master_priv;
	struct sde_kms *master_kms;
	struct edrm_plane *edrm_plane;
	u32 img_size, src_xy, dst_xy;
	u32 ystride0, plane_addr;

	edrm_kms = to_edrm_kms(kms);
	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);

	if (!plane->state->crtc) {
		pr_err("state crtc is null, skip pipe programming\n");
		return;
	}
	if (!plane->state->fb) {
		pr_err("state fb is null, skip pipe programming\n");
		return;
	}
	/* Support RGB format only */
	edrm_plane = to_edrm_plane(plane);
	img_size = (plane->state->fb->height << 16) | plane->state->fb->width;
	src_xy = (plane->state->src_x << 16) | plane->state->src_y;
	dst_xy = (plane->state->crtc_x << 16) | plane->state->crtc_y;
	ystride0 = (plane->state->fb->width *
		plane->state->fb->bits_per_pixel/8);
	plane_addr = msm_framebuffer_iova(plane->state->fb,
		edrm_plane->aspace, 0);
	if (!plane_addr) {
		pr_err("plane update failed to retrieve base addr\n");
		return;
	}

	/* rectangle register programming */
	writel_relaxed(plane_addr, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC0_ADDR);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC_SIZE);
	writel_relaxed(src_xy, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC_XY);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_OUT_SIZE);
	writel_relaxed(dst_xy, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_OUT_XY);
	writel_relaxed(ystride0, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC_YSTRIDE0);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C0_REQ);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C1C2_REQ);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C3_REQ);

	/* RGB888 format */
	writel_relaxed(SSPP_RGB888_FORMAT, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
	writel_relaxed(SSPP_RGB_PATTERN, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
}

/* Plane disable should setup the sspp to show a transparent frame
 * If the pipe still attached with a buffer pointer, the buffer could
 * be released and cause SMMU fault.  We don't want to change CTL and
 * LM during eDRM closing because main DRM could be updating CTL and
 * LM at any moment.  In eDRM lastclose(), it will notify main DRM to
 * release eDRM display resouse.  The next main DRM commit will clear
 * the stage setup by eDRM
 */
static void edrm_plane_atomic_disable(struct drm_plane *plane,
		struct drm_plane_state *state)
{
	struct drm_device *dev = plane->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct msm_kms *kms = priv->kms;
	struct msm_edrm_kms *edrm_kms;
	struct msm_drm_private *master_priv;
	struct sde_kms *master_kms;
	struct msm_edrm_display *display;
	struct edrm_plane *edrm_plane;
	u32 lm_off, img_size, stride;

	edrm_kms = to_edrm_kms(kms);
	master_priv = edrm_kms->master_dev->dev_private;
	master_kms = to_sde_kms(master_priv->kms);
	dev = edrm_kms->dev;
	priv = dev->dev_private;

	edrm_plane = to_edrm_plane(plane);
	display = &edrm_kms->display[edrm_plane->display_id];
	lm_off = display->lm_off;

	/* setup SSPP */
	img_size = (display->mode.vdisplay << 16) | display->mode.hdisplay;
	stride = display->mode.hdisplay * 4;
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
		SSPP_SRC_SIZE);
	writel_relaxed(0, master_kms->mmio + edrm_plane->sspp_offset
		+ SSPP_SRC_XY);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_OUT_SIZE);
	writel_relaxed(0, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_OUT_XY);
	writel_relaxed(stride, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_SRC_YSTRIDE0);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C0_REQ);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
			PIPE_SW_PIXEL_EXT_C1C2_REQ);
	writel_relaxed(img_size, master_kms->mmio + edrm_plane->sspp_offset +
		PIPE_SW_PIXEL_EXT_C3_REQ);

	/* RGB format */
	writel_relaxed(SSPP_SOLID_FILL_FORMAT, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_FORMAT);
	writel_relaxed(SSPP_RGB888_FORMAT, master_kms->mmio +
		edrm_plane->sspp_offset + SSPP_SRC_UNPACK_PATTERN);
	/* do a solid fill of transparent color */
	writel_relaxed(0x0, master_kms->mmio + edrm_plane->sspp_offset +
			SSPP_CONSTANT_COLOR);
}

static int edrm_plane_prepare_fb(struct drm_plane *plane,
		const struct drm_plane_state *new_state)
{
	struct drm_framebuffer *fb;
	struct edrm_plane *edrm_plane;

	if (!plane || !new_state)
		return -EINVAL;

	if (!new_state->fb)
		return 0;
	edrm_plane = to_edrm_plane(plane);
	fb = new_state->fb;
	return msm_framebuffer_prepare(fb, edrm_plane->aspace);
}

static void edrm_plane_cleanup_fb(struct drm_plane *plane,
		const struct drm_plane_state *old_state)
{
	struct drm_framebuffer *fb = old_state ? old_state->fb : NULL;
	struct edrm_plane *edrm_plane = plane ? to_edrm_plane(plane) : NULL;

	if (!fb || !plane)
		return;

	msm_framebuffer_cleanup(fb, edrm_plane->aspace);
}

static const struct drm_plane_funcs edrm_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = edrm_plane_destroy,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
};

static const struct drm_plane_helper_funcs edrm_plane_helper_funcs = {
	.prepare_fb = edrm_plane_prepare_fb,
	.cleanup_fb = edrm_plane_cleanup_fb,
	.atomic_check = edrm_plane_atomic_check,
	.atomic_update = edrm_plane_atomic_update,
	.atomic_disable = edrm_plane_atomic_disable,
};

static uint32_t edrm_plane_formats[] = {
	DRM_FORMAT_ARGB8888,
};

struct drm_plane *edrm_plane_init(struct drm_device *dev, int pipe)
{
	struct msm_drm_private *priv;
	struct msm_edrm_kms *edrm_kms;
	struct edrm_plane *edrm_plane;
	struct drm_plane *plane;
	int ret;

	edrm_plane = kzalloc(sizeof(*edrm_plane), GFP_KERNEL);
	if (!edrm_plane) {
		ret = -ENOMEM;
		goto fail;
	}

	plane = &edrm_plane->base;

	ret = drm_universal_plane_init(dev, plane, 0,
		&edrm_plane_funcs,
		edrm_plane_formats,
		ARRAY_SIZE(edrm_plane_formats),
		DRM_PLANE_TYPE_PRIMARY);
	if (ret)
		goto fail;

	drm_plane_helper_add(plane, &edrm_plane_helper_funcs);

	priv = dev->dev_private;
	edrm_kms = to_edrm_kms(priv->kms);

	edrm_plane->pipe = pipe;
	edrm_plane->aspace = edrm_kms->aspace;

	return plane;
fail:
	return ERR_PTR(ret);
}
