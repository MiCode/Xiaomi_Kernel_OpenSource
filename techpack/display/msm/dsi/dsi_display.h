/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_DISPLAY_H_
#define _DSI_DISPLAY_H_

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/firmware.h>
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
#define MAX_CMD_PAYLOAD_SIZE	256
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
	char *boot_param;
	bool boot_disp_en;
	int length;
	struct device_node *node;
	int cmdline_topology;
	void *disp;
};

/**
 * struct dsi_display_clk_info - dsi display clock source information
 * @src_clks:          Source clocks for DSI display.
 * @mux_clks:          Mux clocks used for DFPS.
 * @shadow_clks:       Used for D-phy clock switch.
 * @shadow_cphy_clks:  Used for C-phy clock switch.
 */
struct dsi_display_clk_info {
	struct dsi_clk_link_set src_clks;
	struct dsi_clk_link_set mux_clks;
	struct dsi_clk_link_set cphy_clks;
	struct dsi_clk_link_set shadow_clks;
	struct dsi_clk_link_set shadow_cphy_clks;
};

/**
 * struct dsi_display_ext_bridge - dsi display external bridge information
 * @display:           Pointer of DSI display.
 * @node_of:           Bridge node created from bridge driver.
 * @bridge:            Bridge created from bridge driver
 * @orig_funcs:        Bridge function from bridge driver (split mode only)
 * @bridge_funcs:      Overridden function from bridge driver (split mode only)
 */
struct dsi_display_ext_bridge {
	void *display;
	struct device_node *node_of;
	struct drm_bridge *bridge;
	const struct drm_bridge_funcs *orig_funcs;
	struct drm_bridge_funcs bridge_funcs;
};

/**
 * struct dsi_display - dsi display information
 * @pdev:             Pointer to platform device.
 * @drm_dev:          DRM device associated with the display.
 * @drm_conn:         Pointer to DRM connector associated with the display
 * @ext_conn:         Pointer to external connector attached to DSI connector
 * @name:             Name of the display.
 * @display_type:     Display type as defined in device tree.
 * @list:             List pointer.
 * @is_active:        Is display active.
 * @is_cont_splash_enabled:  Is continuous splash enabled
 * @sw_te_using_wd:   Is software te enabled
 * @display_lock:     Mutex for dsi_display interface.
 * @disp_te_gpio:     GPIO for panel TE interrupt.
 * @is_te_irq_enabled:bool to specify whether TE interrupt is enabled.
 * @esd_te_gate:      completion gate to signal TE interrupt.
 * @ctrl_count:       Number of DSI interfaces required by panel.
 * @ctrl:             Controller information for DSI display.
 * @panel:            Handle to DSI panel.
 * @panel_node:       pHandle to DSI panel actually in use.
 * @ext_bridge:       External bridge information for DSI display.
 * @ext_bridge_cnt:   Number of external bridges
 * @modes:            Array of probed DSI modes
 * @type:             DSI display type.
 * @clk_master_idx:   The master controller for controlling clocks. This is an
 *		      index into the ctrl[MAX_DSI_CTRLS_PER_DISPLAY] array.
 * @cmd_master_idx:   The master controller for sending DSI commands to panel.
 * @video_master_idx: The master controller for enabling video engine.
 * @cached_clk_rate:  The cached DSI clock rate set dynamically by sysfs.
 * @clkrate_change_pending: Flag indicating the pending DSI clock re-enabling.
 * @clock_info:       Clock sourcing for DSI display.
 * @config:           DSI host configuration information.
 * @lane_map:         Lane mapping between DSI host and Panel.
 * @cmdline_topology: Display topology shared from kernel command line.
 * @cmdline_timing:   Display timing shared from kernel command line.
 * @is_tpg_enabled:   TPG state.
 * @poms_pending;      Flag indicating the pending panel operating mode switch.
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
 * @esd_trigger       field indicating ESD trigger through debugfs
 * @te_source         vsync source pin information
 * @clk_gating_config Clocks for which clock gating needs to be enabled
 * @queue_cmd_waits   Indicates if wait for dma commands done has to be queued.
 * @dma_cmd_workq:	Pointer to the workqueue of DMA command transfer done
 *				wait sequence.
 */
struct dsi_display {
	struct platform_device *pdev;
	struct drm_device *drm_dev;
	struct drm_connector *drm_conn;
	struct drm_connector *ext_conn;

	const char *name;
	const char *display_type;
	struct list_head list;
	bool is_cont_splash_enabled;
	bool is_prim_display;
	bool sw_te_using_wd;
	struct mutex display_lock;
	int disp_te_gpio;
	bool is_te_irq_enabled;
	struct completion esd_te_gate;

	u32 ctrl_count;
	struct dsi_display_ctrl ctrl[MAX_DSI_CTRLS_PER_DISPLAY];

	/* panel info */
	struct dsi_panel *panel;
	struct device_node *panel_node;
	struct device_node *parser_node;

	/* external bridge */
	struct dsi_display_ext_bridge ext_bridge[MAX_DSI_CTRLS_PER_DISPLAY];
	u32 ext_bridge_cnt;

	struct dsi_display_mode *modes;

	enum dsi_display_type type;
	u32 clk_master_idx;
	u32 cmd_master_idx;
	u32 video_master_idx;

	/* dynamic DSI clock info*/
	u32  cached_clk_rate;
	atomic_t clkrate_change_pending;

	struct dsi_display_clk_info clock_info;
	struct dsi_host_config config;
	struct dsi_lane_map lane_map;
	int cmdline_topology;
	int cmdline_timing;
	bool is_tpg_enabled;
	bool poms_pending;
	bool ulps_enabled;
	bool clamp_enabled;
	bool phy_idle_power_off;
	struct drm_gem_object *tx_cmd_buf;
	u32 cmd_buffer_size;
	u64 cmd_buffer_iova;
	void *vaddr;
	struct msm_gem_address_space *aspace;

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
	u32 esd_trigger;
	/* multiple dsi error handlers */
	struct workqueue_struct *err_workq;
	struct work_struct fifo_underflow_work;
	struct work_struct fifo_overflow_work;
	struct work_struct lp_rx_timeout_work;

	/* firmware panel data */
	const struct firmware *fw;
	void *parser;

	struct dsi_display_boot_param *boot_disp;

	u32 te_source;
	u32 clk_gating_config;
	bool queue_cmd_waits;
	struct workqueue_struct *dma_cmd_workq;
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
 * dsi_display_drm_ext_bridge_init() - initializes DRM bridge for ext bridge
 * @display:            Handle to the display.
 * @enc:                Pointer to the encoder object which is connected to the
 *                      display.
 * @connector:          Pointer to the connector object which is connected to
 *                      the display.
 *
 * Return: error code.
 */
int dsi_display_drm_ext_bridge_init(struct dsi_display *display,
		struct drm_encoder *enc, struct drm_connector *connector);

/**
 * dsi_display_get_info() - returns the display properties
 * @connector:        Pointer to drm connector structure
 * @info:             Pointer to the structure where info is stored.
 * @disp:             Handle to the display.
 *
 * Return: error code.
 */
int dsi_display_get_info(struct drm_connector *connector,
		struct msm_display_info *info, void *disp);

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
 * @modes;              Output param, list of DSI modes. Number of modes matches
 *                      count got from display->panel->num_display_modes;
 *
 * Return: error code.
 */
int dsi_display_get_modes(struct dsi_display *display,
			  struct dsi_display_mode **modes);

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
 * dsi_display_get_default_lms() - retrieve max number of lms used
 *             for dsi display by traversing through all topologies
 * @display:            Handle to display.
 * @num_lm:             Number of LMs used
 *
 * Return: error code.
 */
int dsi_display_get_default_lms(void *dsi_display, u32 *num_lm);

/**
 * dsi_display_find_mode() - retrieve cached DSI mode given relevant params
 * @display:            Handle to display.
 * @cmp:                Mode to use as comparison to find original
 * @out_mode:           Output parameter, pointer to retrieved mode
 *
 * Return: error code.
 */
int dsi_display_find_mode(struct dsi_display *display,
		const struct dsi_display_mode *cmp,
		struct dsi_display_mode **out_mode);
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
 * dsi_display_validate_mode_change() - validates mode if variable refresh case
 *				or dynamic clk change case
 * @display:             Handle to display.
 * @mode:                Mode to be validated..
 *
 * Return: 0 if  error code.
 */
int dsi_display_validate_mode_change(struct dsi_display *display,
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
 * dsi_display_splash_res_cleanup() - cleanup for continuous splash
 * @display:    Pointer to dsi display
 * Returns:     Zero on success
 */
int dsi_display_splash_res_cleanup(struct  dsi_display *display);

/**
 * dsi_display_config_ctrl_for_cont_splash()- Enable engine modes for DSI
 *                                     controller during continuous splash
 * @display: Handle to DSI display
 *
 * Return:        returns error code
 */
int dsi_display_config_ctrl_for_cont_splash(struct dsi_display *display);

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
 * @l_type: specifies if the clock is HS or LP type. Valid only for link clocks.
 * @new_state: next state for the clock.
 *
 * @return: error code.
 */
int dsi_pre_clkoff_cb(void *priv, enum dsi_clk_type clk_type,
		enum dsi_lclk_type l_type,
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
 * @l_type: specifies if the clock is HS or LP type. Valid only for link clocks.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
int dsi_post_clkoff_cb(void *priv, enum dsi_clk_type clk_type,
		enum dsi_lclk_type l_type,
		enum dsi_clk_state curr_state);

/**
 * dsi_post_clkon_cb() - Callback after clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @l_type: specifies if the clock is HS or LP type. Valid only for link clocks.
 * @curr_state: current state for the clock.
 *
 * @return: error code.
 */
int dsi_post_clkon_cb(void *priv, enum dsi_clk_type clk_type,
		enum dsi_lclk_type l_type,
		enum dsi_clk_state curr_state);

/**
 * dsi_pre_clkon_cb() - Callback before clock is turned on
 * @priv: private data pointer.
 * @clk_type: clock which is being turned on.
 * @l_type: specifies if the clock is HS or LP type. Valid only for link clocks.
 * @new_state: next state for the clock.
 *
 * @return: error code.
 */
int dsi_pre_clkon_cb(void *priv, enum dsi_clk_type clk_type,
		enum dsi_lclk_type l_type,
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
 * dsi_display_get_drm_panel() - get drm_panel from display.
 * @display:            Handle to display.
 * Get drm_panel which was inclued in dsi_display's dsi_panel.
 *
 * Return: drm_panel/NULL.
 */
struct drm_panel *dsi_display_get_drm_panel(struct dsi_display *display);

/**
 * dsi_display_enable_event() - enable interrupt based connector event
 * @connector:          Pointer to drm connector structure
 * @display:            Handle to display.
 * @event_idx:          Event index.
 * @event_info:         Event callback definition.
 * @enable:             Whether to enable/disable the event interrupt.
 */
void dsi_display_enable_event(struct drm_connector *connector,
		struct dsi_display *display,
		uint32_t event_idx, struct dsi_event_cb_info *event_info,
		bool enable);

/**
 * dsi_display_set_backlight() - set backlight
 * @connector:          Pointer to drm connector structure
 * @display:            Handle to display.
 * @bl_lvl:             Backlight level.
 * @event_info:         Event callback definition.
 * @enable:             Whether to enable/disable the event interrupt.
 */
int dsi_display_set_backlight(struct drm_connector *connector,
		void *display, u32 bl_lvl);

/**
 * dsi_display_check_status() - check if panel is dead or alive
 * @connector:          Pointer to drm connector structure
 * @display:            Handle to display.
 * @te_check_override:	Whether check for TE from panel or default check
 */
int dsi_display_check_status(struct drm_connector *connector, void *display,
				bool te_check_override);

/**
 * dsi_display_cmd_transfer() - transfer command to the panel
 * @connector:          Pointer to drm connector structure
 * @display:            Handle to display.
 * @cmd_buf:            Command buffer
 * @cmd_buf_len:        Command buffer length in bytes
 */
int dsi_display_cmd_transfer(struct drm_connector *connector,
		void *display, const char *cmd_buffer,
		u32 cmd_buf_len);

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
 * @connector: Pointer to drm connector structure
 * @display: Pointer to private display structure
 * @params: Parameters for kickoff-time programming
 * Returns: Zero on success
 */
int dsi_display_pre_kickoff(struct drm_connector *connector,
		struct dsi_display *display,
		struct msm_display_kickoff_params *params);

/*
 * dsi_display_pre_commit - program pre commit features
 * @display: Pointer to private display structure
 * @params: Parameters for pre commit time programming
 * Returns: Zero on success
 */
int dsi_display_pre_commit(void *display,
		struct msm_display_conn_params *params);

ssize_t wp_info_show(struct device *device,
	    struct device_attribute *attr,
		char *buf);

/**
 * dsi_display_get_dst_format() - get dst_format from DSI display
 * @connector:        Pointer to drm connector structure
 * @display:         Handle to display
 *
 * Return: enum dsi_pixel_format type
 */
enum dsi_pixel_format dsi_display_get_dst_format(
		struct drm_connector *connector,
		void *display);

/**
 * dsi_display_cont_splash_config() - initialize splash resources
 * @display:         Handle to display
 *
 * Return: Zero on Success
 */
int dsi_display_cont_splash_config(void *display);
/*
 * dsi_display_get_panel_vfp - get panel vsync
 * @display: Pointer to private display structure
 * @h_active: width
 * @v_active: height
 * Returns: v_front_porch on success error code on failure
 */
int dsi_display_get_panel_vfp(void *display,
	int h_active, int v_active);

int dsi_display_cmd_engine_enable(struct dsi_display *display);
int dsi_display_cmd_engine_disable(struct dsi_display *display);
int dsi_host_alloc_cmd_tx_buffer(struct dsi_display *display);

int dsi_display_hbm_set_disp_param(struct drm_connector *connector,
				u32 param_type);
#endif /* _DSI_DISPLAY_H_ */
