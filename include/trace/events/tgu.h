/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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
