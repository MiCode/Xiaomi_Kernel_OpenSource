/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM serial

#if !defined(_TRACE_SERIAL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_SERIAL_TRACE_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(serial_info_str,

	TP_PROTO(const char *func, char *name),

	TP_ARGS(func, name),

	TP_STRUCT__entry(
		__string(func, func)
		__string(name, name)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__assign_str(name, name);
	),

	TP_printk("%s:%s", __get_str(func), __get_str(name))

);

TRACE_EVENT(serial_info_str1,

	TP_PROTO(const char *func, char *name, char *strval),

	TP_ARGS(func, name, strval),

	TP_STRUCT__entry(
		__string(func, func)
		__string(name, name)
		__string(strval, strval)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__assign_str(name, name);
		__assign_str(strval, strval);
	),

	TP_printk("%s:%s,%s", __get_str(func), __get_str(name), __get_str(strval))
);

TRACE_EVENT(serial_info_numeric1,

	TP_PROTO(const char *func, char *name, unsigned int val),

	TP_ARGS(func, name, val),

	TP_STRUCT__entry(
		__string(func, func)
		__string(name, name)
		__field(unsigned int, val)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__assign_str(name, name);
		__entry->val = val;
	),

	TP_printk("%s:%s: %d", __get_str(func), __get_str(name),
		(unsigned int)__entry->val)

);

TRACE_EVENT(serial_info_numeric2,

TP_PROTO(const char *func, char *name1, unsigned int val1,
				char *name2, unsigned int val2),

	TP_ARGS(func, name1, val1, name2, val2),

	TP_STRUCT__entry(
		__string(func, func)
		__string(name1, name1)
		__field(unsigned int, val1)
		__string(name2, name2)
		__field(unsigned int, val2)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__assign_str(name1, name1);
		__entry->val1 = val1;
		__assign_str(name2, name2);
		__entry->val2 = val2;
	),

	TP_printk("%s: %s:%d %s:%d", __get_str(func), __get_str(name1),
		(unsigned int)__entry->val1, __get_str(name2), (unsigned int)__entry->val2)

);

TRACE_EVENT(serial_info_hex1,

	TP_PROTO(const char *func, char *name, unsigned int val),

	TP_ARGS(func, name, val),

	TP_STRUCT__entry(
		__string(func, func)
		__string(name, name)
		__field(unsigned int, val)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__assign_str(name, name);
		__entry->val = val;
	),

	TP_printk("%s:%s: 0x%x", __get_str(func), __get_str(name),
		(unsigned int)__entry->val)

);

TRACE_EVENT(serial_info_hex2,

TP_PROTO(const char *func, char *name1, unsigned int val1,
				char *name2, unsigned int val2),

	TP_ARGS(func, name1, val1, name2, val2),

	TP_STRUCT__entry(
		__string(func, func)
		__string(name1, name1)
		__field(unsigned int, val1)
		__string(name2, name2)
		__field(unsigned int, val2)
	),

	TP_fast_assign(
		__assign_str(func, func);
		__assign_str(name1, name1);
		__entry->val1 = val1;
		__assign_str(name2, name2);
		__entry->val2 = val2;
	),

	TP_printk("%s: %s:0x%x %s:0x%x", __get_str(func), __get_str(name1),
		(unsigned int)__entry->val1, __get_str(name2), (unsigned int)__entry->val2)

);

#endif /* _TRACE_SERIAL_TRACE_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE serial_trace
#include <trace/define_trace.h>
