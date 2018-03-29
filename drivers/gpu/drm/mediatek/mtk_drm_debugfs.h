/*
 * Copyright (c) 2014 MediaTek Inc.
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

struct drm_device;
struct mtk_drm_private;

#ifdef CONFIG_DEBUG_FS
void mtk_drm_debugfs_init(struct drm_device *dev,
			  struct mtk_drm_private *priv);
void mtk_drm_debugfs_deinit(void);
#else
static inline void mtk_drm_debugfs_init(struct drm_device *dev,
					struct mtk_drm_private *priv) {}
static inline void mtk_drm_debugfs_deinit(void) {}
#endif
bool force_alpha(void);

#endif /* MTK_DRM_DEBUGFS_H */
