/* Copyright (c) 2012, 2014, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM msm_low_power

#if !defined(_TRACE_MSM_LOW_POWER_H_) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_LOW_POWER_H_

#include <linux/tracepoint.h>

DECLARE_EVENT_CLASS(msm_pm_enter,

	TP_PROTO(unsigned int cpu, uint32_t latency,
		uint32_t sleep_us, uint32_t wake_up),

	TP_ARGS(cpu, latency, sleep_us, wake_up),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(uint32_t, latency)
		__field(uint32_t, sleep_us)
		__field(uint32_t, wake_up)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->latency = latency;
		__entry->sleep_us = sleep_us;
		__entry->wake_up = wake_up;
	),

	TP_printk("cpu: %u latency: %uus sleep: %uus wake_up: %u",
		__entry->cpu,
		__entry->latency,
		__entry->sleep_us,
		__entry->wake_up)
);

DEFINE_EVENT(msm_pm_enter, msm_pm_enter_pc,

	TP_PROTO(unsigned int cpu, uint32_t latency,
		uint32_t sleep_us, uint32_t wake_up),

	TP_ARGS(cpu, latency, sleep_us, wake_up)
);

DEFINE_EVENT(msm_pm_enter, msm_pm_enter_ret,

	TP_PROTO(unsigned int cpu, uint32_t latency,
		uint32_t sleep_us, uint32_t wake_up),

	TP_ARGS(cpu, latency, sleep_us, wake_up)
);

DEFINE_EVENT(msm_pm_enter, msm_pm_enter_spc,

	TP_PROTO(unsigned int cpu, uint32_t latency,
		uint32_t sleep_us, uint32_t wake_up),

	TP_ARGS(cpu, latency, sleep_us, wake_up)
);

DEFINE_EVENT(msm_pm_enter, msm_pm_enter_wfi,

	TP_PROTO(unsigned int cpu, uint32_t latency,
		uint32_t sleep_us, uint32_t wake_up),

	TP_ARGS(cpu, latency, sleep_us, wake_up)
);

DECLARE_EVENT_CLASS(msm_pm_exit,

	TP_PROTO(unsigned int cpu, bool success),

	TP_ARGS(cpu, success),

	TP_STRUCT__entry(
		__field(unsigned int , cpu)
		__field(int, success)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->success = success;
	),

	TP_printk("cpu:%u success:%d",
			__entry->cpu,
			__entry->success)
);

DEFINE_EVENT(msm_pm_exit, msm_pm_exit_pc,

	TP_PROTO(unsigned int cpu, bool success),

	TP_ARGS(cpu, success)
);

DEFINE_EVENT(msm_pm_exit, msm_pm_exit_ret,

	TP_PROTO(unsigned int cpu, bool success),

	TP_ARGS(cpu, success)
);

DEFINE_EVENT(msm_pm_exit, msm_pm_exit_spc,

	TP_PROTO(unsigned int cpu, bool success),

	TP_ARGS(cpu, success)
);

DEFINE_EVENT(msm_pm_exit, msm_pm_exit_wfi,

	TP_PROTO(unsigned int cpu, bool success),

	TP_ARGS(cpu, success)
);

TRACE_EVENT(lpm_resources,

	TP_PROTO(uint32_t sleep_value , char *name),

	TP_ARGS(sleep_value, name),

	TP_STRUCT__entry(
		__field(uint32_t , sleep_value)
		__string(name, name)
	),

	TP_fast_assign(
		__entry->sleep_value = sleep_value;
		__assign_str(name, name);
	),

	TP_printk("name:%s sleep_value:%d",
			 __get_str(name),
			__entry->sleep_value)
);
#endif
#define TRACE_INCLUDE_FILE trace_msm_low_power
#include <trace/define_trace.h>
