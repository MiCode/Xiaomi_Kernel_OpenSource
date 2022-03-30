// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <Johnson-CH.chiu@mediatek.com>
 *
 */

#include "mtk_imgsys-trace.h"

int imgsys_ftrace_en;
module_param(imgsys_ftrace_en, int, 0644);

static noinline int tracing_mark_write(const char *buf)
{
	trace_puts(buf);
	return 0;
}

void __imgsys_systrace_b(pid_t tgid, const char *fmt, ...)
{
	char log[256];
	va_list args;
	int len;
	char buf2[256];

	memset(log, ' ', sizeof(log));
	va_start(args, fmt);
	len = vsnprintf(log, sizeof(log), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		log[255] = '\0';

	len = snprintf(buf2, sizeof(buf2), "B|%d|%s\n", tgid, log);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	tracing_mark_write(buf2);
}

void __imgsys_systrace_e(void)
{
	char buf2[256];
	int len;

	len = snprintf(buf2, sizeof(buf2), "E\n");

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == 256))
		buf2[255] = '\0';

	tracing_mark_write(buf2);
}

bool imgsys_core_ftrace_enabled(void)
{
	return imgsys_ftrace_en;
}


