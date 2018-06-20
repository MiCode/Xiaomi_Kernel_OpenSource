/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM pdc

#if !defined(_TRACE_PDC_) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_PDC_H_

#include <linux/tracepoint.h>

TRACE_EVENT(irq_pin_config,

	TP_PROTO(char *func, u32 pin, u32 hwirq, u32 type, u32 enable),

	TP_ARGS(func, pin, hwirq, type, enable),

	TP_STRUCT__entry(
		__field(char *, func)
		__field(u32, pin)
		__field(u32, hwirq)
		__field(u32, type)
		__field(u32, enable)
	),

	TP_fast_assign(
		__entry->pin = pin;
		__entry->func = func;
		__entry->hwirq = hwirq;
		__entry->type = type;
		__entry->enable = enable;
	),

	TP_printk("%s hwirq:%u pin:%u type:%u enable:%u",
		__entry->func, __entry->hwirq, __entry->pin, __entry->type,
		__entry->enable)
);

#endif
#define TRACE_INCLUDE_FILE pdc
#include <trace/define_trace.h>
