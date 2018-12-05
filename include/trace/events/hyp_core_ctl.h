/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hyp_core_ctl

#if !defined(_TRACE_HYP_CORE_CTL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HYP_CORE_CTL_H

#include <linux/tracepoint.h>

TRACE_EVENT(hyp_core_ctl_enable,

	TP_PROTO(bool enable),

	TP_ARGS(enable),

	TP_STRUCT__entry(
		__field(bool, enable)
	),

	TP_fast_assign(
		__entry->enable = enable;
	),

	TP_printk("enable=%d", __entry->enable)
);

TRACE_EVENT(hyp_core_ctl_status,

	TP_PROTO(struct hyp_core_ctl_data *hcd, const char *event),

	TP_ARGS(hcd, event),

	TP_STRUCT__entry(
		__string(event, event)
		__array(char, reserve, 32)
		__array(char, reserved, 32)
		__array(char, our_isolated, 32)
		__array(char, online, 32)
		__array(char, isolated, 32)
		__array(char, thermal, 32)
	),

	TP_fast_assign(
		__assign_str(event, event);
		scnprintf(__entry->reserve, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(&hcd->reserve_cpus));
		scnprintf(__entry->reserved, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(&hcd->final_reserved_cpus));
		scnprintf(__entry->our_isolated, sizeof(__entry->reserve),
			  "%*pbl", cpumask_pr_args(&hcd->our_isolated_cpus));
		scnprintf(__entry->online, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(cpu_online_mask));
		scnprintf(__entry->isolated, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(cpu_isolated_mask));
		scnprintf(__entry->thermal, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(cpu_cooling_get_max_level_cpumask()));
	),

	TP_printk("event=%s reserve=%s reserved=%s our_isolated=%s online=%s isolated=%s thermal=%s",
		  __get_str(event), __entry->reserve, __entry->reserved,
		  __entry->our_isolated, __entry->online, __entry->isolated,
		  __entry->thermal)
);

#endif /* _TRACE_HYP_CORE_CTL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
