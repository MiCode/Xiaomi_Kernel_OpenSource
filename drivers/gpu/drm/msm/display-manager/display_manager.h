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

struct display_manager {
	struct drm_device *drm_dev;
	struct platform_device *pdev;
	const char *name;

	struct mutex lock;

	u32 display_count;
	void **displays;

	u32 dsi_display_count;
	void **dsi_displays;

	u32 hdmi_display_count;
	void **hdmi_displays;

	u32 dp_display_count;
	void **dp_displays;

	u32 wb_display_count;
	void **wb_displays;

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
				      struct msm_display_info *info);

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
