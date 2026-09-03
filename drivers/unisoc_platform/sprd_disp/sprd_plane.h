/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_PLANE_H_
#define _SPRD_PLANE_H_

#include <drm/drm_plane.h>

#define to_sprd_plane(x)		container_of(x, struct sprd_plane, base)
#define to_sprd_plane_state(x)	container_of(x, struct sprd_plane_state, base)

struct sprd_layer_state {
	u8 index;
	u8 planes;
	u32 addr[4];
	u32 pitch[4];
	s16 src_x;
	s16 src_y;
	s16 src_w;
	s16 src_h;
	s16 dst_x;
	s16 dst_y;
	u16 dst_w;
	u16 dst_h;
	u32 format;
	u32 alpha;
	u32 blending;
	u32 rotation;
	u32 xfbc;
	u32 fbc_hsize_r;
	u32 fbc_hsize_y;
	u32 fbc_hsize_uv;
	u32 y2r_coef;
	u8 pallete_en;
	u32 pallete_color;
	u32 secure_en;
};

struct sprd_plane_state {
	struct drm_plane_state base;
	struct sprd_layer_state layer;
};

struct sprd_plane {
	struct drm_plane base;
	struct drm_property *fbc_enabled_property;
	struct drm_property *fbc_hsize_r_property;
	struct drm_property *fbc_hsize_y_property;
	struct drm_property *fbc_hsize_uv_property;
	struct drm_property *y2r_coef_property;
	struct drm_property *pallete_en_property;
	struct drm_property *pallete_color_property;
	struct drm_property *secure_en_property;
	u32 index;
};

struct sprd_plane *sprd_plane_init(struct drm_device *drm,
					struct sprd_crtc_capability *cap);

#endif /* _SPRD_PLANE_H_ */
