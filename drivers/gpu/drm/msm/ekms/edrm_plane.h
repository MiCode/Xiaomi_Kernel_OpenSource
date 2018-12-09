/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _EDRM_PLANE_H_
#define _EDRM_PLANE_H_

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "edrm_kms.h"

struct edrm_plane {
	struct drm_plane base;
	struct msm_gem_address_space *aspace;
	int pipe;
	int display_id;
	u32 sspp_offset;
	u32 sspp_cfg_id;
	u32 lm_stage;
	u32 sspp_type;
};

int edrm_plane_flush(struct drm_plane *plane);

struct drm_plane *edrm_plane_init(struct drm_device *dev, int id, u32 type);

void edrm_plane_destroy(struct drm_plane *plane);

#define to_edrm_plane(x) container_of(x, struct edrm_plane, base)

#endif /* _EDRM_ENCODER_H_ */
