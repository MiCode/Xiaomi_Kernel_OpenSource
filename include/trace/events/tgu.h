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
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM tgu

#if !defined(_TRACE_TGU_) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_TGU_H_

#include <linux/tracepoint.h>

TRACE_EVENT(tgu_interrupt,

	TP_PROTO(uint32_t irqs),

	TP_ARGS(irqs),

	TP_STRUCT__entry(
		__field(uint32_t, irqs)
	),

	TP_fast_assign(
		__entry->irqs = irqs;
	),

	TP_printk("irq:%u  ", __entry->irqs)
);

#endif
#define TRACE_INCLUDE_FILE tgu
#include <trace/define_trace.h>
