/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef _DSI_DISPLAY_H_
#define _DSI_DISPLAY_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/of_device.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>

#include "dsi_defs.h"
#include "dsi_ctrl.h"
#include "dsi_phy.h"
#include "dsi_panel.h"

#define MAX_DSI_CTRLS_PER_DISPLAY             2

/**
 * enum dsi_display_type - enumerates DSI display types
 * @DSI_DISPLAY_SINGLE:       A panel connected on a single DSI interface.
 * @DSI_DISPLAY_EXT_BRIDGE:   A bridge is connected between panel and DSI host.
 *			      It utilizes a single DSI interface.
 * @DSI_DISPLAY_SPLIT:        A panel that utilizes more than one DSI
 *			      interfaces.
 * @DSI_DISPLAY_SPLIT_EXT_BRIDGE: A bridge is present between panel and DSI
 *				  host. It utilizes more than one DSI interface.
 */
enum dsi_display_type {
	DSI_DISPLAY_SINGLE = 0,
	DSI_DISPLAY_EXT_BRIDGE,
	DSI_DISPLAY_SPLIT,
	DSI_DISPLAY_SPLIT_EXT_BRIDGE,
	DSI_DISPLAY_MAX,
};

/**
 * struct dsi_display_info - defines dsi display properties
 * @display_type:      Display type as defined by device tree.
 * @type:              Type of panel connected to DSI interface.
 * @num_of_h_tiles:    In case of split panels, number of h tiles indicates the
 *		       number of dsi interfaces used. For single DSI panels this
 *		       is set to 1. This will be set for horizontally split
 *		       panels.
 * @h_tile_ids:        The DSI instance ID for each tile.
 * @is_hot_pluggable:  Can panel be hot plugged.
 * @is_connected:      Is panel connected.
 * @is_edid_supported: Does panel support reading EDID information.
 * @width_mm:          Physical width of panel in millimeters.
 * @height_mm:         Physical height of panel in millimeters.
 */
struct dsi_display_info {
	char display_type[20];
	enum dsi_display_type type;

	/* Split DSI properties */
	bool h_tiled;
	u32 num_of_h_tiles;
	u32 h_tile_ids[MAX_DSI_CTRLS_PER_DISPLAY];

	/* HPD */
	bool is_hot_pluggable;
	bool is_connected;
	bool is_edid_supported;

	/* Physical properties */
	u32 width_mm;
	u32 height_mm;
};

/**
 * struct dsi_display_ctrl - dsi ctrl/phy information for the display
 * @ctrl:           Handle to the DSI controller device.
 * @ctrl_of_node:   pHandle to the DSI controller device.
 * @dsi_ctrl_idx:   DSI controller instance id.
 * @power_state:    Current power state of the DSI controller.
 * @cmd_engine_enabled:   Command engine status.
 * @video_engine_enabled: Video engine status.
 * @ulps_enabled:         ULPS status for the controller.
 * @clamps_enabled:       Clamps status for the controller.
 * @phy:                  Handle to the DSI PHY device.
 * @phy_of_node:          pHandle to the DSI PHY device.
 * @phy_enabled:          PHY power status.
 */
struct dsi_display_ctrl {
	/* controller info */
	struct dsi_ctrl *ctrl;
	struct device_node *ctrl_of_node;
	u32 dsi_ctrl_idx;

	enum dsi_power_state power_state;

	/* phy info */
	struct msm_dsi_phy *phy;
	struct device_node *phy_of_node;

	bool phy_enabled;
};

/**
 * struct dsi_display_clk_info - dsi display clock source information
 * @src_clks:          Source clocks for DSI display.
 * @mux_clks:          Mux clocks used for DFPS.
 * @shadow_clks:       Used for DFPS.
 */
struct dsi_display_clk_info {
	struct dsi_clk_link_set src_clks;
	struct dsi_clk_link_set mux_clks;
	struct dsi_clk_link_set shadow_clks;
};

/**
 * struct dsi_display - dsi display information
 * @pdev:             Pointer to platform device.
 * @drm_dev:          DRM device associated with the display.
 * @name:             Name of the display.
 * @display_type:     Display type as defined in device tree.
 * @list:             List pointer.
 * @is_active:        Is display active.
 * @display_lock:     Mutex for dsi_display interface.
 * @ctrl_count:       Number of DSI interfaces required by panel.
 * @ctrl:             Controller information for DSI display.
 * @panel:            Handle to DSI panel.
 * @panel_of:         pHandle to DSI panel.
 * @type:             DSI display type.
 * @clk_master_idx:   The master controller for controlling clocks. This is an
 *		      index into the ctrl[MAX_DSI_CTRLS_PER_DISPLAY] array.
 * @cmd_master_idx:   The master controller for sending DSI commands to panel.
 * @video_master_idx: The master controller for enabling video engine.
 * @clock_info:       Clock sourcing for DSI display.
 * @lane_map:         Lane mapping between DSI host and Panel.
 * @num_of_modes:     Number of modes supported by display.
 * @is_tpg_enabled:   TPG state.
 * @host:             DRM MIPI DSI Host.
 * @connector:        Pointer to DRM connector object.
 * @bridge:           Pointer to DRM bridge object.
 */
struct dsi_display {
	struct platform_device *pdev;
	struct drm_device *drm_dev;

	const char *name;
	const char *display_type;
	struct list_head list;
	bool is_active;
	struct mutex display_lock;

	u32 ctrl_count;
	struct dsi_display_ctrl ctrl[MAX_DSI_CTRLS_PER_DISPLAY];

	/* panel info */
	struct dsi_panel *panel;
	struct device_node *panel_of;

	enum dsi_display_type type;
	u32 clk_master_idx;
	u32 cmd_master_idx;
	u32 video_master_idx;

	struct dsi_display_clk_info clock_info;
	struct dsi_host_config config;
	struct dsi_lane_mapping lane_map;
	u32 num_of_modes;
	bool is_tpg_enabled;

	struct mipi_dsi_host host;
	struct dsi_connector *connector;
	struct dsi_bridge    *bridge;
	u32 cmd_engine_refcount;
};

int dsi_display_dev_probe(struct platform_device *pdev);
int dsi_display_dev_remove(struct platform_device *pdev);

/**
 * dsi_display_get_num_of_displays() - returns number of display devices
 *				       supported.
 *
 * Return: number of displays.
 */
u32 dsi_display_get_num_of_displays(void);

/**
 * dsi_display_get_display_by_index()- finds display by index
 * @index:      index of the display.
 *
 * Return: handle to the display or error code.
 */
struct dsi_display *dsi_display_get_display_by_index(u32 index);

/**
 * dsi_display_get_display_by_name()- finds display by name
 * @index:      name of the display.
 *
 * Return: handle to the display or error code.
 */
struct dsi_display *dsi_display_get_display_by_name(const char *name);

/**
 * dsi_display_set_active_state() - sets the state of the display
 * @display:        Handle to display.
 * @is_active:      state
 */
void dsi_display_set_active_state(struct dsi_display *display, bool is_active);

/**
 * dsi_display_is_active() - returns the state of the display
 * @display:        Handle to the display.
 *
 * Return: state.
 */
bool dsi_display_is_active(struct dsi_display *display);

/**
 * dsi_display_dev_init() - Initializes the display device
 * @display:         Handle to the display.
 *
 * Initialization will acquire references to the resources required for the
 * display hardware to function.
 *
 * Return: error code.
 */
int dsi_display_dev_init(struct dsi_display *display);

/**
 * dsi_display_dev_deinit() - Desinitializes the display device
 * @display:        Handle to the display.
 *
 * All the resources acquired during device init will be released.
 *
 * Return: error code.
 */
int dsi_display_dev_deinit(struct dsi_display *display);

/**
 * dsi_display_bind() - Binds the display device to the DRM device
 * @display:       Handle to the display.
 * @dev:           Pointer to the DRM device.
 *
 * Return: error code.
 */
int dsi_display_bind(struct dsi_display *display, struct drm_device *dev);

/**
 * dsi_display_unbind() - Unbinds the display device from the DRM device
 * @display:         Handle to the display.
 *
 * Return: error code.
 */
int dsi_display_unbind(struct dsi_display *display);

/**
 * dsi_display_drm_init() - initializes DRM objects for the display device.
 * @display:            Handle to the display.
 * @encoder:            Pointer to the encoder object which is connected to the
 *			display.
 *
 * Return: error code.
 */
int dsi_display_drm_init(struct dsi_display *display, struct drm_encoder *enc);

/**
 * dsi_display_drm_deinit() - destroys DRM objects assosciated with the display
 * @display:        Handle to the display.
 *
 * Return: error code.
 */
int dsi_display_drm_deinit(struct dsi_display *display);

/**
 * dsi_display_get_info() - returns the display properties
 * @display:          Handle to the display.
 * @info:             Pointer to the structure where info is stored.
 *
 * Return: error code.
 */
int dsi_display_get_info(struct dsi_display *display,
			 struct dsi_display_info *info);

/**
 * dsi_display_get_modes() - get modes supported by display
 * @display:            Handle to display.
 * @modes;              Pointer to array of modes. Memory allocated should be
 *			big enough to store (count * struct dsi_display_mode)
 *			elements. If modes pointer is NULL, number of modes will
 *			be stored in the memory pointed to by count.
 * @count:              If modes is NULL, number of modes will be stored. If
 *			not, mode information will be copied (number of modes
 *			copied will be equal to *count).
 *
 * Return: error code.
 */
int dsi_display_get_modes(struct dsi_display *display,
			  struct dsi_display_mode *modes,
			  u32 *count);

/**
 * dsi_display_validate_mode() - validates if mode is supported by display
 * @display:             Handle to display.
 * @mode:                Mode to be validated.
 *
 * Return: 0 if supported or error code.
 */
int dsi_display_validate_mode(struct dsi_display *display,
			      struct dsi_display_mode *mode);

/**
 * dsi_display_set_mode() - Set mode on the display.
 * @display:           Handle to display.
 * @mode:              mode to be set.
 * @flags:             Modifier flags.
 *
 * Return: error code.
 */
int dsi_display_set_mode(struct dsi_display *display,
			 struct dsi_display_mode *mode,
			 u32 flags);

/**
 * dsi_display_prepare() - prepare display
 * @display:          Handle to display.
 *
 * Prepare will perform power up sequences for the host and panel hardware.
 * Power and clock resources might be turned on (depending on the panel mode).
 * The video engine is not enabled.
 *
 * Return: error code.
 */
int dsi_display_prepare(struct dsi_display *display);

/**
 * dsi_display_enable() - enable display
 * @display:            Handle to display.
 *
 * Enable will turn on the host engine and the panel. At the end of the enable
 * function, Host and panel hardware are ready to accept pixel data from
 * upstream.
 *
 * Return: error code.
 */
int dsi_display_enable(struct dsi_display *display);

/**
 * dsi_display_post_enable() - perform post enable operations.
 * @display:         Handle to display.
 *
 * Some panels might require some commands to be sent after pixel data
 * transmission has started. Such commands are sent as part of the post_enable
 * function.
 *
 * Return: error code.
 */
int dsi_display_post_enable(struct dsi_display *display);

/**
 * dsi_display_pre_disable() - perform pre disable operations.
 * @display:          Handle to display.
 *
 * If a panel requires commands to be sent before pixel data transmission is
 * stopped, those can be sent as part of pre_disable.
 *
 * Return: error code.
 */
int dsi_display_pre_disable(struct dsi_display *display);

/**
 * dsi_display_disable() - disable panel and host hardware.
 * @display:             Handle to display.
 *
 * Disable host and panel hardware and pixel data transmission can not continue.
 *
 * Return: error code.
 */
int dsi_display_disable(struct dsi_display *display);

/**
 * dsi_display_unprepare() - power off display hardware.
 * @display:            Handle to display.
 *
 * Host and panel hardware is turned off. Panel will be in reset state at the
 * end of the function.
 *
 * Return: error code.
 */
int dsi_display_unprepare(struct dsi_display *display);

int dsi_display_set_tpg_state(struct dsi_display *display, bool enable);

int dsi_display_clock_gate(struct dsi_display *display, bool enable);
int dsi_dispaly_static_frame(struct dsi_display *display, bool enable);

#endif /* _DSI_DISPLAY_H_ */
