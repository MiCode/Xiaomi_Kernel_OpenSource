/* Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _DP_MST_SIM_H_
#define _DP_MST_SIM_H_

#include "dp_aux_bridge.h"
#include "dp_mst_sim_helper.h"
#include <drm/drm_connector.h>
#include <drm/drm_modes.h>

enum dp_sim_mode_type {
	DP_SIM_MODE_EDID       = 0x00000001,
	DP_SIM_MODE_DPCD_READ  = 0x00000002,
	DP_SIM_MODE_DPCD_WRITE = 0x00000004,
	DP_SIM_MODE_LINK_TRAIN = 0x00000008,
	DP_SIM_MODE_MST        = 0x00000010,
	DP_SIM_MODE_ALL        = 0x0000001F,
};

int dp_sim_create_bridge(struct device *dev,
		struct dp_aux_bridge **bridge);

int dp_sim_destroy_bridge(struct dp_aux_bridge *bridge);

int dp_sim_set_sim_mode(struct dp_aux_bridge *bridge, u32 sim_mode);

int dp_sim_update_port_num(struct dp_aux_bridge *bridge, u32 port_num);

int dp_sim_update_port_status(struct dp_aux_bridge *bridge,
		int port, enum drm_connector_status status);

int dp_sim_update_port_edid(struct dp_aux_bridge *bridge,
		int port, const u8 *edid, u32 size);

int dp_sim_write_dpcd_reg(struct dp_aux_bridge *bridge,
		const u8 *dpcd, u32 size, u32 offset);

int dp_sim_read_dpcd_reg(struct dp_aux_bridge *bridge,
		u8 *dpcd, u32 size, u32 offset);

#endif /* _DP_MST_SIM_H_ */
