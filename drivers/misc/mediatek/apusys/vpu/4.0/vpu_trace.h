// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "apusys_trace.h"

/* Performance Measure */
#ifdef CONFIG_FTRACE
#include <linux/trace_events.h>
extern u8 cfg_apusys_trace;

#ifdef vpu_trace_begin
#undef vpu_trace_begin
#endif

#define vpu_trace_begin(format, args...) \
	{ \
		char buf[256]; \
		int len; \
		if (cfg_apusys_trace) {\
			len = snprintf(buf, sizeof(buf), \
				       format, args); \
			if (len >= 0) \
				trace_async_tag(1, buf); \
		} \
	}

#ifdef vpu_trace_end
#undef vpu_trace_end
#endif
#define vpu_trace_end(format, args...) \
	{ \
		char buf[256]; \
		int len; \
		if (cfg_apusys_trace) { \
			len = snprintf(buf, sizeof(buf), \
				       format, args); \
			if (len >= 0) \
				trace_async_tag(0, buf); \
		} \
	}
#else
#define vpu_trace_begin(...)
#define vpu_trace_end(...)
#endif /* CONFIG_FTRACE */
