/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
#include "sde_kms.h"
#include "sde_connector.h"

#define MAX_CHANNELS_PER_ENC 2

#define SDE_ENCODER_FRAME_EVENT_DONE			BIT(0)
#define SDE_ENCODER_FRAME_EVENT_ERROR			BIT(1)
#define SDE_ENCODER_FRAME_EVENT_PANEL_DEAD		BIT(2)
#define SDE_ENCODER_FRAME_EVENT_SIGNAL_RELEASE_FENCE	BIT(3)
#define SDE_ENCODER_FRAME_EVENT_SIGNAL_RETIRE_FENCE	BIT(4)
#define SDE_ENCODER_FRAME_EVENT_CWB_DONE		BIT(5)

#define IDLE_POWERCOLLAPSE_DURATION	(66 - 16/2)
#define IDLE_POWERCOLLAPSE_IN_EARLY_WAKEUP (200 - 16/2)

/**
 * Encoder functions and data types
 * @intfs:	Interfaces this encoder is using, INTF_MODE_NONE if unused
 * @wbs:	Writebacks this encoder is using, INTF_MODE_NONE if unused
 * @needs_cdm:	Encoder requests a CDM based on pixel format conversion needs
 * @display_num_of_h_tiles: Number of horizontal tiles in case of split
 *                          interface
 * @display_type: Type of the display
 * @topology:   Topology of the display
 */
struct sde_encoder_hw_resources {
	enum sde_intf_mode intfs[INTF_MAX];
	enum sde_intf_mode wbs[WB_MAX];
	bool needs_cdm;
	u32 display_num_of_h_tiles;
	enum sde_connector_display display_type;
	struct msm_display_topology topology;
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

/**
 * struct sde_encoder_ops - callback functions for generic sde encoder
 * Individual callbacks documented below.
 */
struct sde_encoder_ops {
	/**
	 * phys_init - phys initialization function
	 * @type: controller type
	 * @controller_id: controller id
	 * @phys_init_params: Pointer of structure sde_enc_phys_init_params
	 * Returns: Pointer of sde_encoder_phys, NULL if failed
	 */
	void *(*phys_init)(enum sde_intf_type type,
			u32 controller_id, void *phys_init_params);
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
 * @crtc:	pointer to drm_crtc object interested in frame events
 */
void sde_encoder_register_frame_event_callback(struct drm_encoder *encoder,
		void (*cb)(void *, u32), struct drm_crtc *crtc);

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
 * @Returns:	Zero on success, last detected error otherwise
 */
int sde_encoder_prepare_for_kickoff(struct drm_encoder *encoder,
		struct sde_encoder_kickoff_params *params);

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
 * @is_error:	whether the current commit needs to be aborted and replaced
 *		with a 'safe' commit
 */
void sde_encoder_kickoff(struct drm_encoder *encoder, bool is_error);

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
 * sde_encoder_init_with_ops - initialize virtual encoder object with init ops
 * @dev:        Pointer to drm device structure
 * @disp_info:  Pointer to display information structure
 * @ops:        Pointer to encoder ops structure
 * Returns:     Pointer to newly created drm encoder
 */
struct drm_encoder *sde_encoder_init_with_ops(
		struct drm_device *dev,
		struct msm_display_info *disp_info,
		const struct sde_encoder_ops *ops);

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
 * sde_encoder_recovery_events_handler - handler to enable/disable the
 * sw recovery for this connector
 * @drm_enc:    Pointer to drm encoder structure
 */
void sde_encoder_recovery_events_handler(struct drm_encoder *encoder,
		bool val);
/**
 * sde_encoder_in_clone_mode - checks if underlying phys encoder is in clone
 *	mode or independent display mode. ref@ WB in Concurrent writeback mode.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if successful in updating the encoder structure
 */
bool sde_encoder_in_clone_mode(struct drm_encoder *enc);

/**
 * sde_encoder_is_primary_display - checks if underlying display is primary
 *     display or not.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if it is primary display. false if secondary display
 */
bool sde_encoder_is_primary_display(struct drm_encoder *enc);

/**
 * sde_encoder_is_dsi_display - checks if underlying display is DSI
 *     display or not.
 * @drm_enc:    Pointer to drm encoder structure
 * @Return:     true if it is primary display. false if secondary display
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

#endif /* __SDE_ENCODER_H__ */
