/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SDE_ENCODER_H__
#define __SDE_ENCODER_H__

#include <drm/drm_crtc.h>

#include "msm_prop.h"
#include "sde_hw_mdss.h"

#define SDE_ENCODER_FRAME_EVENT_DONE		BIT(0)
#define SDE_ENCODER_FRAME_EVENT_ERROR		BIT(1)
#define SDE_ENCODER_FRAME_EVENT_PANEL_DEAD	BIT(2)

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
 * sde_encoder_register_frame_event_callback - provide callback to encoder that
 *	will be called after the request is complete, or other events.
 * @encoder:	encoder pointer
 * @cb:		callback pointer, provide NULL to deregister
 * @data:	user data provided to callback
 */
void sde_encoder_register_frame_event_callback(struct drm_encoder *encoder,
		void (*cb)(void *, u32), void *data);

/**
 * sde_encoder_update_rsc_client - updates the rsc client state for primary
 *      for primary display.
 * @encoder:	encoder pointer
 * @enable:	enable/disable the client
 */
struct sde_rsc_client *sde_encoder_update_rsc_client(
		struct drm_encoder *encoder, bool enable);

/**
 * sde_encoder_prepare_for_kickoff - schedule double buffer flip of the ctl
 *	path (i.e. ctl flush and start) at next appropriate time.
 *	Immediately: if no previous commit is outstanding.
 *	Delayed: Block until next trigger can be issued.
 * @encoder:	encoder pointer
 */
void sde_encoder_prepare_for_kickoff(struct drm_encoder *encoder);

/**
 * sde_encoder_kickoff - trigger a double buffer flip of the ctl path
 *	(i.e. ctl flush and start) immediately.
 * @encoder:	encoder pointer
 */
void sde_encoder_kickoff(struct drm_encoder *encoder);

/**
 * sde_encoder_wait_nxt_committed - Wait for hardware to have flushed the
 *	current pending frames to hardware at a vblank or ctl_start
 *	Encoders will map this differently depending on irqs
 *	vid mode -> vsync_irq
 * @encoder:	encoder pointer
 * Returns: 0 on success, -EWOULDBLOCK if already signaled, error otherwise
 */
int sde_encoder_wait_for_commit_done(struct drm_encoder *drm_encoder);

/*
 * sde_encoder_get_intf_mode - get interface mode of the given encoder
 * @encoder: Pointer to drm encoder object
 */
enum sde_intf_mode sde_encoder_get_intf_mode(struct drm_encoder *encoder);

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
