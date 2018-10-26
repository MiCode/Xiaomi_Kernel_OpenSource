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

#ifndef _EDRM_CRTC_H_
#define _EDRM_CRTC_H_

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "edrm_kms.h"

struct edrm_crtc {
	struct drm_crtc base;
	u32 sspp_flush_mask;
	int display_id;
};

#define to_edrm_crtc(x) container_of(x, struct edrm_crtc, base)

struct drm_crtc *edrm_crtc_init(struct drm_device *dev,
	struct msm_edrm_display *display, struct drm_plane *primary_plane);

/**
 * Helper function to setup the control path
 * @crtc: Pointer to drm crtc object
 */
void edrm_crtc_postinit(struct drm_crtc *crtc);

/**
 * edrm_crtc_commit_kickoff - trigger kickoff of the commit for this crtc
 * @crtc: Pointer to drm crtc object
 */
void edrm_crtc_commit_kickoff(struct drm_crtc *crtc);

/**
 * edrm_crtc_complete_commit - callback to prepare for output fences
 * @crtc: Pointer to drm crtc object
 * @old_state: Pointer to drm crtc old state object
 */
void edrm_crtc_complete_commit(struct drm_crtc *crtc,
				struct drm_crtc_state *old_state);

void edrm_crtc_prepare_commit(struct drm_crtc *crtc,
				struct drm_crtc_state *old_state);

/**
 * edrm_crtc_destroy - free up edrm_crtc structure
 * @crtc: Pointer to drm crtc object
 */
void edrm_crtc_destroy(struct drm_crtc *crtc);

#endif /* _EDRM_ENCODER_H_ */
