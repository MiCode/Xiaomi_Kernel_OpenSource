/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef MTK_DRM_LEASE_H
#define MTK_DRM_LEASE_H

int mtk_drm_set_lease_info_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);

int mtk_drm_get_lease_info_ioctl(struct drm_device *dev, void *data,
		struct drm_file *file_priv);

#endif
