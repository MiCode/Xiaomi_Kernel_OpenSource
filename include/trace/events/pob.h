/*
 * Copyright (C) 2018 MediaTek Inc.
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
#define TRACE_SYSTEM pob

#if !defined(_TRACE_POB_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_POB_H

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(pob_log_template,

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

DEFINE_EVENT(pob_log_template, pob_log,
	     TP_PROTO(char *log),
	     TP_ARGS(log));

#endif /* _TRACE_POB_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
