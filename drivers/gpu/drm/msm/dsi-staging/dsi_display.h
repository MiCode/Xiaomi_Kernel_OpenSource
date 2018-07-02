/*
 * Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>

#include "msm_drv.h"
#include "dsi_defs.h"
#include "dsi_ctrl.h"
#include "dsi_phy.h"
#include "dsi_panel.h"

#define MAX_DSI_CTRLS_PER_DISPLAY             2

/*
 * DSI Validate Mode modifiers
 * @DSI_VALIDATE_FLAG_ALLOW_ADJUST:	Allow mode validation to also do fixup
 */
#define DSI_VALIDATE_FLAG_ALLOW_ADJUST	0x1

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
 * struct dsi_display_ctrl - dsi ctrl/phy information for the display
 * @ctrl:           Handle to the DSI controller device.
 * @ctrl_of_node:   pHandle to the DSI controller device.
 * @dsi_ctrl_idx:   DSI controller instance id.
 * @power_state:    Current power state of the DSI controller.
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
 * @panel_count:      Number of DSI panel.
 * @panel:            Handle to DSI panel.
 * @panel_of:         pHandle to DSI panel, it's an array with panel_count
 *		      of struct device_node pointers.
 * @bridge_idx:       Bridge chip index for each panel_of.
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
 * @cmd_engine_refcount:  Reference count enforcing single instance of cmd eng
 * @root:                 Debugfs root directory
 * @cont_splash_enabled:  Early splash status.
 * @dsi_split_swap:       Swap dsi output in split mode.
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
	u32 panel_count;
	struct dsi_panel **panel;
	struct device_node **panel_of;
	u32 *bridge_idx;

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
	struct dsi_bridge    *bridge;
	u32 cmd_engine_refcount;

	/* DEBUG FS */
	struct dentry *root;

	bool cont_splash_enabled;
	bool dsi_split_swap;
};

int dsi_display_dev_probe(struct platform_device *pdev);
int dsi_display_dev_remove(struct platform_device *pdev);

/**
 * dsi_display_get_num_of_displays() - returns number of display devices
 *				       supported.
 *
 * Return: number of displays.
 */
int dsi_display_get_num_of_displays(void);

/**
 * dsi_display_get_active_displays - returns pointers for active display devices
 * @display_array: Pointer to display array to be filled
 * @max_display_count: Size of display_array
 * @Returns: Number of display entries filled
 */
int dsi_display_get_active_displays(void **display_array,
		u32 max_display_count);

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
 * dsi_display_drm_bridge_init() - initializes DRM bridge object for DSI
 * @display:            Handle to the display.
 * @encoder:            Pointer to the encoder object which is connected to the
 *			display.
 *
 * Return: error code.
 */
int dsi_display_drm_bridge_init(struct dsi_display *display,
		struct drm_encoder *enc);

/**
 * dsi_display_drm_bridge_deinit() - destroys DRM bridge for the display
 * @display:        Handle to the display.
 *
 * Return: error code.
 */
int dsi_display_drm_bridge_deinit(struct dsi_display *display);

/**
 * dsi_display_get_info() - returns the display properties
 * @info:             Pointer to the structure where info is stored.
 * @disp:             Handle to the display.
 *
 * Return: error code.
 */
int dsi_display_get_info(struct msm_display_info *info, void *disp);

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
 * @flags:               Modifier flags.
 *
 * Return: 0 if supported or error code.
 */
int dsi_display_validate_mode(struct dsi_display *display,
			      struct dsi_display_mode *mode,
			      u32 flags);

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

int dsi_display_set_backlight(void *display, u32 bl_lvl);

/**
 * dsi_dsiplay_setup_splash_resource
 * @display:            Handle to display.
 *
 * Setup DSI splash resource to avoid reset and glitch if DSI is enabled
 * in bootloder.
 *
 * Return: error code.
 */
int dsi_dsiplay_setup_splash_resource(struct dsi_display *display);
#endif /* _DSI_DISPLAY_H_ */
