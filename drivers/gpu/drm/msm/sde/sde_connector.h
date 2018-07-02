/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_panel.h>

#include "msm_drv.h"
#include "msm_prop.h"
#include "sde_kms.h"
#include "sde_fence.h"

#define SDE_MODE_HPD_ON        0
#define SDE_MODE_HPD_OFF       1

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
	 * pre_deinit - perform additional deinitialization steps
	 * @connector: Pointer to drm connector structure
	 * @display: Pointer to private display handle
	 * Returns: Zero on success
	 */
	int (*pre_deinit)(struct drm_connector *connector,
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

	int (*set_backlight)(void *display, u32 bl_lvl);


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
	 * mode_needs_full_range - does the mode need full range
	 * quantization
	 * @display: Pointer to private display structure
	 * Returns: true or false based on whether full range is needed
	 */
	bool (*mode_needs_full_range)(void *display);

	/**
	 * get_csc_type - returns the CSC type to be used
	 * by the CDM block based on HDR state
	 * @connector: Pointer to drm connector structure
	 * @display: Pointer to private display structure
	 * Returns: type of CSC matrix to be used
	 */
	enum sde_csc_type (*get_csc_type)(struct drm_connector *connector,
		void *display);

	/**
	 * set_power - update dpms setting
	 * @connector: Pointer to drm connector structure
	 * @power_mode: One of the following,
	 *		SDE_MODE_DPMS_ON
	 *		SDE_MODE_DPMS_LP1
	 *		SDE_MODE_DPMS_LP2
	 *		SDE_MODE_DPMS_OFF
	 * @display: Pointer to private display structure
	 * Returns: Zero on success
	 */
	int (*set_power)(struct drm_connector *connector,
			int power_mode, void *display);
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
 * @retire_fence: Retire fence reference
 * @ops: Local callback function pointer table
 * @dpms_mode: DPMS property setting from user space
 * @lp_mode: LP property setting from user space
 * @last_panel_power_mode: Last consolidated dpms/lp mode setting
 * @property_info: Private structure for generic property handling
 * @property_data: Array of private data for generic property handling
 * @blob_caps: Pointer to blob structure for 'capabilities' property
 * @blob_hdr: Pointer to blob structure for 'hdr_properties' property
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
	struct sde_fence retire_fence;
	struct sde_connector_ops ops;
	int dpms_mode;
	u64 hpd_mode;
	int lp_mode;
	int last_panel_power_mode;

	struct msm_property_info property_info;
	struct msm_property_data property_data[CONNECTOR_PROP_COUNT];
	struct drm_property_blob *blob_caps;
	struct drm_property_blob *blob_hdr;
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
 * @aspace: Address space for accessing frame buffer objects, if applicable
 * @property_values: Local cache of current connector property values
 * @hdr_ctrl: HDR control info passed from userspace
 */
struct sde_connector_state {
	struct drm_connector_state base;
	struct drm_framebuffer *out_fb;
	struct msm_gem_address_space *aspace;
	uint64_t property_values[CONNECTOR_PROP_COUNT];
	struct drm_msm_ext_panel_hdr_ctrl hdr_ctrl;
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
	 (to_sde_connector_state((S))->property_values[(X)]) : 0)

/**
 * sde_connector_get_property_values - retrieve property values cache
 * @S: Pointer to drm connector state
 * Returns: Integer value of requested property
 */
#define sde_connector_get_property_values(S) \
	((S) ? (to_sde_connector_state((S))->property_values) : NULL)

/**
 * sde_connector_get_out_fb - query out_fb value from sde connector state
 * @S: Pointer to drm connector state
 * Returns: Output fb associated with specified connector state
 */
#define sde_connector_get_out_fb(S) \
	((S) ? to_sde_connector_state((S))->out_fb : NULL)

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
 */
void sde_connector_complete_commit(struct drm_connector *connector);

/**
 * sde_connector_get_info - query display specific information
 * @connector: Pointer to drm connector object
 * @info: Pointer to msm display information structure
 * Returns: Zero on success
 */
int sde_connector_get_info(struct drm_connector *connector,
		struct msm_display_info *info);

/**
 * sde_connector_pre_kickoff - trigger kickoff time feature programming
 * @connector: Pointer to drm connector object
 * Returns: Zero on success
 */
int sde_connector_pre_kickoff(struct drm_connector *connector);

/**
 * sde_connector_mode_needs_full_range - query quantization type
 * for the connector mode
 * @connector: Pointer to drm connector object
 * Returns: true OR false based on connector mode
 */
bool sde_connector_mode_needs_full_range(struct drm_connector *connector);

/**
 * sde_connector_get_csc_type - query csc type
 * to be used for the connector
 * @connector: Pointer to drm connector object
 * Returns: csc type based on connector HDR state
 */
enum sde_csc_type sde_connector_get_csc_type(struct drm_connector *conn);

/**
 * sde_connector_get_dpms - query dpms setting
 * @connector: Pointer to drm connector structure
 * Returns: Current DPMS setting for connector
 */
int sde_connector_get_dpms(struct drm_connector *connector);

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

#endif /* _SDE_CONNECTOR_H_ */

