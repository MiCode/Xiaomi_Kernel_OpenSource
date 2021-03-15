/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018,2020-2021 The Linux Foundation. All rights reserved.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hyp_core_ctl
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE hyp_core_ctl_trace

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
		__array(char, our_paused, 32)
		__array(char, online, 32)
		__array(char, active, 32)
		__array(char, thermal, 32)
	),

	TP_fast_assign(
		__assign_str(event, event);
		scnprintf(__entry->reserve, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(&hcd->reserve_cpus));
		scnprintf(__entry->reserved, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(&hcd->final_reserved_cpus));
		scnprintf(__entry->our_paused, sizeof(__entry->reserve),
			  "%*pbl", cpumask_pr_args(&hcd->our_paused_cpus));
		scnprintf(__entry->online, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(cpu_online_mask));
		scnprintf(__entry->active, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(cpu_active_mask));
		scnprintf(__entry->thermal, sizeof(__entry->reserve), "%*pbl",
			  cpumask_pr_args(thermal_paused_cpumask()));
	),

	TP_printk("event=%s reserve=%s reserved=%s our_paused=%s online=%s active=%s thermal=%s",
		  __get_str(event), __entry->reserve, __entry->reserved,
		  __entry->our_paused, __entry->online, __entry->active,
		  __entry->thermal)
);

#endif /* _TRACE_HYP_CORE_CTL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
