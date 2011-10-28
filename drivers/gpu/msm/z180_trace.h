/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#if !defined(_Z180_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _Z180_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM kgsl
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE z180_trace

#include <linux/tracepoint.h>

struct kgsl_device;

/*
 * Tracepoint for z180 irq. Includes status info
 */
TRACE_EVENT(kgsl_z180_irq_status,

	TP_PROTO(struct kgsl_device *device, unsigned int status),

	TP_ARGS(device, status),

	TP_STRUCT__entry(
		__string(device_name, device->name)
		__field(unsigned int, status)
	),

	TP_fast_assign(
		__assign_str(device_name, device->name);
		__entry->status = status;
	),

	TP_printk(
		"d_name=%s status=%s",
		__get_str(device_name),
		__entry->status ? __print_flags(__entry->status, "|",
			{ REG_VGC_IRQSTATUS__MH_MASK, "MH" },
			{ REG_VGC_IRQSTATUS__G2D_MASK, "G2D" },
			{ REG_VGC_IRQSTATUS__FIFO_MASK, "FIFO" }) : "None"
	)
);

#endif /* _Z180_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
