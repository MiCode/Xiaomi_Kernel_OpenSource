/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DISP_GAMMA_H__
#define __MTK_DISP_GAMMA_H__

#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>

#define GAMMA_ENTRY(r10, g10, b10) (((r10) << 20) | ((g10) << 10) | (b10))

int mtk_drm_ioctl_set_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_set_12bit_gammalut(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_bypass_disp_gamma(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

void disp_gamma_on_start_of_frame(void);

#endif

