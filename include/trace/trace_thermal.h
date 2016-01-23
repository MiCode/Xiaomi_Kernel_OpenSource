/*
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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

#ifdef TRACE_MSM_LMH
DECLARE_EVENT_CLASS(msm_lmh_print_sensor_reading,

	TP_PROTO(const char *sensor_name, unsigned int intensity),

	TP_ARGS(
		sensor_name, intensity
	),

	TP_STRUCT__entry(
		__string(_name, sensor_name)
		__field(unsigned int, reading)
	),

	TP_fast_assign(
		__assign_str(_name, sensor_name);
		__entry->reading = intensity;
	),

	TP_printk(
		"Sensor:[%s] throttling intensity:%u", __get_str(_name),
		__entry->reading
	)
);

DECLARE_EVENT_CLASS(msm_lmh_print_event,

	TP_PROTO(const char *event_name),

	TP_ARGS(
		event_name
	),

	TP_STRUCT__entry(
		__string(_name,	event_name)
	),

	TP_fast_assign(
		__assign_str(_name, event_name);
	),

	TP_printk(
		"Event:[%s]", __get_str(_name)
	)
);

DEFINE_EVENT(msm_lmh_print_sensor_reading, lmh_sensor_interrupt,

	TP_PROTO(const char *sensor_name, unsigned int intensity),

	TP_ARGS(sensor_name, intensity)
);

DEFINE_EVENT(msm_lmh_print_sensor_reading, lmh_sensor_reading,

	TP_PROTO(const char *sensor_name, unsigned int intensity),

	TP_ARGS(sensor_name, intensity)
);

DEFINE_EVENT(msm_lmh_print_event, lmh_event_call,

	TP_PROTO(const char *event_name),

	TP_ARGS(event_name)
);

TRACE_EVENT(lmh_debug_data,
	TP_PROTO(const char *pre_data, uint32_t *data_buf, uint32_t buffer_len),

	TP_ARGS(
		pre_data, data_buf, buffer_len
	),

	TP_STRUCT__entry(
		__string(_data, pre_data)
		__field(u32, _buffer_len)
		__dynamic_array(u32, _buffer, buffer_len)
	),

	TP_fast_assign(
		__assign_str(_data, pre_data);
		__entry->_buffer_len = buffer_len * sizeof(uint32_t);
		memcpy(__get_dynamic_array(_buffer), data_buf,
			buffer_len * sizeof(uint32_t));
	),

	TP_printk("%s:\t %s",
		__get_str(_data), __print_hex(__get_dynamic_array(_buffer),
			__entry->_buffer_len)
	)
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

#elif defined(_BCL_SW_TRACE) || defined(_BCL_HW_TRACE)

DECLARE_EVENT_CLASS(msm_bcl_print_reading,

	TP_PROTO(const char *sensor_name, long value),

	TP_ARGS(
		sensor_name, value
	),

	TP_STRUCT__entry(
		__string(_name, sensor_name)
		__field(long, reading)
	),

	TP_fast_assign(
		__assign_str(_name, sensor_name);
		__entry->reading = value;
	),

	TP_printk(
		"%s:[%ld]", __get_str(_name), __entry->reading
	)
);

DECLARE_EVENT_CLASS(msm_bcl_print_event,

	TP_PROTO(const char *event_name),

	TP_ARGS(
		event_name
	),

	TP_STRUCT__entry(
		__string(_name,	event_name)
	),

	TP_fast_assign(
		__assign_str(_name, event_name);
	),

	TP_printk(
		"Event:[%s]", __get_str(_name)
	)
);

#ifdef _BCL_HW_TRACE
DECLARE_EVENT_CLASS(msm_bcl_print_reg,

	TP_PROTO(const char *sensor_name, unsigned int address,
			unsigned int value),

	TP_ARGS(
		sensor_name, address, value
	),

	TP_STRUCT__entry(
		__string(_name, sensor_name)
		__field(unsigned int, _address)
		__field(unsigned int, _value)
	),

	TP_fast_assign(
		__assign_str(_name, sensor_name);
		__entry->_address = address;
		__entry->_value = value;
	),

	TP_printk(
		"%s: address 0x%x: data 0x%02x", __get_str(_name),
		__entry->_address, __entry->_value
	)
);

DEFINE_EVENT(msm_bcl_print_reading, bcl_hw_sensor_reading,

	TP_PROTO(const char *sensor_name, long intensity),

	TP_ARGS(sensor_name, intensity)
);

DEFINE_EVENT(msm_bcl_print_reg, bcl_hw_reg_access,

	TP_PROTO(const char *op_name, unsigned int address, unsigned int value),

	TP_ARGS(op_name, address, value)
);

DEFINE_EVENT(msm_bcl_print_reading, bcl_hw_mitigation,

	TP_PROTO(const char *sensor_name, long intensity),

	TP_ARGS(sensor_name, intensity)
);

DEFINE_EVENT(msm_bcl_print_event, bcl_hw_mitigation_event,

	TP_PROTO(const char *event_name),

	TP_ARGS(event_name)
);

DEFINE_EVENT(msm_bcl_print_reading, bcl_hw_state_event,

	TP_PROTO(const char *sensor_name, long intensity),

	TP_ARGS(sensor_name, intensity)
);

DEFINE_EVENT(msm_bcl_print_event, bcl_hw_event,

	TP_PROTO(const char *event_name),

	TP_ARGS(event_name)
);
#elif defined(_BCL_SW_TRACE)
DEFINE_EVENT(msm_bcl_print_reading, bcl_sw_mitigation,

	TP_PROTO(const char *sensor_name, long intensity),

	TP_ARGS(sensor_name, intensity)
);

DEFINE_EVENT(msm_bcl_print_event, bcl_sw_mitigation_event,

	TP_PROTO(const char *event_name),

	TP_ARGS(event_name)
);
#endif /* _BCL_HW_TRACE */
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
