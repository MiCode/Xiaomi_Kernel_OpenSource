/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: CK Hu <ck.hu@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_plane_helper.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"

#include "../../../media/platform/mtk_mdp/mtk_mdp_core.h"

#define PRIV_BUF	(1920 * 1080 * 4)
static const u32 formats[] = {
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_RGB565,
};

static void mtk_plane_enable(struct mtk_drm_plane *mtk_plane, bool enable,
			     dma_addr_t addr, struct drm_rect *dest)
{
	struct drm_plane *plane = &mtk_plane->base;
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);
	unsigned int pitch, format;
	int x, y;

	if (WARN_ON(!plane->state || (enable && !plane->state->fb)))
		return;

	if (plane->state->fb) {
		pitch = plane->state->fb->pitches[0];
		format = plane->state->fb->pixel_format;
	} else {
		pitch = 0;
		format = DRM_FORMAT_RGBA8888;
	}

	x = plane->state->crtc_x;
	y = plane->state->crtc_y;

	if (x < 0) {
		addr -= x * 4;
		x = 0;
	}

	if (y < 0) {
		addr -= y * pitch;
		y = 0;
	}

	state->pending.enable = enable;
	if (mtk_plane->changed)
		state->pending.pitch = drm_rect_width(dest) * 4;
	else
		state->pending.pitch = pitch;
	state->pending.format = format;
	state->pending.addr = addr;
	state->pending.x = x;
	state->pending.y = y;
	state->pending.width = dest->x2 - dest->x1;
	state->pending.height = dest->y2 - dest->y1;
	wmb(); /* Make sure the above parameters are set before update */
	state->pending.dirty = true;
}

static void mtk_plane_reset(struct drm_plane *plane)
{
	struct mtk_plane_state *state;

	if (plane->state) {
		if (plane->state->fb)
			drm_framebuffer_unreference(plane->state->fb);

		state = to_mtk_plane_state(plane->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		plane->state = &state->base;
	}

	state->base.plane = plane;
	state->pending.format = DRM_FORMAT_RGB565;
}

static struct drm_plane_state *mtk_plane_duplicate_state(struct drm_plane *plane)
{
	struct mtk_plane_state *old_state = to_mtk_plane_state(plane->state);
	struct mtk_plane_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	WARN_ON(state->base.plane != plane);

	state->pending = old_state->pending;

	return &state->base;
}

static void mtk_drm_plane_destroy_state(struct drm_plane *plane,
					struct drm_plane_state *state)
{
	__drm_atomic_helper_plane_destroy_state(plane, state);
	kfree(to_mtk_plane_state(state));
}

static int mtk_plane_atomic_set_property(struct drm_plane *plane,
					 struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t val)
{
	struct mtk_plane_state *mstate = to_mtk_plane_state(state);
	struct mtk_drm_private *priv = plane->dev->dev_private;

	if (property == priv->alpha)
		mstate->pending.alpha = val;
	else if (property == priv->colorkey)
		mstate->pending.colorkey = val;
	else if (property == priv->zpos)
		mstate->pending.zpos = val;
	else
		return -EINVAL;

	return 0;
}

static int mtk_plane_atomic_get_property(struct drm_plane *plane,
					 const struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t *val)
{
	const struct mtk_plane_state *mstate =
		container_of(state, const struct mtk_plane_state, base);
	struct mtk_drm_private *priv = plane->dev->dev_private;

	if (property == priv->alpha)
		*val = mstate->pending.alpha;
	else if (property == priv->colorkey)
		*val = mstate->pending.colorkey;
	else if (property == priv->zpos)
		*val = mstate->pending.zpos;
	else
		return -EINVAL;

	return 0;
}

static const struct drm_plane_funcs mtk_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = mtk_plane_reset,
	.set_property = drm_atomic_helper_plane_set_property,
	.atomic_set_property = mtk_plane_atomic_set_property,
	.atomic_get_property = mtk_plane_atomic_get_property,
	.atomic_duplicate_state = mtk_plane_duplicate_state,
	.atomic_destroy_state = mtk_drm_plane_destroy_state,
};

static int mtk_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	struct drm_framebuffer *fb = state->fb;
	struct drm_crtc *crtc = state->crtc;
	struct drm_crtc_state *crtc_state;
	bool visible;
	struct drm_rect dest = {
		.x1 = state->crtc_x,
		.y1 = state->crtc_y,
		.x2 = state->crtc_x + state->crtc_w,
		.y2 = state->crtc_y + state->crtc_h,
	};
	struct drm_rect src = {
		/* 16.16 fixed point */
		.x1 = state->src_x,
		.y1 = state->src_y,
		.x2 = state->src_x + state->src_w,
		.y2 = state->src_y + state->src_h,
	};
	struct drm_rect clip = { 0, };

	if (!fb)
		return 0;

	if (!mtk_fb_get_gem_obj(fb)) {
		DRM_DEBUG_KMS("buffer is null\n");
		return -EFAULT;
	}

	if (!crtc)
		return 0;

	crtc_state = drm_atomic_get_crtc_state(state->state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	clip.x2 = crtc_state->mode.hdisplay;
	clip.y2 = crtc_state->mode.vdisplay;

	if (drm_plane_helper_check_update(plane, crtc, fb, &src, &dest, &clip,
					  DRM_PLANE_HELPER_NO_SCALING,
					  DRM_PLANE_HELPER_NO_SCALING,
					  true, true, &visible) == -ERANGE ||
					  state->rotation) {
		struct mtk_mdp_fmt	src_fmt;
		struct mtk_mdp_fmt	dst_fmt;
		unsigned int		src_buf_size;
		unsigned int		dst_buf_size;
		struct mdp_ctrl		rotate, hflip, vflip, global_alpha;
		struct mtk_mdp_addr	src_buf_mva;
		struct mtk_mdp_addr	dst_buf_mva;

		struct drm_gem_object *gem;
		struct mtk_drm_gem_obj *mtk_gem;
		struct mtk_drm_plane *mtk_plane = to_mtk_plane(plane);
		struct mtk_mdp_ctx *ctx = mdp_ctx_global;

		if (NULL == ctx->vpu.param) {
			mtk_mdp_vpu_init(&ctx->vpu);
			return -ERANGE;
		}
		mtk_plane->changed = true;
		mtk_plane->gem_idx = 1 - mtk_plane->gem_idx;

		/*set mdp command and sent vi upud*/
		ctx->s_frame.crop.top = src.y1 >> 16;
		ctx->s_frame.crop.left = src.x1 >> 16;
		ctx->s_frame.crop.width = drm_rect_width(&src) >> 16;
		ctx->s_frame.crop.height = drm_rect_height(&src) >> 16;
		ctx->s_frame.f_width = fb->width;
		ctx->s_frame.f_height = fb->height;
		src_fmt.pixelformat = fb->pixel_format;
		src_fmt.num_planes = drm_format_num_planes(fb->pixel_format);
		ctx->s_frame.fmt = &src_fmt;
		src_buf_size = ctx->s_frame.f_width * ctx->s_frame.f_height;
		ctx->s_frame.payload[0] = src_buf_size * drm_format_plane_cpp(
				fb->pixel_format, 0);
		ctx->s_frame.payload[1] = src_buf_size * drm_format_plane_cpp(
				fb->pixel_format, 1);
		ctx->s_frame.payload[2] = src_buf_size * drm_format_plane_cpp(
				fb->pixel_format, 2);
		mtk_mdp_hw_set_in_image_format(ctx);
		mtk_mdp_hw_set_in_size(ctx);

		ctx->d_frame.crop.top = 0;
		ctx->d_frame.crop.left = 0;
		ctx->d_frame.crop.width = drm_rect_width(&dest);
		ctx->d_frame.crop.height = drm_rect_height(&dest);
		ctx->d_frame.f_width = drm_rect_width(&dest);
		ctx->d_frame.f_height = drm_rect_height(&dest);
		dst_fmt.pixelformat = DRM_FORMAT_ARGB8888;
		dst_fmt.num_planes = 1;
		ctx->d_frame.fmt = &dst_fmt;
		dst_buf_size = ctx->d_frame.f_width * ctx->d_frame.f_height * 4;
		ctx->d_frame.payload[0] = dst_buf_size;
		ctx->d_frame.payload[1] = 0;
		ctx->d_frame.payload[2] = 0;
		mtk_mdp_hw_set_out_image_format(ctx);
		mtk_mdp_hw_set_out_size(ctx);

		rotate.val = 0;
		hflip.val = 0;
		vflip.val = 0;
		global_alpha.val = 0;

		/* DRM def: in counter clockwise direction
		 * MDP def: in clockwise direction
		 */
		if (state->rotation & BIT(DRM_ROTATE_90))
			rotate.val = 270;
		else if (state->rotation & BIT(DRM_ROTATE_180))
			rotate.val = 180;
		else if (state->rotation & BIT(DRM_ROTATE_270))
			rotate.val = 90;
		else
			rotate.val = 0;

		if (state->rotation & BIT(DRM_REFLECT_X))
			hflip.val = 1;

		if (state->rotation & BIT(DRM_REFLECT_Y))
			vflip.val = 1;

		ctx->ctrls.rotate = &rotate;
		ctx->ctrls.hflip = &hflip;
		ctx->ctrls.vflip = &vflip;
		ctx->ctrls.global_alpha = &global_alpha;
		mtk_mdp_hw_set_rotation(ctx);
		mtk_mdp_hw_set_global_alpha(ctx);

		gem = mtk_fb_get_gem_obj(fb);
		mtk_gem = to_mtk_gem_obj(gem);

		src_buf_mva.y = mtk_gem->dma_addr;
		src_buf_mva.cb = src_buf_mva.y  + ctx->s_frame.payload[0];
		src_buf_mva.cr = src_buf_mva.cb + ctx->s_frame.payload[1];

		dst_buf_mva.y = mtk_plane->gem[mtk_plane->gem_idx]->dma_addr;
		dst_buf_mva.cb = 0;
		dst_buf_mva.cr = 0;

		mtk_mdp_hw_set_input_addr(ctx, &src_buf_mva);
		mtk_mdp_hw_set_output_addr(ctx, &dst_buf_mva);

		mtk_mdp_hw_set_sfr_update(ctx);

		ctx->state &= ~MTK_MDP_PARAMS;
	} else {
		struct mtk_drm_plane *mtk_plane = to_mtk_plane(plane);
		struct mtk_mdp_ctx *ctx = mdp_ctx_global;

		mtk_plane->changed = false;
		if (NULL == ctx->vpu.param)
			mtk_mdp_vpu_init(&ctx->vpu);
	}

	return 0;
}

static void mtk_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);
	struct drm_crtc *crtc = state->base.crtc;
	struct drm_gem_object *gem;
	struct mtk_drm_gem_obj *mtk_gem;
	struct mtk_drm_plane *mtk_plane = to_mtk_plane(plane);
	struct drm_rect dest = {
		.x1 = state->base.crtc_x,
		.y1 = state->base.crtc_y,
		.x2 = state->base.crtc_x + state->base.crtc_w,
		.y2 = state->base.crtc_y + state->base.crtc_h,
	};
	struct drm_rect clip = { 0, };

	if (!crtc)
		return;

	clip.x2 = state->base.crtc->state->mode.hdisplay;
	clip.y2 = state->base.crtc->state->mode.vdisplay;
	drm_rect_intersect(&dest, &clip);

	gem = mtk_fb_get_gem_obj(state->base.fb);
	mtk_gem = to_mtk_gem_obj(gem);
	if (mtk_plane->changed)
		mtk_plane_enable(mtk_plane, true,
				 mtk_plane->gem[mtk_plane->gem_idx]->dma_addr,
				 &dest);
	else
		mtk_plane_enable(mtk_plane, true, mtk_gem->dma_addr, &dest);
}

static void mtk_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);

	state->pending.enable = false;
	wmb(); /* Make sure the above parameter is set before update */
	state->pending.dirty = true;
}

static const struct drm_plane_helper_funcs mtk_plane_helper_funcs = {
	.atomic_check = mtk_plane_atomic_check,
	.atomic_update = mtk_plane_atomic_update,
	.atomic_disable = mtk_plane_atomic_disable,
};

int mtk_plane_init(struct drm_device *dev, struct mtk_drm_plane *mtk_plane,
		   unsigned long possible_crtcs, enum drm_plane_type type,
		   unsigned int zpos)
{
	struct mtk_drm_private *priv = dev->dev_private;
	int err, i;

	err = drm_universal_plane_init(dev, &mtk_plane->base, possible_crtcs,
				       &mtk_plane_funcs, formats,
				       ARRAY_SIZE(formats), type);
	if (err) {
		DRM_ERROR("failed to initialize plane\n");
		return err;
	}

	drm_plane_helper_add(&mtk_plane->base, &mtk_plane_helper_funcs);
	mtk_plane->idx = zpos;

	mtk_drm_gem_create(dev, 4096, true);
	for (i = 0; i < 2; i++)
		mtk_plane->gem[i] = mtk_drm_gem_create(dev, PRIV_BUF, false);

	drm_object_attach_property(&mtk_plane->base.base,
				   dev->mode_config.rotation_property, 0);
	drm_object_attach_property(&mtk_plane->base.base, priv->alpha, 255);
	drm_object_attach_property(&mtk_plane->base.base, priv->colorkey, 0);
	drm_object_attach_property(&mtk_plane->base.base, priv->zpos, 1);

	return 0;
}
