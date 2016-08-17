/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM input_cfboost

#if !defined(_TRACE_INPUT_CFBOOST_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_INPUT_CFBOOST_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

TRACE_EVENT(input_cfboost_params,
	TP_PROTO(const char *name, unsigned int freq, unsigned long time),
	TP_ARGS(name, freq, time),
	TP_STRUCT__entry(
		__field(const char *, name)
		__field(unsigned int, freq)
		__field(unsigned long, time)
	),
	TP_fast_assign(
		__entry->name = name;
		__entry->freq = freq;
		__entry->time = time;
	),
	TP_printk("name=%s freq=%u time=%lu",
	  __entry->name, __entry->freq, __entry->time)
);

TRACE_EVENT(input_cfboost_event,
	TP_PROTO(const char *name, unsigned int type,
		unsigned int code, int value),
	TP_ARGS(name, type, code, value),
	TP_STRUCT__entry(
		__field(const char *, name)
		__field(unsigned int, type)
		__field(unsigned int, code)
		__field(int, value)
	),
	TP_fast_assign(
		__entry->name = name;
		__entry->type = type;
		__entry->code = code;
		__entry->value = value;
	),
	TP_printk("name=%s type=%u code=%u value=%d",
		__entry->name, __entry->type, __entry->code, __entry->value)
);

#endif /*  _TRACE_INPUT_CFBOOST_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
