/* Copyright (c) 2012, 2014, The Linux Foundation. All rights reserved.
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
#define TRACE_SYSTEM rpm_smd

#if !defined(_TRACE_RPM_SMD_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_RPM_SMD_H

#include <linux/tracepoint.h>

TRACE_EVENT(rpm_ack_recd,

	TP_PROTO(unsigned int irq, unsigned int msg_id),

	TP_ARGS(irq, msg_id),

	TP_STRUCT__entry(
		__field(int, irq)
		__field(int, msg_id)
	),

	TP_fast_assign(
		__entry->irq = irq;
		__entry->msg_id = msg_id;
	),

	TP_printk("ctx:%s id:%d",
		__entry->irq ? "noslp" : "sleep",
		__entry->msg_id)
);

TRACE_EVENT(rpm_send_message,

	TP_PROTO(unsigned int irq, unsigned int set, unsigned int rsc_type,
		unsigned int rsc_id, unsigned int msg_id),

	TP_ARGS(irq, set, rsc_type, rsc_id, msg_id),

	TP_STRUCT__entry(
		__field(u32, irq)
		__field(u32, set)
		__field(u32, rsc_type)
		__field(u32, rsc_id)
		__field(u32, msg_id)
		__array(char, name, 5)
	),

	TP_fast_assign(
		__entry->irq	= irq;
		__entry->name[4] = 0;
		__entry->set = set;
		__entry->rsc_type = rsc_type;
		__entry->rsc_id = rsc_id;
		__entry->msg_id = msg_id;
		memcpy(__entry->name, &rsc_type, sizeof(uint32_t));

	),

	TP_printk("ctx:%s set:%s rsc_type:0x%08x(%s), rsc_id:0x%08x, id:%d",
			__entry->irq ? "noslp" : "sleep",
			__entry->set ? "slp" : "act",
			__entry->rsc_type, __entry->name,
			__entry->rsc_id, __entry->msg_id)
);
#endif
#define TRACE_INCLUDE_FILE trace_rpm_smd
#include <trace/define_trace.h>
