/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_DRM_ASSERT_H
#define _MTK_DRM_ASSERT_H

#include "mtk_drm_assert_ext.h"

int mtk_drm_assert_layer_init(struct drm_crtc *crtc);
void mtk_drm_assert_fb_init(struct drm_device *dev, u32 width, u32 height);
int mtk_drm_dal_enable(void);
int drm_show_dal(struct drm_crtc *crtc, bool enable);
void drm_set_dal(struct drm_crtc *crtc, struct cmdq_pkt *cmdq_handle);

#endif
