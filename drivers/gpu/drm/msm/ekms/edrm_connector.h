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

#ifndef _EDRM_CONNECTOR_H_
#define _EDRM_CONNECTOR_H_

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "edrm_kms.h"

struct drm_connector *edrm_connector_init(struct drm_device *dev,
			struct drm_encoder *encoder,
			struct msm_edrm_display *display);

void edrm_connector_destroy(struct drm_connector *connector);

#endif /* _EDRM_CONNECTOR_H_ */
