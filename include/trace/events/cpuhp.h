/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM cpuhp

#if !defined(_TRACE_CPUHP_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_CPUHP_H

#include <linux/tracepoint.h>
#include <linux/sched/clock.h>

TRACE_EVENT(cpuhp_enter,

	TP_PROTO(unsigned int cpu,
		 int target,
		 int idx,
		 int (*fun)(unsigned int)),

	TP_ARGS(cpu, target, idx, fun),

	TP_STRUCT__entry(
		__field( unsigned int,	cpu		)
		__field( int,		target		)
		__field( int,		idx		)
		__field( void *,	fun		)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->target	= target;
		__entry->idx	= idx;
		__entry->fun	= fun;
	),

	TP_printk("cpu: %04u target: %3d step: %3d (%pf)",
		  __entry->cpu, __entry->target, __entry->idx, __entry->fun)
);

TRACE_EVENT(cpuhp_multi_enter,

	TP_PROTO(unsigned int cpu,
		 int target,
		 int idx,
		 int (*fun)(unsigned int, struct hlist_node *),
		 struct hlist_node *node),

	TP_ARGS(cpu, target, idx, fun, node),

	TP_STRUCT__entry(
		__field( unsigned int,	cpu		)
		__field( int,		target		)
		__field( int,		idx		)
		__field( void *,	fun		)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->target	= target;
		__entry->idx	= idx;
		__entry->fun	= fun;
	),

	TP_printk("cpu: %04u target: %3d step: %3d (%pf)",
		  __entry->cpu, __entry->target, __entry->idx, __entry->fun)
);

TRACE_EVENT(cpuhp_exit,

	TP_PROTO(unsigned int cpu,
		 int state,
		 int idx,
		 int ret),

	TP_ARGS(cpu, state, idx, ret),

	TP_STRUCT__entry(
		__field( unsigned int,	cpu		)
		__field( int,		state		)
		__field( int,		idx		)
		__field( int,		ret		)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->state	= state;
		__entry->idx	= idx;
		__entry->ret	= ret;
	),

	TP_printk(" cpu: %04u  state: %3d step: %3d ret: %d",
		  __entry->cpu, __entry->state, __entry->idx,  __entry->ret)
);

TRACE_EVENT(cpuhp_latency,

	TP_PROTO(unsigned int cpu, unsigned int state,
		 u64 start_time,  int ret),

	TP_ARGS(cpu, state, start_time, ret),

	TP_STRUCT__entry(
		__field(unsigned int,	cpu)
		__field(unsigned int,	state)
		__field(u64,		time)
		__field(int,		ret)
	),

	TP_fast_assign(
		__entry->cpu	= cpu;
		__entry->state	= state;
		__entry->time	= div64_u64(sched_clock() - start_time, 1000);
		__entry->ret	= ret;
	),

	TP_printk(" cpu:%d state:%s latency:%llu USEC ret: %d",
		__entry->cpu, __entry->state ? "online" : "offline",
		__entry->time, __entry->ret)
);



#endif

/* This part must be outside protection */
#include <trace/define_trace.h>
