/*
 * Copyright (c) 2020 MediaTek Inc.
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


#ifndef __DRTX_API_H__
#define __DRTX_API_H__

#include <drm/drmP.h>

void mtk_dp_SWInterruptSet(int bstatus);
int mtk_drm_dp_get_dev_info(struct drm_device *dev,
			void *data, struct drm_file *file_priv);
int mtk_drm_dp_audio_enable(struct drm_device *dev,
			void *data, struct drm_file *file_priv);
int mtk_drm_dp_audio_config(struct drm_device *dev,
			void *data, struct drm_file *file_priv);
int mtk_drm_dp_get_cap(struct drm_device *dev,
			void *data, struct drm_file *file_priv);
int mtk_drm_dp_get_info(struct drm_device *dev,
			struct drm_mtk_session_info *info);
void mtk_dp_get_dsc_capability(u8 *dsc_cap);
#endif

