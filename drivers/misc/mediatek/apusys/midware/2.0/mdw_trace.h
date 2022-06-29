/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/string.h>
#include <linux/kernel.h>
#include "apusys_trace.h"

#if IS_ENABLED(CONFIG_FTRACE)
extern u8 cfg_apusys_trace;
#ifdef mdw_trace_begin
#undef mdw_trace_begin
#endif
#define _mdw_trace_begin(format, args...) \
	{ \
		char buf[256]; \
		int len; \
		if (cfg_apusys_trace) { \
			len = snprintf(buf, sizeof(buf), \
				format "%s", args); \
			trace_tag_begin(buf); \
		} \
	}

#define mdw_trace_begin(...) _mdw_trace_begin(__VA_ARGS__, "")
#ifdef mdw_trace_end
#undef mdw_trace_end
#endif
#define mdw_trace_end() \
	{ \
		if (cfg_apusys_trace) { \
			trace_tag_end(); \
		} \
	}
#else
#define mdw_trace_begin(...)
#define mdw_trace_end(...)
#endif /* CONFIG_FTRACE */
