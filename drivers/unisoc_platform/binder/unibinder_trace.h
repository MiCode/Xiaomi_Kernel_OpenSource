/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright 2023 Unisoc(Shanghai) Technologies Co.Ltd
 * This program is free software; you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation; version 2.
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM unisoc_binder

#if !defined(_UNIBINDER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _UNIBINDER_TRACE_H

#include <linux/tracepoint.h>

TRACE_EVENT(unibinder_set_feature,
	TP_PROTO(int proc, int thread, int flags),
	TP_ARGS(proc, thread, flags),

	TP_STRUCT__entry(
		__field(int, proc)
		__field(int, thread)
		__field(int, flags)
	),
	TP_fast_assign(
		__entry->proc = proc;
		__entry->thread = thread;
		__entry->flags = flags;
	),
	TP_printk("thread %d:%d feature flags set to %d", __entry->proc,
		  __entry->thread, __entry->flags)
);

TRACE_EVENT(unibinder_set_priority,
	TP_PROTO(int proc, int thread, unsigned int old_prio, unsigned int new_prio,
			 int set_from, int do_set, int orig_prio_state),
	TP_ARGS(proc, thread, old_prio, new_prio, set_from, do_set, orig_prio_state),

	TP_STRUCT__entry(
		__field(int, proc)
		__field(int, thread)
		__field(int, old_prio)
		__field(int, new_prio)
		__field(int, set_from)
		__field(int, do_set)
		__field(int, orig_prio_state)
	),
	TP_fast_assign(
		__entry->proc			= proc;
		__entry->thread			= thread;
		__entry->old_prio		= old_prio;
		__entry->new_prio		= new_prio;
		__entry->set_from		= set_from;
		__entry->do_set			= do_set;
		__entry->orig_prio_state	= orig_prio_state;
	),
	TP_printk("proc=%d thread=%d old=%d => new=%d set_from=%d do_set=%d orig_prio_state=%d",
		  __entry->proc, __entry->thread, __entry->old_prio, __entry->new_prio,
		  __entry->set_from, __entry->do_set, __entry->orig_prio_state)
);

#endif /* _UNIBINDER_TRACE_H */

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE unibinder_trace
#include <trace/define_trace.h>
