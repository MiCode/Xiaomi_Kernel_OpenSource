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

#ifdef IMGSYS_FTRACE

#define IMGSYS_TRACE_FORCE_BEGIN(fmt, args...) do { \
	preempt_disable(); \
	event_trace_printk(imgsys_get_tracing_mark(), \
		"B|%d|"fmt, current->tgid, ##args); \
	preempt_enable();\
} while (0)

#define IMGSYS_TRACE_FORCE_END() do { \
	preempt_disable(); \
	event_trace_printk(imgsys_get_tracing_mark(), "E\n"); \
	preempt_enable(); \
} while (0)


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
unsigned long imgsys_get_tracing_mark(void);

#else

#define IMGSYS_SYSTRACE_BEGIN(fmt, args...)
#define IMGSYS_SYSTRACE_END()

#endif

#endif
