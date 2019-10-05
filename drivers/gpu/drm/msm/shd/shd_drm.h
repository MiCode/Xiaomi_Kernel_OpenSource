/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#ifndef _SHD_DRM_H_
#define _SHD_DRM_H_

#include <linux/types.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include "msm_drv.h"
#include "dsi_display.h"

struct shd_mode_info {
	int x_offset;
	int y_offset;
	int width;
	int height;
};

struct shd_stage_range {
	u32 start;
	u32 size;
};

struct shd_display_base {
	struct mutex           base_mutex;
	struct drm_display_mode mode;
	struct drm_crtc       *crtc;
	struct drm_encoder    *encoder;
	struct drm_connector  *connector;
	struct list_head       head;
	struct list_head       disp_list;
	struct device_node    *of_node;
	struct sde_connector_ops ops;

	int intf_idx;
	int connector_type;
	bool mst_port;
	bool enabled;
	bool enable_changed;
};

struct shd_display {
	struct dsi_display dsi_base;
	struct drm_device *drm_dev;
	const char *name;
	const char *display_type;

	struct shd_display_base *base;
	struct drm_bridge *bridge;

	struct device_node *base_of;
	struct sde_rect src;
	struct sde_rect roi;
	struct shd_stage_range stage_range;

	struct platform_device *pdev;
	struct completion vsync_comp;
	struct list_head head;

	bool enabled;
	bool enable_changed;
};

/* drm internal header */
struct drm_minor *drm_minor_acquire(unsigned int minor_id);
void drm_vblank_cleanup(struct drm_device *dev);
void drm_minor_release(struct drm_minor *minor);

void *sde_encoder_phys_shd_init(enum sde_intf_type type,
			u32 controller_id, void *phys_init_params);


#endif /* _SHD_DRM_H_ */
