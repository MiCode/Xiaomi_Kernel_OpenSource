/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 Mediatek Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM mtk_idle_event

#if !defined(_TRACE_MTK_IDLE_EVENT_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_IDLE_EVENT_H

#include <linux/tracepoint.h>

TRACE_EVENT(rgidle,

	TP_PROTO(
		int cpu,
		int enter
	),

	TP_ARGS(cpu, enter),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, enter)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter = enter;
	),

	TP_printk("cpu = %d %d", (int)__entry->cpu, (int)__entry->enter)
);

TRACE_EVENT(mcdi,

	TP_PROTO(
		int cpu,
		int enter
	),

	TP_ARGS(cpu, enter),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, enter)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter = enter;
	),

	TP_printk("cpu = %d %d",
				(int)__entry->cpu,
				(int)__entry->enter)
);

TRACE_EVENT(sodi,

	TP_PROTO(
		int cpu,
		int enter
	),

	TP_ARGS(cpu, enter),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, enter)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter = enter;
	),

	TP_printk("cpu = %d %d",
				(int)__entry->cpu,
				(int)__entry->enter)
);

TRACE_EVENT(sodi3,

	TP_PROTO(
		int cpu,
		int enter
	),

	TP_ARGS(cpu, enter),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, enter)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter = enter;
	),

	TP_printk("cpu = %d %d",
				(int)__entry->cpu,
				(int)__entry->enter)
);

TRACE_EVENT(dpidle,

	TP_PROTO(
		int cpu,
		int enter
	),

	TP_ARGS(cpu, enter),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, enter)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter = enter;
	),

	TP_printk("cpu = %d %d",
				(int)__entry->cpu,
				(int)__entry->enter)
);

TRACE_EVENT(check_anycore,

	TP_PROTO(
		int cpu,
		int enter,
		int select_state
	),

	TP_ARGS(cpu, enter, select_state),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, enter)
		__field(int, select_state)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter = enter;
		__entry->select_state = select_state;
	),

	TP_printk("cpu = %d %d %d",
				(int)__entry->cpu,
				(int)__entry->enter,
				(int)__entry->select_state)
);

TRACE_EVENT(mcdi_cpu_cluster_stat,

	TP_PROTO(
		int cpu,
		unsigned int on_off_stat,
		unsigned int check_mask
	),

	TP_ARGS(cpu, on_off_stat, check_mask),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, on_off_stat)
		__field(unsigned int, check_mask)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->on_off_stat = on_off_stat;
		__entry->check_mask = check_mask;
	),

	TP_printk("cpu = %d %x %x",
				(int)__entry->cpu,
				(unsigned int)__entry->on_off_stat,
				(unsigned int)__entry->check_mask
	)
);

TRACE_EVENT(mcdi_multi_core,

	TP_PROTO(
		int cpu,
		unsigned int on_off_stat,
		unsigned int check_mask
	),

	TP_ARGS(cpu, on_off_stat, check_mask),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(unsigned int, on_off_stat)
		__field(unsigned int, check_mask)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->on_off_stat = on_off_stat;
		__entry->check_mask = check_mask;
	),

	TP_printk("cpu = %d %x %x",
				(int)__entry->cpu,
				(unsigned int)__entry->on_off_stat,
				(unsigned int)__entry->check_mask
	)
);

TRACE_EVENT(any_core_residency,

	TP_PROTO(
		int cpu
	),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("cpu = %d", (int)__entry->cpu)
);

TRACE_EVENT(mtk_idle_select,

	TP_PROTO(
		int cpu,
		int mtk_idle_state
	),

	TP_ARGS(cpu, mtk_idle_state),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, mtk_idle_state)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->mtk_idle_state = mtk_idle_state;
	),

	TP_printk("cpu = %d %d",
				(int)__entry->cpu,
				(int)__entry->mtk_idle_state
	)
);

TRACE_EVENT(mcdi_task_pause,
	TP_PROTO(
		int cpu,
		int enter
	),

	TP_ARGS(cpu, enter),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, enter)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->enter = enter;
	),

	TP_printk("cpu = %d %d",
				(int)__entry->cpu,
				(int)__entry->enter)
);

TRACE_EVENT(mtk_menu,
	TP_PROTO(
		int cpu,
		int ratio,
		int dur
	),

	TP_ARGS(cpu, ratio, dur),

	TP_STRUCT__entry(
		__field(int, cpu)
		__field(int, ratio)
		__field(int, dur)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->ratio = ratio;
		__entry->dur = dur;
	),

	TP_printk("cpu = %d %d %d",
				(int)__entry->cpu,
				(int)__entry->ratio,
				(int)__entry->dur)
);

TRACE_EVENT(all_cpu_idle,

	TP_PROTO(
		int enter
	),

	TP_ARGS(enter),

	TP_STRUCT__entry(
		__field(int, enter)
	),

	TP_fast_assign(
		__entry->enter = enter;
	),

	TP_printk("enter = %d", (int)__entry->enter)
);

TRACE_EVENT(idle_cg,

	TP_PROTO(
		int id,
		int enable
	),

	TP_ARGS(id, enable),

	TP_STRUCT__entry(
		__field(int, id)
		__field(int, enable)
	),

	TP_fast_assign(
		__entry->id = id;
		__entry->enable = enable;
	),

	TP_printk("id = %d %d", (int)__entry->id, (int)__entry->enable)
);


#endif /* _TRACE_MTK_IDLE_EVENT_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
