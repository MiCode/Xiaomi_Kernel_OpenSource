/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_DISP_CHIST_H__
#define __MTK_DISP_CHIST_H__

#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>

int mtk_drm_ioctl_get_chist(struct drm_device *dev, void *data,
	struct drm_file *file_priv);


int mtk_drm_ioctl_get_chist_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_set_chist_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif

