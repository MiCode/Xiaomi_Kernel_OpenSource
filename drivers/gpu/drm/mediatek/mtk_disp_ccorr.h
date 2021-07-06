/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __MTK_DISP_CCORR_H__
#define __MTK_DISP_CCORR_H__

#include <drm/mediatek_drm.h>

void ccorr_test(const char *cmd, char *debug_output);
int ccorr_interface_for_color(unsigned int ccorr_idx,
	unsigned int ccorr_coef[3][3], void *handle);
void disp_ccorr_on_end_of_frame(struct mtk_ddp_comp *comp);
void disp_pq_notify_backlight_changed(int bl_1024);
int disp_ccorr_set_color_matrix(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int32_t matrix[16], int32_t hint, bool fte_flag);
int disp_ccorr_set_RGB_Gain(struct mtk_ddp_comp *comp,
	struct cmdq_pkt *handle, int r, int g, int b);
int mtk_drm_ioctl_set_ccorr(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_ccorr_eventctl(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_ccorr_get_irq(struct drm_device *dev, void *data,
	struct drm_file *file_priv);
int mtk_drm_ioctl_support_color_matrix(struct drm_device *dev, void *data,
	struct drm_file *file_priv);

#endif

