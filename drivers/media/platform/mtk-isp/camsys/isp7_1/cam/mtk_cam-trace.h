/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MTK_CAM_TRACE_H
#define __MTK_CAM_TRACE_H

#define MTK_CAM_TRACE_ENABLE	0

#if IS_ENABLED(CONFIG_TRACING) && MTK_CAM_TRACE_ENABLE

#include <stdarg.h>
#include <linux/sched.h>
#include <linux/kernel.h>

bool mtk_cam_trace_enabled(void);

__printf(1, 2)
void mtk_cam_trace(const char *fmt, ...);

#define MTK_CAM_TRACE_BEGIN(fmt, args...)			\
do {								\
	if (unlikely(mtk_cam_trace_enabled()))			\
		mtk_cam_trace("B|%d|camsys:" fmt "\n",		\
			      task_tgid_nr(current), ##args);	\
} while (0)

#define MTK_CAM_TRACE_END()					\
do {								\
	if (unlikely(mtk_cam_trace_enabled()))			\
		mtk_cam_trace("E|%d\n",				\
			      task_tgid_nr(current));		\
} while (0)

#define MTK_CAM_TRACE_FUNC_BEGIN()		\
	MTK_CAM_TRACE_BEGIN("%s", __func__)

#else

#define MTK_CAM_TRACE_BEGIN(fmt, args...)
#define MTK_CAM_TRACE_END()
#define MTK_CAM_TRACE_FUNC_BEGIN()

#endif

#endif /* __MTK_CAM_TRACE_H */
