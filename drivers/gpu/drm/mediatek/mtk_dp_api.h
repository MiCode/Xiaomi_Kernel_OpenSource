/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

