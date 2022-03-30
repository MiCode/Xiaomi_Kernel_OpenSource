// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_cam-trace.h"

#include <linux/module.h>

static int ftrace_tags;
module_param(ftrace_tags, int, 0644);
MODULE_PARM_DESC(ftrace_tags, "enable ftrace tags (bitmask)");

int mtk_cam_trace_enabled_tags(void)
{
	return ftrace_tags;
}

static noinline
int tracing_mark_write(const char *fmt, va_list ap)
{
	ftrace_vprintk(fmt, ap);
	return 0;
}

void mtk_cam_trace(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	tracing_mark_write(fmt, args);
	va_end(args);
}
