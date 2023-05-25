/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 */

#ifndef _ADAPTOR_TRACE_H__
#define _ADAPTOR_TRACE_H__

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/trace_events.h>
#define ADAPTOR_FTRACE
#ifdef ADAPTOR_FTRACE

#define BUF_LENGTH 256

#define ADAPTOR_TRACE_FORCE_BEGIN(fmt, args...)	\
	__adaptor_systrace_b(fmt, ##args)

#define ADAPTOR_TRACE_FORCE_END()	__adaptor_systrace_e()


#define ADAPTOR_SYSTRACE_BEGIN(fmt, args...) do { \
	if (adaptor_trace_enabled()) { \
		ADAPTOR_TRACE_FORCE_BEGIN(fmt, ##args); \
	} \
} while (0)

#define ADAPTOR_SYSTRACE_END() do { \
	if (adaptor_trace_enabled()) { \
		ADAPTOR_TRACE_FORCE_END(); \
	} \
} while (0)

bool adaptor_trace_enabled(void);
void __adaptor_systrace_b(const char *fmt, ...);
void __adaptor_systrace_e(void);

#else

#define ADAPTOR_SYSTRACE_BEGIN(fmt, args...)
#define ADAPTOR_SYSTRACE_END()

#endif

#endif
