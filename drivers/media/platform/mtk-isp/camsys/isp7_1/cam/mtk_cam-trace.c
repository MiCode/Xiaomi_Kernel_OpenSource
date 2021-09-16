// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <stdarg.h>

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kernel.h>

#define TRACE_LEN 256
#define SYSTRACE_PREF_BEGIN "B"
#define SYSTRACE_PREF_END "E"
#define SYSTRACE_PREF_CUSTOM "C"
#define SYSTRACE_PREF_ASYNC_BEGIN "S"
#define SYSTRACE_PREF_ASYNC_END "F"

static noinline int tracing_mark_write(const char *format, ...)
{
	int len;
	char buf[TRACE_LEN];
	va_list args;

	memset(buf, ' ', sizeof(buf));
	va_start(args, format);
	len = vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if (unlikely(len < 0))
		return len;
	else if (unlikely(len == TRACE_LEN))
		buf[TRACE_LEN - 1] = '\0';

	trace_puts(buf);
	return 0;
}

void mtk_cam_systrace_begin(const char *format, ...)
{
	int len;
	char log[256];
	va_list args;

	memset(log, ' ', sizeof(log));
	va_start(args, format);
	len = vsnprintf(log, sizeof(log), format, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == TRACE_LEN))
		log[TRACE_LEN - 1] = '\0';

	tracing_mark_write("%s|%d|camsys:%s\n",
			SYSTRACE_PREF_BEGIN, task_pid_nr(current), log);
}

void mtk_cam_systrace_end(void)
{
	tracing_mark_write("E\n");
}

void mtk_cam_systrace_async(bool is_begin, int val, const char *format, ...)
{
	char log[TRACE_LEN];
	int len;
	va_list args;

	memset(log, ' ', sizeof(log));
	va_start(args, format);
	len = vsnprintf(log, sizeof(log), format, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == TRACE_LEN))
		log[TRACE_LEN - 1] = '\0';

	tracing_mark_write("%s|%d|camsys:%s|%d\n",
		(is_begin) ? SYSTRACE_PREF_ASYNC_BEGIN : SYSTRACE_PREF_ASYNC_END,
		task_pid_nr(current), log, val);
}
