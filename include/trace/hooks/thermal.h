/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM thermal

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_THERMAL_H

#include <linux/tracepoint.h>
#include <trace/hooks/vendor_hooks.h>
#include <linux/cpufreq.h>

DECLARE_HOOK(android_vh_modify_thermal_request_freq,
	TP_PROTO(struct cpufreq_policy *policy, unsigned long *request_freq),
	TP_ARGS(policy, request_freq));

DECLARE_HOOK(android_vh_modify_thermal_target_freq,
	TP_PROTO(struct cpufreq_policy *policy, unsigned int *target_freq),
	TP_ARGS(policy, target_freq));

DECLARE_HOOK(android_vh_enable_thermal_power_throttle,
	TP_PROTO(int *enable),
	TP_ARGS(enable));

#endif /* _TRACE_HOOK_THERMAL_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
