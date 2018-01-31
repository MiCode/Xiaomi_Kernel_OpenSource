/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM thermal_virtual

#if !defined(_TRACE_VIRTUAL_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_VIRTUAL_H

#include <linux/thermal.h>
#include <linux/tracepoint.h>

TRACE_EVENT(virtual_temperature,

	TP_PROTO(struct thermal_zone_device *virt_tz,
		struct thermal_zone_device *tz, int sens_temp,
		int est_temp),

	TP_ARGS(virt_tz, tz, sens_temp, est_temp),

	TP_STRUCT__entry(
		__string(virt_zone, virt_tz->type)
		__string(therm_zone, tz->type)
		__field(int, sens_temp)
		__field(int, est_temp)
	),

	TP_fast_assign(
		__assign_str(virt_zone, virt_tz->type);
		__assign_str(therm_zone, tz->type);
		__entry->sens_temp = sens_temp;
		__entry->est_temp = est_temp;
	),

	TP_printk("virt_zone=%s zone=%s temp=%d virtual zone estimated temp=%d",
		__get_str(virt_zone), __get_str(therm_zone),
		__entry->sens_temp,
		__entry->est_temp)
);

#endif /* _TRACE_VIRTUAL_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
