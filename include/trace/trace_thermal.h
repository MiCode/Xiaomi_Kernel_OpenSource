/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM thermal

#if !defined(_TRACE_THERMAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_THERMAL_H

#include <linux/tracepoint.h>

#ifdef TRACE_SUPPLY_LM
DECLARE_EVENT_CLASS(supply_lm_scm_ctl,

	TP_PROTO(unsigned int value),

	TP_ARGS(value),

	TP_STRUCT__entry(
		__field(unsigned int, value)
	),

	TP_fast_assign(
		__entry->value = value;
	),

	TP_printk("inp=0x%x", __entry->value)
);

DEFINE_EVENT(supply_lm_scm_ctl, supply_lm_pre_scm,

	TP_PROTO(unsigned int value),

	TP_ARGS(value)
);

DEFINE_EVENT(supply_lm_scm_ctl, supply_lm_post_scm,

	TP_PROTO(unsigned int ret),

	TP_ARGS(ret)
);

DECLARE_EVENT_CLASS(supply_lm_inp_ctl,

	TP_PROTO(unsigned int inp, unsigned int val),

	TP_ARGS(inp, val),

	TP_STRUCT__entry(
		__field(unsigned int, inp)
		__field(unsigned int, val)
	),

	TP_fast_assign(
		__entry->inp = inp;
		__entry->val = val;
	),

	TP_printk("inp=%u val=%u",
		 __entry->inp, __entry->val)
);

DEFINE_EVENT(supply_lm_inp_ctl, supply_lm_inp_start_trig,

	TP_PROTO(unsigned int inp, unsigned int val),

	TP_ARGS(inp, val)
);

DEFINE_EVENT(supply_lm_inp_ctl, supply_lm_inp_end_trig,

	TP_PROTO(unsigned int inp, unsigned int val),

	TP_ARGS(inp, val)
);

#elif defined(TRACE_MSM_THERMAL)

DECLARE_EVENT_CLASS(msm_thermal_post_core_ctl,

	TP_PROTO(unsigned int cpu, unsigned int online),

	TP_ARGS(cpu, online),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, online)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->online = online;
	),

	TP_printk("device=cpu%u online=%u",
		 __entry->cpu,  __entry->online)
);
DECLARE_EVENT_CLASS(msm_thermal_pre_core_ctl,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
	),

	TP_printk("device=cpu%u", __entry->cpu)
);

DEFINE_EVENT(msm_thermal_pre_core_ctl, thermal_pre_core_offline,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu)
);

DEFINE_EVENT(msm_thermal_post_core_ctl, thermal_post_core_offline,

	TP_PROTO(unsigned int cpu, unsigned int online),

	TP_ARGS(cpu, online)
);

DEFINE_EVENT(msm_thermal_pre_core_ctl, thermal_pre_core_online,

	TP_PROTO(unsigned int cpu),

	TP_ARGS(cpu)
);

DEFINE_EVENT(msm_thermal_post_core_ctl, thermal_post_core_online,

	TP_PROTO(unsigned int cpu, unsigned int online),

	TP_ARGS(cpu, online)
);

DECLARE_EVENT_CLASS(msm_thermal_freq_mit,

	TP_PROTO(unsigned int cpu, unsigned int max_freq,
		unsigned int min_freq),

	TP_ARGS(cpu, max_freq, min_freq),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(unsigned int, max_freq)
		__field(unsigned int, min_freq)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->max_freq = max_freq;
		__entry->min_freq = min_freq;
	),

	TP_printk("device=cpu%u max_frequency=%u min_frequency=%u",
			 __entry->cpu, __entry->max_freq,
			 __entry->min_freq)
);

DEFINE_EVENT(msm_thermal_freq_mit, thermal_pre_frequency_mit,

	TP_PROTO(unsigned int cpu, unsigned int max_freq,
		unsigned int min_freq),

	TP_ARGS(cpu, max_freq, min_freq)
);

DEFINE_EVENT(msm_thermal_freq_mit, thermal_post_frequency_mit,

	TP_PROTO(unsigned int cpu, unsigned int max_freq,
		unsigned int min_freq),

	TP_ARGS(cpu, max_freq, min_freq)
);
#else
DECLARE_EVENT_CLASS(tsens,

	TP_PROTO(unsigned long temp, unsigned int sensor),

	TP_ARGS(temp, sensor),

	TP_STRUCT__entry(
		__field(unsigned long, temp)
		__field(unsigned int, sensor)
	),

	TP_fast_assign(
		__entry->temp = temp;
		__entry->sensor = sensor;
	),

	TP_printk("temp=%lu sensor=tsens_tz_sensor%u",
				__entry->temp, __entry->sensor)
);

DEFINE_EVENT(tsens, tsens_read,

	TP_PROTO(unsigned long temp, unsigned int sensor),

	TP_ARGS(temp, sensor)
);

DEFINE_EVENT(tsens, tsens_threshold_hit,

	TP_PROTO(unsigned long temp, unsigned int sensor),

	TP_ARGS(temp, sensor)
);

DEFINE_EVENT(tsens, tsens_threshold_clear,

	TP_PROTO(unsigned long temp, unsigned int sensor),

	TP_ARGS(temp, sensor)
);
#endif
#endif
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_thermal
#include <trace/define_trace.h>
