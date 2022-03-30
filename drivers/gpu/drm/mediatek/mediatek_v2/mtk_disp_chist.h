/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2021 MediaTek Inc.
 */
#ifndef __MTK_DISP_CHIST_H__
#define __MTK_DISP_CHIST_H__

#include <linux/uaccess.h>
#include <drm/mediatek_drm.h>

struct mtk_disp_chist_data {
	bool support_shadow;
	unsigned int module_count;
	unsigned int color_format;
	unsigned int max_channel;
	unsigned int max_bin;
};

struct mtk_disp_chist {
	struct mtk_ddp_comp ddp_comp;
	struct drm_crtc *crtc;
	const struct mtk_disp_chist_data *data;
};

static inline struct mtk_disp_chist *comp_to_chist(struct mtk_ddp_comp *comp)
{
	return container_of(comp, struct mtk_disp_chist, ddp_comp);
}

int mtk_drm_ioctl_get_chist(struct drm_device *dev, void *data,
	struct drm_file *file_priv);


int mtk_drm_ioctl_get_chist_caps(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

int mtk_drm_ioctl_set_chist_config(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif

