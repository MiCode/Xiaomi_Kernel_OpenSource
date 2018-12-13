/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_PLANE_H_
#define _SDE_PLANE_H_

#include <drm/drm_crtc.h>

#include "msm_prop.h"
#include "sde_hw_mdss.h"

/**
 * struct sde_plane_state: Define sde extension of drm plane state object
 * @base:	base drm plane state object
 * @property_values:	cached plane property values
 * @property_blobs:	blob properties
 * @input_fence:	dereferenced input fence pointer
 * @stage:	assigned by crtc blender
 * @dirty:	bitmask for which pipe h/w config functions need to be updated
 * @pending:	whether the current update is still pending
 */
struct sde_plane_state {
	struct drm_plane_state base;
	uint64_t property_values[PLANE_PROP_COUNT];
	struct drm_property_blob *property_blobs[PLANE_PROP_BLOBCOUNT];
	void *input_fence;
	enum sde_stage stage;
	uint32_t dirty;
	bool pending;
};

#define to_sde_plane_state(x) \
	container_of(x, struct sde_plane_state, base)

/**
 * sde_plane_get_property - Query integer value of plane property
 * @S: Pointer to plane state
 * @X: Property index, from enum msm_mdp_plane_property
 * Returns: Integer value of requested property
 */
#define sde_plane_get_property(S, X) \
	((S) && ((X) < PLANE_PROP_COUNT) ? ((S)->property_values[(X)]) : 0)

/**
 * sde_plane_pipe - return sspp identifier for the given plane
 * @plane:   Pointer to DRM plane object
 * @index:   Plane index
 * Returns: sspp identifier of the given plane
 */
enum sde_sspp sde_plane_pipe(struct drm_plane *plane, uint32_t index);

/**
 * sde_plane_flush - final plane operations before commit flush
 * @plane: Pointer to drm plane structure
 */
void sde_plane_flush(struct drm_plane *plane);

/**
 * sde_plane_init - create new sde plane for the given pipe
 * @dev:   Pointer to DRM device
 * @pipe:  sde hardware pipe identifier
 * @primary_plane: true if this pipe is primary plane for crtc
 * @possible_crtcs: bitmask of crtc that can be attached to the given pipe
 * @vp_enabled:  Flag indicating if virtual planes enabled
 * @plane_reserved: Flag indicating the plane is occupied in bootloader
 */
struct drm_plane *sde_plane_init(struct drm_device *dev,
		uint32_t pipe, bool primary_plane,
		unsigned long possible_crtcs,
		bool vp_enabled, bool plane_reserved);

/**
 * sde_plane_wait_input_fence - wait for input fence object
 * @plane:   Pointer to DRM plane object
 * @wait_ms: Wait timeout value
 * Returns: Zero on success
 */
int sde_plane_wait_input_fence(struct drm_plane *plane, uint32_t wait_ms);

/**
 * sde_plane_color_fill - enables color fill on plane
 * @plane:  Pointer to DRM plane object
 * @color:  RGB fill color value, [23..16] Blue, [15..8] Green, [7..0] Red
 * @alpha:  8-bit fill alpha value, 255 selects 100% alpha
 * Returns: 0 on success
 */
int sde_plane_color_fill(struct drm_plane *plane,
		uint32_t color, uint32_t alpha);

/**
 * sde_plane_update_blob_property - update plane blob property
 * @plane:  Pointer to DRM plane object
 * @key:    Pointer to key string
 * @value:  Signed 32 bit integer value
 */
void sde_plane_update_blob_property(struct drm_plane *plane,
				const char *key,
				int32_t value);
#endif /* _SDE_PLANE_H_ */
