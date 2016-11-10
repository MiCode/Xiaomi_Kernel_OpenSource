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

#ifndef _DBA_BRIDGE_H_
#define _DBA_BRIDGE_H_

#include <linux/types.h>
#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

#include "msm_drv.h"

/**
 * struct dba_bridge_init - Init parameters for DBA bridge
 * @client_name:          Client's name who calls the init
 * @chip_name:            Bridge chip name
 * @id:                   Bridge driver index
 * @display:              Private display handle
 * @hdmi_mode:            HDMI or DVI mode for the sink
 * @num_of_input_lanes:   Number of input lanes in case of DSI/LVDS
 * @precede_bridge:       Precede bridge chip
 * @pluggable:            If it's pluggable
 * @panel_count:          Number of panels attached to this display
 */
struct dba_bridge_init {
	const char *client_name;
	const char *chip_name;
	u32 id;
	void *display;
	bool hdmi_mode;
	u32 num_of_input_lanes;
	struct drm_bridge *precede_bridge;
	bool pluggable;
	u32 panel_count;
};

/**
 * dba_bridge_init - Initialize the DBA bridge
 * @dev:           Pointer to drm device handle
 * @encoder:       Pointer to drm encoder handle
 * @data:          Pointer to init data
 * Returns: pointer of struct drm_bridge
 */
struct drm_bridge *dba_bridge_init(struct drm_device *dev,
				struct drm_encoder *encoder,
				struct dba_bridge_init *data);

/**
 * dba_bridge_cleanup - Clean up the DBA bridge
 * @bridge:           Pointer to DBA bridge handle
 * Returns: void
 */
void dba_bridge_cleanup(struct drm_bridge *bridge);

#endif /* _DBA_BRIDGE_H_ */
