/*
 * Copyright (c) 2015-2020 The Linux Foundation. All rights reserved.
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
#include <linux/of_fdt.h>
#include <drm/drm_crtc.h>
#include "msm_prop.h"
#include "sde_fence.h"
#include "sde_kms.h"
#include "sde_core_perf.h"
#include "sde_hw_ds.h"

#define SDE_CRTC_NAME_SIZE	12
#define RGB_NUM_COMPONENTS	3

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
 * enum sde_crtc_idle_pc_state: states of idle power collapse
 * @IDLE_PC_NONE: no-op
 * @IDLE_PC_ENABLE: enable idle power-collapse
 * @IDLE_PC_DISABLE: disable idle power-collapse
 */
enum sde_crtc_idle_pc_state {
	IDLE_PC_NONE,
	IDLE_PC_ENABLE,
	IDLE_PC_DISABLE,
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
/**
 * struct sde_crtc_fps_info - structure for measuring fps periodicity
 * @frame_count		: Total frames during configured periodic duration
 * @last_sampled_time_us: Stores the last ktime in microsecs when fps
 *                        was calculated
 * @measured_fps	: Last measured fps value
 * @fps_periodic_duration	: Duration in milliseconds to measure the fps.
 *                                Default value is 1 second.
 * @time_buf		: Buffer for storing ktime of the commits
 * @next_time_index	: index into time_buf for storing ktime for next commit
 */
struct sde_crtc_fps_info {
	u32 frame_count;
	ktime_t last_sampled_time_us;
	u32 measured_fps;
	u32 fps_periodic_duration;
	ktime_t *time_buf;
	u32 next_time_index;
};

/**
 * struct sde_ltm_buffer - defines LTM buffer structure.
 * @fb: frm framebuffer for the buffer
 * @gem: drm gem handle for the buffer
 * @asapce : pointer to address space
 * @drm_fb_id: framebuffer id associated with this buffer
 * @offset: offset for alignment
 * @iova: device address
 * @kva: kernel virtual address
 * @node: list node for LTM buffer list;
 */
struct sde_ltm_buffer {
	struct drm_framebuffer *fb;
	struct drm_gem_object *gem;
	struct msm_gem_address_space *aspace;
	u32 drm_fb_id;
	u32 offset;
	u64 iova;
	void *kva;
	struct list_head node;
};

/**
 * struct sde_crtc_misr_info - structure for misr information
 * @misr_enable : enable/disable flag
 * @misr_frame_count : Number of frames for misr calculation.
 */
struct sde_crtc_misr_info {
	bool misr_enable;
	u32 misr_frame_count;
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
 * @priv_handle   : Pointer to external private handle, if present
 * @vblank_cb_count : count of vblank callback since last reset
 * @play_count    : frame count between crtc enable and disable
 * @vblank_cb_time  : ktime at vblank count reset
 * @vblank_last_cb_time  : ktime at last vblank notification
 * @sysfs_dev  : sysfs device node for crtc
 * @vsync_event_sf : vsync event notifier sysfs device
 * @enabled       : whether the SDE CRTC is currently enabled. updated in the
 *                  commit-thread, not state-swap time which is earlier, so
 *                  safe to make decisions on during VBLANK on/off work
 * @ds_reconfig   : force reconfiguration of the destination scaler block
 * @feature_list  : list of color processing features supported on a crtc
 * @active_list   : list of color processing features are active
 * @dirty_list    : list of color processing features are dirty
 * @ad_dirty      : list containing ad properties that are dirty
 * @ad_active     : list containing ad properties that are active
 * @crtc_lock     : crtc lock around create, destroy and access.
 * @vblank_modeset_ctrl_lock     : lock used for controlling vblank
				during modeset
 * @frame_pending : Whether or not an update is pending
 * @frame_events  : static allocation of in-flight frame events
 * @frame_event_list : available frame event list
 * @spin_lock     : spin lock for frame event, transaction status, etc...
 * @event_thread  : Pointer to event handler thread
 * @event_worker  : Event worker queue
 * @event_cache   : Local cache of event worker structures
 * @event_free_list : List of available event structures
 * @event_lock    : Spinlock around event handling code
 * @misr_enable_sui : boolean entry indicates misr enable/disable status
 *                    for secure cases.
 * @misr_enable_debugfs : boolean entry indicates misr enable/disable status
 *                        from debugfs.
 * @misr_frame_count  : misr frame count provided by client
 * @misr_data     : store misr data before turning off the clocks.
 * @idle_notify_work: delayed worker to notify idle timeout to user space
 * @power_event   : registered power event handle
 * @cur_perf      : current performance committed to clock/bandwidth driver
 * @plane_mask_old: keeps track of the planes used in the previous commit
 * @frame_trigger_mode: frame trigger mode
 * @ltm_buffer_cnt  : number of ltm buffers
 * @ltm_buffers     : struct stores ltm buffer related data
 * @ltm_buf_free    : list of LTM buffers that are available
 * @ltm_buf_busy    : list of LTM buffers that are been used by HW
 * @ltm_hist_en     : flag to indicate whether LTM hist is enabled or not
 * @ltm_buffer_lock : muttx to protect ltm_buffers allcation and free
 * @ltm_lock        : Spinlock to protect ltm buffer_cnt, hist_en and ltm lists
 * @needs_hw_reset  : Initiate a hw ctl reset
 * @comp_ratio      : Compression ratio
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
	struct sde_fence_context *output_fence;

	struct sde_hw_stage_cfg stage_cfg;
	struct dentry *debugfs_root;
	void *priv_handle;

	u32 vblank_cb_count;
	u64 play_count;
	ktime_t vblank_cb_time;
	ktime_t vblank_last_cb_time;
	struct sde_crtc_fps_info fps_info;
	struct device *sysfs_dev;
	struct kernfs_node *vsync_event_sf;
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
	struct mutex vblank_modeset_ctrl_lock;

	atomic_t frame_pending;
	struct sde_crtc_frame_event frame_events[SDE_CRTC_FRAME_EVENT_SIZE];
	struct list_head frame_event_list;
	spinlock_t spin_lock;

	/* for handling internal event thread */
	struct sde_crtc_event event_cache[SDE_CRTC_MAX_EVENT_COUNT];
	struct list_head event_free_list;
	spinlock_t event_lock;
	bool misr_enable_sui;
	bool misr_enable_debugfs;
	u32 misr_frame_count;
	struct kthread_delayed_work idle_notify_work;

	struct sde_power_event *power_event;

	struct sde_core_perf_params cur_perf;
	struct sde_core_perf_params new_perf;

	u32 plane_mask_old;

	/* blob for histogram data */
	struct drm_property_blob *hist_blob;
	enum frame_trigger_mode_type frame_trigger_mode;

	u32 ltm_buffer_cnt;
	struct sde_ltm_buffer *ltm_buffers[LTM_BUFFER_SIZE];
	struct list_head ltm_buf_free;
	struct list_head ltm_buf_busy;
	bool ltm_hist_en;
	struct drm_msm_ltm_cfg_param ltm_cfg;
	struct mutex ltm_buffer_lock;
	spinlock_t ltm_lock;
	bool needs_hw_reset;

	int comp_ratio;
};

#define to_sde_crtc(x) container_of(x, struct sde_crtc, base)

/**
 * enum sde_crtc_mi_layer_type: type of mi layer
 * @MI_LAYER_FOD_PRESSED_ICON: FOD touched icon layer
 * @MI_LAYER_FOD_ICON: FOD untouch icon layer
 * @MI_LAYER_AOD: AOD layer
 */
enum sde_crtc_mi_layer_type {
	MI_LAYER_NULL = 0x0,
	MI_LAYER_FOD_HBM_OVERLAY = 0x1,
	MI_LAYER_FOD_ICON = 0x2,
	MI_LAYER_AOD = 0x4,
	MI_LAYER_MAX,
};

/**
 * sde_crtc_mi_dc_backlight - mi dc backlight
 * @mi_dc_bl_state: dc backlihgt state
 * @mi_dc_backlight_level: last backlight stash
 * @mi_dc_layer_alpha: dc dim layer alpha
 */
typedef struct sde_crtc_mi_dc_backlight
{
	uint8_t mi_dc_bl_state;
	int32_t mi_dc_bl_level;
	int32_t mi_dc_bl_layer_alpha;
} sde_crtc_mi_dc_backlight;

typedef struct sde_crtc_mi_layer
{
	int32_t layer_index;
	enum sde_crtc_mi_layer_type last_state;
} sde_crtc_mi_layer;

/**
 * sde_crtc_mi_state - mi crtc state
 * @mi_dim_layer: dim layer added by Mi
 */
struct sde_crtc_mi_state {
	struct sde_hw_dim_layer *mi_dim_layer;
	struct sde_crtc_mi_layer mi_layer;
	uint32_t dimlayer_backlight_stash;
	uint8_t  dimlayer_alpha_stash;
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
 * @mi_state: Mi part of crtc state
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
    /* Mi crtc state */
	struct sde_crtc_mi_state mi_state;
	uint32_t num_dim_layers_bank;
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
 * sde_crtc_set_needs_hw_reset - set hw reset flag, to handle reset during
 *                               commit kickoff
 * @crtc: Pointer to DRM crtc instance
 */
static inline void sde_crtc_set_needs_hw_reset(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;

	if (!crtc)
		return;

	sde_crtc = to_sde_crtc(crtc);
	sde_crtc->needs_hw_reset = true;
}

/**
 * sde_crtc_reset_hw - attempt hardware reset on errors
 * @crtc: Pointer to DRM crtc instance
 * @old_state: Pointer to crtc state for previous commit
 * @recovery_events: Whether or not recovery events are enabled
 * Returns: Zero if current commit should still be attempted
 */
int sde_crtc_reset_hw(struct drm_crtc *crtc, struct drm_crtc_state *old_state,
	bool recovery_events);

/**
 * sde_crtc_request_frame_reset - requests for next frame reset
 * @crtc: Pointer to drm crtc object
 */
static inline int sde_crtc_request_frame_reset(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	if (sde_crtc->frame_trigger_mode == FRAME_DONE_WAIT_POSTED_START)
		sde_crtc_reset_hw(crtc, crtc->state, false);

	return 0;
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
 * @crtc: Pointert to DRM crtc
 * @crtc: Pointert to DRM crtc_state
 */
enum sde_intf_mode sde_crtc_get_intf_mode(struct drm_crtc *crtc,
		struct drm_crtc_state *cstate);

/**
 * sde_crtc_get_fps_mode - get frame rate of the given crtc
 * @crtc: Pointert to crtc
 */
u32 sde_crtc_get_fps_mode(struct drm_crtc *crtc);

/**
 * sde_crtc_get_client_type - check the crtc type- rt, rsc_rt, etc.
 * @crtc: Pointer to crtc
 */
static inline enum sde_crtc_client_type sde_crtc_get_client_type(
						struct drm_crtc *crtc)
{
	struct sde_crtc_state *cstate =
			crtc ? to_sde_crtc_state(crtc->state) : NULL;

	if (!cstate)
		return RT_CLIENT;

	return cstate->rsc_client ? RT_RSC_CLIENT : RT_CLIENT;
}

/**
 * sde_crtc_is_rt_client - check if real-time client or not
 * @crtc: Pointer to DRM crtc
 * @crtc_state: Pointer to DRM crtc_state
 */
static inline bool sde_crtc_is_rt_client(struct drm_crtc *crtc,
		struct drm_crtc_state *cstate)
{
	if (!crtc || !cstate)
		return true;

	return (sde_crtc_get_intf_mode(crtc, cstate) != INTF_MODE_WB_LINE);
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

/** sde_crtc_atomic_check_has_modeset - checks if the new_crtc_state in the
 *	drm_atomic_state has a modeset
 * @state : pointer to drm_atomic_state
 * @crtc : Pointer to drm crtc structure
 * Returns true if crtc has modeset
 */
static inline bool sde_crtc_atomic_check_has_modeset(
	struct drm_atomic_state *state, struct drm_crtc *crtc)
{
	struct drm_crtc_state *crtc_state;

	if (!state || !crtc)
		return false;

	crtc_state = drm_atomic_get_new_crtc_state(state,
					crtc);
	return (crtc_state && drm_atomic_crtc_needs_modeset(crtc_state));
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
 * sde_crtc_find_plane_fb_modes - finds the modes of all planes attached
 *                                  to crtc
 * @crtc: Pointer to DRM crtc object
 * @fb_ns: number of non secure planes
 * @fb_sec: number of secure-playback planes
 * @fb_sec_dir: number of secure-ui/secure-camera planes
 */
int sde_crtc_find_plane_fb_modes(struct drm_crtc *crtc,
		uint32_t *fb_ns, uint32_t *fb_sec, uint32_t *fb_sec_dir);

/**
 * sde_crtc_state_find_plane_fb_modes - finds the modes of all planes attached
 *                                       to the crtc state
 * @crtc_state: Pointer to DRM crtc state object
 * @fb_ns: number of non secure planes
 * @fb_sec: number of secure-playback planes
 * @fb_sec_dir: number of secure-ui/secure-camera planes
 */
int sde_crtc_state_find_plane_fb_modes(struct drm_crtc_state *state,
		uint32_t *fb_ns, uint32_t *fb_sec, uint32_t *fb_sec_dir);

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
 * sde_crtc_update_cont_splash_settings - update mixer settings
 *	during device bootup for cont_splash use case
 * @crtc: Pointer to drm crtc structure
 */
void sde_crtc_update_cont_splash_settings(
		struct drm_crtc *crtc);

/**
 * sde_crtc_misr_setup - to configure and enable/disable MISR
 * @crtc: Pointer to drm crtc structure
 * @enable: boolean to indicate enable/disable misr
 * @frame_count: frame_count to be configured
 */
void sde_crtc_misr_setup(struct drm_crtc *crtc, bool enable, u32 frame_count);

/**
 * sde_crtc_get_misr_info - to configure and enable/disable MISR
 * @crtc: Pointer to drm crtc structure
 * @crtc_misr_info: Pointer to crtc misr info structure
 */
void sde_crtc_get_misr_info(struct drm_crtc *crtc,
		struct sde_crtc_misr_info *crtc_misr_info);

/**
 * sde_crtc_mi_atomic_check - to do crtc mi atomic check
 * @crtc: Pointer to sde crtc state structure
 * @cstate: Pointer to sde crtc state structure
 * @pstates: Pointer to sde plane state structure
 * @cnt: plane refence count
 */
int sde_crtc_mi_atomic_check(struct sde_crtc *sde_crtc, struct sde_crtc_state *cstate,
		void *pstates, int cnt);

/**
 * sde_crtc_get_mi_fod_sync_info - to do crtc mi sync info
 * @cstate: Pointer to sde crtc state structure
 */
uint32_t sde_crtc_get_mi_fod_sync_info(struct sde_crtc_state *cstate);


/**
 * sde_crtc_get_num_datapath - get the number of datapath active
 *				of primary connector
 * @crtc: Pointer to DRM crtc object
 * @connector: Pointer to DRM connector object of WB in CWB case
 */
int sde_crtc_get_num_datapath(struct drm_crtc *crtc,
		struct drm_connector *connector);

/*
 * sde_crtc_set_compression_ratio - set compression ratio src_bpp/target_bpp
 * @msm_mode_info: Mode info
 * @crtc: Pointer to drm crtc structure
 */
static inline void sde_crtc_set_compression_ratio(
		struct msm_mode_info mode_info, struct drm_crtc *crtc)
{
	int target_bpp, src_bpp;
	struct sde_crtc *sde_crtc = to_sde_crtc(crtc);

	/**
	 * In cases where DSC compression type is not found, set
	 * compression value to default value of 1.
	 */
	if (mode_info.comp_info.comp_type != MSM_DISPLAY_COMPRESSION_DSC) {
		sde_crtc->comp_ratio = 1;
		goto end;
	}

	target_bpp = mode_info.comp_info.dsc_info.bpp;
	src_bpp = mode_info.comp_info.dsc_info.bpc * RGB_NUM_COMPONENTS;
	sde_crtc->comp_ratio = mult_frac(1, src_bpp, target_bpp);
end:
	SDE_DEBUG("sde_crtc comp ratio: %d\n", sde_crtc->comp_ratio);
}

#endif /* _SDE_CRTC_H_ */
