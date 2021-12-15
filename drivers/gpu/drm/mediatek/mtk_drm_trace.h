/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_DRM_TRACE__
#define __MTK_DRM_TRACE__

#include "mtk_drm_ddp_comp.h"

#define DRM_TRACE_ID 0xFFFF0000
#define DRM_TRACE_FPS_ID (DRM_TRACE_ID + 1)
#define DRM_TRACE_FENCE_ID (DRM_TRACE_ID + 2)
#define DRM_TRACE_VSYNC_ID (DRM_TRACE_ID + 3)

extern int hwc_pid;

/* MTK_DRM FTRACE */
extern bool g_trace_log;
#define mtk_drm_trace_begin(fmt, args...) do { \
	if (g_trace_log) { \
		preempt_disable(); \
		event_trace_printk(mtk_drm_get_tracing_mark(), \
			"B|%d|"fmt"\n", current->tgid, ##args); \
		preempt_enable();\
	} \
} while (0)

#define mtk_drm_trace_end() do { \
	if (g_trace_log) { \
		preempt_disable(); \
		event_trace_printk(mtk_drm_get_tracing_mark(), "E\n"); \
		preempt_enable(); \
	} \
} while (0)

#define mtk_drm_trace_async_begin(fmt, args...) do { \
		if (g_trace_log) { \
			preempt_disable(); \
			event_trace_printk(mtk_drm_get_tracing_mark(), \
				"S|%d|"fmt"\n", current->tgid, ##args); \
			preempt_enable();\
		} \
	} while (0)

#define mtk_drm_trace_async_end(fmt, args...) do { \
		if (g_trace_log) { \
			preempt_disable(); \
			event_trace_printk(mtk_drm_get_tracing_mark(), \
				"F|%d|"fmt"\n", current->tgid, ##args); \
			preempt_enable(); \
		} \
	} while (0)

#define mtk_drm_trace_c(fmt, args...) do { \
	if (g_trace_log) { \
		preempt_disable(); \
		event_trace_printk(mtk_drm_get_tracing_mark(), \
			"C|"fmt"\n", ##args); \
		preempt_enable();\
	} \
} while (0)

unsigned long mtk_drm_get_tracing_mark(void);
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
