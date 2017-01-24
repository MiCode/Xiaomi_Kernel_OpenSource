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

#define DBG(fmt, ...) DRM_DEBUG(fmt"\n", ##__VA_ARGS__)

#define CRTC_DUAL_MIXERS	2
#define PENDING_FLIP		2
/* worst case one frame wait time based on 30 FPS : 33.33ms*/
#define CRTC_MAX_WAIT_ONE_FRAME     34
#define CRTC_HW_MIXER_MAXSTAGES(c, idx) ((c)->mixer[idx].sblk->maxblendstages)

/**
 * struct sde_crtc_mixer - stores the map for each virtual pipeline in the CRTC
 * @hw_dspp     : DSPP HW Driver context
 * @hw_lm       : LM HW Driver context
 * @hw_ctl      : CTL Path HW driver context
 * @intf_idx    : Interface idx
 * @mode        : Interface mode Active/CMD
 * @flush_mask  : Flush mask value for this commit
 */
struct sde_crtc_mixer {
	struct sde_hw_dspp  *hw_dspp;
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_ctl   *hw_ctl;
	enum sde_intf       intf_idx;
	enum sde_intf_mode  mode;
	u32 flush_mask;
};

/**
 * struct sde_crtc - virtualized CRTC data structure
 * @base          : Base drm crtc structure
 * @name          : ASCII description of this crtc
 * @encoder       : Associated drm encoder object
 * @id            : Unique crtc identifier
 * @lm_lock       : LM register access spinlock
 * @num_ctls      : Number of ctl paths in use
 * @num_mixers    : Number of mixers in use
 * @mixer         : List of active mixers
 * @event         : Pointer to last received drm vblank event
 * @pending       : Whether or not an update is pending
 * @vsync_count   : Running count of received vsync events
 */
struct sde_crtc {
	struct drm_crtc base;
	char name[8];
	struct drm_encoder *encoder;
	int id;

	spinlock_t lm_lock;	/* protect registers */

	/* HW Resources reserved for the crtc */
	u32  num_ctls;
	u32  num_mixers;
	struct sde_crtc_mixer mixer[CRTC_DUAL_MIXERS];

	/*if there is a pending flip, these will be non-null */
	struct drm_pending_vblank_event *event;
	atomic_t pending;
	u32 vsync_count;
};

#define to_sde_crtc(x) container_of(x, struct sde_crtc, base)

#endif /* _SDE_CRTC_H_ */
