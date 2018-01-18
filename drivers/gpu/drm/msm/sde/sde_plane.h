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

#ifndef _SDE_PLANE_H_
#define _SDE_PLANE_H_

#include <drm/drm_crtc.h>

#include "msm_prop.h"
#include "sde_kms.h"
#include "sde_hw_mdss.h"
#include "sde_hw_rot.h"
#include "sde_hw_sspp.h"

/**
 * struct sde_plane_rot_state - state of pre-sspp rotator stage
 * @sequence_id: sequence identifier, incremented per state duplication
 * @rot_hw: Pointer to rotator hardware driver
 * @rot90: true if rotation of 90 degree is required
 * @hflip: true if horizontal flip is required
 * @vflip: true if vertical flip is required
 * @rot_cmd: rotator configuration command
 * @nplane: total number of drm plane attached to rotator
 * @in_fb: input fb attached to rotator
 * @in_rotation: input rotation property of rotator stage
 * @in_rot_rect: input rectangle of the rotator in plane fb coordinate
 * @out_rotation: output rotation property of rotator stage
 * @out_rot_rect: output rectangle of the rotator in plane fb coordinate
 * @out_src_rect: output rectangle of the plane source in plane fb coordinate
 * @out_src_x: output src_x of rotator stage in rotator output fb coordinate
 * @out_src_y: output src y of rotator stage in rotator output fb coordinate
 * @out_src_w: output src w of rotator stage in rotator output fb ooordinate
 * @out_src_h: output src h of rotator stage in rotator output fb coordinate
 * @out_fb_width: output framebuffer width of rotator stage
 * @out_fb_height: output framebuffer height of rotator stage
 * @out_fb_pixel_format: output framebuffer pixel format of rotator stage
 * @out_fb_modifier: output framebuffer modifier of rotator stage
 * @out_fb_flags: output framebuffer flags of rotator stage
 * @out_sbuf: true if output streaming buffer is required
 * @out_fb_format: Pointer to output framebuffer format of rotator stage
 * @out_fb: Pointer to output drm framebuffer of rotator stage
 * @out_fbo: framebuffer object of output streaming buffer
 * @out_xpos: relative horizontal position of the plane (0 - leftmost)
 */
struct sde_plane_rot_state {
	u32 sequence_id;
	struct sde_hw_rot *rot_hw;
	bool rot90;
	bool hflip;
	bool vflip;
	struct sde_hw_rot_cmd rot_cmd;
	int nplane;
	/* input */
	struct drm_framebuffer *in_fb;
	struct drm_rect in_rot_rect;
	u32 in_rotation;
	/* output */
	struct drm_rect out_rot_rect;
	struct drm_rect out_src_rect;
	u32 out_rotation;
	u32 out_src_x;
	u32 out_src_y;
	u32 out_src_w;
	u32 out_src_h;
	u32 out_fb_width;
	u32 out_fb_height;
	u32 out_fb_pixel_format;
	u64 out_fb_modifier[4];
	u32 out_fb_flags;
	bool out_sbuf;
	const struct sde_format *out_fb_format;
	struct drm_framebuffer *out_fb;
	struct sde_kms_fbo *out_fbo;
	int out_xpos;
};

/* dirty bits for update function */
#define SDE_PLANE_DIRTY_RECTS	0x1
#define SDE_PLANE_DIRTY_FORMAT	0x2
#define SDE_PLANE_DIRTY_SHARPEN	0x4
#define SDE_PLANE_DIRTY_PERF	0x8
#define SDE_PLANE_DIRTY_FB_TRANSLATION_MODE	0x10
#define SDE_PLANE_DIRTY_ALL	0xFFFFFFFF

/**
 * enum sde_plane_sclcheck_state - User scaler data status
 *
 * @SDE_PLANE_SCLCHECK_NONE: No user data provided
 * @SDE_PLANE_SCLCHECK_INVALID: Invalid user data provided
 * @SDE_PLANE_SCLCHECK_SCALER_V1: Valid scaler v1 data
 * @SDE_PLANE_SCLCHECK_SCALER_V1_CHECK: Unchecked scaler v1 data
 * @SDE_PLANE_SCLCHECK_SCALER_V2: Valid scaler v2 data
 * @SDE_PLANE_SCLCHECK_SCALER_V2_CHECK: Unchecked scaler v2 data
 */
enum sde_plane_sclcheck_state {
	SDE_PLANE_SCLCHECK_NONE,
	SDE_PLANE_SCLCHECK_INVALID,
	SDE_PLANE_SCLCHECK_SCALER_V1,
	SDE_PLANE_SCLCHECK_SCALER_V1_CHECK,
	SDE_PLANE_SCLCHECK_SCALER_V2,
	SDE_PLANE_SCLCHECK_SCALER_V2_CHECK,
};

/**
 * struct sde_plane_state: Define sde extension of drm plane state object
 * @base:	base drm plane state object
 * @property_state: Local storage for msm_prop properties
 * @property_values:	cached plane property values
 * @aspace:	pointer to address space for input/output buffers
 * @input_fence:	dereferenced input fence pointer
 * @stage:	assigned by crtc blender
 * @excl_rect:	exclusion rect values
 * @dirty:	bitmask for which pipe h/w config functions need to be updated
 * @multirect_index: index of the rectangle of SSPP
 * @multirect_mode: parallel or time multiplex multirect mode
 * @const_alpha_en: const alpha channel is enabled for this HW pipe
 * @pending:	whether the current update is still pending
 * @defer_prepare_fb:	indicate if prepare_fb call was deferred
 * @scaler3_cfg: configuration data for scaler3
 * @pixel_ext: configuration data for pixel extensions
 * @scaler_check_state: indicates status of user provided pixel extension data
 * @cdp_cfg:	CDP configuration
 */
struct sde_plane_state {
	struct drm_plane_state base;
	struct msm_property_state property_state;
	struct msm_property_value property_values[PLANE_PROP_COUNT];
	struct msm_gem_address_space *aspace;
	void *input_fence;
	enum sde_stage stage;
	struct sde_rect excl_rect;
	uint32_t dirty;
	uint32_t multirect_index;
	uint32_t multirect_mode;
	bool const_alpha_en;
	bool pending;
	bool defer_prepare_fb;

	/* scaler configuration */
	struct sde_hw_scaler3_cfg scaler3_cfg;
	struct sde_hw_pixel_ext pixel_ext;
	enum sde_plane_sclcheck_state scaler_check_state;

	/* @sc_cfg: system_cache configuration */
	struct sde_hw_pipe_sc_cfg sc_cfg;
	struct sde_plane_rot_state rot;

	struct sde_hw_pipe_cdp_cfg cdp_cfg;
};

/**
 * struct sde_multirect_plane_states: Defines multirect pair of drm plane states
 * @r0: drm plane configured on rect 0
 * @r1: drm plane configured on rect 1
 */
struct sde_multirect_plane_states {
	const struct drm_plane_state *r0;
	const struct drm_plane_state *r1;
};

#define to_sde_plane_state(x) \
	container_of(x, struct sde_plane_state, base)

/**
 * sde_plane_get_property - Query integer value of plane property
 * @S: Pointer to plane state
 * @X: Property index, from enum msm_mdp_plane_property
 * Returns: Integer value of requested property
 */
#define sde_plane_get_property(S, X) ((S) && ((X) < PLANE_PROP_COUNT) ? \
	((S)->property_values[(X)].value) : 0)

/**
 * sde_plane_pipe - return sspp identifier for the given plane
 * @plane:   Pointer to DRM plane object
 * Returns: sspp identifier of the given plane
 */
enum sde_sspp sde_plane_pipe(struct drm_plane *plane);

/**
 * is_sde_plane_virtual - check for virtual plane
 * @plane: Pointer to DRM plane object
 * returns: true - if the plane is virtual
 *          false - if the plane is primary
 */
bool is_sde_plane_virtual(struct drm_plane *plane);

/**
 * sde_plane_confirm_hw_rsvps - reserve an sbuf resource, if needed
 * @plane: Pointer to DRM plane object
 * @state: Pointer to plane state
 * @cstate: Pointer to crtc state containing the resource pool
 * Returns: Zero on success
 */
int sde_plane_confirm_hw_rsvps(struct drm_plane *plane,
		const struct drm_plane_state *state,
		struct drm_crtc_state *cstate);

/**
 * sde_plane_get_ctl_flush - get control flush mask
 * @plane:   Pointer to DRM plane object
 * @ctl: Pointer to control hardware
 * @flush_sspp: Pointer to sspp flush control word
 * @flush_rot: Pointer to rotator flush control word
 */
void sde_plane_get_ctl_flush(struct drm_plane *plane, struct sde_hw_ctl *ctl,
		u32 *flush_sspp, u32 *flush_rot);

/**
 * sde_plane_rot_get_prefill - calculate rotator start prefill
 * @plane: Pointer to drm plane
 * return: prefill time in lines
 */
u32 sde_plane_rot_get_prefill(struct drm_plane *plane);

/**
 * sde_plane_restore - restore hw state if previously power collapsed
 * @plane: Pointer to drm plane structure
 */
void sde_plane_restore(struct drm_plane *plane);

/**
 * sde_plane_flush - final plane operations before commit flush
 * @plane: Pointer to drm plane structure
 */
void sde_plane_flush(struct drm_plane *plane);

/**
 * sde_plane_halt_requests - control halting of vbif transactions for this plane
 *	This function isn't thread safe. Plane halt enable/disable requests
 *	should always be made from the same commit cycle.
 * @plane: Pointer to drm plane structure
 * @enable: Whether to enable/disable halting of vbif transactions
 */
void sde_plane_halt_requests(struct drm_plane *plane, bool enable);

/**
 * sde_plane_reset_rot - reset rotator operations before commit kickoff
 * @plane: Pointer to drm plane structure
 * @state: Pointer to plane state associated with reset request
 * Returns: Zero on success
 */
int sde_plane_reset_rot(struct drm_plane *plane, struct drm_plane_state *state);

/**
 * sde_plane_kickoff_rot - final plane rotator operations before commit kickoff
 * @plane: Pointer to drm plane structure
 * Returns: Zero on success
 */
int sde_plane_kickoff_rot(struct drm_plane *plane);

/**
 * sde_plane_set_error: enable/disable error condition
 * @plane: pointer to drm_plane structure
 */
void sde_plane_set_error(struct drm_plane *plane, bool error);

/**
 * sde_plane_init - create new sde plane for the given pipe
 * @dev:   Pointer to DRM device
 * @pipe:  sde hardware pipe identifier
 * @primary_plane: true if this pipe is primary plane for crtc
 * @possible_crtcs: bitmask of crtc that can be attached to the given pipe
 * @master_plane_id: primary plane id of a multirect pipe. 0 value passed for
 *                   a regular plane initialization. A non-zero primary plane
 *                   id will be passed for a virtual pipe initialization.
 *
 */
struct drm_plane *sde_plane_init(struct drm_device *dev,
		uint32_t pipe, bool primary_plane,
		unsigned long possible_crtcs, u32 master_plane_id);

/**
 * sde_plane_validate_multirecti_v2 - validate the multirect planes
 *				      against hw limitations
 * @plane: drm plate states of the multirect pair
 */
int sde_plane_validate_multirect_v2(struct sde_multirect_plane_states *plane);

/**
 * sde_plane_clear_multirect - clear multirect bits for the given pipe
 * @drm_state: Pointer to DRM plane state
 */
void sde_plane_clear_multirect(const struct drm_plane_state *drm_state);

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
 * sde_plane_set_revalidate - sets revalidate flag which forces a full
 *	validation of the plane properties in the next atomic check
 * @plane: Pointer to DRM plane object
 * @enable: Boolean to set/unset the flag
 */
void sde_plane_set_revalidate(struct drm_plane *plane, bool enable);

/**
 * sde_plane_helper_reset_properties - reset properties to default values in the
 *	given DRM plane state object
 * @plane: Pointer to DRM plane object
 * @plane_state: Pointer to DRM plane state object
 * Returns: 0 on success, negative errno on failure
 */
int sde_plane_helper_reset_custom_properties(struct drm_plane *plane,
		struct drm_plane_state *plane_state);

#endif /* _SDE_PLANE_H_ */
