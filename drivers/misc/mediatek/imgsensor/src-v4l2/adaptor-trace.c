// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include "adaptor-trace.h"

uint adaptor_trace_en;
module_param(adaptor_trace_en, uint, 0644);
MODULE_PARM_DESC(adaptor_trace_en, "adaptor_trace_en");

static noinline int tracing_mark_write(const char *buf)
{
	trace_puts(buf);
	return 0;
}

void __adaptor_systrace_b(const char *fmt, ...)
{
	char log[BUF_LENGTH];
	va_list args;
	int len;
	char buf2[BUF_LENGTH];

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == BUF_LENGTH))
		log[BUF_LENGTH - 1] = '\0';

	len = snprintf(buf2, sizeof(buf2), "B|%d|%s\n", task_tgid_nr(current), log);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == BUF_LENGTH))
		buf2[BUF_LENGTH - 1] = '\0';

	tracing_mark_write(buf2);
}

void __adaptor_systrace_e(void)
{
	char buf2[BUF_LENGTH];
	int len;

	len = snprintf(buf2, sizeof(buf2), "E|%d\n", task_tgid_nr(current));

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == BUF_LENGTH))
		buf2[BUF_LENGTH - 1] = '\0';

	tracing_mark_write(buf2);
}

bool adaptor_trace_enabled(void)
{
	return adaptor_trace_en;
}


