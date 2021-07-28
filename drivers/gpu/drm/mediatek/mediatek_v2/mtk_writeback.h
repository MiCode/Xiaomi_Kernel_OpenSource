/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_WRITEBACK_H
#define MTK_WRITEBACK_H

#include "mtk_drm_crtc.h"

int mtk_wb_connector_init(struct drm_device *drm_dev,
			  struct mtk_drm_crtc *mtk_crtc);
void mtk_wb_remove_comp_from_path(struct mtk_drm_crtc *mtk_crtc);
int mtk_wb_set_possible_crtcs(struct drm_device *drm_dev,
			      struct mtk_drm_crtc *mtk_crtc,
			      unsigned long possible_crtc);
#ifndef DRM_CMDQ_DISABLE
void mtk_wb_atomic_commit(struct mtk_drm_crtc *mtk_crtc, unsigned int v,
			  unsigned int h, void *cmdq_handle);
#else
void mtk_wb_atomic_commit(struct mtk_drm_crtc *mtk_crtc);
#endif

#endif
