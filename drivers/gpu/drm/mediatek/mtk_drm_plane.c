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
#include <linux/mailbox_controller.h>

#include "mtk_drm_crtc.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_fb.h"
#include "mtk_drm_gem.h"
#include "mtk_drm_plane.h"
#include "cmdq-sec.h"

#define MTK_DRM_PLANE_SCALING_MIN 16
#define MTK_DRM_PLANE_SCALING_MAX (1 << 16)

static const u32 formats[] = {
	DRM_FORMAT_C8,       DRM_FORMAT_XRGB8888, DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888, DRM_FORMAT_ABGR8888, DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGBX8888, DRM_FORMAT_BGRA8888, DRM_FORMAT_RGBA8888,
	DRM_FORMAT_BGR888,   DRM_FORMAT_RGB888,   DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB565,   DRM_FORMAT_YUYV,     DRM_FORMAT_YVYU,
	DRM_FORMAT_UYVY,     DRM_FORMAT_VYUY,     DRM_FORMAT_ABGR2101010,
	DRM_FORMAT_ABGR16161616F,
};

unsigned int to_crtc_plane_index(unsigned int plane_index)
{
	if ((plane_index >= 0) && (plane_index < OVL_LAYER_NR))
		return plane_index;
	else if (plane_index < (OVL_LAYER_NR + EXTERNAL_INPUT_LAYER_NR))
		return plane_index - OVL_LAYER_NR;
	else if (plane_index < MAX_PLANE_NR)
		return plane_index - OVL_LAYER_NR - EXTERNAL_INPUT_LAYER_NR;
	else
		return 0;
}

int mtk_get_format_bpp(uint32_t format)
{
	switch (format) {
	case MTK_DRM_FORMAT_DIM:
	case DRM_FORMAT_C8:
		return 0;
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		return 2;
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		return 3;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
		return 4;
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
		return 2;
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		return 3;
	case DRM_FORMAT_ABGR16161616F:
		return 8;
	default:
		return 4;
	}
}

char *mtk_get_format_name(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_C8:
		return "C8";
	case DRM_FORMAT_XRGB8888:
		return "XRGB8888";
	case DRM_FORMAT_XBGR8888:
		return "XBGR8888";
	case DRM_FORMAT_ARGB8888:
		return "ARGB8888";
	case DRM_FORMAT_ABGR8888:
		return "ABGR8888";
	case DRM_FORMAT_BGRX8888:
		return "BGRX8888";
	case DRM_FORMAT_RGBX8888:
		return "RGBX8888";
	case DRM_FORMAT_BGRA8888:
		return "BGRA8888";
	case DRM_FORMAT_RGBA8888:
		return "RGBA8888";
	case DRM_FORMAT_BGR888:
		return "BGR888";
	case DRM_FORMAT_RGB888:
		return "RGB888";
	case DRM_FORMAT_BGR565:
		return "BGR565";
	case DRM_FORMAT_RGB565:
		return "RGB565";
	case DRM_FORMAT_YUYV:
		return "YUYV";
	case DRM_FORMAT_YVYU:
		return "YVYU";
	case DRM_FORMAT_UYVY:
		return "UYVY";
	case DRM_FORMAT_VYUY:
		return "VYUY";
	case DRM_FORMAT_ABGR2101010:
		return "ABGR2101010";
	case DRM_FORMAT_ABGR16161616F:
		return "ABGRFP16";
	}
	return "fmt_unknown";
}

static struct mtk_drm_property mtk_plane_property[PLANE_PROP_MAX] = {
	{DRM_MODE_PROP_ATOMIC, "NEXT_BUFF_IDX", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "LYE_BLOB_IDX", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "PLANE_PROP_ALPHA_CON", 0, 0x1, 0x1},
	{DRM_MODE_PROP_ATOMIC, "PLANE_PROP_PLANE_ALPHA", 0, 0xFF, 0xFF},
	{DRM_MODE_PROP_ATOMIC, "DATASPACE", 0, INT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "VPITCH", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "COMPRESS", 0, UINT_MAX, 0},
	{DRM_MODE_PROP_ATOMIC, "DIM_COLOR", 0, UINT_MAX, 0},
};

static void mtk_plane_reset(struct drm_plane *plane)
{
	struct mtk_plane_state *state;

	if (plane->state) {
		__drm_atomic_helper_plane_destroy_state(plane->state);

		state = to_mtk_plane_state(plane->state);
		memset(state, 0, sizeof(*state));
	} else {
		state = kzalloc(sizeof(*state), GFP_KERNEL);
		if (!state)
			return;
		plane->state = &state->base;
	}
	state->prop_val[PLANE_PROP_ALPHA_CON] = 0x1;
	state->prop_val[PLANE_PROP_PLANE_ALPHA] = 0xFF;
	state->base.plane = plane;
	state->pending.format = DRM_FORMAT_RGB565;
}

static struct drm_plane_state *
mtk_plane_duplicate_state(struct drm_plane *plane)
{
	struct mtk_plane_state *old_state = to_mtk_plane_state(plane->state);
	struct mtk_plane_state *state;

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_plane_duplicate_state(plane, &state->base);

	if (state->base.plane != plane)
		DDPAEE("%s:%d, invalid plane:(%p,%p)\n",
			__func__, __LINE__,
			state->base.plane, plane);

	state->prop_val[PLANE_PROP_ALPHA_CON] =
		old_state->prop_val[PLANE_PROP_ALPHA_CON];
	state->prop_val[PLANE_PROP_PLANE_ALPHA] =
		old_state->prop_val[PLANE_PROP_PLANE_ALPHA];
	state->pending = old_state->pending;
	state->comp_state = old_state->comp_state;
	state->crtc = old_state->crtc;

	return &state->base;
}

static void mtk_drm_plane_destroy_state(struct drm_plane *plane,
					struct drm_plane_state *state)
{
	struct mtk_plane_state *s;

	s = to_mtk_plane_state(state);
	__drm_atomic_helper_plane_destroy_state(state);
	kfree(s);
}

static int mtk_plane_atomic_set_property(struct drm_plane *plane,
					 struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t val)
{
	struct mtk_drm_plane *mtk_plane = to_mtk_plane(plane);
	struct mtk_plane_state *plane_state = to_mtk_plane_state(state);
	int ret = 0;
	int i;

	if (!mtk_plane) {
		DDPPR_ERR("%s:%d mtk plane is null\n", __func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < PLANE_PROP_MAX; i++) {
		if (mtk_plane->plane_property[i] == property) {
			plane_state->prop_val[i] = (unsigned int)val;
			DDPDBG("set property:%s %d\n", property->name,
				(unsigned int)val);
			return ret;
		}
	}

	DDPPR_ERR("%s:%d fail to set property:%s %d\n", property->name,
		  (unsigned int)val, __func__, __LINE__);
	return -EINVAL;
}

static int mtk_plane_atomic_get_property(struct drm_plane *plane,
					 const struct drm_plane_state *state,
					 struct drm_property *property,
					 uint64_t *val)
{
	struct mtk_drm_plane *mtk_plane = to_mtk_plane(plane);
	struct mtk_plane_state *plane_state = to_mtk_plane_state(state);
	int ret = 0;
	int i;

	if (!mtk_plane) {
		DDPPR_ERR("%s:%d mtk plane is null\n", __func__, __LINE__);
		return -EINVAL;
	}

	for (i = 0; i < PLANE_PROP_MAX; i++) {
		if (mtk_plane->plane_property[i] == property) {
			*val = plane_state->prop_val[i];
			DDPINFO("get property:%s %lld\n", property->name, *val);
			return ret;
		}
	}

	DDPPR_ERR("%s:%d fail to get property:%s %p\n", __func__, __LINE__,
			property->name, val);

	return -EINVAL;
}

static const struct drm_plane_funcs mtk_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = mtk_plane_reset,
	.atomic_duplicate_state = mtk_plane_duplicate_state,
	.atomic_destroy_state = mtk_drm_plane_destroy_state,
	.atomic_set_property = mtk_plane_atomic_set_property,
	.atomic_get_property = mtk_plane_atomic_get_property,
};

static int mtk_plane_atomic_check(struct drm_plane *plane,
				  struct drm_plane_state *state)
{
	struct drm_framebuffer *fb = state->fb;
	struct drm_crtc_state *crtc_state;
	struct drm_rect clip = {
		0,
	};
	struct mtk_drm_private *private = plane->dev->dev_private;

	if (!fb)
		return 0;

	if (!mtk_fb_get_gem_obj(fb) && fb->format->format != DRM_FORMAT_C8) {
		DRM_DEBUG_KMS("buffer is null\n");
		return -EFAULT;
	}

	if (!state->crtc)
		return 0;

	crtc_state = drm_atomic_get_crtc_state(state->state, state->crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	clip.x2 = crtc_state->mode.hdisplay;
	clip.y2 = crtc_state->mode.vdisplay;

	if (mtk_drm_helper_get_opt(private->helper_opt, MTK_DRM_OPT_RPO))
		return drm_plane_helper_check_state(
			state, &clip, MTK_DRM_PLANE_SCALING_MIN,
			MTK_DRM_PLANE_SCALING_MAX, true, true);
	else
		return drm_plane_helper_check_state(
			state, &clip, DRM_PLANE_HELPER_NO_SCALING,
			DRM_PLANE_HELPER_NO_SCALING, true, true);
}

#ifdef MTK_DRM_ADVANCE
static void _mtk_plane_get_comp_state(struct mtk_drm_lyeblob_ids *lyeblob_ids,
				      struct mtk_plane_comp_state *comp_state,
				      struct drm_crtc *crtc,
				      unsigned int plane_index)
{
	int blob_id;
	int ref_cnt;
	struct drm_property_blob *blob;

	blob_id = lyeblob_ids->lye_plane_blob_id[crtc->index][plane_index];
	ref_cnt = lyeblob_ids->ref_cnt;
	if (blob_id && ref_cnt) {
		blob = drm_property_lookup_blob(crtc->dev, blob_id);
		if (blob) {
			memcpy(comp_state, blob->data,
			       sizeof(struct mtk_plane_comp_state));
			drm_property_unreference_blob(blob);
		}
	}
}

void mtk_plane_get_comp_state(struct drm_plane *plane,
			      struct mtk_plane_comp_state *comp_state,
			      struct drm_crtc *crtc, int lock)
{
	struct mtk_drm_lyeblob_ids *lyeblob_ids, *next;
	struct mtk_drm_private *mtk_drm = crtc->dev->dev_private;
	struct mtk_crtc_state *crtc_state = to_mtk_crtc_state(crtc->state);
	unsigned int crtc_lye_idx = crtc_state->prop_val[CRTC_PROP_LYE_IDX];
	unsigned int plane_index = to_crtc_plane_index(plane->index);

	memset(comp_state, 0x0, sizeof(struct mtk_plane_comp_state));

	if (lock)
		mutex_lock(&mtk_drm->lyeblob_list_mutex);
	list_for_each_entry_safe(lyeblob_ids, next, &mtk_drm->lyeblob_head,
				 list) {
		if (lyeblob_ids->lye_idx == crtc_lye_idx) {
			_mtk_plane_get_comp_state(lyeblob_ids, comp_state, crtc,
						  plane_index);
		} else if (lyeblob_ids->lye_idx > crtc_lye_idx)
			break;
	}

	if (lock)
		mutex_unlock(&mtk_drm->lyeblob_list_mutex);
}
#endif

static void mtk_plane_atomic_update(struct drm_plane *plane,
				    struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);
	struct drm_crtc *crtc = plane->state->crtc;
	struct mtk_crtc_state *crtc_state;
	struct drm_framebuffer *fb = plane->state->fb;
	int src_w, src_h, dst_x, dst_y, dst_w, dst_h, i;
	struct mtk_drm_crtc *mtk_crtc;
	unsigned int plane_index = to_crtc_plane_index(plane->index);
	static int cnt;
	bool skip_update = 0;
	int crtc_index = 0;

	if (!crtc)
		return;

	crtc_state = to_mtk_crtc_state(crtc->state);
	mtk_crtc = to_mtk_crtc(crtc);
	crtc_index = drm_crtc_index(crtc);

	if ((!fb) || (mtk_crtc->ddp_mode == DDP_NO_USE))
		return;

	src_w = drm_rect_width(&plane->state->src) >> 16;
	src_h = drm_rect_height(&plane->state->src) >> 16;
	dst_x = plane->state->dst.x1;
	dst_y = plane->state->dst.y1;
	dst_w = drm_rect_width(&plane->state->dst);
	dst_h = drm_rect_height(&plane->state->dst);

	if (src_w < dst_w || src_h < dst_h) {
		dst_x = ((dst_x * src_w * 10) / dst_w + 5) / 10
			- crtc_state->rsz_src_roi.x;
		dst_y = ((dst_y * src_h * 10) / dst_h + 5) / 10
			- crtc_state->rsz_src_roi.y;
		dst_w = src_w;
		dst_h = src_h;
	}

	state->pending.enable = plane->state->visible;
	state->pending.pitch = fb->pitches[0];
	state->pending.format = fb->format->format;
	state->pending.modifier = fb->modifier[0];
	state->pending.addr = mtk_fb_get_dma(fb);
	state->pending.size = mtk_fb_get_size(fb);
	state->pending.src_x = (plane->state->src.x1 >> 16);
	state->pending.src_y = (plane->state->src.y1 >> 16);
	state->pending.dst_x = dst_x;
	state->pending.dst_y = dst_y;
	state->pending.width = dst_w;
	state->pending.height = dst_h;
	if (mtk_drm_fb_is_secure(fb))
		state->pending.is_sec = true;
	else
		state->pending.is_sec = false;
	for (i = 0; i < PLANE_PROP_MAX; i++)
		state->pending.prop_val[i] = state->prop_val[i];

	wmb(); /* Make sure the above parameters are set before update */
	state->pending.dirty = true;

	DDPINFO("%s:%d enable%d,pitch%d,format%d\n",
		__func__, __LINE__, (unsigned int)state->pending.enable,
		state->pending.pitch, state->pending.format);
	DDPINFO("addr0x%lx,x%d,y%d,width%d,height%d\n",
		(unsigned long)state->pending.addr, state->pending.dst_x,
		state->pending.dst_y, state->pending.width,
		state->pending.height);

	for (i = 0; i < PLANE_PROP_MAX; i++) {
		DDPINFO("prop_val[%d]:%d ", i,
			(unsigned int)state->pending.prop_val[i]);
	}
	DDPINFO("\n");

	DDPFENCE("S+/%sL%d/e%d/id%d/mva0x%08llx/size0x%08lx/S%d\n",
		mtk_crtc_index_spy(crtc_index),
		plane_index,
		state->pending.enable,
		state->pending.prop_val[PLANE_PROP_NEXT_BUFF_IDX],
		state->pending.addr,
		state->pending.size,
		state->pending.is_sec);

	if (!mtk_crtc->sec_on && state->pending.is_sec) {
		DDPMSG("receive sec buffer in non-sec mode\n");
		return;
	}

	if (state->pending.enable)
		atomic_set(&mtk_crtc->already_config, 1);

	if (cnt <= 5) {
		cnt++;
		if (state->pending.format == DRM_FORMAT_RGB565 &&
			drm_crtc_index(crtc) == 0)
			skip_update = 1;
	}

	/* workaround for skip plane update when hwc set crtc */
	if (skip_update == 0)
		mtk_drm_crtc_plane_update(crtc, plane, state);

}

static void mtk_plane_atomic_disable(struct drm_plane *plane,
				     struct drm_plane_state *old_state)
{
	struct mtk_plane_state *state = to_mtk_plane_state(plane->state);

	state->pending.enable = false;
	wmb(); /* Make sure the above parameter is set before update */
	state->pending.dirty = true;

#ifdef MTK_DRM_ADVANCE
	if (!state->crtc) {
		DDPPR_ERR("%s, empty crtc state\n", __func__);
	} else {
		mtk_drm_crtc_plane_update(state->crtc, plane, state);
	}
#endif
}

static const struct drm_plane_helper_funcs mtk_plane_helper_funcs = {
	.atomic_check = mtk_plane_atomic_check,
	.atomic_update = mtk_plane_atomic_update,
	.atomic_disable = mtk_plane_atomic_disable,
};

static void mtk_plane_attach_property(struct mtk_drm_plane *plane)
{
	struct drm_device *dev = plane->base.dev;
	struct drm_property *prop;
	static struct drm_property *mtk_prop[PLANE_PROP_MAX];
	struct mtk_drm_property *plane_prop;
	int i;
	static int num;

	if (num == 0) {
		for (i = 0; i < PLANE_PROP_MAX; i++) {
			plane_prop = &(mtk_plane_property[i]);
			mtk_prop[i] = drm_property_create_range(
				dev, plane_prop->flags, plane_prop->name,
				plane_prop->min, plane_prop->max);
			if (!mtk_prop[i]) {
				DDPPR_ERR("fail to create property:%s\n",
					  plane_prop->name);
				return;
			}
			DDPINFO("create property:%s, flags:0x%x\n",
				plane_prop->name, mtk_prop[i]->flags);
		}
		num++;
	}

	for (i = 0; i < PLANE_PROP_MAX; i++) {
		prop = plane->plane_property[i];
		plane_prop = &(mtk_plane_property[i]);
		if (!prop) {
			prop = mtk_prop[i];
			plane->plane_property[i] = prop;
			drm_object_attach_property(&plane->base.base, prop,
						   plane_prop->val);
		}
	}
}

int mtk_plane_init(struct drm_device *dev, struct mtk_drm_plane *plane,
		   unsigned int zpos, unsigned long possible_crtcs,
		   enum drm_plane_type type)
{
	int err;

	err = drm_universal_plane_init(dev, &plane->base, possible_crtcs,
				       &mtk_plane_funcs, formats,
				       ARRAY_SIZE(formats), NULL,
				       type, NULL);
	if (err) {
		DRM_ERROR("%s:%d failed to initialize plane\n", __func__,
			  __LINE__);
		return err;
	}

	drm_plane_helper_add(&plane->base, &mtk_plane_helper_funcs);

	mtk_plane_attach_property(plane);

	return 0;
}
