/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DRM_CRTC_H_
#define _MI_DRM_CRTC_H_

#include <linux/types.h>

enum mi_layer_type {
	MI_LAYER_FOD_HBM_OVERLAY = BIT(0),
	MI_LAYER_FOD_ICON = BIT(1),
	MI_LAYER_AOD = BIT(2),
	MI_LAYER_FOD_ANIM = BIT(3),
};

struct mi_layer_flags {
	bool fod_overlay_flag;
	bool fod_icon_flag;
	bool aod_flag;
	bool fod_anim_flag;
};

struct mi_layer_state {
	struct mi_layer_flags layer_flags;
	u32 current_backlight;
};

int mi_drm_crtc_update_layer_state(struct drm_crtc *crtc);
int mi_drm_bl_wait_for_completion(struct drm_crtc *crtc, unsigned int level);

#endif /* _MI_DRM_CRTC_H_ */

