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

#if defined(CONFIG_DEBUG_FS)
int mtk_drm_dsi_debugfs_init(struct mtk_dsi *dsi);
void mtk_drm_dsi_debugfs_exit(struct mtk_dsi *dsi);
#else
int mtk_drm_dsi_debugfs_init(struct mtk_dsi *dsi)
{
	return 0;
}

void mtk_drm_dsi_debugfs_exit(struct mtk_dsi *dsi)
{
}
#endif /* CONFIG_DEBUG_FS */

#endif /* MTK_DRM_DEBUGFS_H */
