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

#ifndef _SDE_CRTC_H_
#define _SDE_CRTC_H_

#include "drm_crtc.h"
#include "msm_prop.h"
#include "sde_fence.h"
#include "sde_kms.h"

#define SDE_CRTC_NAME_SIZE	12
#define PENDING_FLIP		2
/* worst case one frame wait time based on 30 FPS : 33.33ms*/
#define CRTC_MAX_WAIT_ONE_FRAME     34
#define CRTC_HW_MIXER_MAXSTAGES(c, idx) ((c)->mixer[idx].sblk->maxblendstages)

/**
 * struct sde_crtc_mixer: stores the map for each virtual pipeline in the CRTC
 * @hw_lm:	LM HW Driver context
 * @hw_ctl:	CTL Path HW driver context
 * @encoder:	Encoder attached to this lm & ctl
 */
struct sde_crtc_mixer {
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_ctl *hw_ctl;
	struct drm_encoder *encoder;
};

/**
 * struct sde_crtc - virtualized CRTC data structure
 * @base          : Base drm crtc structure
 * @name          : ASCII description of this crtc
 * @drm_crtc_id   : Id for reporting vblank. Id is relative init order into
 *                  mode_config.crtc_list and used by user space to identify
 *                  specific crtc in apis such as drm_wait_vblank
 * @num_ctls      : Number of ctl paths in use
 * @num_mixers    : Number of mixers in use
 * @mixer         : List of active mixers
 * @event         : Pointer to last received drm vblank event
 * @pending       : Whether or not an update is pending
 * @vsync_count   : Running count of received vsync events
 * @drm_requested_vblank : Whether vblanks have been enabled in the encoder
 * @property_info : Opaque structure for generic property support
 * @property_defaults : Array of default values for generic property support
 * @stage_cfg     : H/w mixer stage configuration
 * @debugfs_root  : Parent of debugfs node
 */
struct sde_crtc {
	struct drm_crtc base;
	char name[SDE_CRTC_NAME_SIZE];

	int drm_crtc_id;

	/* HW Resources reserved for the crtc */
	u32 num_ctls;
	u32 num_mixers;
	struct sde_crtc_mixer mixers[CRTC_DUAL_MIXERS];

	/*if there is a pending flip, these will be non-null */
	struct drm_pending_vblank_event *event;
	atomic_t pending;
	u32 vsync_count;
	atomic_t drm_requested_vblank;

	struct msm_property_info property_info;
	struct msm_property_data property_data[CRTC_PROP_COUNT];

	/* output fence support */
	struct sde_fence output_fence;

	struct sde_hw_stage_cfg stage_cfg;
	struct dentry *debugfs_root;
};

#define to_sde_crtc(x) container_of(x, struct sde_crtc, base)

/**
 * struct sde_crtc_state - sde container for atomic crtc state
 * @base: Base drm crtc state structure
 * @property_values: Current crtc property values
 * @input_fence_timeout_ns : Cached input fence timeout, in ns
 * @property_blobs: Reference pointers for blob properties
 */
struct sde_crtc_state {
	struct drm_crtc_state base;
	uint64_t property_values[CRTC_PROP_COUNT];
	uint64_t input_fence_timeout_ns;
	struct drm_property_blob *property_blobs[CRTC_PROP_COUNT];
};

#define to_sde_crtc_state(x) \
	container_of(x, struct sde_crtc_state, base)

/**
 * sde_crtc_get_property - query integer value of crtc property
 * @S: Pointer to crtc state
 * @X: Property index, from enum msm_mdp_crtc_property
 * Returns: Integer value of requested property
 */
#define sde_crtc_get_property(S, X) \
	((S) && ((X) < CRTC_PROP_COUNT) ? ((S)->property_values[(X)]) : 0)

#endif /* _SDE_CRTC_H_ */
