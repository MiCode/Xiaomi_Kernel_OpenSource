/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <Johnson-CH.chiu@mediatek.com>
 *
 */
#ifndef __MTK_IMG_TRACE_H__
#define __MTK_IMG_TRACE_H__

#include <linux/kernel.h>
#include <linux/trace_events.h>
#define IMGSYS_FTRACE
#ifdef IMGSYS_FTRACE

#define IMGSYS_TRACE_LEN 1024

#define IMGSYS_TRACE_FORCE_BEGIN(fmt, args...) \
	__imgsys_systrace("B|%d|" fmt "\n", current->tgid, ##args)

#define IMGSYS_TRACE_FORCE_END() \
	__imgsys_systrace("E\n")

#define IMGSYS_SYSTRACE_BEGIN(fmt, args...) do { \
	if (imgsys_core_ftrace_enabled()) { \
		IMGSYS_TRACE_FORCE_BEGIN(fmt, ##args); \
	} \
} while (0)

#define IMGSYS_SYSTRACE_END() do { \
	if (imgsys_core_ftrace_enabled()) { \
		IMGSYS_TRACE_FORCE_END(); \
	} \
} while (0)

bool imgsys_core_ftrace_enabled(void);
void __imgsys_systrace(const char *fmt, ...);

#else

#define IMGSYS_SYSTRACE_BEGIN(fmt, args...)
#define IMGSYS_SYSTRACE_END()

#endif

#endif
