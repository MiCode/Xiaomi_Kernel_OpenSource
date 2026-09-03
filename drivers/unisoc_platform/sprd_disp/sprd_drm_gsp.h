/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _SPRD_DRM_GSP_H
#define _SPRD_DRM_GSP_H

#if IS_ENABLED(CONFIG_UNISOC_GSP)
int sprd_gsp_get_capability_ioctl(struct drm_device *dev,
				void *data, struct drm_file *file_priv);
int sprd_gsp_trigger_ioctl(struct drm_device *dev,
				void *data, struct drm_file *file_priv);
#else
static inline int sprd_gsp_get_capability_ioctl(
			struct drm_device *dev, void *data,
			struct drm_file *file_priv)
{
	pr_err("gsp get cap not implement\n");
	return -ENODEV;
}

static inline int sprd_gsp_trigger_ioctl(struct drm_device *dev,
					       void *data,
					       struct drm_file *file_priv)
{
	pr_err("gsp trigger not implement\n");
	return -ENODEV;
}
#endif

#endif
