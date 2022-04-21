/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_CAM_TRACE_H
#define __MTK_CAM_TRACE_H

#if IS_ENABLED(CONFIG_TRACING) && defined(MTK_CAM_TRACE_SUPPORT)

#include <stdarg.h>
#include <linux/sched.h>
#include <linux/kernel.h>

int mtk_cam_trace_enabled_tags(void);

#define _MTK_CAM_TRACE_ENABLED(category)	\
	(mtk_cam_trace_enabled_tags() & (1UL << category))

__printf(1, 2)
void mtk_cam_trace(const char *fmt, ...);

#define _MTK_CAM_TRACE(category, fmt, args...)			\
do {								\
	if (unlikely(_MTK_CAM_TRACE_ENABLED(category)))		\
		mtk_cam_trace(fmt "\n",	##args);		\
} while (0)

#else

#define _MTK_CAM_TRACE_ENABLED(category)	0
#define _MTK_CAM_TRACE(category, fmt, args...)

#endif

enum trace_category {
	TRACE_BASIC,
	TRACE_HW_IRQ,
	TRACE_BUFFER,
	TRACE_FBC,
};

#define _TRACE_CAT(cat)		TRACE_ ## cat

#define MTK_CAM_TRACE_ENABLED(category)	\
	_MTK_CAM_TRACE_ENABLED(_TRACE_CAT(category))

#define MTK_CAM_TRACE(category, fmt, args...)				\
	_MTK_CAM_TRACE(_TRACE_CAT(category), "camsys:" fmt, ##args)

/*
 * systrace format
 */

#define MTK_CAM_TRACE_BEGIN(category, fmt, args...)			\
	_MTK_CAM_TRACE(_TRACE_CAT(category), "B|%d|camsys:" fmt,	\
		      task_tgid_nr(current), ##args)			\

#define MTK_CAM_TRACE_END(category)					\
	_MTK_CAM_TRACE(_TRACE_CAT(category), "E|%d",			\
		      task_tgid_nr(current))				\

#define MTK_CAM_TRACE_FUNC_BEGIN(category)				\
	MTK_CAM_TRACE_BEGIN(category, "%s", __func__)

#endif /* __MTK_CAM_TRACE_H */
