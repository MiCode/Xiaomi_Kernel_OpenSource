/*
 * Copyright (c) 2015-2018 The Linux Foundation. All rights reserved.
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

#include <linux/kthread.h>
#include <drm/drm_crtc.h>
#include "msm_prop.h"
#include "sde_fence.h"
#include "sde_kms.h"
#include "sde_core_perf.h"
#include "sde_hw_blk.h"
#include "sde_hw_ds.h"

#define SDE_CRTC_NAME_SIZE	12

/* define the maximum number of in-flight frame events */
/* Expand it to 2x for handling atleast 2 connectors safely */
#define SDE_CRTC_FRAME_EVENT_SIZE	(4 * 2)

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
 * enum sde_crtc_output_capture_point
 * @MIXER_OUT : capture mixer output
 * @DSPP_OUT : capture output of dspp
 */
enum sde_crtc_output_capture_point {
	CAPTURE_MIXER_OUT,
	CAPTURE_DSPP_OUT
};

/**
 * @connectors    : Currently associated drm connectors for retire event
 * @num_connectors: Number of associated drm connectors for retire event
 * @list:	event list
 */
struct sde_crtc_retire_event {
	struct drm_connector *connectors[MAX_CONNECTORS];
	int num_connectors;
	struct list_head list;
};

/**
 * struct sde_crtc_mixer: stores the map for each virtual pipeline in the CRTC
 * @hw_lm:	LM HW Driver context
 * @hw_ctl:	CTL Path HW driver context
 * @hw_dspp:	DSPP HW driver context
 * @hw_ds:	DS HW driver context
 * @encoder:	Encoder attached to this lm & ctl
 * @mixer_op_mode: mixer blending operation mode
 */
struct sde_crtc_mixer {
	struct sde_hw_mixer *hw_lm;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_dspp *hw_dspp;
	struct sde_hw_ds *hw_ds;
	struct drm_encoder *encoder;
	u32 mixer_op_mode;
};

/**
 * struct sde_crtc_frame_event_cb_data : info of drm objects of a frame event
 * @crtc:       pointer to drm crtc object registered for frame event
 * @connector:  pointer to drm connector which is source of frame event
 */
struct sde_crtc_frame_event_cb_data {
	 struct drm_crtc *crtc;
	 struct drm_connector *connector;
};

/**
 * struct sde_crtc_frame_event: stores crtc frame event for crtc processing
 * @work:	base work structure
 * @crtc:	Pointer to crtc handling this event
 * @connector:  pointer to drm connector which is source of frame event
 * @list:	event list
 * @ts:		timestamp at queue entry
 * @event:	event identifier
 */
struct sde_crtc_frame_event {
	struct kthread_work work;
	struct drm_crtc *crtc;
	struct drm_connector *connector;
	struct list_head list;
	ktime_t ts;
	u32 event;
};

/**
 * struct sde_crtc_event - event callback tracking structure
 * @list:     Linked list tracking node
 * @kt_work:  Kthread worker structure
 * @sde_crtc: Pointer to associated sde_crtc structure
 * @cb_func:  Pointer to callback function
 * @usr:      Pointer to user data to be provided to the callback
 */
struct sde_crtc_event {
	struct list_head list;
	struct kthread_work kt_work;
	void *sde_crtc;

	void (*cb_func)(struct drm_crtc *crtc, void *usr);
	void *usr;
};

/*
 * Maximum number of free event structures to cache
 */
#define SDE_CRTC_MAX_EVENT_COUNT	16

/**
 * struct sde_crtc - virtualized CRTC data structure
 * @base          : Base drm crtc structure
 * @name          : ASCII description of this crtc
 * @num_ctls      : Number of ctl paths in use
 * @num_mixers    : Number of mixers in use
 * @mixers_swapped: Whether the mixers have been swapped for left/right update
 *                  especially in the case of DSC Merge.
 * @mixers        : List of active mixers
 * @event         : Pointer to last received drm vblank event. If there is a
 *                  pending vblank event, this will be non-null.
 * @vsync_count   : Running count of received vsync events
 * @drm_requested_vblank : Whether vblanks have been enabled in the encoder
 * @property_info : Opaque structure for generic property support
 * @property_defaults : Array of default values for generic property support
 * @output_fence  : output release fence context
 * @stage_cfg     : H/w mixer stage configuration
 * @debugfs_root  : Parent of debugfs node
 * @vblank_cb_count : count of vblank callback since last reset
 * @play_count    : frame count between crtc enable and disable
 * @vblank_cb_time  : ktime at vblank count reset
 * @vblank_last_cb_time  : ktime at last vblank notification
 * @sysfs_dev  : sysfs device node for crtc
 * @vsync_event_sf : vsync event notifier sysfs device
 * @vblank_requested : whether the user has requested vblank events
 * @suspend         : whether or not a suspend operation is in progress
 * @enabled       : whether the SDE CRTC is currently enabled. updated in the
 *                  commit-thread, not state-swap time which is earlier, so
 *                  safe to make decisions on during VBLANK on/off work
 * @ds_reconfig   : force reconfiguration of the destination scaler block
 * @feature_list  : list of color processing features supported on a crtc
 * @active_list   : list of color processing features are active
 * @dirty_list    : list of color processing features are dirty
 * @ad_dirty: list containing ad properties that are dirty
 * @ad_active: list containing ad properties that are active
 * @crtc_lock     : crtc lock around create, destroy and access.
 * @frame_pending : Whether or not an update is pending
 * @frame_events  : static allocation of in-flight frame events
 * @frame_event_list : available frame event list
 * @spin_lock     : spin lock for frame event, transaction status, etc...
 * @event_thread  : Pointer to event handler thread
 * @event_worker  : Event worker queue
 * @event_cache   : Local cache of event worker structures
 * @event_free_list : List of available event structures
 * @event_lock    : Spinlock around event handling code
 * @misr_enable   : boolean entry indicates misr enable/disable status.
 * @misr_frame_count  : misr frame count provided by client
 * @misr_data     : store misr data before turning off the clocks.
 * @sbuf_op_mode_old : inline rotator op mode for previous commit cycle
 * @sbuf_rot_id   : inline rotator block id for attached planes
 * @sbuf_rot_id_old: inline rotator id for previous commit
 * @sbuf_rot_id_delta: inline rotator id for current delta state
 * @idle_notify_work: delayed worker to notify idle timeout to user space
 * @power_event   : registered power event handle
 * @cur_perf      : current performance committed to clock/bandwidth driver
 * @rp_lock       : serialization lock for resource pool
 * @rp_head       : list of active resource pool
 */
struct sde_crtc {
	struct drm_crtc base;
	char name[SDE_CRTC_NAME_SIZE];

	/* HW Resources reserved for the crtc */
	u32 num_ctls;
	u32 num_mixers;
	bool mixers_swapped;
	struct sde_crtc_mixer mixers[CRTC_DUAL_MIXERS];

	struct drm_pending_vblank_event *event;
	u32 vsync_count;

	struct msm_property_info property_info;
	struct msm_property_data property_data[CRTC_PROP_COUNT];
	struct drm_property_blob *blob_info;

	/* output fence support */
	struct sde_fence_context output_fence;

	struct sde_hw_stage_cfg stage_cfg;
	struct dentry *debugfs_root;

	u32 vblank_cb_count;
	u64 play_count;
	ktime_t vblank_cb_time;
	ktime_t vblank_last_cb_time;
	struct device *sysfs_dev;
	struct kernfs_node *vsync_event_sf;
	bool vblank_requested;
	bool suspend;
	bool enabled;

	bool ds_reconfig;
	struct list_head feature_list;
	struct list_head active_list;
	struct list_head dirty_list;
	struct list_head ad_dirty;
	struct list_head ad_active;
	struct list_head user_event_list;

	struct mutex crtc_lock;
	struct mutex crtc_cp_lock;

	atomic_t frame_pending;
	struct sde_crtc_frame_event frame_events[SDE_CRTC_FRAME_EVENT_SIZE];
	struct list_head frame_event_list;
	spinlock_t spin_lock;

	/* for handling internal event thread */
	struct sde_crtc_event event_cache[SDE_CRTC_MAX_EVENT_COUNT];
	struct list_head event_free_list;
	spinlock_t event_lock;
	bool misr_enable;
	u32 misr_frame_count;
	u32 misr_data[CRTC_DUAL_MIXERS];

	u32 sbuf_op_mode_old;
	u32 sbuf_rot_id;
	u32 sbuf_rot_id_old;
	u32 sbuf_rot_id_delta;
	struct kthread_delayed_work idle_notify_work;

	struct sde_power_event *power_event;

	struct sde_core_perf_params cur_perf;
	struct sde_core_perf_params new_perf;

	struct mutex rp_lock;
	struct list_head rp_head;

	/* blob for histogram data */
	struct drm_property_blob *hist_blob;
};

#define to_sde_crtc(x) container_of(x, struct sde_crtc, base)

/**
 * struct sde_crtc_res_ops - common operations for crtc resources
 * @get: get given resource
 * @put: put given resource
 */
struct sde_crtc_res_ops {
	void *(*get)(void *val, u32 type, u64 tag);
	void (*put)(void *val);
};

/* crtc resource type (0x0-0xffff reserved for hw block type */
#define SDE_CRTC_RES_ROT_PLANE		0x10000
#define SDE_CRTC_RES_ROT_IN_FB		0x10001

#define SDE_CRTC_RES_FLAG_FREE		BIT(0)

/**
 * struct sde_crtc_res - definition of crtc resources
 * @list: list of crtc resource
 * @type: crtc resource type
 * @tag: unique identifier per type
 * @refcount: reference/usage count
 * @ops: callback operations
 * @val: resource handle associated with type/tag
 * @flags: customization flags
 */
struct sde_crtc_res {
	struct list_head list;
	u32 type;
	u64 tag;
	atomic_t refcount;
	struct sde_crtc_res_ops ops;
	void *val;
	u32 flags;
};

/**
 * sde_crtc_respool - crtc resource pool
 * @rp_lock: pointer to serialization lock
 * @rp_head: pointer to head of active resource pools of this crtc
 * @rp_list: list of crtc resource pool
 * @sequence_id: sequence identifier, incremented per state duplication
 * @res_list: list of resource managed by this resource pool
 * @ops: resource operations for parent resource pool
 */
struct sde_crtc_respool {
	struct mutex *rp_lock;
	struct list_head *rp_head;
	struct list_head rp_list;
	u32 sequence_id;
	struct list_head res_list;
	struct sde_crtc_res_ops ops;
};

/**
 * struct sde_crtc_state - sde container for atomic crtc state
 * @base: Base drm crtc state structure
 * @connectors    : Currently associated drm connectors
 * @num_connectors: Number of associated drm connectors
 * @rsc_client    : sde rsc client when mode is valid
 * @is_ppsplit    : Whether current topology requires PPSplit special handling
 * @bw_control    : true if bw/clk controlled by core bw/clk properties
 * @bw_split_vote : true if bw controlled by llcc/dram bw properties
 * @crtc_roi      : Current CRTC ROI. Possibly sub-rectangle of mode.
 *                  Origin top left of CRTC.
 * @lm_bounds     : LM boundaries based on current mode full resolution, no ROI.
 *                  Origin top left of CRTC.
 * @lm_roi        : Current LM ROI, possibly sub-rectangle of mode.
 *                  Origin top left of CRTC.
 * @user_roi_list : List of user's requested ROIs as from set property
 * @property_state: Local storage for msm_prop properties
 * @property_values: Current crtc property values
 * @input_fence_timeout_ns : Cached input fence timeout, in ns
 * @num_dim_layers: Number of dim layers
 * @dim_layer: Dim layer configs
 * @num_ds: Number of destination scalers to be configured
 * @num_ds_enabled: Number of destination scalers enabled
 * @ds_dirty: Boolean to indicate if dirty or not
 * @ds_cfg: Destination scaler config
 * @scl3_lut_cfg: QSEED3 lut config
 * @new_perf: new performance state being requested
 * @sbuf_cfg: stream buffer configuration
 * @sbuf_prefill_line: number of line for inline rotator prefetch
 * @sbuf_clk_rate : previous and current user specified inline rotator clock
 * @sbuf_clk_shifted : whether or not sbuf_clk_rate has been shifted as part
 *	of crtc atomic check
 */
struct sde_crtc_state {
	struct drm_crtc_state base;

	struct drm_connector *connectors[MAX_CONNECTORS];
	int num_connectors;
	struct sde_rsc_client *rsc_client;
	bool rsc_update;
	bool bw_control;
	bool bw_split_vote;

	bool is_ppsplit;
	struct sde_rect crtc_roi;
	struct sde_rect lm_bounds[CRTC_DUAL_MIXERS];
	struct sde_rect lm_roi[CRTC_DUAL_MIXERS];
	struct msm_roi_list user_roi_list;

	struct msm_property_state property_state;
	struct msm_property_value property_values[CRTC_PROP_COUNT];
	uint64_t input_fence_timeout_ns;
	uint32_t num_dim_layers;
	struct sde_hw_dim_layer dim_layer[SDE_MAX_DIM_LAYERS];
	uint32_t num_ds;
	uint32_t num_ds_enabled;
	bool ds_dirty;
	struct sde_hw_ds_cfg ds_cfg[SDE_MAX_DS_COUNT];
	struct sde_hw_scaler3_lut_cfg scl3_lut_cfg;

	struct sde_core_perf_params new_perf;
	struct sde_ctl_sbuf_cfg sbuf_cfg;
	u32 sbuf_prefill_line;
	u64 sbuf_clk_rate[2];
	bool sbuf_clk_shifted;

	struct sde_crtc_respool rp;
};

enum sde_crtc_irq_state {
	IRQ_NOINIT,
	IRQ_ENABLED,
	IRQ_DISABLING,
	IRQ_DISABLED,
};

/**
 * sde_crtc_irq_info - crtc interrupt info
 * @irq: interrupt callback
 * @event: event type of the interrupt
 * @func: function pointer to enable/disable the interrupt
 * @list: list of user customized event in crtc
 * @state: state of the interrupt
 * @state_lock: spin lock for interrupt state
 */
struct sde_crtc_irq_info {
	struct sde_irq_callback irq;
	u32 event;
	int (*func)(struct drm_crtc *crtc, bool en,
			struct sde_irq_callback *irq);
	struct list_head list;
	enum sde_crtc_irq_state state;
	spinlock_t state_lock;
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
	((S) && ((X) < CRTC_PROP_COUNT) ? ((S)->property_values[(X)].value) : 0)

/**
 * sde_crtc_get_mixer_width - get the mixer width
 * Mixer width will be same as panel width(/2 for split)
 * unless destination scaler feature is enabled
 */
static inline int sde_crtc_get_mixer_width(struct sde_crtc *sde_crtc,
	struct sde_crtc_state *cstate, struct drm_display_mode *mode)
{
	u32 mixer_width;

	if (!sde_crtc || !cstate || !mode)
		return 0;

	if (cstate->num_ds_enabled)
		mixer_width = cstate->ds_cfg[0].lm_width;
	else
		mixer_width = (sde_crtc->num_mixers == CRTC_DUAL_MIXERS ?
			mode->hdisplay / CRTC_DUAL_MIXERS : mode->hdisplay);

	return mixer_width;
}

/**
 * sde_crtc_get_mixer_height - get the mixer height
 * Mixer height will be same as panel height unless
 * destination scaler feature is enabled
 */
static inline int sde_crtc_get_mixer_height(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate, struct drm_display_mode *mode)
{
	if (!sde_crtc || !cstate || !mode)
		return 0;

	return (cstate->num_ds_enabled ?
			cstate->ds_cfg[0].lm_height : mode->vdisplay);
}

/**
 * sde_crtc_get_rotator_op_mode - get the rotator op mode from the crtc state
 * @crtc: Pointer to drm crtc object
 */
static inline enum sde_ctl_rot_op_mode sde_crtc_get_rotator_op_mode(
		struct drm_crtc *crtc)
{
	if (!crtc || !crtc->state)
		return SDE_CTL_ROT_OP_MODE_OFFLINE;

	return to_sde_crtc_state(crtc->state)->sbuf_cfg.rot_op_mode;
}

/**
 * sde_crtc_frame_pending - retun the number of pending frames
 * @crtc: Pointer to drm crtc object
 */
static inline int sde_crtc_frame_pending(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;

	if (!crtc)
		return -EINVAL;

	sde_crtc = to_sde_crtc(crtc);
	return atomic_read(&sde_crtc->frame_pending);
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
 * @old_state: Pointer to drm crtc old state object
 */
void sde_crtc_commit_kickoff(struct drm_crtc *crtc,
		struct drm_crtc_state *old_state);

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
 * sde_crtc_post_init - update crtc object with post initialization. It
 *      can update the debugfs, sysfs, entires.
 * @dev: sde device
 * @crtc: Pointer to drm crtc structure
 */
int sde_crtc_post_init(struct drm_device *dev, struct drm_crtc *crtc);

/**
 * sde_crtc_complete_flip - complete flip for clients
 * @crtc: Pointer to drm crtc object
 * @file: client to cancel's file handle
 */
void sde_crtc_complete_flip(struct drm_crtc *crtc, struct drm_file *file);

/**
 * sde_crtc_register_custom_event - api for enabling/disabling crtc event
 * @kms: Pointer to sde_kms
 * @crtc_drm: Pointer to crtc object
 * @event: Event that client is interested
 * @en: Flag to enable/disable the event
 */
int sde_crtc_register_custom_event(struct sde_kms *kms,
		struct drm_crtc *crtc_drm, u32 event, bool en);

/**
 * sde_crtc_get_intf_mode - get interface mode of the given crtc
 * @crtc: Pointert to crtc
 */
enum sde_intf_mode sde_crtc_get_intf_mode(struct drm_crtc *crtc);

/**
 * sde_crtc_get_client_type - check the crtc type- rt, nrt, rsc, etc.
 * @crtc: Pointer to crtc
 */
static inline enum sde_crtc_client_type sde_crtc_get_client_type(
						struct drm_crtc *crtc)
{
	struct sde_crtc_state *cstate =
			crtc ? to_sde_crtc_state(crtc->state) : NULL;

	if (!cstate)
		return NRT_CLIENT;

	return sde_crtc_get_intf_mode(crtc) == INTF_MODE_WB_LINE ? NRT_CLIENT :
			(cstate->rsc_client ? RT_RSC_CLIENT : RT_CLIENT);
}

/**
 * sde_crtc_is_enabled - check if sde crtc is enabled or not
 * @crtc: Pointer to crtc
 */
static inline bool sde_crtc_is_enabled(struct drm_crtc *crtc)
{
	return crtc ? crtc->enabled : false;
}

/**
 * sde_crtc_get_inline_prefill - get current inline rotation prefill
 * @crtc: Pointer to crtc
 * return: number of prefill lines
 */
static inline u32 sde_crtc_get_inline_prefill(struct drm_crtc *crtc)
{
	struct sde_crtc_state *cstate;

	if (!crtc || !crtc->state)
		return 0;

	cstate = to_sde_crtc_state(crtc->state);
	return cstate->sbuf_cfg.rot_op_mode != SDE_CTL_ROT_OP_MODE_OFFLINE ?
		cstate->sbuf_prefill_line : 0;
}

/**
 * sde_crtc_is_reset_required - validate the reset request based on the
 *	pm_suspend and crtc's active status. crtc's are left active
 *	on pm_suspend during LP1/LP2 states, as the display is still
 *	left ON. Avoid reset for the subsequent pm_resume in such cases.
 * @crtc: Pointer to crtc
 * return: false if in suspend state and crtc active, true otherwise
 */
static inline bool sde_crtc_is_reset_required(struct drm_crtc *crtc)
{
	/*
	 * reset is required even when there is no crtc_state as it is required
	 * to create the initial state object
	 */
	if (!crtc || !crtc->state)
		return true;

	/* reset not required if crtc is active during suspend state */
	if (sde_kms_is_suspend_state(crtc->dev) && crtc->state->active)
		return false;

	return true;
}

/**
 * sde_crtc_event_queue - request event callback
 * @crtc: Pointer to drm crtc structure
 * @func: Pointer to callback function
 * @usr: Pointer to user data to be passed to callback
 * @color_processing_event: True if color processing event
 * Returns: Zero on success
 */
int sde_crtc_event_queue(struct drm_crtc *crtc,
		void (*func)(struct drm_crtc *crtc, void *usr),
		void *usr, bool color_processing_event);

/**
 * sde_crtc_res_add - add given resource to resource pool in crtc state
 * @state: Pointer to drm crtc state
 * @type: Resource type
 * @tag: Search tag for given resource
 * @val: Resource handle
 * @ops: Resource callback operations
 * return: 0 if success; error code otherwise
 */
int sde_crtc_res_add(struct drm_crtc_state *state, u32 type, u64 tag,
		void *val, struct sde_crtc_res_ops *ops);

/**
 * sde_crtc_res_get - get given resource from resource pool in crtc state
 * @state: Pointer to drm crtc state
 * @type: Resource type
 * @tag: Search tag for given resource
 * return: Resource handle if success; pointer error or null otherwise
 */
void *sde_crtc_res_get(struct drm_crtc_state *state, u32 type, u64 tag);

/**
 * sde_crtc_res_put - return given resource to resource pool in crtc state
 * @state: Pointer to drm crtc state
 * @type: Resource type
 * @tag: Search tag for given resource
 * return: None
 */
void sde_crtc_res_put(struct drm_crtc_state *state, u32 type, u64 tag);

/**
 * sde_crtc_get_crtc_roi - retrieve the crtc_roi from the given state object
 *	used to allow the planes to adjust their final lm out_xy value in the
 *	case of partial update
 * @crtc_state: Pointer to crtc state
 * @crtc_roi: Output pointer to crtc roi in the given state
 */
void sde_crtc_get_crtc_roi(struct drm_crtc_state *state,
		const struct sde_rect **crtc_roi);

/**
 * sde_crtc_is_crtc_roi_dirty - retrieve whether crtc_roi was updated this frame
 *	Note: Only use during atomic_check since dirty properties may be popped
 * @crtc_state: Pointer to crtc state
 * Return: true if roi is dirty, false otherwise
 */
bool sde_crtc_is_crtc_roi_dirty(struct drm_crtc_state *state);

/** sde_crt_get_secure_level - retrieve the secure level from the give state
 *	object, this is used to determine the secure state of the crtc
 * @crtc : Pointer to drm crtc structure
 * @usr: Pointer to drm crtc state
 * return: secure_level
 */
static inline int sde_crtc_get_secure_level(struct drm_crtc *crtc,
		struct drm_crtc_state *state)
{
	if (!crtc || !state)
		return -EINVAL;

	return sde_crtc_get_property(to_sde_crtc_state(state),
			CRTC_PROP_SECURITY_LEVEL);
}

/**
 * sde_crtc_get_secure_transition - determines the operations to be
 * performed before transitioning to secure state
 * This function should be called after swapping the new state
 * @crtc: Pointer to drm crtc structure
 * @old_crtc_state: Poniter to previous CRTC state
 * Returns the bitmask of operations need to be performed, -Error in
 * case of error cases
 */
int sde_crtc_get_secure_transition_ops(struct drm_crtc *crtc,
		struct drm_crtc_state *old_crtc_state,
		bool old_valid_fb);

/**
 * sde_crtc_secure_ctrl - Initiates the transition between secure and
 *                          non-secure world
 * @crtc: Pointer to crtc
 * @post_commit: if this operation is triggered after commit
 */
int sde_crtc_secure_ctrl(struct drm_crtc *crtc, bool post_commit);

/**
 * sde_crtc_helper_reset_properties - reset properties to default values in the
 *	given DRM CRTC state object
 * @crtc: Pointer to DRM crtc object
 * @crtc_state: Pointer to DRM crtc state object
 * Returns: 0 on success, negative errno on failure
 */
int sde_crtc_helper_reset_custom_properties(struct drm_crtc *crtc,
		struct drm_crtc_state *crtc_state);

/**
 * sde_crtc_timeline_status - current buffer timeline status
 * @crtc: Pointer to crtc
 */
void sde_crtc_timeline_status(struct drm_crtc *crtc);

/**
 * sde_crtc_update_cont_splash_mixer_settings - update mixer settings
 *	during device bootup for cont_splash use case
 * @crtc: Pointer to drm crtc structure
 */
void sde_crtc_update_cont_splash_mixer_settings(
		struct drm_crtc *crtc);

/**
 * sde_crtc_get_sbuf_clk - get user specified sbuf clock settings
 * @state: Pointer to DRM crtc state object
 * Returns: Filtered sbuf clock setting from user space
 */
uint64_t sde_crtc_get_sbuf_clk(struct drm_crtc_state *state);

/**
 * sde_crtc_misr_setup - to configure and enable/disable MISR
 * @crtc: Pointer to drm crtc structure
 * @enable: boolean to indicate enable/disable misr
 * @frame_count: frame_count to be configured
 */
void sde_crtc_misr_setup(struct drm_crtc *crtc, bool enable, u32 frame_count);

#endif /* _SDE_CRTC_H_ */
