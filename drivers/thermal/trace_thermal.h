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

#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE trace_thermal
#include <trace/define_trace.h>
