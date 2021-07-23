/*
 * Copyright (c) 2014 MediaTek Inc.
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

#ifndef MTK_DRM_DEBUGFS_H
#define MTK_DRM_DEBUGFS_H

#include "mtk_drm_drv.h"

#ifdef CONFIG_DRM_MEDIATEK_DEBUG_FS
void mtk_drm_debugfs_init(struct drm_device *dev, struct mtk_drm_private *priv);
void mtk_drm_debugfs_deinit(void);
#endif
bool force_alpha(void);

#endif /* MTK_DRM_DEBUGFS_H */
