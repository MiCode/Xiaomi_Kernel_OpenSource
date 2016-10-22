/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef __SDE_ENCODER_H__
#define __SDE_ENCODER_H__

#include <drm/drm_crtc.h>

#include "msm_prop.h"
#include "sde_hw_mdss.h"

/**
 * Encoder functions and data types
 * @intfs:	Interfaces this encoder is using, INTF_MODE_NONE if unused
 * @wbs:	Writebacks this encoder is using, INTF_MODE_NONE if unused
 * @needs_cdm:	Encoder requests a CDM based on pixel format conversion needs
 * @display_num_of_h_tiles:
 */
struct sde_encoder_hw_resources {
	enum sde_intf_mode intfs[INTF_MAX];
	enum sde_intf_mode wbs[WB_MAX];
	bool needs_cdm;
	u32 display_num_of_h_tiles;
};

/**
 * sde_encoder_get_hw_resources - Populate table of required hardware resources
 * @encoder:	encoder pointer
 * @hw_res:	resource table to populate with encoder required resources
 * @conn_state:	report hw reqs based on this proposed connector state
 */
void sde_encoder_get_hw_resources(struct drm_encoder *encoder,
		struct sde_encoder_hw_resources *hw_res,
		struct drm_connector_state *conn_state);

/**
 * sde_encoder_register_vblank_callback - provide callback to encoder that
 *	will be called on the next vblank.
 * @encoder:	encoder pointer
 * @cb:		callback pointer, provide NULL to deregister and disable IRQs
 * @data:	user data provided to callback
 */
void sde_encoder_register_vblank_callback(struct drm_encoder *encoder,
		void (*cb)(void *), void *data);

/**
 * sde_encoder_schedule_kickoff - Register a callback with the encoder to
 *	trigger a double buffer flip of the ctl path (i.e. ctl flush and start)
 *	at the appropriate time.
 *	Immediately: if no previous commit is outstanding.
 *	Delayed: Save the callback, and return. Does not block. Callback will
 *	be triggered later. E.g. cmd encoder will trigger at pp_done irq
 *	irq if it outstanding.
 * @encoder:	encoder pointer
 */
void sde_encoder_schedule_kickoff(struct drm_encoder *encoder);

/**
 * sde_encoder_wait_nxt_committed - Wait for hardware to have flushed the
 *	current pending frames to hardware at a vblank or ctl_start
 *	Encoders will map this differently depending on irqs
 *	vid mode -> vsync_irq
 * @encoder:	encoder pointer
 * Returns: 0 on success, -EWOULDBLOCK if already signaled, error otherwise
 */
int sde_encoder_wait_for_commit_done(struct drm_encoder *drm_encoder);

/**
 * sde_encoder_init - initialize virtual encoder object
 * @dev:        Pointer to drm device structure
 * @disp_info:  Pointer to display information structure
 * Returns:     Pointer to newly created drm encoder
 */
struct drm_encoder *sde_encoder_init(
		struct drm_device *dev,
		struct msm_display_info *disp_info);

/**
 * sde_encoder_destroy - destroy previously initialized virtual encoder
 * @drm_enc:    Pointer to previously created drm encoder structure
 */
void sde_encoder_destroy(struct drm_encoder *drm_enc);

#endif /* __SDE_ENCODER_H__ */
