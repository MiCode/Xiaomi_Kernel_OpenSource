/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_DRM_TRACE__
#define __MTK_DRM_TRACE__

#include "mtk_drm_ddp_comp.h"

#define DRM_TRACE_ID 0xFFFF0000
#define DRM_TRACE_FPS_ID (DRM_TRACE_ID + 1)
#define DRM_TRACE_FENCE_ID (DRM_TRACE_ID + 2)
#define DRM_TRACE_VSYNC_ID (DRM_TRACE_ID + 3)


/* MTK_DRM FTRACE */
extern bool g_trace_log;
#define mtk_drm_trace_begin(fmt, args...) do { \
	if (g_trace_log) { \
		mtk_drm_print_trace( \
			"B|%d|"fmt"\n", current->tgid, ##args); \
	} \
} while (0)

#define mtk_drm_trace_end() do { \
	if (g_trace_log) { \
		mtk_drm_print_trace("E\n"); \
	} \
} while (0)

#define mtk_drm_trace_async_begin(fmt, args...) do { \
	if (g_trace_log) { \
		mtk_drm_print_trace( \
			"S|%d|"fmt"\n", current->tgid, ##args); \
	} \
} while (0)

#define mtk_drm_trace_async_end(fmt, args...) do { \
	if (g_trace_log) { \
		mtk_drm_print_trace( \
			"F|%d|"fmt"\n", current->tgid, ##args); \
	} \
} while (0)

#define mtk_drm_trace_c(fmt, args...) do { \
	if (g_trace_log) { \
		mtk_drm_print_trace( \
			"C|"fmt"\n", ##args); \
	} \
} while (0)

void mtk_drm_print_trace(char *fmt, ...);
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
