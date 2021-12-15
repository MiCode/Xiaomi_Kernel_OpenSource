// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include "apusys_trace.h"

static noinline int tracing_mark_write(const char *buf)
{
	TRACE_PUTS(buf);
	return 0;
}

void trace_tag_begin(const char *format, ...)
{
	char buf[TRACE_LEN];

	int len = snprintf(buf, sizeof(buf),
		"B|%d|%s", task_pid_nr(current), format);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	tracing_mark_write(buf);
}
EXPORT_SYMBOL(trace_tag_begin);

void trace_tag_end(void)
{
	char buf[TRACE_LEN];

	int len = snprintf(buf, sizeof(buf), "E\n");

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	tracing_mark_write(buf);
}
EXPORT_SYMBOL(trace_tag_end);

void trace_async_tag(bool isBegin, const char *format, ...)
{
	char buf[TRACE_LEN];
	int len = 0;

	if (isBegin)
		len = snprintf(buf, sizeof(buf),
			       "S|%d|%s", task_pid_nr(current), format);
	else
		len = snprintf(buf, sizeof(buf),
			       "F|%d|%s", task_pid_nr(current), format);

	if (len >= TRACE_LEN)
		len = TRACE_LEN - 1;

	tracing_mark_write(buf);
}
EXPORT_SYMBOL(trace_async_tag);
