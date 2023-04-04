/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#undef TRACE_SYSTEM
#define TRACE_SYSTEM met_touch

#if !defined(__TRACE_MET_FTRACE_TOUCH_H__) || defined(TRACE_HEADER_MULTI_READ)
#define __TRACE_MET_FTRACE_TOUCH_H__

#include <linux/tracepoint.h>
/*
 * Tracepoint for MET_touch
 */
TRACE_EVENT(MET_touch,

		TP_PROTO(char *touch_type, long tsec, long tusec,
			char *mode, int value),

		TP_ARGS(touch_type, tsec, tusec, mode, value),

		TP_STRUCT__entry(
			__array(char, _touch_type, 16)
			__field(long, _tsec)
			__field(long, _tusec)
			__array(char, _mode, 16)
			__field(int, _value)
			),

		TP_fast_assign(
			strlcpy(__entry->_touch_type, touch_type, 16);
			__entry->_tsec = tsec;
			__entry->_tusec = tusec;
			strlcpy(__entry->_mode, mode, 16);
			__entry->_value = value;
			),

		TP_printk("%s,%ld.%06ld,%s,%x",
				__entry->_touch_type,
				__entry->_tsec,
				__entry->_tusec,
				__entry->_mode,
				__entry->_value
				)
		);

#endif /* __TRACE_MET_FTRACE_TOUCH_H__ */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#undef linux
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE met_ftrace_touch
#include <trace/define_trace.h>
