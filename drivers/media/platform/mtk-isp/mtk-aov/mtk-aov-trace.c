// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Johnson-CH Chiu <Johnson-CH.chiu@mediatek.com>
 *
 */

#include "mtk-aov-config.h"
#include "mtk-aov-trace.h"

int aov_trace;
module_param(aov_trace, int, 0644);

static noinline int trace_mark_write(const char *buf)
{
#if IS_ENABLED(CONFIG_MTK_FTRACER)
	trace_puts(buf);
#endif  // IS_ENABLED(CONFIG_MTK_FTRACER)

	return 0;
}

void __aov_trace_write(const char *fmt, ...)
{
	char buf[AVO_MAX_TRACE_SIZE];
	va_list args;
	int len;

	memset(buf, ' ', sizeof(buf));
	va_start(args, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (unlikely(len < 0))
		return;
	else if (unlikely(len == AVO_MAX_TRACE_SIZE))
		buf[AVO_MAX_TRACE_SIZE - 1] = '\0';

	trace_mark_write(buf);
}

bool is_aov_trace_enable(void)
{
#if IS_ENABLED(CONFIG_MTK_FTRACER)
		return aov_trace;
#else
		return false;
#endif  // IS_ENABLED(CONFIG_MTK_FTRACER)
}
