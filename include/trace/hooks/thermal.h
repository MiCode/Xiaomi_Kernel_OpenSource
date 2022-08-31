/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM thermal

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_THERMAL_H

#include <trace/hooks/vendor_hooks.h>

#ifdef __GENKSYMS__
#include <linux/cpufreq.h>
#include <linux/thermal.h>
#endif

struct cpufreq_policy;
struct thermal_cooling_device;
struct thermal_zone_device;

DECLARE_HOOK(android_vh_modify_thermal_request_freq,
	TP_PROTO(struct cpufreq_policy *policy, unsigned long *request_freq),
	TP_ARGS(policy, request_freq));

DECLARE_HOOK(android_vh_modify_thermal_target_freq,
	TP_PROTO(struct cpufreq_policy *policy, unsigned int *target_freq),
	TP_ARGS(policy, target_freq));

DECLARE_HOOK(android_vh_thermal_register,
	TP_PROTO(struct cpufreq_policy *policy),
	TP_ARGS(policy));

DECLARE_HOOK(android_vh_thermal_unregister,
	TP_PROTO(struct cpufreq_policy *policy),
	TP_ARGS(policy));

DECLARE_HOOK(android_vh_enable_thermal_power_throttle,
	TP_PROTO(bool *enable, bool *override),
	TP_ARGS(enable, override));

DECLARE_HOOK(android_vh_thermal_power_cap,
	TP_PROTO(u32 *power_range),
	TP_ARGS(power_range));

DECLARE_HOOK(android_vh_get_thermal_zone_device,
	TP_PROTO(struct thermal_zone_device *tz),
	TP_ARGS(tz));

DECLARE_HOOK(android_vh_disable_thermal_cooling_stats,
	TP_PROTO(struct thermal_cooling_device *cdev, int *disable_stats),
	TP_ARGS(cdev, disable_stats));

DECLARE_HOOK(android_vh_modify_thermal_cpu_get_power,
	TP_PROTO(struct cpufreq_policy *policy, u32 *power),
	TP_ARGS(policy, power));

#endif /* _TRACE_HOOK_THERMAL_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
