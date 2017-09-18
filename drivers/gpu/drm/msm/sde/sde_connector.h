/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_CONNECTOR_H_
#define _SDE_CONNECTOR_H_

#include <uapi/drm/msm_drm_pp.h>
#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_panel.h>

#include "msm_drv.h"
#include "msm_prop.h"
#include "sde_kms.h"
#include "sde_fence.h"

#define SDE_CONNECTOR_NAME_SIZE	16

struct sde_connector;
struct sde_connector_state;

/**
 * struct sde_connector_ops - callback functions for generic sde connector
 * Individual callbacks documented below.
 */
struct sde_connector_ops {
	/**
	 * post_init - perform additional initialization steps
	 * @connector: Pointer to drm connector structure
	 * @info: Pointer to sde connector info structure
	 * @display: Pointer to private display handle
	 * Returns: Zero on success
	 */
	int (*post_init)(struct drm_connector *connector,
			void *info,
			void *display);

	/**
	 * detect - determine if connector is connected
	 * @connector: Pointer to drm connector structure
	 * @force: Force detect setting from drm framework
	 * @display: Pointer to private display handle
	 * Returns: Connector 'is connected' status
	 */
	enum drm_connector_status (*detect)(struct drm_connector *connector,
			bool force,
			void *display);

	/**
	 * get_modes - add drm modes via drm_mode_probed_add()
	 * @connector: Pointer to drm connector structure
	 * @display: Pointer to private display handle
	 * Returns: Number of modes added
	 */
	int (*get_modes)(struct drm_connector *connector,
			void *display);

	/**
	 * put_modes - free up drm modes of the connector
	 * @connector: Pointer to drm connector structure
	 * @display: Pointer to private display handle
	 */
	void (*put_modes)(struct drm_connector *connector,
			void *display);

	/**
	 * update_pps - update pps command for the display panel
	 * @pps_cmd: Pointer to pps command
	 * @display: Pointer to private display handle
	 * Returns: Zero on success
	 */
	int (*update_pps)(char *pps_cmd, void *display);

	/**
	 * mode_valid - determine if specified mode is valid
	 * @connector: Pointer to drm connector structure
	 * @mode: Pointer to drm mode structure
	 * @display: Pointer to private display handle
	 * Returns: Validity status for specified mode
	 */
	enum drm_mode_status (*mode_valid)(struct drm_connector *connector,
			struct drm_display_mode *mode,
			void *display);

	/**
	 * set_property - set property value
	 * @connector: Pointer to drm connector structure
	 * @state: Pointer to drm connector state structure
	 * @property_index: DRM property index
	 * @value: Incoming property value
	 * @display: Pointer to private display structure
	 * Returns: Zero on success
	 */
	int (*set_property)(struct drm_connector *connector,
			struct drm_connector_state *state,
			int property_index,
			uint64_t value,
			void *display);

	/**
	 * get_property - get property value
	 * @connector: Pointer to drm connector structure
	 * @state: Pointer to drm connector state structure
	 * @property_index: DRM property index
	 * @value: Pointer to variable for accepting property value
	 * @display: Pointer to private display structure
	 * Returns: Zero on success
	 */
	int (*get_property)(struct drm_connector *connector,
			struct drm_connector_state *state,
			int property_index,
			uint64_t *value,
			void *display);

	/**
	 * get_info - get display information
	 * @info: Pointer to msm display info structure
	 * @display: Pointer to private display structure
	 * Returns: Zero on success
	 */
	int (*get_info)(struct msm_display_info *info, void *display);

	/**
	 * get_mode_info - retrieve mode information
	 * @drm_mode: Display mode set for the display
	 * @mode_info: Out parameter. information of the display mode
	 * @max_mixer_width: max width supported by HW layer mixer
	 * @display: Pointer to private display structure
	 * Returns: Zero on success
	 */
	int (*get_mode_info)(const struct drm_display_mode *drm_mode,
			struct msm_mode_info *mode_info,
			u32 max_mixer_width, void *display);

	/**
	 * enable_event - notify display of event registration/unregistration
	 * @connector: Pointer to drm connector structure
	 * @event_idx: SDE connector event index
	 * @enable: Whether the event is being enabled/disabled
	 * @display: Pointer to private display structure
	 */
	void (*enable_event)(struct drm_connector *connector,
			uint32_t event_idx, bool enable, void *display);

	int (*set_backlight)(void *display, u32 bl_lvl);

	/**
	 * soft_reset - perform a soft reset on the connector
	 * @display: Pointer to private display structure
	 * Return: Zero on success, -ERROR otherwise
	 */
	int (*soft_reset)(void *display);

	/**
	 * pre_kickoff - trigger display to program kickoff-time features
	 * @connector: Pointer to drm connector structure
	 * @display: Pointer to private display structure
	 * @params: Parameter bundle of connector-stored information for
	 *	kickoff-time programming into the display
	 * Returns: Zero on success
	 */
	int (*pre_kickoff)(struct drm_connector *connector,
			void *display,
			struct msm_display_kickoff_params *params);

	/**
	 * clk_ctrl - perform clk enable/disable on the connector
	 * @handle: Pointer to clk handle
	 * @type: Type of clks
	 * @enable: State of clks
	 */
	int (*clk_ctrl)(void *handle, u32 type, u32 state);

	/**
	 * set_power - update dpms setting
	 * @connector: Pointer to drm connector structure
	 * @power_mode: One of the following,
	 *              SDE_MODE_DPMS_ON
	 *              SDE_MODE_DPMS_LP1
	 *              SDE_MODE_DPMS_LP2
	 *              SDE_MODE_DPMS_OFF
	 * @display: Pointer to private display structure
	 * Returns: Zero on success
	 */
	int (*set_power)(struct drm_connector *connector,
			int power_mode, void *display);

	/**
	 * get_dst_format - get dst_format from display
	 * @display: Pointer to private display handle
	 * Returns: dst_format of display
	 */
	enum dsi_pixel_format (*get_dst_format)(void *display);

	/**
	 * post_kickoff - display to program post kickoff-time features
	 * @connector: Pointer to drm connector structure
	 * Returns: Zero on success
	 */
	int (*post_kickoff)(struct drm_connector *connector);

	/**
	 * send_hpd_event - send HPD uevent notification to userspace
	 * @display: Pointer to private display structure
	 */
	void (*send_hpd_event)(void *display);

	/**
	 * check_status - check status of connected display panel
	 * @display: Pointer to private display handle
	 * Returns: positive value for success, negetive or zero for failure
	 */
	int (*check_status)(void *display);
};

/**
 * enum sde_connector_events - list of recognized connector events
 */
enum sde_connector_events {
	SDE_CONN_EVENT_VID_DONE, /* video mode frame done */
	SDE_CONN_EVENT_CMD_DONE, /* command mode frame done */
	SDE_CONN_EVENT_COUNT,
};

/**
 * struct sde_connector_evt - local event registration entry structure
 * @cb_func: Pointer to desired callback function
 * @usr: User pointer to pass to callback on event trigger
 */
struct sde_connector_evt {
	void (*cb_func)(uint32_t event_idx,
			uint32_t instance_idx, void *usr,
			uint32_t data0, uint32_t data1,
			uint32_t data2, uint32_t data3);
	void *usr;
};

/**
 * struct sde_connector - local sde connector structure
 * @base: Base drm connector structure
 * @connector_type: Set to one of DRM_MODE_CONNECTOR_ types
 * @encoder: Pointer to preferred drm encoder
 * @panel: Pointer to drm panel, if present
 * @display: Pointer to private display data structure
 * @mmu_secure: MMU id for secure buffers
 * @mmu_unsecure: MMU id for unsecure buffers
 * @name: ASCII name of connector
 * @lock: Mutex lock object for this structure
 * @retire_fence: Retire fence context reference
 * @ops: Local callback function pointer table
 * @dpms_mode: DPMS property setting from user space
 * @lp_mode: LP property setting from user space
 * @last_panel_power_mode: Last consolidated dpms/lp mode setting
 * @property_info: Private structure for generic property handling
 * @property_data: Array of private data for generic property handling
 * @blob_caps: Pointer to blob structure for 'capabilities' property
 * @blob_hdr: Pointer to blob structure for 'hdr_properties' property
 * @blob_dither: Pointer to blob structure for default dither config
 * @fb_kmap: true if kernel mapping of framebuffer is requested
 * @event_table: Array of registered events
 * @event_lock: Lock object for event_table
 * @bl_device: backlight device node
 * @status_work: work object to perform status checks
 * @force_panel_dead: variable to trigger forced ESD recovery
 */
struct sde_connector {
	struct drm_connector base;

	int connector_type;

	struct drm_encoder *encoder;
	struct drm_panel *panel;
	void *display;

	struct msm_gem_address_space *aspace[SDE_IOMMU_DOMAIN_MAX];

	char name[SDE_CONNECTOR_NAME_SIZE];

	struct mutex lock;
	struct sde_fence_context retire_fence;
	struct sde_connector_ops ops;
	int dpms_mode;
	int lp_mode;
	int last_panel_power_mode;

	struct msm_property_info property_info;
	struct msm_property_data property_data[CONNECTOR_PROP_COUNT];
	struct drm_property_blob *blob_caps;
	struct drm_property_blob *blob_hdr;
	struct drm_property_blob *blob_dither;

	bool fb_kmap;
	struct sde_connector_evt event_table[SDE_CONN_EVENT_COUNT];
	spinlock_t event_lock;

	struct backlight_device *bl_device;
	struct delayed_work status_work;
	u32 force_panel_dead;
};

/**
 * to_sde_connector - convert drm_connector pointer to sde connector pointer
 * @X: Pointer to drm_connector structure
 * Returns: Pointer to sde_connector structure
 */
#define to_sde_connector(x)     container_of((x), struct sde_connector, base)

/**
 * sde_connector_get_display - get sde connector's private display pointer
 * @C: Pointer to drm connector structure
 * Returns: Pointer to associated private display structure
 */
#define sde_connector_get_display(C) \
	((C) ? to_sde_connector((C))->display : NULL)

/**
 * sde_connector_get_panel - get sde connector's private panel pointer
 * @C: Pointer to drm connector structure
 * Returns: Pointer to associated private display structure
 */
#define sde_connector_get_panel(C) \
	((C) ? to_sde_connector((C))->panel : NULL)

/**
 * sde_connector_get_encoder - get sde connector's private encoder pointer
 * @C: Pointer to drm connector structure
 * Returns: Pointer to associated private encoder structure
 */
#define sde_connector_get_encoder(C) \
	((C) ? to_sde_connector((C))->encoder : NULL)

/**
 * sde_connector_get_propinfo - get sde connector's property info pointer
 * @C: Pointer to drm connector structure
 * Returns: Pointer to associated private property info structure
 */
#define sde_connector_get_propinfo(C) \
	((C) ? &to_sde_connector((C))->property_info : NULL)

/**
 * struct sde_connector_state - private connector status structure
 * @base: Base drm connector structure
 * @out_fb: Pointer to output frame buffer, if applicable
 * @property_state: Local storage for msm_prop properties
 * @property_values: Local cache of current connector property values
 * @rois: Regions of interest structure for mapping CRTC to Connector output
 * @property_blobs: blob properties
 */
struct sde_connector_state {
	struct drm_connector_state base;
	struct drm_framebuffer *out_fb;
	struct msm_property_state property_state;
	struct msm_property_value property_values[CONNECTOR_PROP_COUNT];

	struct msm_roi_list rois;
	struct drm_property_blob *property_blobs[CONNECTOR_PROP_BLOBCOUNT];
};

/**
 * to_sde_connector_state - convert drm_connector_state pointer to
 *                          sde connector state pointer
 * @X: Pointer to drm_connector_state structure
 * Returns: Pointer to sde_connector_state structure
 */
#define to_sde_connector_state(x) \
	container_of((x), struct sde_connector_state, base)

/**
 * sde_connector_get_property - query integer value of connector property
 * @S: Pointer to drm connector state
 * @X: Property index, from enum msm_mdp_connector_property
 * Returns: Integer value of requested property
 */
#define sde_connector_get_property(S, X) \
	((S) && ((X) < CONNECTOR_PROP_COUNT) ? \
	 (to_sde_connector_state((S))->property_values[(X)].value) : 0)

/**
 * sde_connector_get_property_state - retrieve property state cache
 * @S: Pointer to drm connector state
 * Returns: Pointer to local property state structure
 */
#define sde_connector_get_property_state(S) \
	((S) ? (&to_sde_connector_state((S))->property_state) : NULL)

/**
 * sde_connector_get_out_fb - query out_fb value from sde connector state
 * @S: Pointer to drm connector state
 * Returns: Output fb associated with specified connector state
 */
#define sde_connector_get_out_fb(S) \
	((S) ? to_sde_connector_state((S))->out_fb : 0)

/**
 * sde_connector_get_topology_name - helper accessor to retrieve topology_name
 * @connector: pointer to drm connector
 * Returns: value of the CONNECTOR_PROP_TOPOLOGY_NAME property or 0
 */
static inline uint64_t sde_connector_get_topology_name(
		struct drm_connector *connector)
{
	if (!connector || !connector->state)
		return 0;
	return sde_connector_get_property(connector->state,
			CONNECTOR_PROP_TOPOLOGY_NAME);
}

/**
 * sde_connector_get_lp - helper accessor to retrieve LP state
 * @connector: pointer to drm connector
 * Returns: value of the CONNECTOR_PROP_LP property or 0
 */
static inline uint64_t sde_connector_get_lp(
		struct drm_connector *connector)
{
	if (!connector || !connector->state)
		return 0;
	return sde_connector_get_property(connector->state,
			CONNECTOR_PROP_LP);
}

/**
 * sde_connector_set_property_for_commit - add property set to atomic state
 *	Add a connector state property update for the specified property index
 *	to the atomic state in preparation for a drm_atomic_commit.
 * @connector: Pointer to drm connector
 * @atomic_state: Pointer to DRM atomic state structure for commit
 * @property_idx: Connector property index
 * @value: Updated property value
 * Returns: Zero on success
 */
int sde_connector_set_property_for_commit(struct drm_connector *connector,
		struct drm_atomic_state *atomic_state,
		uint32_t property_idx, uint64_t value);

/**
 * sde_connector_init - create drm connector object for a given display
 * @dev: Pointer to drm device struct
 * @encoder: Pointer to associated encoder
 * @panel: Pointer to associated panel, can be NULL
 * @display: Pointer to associated display object
 * @ops: Pointer to callback operations function table
 * @connector_poll: Set to appropriate DRM_CONNECTOR_POLL_ setting
 * @connector_type: Set to appropriate DRM_MODE_CONNECTOR_ type
 * Returns: Pointer to newly created drm connector struct
 */
struct drm_connector *sde_connector_init(struct drm_device *dev,
		struct drm_encoder *encoder,
		struct drm_panel *panel,
		void *display,
		const struct sde_connector_ops *ops,
		int connector_poll,
		int connector_type);

/**
 * sde_connector_prepare_fence - prepare fence support for current commit
 * @connector: Pointer to drm connector object
 */
void sde_connector_prepare_fence(struct drm_connector *connector);

/**
 * sde_connector_complete_commit - signal completion of current commit
 * @connector: Pointer to drm connector object
 * @ts: timestamp to be updated in the fence signalling
 */
void sde_connector_complete_commit(struct drm_connector *connector, ktime_t ts);

/**
 * sde_connector_commit_reset - reset the completion signal
 * @connector: Pointer to drm connector object
 * @ts: timestamp to be updated in the fence signalling
 */
void sde_connector_commit_reset(struct drm_connector *connector, ktime_t ts);

/**
 * sde_connector_get_info - query display specific information
 * @connector: Pointer to drm connector object
 * @info: Pointer to msm display information structure
 * Returns: Zero on success
 */
int sde_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info);

/**
 * sde_connector_clk_ctrl - enables/disables the connector clks
 * @connector: Pointer to drm connector object
 * @enable: true/false to enable/disable
 */
void sde_connector_clk_ctrl(struct drm_connector *connector, bool enable);

/**
 * sde_connector_get_dpms - query dpms setting
 * @connector: Pointer to drm connector structure
 * Returns: Current DPMS setting for connector
 */
int sde_connector_get_dpms(struct drm_connector *connector);

/**
 * sde_connector_trigger_event - indicate that an event has occurred
 *	Any callbacks that have been registered against this event will
 *	be called from the same thread context.
 * @connector: Pointer to drm connector structure
 * @event_idx: Index of event to trigger
 * @instance_idx: Event-specific "instance index" to pass to callback
 * @data0: Event-specific "data" to pass to callback
 * @data1: Event-specific "data" to pass to callback
 * @data2: Event-specific "data" to pass to callback
 * @data3: Event-specific "data" to pass to callback
 * Returns: Zero on success
 */
int sde_connector_trigger_event(void *drm_connector,
		uint32_t event_idx, uint32_t instance_idx,
		uint32_t data0, uint32_t data1,
		uint32_t data2, uint32_t data3);

/**
 * sde_connector_register_event - register a callback function for an event
 * @connector: Pointer to drm connector structure
 * @event_idx: Index of event to register
 * @cb_func: Pointer to desired callback function
 * @usr: User pointer to pass to callback on event trigger
 * Returns: Zero on success
 */
int sde_connector_register_event(struct drm_connector *connector,
		uint32_t event_idx,
		void (*cb_func)(uint32_t event_idx,
			uint32_t instance_idx, void *usr,
			uint32_t data0, uint32_t data1,
			uint32_t data2, uint32_t data3),
		void *usr);

/**
 * sde_connector_unregister_event - unregister all callbacks for an event
 * @connector: Pointer to drm connector structure
 * @event_idx: Index of event to register
 */
void sde_connector_unregister_event(struct drm_connector *connector,
		uint32_t event_idx);

/**
 * sde_connector_register_custom_event - register for async events
 * @kms: Pointer to sde_kms
 * @conn_drm: Pointer to drm connector object
 * @event: Event for which request is being sent
 * @en: Flag to enable/disable the event
 * Returns: Zero on success
 */
int sde_connector_register_custom_event(struct sde_kms *kms,
		struct drm_connector *conn_drm, u32 event, bool en);

/**
 * sde_connector_pre_kickoff - trigger kickoff time feature programming
 * @connector: Pointer to drm connector object
 * Returns: Zero on success
 */
int sde_connector_pre_kickoff(struct drm_connector *connector);

/**
 * sde_connector_needs_offset - adjust the output fence offset based on
 *                              display type
 * @connector: Pointer to drm connector object
 * Returns: true if offset is required, false for all other cases.
 */
static inline bool sde_connector_needs_offset(struct drm_connector *connector)
{
	struct sde_connector *c_conn;

	if (!connector)
		return false;

	c_conn = to_sde_connector(connector);
	return (c_conn->connector_type != DRM_MODE_CONNECTOR_VIRTUAL);
}

/**
 * sde_connector_get_dither_cfg - get dither property data
 * @conn: Pointer to drm_connector struct
 * @state: Pointer to drm_connector_state struct
 * @cfg: Pointer to pointer to dither cfg
 * @len: length of the dither data
 * Returns: Zero on success
 */
int sde_connector_get_dither_cfg(struct drm_connector *conn,
		struct drm_connector_state *state, void **cfg, size_t *len);

/**
 * sde_connector_schedule_status_work - manage ESD thread
 * conn: Pointer to drm_connector struct
 * @en: flag to start/stop ESD thread
 */
void sde_connector_schedule_status_work(struct drm_connector *conn, bool en);

#endif /* _SDE_CONNECTOR_H_ */
