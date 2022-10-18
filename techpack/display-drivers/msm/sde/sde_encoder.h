/*
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __SDE_ENCODER_H__
#define __SDE_ENCODER_H__

#include <drm/drm_crtc.h>
#include <drm/drm_bridge.h>
#include <linux/sde_rsc.h>

#include "msm_prop.h"
#include "sde_hw_mdss.h"
#include "sde_kms.h"
#include "sde_connector.h"
#include "sde_power_handle.h"

/*
 * Two to anticipate panels that can do cmd/vid dynamic switching
 * plan is to create all possible physical encoder types, and switch between
 * them at runtime
 */
#define NUM_PHYS_ENCODER_TYPES 2

#define MAX_PHYS_ENCODERS_PER_VIRTUAL \
	(MAX_H_TILES_PER_DISPLAY * NUM_PHYS_ENCODER_TYPES)

#define MAX_CHANNELS_PER_ENC 4

#define SDE_ENCODER_FRAME_EVENT_DONE			BIT(0)
#define SDE_ENCODER_FRAME_EVENT_ERROR			BIT(1)
#define SDE_ENCODER_FRAME_EVENT_PANEL_DEAD		BIT(2)
#define SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE	BIT(3)
#define SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE	BIT(4)
#define SDE_ENCODER_FRAME_EVENT_CWB_DONE		BIT(5)

#define IDLE_POWERCOLLAPSE_DURATION	(66 - 16/2)
#define IDLE_POWERCOLLAPSE_IN_EARLY_WAKEUP (200 - 16/2)

/* below this fps limit, timeouts are adjusted based on fps */
#define DEFAULT_TIMEOUT_FPS_THRESHOLD            24

#define NUM_FSC_FIELDS 3
#define PLANAR_RGB_PACKING 3
#define GET_MODE_WIDTH(fsc_mode, mode) \
	(fsc_mode ? mode->hdisplay / PLANAR_RGB_PACKING : mode->hdisplay)
#define GET_MODE_HEIGHT(fsc_mode, mode) \
	(fsc_mode ? mode->vdisplay * NUM_FSC_FIELDS : mode->vdisplay)

/**
 * Encoder functions and data types
 * @intfs:	Interfaces this encoder is using, INTF_MODE_NONE if unused
 * @wbs:	Writebacks this encoder is using, INTF_MODE_NONE if unused
 * @needs_cdm:	Encoder requests a CDM based on pixel format conversion needs
 * @display_num_of_h_tiles: Number of horizontal tiles in case of split
 *                          interface
 * @display_type: Type of the display
 * @topology:   Topology of the display
 * @comp_info: Compression parameters information
 */
struct sde_encoder_hw_resources {
	enum sde_intf_mode intfs[INTF_MAX];
	enum sde_intf_mode wbs[WB_MAX];
	bool needs_cdm;
	u32 display_num_of_h_tiles;
	enum sde_connector_display display_type;
	struct msm_display_topology topology;
	struct msm_compression_info *comp_info;
};

/**
 * sde_encoder_kickoff_params - info encoder requires at kickoff
 * @affected_displays:  bitmask, bit set means the ROI of the commit lies within
 *                      the bounds of the physical display at the bit index
 * @recovery_events_enabled: indicates status of client for recoovery events
 * @frame_trigger_mode: indicates frame trigger mode
 */
struct sde_encoder_kickoff_params {
	unsigned long affected_displays;
	bool recovery_events_enabled;
	enum frame_trigger_mode_type frame_trigger_mode;
};

/*
 * enum sde_enc_rc_states - states that the resource control maintains
 * @SDE_ENC_RC_STATE_OFF: Resource is in OFF state
 * @SDE_ENC_RC_STATE_PRE_OFF: Resource is transitioning to OFF state
 * @SDE_ENC_RC_STATE_ON: Resource is in ON state
 * @SDE_ENC_RC_STATE_MODESET: Resource is in modeset state
 * @SDE_ENC_RC_STATE_IDLE: Resource is in IDLE state
 */
enum sde_enc_rc_states {
	SDE_ENC_RC_STATE_OFF,
	SDE_ENC_RC_STATE_PRE_OFF,
	SDE_ENC_RC_STATE_ON,
	SDE_ENC_RC_STATE_MODESET,
	SDE_ENC_RC_STATE_IDLE
};

/**
 * struct sde_encoder_virt - virtual encoder. Container of one or more physical
 *	encoders. Virtual encoder manages one "logical" display. Physical
 *	encoders manage one intf block, tied to a specific panel/sub-panel.
 *	Virtual encoder defers as much as possible to the physical encoders.
 *	Virtual encoder registers itself with the DRM Framework as the encoder.
 * @base:		drm_encoder base class for registration with DRM
 * @enc_spin_lock:	Virtual-Encoder-Wide Spin Lock for IRQ purposes
 * @bus_scaling_client:	Client handle to the bus scaling interface
 * @te_source:		vsync source pin information
 * @num_phys_encs:	Actual number of physical encoders contained.
 * @phys_encs:		Container of physical encoders managed.
 * @phys_vid_encs:	Video physical encoders for panel mode switch.
 * @phys_cmd_encs:	Command physical encoders for panel mode switch.
 * @cur_master:		Pointer to the current master in this mode. Optimization
 *			Only valid after enable. Cleared as disable.
 * @hw_pp		Handle to the pingpong blocks used for the display. No.
 *			pingpong blocks can be different than num_phys_encs.
 * @hw_dsc:		Array of DSC block handles used for the display.
 * @hw_vdc:		Array of VDC block handles used for the display.
 * @dirty_dsc_ids:	Cached dsc indexes for dirty DSC blocks needing flush
 * @intfs_swapped	Whether or not the phys_enc interfaces have been swapped
 *			for partial update right-only cases, such as pingpong
 *			split where virtual pingpong does not generate IRQs
 * @qdss_status:	indicate if qdss is modified since last update
 * @crtc_vblank_cb:	Callback into the upper layer / CRTC for
 *			notification of the VBLANK
 * @crtc_vblank_cb_data:	Data from upper layer for VBLANK notification
 * @crtc_kickoff_cb:		Callback into CRTC that will flush & start
 *				all CTL paths
 * @crtc_kickoff_cb_data:	Opaque user data given to crtc_kickoff_cb
 * @debugfs_root:		Debug file system root file node
 * @enc_lock:			Lock around physical encoder create/destroy and
				access.
 * @frame_done_cnt:		Atomic counter for tracking which phys_enc is
 *				done with frame processing
 * @crtc_frame_event_cb:	callback handler for frame event
 * @crtc_frame_event_cb_data:	callback handler private data
 * @rsc_client:			rsc client pointer
 * @rsc_state_init:		boolean to indicate rsc config init
 * @disp_info:			local copy of msm_display_info struct
 * @misr_enable:		misr enable/disable status
 * @misr_reconfigure:		boolean entry indicates misr reconfigure status
 * @misr_frame_count:		misr frame count before start capturing the data
 * @idle_pc_enabled:		indicate if idle power collapse is enabled
 *				currently. This can be controlled by user-mode
 * @rc_lock:			resource control mutex lock to protect
 *				virt encoder over various state changes
 * @rc_state:			resource controller state
 * @delayed_off_work:		delayed worker to schedule disabling of
 *				clks and resources after IDLE_TIMEOUT time.
 * @early_wakeup_work:		worker to handle early wakeup event
 * @input_event_work:		worker to handle input device touch events
 * @esd_trigger_work:		worker to handle esd trigger events
 * @input_handler:			handler for input device events
 * @topology:                   topology of the display
 * @vblank_enabled:		boolean to track userspace vblank vote
 * @idle_pc_restore:		flag to indicate idle_pc_restore happened
 * @frame_trigger_mode:		frame trigger mode indication for command mode
 *				display
 * @dynamic_hdr_updated:	flag to indicate if mempool was unchanged
 * @rsc_config:			rsc configuration for display vtotal, fps, etc.
 * @cur_conn_roi:		current connector roi
 * @prv_conn_roi:		previous connector roi to optimize if unchanged
 * @crtc			pointer to drm_crtc
 * @fal10_veto_override:	software override for micro idle fal10 veto
 * @recovery_events_enabled:	status of hw recovery feature enable by client
 * @elevated_ahb_vote:		increase AHB bus speed for the first frame
 *				after power collapse
 * @pm_qos_cpu_req:		qos request for all cpu core frequency
 * @valid_cpu_mask:		actual voted cpu core mask
 * @mode_info:                  stores the current mode and should be used
 *				only in commit phase
 * @delay_kickoff		boolean to delay the kickoff, used in case
 *				of esd attack to ensure esd workqueue detects
 *				the previous frame transfer completion before
 *				next update is triggered.
 * @autorefresh_solver_disable	It tracks if solver state is disabled from this
 *				encoder due to autorefresh concurrency.
 * @fps_switch_high_to_low:	boolean to note direction of fps switch
 * @update_clocks_on_complete_commit:	boolean to force update clocks
 */
struct sde_encoder_virt {
	struct drm_encoder base;
	spinlock_t enc_spinlock;
	struct mutex vblank_ctl_lock;
	uint32_t bus_scaling_client;

	uint32_t display_num_of_h_tiles;
	uint32_t te_source;

	unsigned int num_phys_encs;
	struct sde_encoder_phys *phys_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	struct sde_encoder_phys *phys_vid_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	struct sde_encoder_phys *phys_cmd_encs[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	struct sde_encoder_phys *cur_master;
	struct sde_hw_pingpong *hw_pp[MAX_CHANNELS_PER_ENC];
	struct sde_hw_dsc *hw_dsc[MAX_CHANNELS_PER_ENC];
	struct sde_hw_vdc *hw_vdc[MAX_CHANNELS_PER_ENC];
	struct sde_hw_pingpong *hw_dsc_pp[MAX_CHANNELS_PER_ENC];
	enum sde_dsc dirty_dsc_ids[MAX_CHANNELS_PER_ENC];
	enum sde_vdc dirty_vdc_ids[MAX_CHANNELS_PER_ENC];
	bool intfs_swapped;
	bool qdss_status;

	void (*crtc_vblank_cb)(void *data, ktime_t ts);
	void *crtc_vblank_cb_data;

	struct dentry *debugfs_root;
	struct mutex enc_lock;
	atomic_t frame_done_cnt[MAX_PHYS_ENCODERS_PER_VIRTUAL];
	void (*crtc_frame_event_cb)(void *data, u32 event, ktime_t ts);
	struct sde_kms_frame_event_cb_data crtc_frame_event_cb_data;

	struct sde_rsc_client *rsc_client;
	bool rsc_state_init;
	struct msm_display_info disp_info;
	bool misr_enable;
	bool misr_reconfigure;
	u32 misr_frame_count;

	bool idle_pc_enabled;
	bool input_event_enabled;
	struct mutex rc_lock;
	enum sde_enc_rc_states rc_state;
	struct kthread_delayed_work delayed_off_work;
	struct kthread_work early_wakeup_work;
	struct kthread_work input_event_work;
	struct kthread_work esd_trigger_work;
	struct input_handler *input_handler;
	bool vblank_enabled;
	bool idle_pc_restore;
	enum frame_trigger_mode_type frame_trigger_mode;
	bool dynamic_hdr_updated;

	struct sde_rsc_cmd_config rsc_config;
	struct sde_rect cur_conn_roi;
	struct sde_rect prv_conn_roi;
	struct drm_crtc *crtc;

	bool fal10_veto_override;
	bool recovery_events_enabled;
	bool elevated_ahb_vote;
	struct dev_pm_qos_request pm_qos_cpu_req[NR_CPUS];
	struct cpumask valid_cpu_mask;
	struct msm_mode_info mode_info;
	bool delay_kickoff;
	bool autorefresh_solver_disable;
	bool fps_switch_high_to_low;
	bool update_clocks_on_complete_commit;
	bool prepare_kickoff;
	bool ready_kickoff;
};

#define to_sde_encoder_virt(x) container_of(x, struct sde_encoder_virt, base)

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
 * sde_encoder_trigger_rsc_state_change - rsc state change.
 * @encoder:	encoder pointer
 */
void sde_encoder_trigger_rsc_state_change(struct drm_encoder *drm_enc);

/**
 * sde_encoder_early_wakeup - early wake up display
 * @encoder:	encoder pointer
 */
void sde_encoder_early_wakeup(struct drm_encoder *drm_enc);

/**
 * sde_encoder_register_vblank_callback - provide callback to encoder that
 *	will be called on the next vblank.
 * @encoder:	encoder pointer
 * @cb:		callback pointer, provide NULL to deregister and disable IRQs
 * @data:	user data provided to callback
 */
void sde_encoder_register_vblank_callback(struct drm_encoder *encoder,
		void (*cb)(void *, ktime_t), void *data);

/**
 * sde_encoder_register_frame_event_callback - provide callback to encoder that
 *	will be called after the request is complete, or other events.
 * @encoder:	encoder pointer
 * @cb:		callback pointer, provide NULL to deregister
 * @crtc:	pointer to drm_crtc object interested in frame events
 */
void sde_encoder_register_frame_event_callback(struct drm_encoder *encoder,
		void (*cb)(void *, u32, ktime_t), struct drm_crtc *crtc);

/**
 * sde_encoder_get_rsc_client - gets the rsc client state for primary
 *      for primary display.
 * @encoder:	encoder pointer
 */
struct sde_rsc_client *sde_encoder_get_rsc_client(struct drm_encoder *encoder);

/**
 * sde_encoder_poll_line_counts - poll encoder line counts for start of frame
 * @encoder:	encoder pointer
 * @Returns:	zero on success
 */
int sde_encoder_poll_line_counts(struct drm_encoder *encoder);

/**
 * sde_encoder_prepare_for_kickoff - schedule double buffer flip of the ctl
 *	path (i.e. ctl flush and start) at next appropriate time.
 *	Immediately: if no previous commit is outstanding.
 *	Delayed: Block until next trigger can be issued.
 * @encoder:	encoder pointer
 * @params:	kickoff time parameters
 * @old_crtc_state:	old crtc state pointer
 * @Returns:	Zero on success, last detected error otherwise
 */
int sde_encoder_prepare_for_kickoff(struct drm_encoder *encoder,
		struct sde_encoder_kickoff_params *params,
		struct drm_crtc_state *old_crtc_state);

/**
 * sde_encoder_trigger_kickoff_pending - Clear the flush bits from previous
 *        kickoff and trigger the ctl prepare progress for command mode display.
 * @encoder:	encoder pointer
 */
void sde_encoder_trigger_kickoff_pending(struct drm_encoder *encoder);

/**
 * sde_encoder_kickoff - trigger a double buffer flip of the ctl path
 *	(i.e. ctl flush and start) immediately.
 * @encoder:	encoder pointer
 * @config_changed: if true new configuration is applied on the control path
 */
void sde_encoder_kickoff(struct drm_encoder *encoder, bool config_changed);

/**
 * sde_encoder_wait_for_event - Waits for encoder events
 * @encoder:	encoder pointer
 * @event:      event to wait for
 * MSM_ENC_COMMIT_DONE -  Wait for hardware to have flushed the current pending
 *                        frames to hardware at a vblank or wr_ptr_start
 *                        Encoders will map this differently depending on the
 *                        panel type.
 *	                  vid mode -> vsync_irq
 *                        cmd mode -> wr_ptr_start_irq
 * MSM_ENC_TX_COMPLETE -  Wait for the hardware to transfer all the pixels to
 *                        the panel. Encoders will map this differently
 *                        depending on the panel type.
 *                        vid mode -> vsync_irq
 *                        cmd mode -> pp_done
 * Returns: 0 on success, -EWOULDBLOCK if already signaled, error otherwise
 */
int sde_encoder_wait_for_event(struct drm_encoder *drm_encoder,
						enum msm_event_wait event);

/**
 * sde_encoder_idle_request - request for idle request to avoid 4 vsync cycle
 *                            to turn off the clocks.
 * @encoder:	encoder pointer
 * Returns: 0 on success, errorcode otherwise
 */
int sde_encoder_idle_request(struct drm_encoder *drm_enc);


/**
 * sde_encoder_update_complete_commit - when there is DMS FPS switch in old_crtc_state,
 *					decrease DSI clocks as per low fps.
 * @drm_enc: encoder pointer
 * @old_state: old drm_crtc_state pointer
 */
void sde_encoder_update_complete_commit(struct drm_encoder *drm_enc,
		struct drm_crtc_state *old_state);

/*
 * sde_encoder_get_fps - get interface frame rate of the given encoder
 * @encoder: Pointer to drm encoder object
 */
u32 sde_encoder_get_fps(struct drm_encoder *encoder);

/*
 * sde_encoder_get_intf_mode - get interface mode of the given encoder
 * @encoder: Pointer to drm encoder object
 */
enum sde_intf_mode sde_encoder_get_intf_mode(struct drm_encoder *encoder);

/*
 * sde_encoder_get_frame_count - get hardware frame count of the given encoder
 * @encoder: Pointer to drm encoder object
 */
u32 sde_encoder_get_frame_count(struct drm_encoder *encoder);

/**
 * sde_encoder_get_avr_status - get combined avr_status from all intfs for given virt encoder
 * @drm_enc: Pointer to drm encoder structure
 */
int sde_encoder_get_avr_status(struct drm_encoder *drm_enc);

/*
 * sde_encoder_get_vblank_timestamp - get the last vsync timestamp
 * @encoder: Pointer to drm encoder object
 * @tvblank: vblank timestamp
 */
bool sde_encoder_get_vblank_timestamp(struct drm_encoder *encoder,
		ktime_t *tvblank);

/**
 * sde_encoder_control_te - control enabling/disabling VSYNC_IN_EN
 * @encoder:	encoder pointer
 * @enable:	boolean to indicate enable/disable
 */
void sde_encoder_control_te(struct drm_encoder *encoder, bool enable);

/**
 * sde_encoder_virt_restore - restore the encoder configs
 * @encoder:	encoder pointer
 */
void sde_encoder_virt_restore(struct drm_encoder *encoder);

/**
 * sde_encoder_is_dsc_merge - check if encoder is in DSC merge mode
 * @drm_enc: Pointer to drm encoder object
 * @Return: true if encoder is in DSC merge mode
 */
bool sde_encoder_is_dsc_merge(struct drm_encoder *drm_enc);

/**
 * sde_encoder_check_curr_mode - check if given mode is supported or not
 * @drm_enc: Pointer to drm encoder object
 * @mode: Mode to be checked
 * @Return: true if it is cmd mode
 */
bool sde_encoder_check_curr_mode(struct drm_encoder *drm_enc, u32 mode);

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

/**
 * sde_encoder_prepare_commit - prepare encoder at the very beginning of an
 *	atomic commit, before any registers are written
 * @drm_enc:    Pointer to previously created drm encoder structure
 */
int sde_encoder_prepare_commit(struct drm_encoder *drm_enc);

/**
 * sde_encoder_update_caps_for_cont_splash - update encoder settings during
 *	device bootup when cont_splash is enabled
 * @drm_enc:    Pointer to drm encoder structure
 * @splash_display: Pointer to sde_splash_display corresponding to this encoder
 * @enable:	boolean indicates enable or displae state of splash
 * @Return:     true if successful in updating the encoder structure
 */
int sde_encoder_update_caps_for_cont_splash(struct drm_encoder *encoder,
		struct sde_splash_display *splash_display, bool enable);

/**
 * sde_encoder_display_failure_notification - update sde encoder state for
 * esd timeout or other display failure notification. This event flows from
 * dsi, sde_connector to sde_encoder.
 *
 * This api must not be called from crtc_commit (display) thread because it
 * requests the flush work on same thread. It is called from esd check thread
 * based on current design.
 *
 *      TODO: manage the event at sde_kms level for forward processing.
 * @drm_enc:    Pointer to drm encoder structure
 * @skip_pre_kickoff:    Caller can avoid pre_kickoff if it is triggering this
 *                       event only to switch the panel TE to watchdog mode.
 * @Return:     true if successful in updating the encoder structure
 */
int sde_encoder_display_failure_notification(struct drm_encoder *enc,
	bool skip_pre_kickoff);

/**
 * sde_encoder_recovery_events_enabled - checks if client has enabled
 * sw recovery mechanism for this connector
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if enabled
 */
bool sde_encoder_recovery_events_enabled(struct drm_encoder *encoder);

/**
 * sde_encoder_enable_recovery_event - handler to enable the sw recovery
 * for this connector
 * @drm_enc:    Pointer to drm encoder structure
 */
void sde_encoder_enable_recovery_event(struct drm_encoder *encoder);
/**
 * sde_encoder_in_clone_mode - checks if underlying phys encoder is in clone
 *	mode or independent display mode. ref@ WB in Concurrent writeback mode.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if successful in updating the encoder structure
 */
bool sde_encoder_in_clone_mode(struct drm_encoder *enc);

/**
 * sde_encoder_set_clone_mode - cwb in wb phys enc is enabled.
 * drm_enc:	Pointer to drm encoder structure
 * drm_crtc_state:	Pointer to drm_crtc_state
 */
void sde_encoder_set_clone_mode(struct drm_encoder *drm_enc,
	 struct drm_crtc_state *crtc_state);

/*
 * sde_encoder_is_cwb_disabling - check if cwb encoder disable is pending
 * @drm_enc:    Pointer to drm encoder structure
 * @drm_crtc:    Pointer to drm crtc structure
 * @Return: true if cwb encoder disable is pending
 */
bool sde_encoder_is_cwb_disabling(struct drm_encoder *drm_enc,
	struct drm_crtc *drm_crtc);

/**
 * sde_encoder_is_primary_display - checks if underlying display is primary
 *     display or not.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if it is primary display. false otherwise
 */
bool sde_encoder_is_primary_display(struct drm_encoder *enc);

/**
 * sde_encoder_is_built_in_display - checks if underlying display is built in
 *     display or not.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if it is a built in display. false otherwise
 */
bool sde_encoder_is_built_in_display(struct drm_encoder *enc);

/**
 * sde_encoder_is_dsi_display - checks if underlying display is DSI
 *     display or not.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if it is a dsi display. false otherwise
 */
bool sde_encoder_is_dsi_display(struct drm_encoder *enc);

/**
 * sde_encoder_control_idle_pc - control enable/disable of idle power collapse
 * @drm_enc:    Pointer to drm encoder structure
 * @enable:	enable/disable flag
 */
void sde_encoder_control_idle_pc(struct drm_encoder *enc, bool enable);

/**
 * sde_encoder_in_cont_splash - checks if display is in continuous splash
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if display in continuous splash
 */
int sde_encoder_in_cont_splash(struct drm_encoder *enc);

/**
 * sde_encoder_helper_hw_reset - hw reset helper function
 * @drm_enc:    Pointer to drm encoder structure
 */
void sde_encoder_needs_hw_reset(struct drm_encoder *enc);

/**
 * sde_encoder_uidle_enable - control enable/disable of uidle
 * @drm_enc:    Pointer to drm encoder structure
 * @enable:	enable/disable flag
 */
void sde_encoder_uidle_enable(struct drm_encoder *drm_enc, bool enable);

/**
 * sde_encoder_irq_control - control enable/disable of IRQ's
 * @drm_enc:	Pointer to drm encoder structure
 * @enable: enable/disable flag
 */
void sde_encoder_irq_control(struct drm_encoder *drm_enc, bool enable);

/**sde_encoder_get_connector - get connector corresponding to encoder
 * @dev:	Pointer to drm device structure
 * @drm_enc:	Pointer to drm encoder structure
 * Returns:	drm connector if found, null if not found
 */
struct drm_connector *sde_encoder_get_connector(struct drm_device *dev,
			struct drm_encoder *drm_enc);

/**sde_encoder_needs_dsc_disable - indicates if dsc should be disabled
 *			based on previous topology
 * @drm_enc:	Pointer to drm encoder structure
 */
bool sde_encoder_needs_dsc_disable(struct drm_encoder *drm_enc);

/**
 * sde_encoder_get_transfer_time - get the mdp transfer time in usecs
 * @drm_enc: Pointer to drm encoder structure
 * @transfer_time_us: Pointer to store the output value
 */
void sde_encoder_get_transfer_time(struct drm_encoder *drm_enc,
		u32 *transfer_time_us);

/*
 * sde_encoder_get_dfps_maxfps - get dynamic FPS max frame rate of
				the given encoder
 * @encoder: Pointer to drm encoder object
 */
static inline u32 sde_encoder_get_dfps_maxfps(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc) {
		SDE_ERROR("invalid encoder\n");
		return 0;
	}

	sde_enc = to_sde_encoder_virt(drm_enc);

	return sde_enc->mode_info.dfps_maxfps;
}

/**
 * sde_encoder_virt_reset - delay encoder virt reset
 * @drm_enc:	Pointer to drm encoder structure
 */
void sde_encoder_virt_reset(struct drm_encoder *drm_enc);

/**
 * sde_encoder_calc_last_vsync_timestamp - read last HW vsync timestamp counter
 *         and calculate the corresponding vsync ktime. Return ktime_get
 *         when HW support is not available
 * @drm_enc:    Pointer to drm encoder structure
 */
ktime_t sde_encoder_calc_last_vsync_timestamp(struct drm_encoder *drm_enc);

/**
 * sde_encoder_cancel_delayed_work - cancel delayed off work for encoder
 * @drm_enc:    Pointer to drm encoder structure
 */
void sde_encoder_cancel_delayed_work(struct drm_encoder *encoder);

/**
 * sde_encoder_get_kms - retrieve the kms from encoder
 * @drm_enc:    Pointer to drm encoder structure
 */
static inline struct sde_kms *sde_encoder_get_kms(struct drm_encoder *drm_enc)
{
	struct msm_drm_private *priv;

	if (!drm_enc || !drm_enc->dev) {
		SDE_ERROR("invalid encoder\n");
		return NULL;
	}
	priv = drm_enc->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid kms\n");
		return NULL;
	}

	return to_sde_kms(priv->kms);
}

/*
 * sde_encoder_is_widebus_enabled - check if widebus is enabled for current mode
 * @drm_enc:    Pointer to drm encoder structure
 * @Return: true if widebus is enabled for current mode
 */
static inline bool sde_encoder_is_widebus_enabled(struct drm_encoder *drm_enc)
{
	struct sde_encoder_virt *sde_enc;

	if (!drm_enc)
		return false;

	sde_enc = to_sde_encoder_virt(drm_enc);
	return sde_enc->mode_info.wide_bus_en;
}

/**
 * sde_crtc_has_fps_switch_to_low_set - return fps_switch_high_to_low in sde enc
 * @crtc:	Pointer to drm crtc structure
 * @Return:	true if there is fps_switch from high_to_low
 */
bool sde_crtc_has_fps_switch_to_low_set(struct drm_crtc *crtc);

void sde_encoder_add_data_to_minidump_va(struct drm_encoder *drm_enc);
#endif /* __SDE_ENCODER_H__ */
