/*
 * Copyright (c) 2017 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#ifdef CONFIG_MTK_DISPLAY_CMDQ
void mtk_wb_atomic_commit(struct mtk_drm_crtc *mtk_crtc, unsigned int v,
			  unsigned int h, void *cmdq_handle);
#else
void mtk_wb_atomic_commit(struct mtk_drm_crtc *mtk_crtc);
#endif

#endif
