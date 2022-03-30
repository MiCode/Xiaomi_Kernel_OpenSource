/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_DRM_ASSERT_H
#define _MTK_DRM_ASSERT_H

#include "mtk_drm_assert_ext.h"

int mtk_drm_assert_layer_init(struct drm_crtc *crtc);
void mtk_drm_assert_fb_init(struct drm_device *dev, u32 width, u32 height);
void mtk_drm_assert_init(struct drm_device *dev);
int mtk_drm_dal_enable(void);
#ifndef DRM_CMDQ_DISABLE
int drm_show_dal(struct drm_crtc *crtc, bool enable);
#endif
void drm_set_dal(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle);
void drm_update_dal(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle);
#endif
