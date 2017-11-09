/* Copyright (c) 2014,2016 The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM msm_core

#if !defined(_TRACE_MSM_CORE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_MSM_CORE_H

#include <linux/tracepoint.h>
#include <linux/thermal.h>

TRACE_EVENT(cpu_stats,

	TP_PROTO(unsigned int cpu, long temp,
	uint64_t min_power, uint64_t max_power),

	TP_ARGS(cpu, temp, min_power, max_power),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(long, temp)
		__field(uint64_t, min_power)
		__field(uint64_t, max_power)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->temp = temp;
		__entry->min_power = min_power;
		__entry->max_power = max_power;
	),

	TP_printk("Cpu%d: temp:%ld power@minfreq:%llu power@maxfreq:%llu",
		__entry->cpu, __entry->temp, __entry->min_power,
		__entry->max_power)
);

TRACE_EVENT(temp_threshold,

	TP_PROTO(unsigned int cpu, long temp,
		long hi_thresh, long low_thresh),

	TP_ARGS(cpu, temp, hi_thresh, low_thresh),

	TP_STRUCT__entry(
		__field(unsigned int, cpu)
		__field(long, temp)
		__field(long, hi_thresh)
		__field(long, low_thresh)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->temp = temp;
		__entry->hi_thresh = hi_thresh;
		__entry->low_thresh = low_thresh;
	),

	TP_printk("Cpu%d: temp:%ld hi_thresh:%ld low_thresh:%ld",
		__entry->cpu, __entry->temp, __entry->hi_thresh,
		__entry->low_thresh)
);

TRACE_EVENT(temp_notification,

	TP_PROTO(unsigned int sensor_id, enum thermal_trip_type type,
		int temp, int prev_temp),

	TP_ARGS(sensor_id, type, temp, prev_temp),

	TP_STRUCT__entry(
		__field(unsigned int, sensor_id)
		__field(enum thermal_trip_type, type)
		__field(int, temp)
		__field(int, prev_temp)
	),

	TP_fast_assign(
		__entry->sensor_id = sensor_id;
		__entry->type = type;
		__entry->temp = temp;
		__entry->prev_temp = prev_temp;
	),

	TP_printk("Sensor_id%d: %s threshold triggered temp:%d(previous:%d)",
		__entry->sensor_id,
		__entry->type == THERMAL_TRIP_CONFIGURABLE_HI ? "High" : "Low",
		__entry->temp, __entry->prev_temp)
);

#endif
#define TRACE_INCLUDE_FILE trace_msm_core
#include <trace/define_trace.h>
