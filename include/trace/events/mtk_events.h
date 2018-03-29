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

	TP_PROTO(unsigned int cpu_id, unsigned int state, unsigned long long ts),

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
	    TP_STRUCT__entry(__field(unsigned int, mask)
			     __field(unsigned int, budget)
			     __field(unsigned int, root)
			     __string(limits, ppm_limits)
	    ),
	    TP_fast_assign(__entry->mask = policy_mask;
			   __entry->budget = power_budget;
			   __entry->root = root_cluster;
			   __assign_str(limits, ppm_limits);),
	    TP_printk("(0x%x)(%d)(%d)%s", __entry->mask, __entry->budget,
		      __entry->root, __get_str(limits))
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
	    TP_ARGS(actionID, online, cur_load, cur_tlp, cur_iowait, hvytsk, limit, base,
		    up_avg, down_avg, tlp_avg, rush_cnt, target),
	    TP_STRUCT__entry(__field(unsigned int, actionID)
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
	    TP_fast_assign(__entry->actionID = actionID;
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
	    TP_printk("(0x%X)%s action end (%u)(%u)(%u) %s %s%s (%u)(%u)(%u)(%u) %s",
		      __entry->actionID, __get_str(online), __entry->cur_load, __entry->cur_tlp,
		      __entry->cur_iowait, __get_str(hvytsk), __get_str(limit), __get_str(base),
		      __entry->up_avg, __entry->down_avg, __entry->tlp_avg, __entry->rush_cnt,
		      __get_str(target))
    );



#endif				/* _TRACE_MTK_EVENTS_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
