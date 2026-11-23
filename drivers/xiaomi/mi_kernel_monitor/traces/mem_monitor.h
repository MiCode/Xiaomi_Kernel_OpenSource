/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM mem_monitor

#if !defined(_TRACE_MEM_MONITOR_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MEM_MONITOR_H
#include <linux/tracepoint.h>
#include "internal.h"

TRACE_EVENT(mem_failure_prop_info,
	TP_PROTO(struct task_struct *task, unsigned short prop_depth),
	TP_ARGS(task, prop_depth),

	TP_STRUCT__entry(
		__array(char, comm, TASK_COMM_LEN)
		__field(int, pid)
		__field(unsigned short, prop_depth)
	),
	TP_fast_assign(
		memcpy(__entry->comm, task->comm, TASK_COMM_LEN);
		__entry->pid = task->pid;
		__entry->prop_depth = prop_depth;
	),

	TP_printk("comm=%s pid=%d prop_depth=%u",
		__entry->comm, __entry->pid, __entry->prop_depth)
);

TRACE_EVENT(cma_alloc_delay,
	TP_PROTO(const char *name, u64 delay, int count, int ret),
	TP_ARGS(name, delay, count, ret),

	TP_STRUCT__entry(
		__string(name, name)
		__field(u64, delay)
		__field(int, count)
		__field(int, ret)
	),
	TP_fast_assign(
		__assign_str(name);
		__entry->delay = delay;
		__entry->count = count;
		__entry->ret = ret;
	),

	TP_printk("name=%s delay=%llu count=%d ret=%d",
		__get_str(name), __entry->delay, __entry->count, __entry->ret)
);

TRACE_EVENT(direct_reclaim_delay,
	TP_PROTO(int order, u64 delay, unsigned long nr_reclaimed,
		u64 accumulate_delay, int accumulate_cnt, u64 window_size),
	TP_ARGS(order, delay, nr_reclaimed, accumulate_delay, accumulate_cnt,
		window_size),

	TP_STRUCT__entry(
		__field(int, order)
		__field(u64, delay)
		__field(unsigned long, nr_reclaimed)
		__field(u64, accumulate_delay)
		__field(int, accumulate_cnt)
		__field(u64, window_size)
	),
	TP_fast_assign(
		__entry->order = order;
		__entry->delay = delay;
		__entry->nr_reclaimed = nr_reclaimed;
		__entry->accumulate_delay = accumulate_delay;
		__entry->accumulate_cnt = accumulate_cnt;
		__entry->window_size = window_size;
	),

	TP_printk("order=%d delay=%llu nr_reclaimed=%lu accumulate_delay=%llu "
		"accumulate_cnt=%d window_size=%llu",
		__entry->order, __entry->delay, __entry->nr_reclaimed,
		__entry->accumulate_delay, __entry->accumulate_cnt,
		__entry->window_size)
);

TRACE_EVENT(try_to_free_pages_delay,
	TP_PROTO(int order, u64 delay),
	TP_ARGS(order, delay),

	TP_STRUCT__entry(
		__field(int, order)
		__field(u64, delay)
	),
	TP_fast_assign(
		__entry->order = order;
		__entry->delay = delay;
	),

	TP_printk("order=%d delay=%llu", __entry->order, __entry->delay)
);

TRACE_EVENT(migration_entry_wait_delay,
	TP_PROTO(u64 delay),
	TP_ARGS(delay),

	TP_STRUCT__entry(
		__field(u64, delay)
	),
	TP_fast_assign(
		__entry->delay = delay;
	),

	TP_printk("delay=%llu", __entry->delay)
);

TRACE_EVENT(try_to_compact_pages_delay,
	TP_PROTO(int order, u64 delay, int compact_result),
	TP_ARGS(order, delay, compact_result),

	TP_STRUCT__entry(
		__field(int, order)
		__field(u64, delay)
		__field(int, compact_result)
	),
	TP_fast_assign(
		__entry->order = order;
		__entry->delay = delay;
		__entry->compact_result = compact_result;
	),

	TP_printk("order=%d delay=%llu result=%d",
		__entry->order, __entry->delay, __entry->compact_result)
);

TRACE_EVENT(swap_read_folio_delay,
	TP_PROTO(u64 delay, bool io_wait),
	TP_ARGS(delay, io_wait),

	TP_STRUCT__entry(
		__field(u64, delay)
		__field(bool, io_wait)
	),
	TP_fast_assign(
		__entry->delay = delay;
		__entry->io_wait = io_wait;
	),

	TP_printk("delay=%llu io_wait=%d", __entry->delay, __entry->io_wait)
);

TRACE_EVENT(swap_write_folio_delay,
	TP_PROTO(u64 delay),
	TP_ARGS(delay),

	TP_STRUCT__entry(
		__field(u64, delay)
	),
	TP_fast_assign(
		__entry->delay = delay;
	),

	TP_printk("delay=%llu", __entry->delay)
);

#endif /* _TRACE_MEM_MONITOR_H */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH ../../drivers/xiaomi/mi_kernel_monitor/traces

/* This part must be outside protection */
#include <trace/define_trace.h>
