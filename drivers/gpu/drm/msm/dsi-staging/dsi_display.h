/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#define DSI_CLIENT_NAME_SIZE		20
#define MAX_CMDLINE_PARAM_LEN	 512
/*
 * DSI Validate Mode modifiers
 * @DSI_VALIDATE_FLAG_ALLOW_ADJUST:	Allow mode validation to also do fixup
 */
#define DSI_VALIDATE_FLAG_ALLOW_ADJUST	0x1

/**
 * enum dsi_display_selection_type - enumerates DSI display selection types
 * @DSI_PRIMARY:    primary DSI display selected from module parameter
 * @DSI_SECONDARY:  Secondary DSI display selected from module parameter
 * @MAX_DSI_ACTIVE_DISPLAY: Maximum acive displays that can be selected
 */
enum dsi_display_selection_type {
	DSI_PRIMARY = 0,
	DSI_SECONDARY,
	MAX_DSI_ACTIVE_DISPLAY,
};

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
 * struct dsi_display_boot_param - defines DSI boot display selection
 * @name:Name of DSI display selected as a boot param.
 * @boot_disp_en:bool to indicate dtsi availability of display node
 * @is_primary:bool to indicate whether current display is primary display
 * @length:length of DSI display.
 * @cmdline_topology: Display topology shared from kernel command line.
 */
struct dsi_display_boot_param {
	char name[MAX_CMDLINE_PARAM_LEN];
	bool boot_disp_en;
	bool is_primary;
	int length;
	struct device_node *node;
	int cmdline_topology;
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
 * @config:           DSI host configuration information.
 * @lane_map:         Lane mapping between DSI host and Panel.
 * @cmdline_topology: Display topology shared from kernel command line.
 * @cmdline_timing:   Display timing shared from kernel command line.
 * @is_tpg_enabled:   TPG state.
 * @ulps_enabled:     ulps state.
 * @clamp_enabled:    clamp state.
 * @phy_idle_power_off:   PHY power state.
 * @host:             DRM MIPI DSI Host.
 * @bridge:           Pointer to DRM bridge object.
 * @cmd_engine_refcount:  Reference count enforcing single instance of cmd eng
 * @clk_mngr:         DSI clock manager.
 * @dsi_clk_handle:   DSI clock handle.
 * @mdp_clk_handle:   MDP clock handle.
 * @root:             Debugfs root directory
 * @misr_enable       Frame MISR enable/disable
 * @misr_frame_count  Number of frames to accumulate the MISR value
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
	struct dsi_lane_map lane_map;
	int cmdline_topology;
	int cmdline_timing;
	bool is_tpg_enabled;
	bool ulps_enabled;
	bool clamp_enabled;
	bool phy_idle_power_off;
	struct drm_gem_object *tx_cmd_buf;
	u32 cmd_buffer_size;
	u32 cmd_buffer_iova;
	void *vaddr;

	struct mipi_dsi_host host;
	struct dsi_bridge    *bridge;
	u32 cmd_engine_refcount;

	void *clk_mngr;
	void *dsi_clk_handle;
	void *mdp_clk_handle;

	/* DEBUG FS */
	struct dentry *root;

	bool misr_enable;
	u32 misr_frame_count;
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
 * dsi_display_get_boot_display()- get DSI boot display name
 * @index:	index of display selection
 *
 * Return:	returns the display node pointer
 */
struct device_node *dsi_display_get_boot_display(int index);

/**
 * dsi_display_get_display_by_name()- finds display by name
 * @name:	name of the display.
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
 * dsi_display_get_mode_count() - get number of modes supported by the display
 * @display:            Handle to display.
 * @count:              Number of modes supported
 *
 * Return: error code.
 */
int dsi_display_get_mode_count(struct dsi_display *display, u32 *count);

/**
 * dsi_display_get_modes() - get modes supported by display
 * @display:            Handle to display.
 * @modes;              Pointer to array of modes. Memory allocated should be
 *			big enough to store (count * struct dsi_display_mode)
 *			elements. If modes pointer is NULL, number of modes will
 *			be stored in the memory pointed to by count.
 *
 * Return: error code.
 */
int dsi_display_get_modes(struct dsi_display *display,
			  struct dsi_display_mode *modes);

/**
 * dsi_display_put_mode() - free up mode created for the display
 * @display:            Handle to display.
 * @mode:               Display mode to be freed up
 *
 * Return: error code.
 */
void dsi_display_put_mode(struct dsi_display *display,
	struct dsi_display_mode *mode);

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
 * dsi_display_validate_mode_vrr() - validates mode if variable refresh case
 * @display:             Handle to display.
 * @mode:                Mode to be validated..
 *
 * Return: 0 if  error code.
 */
int dsi_display_validate_mode_vrr(struct dsi_display *display,
			struct dsi_display_mode *cur_dsi_mode,
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
 * dsi_pre_clkoff_cb() - Callback before clock is turned off
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @new_state: next state for the clock.
 *
 * @return: error code.
 */
int dsi_pre_clkoff_cb(void *priv, enum dsi_clk_type clk_type,
	enum dsi_clk_state new_state);

/**
 * dsi_display_update_pps() - update PPS buffer.
 * @pps_cmd:             PPS buffer.
 * @display:             Handle to display.
 *
 * Copies new PPS buffer into display structure.
 *
 * Return: error code.
 */
int dsi_display_update_pps(char *pps_cmd, void *display);

/**
 * dsi_post_clkoff_cb() - Callback after clock is turned off
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
int dsi_post_clkoff_cb(void *priv, enum dsi_clk_type clk_type,
	enum dsi_clk_state curr_state);

/**
 * dsi_post_clkon_cb() - Callback after clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
int dsi_post_clkon_cb(void *priv, enum dsi_clk_type clk_type,
	enum dsi_clk_state curr_state);


/**
 * dsi_pre_clkon_cb() - Callback before clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @new_state: next state for the clock.
 *
 * @return: error code.
 */
int dsi_pre_clkon_cb(void *priv, enum dsi_clk_type clk_type,
	enum dsi_clk_state new_state);

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

/**
 * dsi_display_enable_event() - enable interrupt based connector event
 * @display:            Handle to display.
 * @event_idx:          Event index.
 * @event_info:         Event callback definition.
 * @enable:             Whether to enable/disable the event interrupt.
 */
void dsi_display_enable_event(struct dsi_display *display,
		uint32_t event_idx, struct dsi_event_cb_info *event_info,
		bool enable);

int dsi_display_set_backlight(void *display, u32 bl_lvl);

/**
 * dsi_display_check_status() - check if panel is dead or alive
 * @display:            Handle to display.
 */
int dsi_display_check_status(void *display);

/**
 * dsi_display_soft_reset() - perform a soft reset on DSI controller
 * @display:         Handle to display
 *
 * The video, command and controller engines will be disabled before the
 * reset is triggered. After, the engines will be re-enabled to the same state
 * as before the reset.
 *
 * If the reset is done while MDP timing engine is turned on, the video
 * engine should be re-enabled only during the vertical blanking time.
 *
 * Return: error code
 */
int dsi_display_soft_reset(void *display);

/**
 * dsi_display_set_power - update power/dpms setting
 * @connector: Pointer to drm connector structure
 * @power_mode: One of the following,
 *              SDE_MODE_DPMS_ON
 *              SDE_MODE_DPMS_LP1
 *              SDE_MODE_DPMS_LP2
 *              SDE_MODE_DPMS_STANDBY
 *              SDE_MODE_DPMS_SUSPEND
 *              SDE_MODE_DPMS_OFF
 * @display: Pointer to private display structure
 * Returns: Zero on success
 */
int dsi_display_set_power(struct drm_connector *connector,
		int power_mode, void *display);

/*
 * dsi_display_pre_kickoff - program kickoff-time features
 * @display: Pointer to private display structure
 * @params: Parameters for kickoff-time programming
 * Returns: Zero on success
 */
int dsi_display_pre_kickoff(struct dsi_display *display,
		struct msm_display_kickoff_params *params);
/**
 * dsi_display_get_dst_format() - get dst_format from DSI display
 * @display:         Handle to display
 *
 * Return: enum dsi_pixel_format type
 */
enum dsi_pixel_format dsi_display_get_dst_format(void *display);

#endif /* _DSI_DISPLAY_H_ */
