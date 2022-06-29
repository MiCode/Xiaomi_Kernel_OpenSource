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

void __imgsys_systrace(const char *fmt, ...)
{
	char buf[IMGSYS_TRACE_LEN];
	va_list args;
	int len;

	memset(buf, ' ', sizeof(buf));
	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == IMGSYS_TRACE_LEN))
		buf[IMGSYS_TRACE_LEN - 1] = '\0';

	tracing_mark_write(buf);
}

bool imgsys_core_ftrace_enabled(void)
{
	return imgsys_ftrace_en;
}


