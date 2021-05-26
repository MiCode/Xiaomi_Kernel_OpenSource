/*
 * Copyright (C) 2020 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM ccci

#if !defined(_TRACE_EVENT_CCCI_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_CCCI_H

#include <linux/tracepoint.h>


TRACE_EVENT(ccci_event,
	TP_PROTO(char *string, char *sub_string, unsigned int sub_type, unsigned int resv),
	TP_ARGS(string, sub_string, sub_type, resv),
	TP_STRUCT__entry(__string(str0, string)
		__string(str1, sub_string)
		__field(unsigned int, sub_type)
		__field(unsigned int, resv)
	),
	TP_fast_assign(__assign_str(str0, string);
	__assign_str(str1, sub_string);
	__entry->sub_type = sub_type;
	__entry->resv = resv;
	),
	TP_printk("%s:%s	%u	%u",
	__get_str(str0), __get_str(str1),
	__entry->sub_type == 0, __entry->resv)
	);
#endif

/***** NOTICE! The #if protection ends here. *****/

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
/*
 * TRACE_INCLUDE_FILE is not needed if the filename and TRACE_SYSTEM are equal
 */
#define TRACE_INCLUDE_FILE ccci_events
#include <trace/define_trace.h>
