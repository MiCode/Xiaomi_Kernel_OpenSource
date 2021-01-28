/*
 * Copyright (C) 2015 MediaTek Inc.
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
#define TRACE_SYSTEM mtk_events

#if !defined(_TRACE_MTK_EVENTS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MTK_EVENTS_H

#include <linux/tracepoint.h>

TRACE_EVENT(tracing_on,

	TP_PROTO(int on, unsigned long ip),

	TP_ARGS(on, ip),

	TP_STRUCT__entry(
		__field(int, on)
		__field(unsigned long, ip)
	),

	TP_fast_assign(
		__entry->on = on;
		__entry->ip = ip;
	),

	TP_printk("ftrace is %s caller=%pf",
		__entry->on ? "enabled" : "disabled",
		(void *)__entry->ip)
);

TRACE_EVENT(cpu_hotplug,

	TP_PROTO(unsigned int cpu_id, unsigned int state,
		unsigned long long ts),

	TP_ARGS(cpu_id, state, ts),

	TP_STRUCT__entry(
		__field(u32, cpu_id)
		__field(u32, state)
		__field(u64, ts)
	),

	TP_fast_assign(
		__entry->cpu_id = cpu_id;
		__entry->state = state;
		__entry->ts = ts;
	),

	TP_printk("cpu=%03lu state=%s last_%s_ts=%llu",
		(unsigned long)__entry->cpu_id,
		__entry->state ? "online" : "offline",
		__entry->state ? "offline" : "online", __entry->ts
	)
);

TRACE_EVENT(irq_entry,

	TP_PROTO(int irqnr, const char *irqname),

	TP_ARGS(irqnr, irqname),

	TP_STRUCT__entry(
		__field(int, irq)
		__string(name, irqname)
	),

	TP_fast_assign(
		__entry->irq = irqnr;
		__assign_str(name, irqname);
	),

	TP_printk("irq=%d name=%s", __entry->irq, __get_str(name))
);

TRACE_EVENT(irq_exit,

	TP_PROTO(int irqnr),

	TP_ARGS(irqnr),

	TP_STRUCT__entry(
		__field(int, irq)
	),

	TP_fast_assign(
		__entry->irq = irqnr;
	),

	TP_printk("irq=%d", __entry->irq)
);

TRACE_EVENT(gpu_freq,

	TP_PROTO(unsigned int frequency),

	TP_ARGS(frequency),

	TP_STRUCT__entry(
		__field(u32, frequency)
	),

	TP_fast_assign(
		__entry->frequency = frequency;
	),

	TP_printk("frequency=%lu", (unsigned long)__entry->frequency)
);

TRACE_EVENT(ppm_update,

	TP_PROTO(unsigned int policy_mask,
		 unsigned int power_budget,
		 unsigned int root_cluster,
		 char *ppm_limits),

	TP_ARGS(policy_mask, power_budget, root_cluster, ppm_limits),

	TP_STRUCT__entry(
		__field(unsigned int, mask)
		__field(unsigned int, budget)
		__field(unsigned int, root)
		__string(limits, ppm_limits)
	),

	TP_fast_assign(
		__entry->mask = policy_mask;
		__entry->budget = power_budget;
		__entry->root = root_cluster;
		__assign_str(limits, ppm_limits);
	),

	TP_printk("(0x%x)(%d)(%d)%s", __entry->mask, __entry->budget,
		__entry->root, __get_str(limits))
);

TRACE_EVENT(ppm_hica,

	TP_PROTO(const char *cur_state,
		 const char *target_state,
		 long usage,
		 long capacity,
		 int big_tsk_L,
		 int big_tsk_B,
		 int heavy_tsk,
		 int freq,
		 int result),

	TP_ARGS(cur_state, target_state, usage, capacity, big_tsk_L,
		big_tsk_B, heavy_tsk, freq, result),

	TP_STRUCT__entry(
		__string(cur, cur_state)
		__string(target, target_state)
		__field(long, usage)
		__field(long, capacity)
		__field(int, big_tsk_L)
		__field(int, big_tsk_B)
		__field(int, heavy_tsk)
		__field(int, freq)
		__field(int, result)
	),

	TP_fast_assign(
		__assign_str(cur, cur_state);
		__assign_str(target, target_state);
		__entry->usage = usage;
		__entry->capacity = capacity;
		__entry->big_tsk_L = big_tsk_L;
		__entry->big_tsk_B = big_tsk_B;
		__entry->heavy_tsk = heavy_tsk;
		__entry->freq = freq;
		__entry->result = result;
	),

	TP_printk
("%s->%s(%s), usage=%ld, capacity=%ld, big_tsk=%d/%d, heavy_tsk=%d, freq=%d",
		__get_str(cur), __get_str(target),
		(__entry->result) ? "O" : "X",
		__entry->usage, __entry->capacity,
		__entry->big_tsk_L, __entry->big_tsk_B,
		__entry->heavy_tsk, __entry->freq)
);

TRACE_EVENT(ppm_overutil_update,

	TP_PROTO(int overutil_l,
		 int overutil_h),

	TP_ARGS(overutil_l, overutil_h),

	TP_STRUCT__entry(
		__field(int, ou_l)
		__field(int, ou_h)
	),

	TP_fast_assign(
		__entry->ou_l = overutil_l;
		__entry->ou_h = overutil_h;
	),

	TP_printk("(%d)(%d)", __entry->ou_l, __entry->ou_h)
);

TRACE_EVENT(ppm_limit_callback_update,

	TP_PROTO(int i,
		 int has_advise_core,
		 int min_cpu_core,
		 int max_cpu_core),

	TP_ARGS(i, has_advise_core, min_cpu_core, max_cpu_core),

	TP_STRUCT__entry(
		__field(int, i)
		__field(int, has_advise_core)
		__field(int, min_cpu_core)
		__field(int, max_cpu_core)
	),

	TP_fast_assign(
		__entry->i = i;
		__entry->has_advise_core = has_advise_core;
		__entry->min_cpu_core = min_cpu_core;
		__entry->max_cpu_core = max_cpu_core;
	),

	TP_printk
	("ppm_limit_callback -> cluster%d: has_advise_core = %d, [%d, %d]\n",
		__entry->i, __entry->has_advise_core,
		__entry->min_cpu_core, __entry->max_cpu_core)
);

TRACE_EVENT(hps_update,

	TP_PROTO(unsigned int actionID,
		 char *online,
		 unsigned int cur_load,
		 unsigned int cur_tlp,
		 unsigned int cur_iowait,
		 char *hvytsk,
		 char *limit,
		 char *base,
		 unsigned int up_avg,
		 unsigned int down_avg,
		 unsigned int tlp_avg,
		 unsigned int rush_cnt,
		 char *target),

	TP_ARGS(actionID, online, cur_load, cur_tlp, cur_iowait, hvytsk,
		limit, base, up_avg, down_avg, tlp_avg, rush_cnt, target),

	TP_STRUCT__entry(
		__field(unsigned int, actionID)
		__string(online, online)
		__field(unsigned int, cur_load)
		__field(unsigned int, cur_tlp)
		__field(unsigned int, cur_iowait)
		__string(hvytsk, hvytsk)
		__string(limit, limit)
		__string(base, base)
		__field(unsigned int, up_avg)
		__field(unsigned int, down_avg)
		__field(unsigned int, tlp_avg)
		__field(unsigned int, rush_cnt)
		__string(target, target)
	),

	TP_fast_assign(
		__entry->actionID = actionID;
		__assign_str(online, online);
		__entry->cur_load = cur_load;
		__entry->cur_tlp = cur_tlp;
		__entry->cur_iowait = cur_iowait;
		__assign_str(hvytsk, hvytsk);
		__assign_str(limit, limit);
		__assign_str(base, base);
		__entry->up_avg = up_avg;
		__entry->down_avg = down_avg;
		__entry->tlp_avg = tlp_avg;
		__entry->rush_cnt = rush_cnt;
		__assign_str(target, target);),

	TP_printk
	("(0x%X)%s action end (%u)(%u)(%u) %s %s%s (%u)(%u)(%u)(%u) %s",
		__entry->actionID, __get_str(online), __entry->cur_load,
		__entry->cur_tlp, __entry->cur_iowait, __get_str(hvytsk),
		__get_str(limit), __get_str(base), __entry->up_avg,
		__entry->down_avg, __entry->tlp_avg, __entry->rush_cnt,
		__get_str(target))
);

#if 0
TRACE_EVENT(sched_update,

	TP_PROTO(
		unsigned int cpu_id,
		unsigned int sched_info_cpu0,
		unsigned int sched_info_cpu1,
		unsigned int sched_info_cpu2,
		unsigned int sched_info_cpu3
	),

	TP_ARGS(cpu_id, sched_info_cpu0, sched_info_cpu1,
		sched_info_cpu2, sched_info_cpu3),

	TP_STRUCT__entry(
		__field(unsigned int, root_cpu)
		__field(unsigned int, sched_info_0)
		__field(unsigned int, sched_info_1)
		__field(unsigned int, sched_info_2)
		__field(unsigned int, sched_info_3)
	),

	TP_fast_assign(
		__entry->root_cpu = cpu_id;
		__entry->sched_info_0 = sched_info_cpu0;
		__entry->sched_info_1 = sched_info_cpu1;
		__entry->sched_info_2 = sched_info_cpu2;
		__entry->sched_info_3 = sched_info_cpu3;
	),

	TP_printk("(%d)(0x%x)(0x%x)(0x%x)(0x%x)",
	__entry->root_cpu, __entry->sched_info_0,
	__entry->sched_info_1, __entry->sched_info_2, __entry->sched_info_3)
);
#else
TRACE_EVENT(sched_update,

	TP_PROTO(
		unsigned int cluster,
		unsigned int sched_info_cluster
	),

	TP_ARGS(cluster, sched_info_cluster),

	TP_STRUCT__entry(
		__field(unsigned int, cluster_id)
		__field(unsigned int, sched_info)
	),

	TP_fast_assign(
		__entry->cluster_id = cluster;
		__entry->sched_info = sched_info_cluster;
	),

	TP_printk("(%d)(0x%x)", __entry->cluster_id, __entry->sched_info)
);
#endif

TRACE_EVENT(sspm_ipi,

	TP_PROTO(
		int start,
		int ipi_id,
		int ipi_opt
	),

	TP_ARGS(start, ipi_id, ipi_opt),

	TP_STRUCT__entry(
		__field(int, start)
		__field(int, ipi_id)
		__field(int, ipi_opt)
	),

	TP_fast_assign(
		__entry->start = start;
		__entry->ipi_id = ipi_id;
		__entry->ipi_opt = ipi_opt;
	),

	TP_printk("start=%d, id=%d, opt=%d",
	__entry->start, __entry->ipi_id, __entry->ipi_opt)
);

#endif /* _TRACE_MTK_EVENTS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
