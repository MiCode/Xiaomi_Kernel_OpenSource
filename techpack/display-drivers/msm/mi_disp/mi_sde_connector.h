/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_SDE_CONNECTOR_H_
#define _MI_SDE_CONNECTOR_H_

#include <linux/types.h>
#include "mi_disp_config.h"
#include <drm/drm_connector.h>

struct sde_connector;

enum mi_layer_type {
	MI_LAYER_FOD_HBM_OVERLAY = BIT(0),
	MI_LAYER_FOD_ICON = BIT(1),
	MI_LAYER_AOD = BIT(2),
	MI_LAYER_FOD_ANIM = BIT(3),
};

struct mi_layer_flags
{
	bool fod_overlay_flag;
	bool fod_icon_flag;
	bool aod_flag;
	bool fod_anim_flag;
};

struct mi_layer_state
{
	struct mi_layer_flags layer_flags;
	u32 current_backlight;
};

int mi_sde_connector_register_esd_irq(struct sde_connector *c_conn);

int mi_sde_connector_update_layer_state(struct drm_connector *connector,
		u32 mi_layer_type);

int mi_sde_connector_flat_fence(struct drm_connector *connector);

#if MI_DISP_DEBUGFS_ENABLE
int mi_sde_connector_debugfs_esd_sw_trigger(void *display);
#else
static inline int mi_sde_connector_debugfs_esd_sw_trigger(void *display) { return 0; }
#endif

int mi_sde_connector_panel_ctl(struct drm_connector *connector, uint32_t op_code);

#endif /* _MI_SDE_CONNECTOR_H_ */
