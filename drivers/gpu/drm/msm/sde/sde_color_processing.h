
/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_COLOR_PROCESSING_H
#define _SDE_COLOR_PROCESSING_H
#include <drm/drm_crtc.h>

/**
 * sde_cp_crtc_init(): Initialize color processing lists for a crtc.
 *                     Should be called during crtc initialization.
 * @crtc:  Pointer to sde_crtc.
 */
void sde_cp_crtc_init(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_install_properties(): Installs the color processing
 *                                properties for a crtc.
 *                                Should be called during crtc initialization.
 * @crtc:  Pointer to crtc.
 */
void sde_cp_crtc_install_properties(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_destroy_properties: Destroys color processing
 *                                            properties for a crtc.
 * should be called during crtc de-initialization.
 * @crtc:  Pointer to crtc.
 */
void sde_cp_crtc_destroy_properties(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_set_property: Set a color processing property
 *                                      for a crtc.
 *                                      Should be during atomic set property.
 * @crtc: Pointer to crtc.
 * @property: Property that needs to enabled/disabled.
 * @val: Value of property.
 */
int sde_cp_crtc_set_property(struct drm_crtc *crtc,
				struct drm_property *property, uint64_t val);

/**
 * sde_cp_crtc_apply_properties: Enable/disable properties
 *                               for a crtc.
 *                               Should be called during atomic commit call.
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_apply_properties(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_get_property: Get value of color processing property
 *                                      for a crtc.
 *                                      Should be during atomic get property.
 * @crtc: Pointer to crtc.
 * @property: Property that needs to enabled/disabled.
 * @val: Value of property.
 *
 */
int sde_cp_crtc_get_property(struct drm_crtc *crtc,
				struct drm_property *property, uint64_t *val);
#endif /*_SDE_COLOR_PROCESSING_H */
