/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
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
#include "sde_hw_sspp.h"
#include "sde_crtc.h"

/* dirty bits for update function */
#define SDE_PLANE_DIRTY_RECTS	0x1
#define SDE_PLANE_DIRTY_FORMAT	0x2
#define SDE_PLANE_DIRTY_SHARPEN	0x4
#define SDE_PLANE_DIRTY_PERF	0x8
#define SDE_PLANE_DIRTY_FB_TRANSLATION_MODE	0x10
#define SDE_PLANE_DIRTY_VIG_GAMUT 0x20
#define SDE_PLANE_DIRTY_VIG_IGC 0x40
#define SDE_PLANE_DIRTY_DMA_IGC 0x80
#define SDE_PLANE_DIRTY_DMA_GC 0x100
#define SDE_PLANE_DIRTY_QOS     0x200
#define SDE_PLANE_DIRTY_FP16_IGC 0x400
#define SDE_PLANE_DIRTY_FP16_GC 0x800
#define SDE_PLANE_DIRTY_FP16_CSC 0x1000
#define SDE_PLANE_DIRTY_FP16_UNMULT 0x2000
#define SDE_PLANE_DIRTY_CP (SDE_PLANE_DIRTY_VIG_GAMUT |\
		SDE_PLANE_DIRTY_VIG_IGC | SDE_PLANE_DIRTY_DMA_IGC |\
		SDE_PLANE_DIRTY_DMA_GC | SDE_PLANE_DIRTY_FP16_IGC |\
		SDE_PLANE_DIRTY_FP16_GC | SDE_PLANE_DIRTY_FP16_CSC |\
		SDE_PLANE_DIRTY_FP16_UNMULT)
#define SDE_PLANE_DIRTY_ALL	(0xFFFFFFFF & ~(SDE_PLANE_DIRTY_CP))

/**
 * enum sde_layout
 * Describes SSPP to LM staging layout when using more than 1 pair of LMs
 * @SDE_LAYOUT_NONE    : SSPPs to LMs staging layout not enabled
 * @SDE_LAYOUT_LEFT    : SSPPs will be staged on left two LMs
 * @SDE_LAYOUT_RIGHT   : SSPPs will be staged on right two LMs
 * @SDE_LAYOUT_MAX     :
 */
enum sde_layout {
	SDE_LAYOUT_NONE = 0,
	SDE_LAYOUT_LEFT,
	SDE_LAYOUT_RIGHT,
	SDE_LAYOUT_MAX,
};

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
 * @pipe_order_flags: contains pipe order flags:
 *			SDE_SSPP_RIGHT - right pipe in source split pair
 * @layout_offset:	horizontal layout offset for global coordinate
 * @layout:             layout for topology requiring more than 1 lm pair.
 * @scaler3_cfg: configuration data for scaler3
 * @pixel_ext: configuration data for pixel extensions
 * @scaler_check_state: indicates status of user provided pixel extension data
 * @pre_down:		pre down scale configuration
 * @sc_cfg:		system cache configuration
 * @rotation:		rotation cache state
 * @static_cache_state:	plane cache state for static image
 * @cdp_cfg:	CDP configuration
 * @cont_splash_populated: State was populated as part of cont. splash
 * @ubwc_stats_roi: cached roi for ubwc stats
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
	uint32_t pipe_order_flags;
	int layout_offset;
	enum sde_layout layout;

	/* scaler configuration */
	struct sde_hw_scaler3_cfg scaler3_cfg;
	struct sde_hw_pixel_ext pixel_ext;
	enum sde_plane_sclcheck_state scaler_check_state;
	struct sde_hw_inline_pre_downscale_cfg pre_down;

	/* @sc_cfg: system_cache configuration */
	struct sde_hw_pipe_sc_cfg sc_cfg;
	uint32_t rotation;
	uint32_t static_cache_state;

	struct sde_hw_pipe_cdp_cfg cdp_cfg;

	bool cont_splash_populated;

	struct sde_drm_ubwc_stats_roi ubwc_stats_roi;
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
 * sde_plane_destroy_fb - destroy fb object and clear fb
 * @state: old plane state
 */
void sde_plane_destroy_fb(struct drm_plane_state *state);

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
 * sde_plane_ctl_flush - set/clear control flush mask
 * @plane:   Pointer to DRM plane object
 * @ctl: Pointer to control hardware
 * @set: set if true else clear
 */
void sde_plane_ctl_flush(struct drm_plane *plane, struct sde_hw_ctl *ctl,
		bool set);

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
 * sde_plane_validate_src_addr - validate if current sspp addr of given
 * plane is within the input address range
 * @drm_plane:	Pointer to DRM plane object
 * @base_addr:	Start address of the input address range
 * @size:	Size of the input address range
 * @Return:	Non-zero if source pipe current address is not in input range
 */
int sde_plane_validate_src_addr(struct drm_plane *plane,
		unsigned long base_addr, u32 size);

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

/* sde_plane_is_sec_ui_allowed - indicates if the sspp allows secure-ui layers
 * @plane: Pointer to DRM plane object
 * Returns: true if allowed; false otherwise
 */
bool sde_plane_is_sec_ui_allowed(struct drm_plane *plane);

/* sde_plane_secure_ctrl_xin_client - controls the VBIF programming of
 *	the xin-client before the secure-ui session. Programs the QOS
 *	and OT limits in VBIF for the sec-ui allowed xins
 * @plane: Pointer to DRM plane object
 * @crtc: Pointer to DRM CRTC state object
 */
void sde_plane_secure_ctrl_xin_client(struct drm_plane *plane,
		struct drm_crtc *crtc);
/*
 * sde_plane_get_frame_data - gets the plane frame data
 * @plane: Pointer to DRM plane object
 * @frame_data: Pointer to plane frame data structure
 */
void sde_plane_get_frame_data(struct drm_plane *plane,
		struct sde_drm_plane_frame_data *frame_data);

/*
 * sde_plane_setup_src_split_order - enable/disable pipe's src_split_order
 * @plane: Pointer to DRM plane object
 * @rect_mode: multirect mode
 * @enable: enable/disable flag
 */
void sde_plane_setup_src_split_order(struct drm_plane *plane,
		enum sde_sspp_multirect_index rect_mode, bool enable);

/*
 * sde_plane_set_sid - set VM SID for the plane
 * @plane: Pointer to DRM plane object
 * @vm: VM id
 */
void sde_plane_set_sid(struct drm_plane *plane, u32 vm);

/* sde_plane_is_cache_required - indicates if the system cache is
 *	required for the plane.
 * @plane: Pointer to DRM plane object
 * @type: sys cache type
 * Returns: true if sys cache is required, otherwise false.
 */
bool sde_plane_is_cache_required(struct drm_plane *plane,
		enum sde_sys_cache_type type);

/**
 * sde_plane_static_img_control - Switch the static image state
 * @plane: Pointer to drm plane structure
 * @state: state to set
 */
void sde_plane_static_img_control(struct drm_plane *plane,
		enum sde_crtc_cache_state state);

void sde_plane_add_data_to_minidump_va(struct drm_plane *plane);
#endif /* _SDE_PLANE_H_ */
