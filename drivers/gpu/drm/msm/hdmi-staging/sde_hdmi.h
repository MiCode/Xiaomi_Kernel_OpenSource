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

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include "hdmi.h"

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

/**
 * struct hdmi_panel_dba - HDMI DBA panel information
 * @dba_panel:          Indicate if it's DBA panel
 * @bridge_name:        Bridge chip name
 * @bridge_instance:    Bridge chip instance.
 */
struct hdmi_panel_dba {
	bool dba_panel;
	const char *bridge_name;
	int bridge_instance;
};

/**
 * struct sde_hdmi_panel - hdmi panel information
 * @head:             Head of the list.
 * @mode:             DRM mode info.
 * @name:             Name of the panel.
 * @dba_config:       DBA related information.
 */
struct sde_hdmi_panel {
	struct list_head head;
	struct drm_display_mode mode;
	const char *name;
	struct hdmi_panel_dba dba_config;
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
 * @num_of_panels:    Number of panels defined in dtsi.
 * @panel_list:       Panel list, refer to struct sde_hdmi_panel.
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

	bool non_pluggable;
	u32 num_of_panels;
	struct list_head panel_list;
	bool is_tpg_enabled;

	struct work_struct hpd_work;

	/* DEBUG FS */
	struct dentry *root;
};

/**
 * hdmi_tx_scdc_access_type() - hdmi 2.0 DDC functionalities.
 */
enum hdmi_tx_scdc_access_type {
	HDMI_TX_SCDC_SCRAMBLING_STATUS,
	HDMI_TX_SCDC_SCRAMBLING_ENABLE,
	HDMI_TX_SCDC_TMDS_BIT_CLOCK_RATIO_UPDATE,
	HDMI_TX_SCDC_CLOCK_DET_STATUS,
	HDMI_TX_SCDC_CH0_LOCK_STATUS,
	HDMI_TX_SCDC_CH1_LOCK_STATUS,
	HDMI_TX_SCDC_CH2_LOCK_STATUS,
	HDMI_TX_SCDC_CH0_ERROR_COUNT,
	HDMI_TX_SCDC_CH1_ERROR_COUNT,
	HDMI_TX_SCDC_CH2_ERROR_COUNT,
	HDMI_TX_SCDC_READ_ENABLE,
	HDMI_TX_SCDC_MAX,
};

/**
 * hdmi_tx_ddc_timer_type() - hdmi DDC timer functionalities.
 */
enum hdmi_tx_ddc_timer_type {
	HDMI_TX_DDC_TIMER_HDCP2P2_RD_MSG,
	HDMI_TX_DDC_TIMER_SCRAMBLER_STATUS,
	HDMI_TX_DDC_TIMER_UPDATE_FLAGS,
	HDMI_TX_DDC_TIMER_STATUS_FLAGS,
	HDMI_TX_DDC_TIMER_CED,
	HDMI_TX_DDC_TIMER_MAX,
};

#ifdef CONFIG_DRM_SDE_HDMI
/**
 * sde_hdmi_register() - register hdmi display platform driver
 */
void sde_hdmi_register(void);

/**
 * sde_hdmi_unregister() - unregister hdmi display platform driver
 */
void sde_hdmi_unregister(void);

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
 * sde_hdmi_bind() - Binds the display device to the DRM device
 * @display:       Handle to the display.
 * @dev:           Pointer to the DRM device.
 *
 * Return: error code.
 */
int sde_hdmi_bind(struct sde_hdmi *display, struct drm_device *dev);

/**
 * sde_hdmi_unbind() - Unbinds the display device from the DRM device
 * @display:         Handle to the display.
 *
 * Return: error code.
 */
int sde_hdmi_unbind(struct sde_hdmi *display);

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
 * sde_hdmi_ddc_read() - common hdmi ddc read API.
 * @hdmi:          Handle to the hdmi.
 * @addr:          Command address.
 * @offset:        Command offset.
 * @data:          Data buffer for read back.
 * @data_len:      Data buffer length.
 *
 * Return: error code.
 */
int sde_hdmi_ddc_read(struct hdmi *hdmi, u16 addr, u8 offset,
	u8 *data, u16 data_len);

/**
 * sde_hdmi_ddc_write() - common hdmi ddc write API.
 * @hdmi:          Handle to the hdmi.
 * @addr:          Command address.
 * @offset:        Command offset.
 * @data:          Data buffer for write.
 * @data_len:      Data buffer length.
 *
 * Return: error code.
 */
int sde_hdmi_ddc_write(struct hdmi *hdmi, u16 addr, u8 offset,
	u8 *data, u16 data_len);

/**
 * sde_hdmi_scdc_read() - hdmi 2.0 ddc read API.
 * @hdmi:          Handle to the hdmi.
 * @data_type:     DDC data type, refer to enum hdmi_tx_scdc_access_type.
 * @val:           Read back value.
 *
 * Return: error code.
 */
int sde_hdmi_scdc_read(struct hdmi *hdmi, u32 data_type, u32 *val);

/**
 * sde_hdmi_scdc_write() - hdmi 2.0 ddc write API.
 * @hdmi:          Handle to the hdmi.
 * @data_type:     DDC data type, refer to enum hdmi_tx_scdc_access_type.
 * @val:           Value write through DDC.
 *
 * Return: error code.
 */
int sde_hdmi_scdc_write(struct hdmi *hdmi, u32 data_type, u32 val);

#else /*#ifdef CONFIG_DRM_SDE_HDMI*/
static inline void sde_hdmi_register(void)
{
}

static inline void sde_hdmi_unregister(void)
{
}

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

static inline int sde_hdmi_bind(struct sde_hdmi *display,
	struct drm_device *dev)
{
	return 0;
}

static inline int sde_hdmi_unbind(struct sde_hdmi *display)
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
