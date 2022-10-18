/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_MST_DRM_H_
#define _DP_MST_DRM_H_

#include <linux/types.h>
#include <drm/drm_crtc.h>
#include <drm/drm_bridge.h>

#include "dp_display.h"

#if IS_ENABLED(CONFIG_DRM_MSM_DP_MST)

/**
 * dp_mst_drm_bridge_init - initialize mst bridge
 * @display: Pointer to private display structure
 * @encoder: Pointer to encoder for mst bridge mapping
 */
int dp_mst_drm_bridge_init(void *display,
	struct drm_encoder *encoder);

/**
 * dp_mst_drm_bridge_deinit - de-initialize mst bridges
 * @display: Pointer to private display structure
 */
void dp_mst_drm_bridge_deinit(void *display);

/**
 * dp_mst_init - initialize mst objects for the given display
 * @display: Pointer to private display structure
 */
int dp_mst_init(struct dp_display *dp_display);

/**
 * dp_mst_deinit - de-initialize mst objects for the given display
 * @display: Pointer to private display structure
 */
void dp_mst_deinit(struct dp_display *dp_display);

/**
 * dp_mst_clear_edid_cache - clear mst edid cache for the given display
 * @display: Pointer to private display structure
 */
void dp_mst_clear_edid_cache(void *dp_display);
#else

static inline int dp_mst_drm_bridge_init(void *display,
	struct drm_encoder *encoder)
{
	return 0;
}

static inline void dp_mst_drm_bridge_deinit(void *display)
{
}

static inline int dp_mst_init(struct dp_display *dp_display)
{
	return 0;
}

static inline int dp_mst_deinit(struct dp_display *dp_display)
{
	return 0;
}

static inline void dp_mst_clear_edid_cache(void *display)
{
}
#endif /* CONFIG_DRM_MSM_DP_MST */

#endif /* _DP_MST_DRM_H_ */
