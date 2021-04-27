/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM devmpu

#if !defined(_TRACE_EVENT_DEVICE_MPU_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_EVENT_DEVICE_MPU_H

#include <linux/tracepoint.h>

TRACE_EVENT(devmpu_event,
	TP_PROTO(unsigned long long vio_addr, unsigned int vio_id,
			unsigned int vio_domain, unsigned int vio_rw),
	TP_ARGS(vio_addr, vio_id, vio_domain, vio_rw),

	TP_STRUCT__entry(
		__field(unsigned long long, vio_addr)
		__field(unsigned int, vio_id)
		__field(unsigned int, vio_domain)
		__field(unsigned int, vio_rw)
	),

	TP_fast_assign(
		__entry->vio_addr = vio_addr;
		__entry->vio_id = vio_id;
		__entry->vio_domain = vio_domain;
		__entry->vio_rw = vio_rw;
	),

	TP_printk("devmpu:vio_id(0x%x),vio_domain(0x%x),vio_rw(0x%x),vio_addr(0x%llx)",
		  __entry->vio_id, __entry->vio_domain, __entry->vio_rw, __entry->vio_addr)
	);
#endif

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE devmpu_trace
#include <trace/define_trace.h>
