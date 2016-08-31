/*
 * include/trace/events/nvsecurity.h
 *
 * Security event logging to ftrace.
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM nvsecurity

#if !defined(_TRACE_NVSECURITY_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_NVSECURITY_H

#include <linux/ktime.h>
#include <linux/tracepoint.h>

extern u32 notrace tegra_read_usec_raw(void);

DECLARE_EVENT_CLASS(usec_profiling,

	TP_PROTO(unsigned long state),

	TP_ARGS(state),

	TP_STRUCT__entry(
		__field(u32, counter)
		__field(u32, state)
	),

	TP_fast_assign(
		__entry->counter = tegra_read_usec_raw();
		__entry->state = state;
	),

	TP_printk("counter=%lu, state=%lu",
		(unsigned long)__entry->counter,
		(unsigned long)__entry->state
	)
);

DEFINE_EVENT(usec_profiling, smc_sleep_cpu,

	TP_PROTO(unsigned long state),

	TP_ARGS(state)
);

DEFINE_EVENT(usec_profiling, smc_sleep_core,

	TP_PROTO(unsigned long state),

	TP_ARGS(state)
);

DEFINE_EVENT(usec_profiling, smc_init_cache,

	TP_PROTO(unsigned long state),

	TP_ARGS(state)
);

DECLARE_EVENT_CLASS(cntr_profiling,

	TP_PROTO(unsigned long counter, unsigned long state),

	TP_ARGS(counter, state),

	TP_STRUCT__entry(
		__field(u32, counter)
		__field(u32, state)
	),

	TP_fast_assign(
		__entry->counter = counter;
		__entry->state = state;
	),

	TP_printk("counter=%lu, state=%lu",
		(unsigned long)__entry->counter,
		(unsigned long)__entry->state
	)
);

DEFINE_EVENT(cntr_profiling, smc_wake,

	TP_PROTO(unsigned long counter, unsigned long state),

	TP_ARGS(counter, state)
);

DEFINE_EVENT(cntr_profiling, secureos_init,

	TP_PROTO(unsigned long counter, unsigned long state),

	TP_ARGS(counter, state)
);

extern u32 notrace tegra_read_cycle(void);

DECLARE_EVENT_CLASS(cycle_profiling,

	TP_PROTO(unsigned long state),

	TP_ARGS(state),

	TP_STRUCT__entry(
		__field(u32, counter)
		__field(u32, state)
	),

	TP_fast_assign(
		__entry->counter = tegra_read_cycle();
		__entry->state = state;
	),

	TP_printk("counter=%lu, state=%lu",
		(unsigned long)__entry->counter,
		(unsigned long)__entry->state
	)
);

DEFINE_EVENT(cycle_profiling, invoke_client_command,

	TP_PROTO(unsigned long state),

	TP_ARGS(state)
);

DEFINE_EVENT(cycle_profiling, smc_generic_call,

	TP_PROTO(unsigned long state),

	TP_ARGS(state)
);

/* This file can get included multiple times, TRACE_HEADER_MULTI_READ at top */
#ifndef _NVSEC_EVENT_AVOID_DOUBLE_DEFINING
#define _NVSEC_EVENT_AVOID_DOUBLE_DEFINING

enum {
	NVSEC_SMC_START,
	NVSEC_SMC_DONE
};

enum {
	NVSEC_INVOKE_CMD_START,
	NVSEC_INVOKE_CMD_DONE
};

enum {
	NVSEC_SUSPEND_EXIT_START,
	NVSEC_SUSPEND_EXIT_DONE
};

#endif
#endif /* _TRACE_NVSECURITY_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
