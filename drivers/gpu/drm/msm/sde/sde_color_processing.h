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

struct sde_irq_callback;

/*
 * PA MEMORY COLOR types
 * @MEMCOLOR_SKIN          Skin memory color type
 * @MEMCOLOR_SKY           Sky memory color type
 * @MEMCOLOR_FOLIAGE       Foliage memory color type
 */
enum sde_memcolor_type {
	MEMCOLOR_SKIN = 0,
	MEMCOLOR_SKY,
	MEMCOLOR_FOLIAGE,
	MEMCOLOR_MAX
};

/*
 * PA HISTOGRAM modes
 * @HIST_DISABLED          Histogram disabled
 * @HIST_ENABLED           Histogram enabled
 */
enum sde_hist_modes {
	HIST_DISABLED,
	HIST_ENABLED
};

/**
 * struct drm_prop_enum_list - drm structure for creating enum property and
 *                             enumerating values
 */
static const struct drm_prop_enum_list sde_hist_modes[] = {
	{HIST_DISABLED, "hist_off"},
	{HIST_ENABLED, "hist_on"},
};

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

/**
 * sde_cp_crtc_suspend: Suspend the crtc features
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_suspend(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_resume: Resume the crtc features
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_resume(struct drm_crtc *crtc);

/**
 * sde_cp_ad_interrupt: Api to enable/disable ad interrupt
 * @crtc: Pointer to crtc.
 * @en: Variable to enable/disable interrupt.
 * @irq: Pointer to irq callback
 */
int sde_cp_ad_interrupt(struct drm_crtc *crtc, bool en,
		struct sde_irq_callback *irq);

/**
 * sde_cp_crtc_pre_ipc: Handle color processing features
 *                      before entering IPC
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_pre_ipc(struct drm_crtc *crtc);

/**
 * sde_cp_crtc_post_ipc: Handle color processing features
 *                       after exiting IPC
 * @crtc: Pointer to crtc.
 */
void sde_cp_crtc_post_ipc(struct drm_crtc *crtc);

/**
 * sde_cp_hist_interrupt: Api to enable/disable histogram interrupt
 * @crtc: Pointer to crtc.
 * @en: Variable to enable/disable interrupt.
 * @irq: Pointer to irq callback
 */
int sde_cp_hist_interrupt(struct drm_crtc *crtc_drm, bool en,
	struct sde_irq_callback *hist_irq);
#endif /*_SDE_COLOR_PROCESSING_H */
