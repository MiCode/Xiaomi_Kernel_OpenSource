/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */

#ifndef _MI_DRM_CRTC_H_
#define _MI_DRM_CRTC_H_

enum mi_layer_type {
	MI_DIMLAYER_NULL = 0x0,
	MI_DIMLAYER_FOD_HBM_OVERLAY = 0x1,
	MI_DIMLAYER_FOD_ICON = 0x2,
	MI_DIMLAYER_AOD = 0x4,
	MI_DIMLAYER_FOD_ANIM = 0x8,
	MI_DIMLAYER_MAX,
};

int mi_drm_crtc_fod_hbm_fence(struct drm_crtc *crtc);

#endif /* _MI_DRM_CRTC_H_ */

