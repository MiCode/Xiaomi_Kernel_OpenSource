/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_WB_H__
#define __SDE_WB_H__

#include <linux/platform_device.h>

#include "msm_kms.h"
#include "sde_kms.h"
#include "sde_connector.h"

/**
 * struct sde_wb_device - Writeback device context
 * @drm_dev:		Pointer to controlling DRM device
 * @index:		Index of hardware instance from device tree
 * @wb_idx:		Writeback identifier of enum sde_wb
 * @wb_cfg:		Writeback configuration catalog
 * @name:		Name of writeback device from device tree
 * @display_type:	Display type from device tree
 * @wb_list		List of all writeback devices
 * @wb_lock		Serialization lock for writeback context structure
 * @connector:		Connector associated with writeback device
 * @encoder:		Encoder associated with writeback device
 * @max_mixer_width:    Max width supported by SDE LM HW block
 * @count_modes:	Length of writeback connector modes array
 * @modes:		Writeback connector modes array
 */
struct sde_wb_device {
	struct drm_device *drm_dev;

	u32 index;
	u32 wb_idx;
	struct sde_wb_cfg *wb_cfg;
	const char *name;

	struct list_head wb_list;
	struct mutex wb_lock;

	struct drm_connector *connector;
	struct drm_encoder *encoder;

	enum drm_connector_status detect_status;
	u32 max_mixer_width;

	u32 count_modes;
	struct drm_mode_modeinfo *modes;
};

/**
 * sde_wb_get_index - get device index of the given writeback device
 * @wb_dev:	Pointer to writeback device
 * Returns:	Index of hardware instance
 */
static inline
int sde_wb_get_index(struct sde_wb_device *wb_dev)
{
	return wb_dev ? wb_dev->index : -1;
}

#ifdef CONFIG_DRM_SDE_WB
/**
 * sde_wb_get_output_fb - get framebuffer in current atomic state
 * @wb_dev:	Pointer to writeback device
 * Returns:	Pointer to framebuffer
 */
struct drm_framebuffer *sde_wb_get_output_fb(struct sde_wb_device *wb_dev);

/**
 * sde_wb_get_output_roi - get region-of-interest in current atomic state
 * @wb_dev:	Pointer to writeback device
 * @roi:	Pointer to region of interest
 * Returns:	0 if success; error code otherwise
 */
int sde_wb_get_output_roi(struct sde_wb_device *wb_dev, struct sde_rect *roi);

/**
 * sde_wb_get_num_of_displays - get total number of writeback devices
 * Returns:	Number of writeback devices
 */
u32 sde_wb_get_num_of_displays(void);

/**
 * wb_display_get_displays - returns pointers for supported display devices
 * @display_array: Pointer to display array to be filled
 * @max_display_count: Size of display_array
 * @Returns: Number of display entries filled
 */
int wb_display_get_displays(void **display_array, u32 max_display_count);

void sde_wb_set_active_state(struct sde_wb_device *wb_dev, bool is_active);
bool sde_wb_is_active(struct sde_wb_device *wb_dev);

/**
 * sde_wb_drm_init - perform DRM initialization
 * @wb_dev:	Pointer to writeback device
 * @encoder:	Pointer to associated encoder
 * Returns:	0 if success; error code otherwise
 */
int sde_wb_drm_init(struct sde_wb_device *wb_dev, struct drm_encoder *encoder);

/**
 * sde_wb_drm_deinit - perform DRM de-initialization
 * @wb_dev:	Pointer to writeback device
 * Returns:	0 if success; error code otherwise
 */
int sde_wb_drm_deinit(struct sde_wb_device *wb_dev);

/**
 * sde_wb_config - setup connection status and available drm modes of the
 *			given writeback connector
 * @drm_dev:	Pointer to DRM device
 * @data:	Pointer to writeback configuration
 * @file_priv:	Pointer file private data
 * Returns:	0 if success; error code otherwise
 *
 * This function will initiate hot-plug detection event.
 */
int sde_wb_config(struct drm_device *drm_dev, void *data,
				struct drm_file *file_priv);

/**
 * sde_wb_connector_post_init - perform writeback specific initialization
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display structure
 * Returns: Zero on success
 */
int sde_wb_connector_post_init(struct drm_connector *connector, void *display);

/**
 * sde_wb_connector_set_info_blob - perform writeback info blob initialization
 * @connector: Pointer to drm connector structure
 * @info: Pointer to connector info
 * @display: Pointer to private display structure
 * @mode_info: Pointer to the mode info structure
 * Returns: Zero on success
 */
int sde_wb_connector_set_info_blob(struct drm_connector *connector,
		void *info,
		void *display,
		struct msm_mode_info *mode_info);

/**
 * sde_wb_connector_detect - perform writeback connection status detection
 * @connector:	Pointer to connector
 * @force:	Indicate force detection
 * @display:	Pointer to writeback device
 * Returns:	connector status
 */
enum drm_connector_status
sde_wb_connector_detect(struct drm_connector *connector,
		bool force,
		void *display);

/**
 * sde_wb_connector_get_modes - get display modes of connector
 * @connector:	Pointer to connector
 * @display:	Pointer to writeback device
 * @avail_res: Pointer with curr available resources
 * Returns:	Number of modes
 *
 * If display modes are not specified in writeback configuration IOCTL, this
 * function will install default EDID modes up to maximum resolution support.
 */
int sde_wb_connector_get_modes(struct drm_connector *connector, void *display,
		const struct msm_resource_caps_info *avail_res);

/**
 * sde_wb_connector_set_property - set atomic connector property
 * @connector: Pointer to drm connector structure
 * @state: Pointer to drm connector state structure
 * @property_index: DRM property index
 * @value: Incoming property value
 * @display: Pointer to private display structure
 * Returns: Zero on success
 */
int sde_wb_connector_set_property(struct drm_connector *connector,
		struct drm_connector_state *state,
		int property_index,
		uint64_t value,
		void *display);

/**
 * sde_wb_get_info - retrieve writeback 'display' information
 * @connector: Pointer to drm connector structure
 * @info: Pointer to display info structure
 * @display: Pointer to private display structure
 * Returns: Zero on success
 */
int sde_wb_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *display);

/**
 * sde_wb_get_mode_info - retrieve information of the mode selected
 * @connector: Pointer to drm connector structure
 * @drm_mode: Display mode set for the display
 * @mode_info: Out parameter. information of the mode.
 * @display: Pointer to private display structure
 * @avail_res: Pointer with curr available resources
 * Returns: zero on success
 */
int sde_wb_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info,
		void *display, const struct msm_resource_caps_info *avail_res);

/**
 * sde_wb_connector_get_wb - retrieve writeback device of the given connector
 * @connector: Pointer to drm connector
 * Returns: Pointer to writeback device on success; NULL otherwise
 */
static inline
struct sde_wb_device *sde_wb_connector_get_wb(struct drm_connector *connector)
{
	if (!connector ||
		(connector->connector_type != DRM_MODE_CONNECTOR_VIRTUAL)) {
		SDE_ERROR("invalid params\n");
		return NULL;
	}

	return sde_connector_get_display(connector);
}

/**
 * sde_wb_connector_state_get_output_fb - get framebuffer of given state
 * @state:	Pointer to connector state
 * Returns:	Pointer to framebuffer
 */
struct drm_framebuffer *
sde_wb_connector_state_get_output_fb(struct drm_connector_state *state);

/**
 * sde_wb_connector_state_get_output_roi - get roi from given atomic state
 * @state:	Pointer to atomic state
 * @roi:	Pointer to region of interest
 * Returns:	0 if success; error code otherwise
 */
int sde_wb_connector_state_get_output_roi(struct drm_connector_state *state,
		struct sde_rect *roi);

#else
static inline
struct drm_framebuffer *sde_wb_get_output_fb(struct sde_wb_device *wb_dev)
{
	return NULL;
}
static inline
int sde_wb_get_output_roi(struct sde_wb_device *wb_dev, struct sde_rect *roi)
{
	return 0;
}
static inline
u32 sde_wb_get_num_of_displays(void)
{
	return 0;
}
static inline
int wb_display_get_displays(void **display_array, u32 max_display_count)
{
	return 0;
}
static inline
void sde_wb_set_active_state(struct sde_wb_device *wb_dev, bool is_active)
{
}
static inline
bool sde_wb_is_active(struct sde_wb_device *wb_dev)
{
	return false;
}
static inline
int sde_wb_drm_init(struct sde_wb_device *wb_dev, struct drm_encoder *encoder)
{
	return 0;
}
static inline
int sde_wb_drm_deinit(struct sde_wb_device *wb_dev)
{
	return 0;
}
static inline
int sde_wb_config(struct drm_device *drm_dev, void *data,
				struct drm_file *file_priv)
{
	return 0;
}
static inline
int sde_wb_connector_post_init(struct drm_connector *connector,
					 void *display)
{
	return 0;
}
static inline
enum drm_connector_status
sde_wb_connector_detect(struct drm_connector *connector,
		bool force,
		void *display)
{
	return connector_status_disconnected;
}
static inline
int sde_wb_connector_get_modes(struct drm_connector *connector, void *display,
		const struct msm_resource_caps_info *avail_res)
{
	return -EINVAL;
}
static inline
int sde_wb_connector_set_property(struct drm_connector *connector,
		struct drm_connector_state *state,
		int property_index,
		uint64_t value,
		void *display)
{
	return 0;
}
static inline
int sde_wb_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *display)
{
	return 0;
}
static inline
struct sde_wb_device *sde_wb_connector_get_wb(struct drm_connector *connector)
{
	return NULL;
}

static inline
struct drm_framebuffer *
sde_wb_connector_state_get_output_fb(struct drm_connector_state *state)
{
	return NULL;
}

static inline
int sde_wb_connector_state_get_output_roi(struct drm_connector_state *state,
		struct sde_rect *roi)
{
	return 0;
}
static inline
int sde_wb_connector_set_info_blob(struct drm_connector *connector,
		void *info,
		void *display,
		struct msm_mode_info *mode_info)
{
	return 0;
}

static inline
int sde_wb_get_mode_info(struct drm_connector *connector,
		const struct drm_display_mode *drm_mode,
		struct msm_mode_info *mode_info,
		void *display, const struct msm_resource_caps_info *avail_res)
{
	return 0;
}
#endif
#endif /* __SDE_WB_H__ */

