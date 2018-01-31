/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _DP_DISPLAY_H_
#define _DP_DISPLAY_H_

#include <drm/drmP.h>
#include <drm/msm_drm.h>

#include "dp_panel.h"

struct dp_display {
	struct drm_device *drm_dev;
	struct dp_bridge *bridge;
	struct drm_connector *connector;
	bool is_connected;
	u32 max_pclk_khz;

	int (*enable)(struct dp_display *dp_display);
	int (*post_enable)(struct dp_display *dp_display);

	int (*pre_disable)(struct dp_display *dp_display);
	int (*disable)(struct dp_display *dp_display);

	int (*set_mode)(struct dp_display *dp_display,
			struct dp_display_mode *mode);
	int (*validate_mode)(struct dp_display *dp_display, u32 mode_pclk_khz);
	int (*get_modes)(struct dp_display *dp_display,
		struct dp_display_mode *dp_mode);
	int (*prepare)(struct dp_display *dp_display);
	int (*unprepare)(struct dp_display *dp_display);
	int (*request_irq)(struct dp_display *dp_display);
	struct dp_debug *(*get_debug)(struct dp_display *dp_display);
	void (*post_open)(struct dp_display *dp_display);
	int (*config_hdr)(struct dp_display *dp_display,
				struct drm_msm_ext_hdr_metadata *hdr_meta);
	void (*post_init)(struct dp_display *dp_display);
};

int dp_display_get_num_of_displays(void);
int dp_display_get_displays(void **displays, int count);
#endif /* _DP_DISPLAY_H_ */
