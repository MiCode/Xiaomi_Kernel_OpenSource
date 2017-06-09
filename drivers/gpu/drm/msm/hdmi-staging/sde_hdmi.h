/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SDE_HDMI_H_
#define _SDE_HDMI_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/msm_ext_display.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "hdmi.h"

#define MAX_NUMBER_ADB 5
#define MAX_AUDIO_DATA_BLOCK_SIZE 30
#define MAX_SPKR_ALLOC_DATA_BLOCK_SIZE 3

/**
 * struct sde_hdmi_info - defines hdmi display properties
 * @display_type:      Display type as defined by device tree.
 * @is_hot_pluggable:  Can panel be hot plugged.
 * @is_connected:      Is panel connected.
 * @is_edid_supported: Does panel support reading EDID information.
 * @width_mm:          Physical width of panel in millimeters.
 * @height_mm:         Physical height of panel in millimeters.
 */
struct sde_hdmi_info {
	const char *display_type;

	/* HPD */
	bool is_hot_pluggable;
	bool is_connected;
	bool is_edid_supported;

	/* Physical properties */
	u32 width_mm;
	u32 height_mm;
};

/**
 * struct sde_hdmi_ctrl - hdmi ctrl/phy information for the display
 * @ctrl:           Handle to the HDMI controller device.
 * @ctrl_of_node:   pHandle to the HDMI controller device.
 * @hdmi_ctrl_idx:   HDMI controller instance id.
 */
struct sde_hdmi_ctrl {
	/* controller info */
	struct hdmi *ctrl;
	struct device_node *ctrl_of_node;
	u32 hdmi_ctrl_idx;
};

struct hdmi_edid_ctrl {
	struct edid *edid;
	u8 audio_data_block[MAX_NUMBER_ADB * MAX_AUDIO_DATA_BLOCK_SIZE];
	int adb_size;
	u8 spkr_alloc_data_block[MAX_SPKR_ALLOC_DATA_BLOCK_SIZE];
	int sadb_size;
};

/**
 * struct sde_hdmi - hdmi display information
 * @pdev:             Pointer to platform device.
 * @drm_dev:          DRM device associated with the display.
 * @name:             Name of the display.
 * @display_type:     Display type as defined in device tree.
 * @list:             List pointer.
 * @display_lock:     Mutex for sde_hdmi interface.
 * @ctrl:             Controller information for HDMI display.
 * @non_pluggable:    If HDMI display is non pluggable
 * @num_of_modes:     Number of modes supported by display if non pluggable.
 * @mode_list:        Mode list if non pluggable.
 * @connected:        If HDMI display is connected.
 * @is_tpg_enabled:   TPG state.
 * @hpd_work:         HPD work structure.
 * @root:             Debug fs root entry.
 */
struct sde_hdmi {
	struct platform_device *pdev;
	struct drm_device *drm_dev;

	const char *name;
	const char *display_type;
	struct list_head list;
	struct mutex display_lock;

	struct sde_hdmi_ctrl ctrl;

	struct platform_device *ext_pdev;
	struct msm_ext_disp_init_data ext_audio_data;
	struct hdmi_edid_ctrl edid;

	bool non_pluggable;
	u32 num_of_modes;
	struct list_head mode_list;
	bool connected;
	bool is_tpg_enabled;

	struct work_struct hpd_work;

	/* DEBUG FS */
	struct dentry *root;
};

#ifdef CONFIG_DRM_SDE_HDMI
/**
 * sde_hdmi_get_num_of_displays() - returns number of display devices
 *				       supported.
 *
 * Return: number of displays.
 */
u32 sde_hdmi_get_num_of_displays(void);

/**
 * sde_hdmi_get_displays() - returns the display list that's available.
 * @display_array: Pointer to display list
 * @max_display_count: Number of maximum displays in the list
 *
 * Return: number of available displays.
 */
int sde_hdmi_get_displays(void **display_array, u32 max_display_count);

/**
 * sde_hdmi_connector_pre_deinit()- perform additional deinitialization steps
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle
 *
 * Return: error code
 */
int sde_hdmi_connector_pre_deinit(struct drm_connector *connector,
		void *display);

/**
 * sde_hdmi_connector_post_init()- perform additional initialization steps
 * @connector: Pointer to drm connector structure
 * @info: Pointer to sde connector info structure
 * @display: Pointer to private display handle
 *
 * Return: error code
 */
int sde_hdmi_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display);

/**
 * sde_hdmi_connector_detect()- determine if connector is connected
 * @connector: Pointer to drm connector structure
 * @force: Force detect setting from drm framework
 * @display: Pointer to private display handle
 *
 * Return: error code
 */
enum drm_connector_status
sde_hdmi_connector_detect(struct drm_connector *connector,
		bool force,
		void *display);

/**
 * sde_hdmi_connector_get_modes - add drm modes via drm_mode_probed_add()
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display handle

 * Returns: Number of modes added
 */
int sde_hdmi_connector_get_modes(struct drm_connector *connector,
		void *display);

/**
 * sde_hdmi_mode_valid - determine if specified mode is valid
 * @connector: Pointer to drm connector structure
 * @mode: Pointer to drm mode structure
 * @display: Pointer to private display handle
 *
 * Returns: Validity status for specified mode
 */
enum drm_mode_status sde_hdmi_mode_valid(struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display);

/**
 * sde_hdmi_dev_init() - Initializes the display device
 * @display:         Handle to the display.
 *
 * Initialization will acquire references to the resources required for the
 * display hardware to function.
 *
 * Return: error code.
 */
int sde_hdmi_dev_init(struct sde_hdmi *display);

/**
 * sde_hdmi_dev_deinit() - Desinitializes the display device
 * @display:        Handle to the display.
 *
 * All the resources acquired during device init will be released.
 *
 * Return: error code.
 */
int sde_hdmi_dev_deinit(struct sde_hdmi *display);

/**
 * sde_hdmi_drm_init() - initializes DRM objects for the display device.
 * @display:            Handle to the display.
 * @encoder:            Pointer to the encoder object which is connected to the
 *			display.
 *
 * Return: error code.
 */
int sde_hdmi_drm_init(struct sde_hdmi *display,
				struct drm_encoder *enc);

/**
 * sde_hdmi_drm_deinit() - destroys DRM objects assosciated with the display
 * @display:        Handle to the display.
 *
 * Return: error code.
 */
int sde_hdmi_drm_deinit(struct sde_hdmi *display);

/**
 * sde_hdmi_get_info() - returns the display properties
 * @display:          Handle to the display.
 * @info:             Pointer to the structure where info is stored.
 *
 * Return: error code.
 */
int sde_hdmi_get_info(struct msm_display_info *info,
				void *display);

/**
 * sde_hdmi_bridge_init() - init sde hdmi bridge
 * @hdmi:          Handle to the hdmi.
 *
 * Return: struct drm_bridge *.
 */
struct drm_bridge *sde_hdmi_bridge_init(struct hdmi *hdmi);

/**
 * sde_hdmi_set_mode() - Set HDMI mode API.
 * @hdmi:          Handle to the hdmi.
 * @power_on:      Power on/off request.
 *
 * Return: void.
 */
void sde_hdmi_set_mode(struct hdmi *hdmi, bool power_on);

/**
 * sde_hdmi_audio_on() - enable hdmi audio.
 * @hdmi:          Handle to the hdmi.
 * @params:        audio setup parameters from codec.
 *
 * Return: error code.
 */
int sde_hdmi_audio_on(struct hdmi *hdmi,
	struct msm_ext_disp_audio_setup_params *params);

/**
 * sde_hdmi_audio_off() - disable hdmi audio.
 * @hdmi:          Handle to the hdmi.
 *
 * Return: void.
 */
void sde_hdmi_audio_off(struct hdmi *hdmi);

/**
 * sde_hdmi_config_avmute() - mute hdmi.
 * @hdmi:          Handle to the hdmi.
 * @set:           enable/disable avmute.
 *
 * Return: error code.
 */
int sde_hdmi_config_avmute(struct hdmi *hdmi, bool set);

/**
 * sde_hdmi_notify_clients() - notify hdmi clients of the connection status.
 * @connector:     Handle to the drm_connector.
 * @connected:     connection status.
 *
 * Return: void.
 */
void sde_hdmi_notify_clients(struct drm_connector *connector,
	bool connected);

/**
 * sde_hdmi_ack_state() - acknowledge the connection status.
 * @connector:     Handle to the drm_connector.
 * @status:        connection status.
 *
 * Return: void.
 */
void sde_hdmi_ack_state(struct drm_connector *connector,
	enum drm_connector_status status);

/**
 * sde_hdmi_edid_init() - init edid structure.
 * @display:     Handle to the sde_hdmi.
 *
 * Return: error code.
 */
int sde_hdmi_edid_init(struct sde_hdmi *display);

/**
 * sde_hdmi_edid_deinit() - deinit edid structure.
 * @display:     Handle to the sde_hdmi.
 *
 * Return: error code.
 */
int sde_hdmi_edid_deinit(struct sde_hdmi *display);

/**
 * sde_hdmi_get_edid() - get edid info.
 * @connector:   Handle to the drm_connector.
 * @display:     Handle to the sde_hdmi.
 *
 * Return: void.
 */
void sde_hdmi_get_edid(struct drm_connector *connector,
	struct sde_hdmi *display);

/**
 * sde_hdmi_free_edid() - free edid structure.
 * @display:     Handle to the sde_hdmi.
 *
 * Return: error code.
 */
int sde_hdmi_free_edid(struct sde_hdmi *display);

#else /*#ifdef CONFIG_DRM_SDE_HDMI*/

static inline u32 sde_hdmi_get_num_of_displays(void)
{
	return 0;
}

static inline int sde_hdmi_get_displays(void **display_array,
		u32 max_display_count)
{
	return 0;
}

static inline int sde_hdmi_connector_pre_deinit(struct drm_connector *connector,
		void *display)
{
	return 0;
}

static inline int sde_hdmi_connector_post_init(struct drm_connector *connector,
		void *info,
		void *display)
{
	return 0;
}

static inline enum drm_connector_status
sde_hdmi_connector_detect(struct drm_connector *connector,
		bool force,
		void *display)
{
	return connector_status_disconnected;
}

static inline int sde_hdmi_connector_get_modes(struct drm_connector *connector,
		void *display)
{
	return 0;
}

static inline enum drm_mode_status sde_hdmi_mode_valid(
		struct drm_connector *connector,
		struct drm_display_mode *mode,
		void *display)
{
	return MODE_OK;
}

static inline int sde_hdmi_dev_init(struct sde_hdmi *display)
{
	return 0;
}

static inline int sde_hdmi_dev_deinit(struct sde_hdmi *display)
{
	return 0;
}

static inline int sde_hdmi_drm_init(struct sde_hdmi *display,
				struct drm_encoder *enc)
{
	return 0;
}

static inline int sde_hdmi_drm_deinit(struct sde_hdmi *display)
{
	return 0;
}

static inline int sde_hdmi_get_info(struct msm_display_info *info,
				void *display)
{
	return 0;
}
#endif /*#else of CONFIG_DRM_SDE_HDMI*/
#endif /* _SDE_HDMI_H_ */
