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

#ifndef _EDRM_ENCODER_H_
#define _EDRM_ENCODER_H_

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "edrm_kms.h"

struct edrm_encoder {
	struct drm_encoder base;
	u32 sspp_mask;
	int intf_idx;
};

#define to_edrm_encoder(x) container_of(x, struct edrm_encoder, base)

/**
 * edrm_encoder_wait_for_commit_done - wait until the register flush is done
 * @drm_enc: Pointer to drm_encoder object
 */
int edrm_encoder_wait_for_commit_done(struct drm_encoder *drm_enc);

/**
 * edrm_encoder_destroy - free up drm_encoder object
 * @drm_enc: Pointer to drm encoder object
 */
void edrm_encoder_destroy(struct drm_encoder *encoder);

/**
 * edrm_encoder_init - create drm_encoder object
 * @dev: drm_device that this encoder going to register.
 * @display: display structure that associate with this encoder.
 */
struct drm_encoder *edrm_encoder_init(struct drm_device *dev,
					struct msm_edrm_display *display);

#endif /* _EDRM_ENCODER_H_ */
