/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM fpsgo

#if !defined(_TRACE_FPSGO_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_FPSGO_H

#include <linux/tracepoint.h>
#include <linux/ktime.h>

#ifdef CREATE_TRACE_POINTS
static inline unsigned long long calc_dur(unsigned long long *start,
					  unsigned long long *end)
{
	if (*end < *start)
		return 0ULL;
	return (*end - *start);
}
#endif

TRACE_EVENT(xgf_intvl,

	TP_PROTO(char *type, void *obj, unsigned long long *start,
		 unsigned long long *end),

	TP_ARGS(type, obj, start, end),

	TP_STRUCT__entry(
		__string(string,		type)
		__field(void*,			obj)
		__field(unsigned long long,	start)
		__field(unsigned long long,	end)
		__field(unsigned long long,	dur)
	),

	TP_fast_assign(
		__assign_str(string, type);
		__entry->obj		= obj;
		__entry->start		= *start;
		__entry->end		= *end;
		__entry->dur		= calc_dur(start, end);
	),

	TP_printk("%s %p %llu ==> %llu intvl=%lld", __get_str(string),
			__entry->obj, __entry->start, __entry->end,
			__entry->dur)
);

DECLARE_EVENT_CLASS(fpsgo_log_template,

	TP_PROTO(char *log),

	TP_ARGS(log),

	TP_STRUCT__entry(
		__string(msg, log)
	),

	TP_fast_assign(
		__assign_str(msg, log);
	),

	TP_printk("%s", __get_str(msg))
);

DEFINE_EVENT(fpsgo_log_template, xgf_log,
	     TP_PROTO(char *log),
	     TP_ARGS(log));

DEFINE_EVENT(fpsgo_log_template, minitop_log,
	     TP_PROTO(char *log),
	     TP_ARGS(log));

DEFINE_EVENT(fpsgo_log_template, fpsgo_main_log,
	     TP_PROTO(char *log),
	     TP_ARGS(log));

DEFINE_EVENT(fpsgo_log_template, eara_thrm_log,
	     TP_PROTO(char *log),
	     TP_ARGS(log));


#endif /* _TRACE_FPSGO_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
