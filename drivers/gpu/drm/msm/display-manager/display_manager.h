/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _DISPLAY_MANAGER_H_
#define _DISPLAY_MANAGER_H_

#define MAX_H_TILES_PER_DISPLAY 2

/**
 * enum display_interface_type - enumerates display interface types
 * @DISPLAY_INTF_DSI:        DSI interface
 * @DISPLAY_INTF_HDMI:       HDMI interface
 * @DISPLAY_INTF_DP:         Display Port interface
 */
enum display_interface_type {
	DISPLAY_INTF_DSI = 0,
	DISPLAY_INTF_HDMI,
	DISPLAY_INTF_DP,
	DISPLAY_INTF_MAX,
};

/**
 * enum display_compression_type - compression method used for pixel stream
 * @DISPLAY_COMPRESISON_NONE:     Pixel data is not compressed.
 * @DISPLAY_COMPRESSION_DSC:      DSC compresison is used.
 * @DISPLAY_COMPRESSION_FBC:      FBC compression is used.
 */
enum display_compression_type {
	DISPLAY_COMPRESSION_NONE = 0,
	DISPLAY_COMPRESSION_DSC,
	DISPLAY_COMPRESSION_FBC,
	DISPLAY_COMPRESISON_MAX
};

/**
 * enum display_interface_mode - interface modes supported by the display
 * @DISPLAY_INTF_MODE_VID:       Display supports video or "active" mode
 * @DISPLAY_INTF_MODE_CMD:       Display supports command mode
 */
enum display_interface_mode {
	DISPLAY_INTF_MODE_VID		= BIT(0),
	DISPLAY_INTF_MODE_CMD		= BIT(1),
};

/**
 * struct display_info - defines display properties
 * @intf:               The interface on which display is connected to SOC.
 * @num_of_h_tiles:     number of horizontal tiles in case of split interface.
 * @h_tile_instance:    controller instance used per tile. Number of elements is
 *			based on num_of_h_tiles.
 * @is_hot_pluggable:   Set to true if hot plug detection is supported.
 * @is_connected:       Set to true if display is connected.
 * @is_edid_supported:  True if display supports EDID.
 * @max_width:          Max width of display. In case of hot pluggable display,
 *			this is max width supported by controller.
 * @max_height:         Max height of display. In case of hot pluggable display,
 *			this is max height supported by controller.
 * @compression:        Compression supported by the display.
 * @intf_mode:          Bitmask of interface modes supported by the display
 */
struct display_info {
	enum display_interface_type intf;

	u32 num_of_h_tiles;
	u32 h_tile_instance[MAX_H_TILES_PER_DISPLAY];

	bool is_hot_pluggable;
	bool is_connected;
	bool is_edid_supported;

	u32 max_width;
	u32 max_height;

	enum display_compression_type compression;
	enum display_interface_mode intf_mode;
};

struct display_manager {
	struct platform_device *pdev;
	const char *name;

	struct mutex lock;
	u32 display_count;
	u32 dsi_display_count;
	u32 hdmi_display_count;
	u32 dp_display_count;
	/* Debug fs */
	struct dentry *debugfs_root;
};

/**
 * display_manager_get_count() - returns the number of display present
 * @disp_m:      Handle to Display manager.
 *
 * Returns the sum total of DSI, HDMI and DP display present on the board.
 *
 * Return: error code (< 0) in case of error or number of display ( >= 0)
 */
int display_manager_get_count(struct display_manager *disp_m);

/**
 * display_manager_get_info_by_index() - returns display information
 * @disp_m:        Handle to Display manager.
 * @display_index: display index (valid indices are 0 to (display_count - 1).
 * @info:          Structure where display info is copied.
 *
 * Return: error code.
 */
int display_manager_get_info_by_index(struct display_manager *disp_m,
				      u32 display_index,
				      struct display_info *info);

/**
 * display_manager_drm_init_by_index() - initialize drm objects for display
 * @disp_m:         Handle to Display manager.
 * @display_index:  display index (valid indices are 0 to (display_count - 1).
 * @encoder:        Pointer to encoder object to which display is attached.
 *
 * Return: error code.
 */
int display_manager_drm_init_by_index(struct display_manager *disp_m,
				      u32 display_index,
				      struct drm_encoder *encoder);

/**
 * display_manager_drm_deinit_by_index() - detroys drm objects
 * @disp_m:         Handle to Display manager.
 * @display_index:  display index (valid indices are 0 to (display_count - 1).
 *
 * Return: error code.
 */
int display_manager_drm_deinit_by_index(struct display_manager *disp_m,
					u32 display_index);

/**
 * display_manager_register() - register display interface drivers
 */
void display_manager_register(void);

/**
 * display_manager_unregister() - unregisters display interface drivers
 */
void display_manager_unregister(void);

#endif /* _DISPLAY_MANAGER_H_ */
