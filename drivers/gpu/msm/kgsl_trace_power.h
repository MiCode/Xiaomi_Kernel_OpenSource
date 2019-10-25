/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#if !defined(_KGSL_TRACE_POWER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _KGSL_TRACE_POWER_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM power
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE kgsl_trace_power

#include <linux/tracepoint.h>

/**
 * gpu_frequency - Reports frequency changes in GPU clock domains
 * @state:  New frequency (in KHz)
 * @gpu_id: GPU clock domain
 */
TRACE_EVENT(gpu_frequency,
	TP_PROTO(unsigned int state, unsigned int gpu_id),
	TP_ARGS(state, gpu_id),
	TP_STRUCT__entry(
		__field(unsigned int, state)
		__field(unsigned int, gpu_id)
	),
	TP_fast_assign(
		__entry->state = state;
		__entry->gpu_id = gpu_id;
	),

	TP_printk("state=%lu gpu_id=%lu",
		(unsigned long)__entry->state,
		(unsigned long)__entry->gpu_id)
);

#endif /* _KGSL_TRACE_POWER_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
