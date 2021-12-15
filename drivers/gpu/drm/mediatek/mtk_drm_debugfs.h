/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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
