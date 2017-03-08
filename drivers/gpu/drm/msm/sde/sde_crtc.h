/*
 * Copyright (c) 2015-2017 The Linux Foundation. All rights reserved.
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

#ifndef _SDE_CRTC_H_
#define _SDE_CRTC_H_

#include "drm_crtc.h"
#include "msm_prop.h"
#include "sde_fence.h"
#include "sde_kms.h"
#include "sde_core_perf.h"

#define SDE_CRTC_NAME_SIZE	12

/* define the maximum number of in-flight frame events */
#define SDE_CRTC_FRAME_EVENT_SIZE	2

/**
 * enum sde_crtc_client_type: crtc client type
 * @RT_CLIENT:	RealTime client like video/cmd mode display
 *              voting through apps rsc
 * @NRT_CLIENT:	Non-RealTime client like WB display
 *              voting through apps rsc
 * @RT_RSC_CLIENT:	Realtime display RSC voting client
 */
enum sde_crtc_client_type {
	RT_CLIENT,
	NRT_CLIENT,
	RT_RSC_CLIENT,
};

/**
 * struct sde_crtc_mixer: stores the map for each virtual pipeline in the CRTC
 * @hw_lm:	LM HW Driver context
 * @hw_ctl:	CTL Path HW driver context
 * @hw_dspp:	DSPP HW driver context
 * @encoder:	Encoder attached to this lm & ctl
 * @mixer_op_mode: mixer blending operation mode
 * @flush_mask:	mixer flush mask for ctl, mixer and pipe
 */
struct sde_crtc_mixer {
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_dspp  *hw_dspp;
	struct drm_encoder *encoder;
	u32 mixer_op_mode;
	u32 flush_mask;
};

/**
 * struct sde_crtc_frame_event: stores crtc frame event for crtc processing
 * @work:	base work structure
 * @crtc:	Pointer to crtc handling this event
 * @list:	event list
 * @ts:		timestamp at queue entry
 * @event:	event identifier
 */
struct sde_crtc_frame_event {
	struct kthread_work work;
	struct drm_crtc *crtc;
	struct list_head list;
	ktime_t ts;
	u32 event;
};

/**
 * struct sde_crtc - virtualized CRTC data structure
 * @base          : Base drm crtc structure
 * @name          : ASCII description of this crtc
 * @num_ctls      : Number of ctl paths in use
 * @num_mixers    : Number of mixers in use
 * @mixer         : List of active mixers
 * @event         : Pointer to last received drm vblank event. If there is a
 *                  pending vblank event, this will be non-null.
 * @vsync_count   : Running count of received vsync events
 * @drm_requested_vblank : Whether vblanks have been enabled in the encoder
 * @property_info : Opaque structure for generic property support
 * @property_defaults : Array of default values for generic property support
 * @stage_cfg     : H/w mixer stage configuration
 * @debugfs_root  : Parent of debugfs node
 * @vblank_cb_count : count of vblank callback since last reset
 * @vblank_cb_time  : ktime at vblank count reset
 * @vblank_refcount : reference count for vblank enable request
 * @feature_list  : list of color processing features supported on a crtc
 * @active_list   : list of color processing features are active
 * @dirty_list    : list of color processing features are dirty
 * @crtc_lock     : crtc lock around create, destroy and access.
 * @frame_pending : Whether or not an update is pending
 * @frame_events  : static allocation of in-flight frame events
 * @frame_event_list : available frame event list
 * @spin_lock     : spin lock for frame event, transaction status, etc...
 */
struct sde_crtc {
	struct drm_crtc base;
	char name[SDE_CRTC_NAME_SIZE];

	/* HW Resources reserved for the crtc */
	u32 num_ctls;
	u32 num_mixers;
	struct sde_crtc_mixer mixers[CRTC_DUAL_MIXERS];

	struct drm_pending_vblank_event *event;
	u32 vsync_count;

	struct msm_property_info property_info;
	struct msm_property_data property_data[CRTC_PROP_COUNT];
	struct drm_property_blob *blob_info;

	/* output fence support */
	struct sde_fence output_fence;

	struct sde_hw_stage_cfg stage_cfg;
	struct dentry *debugfs_root;

	u32 vblank_cb_count;
	ktime_t vblank_cb_time;
	atomic_t vblank_refcount;

	struct list_head feature_list;
	struct list_head active_list;
	struct list_head dirty_list;

	struct mutex crtc_lock;

	atomic_t frame_pending;
	struct sde_crtc_frame_event frame_events[SDE_CRTC_FRAME_EVENT_SIZE];
	struct list_head frame_event_list;
	spinlock_t spin_lock;
};

#define to_sde_crtc(x) container_of(x, struct sde_crtc, base)

/**
 * struct sde_crtc_state - sde container for atomic crtc state
 * @base: Base drm crtc state structure
 * @connectors    : Currently associated drm connectors
 * @num_connectors: Number of associated drm connectors
 * @intf_mode     : Interface mode of the primary connector
 * @rsc_mode      : Client vote through sde rsc
 * @rsc_client    : sde rsc client when mode is valid
 * @property_values: Current crtc property values
 * @input_fence_timeout_ns : Cached input fence timeout, in ns
 * @property_blobs: Reference pointers for blob properties
 * @num_dim_layers: Number of dim layers
 * @dim_layer: Dim layer configs
 * @cur_perf: current performance state
 * @new_perf: new performance state
 */
struct sde_crtc_state {
	struct drm_crtc_state base;

	struct drm_connector *connectors[MAX_CONNECTORS];
	int num_connectors;
	enum sde_intf_mode intf_mode;
	bool rsc_mode;
	struct sde_rsc_client *rsc_client;

	uint64_t property_values[CRTC_PROP_COUNT];
	uint64_t input_fence_timeout_ns;
	struct drm_property_blob *property_blobs[CRTC_PROP_COUNT];
	uint32_t num_dim_layers;
	struct sde_hw_dim_layer dim_layer[SDE_MAX_DIM_LAYERS];

	struct sde_core_perf_params cur_perf;
	struct sde_core_perf_params new_perf;
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

static inline int sde_crtc_mixer_width(struct sde_crtc *sde_crtc,
	struct drm_display_mode *mode)
{
	if (!sde_crtc || !mode)
		return 0;

	return  sde_crtc->num_mixers == CRTC_DUAL_MIXERS ?
		mode->hdisplay / CRTC_DUAL_MIXERS : mode->hdisplay;
}

static inline uint32_t get_crtc_split_width(struct drm_crtc *crtc)
{
	struct drm_display_mode *mode;
	struct sde_crtc *sde_crtc;

	if (!crtc || !crtc->state)
		return 0;

	sde_crtc = to_sde_crtc(crtc);
	mode = &crtc->state->adjusted_mode;
	return sde_crtc_mixer_width(sde_crtc, mode);
}

static inline uint32_t get_crtc_mixer_height(struct drm_crtc *crtc)
{
	struct drm_display_mode *mode;

	if (!crtc || !crtc->state)
		return 0;

	mode = &crtc->state->adjusted_mode;
	return mode->vdisplay;
}

/**
 * sde_crtc_vblank - enable or disable vblanks for this crtc
 * @crtc: Pointer to drm crtc object
 * @en: true to enable vblanks, false to disable
 */
int sde_crtc_vblank(struct drm_crtc *crtc, bool en);

/**
 * sde_crtc_commit_kickoff - trigger kickoff of the commit for this crtc
 * @crtc: Pointer to drm crtc object
 */
void sde_crtc_commit_kickoff(struct drm_crtc *crtc);

/**
 * sde_crtc_prepare_commit - callback to prepare for output fences
 * @crtc: Pointer to drm crtc object
 * @old_state: Pointer to drm crtc old state object
 */
void sde_crtc_prepare_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state);

/**
 * sde_crtc_complete_commit - callback signalling completion of current commit
 * @crtc: Pointer to drm crtc object
 * @old_state: Pointer to drm crtc old state object
 */
void sde_crtc_complete_commit(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state);

/**
 * sde_crtc_init - create a new crtc object
 * @dev: sde device
 * @plane: base plane
 * @Return: new crtc object or error
 */
struct drm_crtc *sde_crtc_init(struct drm_device *dev, struct drm_plane *plane);

/**
 * sde_crtc_cancel_pending_flip - complete flip for clients on lastclose
 * @crtc: Pointer to drm crtc object
 * @file: client to cancel's file handle
 */
void sde_crtc_cancel_pending_flip(struct drm_crtc *crtc, struct drm_file *file);

/**
 * sde_crtc_get_intf_mode - get interface mode of the given crtc
 * @crtc: Pointert to crtc
 */
static inline enum sde_intf_mode sde_crtc_get_intf_mode(struct drm_crtc *crtc)
{
	struct sde_crtc_state *cstate =
			crtc ? to_sde_crtc_state(crtc->state) : NULL;

	return cstate ? cstate->intf_mode : INTF_MODE_NONE;
}

/**
 * sde_crtc_get_client_type - check the crtc type- rt, nrt, rsc, etc.
 * @crtc: Pointer to crtc
 */
static inline bool sde_crtc_get_client_type(struct drm_crtc *crtc)
{
	struct sde_crtc_state *cstate =
			crtc ? to_sde_crtc_state(crtc->state) : NULL;

	return cstate && (cstate->intf_mode == INTF_MODE_WB_LINE) ? NRT_CLIENT
		: cstate && cstate->rsc_mode ? RT_RSC_CLIENT : RT_CLIENT;
}

/**
 * sde_crtc_is_enabled - check if sde crtc is enabled or not
 * @crtc: Pointer to crtc
 */
static inline bool sde_crtc_is_enabled(struct drm_crtc *crtc)
{
	return crtc ? crtc->enabled : false;
}

#endif /* _SDE_CRTC_H_ */
