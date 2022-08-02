// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_cam-trace.h"

#include <linux/module.h>
#include <linux/trace_events.h>

static int ftrace_tags;
module_param(ftrace_tags, int, 0644);
MODULE_PARM_DESC(ftrace_tags, "enable ftrace tags (bitmask)");

int mtk_cam_trace_enabled_tags(void)
{
	return ftrace_tags;
}

static noinline
int tracing_mark_write(const char *buf)
{
	trace_puts(buf);
	return 0;
}

void mtk_cam_trace(const char *fmt, ...)
{
	char buf[256];
	va_list args;
	int ret = 0;

	va_start(args, fmt);
	ret = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (ret != -1)
		tracing_mark_write(buf);
}

