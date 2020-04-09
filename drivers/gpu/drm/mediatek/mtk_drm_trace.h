/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_DRM_TRACE__
#define __MTK_DRM_TRACE__

#include "mtk_drm_ddp_comp.h"

void drm_trace_tag_start(const char *tag);
void drm_trace_tag_end(const char *tag);
void drm_trace_tag_mark(const char *tag);
void mtk_drm_refresh_tag_start(struct mtk_ddp_comp *ddp_comp);
void mtk_drm_refresh_tag_end(struct mtk_ddp_comp *ddp_comp);
void MMPathTraceDRM(struct mtk_ddp_comp *ddp_comp);
int MMPathTraceRDMA(struct mtk_ddp_comp *ddp_comp, char *str,
	unsigned int strlen, unsigned int n);
int MMPathTraceWDMA(struct mtk_ddp_comp *ddp_comp, char *str,
	unsigned int strlen, unsigned int n);
int MMPathTraceCrtcPlanes(struct drm_crtc *crtc,
	char *str, int strlen, int n);
#endif
