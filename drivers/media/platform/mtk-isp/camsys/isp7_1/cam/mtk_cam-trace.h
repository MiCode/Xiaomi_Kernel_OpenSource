/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_CAM_TRACE_H
#define __MTK_CAM_TRACE_H


#if IS_ENABLED(CONFIG_FTRACE) && IS_ENABLED(CONFIG_DEBUG_FS)
void mtk_cam_systrace_begin(const char *format, ...);
void mtk_cam_systrace_end(void);
void mtk_cam_systrace_async(bool is_begin, int val, const char *format, ...);

#define mtk_cam_systrace_begin_frame(prefix, raw_id, frame_id) \
	mtk_cam_systrace_begin("%s: raw%d - frame %d", prefix, raw_id, frame_id)

#define mtk_cam_systrace_begin_func(...) \
	mtk_cam_systrace_begin("%s", __func__)

#else
static void mtk_cam_systrace_begin(const char *format, ...)
{
}
static void mtk_cam_systrace_end(void)
{
}

static void mtk_cam_systrace_async(bool isBegin, int val, const char *format, ...)
{
}

#define mtk_cam_systrace_begin_frame(raw_id, frame_id)
#define mtk_cam_systrace_begin_func(...)

#endif

#endif /* __MTK_CAM_TRACE_H */
